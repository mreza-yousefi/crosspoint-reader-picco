#pragma once

#include <cstdint>
#include <memory>

class Epub;

// Book position in "paper pages": the book's text cut into pages of a typical paperback, so the
// number does not depend on font, margins or orientation. Each spine item's text is counted once
// per book, one item per idle loop frame, and saved in the book cache after each item.
class PaperPages {
 public:
  // A typical paperback page: about 250-300 words, spaces included.
  static constexpr uint32_t CHARS_PER_PAGE = 1800;

  // Loads the saved count, resuming a partial one. Once only; after an allocation failure the
  // book has no paper pages.
  void open(const Epub& epub);
  bool counting() const { return state == State::Counting; }
  bool ready() const { return state == State::Ready; }
  // Free heap for one countNext().
  static bool heapAllowsCount();
  // Counts the next spine item. stopNow is polled per stream chunk; a stopped item starts over on
  // the next call. True when this call completed the book.
  bool countNext(const Epub& epub, bool (*stopNow)());

  uint32_t totalPages() const;
  // 1-based page at part/whole of the way through spine item spineIndex, clamped to totalPages().
  uint32_t pageAt(int spineIndex, uint32_t part, uint32_t whole) const;
  // Text length of the spine item in Section visible-text offset units.
  uint32_t visibleChars(int spineIndex) const;

 private:
  enum class State : uint8_t { Closed, Counting, Ready, Unavailable };
  struct SpineCount {
    uint32_t readable;  // characters, whitespace runs as one
    uint32_t visible;   // code points, as Section counts them
  };

  bool load(const Epub& epub);
  void save(const Epub& epub) const;

  State state = State::Closed;
  std::unique_ptr<SpineCount[]> counts;  // one entry per spine item, held while the book is open
  int spineCount = 0;
  int nextSpine = 0;  // spine items counted so far
  uint8_t failedTries = 0;
  uint32_t totalReadable = 0;
  unsigned long countMs = 0;
};
