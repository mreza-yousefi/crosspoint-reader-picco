#include "BackupEngine.h"

#include <HalStorage.h>
#include <LibraryBuilder.h>
#include <Logging.h>
#include <SecureClient.h>
#include <TrustedTime.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <base64.h>
#include <esp_rom_crc.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "BackupConfig.h"

namespace backup {
namespace {

// PEM of the server certificate, pinned as the only trusted root for https://
// (for a self-signed server, its own certificate). Kept on the SD card so no
// server details are compiled in.
constexpr char CA_PATH[] = "/.crosspoint/backup-ca.pem";
constexpr size_t CA_MAX_BYTES = 8192;

constexpr char MANIFEST_DIR[] = "/.crosspoint/backup";
constexpr char MANIFEST_PATH[] = "/.crosspoint/backup/manifest.tsv";
constexpr char MANIFEST_TMP[] = "/.crosspoint/backup/manifest.tmp";
// Small files are compared by content: progress.bin keeps its size and the
// FAT date is unreliable while the clock is unset.
constexpr uint64_t SMALL_FILE_BYTES = 64 * 1024;
constexpr size_t IO_CHUNK = 4096;
constexpr uint32_t IO_TIMEOUT_MS = 20000;
constexpr size_t MANIFEST_SAVE_EVERY = 10;
// Server folder whose files the reader fetches into /books and then deletes
// on the server (File Browser upload queue).
constexpr char DROP_DIR[] = "/to-reader/";
constexpr size_t PROPFIND_MAX_BYTES = 64 * 1024;
constexpr uint32_t DROP_SETTLE_MS = 2000;

// Server clock from the last response's Date header (epoch seconds, 0 = none).
// The home server keeps network time; the reader's RTC may not.
int64_t lastServerEpoch = 0;

// "sun, 01 nov 2026 21:35:12 gmt" (lower-cased RFC 1123) -> epoch seconds, or 0.
int64_t parseHttpDate(const std::string& v) {
  static const char* const MONTHS[] = {"jan", "feb", "mar", "apr", "may", "jun",
                                       "jul", "aug", "sep", "oct", "nov", "dec"};
  int day = 0, year = 0, hh = 0, mm = 0, ss = 0;
  char mon[4] = {};
  const size_t comma = v.find(',');
  if (comma == std::string::npos ||
      sscanf(v.c_str() + comma + 1, " %d %3s %d %d:%d:%d", &day, mon, &year, &hh, &mm, &ss) != 6) {
    return 0;
  }
  int month = 0;
  while (month < 12 && strcmp(mon, MONTHS[month]) != 0) month++;
  if (month == 12 || year < 2025) return 0;
  // Days from civil (Howard Hinnant), month 1..12.
  const int m = month + 1;
  const int y = year - (m <= 2 ? 1 : 0);
  const int era = y / 400;
  const int yoe = y - era * 400;
  const int doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + day - 1;
  const int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  const int64_t days = static_cast<int64_t>(era) * 146097 + doe - 719468;
  return days * 86400 + hh * 3600 + mm * 60 + ss;
}

struct Entry {
  uint64_t size = 0;
  uint32_t mtime = 0;
  uint32_t crc = 0;
};

struct Candidate {
  std::string path;
  Entry entry;
};

bool startsWith(const std::string& s, const char* prefix) { return s.rfind(prefix, 0) == 0; }

bool endsWith(const std::string& s, const char* suffix) {
  const size_t n = strlen(suffix);
  return s.size() >= n && s.compare(s.size() - n, n, suffix) == 0;
}

// Which directories to descend into. `path` has no trailing slash.
bool wantDir(const std::string& path) {
  static const char* const SKIP_ROOT[] = {"/.Spotlight-V100", "/.fseventsd", "/.Trashes", "/System Volume Information",
                                          "/.onyx"};
  for (const char* skip : SKIP_ROOT) {
    if (path == skip) return false;
  }
  if (startsWith(path, "/.crosspoint/")) {
    const std::string sub = path.substr(strlen("/.crosspoint/"));
    // Layout caches are rebuilt by the reader; keep only their progress.bin.
    return startsWith(sub, "bookmarks") || startsWith(sub, "book-settings") || startsWith(sub, "clippings") ||
           startsWith(sub, "epub_") || startsWith(sub, "xtc_");
  }
  return true;
}

bool wantFile(const std::string& dir, const std::string& name) {
  if (startsWith(name, "._") || name == ".DS_Store" || endsWith(name, ".part") || endsWith(name, ".tmp")) return false;
  if (name.find_first_of("\t\n") != std::string::npos) return false;  // manifest is TSV
  if (dir == "/.crosspoint") {
    // State only; credential stores (wifi, koreader, opds tokens) stay on the device.
    return name == "settings.json" || name == "state.json" || name == "recent.json" || name == "opds.json";
  }
  if (startsWith(dir, "/.crosspoint/epub_") || startsWith(dir, "/.crosspoint/xtc_")) return name == "progress.bin";
  return true;
}

uint32_t fileCrc(HalFile& file) {
  static uint8_t buf[IO_CHUNK];
  uint32_t crc = 0;
  file.seekSet(0);
  while (true) {
    const int n = file.read(buf, sizeof(buf));
    if (n <= 0) break;
    crc = esp_rom_crc32_le(crc, buf, static_cast<uint32_t>(n));
  }
  file.seekSet(0);
  return crc;
}

void scan(std::vector<Candidate>& out) {
  std::vector<std::string> dirs{"/"};
  char name[256];
  while (!dirs.empty()) {
    const std::string dir = dirs.back();
    dirs.pop_back();
    HalFile d = Storage.open(dir.c_str());
    if (!d || !d.isDirectory()) continue;
    const std::string prefix = dir == "/" ? "/" : dir + "/";
    while (true) {
      HalFile f = d.openNextFile();
      if (!f) break;
      f.getName(name, sizeof(name));
      const std::string path = prefix + name;
      if (f.isDirectory()) {
        if (wantDir(path)) dirs.push_back(path);
        continue;
      }
      if (!wantFile(dir == "/" ? "/" : dir, name)) continue;
      Candidate c{path, {f.fileSize64(), f.modificationTime(), 0}};
      if (c.entry.size <= SMALL_FILE_BYTES) c.entry.crc = fileCrc(f);
      out.push_back(std::move(c));
    }
  }
}

void loadManifest(std::unordered_map<std::string, Entry>& manifest) {
  HalFile f;
  if (!Storage.openFileForRead("BACKUP", MANIFEST_PATH, f)) return;
  std::string line;
  char c = 0;
  while (f.read(&c, 1) == 1) {
    if (c != '\n') {
      line.push_back(c);
      continue;
    }
    char path[512];
    unsigned long long size = 0;
    unsigned long mtime = 0;
    unsigned long crc = 0;
    if (sscanf(line.c_str(), "%511[^\t]\t%llu\t%lu\t%lu", path, &size, &mtime, &crc) == 4) {
      manifest[path] = Entry{size, static_cast<uint32_t>(mtime), static_cast<uint32_t>(crc)};
    }
    line.clear();
  }
}

bool saveManifest(const std::unordered_map<std::string, Entry>& manifest) {
  Storage.ensureDirectoryExists(MANIFEST_DIR);
  HalFile f;
  if (!Storage.openFileForWrite("BACKUP", MANIFEST_TMP, f)) return false;
  char line[600];
  for (const auto& [path, e] : manifest) {
    const int n =
        snprintf(line, sizeof(line), "%s\t%llu\t%lu\t%lu\n", path.c_str(), static_cast<unsigned long long>(e.size),
                 static_cast<unsigned long>(e.mtime), static_cast<unsigned long>(e.crc));
    if (n <= 0 || static_cast<size_t>(n) >= sizeof(line) || f.write(line, n) != static_cast<size_t>(n)) {
      f.close();
      return false;
    }
  }
  f.close();
  return Storage.replaceFile(MANIFEST_TMP, MANIFEST_PATH);
}

bool unchanged(const std::unordered_map<std::string, Entry>& manifest, const Candidate& c) {
  const auto it = manifest.find(c.path);
  if (it == manifest.end() || it->second.size != c.entry.size) return false;
  return c.entry.size <= SMALL_FILE_BYTES ? it->second.crc == c.entry.crc : it->second.mtime == c.entry.mtime;
}

std::string percentEncode(const std::string& path) {
  static const char kHexDigits[] = "0123456789ABCDEF";
  std::string out;
  out.reserve(path.size());
  for (const unsigned char ch : path) {
    if (isalnum(ch) || ch == '/' || ch == '-' || ch == '_' || ch == '.' || ch == '~') {
      out.push_back(static_cast<char>(ch));
    } else {
      out.push_back('%');
      out.push_back(kHexDigits[ch >> 4]);
      out.push_back(kHexDigits[ch & 0x0F]);
    }
  }
  return out;
}

Result classify(int status) {
  if (status < 0) return Result::NetworkError;
  if (status == 401 || status == 403) return Result::AuthFailed;
  return Result::ServerError;
}

// Minimal keep-alive WebDAV client: MKCOL and PUT with a body streamed from
// the SD card (SecureHttpClient only sends in-memory bodies).
class DavClient {
 public:
  bool begin(const BackupConfig& config) {
    std::string url = config.serverUrl;
    https_ = startsWith(url, "https://");
    if (!https_ && !startsWith(url, "http://")) return false;
    url.erase(0, https_ ? 8 : 7);
    const size_t slash = url.find('/');
    std::string hostPort = url.substr(0, slash);
    basePath_ = slash == std::string::npos ? "" : url.substr(slash);
    while (!basePath_.empty() && basePath_.back() == '/') basePath_.pop_back();
    const size_t colon = hostPort.find(':');
    host_ = hostPort.substr(0, colon);
    port_ =
        colon == std::string::npos ? (https_ ? 443 : 80) : static_cast<uint16_t>(atoi(hostPort.c_str() + colon + 1));
    hostHeader_ = hostPort;
    const std::string credentials = config.username + ":" + config.password;
    auth_ = std::string("Basic ") + base64::encode(credentials.c_str()).c_str();
    if (https_) {
      // Never fall back to an unverified connection: no certificate, no run.
      if (!Storage.readFileToString("BACKUP", CA_PATH, CA_MAX_BYTES, ca_) || ca_.empty()) {
        LOG_ERR("BACKUP", "No server certificate at %s", CA_PATH);
        return false;
      }
      tls_.setCACert(ca_.c_str());
      client_ = &tls_;
    } else {
      client_ = &plain_;
    }
    return !host_.empty();
  }

  void end() {
    if (client_) client_->stop();
  }

  const std::string& basePath() const { return basePath_; }

  // Receives response body bytes; return false to abort (e.g. SD write failed).
  using BodySink = std::function<bool(const uint8_t* data, size_t len)>;

  // Returns the HTTP status, or -1 on a transport failure. `extraHeaders` are
  // complete "Name: value\r\n" lines. Without a sink the body is discarded.
  int request(const char* method, const std::string& remotePath, HalFile* body, uint64_t bodyLen,
              const char* extraHeaders = "", const BodySink& sink = nullptr) {
    // A kept-alive connection the server already closed fails on first use;
    // retry once on a fresh one. Not after a sink saw data: it is not rewindable.
    for (int attempt = 0; attempt < 2; attempt++) {
      const bool reused = client_->connected();
      sinkCalled_ = false;
      const int status = requestOnce(method, remotePath, body, bodyLen, extraHeaders, sink);
      if (status >= 0 || !reused || sinkCalled_) return status;
      if (body) body->seekSet(0);
    }
    return -1;
  }

  // True when the last request's body did not arrive complete.
  bool bodyFailed() const { return bodyFailed_; }

 private:
  bool ensureConnected() {
    if (client_->connected()) return true;
    client_->stop();
    if (!freeink::hostResolves(host_.c_str())) return false;
    return client_->connect(host_.c_str(), port_) == 1;
  }

  bool writeAll(const uint8_t* data, size_t len) {
    const uint32_t deadline = millis() + IO_TIMEOUT_MS;
    while (len > 0) {
      const size_t n = client_->write(data, len);
      if (n == 0) {
        if (millis() > deadline || !client_->connected()) return false;
        delay(2);
        continue;
      }
      data += n;
      len -= n;
    }
    return true;
  }

  bool readLine(std::string& line) {
    line.clear();
    const uint32_t deadline = millis() + IO_TIMEOUT_MS;
    while (millis() < deadline) {
      if (client_->available() <= 0) {
        if (!client_->connected()) return false;
        delay(2);
        continue;
      }
      const int c = client_->read();
      if (c < 0) continue;
      if (c == '\n') {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        return true;
      }
      if (line.size() < 1024) line.push_back(static_cast<char>(c));
    }
    return false;
  }

  // Reads `count` body bytes into the sink (or discards them). With
  // untilClose, reads until the server closes the connection instead.
  bool readBody(size_t count, const BodySink& sink, const bool untilClose = false) {
    static uint8_t buf[IO_CHUNK];
    uint32_t deadline = millis() + IO_TIMEOUT_MS;
    while ((untilClose || count > 0) && millis() < deadline) {
      if (client_->available() <= 0) {
        if (!client_->connected()) return untilClose;
        delay(2);
        continue;
      }
      const size_t want = untilClose || count > sizeof(buf) ? sizeof(buf) : count;
      const int n = client_->read(buf, want);
      if (n <= 0) continue;
      deadline = millis() + IO_TIMEOUT_MS;  // timeout is per stall, not per body
      if (!untilClose) count -= static_cast<size_t>(n);
      if (sink) {
        sinkCalled_ = true;
        if (!sink(buf, static_cast<size_t>(n))) return false;
      }
    }
    return !untilClose && count == 0;
  }

  int requestOnce(const char* method, const std::string& remotePath, HalFile* body, uint64_t bodyLen,
                  const char* extraHeaders, const BodySink& sink) {
    bodyFailed_ = true;
    if (!ensureConnected()) return -1;
    char lenText[24];
    snprintf(lenText, sizeof(lenText), "%llu", static_cast<unsigned long long>(bodyLen));
    std::string head;
    head.reserve(256 + remotePath.size());
    head += method;
    head += ' ';
    head += percentEncode(basePath_ + remotePath);
    head += " HTTP/1.1\r\nHost: " + hostHeader_ + "\r\nAuthorization: " + auth_ +
            "\r\nUser-Agent: CrossPoint/" CROSSPOINT_VERSION "\r\nContent-Length: " + lenText +
            "\r\nConnection: keep-alive\r\n" + extraHeaders + "\r\n";
    if (!writeAll(reinterpret_cast<const uint8_t*>(head.data()), head.size())) return fail();

    if (body) {
      static uint8_t buf[IO_CHUNK];
      uint64_t left = bodyLen;
      while (left > 0) {
        const int n = body->read(buf, left < sizeof(buf) ? static_cast<size_t>(left) : sizeof(buf));
        if (n <= 0 || !writeAll(buf, static_cast<size_t>(n))) return fail();
        left -= static_cast<uint64_t>(n);
      }
    }

    std::string line;
    if (!readLine(line) || line.size() < 12 || !startsWith(line, "HTTP/1.")) return fail();
    const int status = atoi(line.c_str() + 9);
    long contentLength = -1;
    bool chunked = false;
    bool close = false;
    while (readLine(line) && !line.empty()) {
      for (auto& ch : line) ch = static_cast<char>(tolower(static_cast<unsigned char>(ch)));
      if (startsWith(line, "content-length:")) contentLength = atol(line.c_str() + 15);
      if (startsWith(line, "transfer-encoding:") && line.find("chunked") != std::string::npos) chunked = true;
      if (startsWith(line, "connection:") && line.find("close") != std::string::npos) close = true;
      if (startsWith(line, "date:")) {
        const int64_t t = parseHttpDate(line.substr(5));
        if (t > 0) lastServerEpoch = t;
      }
    }
    // Only 2xx bodies go to the sink; error bodies are drained.
    const BodySink& out = status >= 200 && status < 300 ? sink : BodySink();
    if (chunked) {
      while (true) {
        if (!readLine(line)) return fail(status);
        const long size = strtol(line.c_str(), nullptr, 16);
        if (size <= 0) {
          readLine(line);
          break;
        }
        if (!readBody(static_cast<size_t>(size), out) || !readLine(line)) return fail(status);
      }
    } else if (contentLength > 0) {
      if (!readBody(static_cast<size_t>(contentLength), out)) return fail(status);
    } else if (contentLength < 0) {
      if (!readBody(0, out, true)) return fail(status);
      close = true;  // body ended at connection close
    }
    bodyFailed_ = false;
    if (close) client_->stop();
    return status;
  }

  int fail(int status = -1) {
    client_->stop();
    return status;
  }

  std::string ca_;  // must outlive tls_'s use of it (setCACert keeps the pointer)
  freeink::SecureClient tls_;
  WiFiClient plain_;
  Client* client_ = nullptr;
  bool https_ = true;
  std::string host_;
  std::string hostHeader_;
  std::string basePath_;
  std::string auth_;
  uint16_t port_ = 443;
  bool sinkCalled_ = false;
  bool bodyFailed_ = false;
};

std::string percentDecode(const std::string& in) {
  std::string out;
  out.reserve(in.size());
  for (size_t i = 0; i < in.size(); i++) {
    if (in[i] == '%' && i + 2 < in.size() && isxdigit(static_cast<unsigned char>(in[i + 1])) &&
        isxdigit(static_cast<unsigned char>(in[i + 2]))) {
      out.push_back(static_cast<char>(strtol(in.substr(i + 1, 2).c_str(), nullptr, 16)));
      i += 2;
    } else {
      out.push_back(in[i]);
    }
  }
  return out;
}

struct DropFile {
  std::string name;
  uint64_t size = 0;
};

// Files directly inside DROP_DIR, from a Depth-1 PROPFIND multistatus, with
// their getcontentlength. Collections (hrefs ending in '/') and hidden files
// are skipped.
std::vector<DropFile> parseDropListing(const std::string& xml, const std::string& basePath) {
  std::vector<DropFile> files;
  std::string lower = xml;
  for (auto& ch : lower) ch = static_cast<char>(tolower(static_cast<unsigned char>(ch)));
  const std::string prefix = basePath + DROP_DIR;
  // Elements may carry any namespace prefix (D:, d:, none); match on the local name.
  const auto textAfter = [&](const std::string& tag, size_t from, size_t until) -> std::string {
    const size_t at = lower.find(tag + ">", from);
    if (at == std::string::npos || at >= until || (at > 0 && lower[at - 1] == '/')) return {};
    const size_t start = at + tag.size() + 1;
    const size_t end = xml.find('<', start);
    return end == std::string::npos || end > until ? std::string() : xml.substr(start, end - start);
  };
  for (size_t pos = lower.find("response>"); pos != std::string::npos;) {
    const size_t next = lower.find("response>", pos + 9);
    if (lower[pos - 1] == '/') {  // closing tag
      pos = next;
      continue;
    }
    const size_t until = next == std::string::npos ? lower.size() : next;
    std::string href = percentDecode(textAfter("href", pos, until));
    const size_t scheme = href.find("://");
    if (scheme != std::string::npos) href.erase(0, href.find('/', scheme + 3));
    if (!href.empty() && href.rfind(prefix, 0) == 0 && href.back() != '/') {
      const std::string name = href.substr(prefix.size());
      if (!name.empty() && name[0] != '.' && name.find('/') == std::string::npos) {
        files.push_back({name, strtoull(textAfter("getcontentlength", pos, until).c_str(), nullptr, 10)});
      }
    }
    pos = next;
  }
  return files;
}

// "/books/name.epub", or "/books/name (2).epub" ... when that exists.
std::string freeBookPath(const std::string& name) {
  const size_t dot = name.rfind('.');
  const std::string stem = dot == std::string::npos ? name : name.substr(0, dot);
  const std::string ext = dot == std::string::npos ? "" : name.substr(dot);
  std::string path = "/books/" + name;
  for (int n = 2; Storage.exists(path.c_str()) && n < 100; n++)
    path = "/books/" + stem + " (" + std::to_string(n) + ")" + ext;
  return path;
}

// Fetches every file waiting in DROP_DIR into /books, deleting each on the
// server once it is safely on the card.
Result pullDropFolder(DavClient& dav, const std::string& basePath, const uint32_t start, const uint32_t budgetMs,
                      Progress& progress, const ProgressCallback& onProgress) {
  const auto list = [&dav, &basePath](std::vector<DropFile>& out) {
    std::string xml;
    const int status =
        dav.request("PROPFIND", DROP_DIR, nullptr, 0, "Depth: 1\r\n", [&xml](const uint8_t* data, const size_t len) {
          if (xml.size() + len > PROPFIND_MAX_BYTES) return false;
          xml.append(reinterpret_cast<const char*>(data), len);
          return true;
        });
    if (status == 207 && !dav.bodyFailed()) out = parseDropListing(xml, basePath);
    return status;
  };
  std::vector<DropFile> first;
  const int listStatus = list(first);
  if (listStatus == 404) return Result::Ok;  // no drop folder on this server
  if (listStatus != 207 || dav.bodyFailed()) {
    LOG_ERR("BACKUP", "PROPFIND %s -> %d", DROP_DIR, listStatus);
    return listStatus < 0 ? Result::NetworkError : classify(listStatus);
  }
  if (first.empty()) return Result::Ok;
  // A browser upload may still be writing a file: list again shortly and
  // fetch only files whose size did not change. The rest come next run.
  delay(DROP_SETTLE_MS);
  std::vector<DropFile> second;
  if (list(second) != 207) return Result::NetworkError;
  std::vector<DropFile> stable;
  for (const auto& f : second) {
    for (const auto& g : first) {
      if (f.name == g.name && f.size == g.size && f.size > 0) stable.push_back(f);
    }
  }
  if (stable.empty()) return Result::Ok;
  Storage.ensureDirectoryExists("/books");

  for (const auto& file : stable) {
    const std::string& name = file.name;
    if (budgetMs != 0 && millis() - start > budgetMs) return Result::Partial;
    const std::string dest = freeBookPath(name);
    const std::string part = dest + ".part";
    HalFile out;
    if (!Storage.openFileForWrite("BACKUP", part, out)) return Result::StorageError;
    uint64_t received = 0;
    const int status = dav.request("GET", std::string(DROP_DIR) + name, nullptr, 0, "",
                                   [&out, &received](const uint8_t* data, const size_t len) {
                                     if (out.write(data, len) != len) return false;
                                     received += len;
                                     return true;
                                   });
    out.close();
    if (status != 200 || dav.bodyFailed() || received != file.size) {
      Storage.remove(part.c_str());
      LOG_ERR("BACKUP", "GET %s -> %d", name.c_str(), status);
      return status < 0 || status == 200 ? Result::NetworkError : classify(status);
    }
    if (!Storage.rename(part.c_str(), dest.c_str())) {
      Storage.remove(part.c_str());
      return Result::StorageError;
    }
    if (library::isLibraryBookFile(dest)) library::markLibraryIndexDirty();
    // Remove from the queue only now; a failed DELETE just fetches it again
    // next time as "name (2)", never loses it.
    const int del = dav.request("DELETE", std::string(DROP_DIR) + name, nullptr, 0);
    if (del < 200 || del >= 300) LOG_ERR("BACKUP", "DELETE %s -> %d", name.c_str(), del);
    LOG_INF("BACKUP", "Fetched %s (%llu bytes)", dest.c_str(), static_cast<unsigned long long>(received));
    progress.filesFetched++;
    if (onProgress && !onProgress(progress)) return Result::Partial;
  }
  return Result::Ok;
}

}  // namespace

const char* resultLabel(const Result result) {
  switch (result) {
    case Result::Ok:
      return "ok";
    case Result::Partial:
      return "partial";
    case Result::NotConfigured:
      return "not configured";
    case Result::NetworkError:
      return "network error";
    case Result::AuthFailed:
      return "login failed";
    case Result::ServerError:
      return "server error";
    case Result::StorageError:
      return "SD card error";
    case Result::NoCertificate:
      return "no certificate";
  }
  return "?";
}

Result ready(const BackupConfig& config) {
  if (!config.isConfigured()) return Result::NotConfigured;
  if (startsWith(config.serverUrl, "https://") && !Storage.exists(CA_PATH)) return Result::NoCertificate;
  return Result::Ok;
}

Result run(const BackupConfig& config, const uint32_t budgetMs, const ProgressCallback& onProgress) {
  if (const Result notReady = ready(config); notReady != Result::Ok) return notReady;
  const uint32_t start = millis();
  lastServerEpoch = 0;

  DavClient dav;
  if (!dav.begin(config)) return Result::NotConfigured;
  Progress progress;
  // Fetch first: new books arrive even on a short battery run, and the scan
  // below then backs them up in the same run.
  const Result fetched = pullDropFolder(dav, dav.basePath(), start, budgetMs, progress, onProgress);
  if (fetched != Result::Ok) {
    dav.end();
    return fetched;
  }

  std::vector<Candidate> files;
  scan(files);
  std::unordered_map<std::string, Entry> manifest;
  loadManifest(manifest);

  std::vector<const Candidate*> pending;
  for (const auto& c : files) {
    if (!unchanged(manifest, c)) pending.push_back(&c);
  }
  LOG_INF("BACKUP", "Scanned %u files, %u changed", static_cast<unsigned>(files.size()),
          static_cast<unsigned>(pending.size()));

  progress.filesPending = pending.size();
  if (onProgress && !onProgress(progress)) {
    dav.end();
    return Result::Partial;
  }
  if (pending.empty()) {
    dav.end();
    return Result::Ok;
  }

  std::unordered_set<std::string> knownDirs;
  Result result = Result::Ok;
  size_t sinceSave = 0;
  for (const Candidate* c : pending) {
    if (budgetMs != 0 && millis() - start > budgetMs) {
      result = Result::Partial;
      break;
    }
    // Create each parent collection once per run; 405 = already exists.
    for (size_t pos = c->path.find('/', 1); pos != std::string::npos; pos = c->path.find('/', pos + 1)) {
      const std::string dir = c->path.substr(0, pos);
      if (knownDirs.count(dir)) continue;
      const int status = dav.request("MKCOL", dir, nullptr, 0);
      if (status != 201 && status != 405) {
        LOG_ERR("BACKUP", "MKCOL %s -> %d", dir.c_str(), status);
        result = classify(status);
        break;
      }
      knownDirs.insert(dir);
    }
    if (result != Result::Ok) break;

    HalFile f;
    if (!Storage.openFileForRead("BACKUP", c->path, f)) continue;  // vanished since the scan
    const int status = dav.request("PUT", c->path, &f, c->entry.size);
    f.close();
    if (status < 200 || status >= 300) {
      LOG_ERR("BACKUP", "PUT %s -> %d", c->path.c_str(), status);
      result = classify(status);
      break;
    }
    manifest[c->path] = c->entry;
    progress.filesDone++;
    progress.bytesDone += c->entry.size;
    if (++sinceSave >= MANIFEST_SAVE_EVERY) {
      saveManifest(manifest);
      sinceSave = 0;
    }
    if (onProgress && !onProgress(progress)) {
      result = Result::Partial;
      break;
    }
  }
  dav.end();
  if (sinceSave > 0 && !saveManifest(manifest) && result == Result::Ok) result = Result::StorageError;
  LOG_INF("BACKUP", "Uploaded %u/%u files (%llu bytes): %s", static_cast<unsigned>(progress.filesDone),
          static_cast<unsigned>(progress.filesPending), static_cast<unsigned long long>(progress.bytesDone),
          resultLabel(result));
  return result;
}

Result runAndRecord(BackupConfig& config, const uint32_t budgetMs, const ProgressCallback& onProgress) {
  const Result result = run(config, budgetMs, onProgress);
  config.lastResult = static_cast<int>(result);
  if (result == Result::Ok) {
    // Prefer the server's clock (Date header of this run's last reply), then
    // network time; the reader's own clock may run ahead (trustedNow() never
    // goes below its persisted floor).
    if (lastServerEpoch > 0) {
      config.lastSuccessEpoch = lastServerEpoch;
    } else {
      const bool synced = WiFi.status() == WL_CONNECTED && trustedtime::syncNow(3000);
      config.lastSuccessEpoch = synced ? static_cast<int64_t>(time(nullptr)) : trustedtime::trustedNow();
    }
    // Persist the clock floor too: after a restart the clock resumes from it,
    // so it must not lag the backup time we just stored.
    trustedtime::note();
  }
  config.save();
  return result;
}

int64_t secondsSinceLastBackup(const BackupConfig& config) {
  const int64_t now = trustedtime::trustedNow();
  if (config.lastSuccessEpoch <= 0 || now <= 0) return -1;
  // A clock resumed from an older floor can trail the stored time briefly.
  return now < config.lastSuccessEpoch ? 0 : now - config.lastSuccessEpoch;
}

}  // namespace backup
