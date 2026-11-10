#include "PaperPages.h"

#include <Arduino.h>
#include <Epub.h>
#include <Epub/ReadableTextCounter.h>
#include <HalMemory.h>
#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>
#include <Serialization.h>

#include <algorithm>

namespace {
// paper_pages.bin: version, spine count, book size (sum of spine item sizes), number of spine items
// counted, then their readable and visible character counts (uint32 each).
constexpr uint8_t FILE_VERSION = 2;
constexpr uint32_t HEADER_SIZE = sizeof(uint8_t) + 3 * sizeof(uint32_t);
// A spine item whose stream fails (not a parse error, which keeps the text before it) is tried
// this many times, then the count stops until the book is opened again.
constexpr uint8_t MAX_TRIES = 3;

// Inflating a spine item briefly takes about 42 KB: a 32 KB window, 8 KB of state, two 1 KB
// buffers. The default heap is where malloc takes them from, PSRAM included where the board has it.
constexpr size_t MIN_FREE_HEAP = 48 * 1024;
constexpr size_t MIN_LARGEST_BLOCK = 36 * 1024;
// Expat's small blocks come from internal RAM even with PSRAM: the chapter build's floor, as it runs
// the same parser.
constexpr size_t MIN_FREE_INTERNAL = 32 * 1024;
constexpr size_t MIN_LARGEST_INTERNAL_BLOCK = 16 * 1024;

std::string filePath(const Epub& epub) { return epub.getCachePath() + "/paper_pages.bin"; }
}  // namespace

void PaperPages::open(const Epub& epub) {
  if (state != State::Closed) return;
  state = State::Unavailable;
  spineCount = epub.getSpineItemsCount();
  if (spineCount <= 0) return;
  // 8 bytes per spine item.
  counts = makeUniqueNoThrow<SpineCount[]>(spineCount);
  if (!counts) {
    LOG_ERR("PPG", "Paper pages table: OOM (%u bytes)", static_cast<unsigned>(spineCount * sizeof(SpineCount)));
    return;
  }
  if (!load(epub)) {
    nextSpine = 0;
    totalReadable = 0;
  }
  if (nextSpine == spineCount) {
    state = State::Ready;
    return;
  }
  LOG_DBG("PPG", "Counting paper pages: spine items %d-%d, %u bytes", nextSpine, spineCount - 1,
          static_cast<unsigned>(epub.getBookSize()));
  state = State::Counting;
}

bool PaperPages::heapAllowsCount() {
  const HalMemory::HeapStats heap = HalMemory::getDefaultHeap();
  const HalMemory::HeapStats internal = HalMemory::getInternalHeap();
  return heap.freeBytes >= MIN_FREE_HEAP && heap.largestBlockBytes >= MIN_LARGEST_BLOCK &&
         internal.freeBytes >= MIN_FREE_INTERNAL && internal.largestBlockBytes >= MIN_LARGEST_INTERNAL_BLOCK;
}

bool PaperPages::countNext(const Epub& epub, bool (*stopNow)()) {
  if (state != State::Counting) return false;
  const unsigned long startMs = millis();
  ReadableTextCounter counter(stopNow);
  if (!counter.ok()) return false;
  const std::string href = epub.getSpineItem(nextSpine).href;
  const bool streamed = !href.empty() && epub.readItemContentsToStream(href, counter, 1024, /*allowEarlyStop=*/true);
  if (counter.stopped()) {
    LOG_DBG("PPG", "Spine %d: stopped by input after %lu ms", nextSpine, millis() - startMs);
    return false;
  }
  if (!streamed && !counter.parseFailed()) {
    if (++failedTries < MAX_TRIES) return false;
    // A partial count would be final; the next open of the book tries again.
    LOG_ERR("PPG", "Spine %d failed to stream %u times; paper pages off until the book is reopened", nextSpine,
            static_cast<unsigned>(MAX_TRIES));
    state = State::Unavailable;
    return false;
  }
  counter.finish();
  const unsigned long itemMs = millis() - startMs;
  countMs += itemMs;
  counts[nextSpine] = {counter.readableChars(), counter.visibleChars()};
  totalReadable += counter.readableChars();
  LOG_DBG("PPG", "Spine %d: %u chars in %lu ms", nextSpine, static_cast<unsigned>(counter.readableChars()), itemMs);
  failedTries = 0;
  nextSpine++;
  save(epub);
  if (nextSpine < spineCount) return false;

  state = State::Ready;
  LOG_DBG("PPG", "Paper pages: %u chars, %u pages, counted in %lu ms", static_cast<unsigned>(totalReadable),
          static_cast<unsigned>(totalPages()), countMs);
  return true;
}

uint32_t PaperPages::totalPages() const {
  return std::max<uint32_t>(1, (totalReadable + CHARS_PER_PAGE / 2) / CHARS_PER_PAGE);
}

uint32_t PaperPages::pageAt(const int spineIndex, const uint32_t part, const uint32_t whole) const {
  if (!ready() || spineIndex < 0 || spineIndex >= spineCount) return 0;
  uint64_t chars = 0;
  for (int i = 0; i < spineIndex; i++) chars += counts[i].readable;
  if (whole > 0) chars += static_cast<uint64_t>(counts[spineIndex].readable) * std::min(part, whole) / whole;
  return static_cast<uint32_t>(std::min<uint64_t>(chars / CHARS_PER_PAGE + 1, totalPages()));
}

uint32_t PaperPages::visibleChars(const int spineIndex) const {
  return ready() && spineIndex >= 0 && spineIndex < spineCount ? counts[spineIndex].visible : 0;
}

// Sets nextSpine and totalReadable from a saved count, complete or partial.
bool PaperPages::load(const Epub& epub) {
  const std::string path = filePath(epub);
  if (!Storage.exists(path.c_str())) return false;
  HalFile f;
  if (!Storage.openFileForRead("PPG", path, f)) return false;
  uint8_t version = 0;
  uint32_t storedSpines = 0;
  uint32_t storedBookSize = 0;
  uint32_t counted = 0;
  if (!serialization::tryReadPod(f, version) || version != FILE_VERSION ||
      !serialization::tryReadPod(f, storedSpines) || storedSpines != static_cast<uint32_t>(spineCount) ||
      !serialization::tryReadPod(f, storedBookSize) || storedBookSize != epub.getBookSize() ||
      !serialization::tryReadPod(f, counted) || counted > storedSpines) {
    LOG_DBG("PPG", "Paper pages cache is stale; counting again");
    return false;
  }
  const size_t tableSize = static_cast<size_t>(counted) * sizeof(SpineCount);
  if (f.size() != HEADER_SIZE + tableSize ||
      f.read(reinterpret_cast<uint8_t*>(counts.get()), tableSize) != static_cast<int>(tableSize)) {
    LOG_DBG("PPG", "Paper pages cache is stale; counting again");
    return false;
  }
  nextSpine = static_cast<int>(counted);
  totalReadable = 0;
  for (int i = 0; i < nextSpine; i++) totalReadable += counts[i].readable;
  return true;
}

// Rewrites the file with the spine items counted so far.
void PaperPages::save(const Epub& epub) const {
  const std::string path = filePath(epub);
  HalFile f;
  if (!Storage.openFileForWrite("PPG", path, f)) return;
  const size_t tableSize = static_cast<size_t>(nextSpine) * sizeof(SpineCount);
  const bool written = serialization::tryWritePod(f, FILE_VERSION) &&
                       serialization::tryWritePod(f, static_cast<uint32_t>(spineCount)) &&
                       serialization::tryWritePod(f, static_cast<uint32_t>(epub.getBookSize())) &&
                       serialization::tryWritePod(f, static_cast<uint32_t>(nextSpine)) &&
                       f.write(reinterpret_cast<const uint8_t*>(counts.get()), tableSize) == tableSize;
  f.close();
  if (!written) {
    LOG_ERR("PPG", "Failed to save %s", path.c_str());
    Storage.remove(path.c_str());
  }
}
