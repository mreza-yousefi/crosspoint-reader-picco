#pragma once

#include <Epub/Page.h>
#include <I18n.h>

#include <memory>
#include <string>

#include "activities/Activity.h"
#include "util/Dictionary.h"
#include "vocabulary/VocabularyStore.h"

// Controller for dictionary lookup: renders the background page, runs the
// lookup against the active StarDict dictionary, and either opens
// DictionaryDefinitionActivity or displays a status popup (Indexing, Error),
// or "Not Found" with Another word / Close.
class DictionaryWordSelectActivity final : public Activity {
 public:
  explicit DictionaryWordSelectActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                        std::unique_ptr<Page> page, int marginLeft, int marginTop,
                                        std::string lookupText = {}, VocabularyContext vocabulary = {})
      : Activity("DictionaryWordSelect", renderer, mappedInput),
        page(std::move(page)),
        marginLeft(marginLeft),
        marginTop(marginTop),
        lookupText(std::move(lookupText)),
        vocabulary(std::move(vocabulary)) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class Popup : uint8_t { None, Busy, NotFound, Error };

  void performLookup();
  // Vocabulary history and the stats' lookup count, once the definition has
  // stayed on screen (LookupRecordGate.h).
  void recordLookup();
  void dropLookupRecord();

  // "Not Found" card over the page: 0 = Another word, 1 = Close, -1 = the card.
  struct Box {
    int x;
    int y;
    int width;
    int height;
  };
  Box notFoundBox(int part) const;
  void drawNotFound() const;
  void drawNotFoundButton(int index, bool pressed) const;
  void handleNotFoundInput();
  void closeNotFound(int index, bool showPressed);

  std::unique_ptr<Page> page;
  const int marginLeft;
  const int marginTop;
  int fontId = 0;

  std::string lookupText;
  VocabularyContext vocabulary;
  bool lookupPending = false;
  // Paint the page into the frame buffer without a panel refresh: the definition
  // sheet draws over it and shows both in one refresh.
  bool paintOnly = false;
  Dictionary dict;
  bool dictOpenAttempted = false;
  bool dictOpenOk = false;
  bool dictNeedsIndex = false;

  // Saved form of a found definition, kept until recordLookup().
  bool recordVocabulary = false;
  std::string recordWord;
  std::string recordHeadword;
  VocabularyDefinition recordDefinition;

  Popup popup = Popup::None;
  StrId popupMsg = StrId::STR_DICT_NOT_FOUND;
  unsigned long popupTime = 0;
};
