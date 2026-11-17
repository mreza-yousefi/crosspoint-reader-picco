#pragma once

#include <ArduinoJson.h>

#include <string>

// Per-book reading settings in /.crosspoint/book-settings/<KOReader document id>.json.
// The id hashes the book's content, so the file follows a renamed or moved book.
namespace booksettings {

// The settings file for a KOReader document id, or "" for an empty id.
std::string pathForDocument(const std::string& documentId);

// Applies the book's saved values to SETTINGS, or creates the file from the
// defaults when the book has none yet. Call after SETTINGS.beginBookValues(),
// so settings.json keeps the defaults. No-op for an empty path.
void applyOrCreate(const std::string& settingsPath);

// Writes SETTINGS' per-book fields. No-op (returns true) for an empty path.
bool save(const std::string& settingsPath);

// Saves the book's settings file, or SETTINGS (the defaults) for an empty path.
void persist(const std::string& settingsPath);

// Writes the keys whose values differ between before and after (readingSettingsToJson
// snapshots) into every book's settings file, one file at a time. Unreadable files are skipped.
// Returns how many files could not be read or written.
int applyChangesToAllBooks(JsonObjectConst before, JsonObjectConst after);

}  // namespace booksettings
