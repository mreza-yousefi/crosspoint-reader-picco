#include "BookSettingsMerge.h"

namespace booksettings {

bool hasChangedKeys(JsonObjectConst before, JsonObjectConst after) {
  for (const JsonPairConst kv : after) {
    if (before[kv.key()] != kv.value()) return true;
  }
  return false;
}

int mergeChangedKeys(JsonObject book, JsonObjectConst before, JsonObjectConst after) {
  int written = 0;
  for (const JsonPairConst kv : after) {
    if (before[kv.key()] == kv.value() || book[kv.key()] == kv.value()) continue;
    book[kv.key()] = kv.value();
    ++written;
  }
  return written;
}

void saveDefaultsOver(JsonObject saved, JsonObjectConst defaults) {
  for (const JsonPairConst kv : defaults) {
    if (kv.key() == "fontFamily") continue;
    saved[kv.key()] = kv.value();
  }
}

}  // namespace booksettings
