#include "NavHistoryFile.h"

#include <Epub.h>
#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <string>

namespace NavHistoryFile {

namespace {

std::string filePath(const Epub& epub) { return epub.getCachePath() + "/history.bin"; }

// links.bin: [depth] then depth x (spine u16 LE, page u16 LE), at most 3 entries.
void importLinkStack(const Epub& epub, NavHistory& history) {
  const std::string path = epub.getCachePath() + "/links.bin";
  if (!Storage.exists(path.c_str())) return;
  {
    HalFile f;
    uint8_t data[1 + 3 * 4];
    const int size = Storage.openFileForRead("NAV", path, f) ? f.read(data, sizeof(data)) : 0;
    const int depth = size > 0 ? data[0] : 0;
    if (history.size() == 0 && depth >= 1 && depth <= 3 && size == 1 + depth * 4) {
      for (int i = 0; i < depth; ++i) {
        const uint8_t* p = data + 1 + i * 4;
        const int spine = p[0] | (p[1] << 8);
        if (spine >= epub.getSpineItemsCount()) break;
        history.push(makePosition(epub, spine, p[2] | (p[3] << 8), 0, std::nullopt), /*link=*/true);
      }
      if (history.size() > 0) save(epub, history);
    }
  }
  Storage.remove(path.c_str());
}

}  // namespace

NavHistory::Position makePosition(const Epub& epub, const int spine, const int page, const int pageCount,
                                  const std::optional<uint32_t> offset) {
  NavHistory::Position pos;
  pos.spine = static_cast<uint16_t>(spine);
  pos.page = static_cast<uint16_t>(std::clamp(page, 0, static_cast<int>(UINT16_MAX)));
  pos.pageCount = static_cast<uint16_t>(std::clamp(pageCount, 0, static_cast<int>(UINT16_MAX)));
  pos.hasOffset = offset.has_value();
  pos.offset = offset.value_or(0);
  const float chapterFraction =
      pos.pageCount > 0 ? static_cast<float>(pos.page) / static_cast<float>(pos.pageCount) : 0.0f;
  const float bookFraction = std::clamp(epub.calculateProgress(spine, chapterFraction), 0.0f, 1.0f);
  pos.percent = static_cast<uint8_t>(bookFraction * 100.0f + 0.5f);
  return pos;
}

void load(const Epub& epub, NavHistory& history) {
  history.clear();
  const std::string path = filePath(epub);
  if (Storage.exists(path.c_str())) {
    HalFile f;
    if (Storage.openFileForRead("NAV", path, f) &&
        !history.readFrom(
            [&f](uint8_t* data, const size_t size) { return f.read(data, size) == static_cast<int>(size); },
            epub.getSpineItemsCount())) {
      LOG_ERR("NAV", "Ignoring damaged history file");
    }
  }
  importLinkStack(epub, history);
}

bool save(const Epub& epub, const NavHistory& history) {
  HalFile f;
  const bool saved =
      Storage.openFileForWrite("NAV", filePath(epub), f) &&
      history.writeTo([&f](const uint8_t* data, const size_t size) { return f.write(data, size) == size; });
  if (!saved) LOG_ERR("NAV", "Failed to save history");
  return saved;
}

void pushToFile(const Epub& epub, const int spine, const int page, const int pageCount,
                const std::optional<uint32_t> offset) {
  if (spine < 0 || spine >= epub.getSpineItemsCount()) return;
  // About 600 bytes, freed on return: too large for the stack.
  auto history = makeUniqueNoThrow<NavHistory>();
  if (!history) return;
  load(epub, *history);
  history->push(makePosition(epub, spine, page, pageCount, offset), /*link=*/false);
  save(epub, *history);
}

}  // namespace NavHistoryFile
