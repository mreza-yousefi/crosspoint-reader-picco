#pragma once

#include <cstddef>
#include <cstdint>

// Per-book Back/Forward history of reader jumps, browser style. Entries before
// the cursor are Back targets (newest last); entries from the cursor on are
// Forward targets (nearest first). No allocation; about 600 bytes.
class NavHistory {
 public:
  struct Position {
    uint16_t spine = 0;
    uint16_t page = 0;       // fallback when the offset is unknown
    uint16_t pageCount = 0;  // chapter page count `page` was counted in
    uint32_t offset = 0;     // visible text offset in the chapter; survives re-layout
    bool hasOffset = false;
    uint8_t percent = 0;  // book percent, shown in the menu
  };

  static constexpr int MAX_ENTRIES = 50;
  // page without offset or page count: the chapter's last page (left from the end-of-book screen).
  static constexpr uint16_t LAST_PAGE = UINT16_MAX;
  static constexpr size_t HEADER_SIZE = 4;
  static constexpr size_t ENTRY_SIZE = 12;
  static constexpr uint8_t FORMAT_VERSION = 1;

  // A jump leaves `from`: Forward entries go, `from` becomes the newest Back
  // target unless it already is (the oldest entry drops when full). link: the
  // jump followed a link; it also marks an existing entry for the same place.
  void push(const Position& from, bool link);
  bool canBack() const { return cursor > 0; }
  bool canForward() const { return cursor < count; }
  // The newest Back target was left by following a link.
  bool backIsLink() const;
  // Only valid while canBack() / canForward().
  Position backTarget() const { return toPosition(entries[cursor - 1]); }
  Position forwardTarget() const { return toPosition(entries[cursor]); }
  // Returns the target and keeps `current` in its slot, so the opposite move returns there.
  Position back(const Position& current);
  Position forward(const Position& current);
  void clear() { count = cursor = 0; }
  int size() const { return count; }

  // write(const uint8_t* data, size_t size) -> bool
  template <typename Write>
  bool writeTo(Write&& write) const {
    const uint8_t header[HEADER_SIZE] = {FORMAT_VERSION, count, cursor, 0};
    if (!write(header, HEADER_SIZE)) return false;
    for (int i = 0; i < count; ++i) {
      uint8_t data[ENTRY_SIZE];
      encode(entries[i], data);
      if (!write(data, ENTRY_SIZE)) return false;
    }
    return true;
  }

  // read(uint8_t* data, size_t size) -> bool (all bytes read). Any invalid
  // data, or a spine outside the book, leaves the history empty.
  template <typename Read>
  bool readFrom(Read&& read, const int spineCount) {
    clear();
    uint8_t header[HEADER_SIZE];
    if (!read(header, HEADER_SIZE) || !headerValid(header)) return false;
    for (int i = 0; i < header[1]; ++i) {
      uint8_t data[ENTRY_SIZE];
      if (!read(data, ENTRY_SIZE) || !decode(data, spineCount, entries[i])) return false;
    }
    count = header[1];
    cursor = header[2];
    return true;
  }

 private:
  struct Entry {
    uint32_t offset;
    uint16_t spine;
    uint16_t page;
    uint16_t pageCount;
    uint8_t percent;
    uint8_t flags;
  };
  static constexpr uint8_t FLAG_OFFSET = 1;
  static constexpr uint8_t FLAG_LINK = 2;

  static Position toPosition(const Entry& entry);
  // Keeps the slot's link flag: it belongs to the jump, not the position.
  static void setPosition(Entry& entry, const Position& pos);
  static bool samePlace(const Entry& entry, const Position& pos);
  static bool headerValid(const uint8_t* header);
  static void encode(const Entry& entry, uint8_t* out);
  static bool decode(const uint8_t* data, int spineCount, Entry& out);

  Entry entries[MAX_ENTRIES] = {};
  uint8_t count = 0;
  uint8_t cursor = 0;
};
