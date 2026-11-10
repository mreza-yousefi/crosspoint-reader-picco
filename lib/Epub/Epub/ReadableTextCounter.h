#pragma once

#include <Print.h>
#include <expat.h>

#include <cstddef>
#include <cstdint>

// Counts the text of one XHTML document streamed through write(), without keeping it.
// Visibility and entity rules match ChapterHtmlSlimParser, so visibleChars() is in the units of
// Section::getVisibleTextOffsetForPage(): code points of body text outside head/script/style.
// readableChars() counts the same text with each whitespace run as one character.
// When the counting rules change, bump FILE_VERSION in src/activities/reader/PaperPages.cpp.
class ReadableTextCounter final : public Print {
 public:
  using StopFn = bool (*)();

  // stopNow is polled before each chunk; true stops the count (write() then accepts nothing).
  explicit ReadableTextCounter(StopFn stopNow = nullptr);
  ~ReadableTextCounter() override;
  ReadableTextCounter(const ReadableTextCounter&) = delete;
  ReadableTextCounter& operator=(const ReadableTextCounter&) = delete;

  bool ok() const { return parser != nullptr; }
  size_t write(uint8_t c) override { return write(&c, 1); }
  size_t write(const uint8_t* buffer, size_t size) override;
  // Ends the document. After a parse error the counts cover the text before it.
  void finish();

  bool stopped() const { return stopped_; }
  bool parseFailed() const { return parseFailed_; }
  uint32_t readableChars() const { return readable; }
  uint32_t visibleChars() const { return visible; }

 private:
  static void XMLCALL startElement(void* userData, const XML_Char* name, const XML_Char** atts);
  static void XMLCALL endElement(void* userData, const XML_Char* name);
  static void XMLCALL characterData(void* userData, const XML_Char* s, int len);
  static void XMLCALL defaultHandlerExpand(void* userData, const XML_Char* s, int len);
  void countText(const char* s, size_t len);

  XML_Parser parser = nullptr;
  StopFn stopNow;
  uint32_t readable = 0;
  uint32_t visible = 0;
  uint16_t nonVisibleDepth = 0;
  bool insideBody = false;
  bool spacePending = false;  // whitespace after counted text, counted once the next text arrives
  bool stopped_ = false;
  bool parseFailed_ = false;
};
