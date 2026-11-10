#include "ReadableTextCounter.h"

#include <Logging.h>
#include <Utf8.h>
#include <XmlParserUtils.h>
#include <strings.h>

#include <cstring>

#include "Epub/VisibleTextUtils.h"
#include "Epub/htmlEntities.h"

ReadableTextCounter::ReadableTextCounter(const StopFn stopNow) : stopNow(stopNow) {
  parser = XML_ParserCreate(nullptr);
  if (!parser) {
    LOG_ERR("RTC", "Couldn't allocate XML parser");
    return;
  }
  XML_SetUserData(parser, this);
  XML_SetDefaultHandlerExpand(parser, defaultHandlerExpand);
  XML_SetElementHandler(parser, startElement, endElement);
  XML_SetCharacterDataHandler(parser, characterData);
}

ReadableTextCounter::~ReadableTextCounter() { destroyXmlParser(parser); }

// A short write ends the stream early; the caller tells a stop from a parse error by the flags.
size_t ReadableTextCounter::write(const uint8_t* buffer, const size_t size) {
  if (!parser || stopped_ || parseFailed_) return 0;
  if (stopNow && stopNow()) {
    stopped_ = true;
    return 0;
  }
  if (XML_Parse(parser, reinterpret_cast<const char*>(buffer), static_cast<int>(size), XML_FALSE) != XML_STATUS_OK) {
    LOG_ERR("RTC", "XML parse error: %s", XML_ErrorString(XML_GetErrorCode(parser)));
    parseFailed_ = true;
    return 0;
  }
  return size;
}

void ReadableTextCounter::finish() {
  if (!parser || stopped_ || parseFailed_) return;
  if (XML_Parse(parser, "", 0, XML_TRUE) != XML_STATUS_OK) {
    LOG_ERR("RTC", "Final XML parse error: %s", XML_ErrorString(XML_GetErrorCode(parser)));
    parseFailed_ = true;
  }
}

void XMLCALL ReadableTextCounter::startElement(void* userData, const XML_Char* name, const XML_Char**) {
  auto* self = static_cast<ReadableTextCounter*>(userData);
  if (strcasecmp(name, "body") == 0) self->insideBody = true;
  if (self->insideBody && (self->nonVisibleDepth > 0 || VisibleTextUtils::isNonVisibleElement(name))) {
    self->nonVisibleDepth++;
  }
}

void XMLCALL ReadableTextCounter::endElement(void* userData, const XML_Char* name) {
  auto* self = static_cast<ReadableTextCounter*>(userData);
  if (self->nonVisibleDepth > 0) self->nonVisibleDepth--;
  if (strcmp(name, "body") == 0) self->insideBody = false;
}

void XMLCALL ReadableTextCounter::characterData(void* userData, const XML_Char* s, const int len) {
  if (len > 0) static_cast<ReadableTextCounter*>(userData)->countText(s, static_cast<size_t>(len));
}

// HTML entities that XML does not define (&nbsp; and the like), as ChapterHtmlSlimParser expands them.
void XMLCALL ReadableTextCounter::defaultHandlerExpand(void* userData, const XML_Char* s, const int len) {
  if (len < 3 || s[0] != '&' || s[len - 1] != ';') return;
  auto* self = static_cast<ReadableTextCounter*>(userData);
  const char* value = lookupHtmlEntity(s, static_cast<size_t>(len));
  if (value) {
    self->countText(value, strlen(value));
  } else {
    self->countText(s, static_cast<size_t>(len));
  }
}

void ReadableTextCounter::countText(const char* s, const size_t len) {
  if (!insideBody || nonVisibleDepth > 0) return;
  const auto* ptr = reinterpret_cast<const unsigned char*>(s);
  const auto* const end = ptr + len;
  while (ptr < end) {
    const unsigned char c = *ptr;
    visible++;
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
      spacePending = readable > 0;
      ptr++;
      continue;
    }
    utf8NextCodepoint(&ptr);
    readable += spacePending ? 2 : 1;
    spacePending = false;
  }
}
