#include <gtest/gtest.h>

#include <cstring>
#include <string>

#include "BookSettingsMerge.h"

using namespace booksettings;

namespace {
JsonDocument parse(const char* json) {
  JsonDocument doc;
  EXPECT_FALSE(deserializeJson(doc, json));
  return doc;
}

std::string dump(const JsonDocument& doc) {
  std::string out;
  serializeJson(doc, out);
  return out;
}

int merge(JsonDocument& book, const JsonDocument& before, const JsonDocument& after) {
  return mergeChangedKeys(book.as<JsonObject>(), before.as<JsonObjectConst>(), after.as<JsonObjectConst>());
}

const char* const BEFORE = R"({"textAntiAliasing":1,"lineSpacing":1,"fontSize":14,"sdFontFamilyName":""})";
}  // namespace

TEST(BookSettingsMerge, NoChangeIsDetected) {
  const JsonDocument before = parse(BEFORE);
  const JsonDocument after = parse(BEFORE);
  EXPECT_FALSE(hasChangedKeys(before.as<JsonObjectConst>(), after.as<JsonObjectConst>()));
}

TEST(BookSettingsMerge, NumberAndStringChangesAreDetected) {
  const JsonDocument before = parse(BEFORE);
  const JsonDocument aa = parse(R"({"textAntiAliasing":0,"lineSpacing":1,"fontSize":14,"sdFontFamilyName":""})");
  const JsonDocument font =
      parse(R"({"textAntiAliasing":1,"lineSpacing":1,"fontSize":14,"sdFontFamilyName":"Literata"})");
  EXPECT_TRUE(hasChangedKeys(before.as<JsonObjectConst>(), aa.as<JsonObjectConst>()));
  EXPECT_TRUE(hasChangedKeys(before.as<JsonObjectConst>(), font.as<JsonObjectConst>()));
}

TEST(BookSettingsMerge, WritesOnlyChangedKeysAndKeepsBookValues) {
  const JsonDocument before = parse(BEFORE);
  const JsonDocument after =
      parse(R"({"textAntiAliasing":0,"lineSpacing":1,"fontSize":14,"sdFontFamilyName":"Literata"})");
  JsonDocument book = parse(R"({"textAntiAliasing":1,"lineSpacing":3,"fontSize":18,"sdFontFamilyName":"Bookerly"})");

  EXPECT_EQ(merge(book, before, after), 2);
  EXPECT_EQ(dump(book), R"({"textAntiAliasing":0,"lineSpacing":3,"fontSize":18,"sdFontFamilyName":"Literata"})");
}

TEST(BookSettingsMerge, BookAlreadyMatchingNeedsNoWrite) {
  const JsonDocument before = parse(BEFORE);
  const JsonDocument after = parse(R"({"textAntiAliasing":0,"lineSpacing":1,"fontSize":14,"sdFontFamilyName":""})");
  JsonDocument book = parse(R"({"textAntiAliasing":0,"lineSpacing":2,"fontSize":12,"sdFontFamilyName":""})");

  EXPECT_EQ(merge(book, before, after), 0);
  EXPECT_EQ(dump(book), R"({"textAntiAliasing":0,"lineSpacing":2,"fontSize":12,"sdFontFamilyName":""})");
}

TEST(BookSettingsMerge, AddsChangedKeyMissingFromOlderBookFile) {
  const JsonDocument before = parse(BEFORE);
  const JsonDocument after = parse(R"({"textAntiAliasing":0,"lineSpacing":1,"fontSize":14,"sdFontFamilyName":""})");
  JsonDocument book = parse(R"({"lineSpacing":2})");

  EXPECT_EQ(merge(book, before, after), 1);
  EXPECT_EQ(dump(book), R"({"lineSpacing":2,"textAntiAliasing":0})");
}

// Same pattern as CrossPointSettings::readingSettingsToJson (const method, char[] field):
// without the cast ArduinoJson keeps a pointer and the snapshot follows later edits.
TEST(BookSettingsMerge, SnapshotKeepsCharArrayValueAfterSourceChanges) {
  struct Settings {
    char sdFontFamilyName[32] = "Bookerly";
    void toJson(JsonDocument& doc) const { doc["sdFontFamilyName"] = static_cast<const char*>(sdFontFamilyName); }
  } settings;
  JsonDocument snapshot;
  settings.toJson(snapshot);
  std::strcpy(settings.sdFontFamilyName, "Literata");
  EXPECT_STREQ(snapshot["sdFontFamilyName"].as<const char*>(), "Bookerly");
}

// settings.json while a book is open: the book's values are in the fields, the
// defaults are written over them; other settings keep their current values.
TEST(BookSettingsMerge, SaveDefaultsOverBookValues) {
  JsonDocument saved =
      parse(R"({"textAntiAliasing":0,"lineSpacing":3,"fontSize":18,"sdFontFamilyName":"Bookerly","orientation":2})");
  const JsonDocument defaults = parse(BEFORE);

  saveDefaultsOver(saved.as<JsonObject>(), defaults.as<JsonObjectConst>());
  EXPECT_EQ(dump(saved),
            R"({"textAntiAliasing":1,"lineSpacing":1,"fontSize":14,"sdFontFamilyName":"","orientation":2})");
}

TEST(BookSettingsMerge, SaveDefaultsAddsKeyTheSavedDocOmits) {
  // toJson() leaves sdFontFamilyName out when the book uses the built-in font.
  JsonDocument saved = parse(R"({"fontSize":18})");
  const JsonDocument defaults = parse(R"({"fontSize":14,"sdFontFamilyName":"Literata"})");

  saveDefaultsOver(saved.as<JsonObject>(), defaults.as<JsonObjectConst>());
  EXPECT_EQ(dump(saved), R"({"fontSize":14,"sdFontFamilyName":"Literata"})");
}

// toJson() stores the built-in family as 1 (STORED_NOTOSANS); readingSettingsToJson()
// writes the in-memory 0, which fromJson() reads as legacy NotoSerif. Saving with a
// book open must keep the stored form and the empty SD family.
TEST(BookSettingsMerge, SaveDefaultsKeepsStoredFontFamily) {
  JsonDocument saved = parse(R"({"fontFamily":1,"fontSize":18,"sdFontFamilyName":"Bookerly"})");
  const JsonDocument defaults = parse(R"({"fontFamily":0,"fontSize":14,"sdFontFamilyName":""})");

  saveDefaultsOver(saved.as<JsonObject>(), defaults.as<JsonObjectConst>());
  EXPECT_EQ(saved["fontFamily"].as<int>(), 1);
  EXPECT_STREQ(saved["sdFontFamilyName"].as<const char*>(), "");
  EXPECT_EQ(saved["fontSize"].as<int>(), 14);
}

TEST(BookSettingsMerge, MergedStringOutlivesSourceDocuments) {
  JsonDocument book = parse(R"({"sdFontFamilyName":""})");
  {
    const JsonDocument before = parse(R"({"sdFontFamilyName":""})");
    const JsonDocument after = parse(R"({"sdFontFamilyName":"Literata"})");
    EXPECT_EQ(merge(book, before, after), 1);
  }
  EXPECT_EQ(dump(book), R"({"sdFontFamilyName":"Literata"})");
}
