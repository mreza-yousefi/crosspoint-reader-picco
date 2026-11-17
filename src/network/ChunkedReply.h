#pragma once

#include <Print.h>
#include <WebServer.h>

#include <cstring>

// Collects a streamed reply into ~1 KB chunks. Each sendContent() is three
// socket writes; hundreds of tiny writes fill lwIP's send queue (errno 11) and
// stall the single-threaded server for seconds while other requests wait.
// A Print, so serializeJson() writes straight into the chunk buffer.
class ChunkedReply final : public Print {
 public:
  explicit ChunkedReply(WebServer& server) : server_(server) {}
  using Print::write;
  size_t write(uint8_t c) override { return write(&c, 1); }
  size_t write(const uint8_t* data, size_t len) override {
    if (used_ + len > sizeof(buffer_)) flush();
    if (len >= sizeof(buffer_)) {
      server_.sendContent(reinterpret_cast<const char*>(data), len);
      return len;
    }
    memcpy(buffer_ + used_, data, len);
    used_ += len;
    return len;
  }
  void flush() override {
    if (used_ == 0) return;
    server_.sendContent(buffer_, used_);
    used_ = 0;
  }
  // Ends the chunked stream after the buffered data.
  void end() {
    flush();
    server_.sendContent("");
  }

 private:
  WebServer& server_;
  char buffer_[1024];
  size_t used_ = 0;
};
