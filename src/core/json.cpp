#include "json.h"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <sstream>

namespace ss::json {

const Value* Value::get(const std::string& key) const {
    if (type != Type::Object) return nullptr;
    for (const auto& kv : obj) if (kv.first == key) return &kv.second;
    return nullptr;
}

Value* Value::get(const std::string& key) {
    if (type != Type::Object) return nullptr;
    for (auto& kv : obj) if (kv.first == key) return &kv.second;
    return nullptr;
}

Value& Value::set(const std::string& key, Value v) {
    type = Type::Object;
    for (auto& kv : obj) if (kv.first == key) { kv.second = std::move(v); return kv.second; }
    obj.emplace_back(key, std::move(v));
    return obj.back().second;
}

namespace {

struct Parser {
    const std::string& s;
    size_t i = 0;
    bool ok = true;

    explicit Parser(const std::string& str) : s(str) {}

    void skipWs() {
        while (i < s.size()) {
            char c = s[i];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') ++i;
            else break;
        }
    }

    Value parseValue() {
        skipWs();
        if (i >= s.size()) { ok = false; return {}; }
        char c = s[i];
        switch (c) {
            case '{': return parseObject();
            case '[': return parseArray();
            case '"': return Value::makeString(parseString());
            case 't': case 'f': return parseBool();
            case 'n': return parseNull();
            default: return parseNumber();
        }
    }

    std::string parseString() {
        std::string out;
        if (s[i] != '"') { ok = false; return out; }
        ++i;
        while (i < s.size()) {
            char c = s[i++];
            if (c == '"') return out;
            if (c == '\\' && i < s.size()) {
                char e = s[i++];
                switch (e) {
                    case 'n': out.push_back('\n'); break;
                    case 't': out.push_back('\t'); break;
                    case 'r': out.push_back('\r'); break;
                    case 'b': out.push_back('\b'); break;
                    case 'f': out.push_back('\f'); break;
                    case '/': out.push_back('/'); break;
                    case '"': out.push_back('"'); break;
                    case '\\': out.push_back('\\'); break;
                    case 'u': {
                        if (i + 4 > s.size()) { ok = false; return out; }
                        unsigned cp = (unsigned)std::stoul(s.substr(i, 4), nullptr, 16);
                        i += 4;
                        // Minimal UTF-8 encode (BMP only; surrogate pairs left as-is).
                        if (cp < 0x80) out.push_back((char)cp);
                        else if (cp < 0x800) {
                            out.push_back((char)(0xC0 | (cp >> 6)));
                            out.push_back((char)(0x80 | (cp & 0x3F)));
                        } else {
                            out.push_back((char)(0xE0 | (cp >> 12)));
                            out.push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
                            out.push_back((char)(0x80 | (cp & 0x3F)));
                        }
                        break;
                    }
                    default: out.push_back(e); break;
                }
            } else {
                out.push_back(c);
            }
        }
        ok = false;
        return out;
    }

    Value parseObject() {
        Value v = Value::makeObject();
        ++i;  // {
        skipWs();
        if (i < s.size() && s[i] == '}') { ++i; return v; }
        while (i < s.size()) {
            skipWs();
            if (s[i] != '"') { ok = false; return v; }
            std::string key = parseString();
            skipWs();
            if (i >= s.size() || s[i] != ':') { ok = false; return v; }
            ++i;
            v.obj.emplace_back(key, parseValue());
            skipWs();
            if (i < s.size() && s[i] == ',') { ++i; continue; }
            if (i < s.size() && s[i] == '}') { ++i; return v; }
            ok = false; return v;
        }
        ok = false; return v;
    }

    Value parseArray() {
        Value v = Value::makeArray();
        ++i;  // [
        skipWs();
        if (i < s.size() && s[i] == ']') { ++i; return v; }
        while (i < s.size()) {
            v.arr.push_back(parseValue());
            skipWs();
            if (i < s.size() && s[i] == ',') { ++i; continue; }
            if (i < s.size() && s[i] == ']') { ++i; return v; }
            ok = false; return v;
        }
        ok = false; return v;
    }

    Value parseBool() {
        if (s.compare(i, 4, "true") == 0) { i += 4; return Value::makeBool(true); }
        if (s.compare(i, 5, "false") == 0) { i += 5; return Value::makeBool(false); }
        ok = false; return {};
    }

    Value parseNull() {
        if (s.compare(i, 4, "null") == 0) { i += 4; return {}; }
        ok = false; return {};
    }

    Value parseNumber() {
        size_t start = i;
        while (i < s.size()) {
            char c = s[i];
            if ((c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.' ||
                c == 'e' || c == 'E') ++i;
            else break;
        }
        if (i == start) { ok = false; return {}; }
        Value v;
        v.type = Value::Type::Number;
        v.str = s.substr(start, i - start);  // keep raw for lossless ints
        try { v.num = std::stod(v.str); } catch (...) { ok = false; }
        return v;
    }
};

void dumpString(const std::string& s, std::string& out) {
    out.push_back('"');
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\t': out += "\\t"; break;
            case '\r': out += "\\r"; break;
            default: out.push_back(c); break;
        }
    }
    out.push_back('"');
}

void dumpValue(const Value& v, int indent, int depth, std::string& out) {
    auto pad = [&](int d) { if (indent >= 0) out.append((size_t)(indent * d), ' '); };
    auto nl = [&]() { if (indent >= 0) out.push_back('\n'); };
    switch (v.type) {
        case Value::Type::Null: out += "null"; break;
        case Value::Type::Bool: out += v.b ? "true" : "false"; break;
        case Value::Type::Number: out += v.str.empty() ? std::to_string(v.num) : v.str; break;
        case Value::Type::String: dumpString(v.str, out); break;
        case Value::Type::Array:
            if (v.arr.empty()) { out += "[]"; break; }
            out.push_back('['); nl();
            for (size_t k = 0; k < v.arr.size(); ++k) {
                pad(depth + 1);
                dumpValue(v.arr[k], indent, depth + 1, out);
                if (k + 1 < v.arr.size()) out.push_back(',');
                nl();
            }
            pad(depth); out.push_back(']');
            break;
        case Value::Type::Object:
            if (v.obj.empty()) { out += "{}"; break; }
            out.push_back('{'); nl();
            for (size_t k = 0; k < v.obj.size(); ++k) {
                pad(depth + 1);
                dumpString(v.obj[k].first, out);
                out += indent >= 0 ? ": " : ":";
                dumpValue(v.obj[k].second, indent, depth + 1, out);
                if (k + 1 < v.obj.size()) out.push_back(',');
                nl();
            }
            pad(depth); out.push_back('}');
            break;
    }
}

}  // namespace

Value parse(const std::string& text, bool* ok) {
    Parser p(text);
    Value v = p.parseValue();
    if (ok) *ok = p.ok;
    return v;
}

std::string dump(const Value& v, int indent) {
    std::string out;
    dumpValue(v, indent, 0, out);
    return out;
}

}  // namespace ss::json
