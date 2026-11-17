#include <gtest/gtest.h>

#include "activities/reader/LookupRecordGate.h"

TEST(LookupRecordGate, WaitsForTheDefinitionToShow) { EXPECT_FALSE(lookupShownLongEnough(0, 5000)); }

TEST(LookupRecordGate, AQuickCloseIsAMistake) {
  EXPECT_FALSE(lookupShownLongEnough(2000, 2000));
  EXPECT_FALSE(lookupShownLongEnough(2000, 2000 + LOOKUP_MIN_VISIBLE_MS - 1));
}

TEST(LookupRecordGate, RecordsAfterOneSecond) {
  EXPECT_EQ(LOOKUP_MIN_VISIBLE_MS, 1000UL);
  EXPECT_TRUE(lookupShownLongEnough(2000, 2000 + LOOKUP_MIN_VISIBLE_MS));
  EXPECT_TRUE(lookupShownLongEnough(2000, 60000));
}

TEST(LookupRecordGate, SurvivesMillisWrap) {
  constexpr unsigned long shown = static_cast<unsigned long>(-500);
  EXPECT_FALSE(lookupShownLongEnough(shown, 400));  // 900 ms after, across the wrap
  EXPECT_TRUE(lookupShownLongEnough(shown, 500));
}
