// Minimal strict JSON reader for the frozen configuration. Numbers keep their
// literal token text so integer values are compared exactly.
#pragma once

#include <cstddef>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace mm {

struct JsonValue {
  enum class Kind { kNull, kBool, kNumber, kString, kArray, kObject };
  Kind kind = Kind::kNull;
  bool boolean = false;
  std::string text;               // number token or string value
  std::vector<JsonValue> items;   // array
  std::vector<std::string> keys;  // object
  std::vector<JsonValue> values;  // object

  const JsonValue* member(const std::string& k) const {
    for (std::size_t i = 0; i < keys.size(); ++i)
      if (keys[i] == k) return &values[i];
    return nullptr;
  }
};

class JsonParser {
 public:
  explicit JsonParser(const std::string& s) : s_(s) {}

  bool parse(JsonValue& out, std::string& err) {
    if (!value(out, 0)) {
      err = err_;
      return false;
    }
    ws();
    if (i_ != s_.size()) {
      err = "trailing characters";
      return false;
    }
    return true;
  }

 private:
  char peek() const { return i_ < s_.size() ? s_[i_] : '\0'; }
  void ws() {
    while (i_ < s_.size() && (s_[i_] == ' ' || s_[i_] == '\n' || s_[i_] == '\r' || s_[i_] == '\t')) ++i_;
  }
  bool fail(const char* m) {
    err_ = std::string(m) + " at offset " + std::to_string(i_);
    return false;
  }

  bool value(JsonValue& v, int depth) {
    if (depth > 64) return fail("nesting too deep");
    ws();
    const char c = peek();
    if (c == '{') return object(v, depth);
    if (c == '[') return array(v, depth);
    if (c == '"') {
      v.kind = JsonValue::Kind::kString;
      return string(v.text);
    }
    if (c == 't') return literal("true", v, JsonValue::Kind::kBool, true);
    if (c == 'f') return literal("false", v, JsonValue::Kind::kBool, false);
    if (c == 'n') return literal("null", v, JsonValue::Kind::kNull, false);
    if (c == '-' || (c >= '0' && c <= '9')) return number(v);
    return fail("unexpected character");
  }

  bool literal(const char* word, JsonValue& v, JsonValue::Kind kind, bool b) {
    const std::size_t n = std::strlen(word);
    if (s_.compare(i_, n, word) != 0) return fail("bad literal");
    i_ += n;
    v.kind = kind;
    v.boolean = b;
    return true;
  }

  bool digits() {
    const std::size_t start = i_;
    while (i_ < s_.size() && s_[i_] >= '0' && s_[i_] <= '9') ++i_;
    return i_ > start;
  }

  bool number(JsonValue& v) {
    const std::size_t start = i_;
    if (peek() == '-') ++i_;
    if (!digits()) return fail("bad number");
    if (peek() == '.') {
      ++i_;
      if (!digits()) return fail("bad fraction");
    }
    if (peek() == 'e' || peek() == 'E') {
      ++i_;
      if (peek() == '+' || peek() == '-') ++i_;
      if (!digits()) return fail("bad exponent");
    }
    v.kind = JsonValue::Kind::kNumber;
    v.text = s_.substr(start, i_ - start);
    return true;
  }

  bool string(std::string& out) {
    if (peek() != '"') return fail("expected string");
    ++i_;
    out.clear();
    for (;;) {
      if (i_ >= s_.size()) return fail("unterminated string");
      const char c = s_[i_++];
      if (c == '"') return true;
      if (static_cast<unsigned char>(c) < 0x20U) return fail("control character in string");
      if (c != '\\') {
        out += c;
        continue;
      }
      if (i_ >= s_.size()) return fail("bad escape");
      const char e = s_[i_++];
      switch (e) {
        case '"': out += '"'; break;
        case '\\': out += '\\'; break;
        case '/': out += '/'; break;
        case 'b': out += '\b'; break;
        case 'f': out += '\f'; break;
        case 'n': out += '\n'; break;
        case 'r': out += '\r'; break;
        case 't': out += '\t'; break;
        case 'u': {
          if (i_ + 4U > s_.size()) return fail("bad unicode escape");
          unsigned code = 0U;
          for (int k = 0; k < 4; ++k) {
            const char h = s_[i_++];
            code <<= 4U;
            if (h >= '0' && h <= '9') code |= static_cast<unsigned>(h - '0');
            else if (h >= 'a' && h <= 'f') code |= static_cast<unsigned>(h - 'a' + 10);
            else if (h >= 'A' && h <= 'F') code |= static_cast<unsigned>(h - 'A' + 10);
            else return fail("bad hex digit");
          }
          if (code >= 0x80U) return fail("non-ASCII unicode escape unsupported");
          out += static_cast<char>(code);
          break;
        }
        default:
          return fail("bad escape");
      }
    }
  }

  bool array(JsonValue& v, int depth) {
    ++i_;
    v.kind = JsonValue::Kind::kArray;
    ws();
    if (peek() == ']') {
      ++i_;
      return true;
    }
    for (;;) {
      JsonValue item;
      if (!value(item, depth + 1)) return false;
      v.items.push_back(std::move(item));
      ws();
      if (peek() == ',') {
        ++i_;
        continue;
      }
      if (peek() == ']') {
        ++i_;
        return true;
      }
      return fail("expected , or ]");
    }
  }

  bool object(JsonValue& v, int depth) {
    ++i_;
    v.kind = JsonValue::Kind::kObject;
    ws();
    if (peek() == '}') {
      ++i_;
      return true;
    }
    for (;;) {
      ws();
      std::string key;
      if (!string(key)) return false;
      if (v.member(key) != nullptr) return fail("duplicate key");
      ws();
      if (peek() != ':') return fail("expected :");
      ++i_;
      JsonValue val;
      if (!value(val, depth + 1)) return false;
      v.keys.push_back(key);
      v.values.push_back(std::move(val));
      ws();
      if (peek() == ',') {
        ++i_;
        continue;
      }
      if (peek() == '}') {
        ++i_;
        return true;
      }
      return fail("expected , or }");
    }
  }

  const std::string& s_;
  std::size_t i_ = 0U;
  std::string err_;
};

inline const JsonValue* json_lookup(const JsonValue& root, const std::string& dotted) {
  const JsonValue* cur = &root;
  std::size_t start = 0U;
  for (;;) {
    const std::size_t dot = dotted.find('.', start);
    const std::string part = dotted.substr(start, dot == std::string::npos ? std::string::npos : dot - start);
    if (cur->kind != JsonValue::Kind::kObject) return nullptr;
    cur = cur->member(part);
    if (cur == nullptr || dot == std::string::npos) return cur;
    start = dot + 1U;
  }
}

}  // namespace mm
