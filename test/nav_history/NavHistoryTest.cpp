#include <gtest/gtest.h>

#include <cstring>
#include <vector>

#include "NavHistory.h"

namespace {

NavHistory::Position at(uint16_t spine, uint32_t offset) {
  NavHistory::Position pos;
  pos.spine = spine;
  pos.page = static_cast<uint16_t>(offset / 1000);
  pos.pageCount = 40;
  pos.offset = offset;
  pos.hasOffset = true;
  pos.percent = static_cast<uint8_t>(spine);
  return pos;
}

void expectAt(const NavHistory::Position& pos, uint16_t spine, uint32_t offset) {
  EXPECT_EQ(pos.spine, spine);
  EXPECT_TRUE(pos.hasOffset);
  EXPECT_EQ(pos.offset, offset);
}

std::vector<uint8_t> serialize(const NavHistory& history) {
  std::vector<uint8_t> bytes;
  EXPECT_TRUE(history.writeTo([&bytes](const uint8_t* data, size_t size) {
    bytes.insert(bytes.end(), data, data + size);
    return true;
  }));
  return bytes;
}

bool deserialize(NavHistory& history, const std::vector<uint8_t>& bytes, int spineCount) {
  size_t pos = 0;
  return history.readFrom(
      [&](uint8_t* data, size_t size) {
        if (pos + size > bytes.size()) return false;
        std::memcpy(data, bytes.data() + pos, size);
        pos += size;
        return true;
      },
      spineCount);
}

TEST(NavHistory, StartsEmpty) {
  NavHistory history;
  EXPECT_FALSE(history.canBack());
  EXPECT_FALSE(history.canForward());
  EXPECT_FALSE(history.backIsLink());
  EXPECT_EQ(history.size(), 0);
}

TEST(NavHistory, BackReturnsToThePositionLeftAndForwardRedoes) {
  NavHistory history;
  history.push(at(1, 100), false);  // jump 1 -> 5
  ASSERT_TRUE(history.canBack());
  expectAt(history.backTarget(), 1, 100);

  // Read on in chapter 5, then Back.
  expectAt(history.back(at(5, 2500)), 1, 100);
  EXPECT_FALSE(history.canBack());
  ASSERT_TRUE(history.canForward());
  expectAt(history.forwardTarget(), 5, 2500);

  // Forward returns where Back was pressed.
  expectAt(history.forward(at(1, 100)), 5, 2500);
  EXPECT_FALSE(history.canForward());
  expectAt(history.backTarget(), 1, 100);
}

TEST(NavHistory, BackWalksNewestFirst) {
  NavHistory history;
  history.push(at(1, 0), false);
  history.push(at(2, 0), false);
  history.push(at(3, 0), false);
  expectAt(history.back(at(4, 0)), 3, 0);
  expectAt(history.back(at(3, 0)), 2, 0);
  expectAt(history.back(at(2, 0)), 1, 0);
  EXPECT_FALSE(history.canBack());
  expectAt(history.forward(at(1, 0)), 2, 0);
  expectAt(history.forward(at(2, 0)), 3, 0);
  expectAt(history.forward(at(3, 0)), 4, 0);
  EXPECT_FALSE(history.canForward());
}

TEST(NavHistory, NewJumpAfterBackDropsForward) {
  NavHistory history;
  history.push(at(1, 0), false);
  history.push(at(2, 0), false);
  history.back(at(3, 0));
  history.back(at(2, 0));
  ASSERT_TRUE(history.canForward());

  history.push(at(1, 0), false);  // the same place as the newest Back target: no new entry
  EXPECT_FALSE(history.canForward());
  EXPECT_EQ(history.size(), 1);

  history.push(at(7, 50), false);
  EXPECT_FALSE(history.canForward());
  EXPECT_EQ(history.size(), 2);
  expectAt(history.backTarget(), 7, 50);
}

TEST(NavHistory, SamePlaceIsRecordedOnce) {
  NavHistory history;
  history.push(at(2, 300), false);
  history.push(at(2, 300), false);
  EXPECT_EQ(history.size(), 1);
  EXPECT_FALSE(history.backIsLink());
  history.push(at(2, 301), false);
  EXPECT_EQ(history.size(), 2);
}

TEST(NavHistory, LinkFromTheSamePlaceMarksTheEntry) {
  NavHistory history;
  history.push(at(2, 300), false);  // contents jump
  history.push(at(2, 300), true);   // then a link followed from the same place
  EXPECT_EQ(history.size(), 1);
  EXPECT_TRUE(history.backIsLink());
  history.push(at(2, 300), false);  // a later plain jump keeps the mark
  EXPECT_TRUE(history.backIsLink());
}

TEST(NavHistory, KeepsTheNewestFifty) {
  NavHistory history;
  for (uint16_t i = 0; i < 60; ++i) history.push(at(i, 0), false);
  EXPECT_EQ(history.size(), NavHistory::MAX_ENTRIES);
  NavHistory::Position pos = at(60, 0);
  int steps = 0;
  while (history.canBack()) {
    pos = history.back(pos);
    ++steps;
  }
  EXPECT_EQ(steps, NavHistory::MAX_ENTRIES);
  expectAt(pos, 10, 0);
}

TEST(NavHistory, PushWhenFullAfterBackKeepsCap) {
  NavHistory history;
  for (uint16_t i = 0; i < 50; ++i) history.push(at(i, 0), false);
  history.back(at(50, 0));
  history.push(at(99, 0), false);  // replaces the Forward entry, no shift needed
  EXPECT_EQ(history.size(), NavHistory::MAX_ENTRIES);
  expectAt(history.backTarget(), 99, 0);
  expectAt(history.back(at(100, 0)), 99, 0);
  expectAt(history.back(at(99, 0)), 48, 0);
}

TEST(NavHistory, LinkFlagFollowsTheJump) {
  NavHistory history;
  history.push(at(1, 0), false);  // contents jump
  history.push(at(2, 0), true);   // link followed
  EXPECT_TRUE(history.backIsLink());
  history.back(at(9, 0));
  EXPECT_FALSE(history.backIsLink());  // the contents jump is next
  history.forward(at(2, 0));
  EXPECT_TRUE(history.backIsLink());  // the link jump again
}

TEST(NavHistory, PositionWithoutOffsetKeepsPage) {
  NavHistory history;
  NavHistory::Position pos;
  pos.spine = 3;
  pos.page = 12;
  pos.pageCount = 30;
  pos.offset = 999;  // ignored without hasOffset
  pos.percent = 42;
  history.push(pos, false);
  const auto target = history.backTarget();
  EXPECT_FALSE(target.hasOffset);
  EXPECT_EQ(target.offset, 0u);
  EXPECT_EQ(target.page, 12);
  EXPECT_EQ(target.pageCount, 30);
  EXPECT_EQ(target.percent, 42);
}

TEST(NavHistory, SerializeRoundTrip) {
  NavHistory history;
  history.push(at(1, 70000), false);
  history.push(at(2, 0x12345678), true);
  history.push(at(300, 5), false);
  history.back(at(4, 4));

  const auto bytes = serialize(history);
  EXPECT_EQ(bytes.size(), NavHistory::HEADER_SIZE + 3 * NavHistory::ENTRY_SIZE);

  NavHistory loaded;
  ASSERT_TRUE(deserialize(loaded, bytes, 400));
  EXPECT_EQ(loaded.size(), 3);
  EXPECT_TRUE(loaded.canForward());
  EXPECT_TRUE(loaded.backIsLink());
  expectAt(loaded.forwardTarget(), 4, 4);
  expectAt(loaded.back(at(0, 0)), 2, 0x12345678);
  const auto first = loaded.back(at(0, 0));
  expectAt(first, 1, 70000);
  EXPECT_EQ(first.page, 70);
  EXPECT_EQ(first.pageCount, 40);
  EXPECT_EQ(first.percent, 1);
}

TEST(NavHistory, EmptyRoundTrip) {
  NavHistory history;
  const auto bytes = serialize(history);
  EXPECT_EQ(bytes.size(), NavHistory::HEADER_SIZE);
  NavHistory loaded;
  loaded.push(at(1, 1), false);
  ASSERT_TRUE(deserialize(loaded, bytes, 10));
  EXPECT_EQ(loaded.size(), 0);
}

TEST(NavHistory, RejectsBadData) {
  NavHistory history;
  history.push(at(1, 0), false);
  history.push(at(8, 0), false);
  const auto good = serialize(history);
  NavHistory loaded;

  // A spine the book no longer has.
  EXPECT_FALSE(deserialize(loaded, good, 8));
  EXPECT_EQ(loaded.size(), 0);

  auto truncated = good;
  truncated.pop_back();
  EXPECT_FALSE(deserialize(loaded, truncated, 10));
  EXPECT_EQ(loaded.size(), 0);

  auto badVersion = good;
  badVersion[0] = NavHistory::FORMAT_VERSION + 1;
  EXPECT_FALSE(deserialize(loaded, badVersion, 10));

  auto badCursor = good;
  badCursor[2] = 3;  // cursor past count
  EXPECT_FALSE(deserialize(loaded, badCursor, 10));

  auto badCount = good;
  badCount[1] = NavHistory::MAX_ENTRIES + 1;
  EXPECT_FALSE(deserialize(loaded, badCount, 10));

  auto badFlags = good;
  badFlags[NavHistory::HEADER_SIZE + NavHistory::ENTRY_SIZE - 1] = 0x80;
  EXPECT_FALSE(deserialize(loaded, badFlags, 10));
  EXPECT_EQ(loaded.size(), 0);

  EXPECT_FALSE(deserialize(loaded, {}, 10));
}

TEST(NavHistory, StaysSmall) { EXPECT_LE(sizeof(NavHistory), 604u); }

}  // namespace
