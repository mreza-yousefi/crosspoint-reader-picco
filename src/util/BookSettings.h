#pragma once

#include <string>

// Per-book reading settings in /.crosspoint/book-settings/<KOReader document id>.json.
// The id hashes the book's content, so the file follows a renamed or moved book.
namespace booksettings {

// The settings file for a KOReader document id, or "" for an empty id.
std::string pathForDocument(const std::string& documentId);

// Applies the book's saved values to SETTINGS, or creates the file from the
// current SETTINGS when the book has none yet. No-op for an empty path.
void applyOrCreate(const std::string& settingsPath);

// Writes SETTINGS' per-book fields. No-op (returns true) for an empty path.
bool save(const std::string& settingsPath);

// Saves SETTINGS and, for a non-empty path, the book's settings file.
void persist(const std::string& settingsPath);

}  // namespace booksettings
