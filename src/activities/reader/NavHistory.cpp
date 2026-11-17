#include "NavHistory.h"

#include <algorithm>

void NavHistory::push(const Position& from, const bool link) {
  count = cursor;
  // Leaving the place the newest Back target already holds adds no entry.
  if (cursor > 0 && samePlace(entries[cursor - 1], from)) {
    if (link) entries[cursor - 1].flags |= FLAG_LINK;
    return;
  }
  if (count == MAX_ENTRIES) {
    std::copy(entries + 1, entries + MAX_ENTRIES, entries);
    --count;
  }
  Entry& entry = entries[count];
  entry.flags = link ? FLAG_LINK : 0;
  setPosition(entry, from);
  cursor = ++count;
}

bool NavHistory::backIsLink() const { return canBack() && (entries[cursor - 1].flags & FLAG_LINK); }

NavHistory::Position NavHistory::back(const Position& current) {
  Entry& slot = entries[cursor - 1];
  const Position target = toPosition(slot);
  setPosition(slot, current);
  --cursor;
  return target;
}

NavHistory::Position NavHistory::forward(const Position& current) {
  Entry& slot = entries[cursor];
  const Position target = toPosition(slot);
  setPosition(slot, current);
  ++cursor;
  return target;
}

NavHistory::Position NavHistory::toPosition(const Entry& entry) {
  Position pos;
  pos.spine = entry.spine;
  pos.page = entry.page;
  pos.pageCount = entry.pageCount;
  pos.offset = entry.offset;
  pos.hasOffset = entry.flags & FLAG_OFFSET;
  pos.percent = entry.percent;
  return pos;
}

void NavHistory::setPosition(Entry& entry, const Position& pos) {
  entry.spine = pos.spine;
  entry.page = pos.page;
  entry.pageCount = pos.pageCount;
  entry.offset = pos.hasOffset ? pos.offset : 0;
  entry.percent = std::min<uint8_t>(pos.percent, 100);
  entry.flags = static_cast<uint8_t>((entry.flags & FLAG_LINK) | (pos.hasOffset ? FLAG_OFFSET : 0));
}

bool NavHistory::samePlace(const Entry& entry, const Position& pos) {
  if (entry.spine != pos.spine) return false;
  const bool hasOffset = entry.flags & FLAG_OFFSET;
  if (hasOffset && pos.hasOffset) return entry.offset == pos.offset;
  return !hasOffset && !pos.hasOffset && entry.page == pos.page && entry.pageCount == pos.pageCount;
}

bool NavHistory::headerValid(const uint8_t* header) {
  return header[0] == FORMAT_VERSION && header[1] <= MAX_ENTRIES && header[2] <= header[1];
}

// spine u16, page u16, pageCount u16, offset u32, percent u8, flags u8; little-endian.
void NavHistory::encode(const Entry& entry, uint8_t* out) {
  out[0] = entry.spine & 0xFF;
  out[1] = entry.spine >> 8;
  out[2] = entry.page & 0xFF;
  out[3] = entry.page >> 8;
  out[4] = entry.pageCount & 0xFF;
  out[5] = entry.pageCount >> 8;
  for (int i = 0; i < 4; ++i) out[6 + i] = static_cast<uint8_t>(entry.offset >> (8 * i));
  out[10] = entry.percent;
  out[11] = entry.flags;
}

bool NavHistory::decode(const uint8_t* data, const int spineCount, Entry& out) {
  out.spine = static_cast<uint16_t>(data[0] | (data[1] << 8));
  out.page = static_cast<uint16_t>(data[2] | (data[3] << 8));
  out.pageCount = static_cast<uint16_t>(data[4] | (data[5] << 8));
  out.offset = 0;
  for (int i = 0; i < 4; ++i) out.offset |= static_cast<uint32_t>(data[6 + i]) << (8 * i);
  out.percent = data[10];
  out.flags = data[11];
  return out.spine < spineCount && out.percent <= 100 && (out.flags & ~(FLAG_OFFSET | FLAG_LINK)) == 0;
}
