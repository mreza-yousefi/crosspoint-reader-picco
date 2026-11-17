#pragma once

#include <cstdint>
#include <optional>

#include "NavHistory.h"

class Epub;

// The reader's Back/Forward history in <book cache>/history.bin.
namespace NavHistoryFile {

// page/pageCount: fallback for an unknown offset, and the book percent shown in the menu.
NavHistory::Position makePosition(const Epub& epub, int spine, int page, int pageCount, std::optional<uint32_t> offset);

// A missing or damaged file leaves the history empty. Imports and removes the
// link back-stack (links.bin) older firmware left.
void load(const Epub& epub, NavHistory& history);
bool save(const Epub& epub, const NavHistory& history);

// Records a jump that leaves this position for a screen that has no history loaded.
void pushToFile(const Epub& epub, int spine, int page, int pageCount, std::optional<uint32_t> offset);

}  // namespace NavHistoryFile
