// Minimal JSON value + parser/serializer (stdlib only), so core/ stays Qt-free
// and headless-testable. Covers what the launcher needs: mapping.json round-trip,
// Steam appdetails / Web-API responses, and (later) Epic .item / GOG metadata.
//
// Not a spec-perfect implementation — it handles objects, arrays, strings (with
// escapes), numbers, bool and null, which is all Valve/Epic/GOG emit. Object key
// order is preserved for stable diffs.

#pragma once

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace ss::json {

struct Value;
using Array = std::vector<Value>;
using Object = std::vector<std::pair<std::string, Value>>;  // ordered

struct Value {
    enum class Type { Null, Bool, Number, String, Array, Object } type = Type::Null;
    bool b = false;
    double num = 0;
    std::string str;          // also holds the raw number text for lossless ints
    Array arr;
    Object obj;

    static Value makeObject() { Value v; v.type = Type::Object; return v; }
    static Value makeArray()  { Value v; v.type = Type::Array;  return v; }
    static Value makeString(std::string s) { Value v; v.type = Type::String; v.str = std::move(s); return v; }
    static Value makeBool(bool x) { Value v; v.type = Type::Bool; v.b = x; return v; }
    static Value makeNumber(double x) { Value v; v.type = Type::Number; v.num = x; return v; }

    bool isObject() const { return type == Type::Object; }
    bool isArray()  const { return type == Type::Array; }
    bool isString() const { return type == Type::String; }

    // Object child lookup; nullptr if absent or not an object.
    const Value* get(const std::string& key) const;
    Value* get(const std::string& key);

    // Convenience accessors with defaults.
    std::string asString(const std::string& def = "") const { return type == Type::String ? str : def; }
    bool asBool(bool def = false) const { return type == Type::Bool ? b : def; }

    // Insert/overwrite an object member (preserves position of an existing key).
    Value& set(const std::string& key, Value v);
};

// Parse JSON text; on error returns a Null value and sets ok=false (if provided).
Value parse(const std::string& text, bool* ok = nullptr);

// Serialize. `indent` >= 0 pretty-prints with that many spaces; <0 is compact.
std::string dump(const Value& v, int indent = 2);

}  // namespace ss::json
