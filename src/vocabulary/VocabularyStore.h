#pragma once

#include <cstdint>
#include <optional>
#include <string>

struct VocabularyContext {
  std::string title;
  std::string author;
  std::string series;
  std::optional<float> seriesIndex;
  std::string chapter;
  uint16_t page = 0;
  uint16_t spine = 0;
  uint32_t startOffset = UINT32_MAX;
};

// A definition as it is saved: plain text of at most 2 KiB.
struct VocabularyDefinition {
  std::string text;
  bool truncated = false;
};

class VocabularyStore {
 public:
  static constexpr const char* FILE_PATH = "/Vocabulary.txt";

  // Converts the dictionary's definition to the saved form, so the full
  // definition need not be kept until the record is written.
  static bool prepareDefinition(const std::string& definition, bool htmlDefinition, VocabularyDefinition& out);
  // A failed export must never prevent a successful dictionary lookup.
  static bool append(const VocabularyContext& context, const std::string& word, const std::string& headword,
                     const VocabularyDefinition& definition);
};
