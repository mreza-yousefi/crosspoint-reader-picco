"""Run with python3 test/test_readable_text_counter.py (requires a host C/C++ compiler)."""

from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]

STUBS = {
    "Print.h": """#pragma once
#include <cstddef>
#include <cstdint>
class Print {
 public:
  virtual ~Print() = default;
  virtual size_t write(uint8_t value) = 0;
  virtual size_t write(const uint8_t* buffer, size_t size) = 0;
};
""",
    "Logging.h": """#pragma once
#define LOG_DBG(...)
#define LOG_ERR(...)
""",
}

CHECK = r'''
#include <cassert>
#include <cstring>
#include <string>

#include "Epub/ReadableTextCounter.h"

namespace {
struct Counts {
  uint32_t readable;
  uint32_t visible;
  bool parseFailed;
};

Counts count(const std::string& xhtml, size_t chunk = 1024) {
  ReadableTextCounter counter;
  assert(counter.ok());
  for (size_t i = 0; i < xhtml.size(); i += chunk) {
    const size_t n = std::min(chunk, xhtml.size() - i);
    counter.write(reinterpret_cast<const uint8_t*>(xhtml.data() + i), n);
  }
  counter.finish();
  return {counter.readableChars(), counter.visibleChars(), counter.parseFailed()};
}

std::string page(const std::string& body) {
  return "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
         "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.1//EN\" \"http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd\">\n"
         "<html xmlns=\"http://www.w3.org/1999/xhtml\"><head><title>Title text</title>"
         "<style>p { margin: 0; }</style></head>\n<body>" + body + "</body></html>";
}

bool alwaysStop() { return true; }
}  // namespace

int main() {
  // Tags are skipped; each whitespace run counts once; leading and trailing whitespace count zero.
  Counts c = count(page("\n  <p>Hello  \n world</p>\n  <p>again</p>\n"));
  assert(c.readable == std::strlen("Hello world again"));
  assert(c.visible == std::strlen("\n  Hello  \n world\n  again\n"));
  assert(!c.parseFailed);

  // head, title, style and script text is not counted.
  c = count(page("<script>var x = 1;</script><p>ab</p><style>p{}</style>"));
  assert(c.readable == 2 && c.visible == 2);

  // XML and HTML entities expand to their characters; &nbsp; is not whitespace.
  c = count(page("<p>a &amp; b&nbsp;c&#8212;d</p>"));
  assert(c.readable == std::strlen("a & b_c_d"));

  // Code points, not bytes: Persian, accented and CJK text.
  c = count(page("<p dir=\"rtl\">\xD8\xB3\xD9\x84\xD8\xA7\xD9\x85 \xD8\xAF\xD9\x86\xDB\x8C\xD8\xA7</p>"
                 "<p>\xC3\xA9t\xC3\xA9 \xE4\xB8\xAD\xE6\x96\x87</p>"));
  assert(c.readable == 9 + 6);
  assert(c.visible == 9 + 6);

  // An image-only chapter has no text.
  c = count(page("<div><img src=\"cover.jpg\" alt=\"Cover\"/></div>"));
  assert(c.readable == 0 && c.visible == 0);

  // Chunk boundaries do not change the counts, even inside a multibyte character.
  const std::string mixed = page("<p>one\xC3\xA9  two</p>\n<p> three \xE4\xB8\xAD</p>");
  const Counts whole = count(mixed);
  const Counts bytes = count(mixed, 1);
  assert(whole.readable == bytes.readable && whole.visible == bytes.visible);
  assert(whole.readable == std::strlen("one_ two three _"));

  // A parse error keeps the text before it.
  c = count(page("<p>kept</p><p></q>"));
  assert(c.parseFailed && c.readable == 4);

  // A stop request ends the count and refuses more input.
  ReadableTextCounter stopping(&alwaysStop);
  const std::string doc = page("<p>text</p>");
  assert(stopping.write(reinterpret_cast<const uint8_t*>(doc.data()), doc.size()) == 0);
  assert(stopping.stopped() && stopping.readableChars() == 0);
  return 0;
}
'''


if __name__ == "__main__":
    with tempfile.TemporaryDirectory(prefix="readable-text-counter-test-") as directory:
        work = Path(directory)
        for name, content in STUBS.items():
            (work / name).write_text(content)
        (work / "check.cpp").write_text(CHECK)
        defines = ["-DXML_GE=0", "-DXML_CONTEXT_BYTES=0"]
        includes = [f"-I{work}"] + [f"-I{ROOT / d}" for d in ("lib/Epub", "lib/Utf8", "lib/XmlParserUtils", "lib/expat")]
        objects = []
        for source in ("xmlparse.c", "xmlrole.c", "xmltok.c"):
            obj = work / (source + ".o")
            subprocess.run(["cc", "-c", *defines, f"-I{ROOT / 'lib/expat'}", str(ROOT / "lib/expat" / source),
                            "-o", str(obj)], check=True)
            objects.append(str(obj))
        subprocess.run([
            "c++", "-std=c++20", "-Wall", "-Wextra", "-Werror", *defines, *includes,
            str(ROOT / "lib/Epub/Epub/ReadableTextCounter.cpp"), str(ROOT / "lib/Epub/Epub/htmlEntities.cpp"),
            str(ROOT / "lib/Utf8/Utf8.cpp"), str(work / "check.cpp"), *objects,
            "-o", str(work / "check"),
        ], check=True)
        subprocess.run([str(work / "check")], check=True)
    print("ReadableTextCounter checks passed")
