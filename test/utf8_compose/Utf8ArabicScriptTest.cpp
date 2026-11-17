#include <gtest/gtest.h>

#include "Utf8.h"

TEST(Utf8ArabicScript, BlockEdges) {
  for (const uint32_t cp : {0x0600u, 0x06FFu, 0x0750u, 0x077Fu, 0x08A0u, 0x08FFu, 0xFB50u, 0xFDFFu, 0xFE70u, 0xFEFFu}) {
    EXPECT_TRUE(utf8IsArabicScript(cp)) << std::hex << cp;
  }
  for (const uint32_t cp : {0x05FFu, 0x0700u, 0x074Fu, 0x0780u, 0x089Fu, 0x0900u, 0xFB4Fu, 0xFE00u, 0xFE6Fu, 0xFF00u}) {
    EXPECT_FALSE(utf8IsArabicScript(cp)) << std::hex << cp;
  }
}

TEST(Utf8ArabicScript, FindsArabicLettersInAWord) {
  EXPECT_TRUE(utf8ContainsArabicScriptLetter("\xD9\x83\xD8\xAA\xD8\xA7\xD8\xA8"));   // Arabic/Persian letters
  EXPECT_TRUE(utf8ContainsArabicScriptLetter("(\xDA\xAF\xD9\x84)"));                 // Persian gaf in brackets
  EXPECT_TRUE(utf8ContainsArabicScriptLetter("abc\xEF\xBB\xBB"));                    // presentation form U+FEFB
  EXPECT_TRUE(utf8ContainsArabicScriptLetter("\xD9\xA1\xD8\xA8"));                   // digit + letter
  EXPECT_FALSE(utf8ContainsArabicScriptLetter("\xD9\xA1\xD9\xA2\xDB\xB3"));          // Arabic-Indic digits only
  EXPECT_FALSE(utf8ContainsArabicScriptLetter("reading"));                           // Latin
  EXPECT_FALSE(utf8ContainsArabicScriptLetter("\xD7\xA9\xD7\x9C\xD7\x95\xD7\x9D"));  // Hebrew keeps focus reading
  EXPECT_FALSE(utf8ContainsArabicScriptLetter(""));
}
