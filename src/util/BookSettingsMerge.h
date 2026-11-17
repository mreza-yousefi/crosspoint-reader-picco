#pragma once

#include <ArduinoJson.h>

// Pure JSON helpers for per-book settings (no storage), so host tests can cover them.
namespace booksettings {

// True when any key in after has a different value in before.
bool hasChangedKeys(JsonObjectConst before, JsonObjectConst after);

// Copies into book each key whose value differs between before and after.
// The book's other keys are left alone. Returns how many of book's values
// changed (0: nothing to write).
int mergeChangedKeys(JsonObject book, JsonObjectConst before, JsonObjectConst after);

// settings.json while a book is open: writes the defaults (readingSettingsToJson)
// over the book's values in saved; other keys are left alone. "fontFamily" is
// skipped: saved holds its stored form, and the in-memory value 0 would load
// as the legacy NotoSerif.
void saveDefaultsOver(JsonObject saved, JsonObjectConst defaults);

}  // namespace booksettings
