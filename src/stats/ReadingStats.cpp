#include "ReadingStats.h"

#include <Arduino.h>
#include <HalFrontlight.h>
#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>
#include <TrustedTime.h>

#include <algorithm>
#include <cstddef>
#include <cstring>

#include "CrossPointSettings.h"

using namespace stats;

namespace {

FileHeader makeHeader(const char (&magic)[4], const uint16_t version, const uint16_t headerSize,
                      const uint16_t recordSize, const uint8_t flags = 0) {
  FileHeader header{};
  memcpy(header.magic, magic, sizeof(header.magic));
  header.version = version;
  header.headerSize = headerSize;
  header.recordSize = recordSize;
  header.flags = flags;
  return header;
}

bool readExact(HalFile& file, void* out, const size_t size) { return file.read(out, size) == static_cast<int>(size); }

bool writeExact(HalFile& file, const void* data, const size_t size) { return file.write(data, size) == size; }

bool ignoreShortOpens() { return SETTINGS.statsIgnoreShortOpens != 0; }

uint8_t summaryFlags() { return ignoreShortOpens() ? SUMMARY_FLAG_IGNORE_SHORT : 0; }

// Copies a string into a fixed field, cut at a UTF-8 character boundary.
void copyField(char* dst, const size_t size, const std::string& src) {
  size_t len = std::min(src.size(), size - 1);
  while (len > 0 && len < src.size() && (static_cast<uint8_t>(src[len]) & 0xC0) == 0x80) --len;
  memcpy(dst, src.data(), len);
  memset(dst + len, 0, size - len);
}

void logPathFor(const char* docId, char* out, const size_t size) {
  snprintf(out, size, "%s/%.32s.log", BOOKS_LOG_DIR, docId);
}

constexpr size_t bookOffset(const int index) {
  return sizeof(RegistryHeader) + static_cast<size_t>(index) * sizeof(BookRecord);
}

int registryCount(HalFile& file) {
  const size_t size = file.size();
  return size < sizeof(RegistryHeader) ? 0 : static_cast<int>((size - sizeof(RegistryHeader)) / sizeof(BookRecord));
}

bool readRegistryHeader(HalFile& file, RegistryHeader& header) {
  return file.size() >= sizeof(RegistryHeader) && file.seek(0) && readExact(file, &header, sizeof(header)) &&
         headerMatches(header.file, MAGIC_REGISTRY, REGISTRY_VERSION, sizeof(RegistryHeader), sizeof(BookRecord));
}

bool openValid(const char* path, const char (&magic)[4], const uint16_t version, const uint16_t headerSize,
               const uint16_t recordSize, HalFile& file, FileHeader& header, const oflag_t mode = O_RDONLY) {
  if (!Storage.exists(path)) return false;
  file = Storage.open(path, mode);
  return file && readExact(file, &header, sizeof(header)) &&
         headerMatches(header, magic, version, headerSize, recordSize);
}

uint64_t idKey(const char* docId) {
  uint64_t key = 0;
  for (int i = 0; i < 16; ++i) {
    const char c = docId[i];
    const uint64_t digit = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : 0;
    key = (key << 4) | digit;
  }
  return key;
}

size_t readHalFile(void* ctx, void* buf, const size_t n) {
  if (n == 0) return 0;
  const int read = static_cast<HalFile*>(ctx)->read(buf, n);
  return read > 0 ? static_cast<size_t>(read) : 0;
}

bool appendToLog(const char* docId, const uint8_t* data, const size_t len) {
  if (len == 0) return true;
  char path[64];
  logPathFor(docId, path, sizeof(path));
  HalFile file = Storage.open(path, O_RDWR | O_CREAT | O_AT_END);
  if (!file) {
    LOG_ERR("STATS", "Cannot open %s", path);
    return false;
  }
  if (file.size() == 0) {
    const FileHeader header = makeHeader(MAGIC_LOG, RAW_LOG_VERSION, sizeof(FileHeader), 0);
    if (!writeExact(file, &header, sizeof(header))) return false;
  }
  if (!writeExact(file, data, len)) {
    LOG_ERR("STATS", "Log write failed: %s", path);
    return false;
  }
  return true;
}

RawEnd makeEnd(const SessionRecord& r, const EndReason reason, const uint32_t durationMs, const uint32_t endUtc) {
  RawEnd end{};
  end.type = RAW_END;
  end.size = sizeof(RawEnd);
  end.reason = reason;
  end.flags = r.flags;
  end.durationMs = durationMs;
  end.activeSec = r.activeSec;
  end.endUtc = endUtc;
  end.charsRead = r.charsRead;
  end.learnedMs = r.learnedMs;
  end.turnsFwd = r.turnsFwd;
  end.turnsBack = r.turnsBack;
  end.jumps = r.jumps;
  end.lookups = r.lookups;
  end.clippings = r.clippings;
  end.bookmarks = r.bookmarks;
  end.endPaperPage = r.endPaperPage;
  end.endPercentBp = r.endPercentBp;
  end.endSpine = r.endSpine;
  end.tzOffsetMin = endUtc ? localTzOffsetMin(endUtc) : 0;
  return end;
}

// Replays one book's raw log into the session index; returns the sessions written. Its own
// frame keeps the record buffers off the caller's stack.
uint32_t replayLog(const char* docId, LogReplay& replay, HalFile& out) {
  char path[64];
  logPathFor(docId, path, sizeof(path));
  HalFile log;
  FileHeader logHeader{};
  if (!openValid(path, MAGIC_LOG, RAW_LOG_VERSION, sizeof(FileHeader), 0, log, logHeader)) return 0;
  uint32_t written = 0;
  LogScanner scanner(&readHalFile, &log, sizeof(FileHeader));
  uint8_t buf[RAW_MAX_KNOWN_SIZE];
  size_t size = 0;
  SessionRecord rec{};
  while (scanner.next(buf, size)) {
    if (replay.feed(buf, size, rec) && writeExact(out, &rec, sizeof(rec))) ++written;
  }
  if (replay.finish(rec) && writeExact(out, &rec, sizeof(rec))) ++written;
  return written;
}

// Cuts a torn tail off a book's log, so records appended later frame correctly. Sets closed
// when the log already ends session seq with an end record (a reset before the index write).
bool repairLog(const char* docId, const uint16_t bookIndex, const uint32_t seq, SessionRecord& closed) {
  char path[64];
  logPathFor(docId, path, sizeof(path));
  HalFile log;
  FileHeader logHeader{};
  auto replay = makeUniqueNoThrow<LogReplay>();
  if (!replay) return false;
  if (!openValid(path, MAGIC_LOG, RAW_LOG_VERSION, sizeof(FileHeader), 0, log, logHeader, O_RDWR)) {
    // A header cut short: empty the file, so the next append writes a new header.
    if (log && log.size() < sizeof(FileHeader)) log.truncate(0);
    return false;
  }
  replay->begin(bookIndex, 0.0f, 0, 0);
  LogScanner scanner(&readHalFile, &log, sizeof(FileHeader));
  uint8_t buf[RAW_MAX_KNOWN_SIZE];
  size_t size = 0;
  SessionRecord rec{};
  bool found = false;
  while (scanner.next(buf, size)) {
    if (replay->feed(buf, size, rec) && buf[0] == RAW_END && rec.sessionSeq == seq) {
      closed = rec;
      found = true;
    }
  }
  if (scanner.completeEnd() < log.size()) {
    LOG_INF("STATS", "Cutting a torn log tail at %u", static_cast<unsigned>(scanner.completeEnd()));
    log.truncate(scanner.completeEnd());
  }
  return found;
}

// Session number of the last record in sessions.bin, 0 for none.
uint32_t lastIndexedSeq() {
  HalFile sessions;
  SessionRecord last{};
  if (!Storage.exists(SESSIONS_PATH) || !Storage.openFileForRead("STATS", SESSIONS_PATH, sessions) ||
      sessions.size() < sizeof(FileHeader) + sizeof(SessionRecord)) {
    return 0;
  }
  const size_t records = (sessions.size() - sizeof(FileHeader)) / sizeof(SessionRecord);
  if (!sessions.seek(sizeof(FileHeader) + (records - 1) * sizeof(SessionRecord)) ||
      !readExact(sessions, &last, sizeof(last))) {
    return 0;
  }
  return last.sessionSeq;
}

// The running session's totals in current.bin, rewritten in place (same size every time).
bool writeSnapshot(const char* docId, const SessionRecord& record) {
  CurrentRecord current{};
  current.file = makeHeader(MAGIC_CURRENT, CURRENT_VERSION, sizeof(CurrentRecord), 0);
  memcpy(current.docId, docId, sizeof(current.docId));
  current.session = record;
  HalFile file = Storage.open(CURRENT_PATH, O_RDWR | O_CREAT);
  if (!file || !file.seek(0) || !writeExact(file, &current, sizeof(current))) {
    LOG_ERR("STATS", "Cannot write %s", CURRENT_PATH);
    return false;
  }
  return true;
}

}  // namespace

ReadingStats& ReadingStats::getInstance() {
  static ReadingStats instance;
  return instance;
}

ReadingStats::ReadingStats() : mutex(xSemaphoreCreateMutexStatic(&mutexBuffer)) {}

// --- Book registry ------------------------------------------------------------------------

bool ReadingStats::loadIdCache() {
  if (!Storage.exists(REGISTRY_PATH)) {
    idCache.clear();
    hiddenByIndex.clear();
    return true;
  }
  HalFile registry;
  RegistryHeader header{};
  if (!Storage.openFileForRead("STATS", REGISTRY_PATH, registry) || !readRegistryHeader(registry, header)) {
    return false;
  }
  const int count = registryCount(registry);
  // Built once per boot. A registry of another size (a book added in a session the cache
  // missed, a replaced stats folder) loads it again.
  if (static_cast<int>(hiddenByIndex.size()) == count) return true;
  idCache.clear();
  hiddenByIndex.clear();
  idCache.reserve(count);
  hiddenByIndex.reserve(count);
  struct {
    char docId[DOC_ID_LEN];
    uint32_t flags;
  } head{};
  static_assert(offsetof(BookRecord, flags) == DOC_ID_LEN);
  for (int i = 0; i < count; ++i) {
    if (!registry.seek(bookOffset(i)) || !readExact(registry, &head, sizeof(head))) {
      idCache.clear();
      hiddenByIndex.clear();
      return false;
    }
    idCache.push_back({idKey(head.docId), static_cast<uint16_t>(i)});
    hiddenByIndex.push_back((head.flags & BOOK_EXCLUDED) ? 1 : 0);
  }
  std::sort(idCache.begin(), idCache.end(), [](const IdEntry& a, const IdEntry& b) { return a.key < b.key; });
  LOG_DBG("STATS", "Book id cache: %d books", count);
  return true;
}

int ReadingStats::findBookIndex(const char* docId) {
  if (!loadIdCache() || idCache.empty()) return -1;
  const uint64_t key = idKey(docId);
  auto it = std::lower_bound(idCache.begin(), idCache.end(), key,
                             [](const IdEntry& e, const uint64_t k) { return e.key < k; });
  HalFile file;
  for (; it != idCache.end() && it->key == key; ++it) {
    if (!file && !Storage.openFileForRead("STATS", REGISTRY_PATH, file)) return -1;
    char id[DOC_ID_LEN];
    if (file.seek(bookOffset(it->index)) && readExact(file, id, sizeof(id)) && memcmp(id, docId, sizeof(id)) == 0) {
      return it->index;
    }
  }
  return -1;
}

bool ReadingStats::bookExcluded(const uint16_t index) {
  return loadIdCache() && index < hiddenByIndex.size() && hiddenByIndex[index] != 0;
}

int ReadingStats::registerBook(const std::string& docId, const std::string& path, const std::string& title,
                               const std::string& author, BookRecord& record, RegistryHeader& header,
                               uint32_t& sessionSeq) {
  Storage.mkdir(BOOKS_LOG_DIR, true);
  if (!loadIdCache()) {
    LOG_ERR("STATS", "Book registry not readable; not recording");
    return -1;
  }
  int index = findBookIndex(docId.c_str());
  const bool added = index < 0;
  HalFile file = Storage.open(REGISTRY_PATH, O_RDWR | O_CREAT);
  if (!file) {
    LOG_ERR("STATS", "Cannot open %s", REGISTRY_PATH);
    return -1;
  }
  if (!readRegistryHeader(file, header)) {
    if (file.size() > 0) {
      LOG_ERR("STATS", "Registry header not recognised; not recording");
      return -1;
    }
    header = {};
    header.file = makeHeader(MAGIC_REGISTRY, REGISTRY_VERSION, sizeof(RegistryHeader), sizeof(BookRecord));
    header.nextSessionSeq = 1;
    // Records are written by seeking, which cannot go past the end of the file.
    if (!file.seek(0) || !writeExact(file, &header, sizeof(header))) {
      LOG_ERR("STATS", "Registry write failed");
      return -1;
    }
  }

  if (!added) {
    if (!file.seek(bookOffset(index)) || !readExact(file, &record, sizeof(record))) return -1;
  } else {
    const int count = registryCount(file);
    if (count >= NO_BOOK || count != static_cast<int>(hiddenByIndex.size())) return -1;
    index = count;
    record = {};
    memcpy(record.docId, docId.data(), sizeof(record.docId));
  }
  copyField(record.title, sizeof(record.title), title);
  copyField(record.author, sizeof(record.author), author);
  copyField(record.path, sizeof(record.path), path);
  sessionSeq = header.nextSessionSeq++;
  if (!file.seek(bookOffset(index)) || !writeExact(file, &record, sizeof(record)) || !file.seek(0) ||
      !writeExact(file, &header, sizeof(header))) {
    LOG_ERR("STATS", "Registry write failed");
    return -1;
  }
  if (added) {
    const IdEntry entry{idKey(record.docId), static_cast<uint16_t>(index)};
    idCache.insert(std::upper_bound(idCache.begin(), idCache.end(), entry,
                                    [](const IdEntry& a, const IdEntry& b) { return a.key < b.key; }),
                   entry);
    hiddenByIndex.push_back(0);
  }
  return index;
}

ReadingStats::Hidden ReadingStats::hiddenState(const std::string& docId) {
  if (docId.size() != DOC_ID_LEN) return Hidden::Unknown;
  const int index = findBookIndex(docId.c_str());
  if (index < 0) return Hidden::Unknown;
  return bookExcluded(static_cast<uint16_t>(index)) ? Hidden::Hidden : Hidden::Shown;
}

bool ReadingStats::setHidden(const std::string& docId, const bool hidden) {
  if (docId.size() != DOC_ID_LEN) return false;
  const int index = findBookIndex(docId.c_str());
  if (index < 0) return false;
  {
    HalFile registry = Storage.open(REGISTRY_PATH, O_RDWR);
    uint32_t flags = 0;
    const size_t at = bookOffset(index) + offsetof(BookRecord, flags);
    if (!registry || !registry.seek(at) || !readExact(registry, &flags, sizeof(flags))) return false;
    flags = hidden ? (flags | BOOK_EXCLUDED) : (flags & ~static_cast<uint32_t>(BOOK_EXCLUDED));
    if (!registry.seek(at) || !writeExact(registry, &flags, sizeof(flags))) return false;
  }
  hiddenByIndex[index] = hidden ? 1 : 0;
  {
    Lock lock(*this);
    if (session && session->bookIndex == index) session->bookExcluded = hidden;
  }
  return rebuildSummaries();
}

bool ReadingStats::loadHiddenFlags(std::vector<uint8_t>& out) {
  if (!loadIdCache()) {
    out.clear();
    return false;
  }
  out = hiddenByIndex;
  return true;
}

bool ReadingStats::bookTitle(const int index, char* out, const size_t size) {
  if (size == 0) return false;
  out[0] = '\0';
  HalFile file;
  char title[sizeof(BookRecord::title)];
  if (index < 0 || !Storage.openFileForRead("STATS", REGISTRY_PATH, file) || index >= registryCount(file) ||
      !file.seek(bookOffset(index) + offsetof(BookRecord, title)) || !readExact(file, title, sizeof(title))) {
    return false;
  }
  title[sizeof(title) - 1] = '\0';
  snprintf(out, size, "%s", title);
  return out[0] != '\0';
}

// --- Recording ----------------------------------------------------------------------------

void ReadingStats::appendPending(Session& s, const void* record, const size_t size, const bool isEnd) {
  const size_t capacity = isEnd ? BUFFER_BYTES : BUFFER_BYTES - sizeof(RawEnd);
  if (s.pendingLen + size > capacity) {
    ++s.droppedRecords;
    return;
  }
  memcpy(s.pending + s.pendingLen, record, size);
  s.pendingLen += size;
  s.dirty = true;
}

void ReadingStats::appendEvent(Session& s, const EventKind kind, const uint32_t nowMs) {
  RawEvent raw{};
  raw.type = RAW_EVENT;
  raw.size = sizeof(RawEvent);
  raw.kind = kind;
  raw.tMs = nowMs - s.startMs;
  appendPending(s, &raw, sizeof(raw));
}

void ReadingStats::beginSession(const std::string& docId, const std::string& path, const std::string& title,
                                const std::string& author) {
  endSession(END_EXIT);
  if (docId.size() != DOC_ID_LEN) return;
  prepareSummaries();

  auto s = makeUniqueNoThrow<Session>();
  auto book = makeUniqueNoThrow<BookRecord>();
  if (!s || !book) {
    LOG_ERR("STATS", "OOM: session (%u bytes)", static_cast<unsigned>(sizeof(Session) + sizeof(BookRecord)));
    return;
  }
  RegistryHeader header{};
  uint32_t seq = 0;
  const int index = registerBook(docId, path, title, author, *book, header, seq);
  if (index < 0) return;

  memcpy(s->docId, docId.data(), DOC_ID_LEN);
  s->bookIndex = static_cast<uint16_t>(index);
  s->bookExcluded = (book->flags & BOOK_EXCLUDED) != 0;
  memcpy(s->bookHist, book->rateHist, sizeof(s->bookHist));
  s->bookSamples = book->rateSamples;
  s->clock.begin(chooseRate(book->rateHist, book->rateSamples, header.globalHist, header.globalSamples),
                 book->typicalPageChars);

  const uint32_t startUtc = static_cast<uint32_t>(trustedtime::trustedNow());
  SessionRecord& r = s->record;
  r.startUtc = startUtc;
  r.tzOffsetMin = startUtc ? localTzOffsetMin(startUtc) : 0;
  r.bookIndex = s->bookIndex;
  r.flags = (startUtc ? SESSION_WALL_KNOWN : 0) | (Frontlight.isOn() ? SESSION_LIGHT_ON : 0);
  r.fontSize = SETTINGS.fontPointSize;
  r.orientation = SETTINGS.orientation;
  r.sessionSeq = seq;

  RawStart& start = s->start;
  start.type = RAW_START;
  start.size = sizeof(RawStart);
  start.flags = r.flags;
  start.fontSize = r.fontSize;
  start.startUtc = startUtc;
  start.tzOffsetMin = r.tzOffsetMin;
  start.orientation = r.orientation;
  start.sessionSeq = seq;

  s->startMs = millis();
  s->lastFlushMs = s->startMs;
  LOG_DBG("STATS", "Session %u started (book %d)", static_cast<unsigned>(seq), index);
  Lock lock(*this);
  session = std::move(s);
  pausedFlag = false;
}

void ReadingStats::onPageShown(const PageInfo& info) {
  Lock lock(*this);
  if (!session) return;
  Session& s = *session;
  const uint32_t now = millis();
  SessionRecord& r = s.record;
  const uint32_t offset = info.pos.offsetKnown ? info.pos.offset : UNKNOWN_OFFSET;
  if (!s.startWritten) {
    s.clock.start(now, info.pos);
    s.start.spine = info.pos.spine;
    s.start.offset = offset;
    s.start.percentBp = info.percentBp;
    s.start.paperPage = info.paperPage;
    s.start.paperPagesTotal = info.paperPagesTotal;
    s.start.tMs = now - s.startMs;
    r.startSpine = info.pos.spine;
    r.startPercentBp = info.percentBp;
    r.startPaperPage = info.paperPage;
    appendPending(s, &s.start, sizeof(s.start));
    s.startWritten = true;
  } else {
    const auto turn = s.clock.pageShown(now, info.pos);
    if (turn.kind == 0) return;
    if (turn.kind == TURN_NEXT) {
      ++r.turnsFwd;
    } else if (turn.kind == TURN_PREV) {
      ++r.turnsBack;
    } else {
      ++r.jumps;
    }
    if (turn.charsKnown) {
      r.charsRead += turn.chars;
      r.learnedMs += turn.countedMs;
    }
    if (turn.learned) {
      addRateSample(s.bookHist, s.bookSamples, turn.rate);
      addRateSample(s.globalAdd, s.globalAddSamples, turn.rate);
      if (s.bookSamples >= MIN_MODEL_SAMPLES) s.clock.setRate(medianRate(s.bookHist));
    }
    RawTurn raw{};
    raw.type = RAW_TURN;
    raw.size = sizeof(RawTurn);
    raw.kind = turn.kind;
    raw.spine = info.pos.spine;
    raw.percentBp = info.percentBp;
    raw.tMs = now - s.startMs;
    raw.offset = offset;
    raw.paperPage = info.paperPage;
    appendPending(s, &raw, sizeof(raw));
    pausedFlag = false;  // a page change ends a pause
  }
  r.endSpine = info.pos.spine;
  r.endPercentBp = info.percentBp;
  if (crossesFinish(r.startPercentBp, info.percentBp)) r.flags |= SESSION_FINISHED;
  if (info.paperPage > 0) r.endPaperPage = info.paperPage;
}

void ReadingStats::noteEndOfBook() {
  Lock lock(*this);
  if (!session || !session->startWritten || session->record.endPercentBp == END_OF_BOOK_BP) return;
  session->record.endPercentBp = END_OF_BOOK_BP;
  session->record.flags |= SESSION_FINISHED;
  appendEvent(*session, EVENT_END_OF_BOOK, millis());
}

void ReadingStats::noteEvent(const EventKind kind) {
  Lock lock(*this);
  if (!session || !session->startWritten) return;
  Session& s = *session;
  switch (kind) {
    case EVENT_LOOKUP:
      ++s.record.lookups;
      break;
    case EVENT_CLIPPING:
      ++s.record.clippings;
      break;
    case EVENT_BOOKMARK:
      ++s.record.bookmarks;
      break;
    default:
      return;
  }
  appendEvent(s, kind, millis());
}

void ReadingStats::setPaused(const bool paused) {
  if (pausedFlag.load() == paused) return;
  Lock lock(*this);
  if (!session || !session->startWritten) return;
  Session& s = *session;
  const uint32_t now = millis();
  const bool was = s.clock.isPaused();
  if (paused) {
    s.clock.pause(now);
  } else {
    s.clock.resume(now);
  }
  if (s.clock.isPaused() != was) appendEvent(s, paused ? EVENT_PAUSE : EVENT_RESUME, now);
  pausedFlag = s.clock.isPaused();
}

void ReadingStats::flush(Session& s, const uint32_t nowMs) {
  const int64_t trusted = trustedtime::trustedNow();
  size_t len = 0;
  bool started = false;
  bool changed = false;
  SessionRecord snapshot{};
  {
    Lock lock(*this);
    started = s.startWritten;
    len = s.pendingLen;
    memcpy(s.scratch, s.pending, len);
    s.pendingLen = 0;
    changed = s.dirty;
    s.dirty = false;
    const uint32_t durationSec = (nowMs - s.startMs) / 1000;
    if (s.record.startUtc == 0 && trusted > static_cast<int64_t>(durationSec)) {
      // The clock became trusted during the session: date the start from now.
      s.record.startUtc = static_cast<uint32_t>(trusted) - durationSec;
      s.record.tzOffsetMin = localTzOffsetMin(s.record.startUtc);
      s.record.flags |= SESSION_WALL_KNOWN;
      changed = true;
    }
    snapshot = s.record;
    snapshot.activeSec = s.clock.activeMsAt(nowMs) / 1000;
    snapshot.durationSec = durationSec;
  }
  s.lastFlushMs = nowMs;
  if (!started) return;
  // The snapshot goes first: a log append cut by a reset always has a record to recover from.
  if ((changed || !s.snapshotWritten) && writeSnapshot(s.docId, snapshot)) s.snapshotWritten = true;
  appendToLog(s.docId, s.scratch, len);
}

void ReadingStats::tick() {
  // The loop task alone replaces `session`; it reads the pointer without the lock.
  if (!session) return;
  Session& s = *session;
  const uint32_t now = millis();
  bool due;
  {
    Lock lock(*this);
    if (!s.startWritten) return;
    const SessionRecord& r = s.record;
    const bool firstSnapshot =
        !s.snapshotWritten && (r.turnsFwd + r.turnsBack + r.jumps > 0 || now - s.startMs >= FIRST_SNAPSHOT_MS);
    due = firstSnapshot || s.pendingLen >= FLUSH_AT_BYTES ||
          (now - s.lastFlushMs >= FLUSH_INTERVAL_MS && (s.dirty || s.pendingLen > 0));
  }
  if (due) flush(s, now);
}

void ReadingStats::endSession(const EndReason reason) {
  std::unique_ptr<Session> s;
  const int64_t trusted = trustedtime::trustedNow();
  uint32_t durationMs = 0;
  {
    Lock lock(*this);
    if (!session) return;
    const uint32_t now = millis();
    Session& x = *session;
    SessionRecord& r = x.record;
    durationMs = now - x.startMs;
    r.activeSec = x.clock.activeMsAt(now) / 1000;
    r.durationSec = durationMs / 1000;
    r.endReason = reason;
    if (isShortOpen(r.activeSec, r.turnsFwd + r.turnsBack + r.jumps)) r.flags |= SESSION_SHORT;
    if (r.startUtc == 0 && trusted > static_cast<int64_t>(r.durationSec)) {
      // The clock became trusted during the session: date the start from the end.
      r.startUtc = static_cast<uint32_t>(trusted) - r.durationSec;
      r.tzOffsetMin = localTzOffsetMin(r.startUtc);
      r.flags |= SESSION_WALL_KNOWN;
    }
    if (x.startWritten) {
      const RawEnd end = makeEnd(r, reason, durationMs, trusted > 0 ? static_cast<uint32_t>(trusted) : 0);
      appendPending(x, &end, sizeof(end), true);
    }
    s = std::move(session);
    pausedFlag = false;
  }
  if (!s->startWritten) {
    if (s->snapshotWritten) Storage.remove(CURRENT_PATH);
    return;  // no page reached the screen
  }
  if (s->droppedRecords > 0) LOG_ERR("STATS", "%u raw records dropped", static_cast<unsigned>(s->droppedRecords));
  if (!s->snapshotWritten) writeSnapshot(s->docId, s->record);
  appendToLog(s->docId, s->pending, s->pendingLen);
  LOG_DBG("STATS", "Session %u ended: %us active of %us", static_cast<unsigned>(s->record.sessionSeq),
          static_cast<unsigned>(s->record.activeSec), static_cast<unsigned>(s->record.durationSec));
  finalize(s->docId, s->bookIndex, s->record, s.get());
}

bool ReadingStats::liveSession(SessionRecord& out, bool& excluded) const {
  Lock lock(*this);
  if (!session || !session->startWritten) return false;
  const uint32_t now = millis();
  out = session->record;
  out.activeSec = session->clock.activeMsAt(now) / 1000;
  out.durationSec = (now - session->startMs) / 1000;
  if (isShortOpen(out.activeSec, out.turnsFwd + out.turnsBack + out.jumps)) out.flags |= SESSION_SHORT;
  excluded = session->bookExcluded;
  return true;
}

// --- Finalizing and summaries -------------------------------------------------------------

ReadingStats::AppendResult ReadingStats::appendSession(const SessionRecord& record) {
  HalFile file = Storage.open(SESSIONS_PATH, O_RDWR | O_CREAT);
  if (!file) return AppendResult::WriteFailed;
  if (file.size() == 0) {
    const FileHeader header = makeHeader(MAGIC_SESSIONS, SESSIONS_VERSION, sizeof(FileHeader), sizeof(SessionRecord));
    if (!writeExact(file, &header, sizeof(header))) return AppendResult::WriteFailed;
  } else {
    FileHeader header{};
    if (!readExact(file, &header, sizeof(header)) ||
        !headerMatches(header, MAGIC_SESSIONS, SESSIONS_VERSION, sizeof(FileHeader), sizeof(SessionRecord))) {
      return AppendResult::BadHeader;
    }
  }
  // A torn earlier append leaves a partial record: write on the record grid.
  const size_t records = (file.size() - sizeof(FileHeader)) / sizeof(SessionRecord);
  const bool written =
      file.seek(sizeof(FileHeader) + records * sizeof(SessionRecord)) && writeExact(file, &record, sizeof(record));
  return written ? AppendResult::Ok : AppendResult::WriteFailed;
}

void ReadingStats::invalidateSummaries() {
  if (Storage.exists(DAYS_PATH)) Storage.remove(DAYS_PATH);
  if (Storage.exists(TOTALS_PATH)) Storage.remove(TOTALS_PATH);
}

// Order: the index first (the summaries derive from it), then the learned model, then the
// summaries. A reset between steps leaves summaries that prepareSummaries() rebuilds.
void ReadingStats::finalize(const char* docId, const uint16_t bookIndex, const SessionRecord& record,
                            const Session* learned) {
  const AppendResult appended = appendSession(record);
  const bool indexed = appended == AppendResult::Ok;
  if (appended == AppendResult::BadHeader) {
    // Not a usable index: the next prepareSummaries() rebuilds it from the logs, which hold this session.
    LOG_ERR("STATS", "sessions.bin not recognised; rebuilding it from the logs later");
    Storage.remove(SESSIONS_PATH);
    invalidateSummaries();
  } else if (appended == AppendResult::WriteFailed) {
    // current.bin stays, so recovery retries the append; the summaries are rebuilt after it.
    LOG_ERR("STATS", "sessions.bin write failed; retried at the next start");
    invalidateSummaries();
  }

  if (learned) {
    HalFile file = Storage.open(REGISTRY_PATH, O_RDWR);
    RegistryHeader header{};
    auto book = makeUniqueNoThrow<BookRecord>();
    if (book && file && readRegistryHeader(file, header) && bookIndex < registryCount(file) &&
        file.seek(bookOffset(bookIndex)) && readExact(file, book.get(), sizeof(BookRecord)) &&
        memcmp(book->docId, docId, DOC_ID_LEN) == 0) {
      memcpy(book->rateHist, learned->bookHist, sizeof(book->rateHist));
      book->rateSamples = learned->bookSamples;
      book->typicalPageChars = learned->clock.typicalPageChars();
      mergeRateHist(header.globalHist, header.globalSamples, learned->globalAdd, learned->globalAddSamples);
      if (!file.seek(bookOffset(bookIndex)) || !writeExact(file, book.get(), sizeof(BookRecord)) || !file.seek(0) ||
          !writeExact(file, &header, sizeof(header))) {
        LOG_ERR("STATS", "Registry write failed");
      }
    } else {
      LOG_ERR("STATS", "Registry entry %u not updated", bookIndex);
    }
  }

  if (indexed) {
    const bool excluded = bookExcluded(bookIndex);
    ApplyResult result;
    bool ok = updateTotals(bookIndex, excluded, record, result);
    if (ok && result.calendar) {
      ok = updateDays(result.day, 0);
    } else if (ok && result.counted && !excluded) {
      ok = updateDays(DayRecord{}, record.activeSec);
    }
    if (!ok) invalidateSummaries();
  }
  if (appended != AppendResult::WriteFailed && Storage.exists(CURRENT_PATH)) Storage.remove(CURRENT_PATH);
}

// Summaries are only created by rebuildSummaries(); a missing, older or differently built
// file makes the caller invalidate them.
bool ReadingStats::updateTotals(const uint16_t bookIndex, const bool excluded, const SessionRecord& record,
                                ApplyResult& result) {
  HalFile file;
  FileHeader header{};
  if (!openValid(TOTALS_PATH, MAGIC_TOTALS, TOTALS_VERSION, sizeof(FileHeader), sizeof(BookTotals), file, header,
                 O_RDWR) ||
      header.flags != summaryFlags()) {
    return false;
  }
  const size_t count = (file.size() - sizeof(FileHeader)) / sizeof(BookTotals);
  const auto offsetOf = [](const size_t i) { return sizeof(FileHeader) + i * sizeof(BookTotals); };
  BookTotals totals{};
  if (bookIndex < count) {
    if (!file.seek(offsetOf(bookIndex)) || !readExact(file, &totals, sizeof(totals))) return false;
  } else {
    // Books without counted sessions have no entry yet: fill up to this one.
    const BookTotals empty{};
    if (!file.seek(offsetOf(count))) return false;
    for (size_t i = count; i < bookIndex; ++i) {
      if (!writeExact(file, &empty, sizeof(empty))) return false;
    }
  }
  result = applySession(totals, excluded, record, ignoreShortOpens());
  return file.seek(offsetOf(bookIndex)) && writeExact(file, &totals, sizeof(totals));
}

bool ReadingStats::updateDays(const DayRecord& add, const uint32_t unknownTimeAdd) {
  HalFile file;
  DaysHeader header{};
  if (!Storage.exists(DAYS_PATH)) return false;
  file = Storage.open(DAYS_PATH, O_RDWR);
  if (!file || !readExact(file, &header, sizeof(header)) ||
      !headerMatches(header.file, MAGIC_DAYS, DAYS_VERSION, sizeof(DaysHeader), sizeof(DayRecord)) ||
      header.file.flags != summaryFlags()) {
    return false;
  }
  if (unknownTimeAdd > 0) {
    header.unknownTimeActiveSec += unknownTimeAdd;
    if (!file.seek(0) || !writeExact(file, &header, sizeof(header))) return false;
  }
  if (add.sessions == 0) return true;

  const size_t count = (file.size() - sizeof(DaysHeader)) / sizeof(DayRecord);
  const auto offsetOf = [](const size_t i) { return sizeof(DaysHeader) + i * sizeof(DayRecord); };
  if (count == 0) return file.seek(offsetOf(0)) && writeExact(file, &add, sizeof(add));
  // Sessions end in time order, so the day is nearly always the last one or a new one.
  constexpr size_t MAX_BACK_SEARCH = 64;
  for (size_t back = 0; back < count && back < MAX_BACK_SEARCH; ++back) {
    const size_t i = count - 1 - back;
    DayRecord day{};
    if (!file.seek(offsetOf(i)) || !readExact(file, &day, sizeof(day))) return false;
    if (day.day == add.day) {
      mergeDay(day, add);
      return file.seek(offsetOf(i)) && writeExact(file, &day, sizeof(day));
    }
    if (day.day < add.day) {
      if (back != 0) return false;  // belongs between two days: rebuild
      return file.seek(offsetOf(count)) && writeExact(file, &add, sizeof(add));
    }
  }
  return false;
}

bool ReadingStats::rebuildSummaries() {
  const unsigned long startMs = millis();
  std::vector<uint8_t> excluded;
  loadHiddenFlags(excluded);
  const size_t count = excluded.size();
  // Peak: 41 bytes per book plus 16 per reading day.
  std::vector<BookTotals> totals(count, BookTotals{});
  std::vector<DayRecord> days;
  {
    HalFile old;
    DaysHeader oldHeader{};
    const bool hadDays = Storage.exists(DAYS_PATH) && Storage.openFileForRead("STATS", DAYS_PATH, old);
    days.reserve(hadDays && old.size() > sizeof(oldHeader) ? (old.size() - sizeof(oldHeader)) / sizeof(DayRecord) + 8
                                                           : 64);
  }
  uint32_t unknownTime = 0;
  const bool ignoreShort = ignoreShortOpens();
  {
    HalFile sessions;
    FileHeader header{};
    if (openValid(SESSIONS_PATH, MAGIC_SESSIONS, SESSIONS_VERSION, sizeof(FileHeader), sizeof(SessionRecord), sessions,
                  header)) {
      SessionRecord record{};
      while (readExact(sessions, &record, sizeof(record))) {
        if (record.bookIndex >= count) continue;
        const bool hidden = excluded[record.bookIndex] != 0;
        const ApplyResult result = applySession(totals[record.bookIndex], hidden, record, ignoreShort);
        if (result.calendar) {
          addDay(days, result.day);
        } else if (result.counted && !hidden) {
          unknownTime += record.activeSec;
        }
      }
    }
  }

  bool ok;
  {
    const std::string tmp = std::string(TOTALS_PATH) + ".tmp";
    HalFile file;
    const FileHeader header =
        makeHeader(MAGIC_TOTALS, TOTALS_VERSION, sizeof(FileHeader), sizeof(BookTotals), summaryFlags());
    ok = Storage.openFileForWrite("STATS", tmp.c_str(), file) && writeExact(file, &header, sizeof(header)) &&
         (totals.empty() || writeExact(file, totals.data(), totals.size() * sizeof(BookTotals)));
    file.close();
    ok = ok && Storage.replaceFile(tmp.c_str(), TOTALS_PATH);
  }
  {
    const std::string tmp = std::string(DAYS_PATH) + ".tmp";
    HalFile file;
    DaysHeader header{};
    header.file = makeHeader(MAGIC_DAYS, DAYS_VERSION, sizeof(DaysHeader), sizeof(DayRecord), summaryFlags());
    header.unknownTimeActiveSec = unknownTime;
    bool written = Storage.openFileForWrite("STATS", tmp.c_str(), file) && writeExact(file, &header, sizeof(header)) &&
                   (days.empty() || writeExact(file, days.data(), days.size() * sizeof(DayRecord)));
    file.close();
    written = written && Storage.replaceFile(tmp.c_str(), DAYS_PATH);
    ok = ok && written;
  }
  if (!ok) invalidateSummaries();
  LOG_INF("STATS", "Summaries rebuilt: %u books, %u days, %lu ms%s", static_cast<unsigned>(count),
          static_cast<unsigned>(days.size()), millis() - startMs, ok ? "" : " (write failed)");
  return ok;
}

bool ReadingStats::rebuildFromLogs() {
  if (!loadIdCache()) return false;
  HalFile registry;
  RegistryHeader header{};
  if (!Storage.openFileForRead("STATS", REGISTRY_PATH, registry) || !readRegistryHeader(registry, header)) {
    return false;
  }
  const int count = registryCount(registry);
  // The running session is finalized at its end; replaying its partial log would count it twice.
  uint32_t liveSeq = 0;
  {
    Lock lock(*this);
    if (session) liveSeq = session->record.sessionSeq;
  }
  auto book = makeUniqueNoThrow<BookRecord>();
  auto replay = makeUniqueNoThrow<LogReplay>();
  if (!book || !replay) {
    LOG_ERR("STATS", "OOM: log replay");
    return false;
  }
  const std::string tmp = std::string(SESSIONS_PATH) + ".tmp";
  HalFile out;
  const FileHeader outHeader = makeHeader(MAGIC_SESSIONS, SESSIONS_VERSION, sizeof(FileHeader), sizeof(SessionRecord));
  if (!Storage.openFileForWrite("STATS", tmp.c_str(), out) || !writeExact(out, &outHeader, sizeof(outHeader))) {
    return false;
  }

  uint32_t written = 0;
  for (int i = 0; i < count; ++i) {
    if (!registry.seek(bookOffset(i)) || !readExact(registry, book.get(), sizeof(BookRecord))) break;
    replay->begin(static_cast<uint16_t>(i),
                  chooseRate(book->rateHist, book->rateSamples, header.globalHist, header.globalSamples),
                  book->typicalPageChars, liveSeq);
    written += replayLog(book->docId, *replay, out);
  }
  out.close();
  registry.close();
  if (!Storage.replaceFile(tmp.c_str(), SESSIONS_PATH)) return false;
  LOG_INF("STATS", "Session index rebuilt from logs: %u sessions", static_cast<unsigned>(written));
  return rebuildSummaries();
}

void ReadingStats::recoverCrashedSession() {
  if (session || !Storage.exists(CURRENT_PATH)) return;
  CurrentRecord current{};
  {
    HalFile file;
    if (!Storage.openFileForRead("STATS", CURRENT_PATH, file) || !readExact(file, &current, sizeof(current)) ||
        !headerMatches(current.file, MAGIC_CURRENT, CURRENT_VERSION, sizeof(CurrentRecord), 0)) {
      file.close();
      Storage.remove(CURRENT_PATH);
      return;
    }
  }
  SessionRecord& r = current.session;
  if (lastIndexedSeq() == r.sessionSeq) {
    // The reset came during finalize: the summaries may hold the session or not.
    Storage.remove(CURRENT_PATH);
    invalidateSummaries();
    return;
  }
  const bool closedInLog = repairLog(current.docId, r.bookIndex, r.sessionSeq, r);
  if (!closedInLog) {
    r.endReason = END_CRASH;
    r.flags |= SESSION_CRASHED;
    if (isShortOpen(r.activeSec, r.turnsFwd + r.turnsBack + r.jumps)) r.flags |= SESSION_SHORT;
    const uint32_t endUtc = hasWallTime(r) ? r.startUtc + r.durationSec : 0;
    const RawEnd end = makeEnd(r, END_CRASH, r.durationSec * 1000, endUtc);
    appendToLog(current.docId, reinterpret_cast<const uint8_t*>(&end), sizeof(end));
  }
  LOG_INF("STATS", "Recovered session %u after a reset", static_cast<unsigned>(r.sessionSeq));
  finalize(current.docId, r.bookIndex, r, nullptr);
}

bool ReadingStats::sessionsUsable() {
  HalFile file;
  FileHeader header{};
  return openValid(SESSIONS_PATH, MAGIC_SESSIONS, SESSIONS_VERSION, sizeof(FileHeader), sizeof(SessionRecord), file,
                   header);
}

bool ReadingStats::summariesStale() {
  HalFile file;
  FileHeader header{};
  if (!openValid(TOTALS_PATH, MAGIC_TOTALS, TOTALS_VERSION, sizeof(FileHeader), sizeof(BookTotals), file, header) ||
      header.flags != summaryFlags()) {
    return true;
  }
  file.close();
  DaysHeader days{};
  if (!Storage.exists(DAYS_PATH) || !Storage.openFileForRead("STATS", DAYS_PATH, file) ||
      !readExact(file, &days, sizeof(days))) {
    return true;
  }
  return !headerMatches(days.file, MAGIC_DAYS, DAYS_VERSION, sizeof(DaysHeader), sizeof(DayRecord)) ||
         days.file.flags != summaryFlags();
}

void ReadingStats::prepareSummaries() {
  if (!Storage.exists(REGISTRY_PATH)) return;
  if (!loadIdCache()) return;
  recoverCrashedSession();
  if (!sessionsUsable()) {
    if (Storage.exists(SESSIONS_PATH)) Storage.remove(SESSIONS_PATH);
    rebuildFromLogs();
  } else if (summariesStale()) {
    rebuildSummaries();
  }
}
