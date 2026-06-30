#include "vdf.h"

#include <cctype>
#include <fstream>
#include <sstream>

namespace ss::vdf {

namespace {

std::string toLower(const std::string& s) {
    std::string out(s.size(), '\0');
    for (size_t i = 0; i < s.size(); ++i)
        out[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(s[i])));
    return out;
}

// A token is either a brace ("{"/"}") or a string ("str", text). We mirror the
// Python generator with a flat vector.
struct Token {
    enum Kind { LBrace, RBrace, Str } kind;
    std::string text;
};

char unescape(char c) {
    switch (c) {
        case 'n': return '\n';
        case 't': return '\t';
        case '"': return '"';
        case '\\': return '\\';
        default: return c;  // unknown escape -> the char itself (matches Python)
    }
}

std::vector<Token> tokenize(const std::string& text) {
    std::vector<Token> out;
    size_t i = 0, n = text.size();
    while (i < n) {
        char c = text[i];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') { ++i; continue; }
        // line comment
        if (c == '/' && i + 1 < n && text[i + 1] == '/') {
            while (i < n && text[i] != '\n') ++i;
            continue;
        }
        if (c == '{') { out.push_back({Token::LBrace, "{"}); ++i; continue; }
        if (c == '}') { out.push_back({Token::RBrace, "}"}); ++i; continue; }
        if (c == '"') {
            ++i;
            std::string buf;
            while (i < n) {
                char ch = text[i];
                if (ch == '\\' && i + 1 < n) { buf.push_back(unescape(text[i + 1])); i += 2; continue; }
                if (ch == '"') { ++i; break; }
                buf.push_back(ch);
                ++i;
            }
            out.push_back({Token::Str, buf});
            continue;
        }
        // bare unquoted token — read until whitespace or brace or quote
        std::string buf;
        while (i < n) {
            char ch = text[i];
            if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n' ||
                ch == '{' || ch == '}' || ch == '"')
                break;
            buf.push_back(ch);
            ++i;
        }
        out.push_back({Token::Str, buf});
    }
    return out;
}

std::string escape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c == '\\') out += "\\\\";
        else if (c == '"') out += "\\\"";
        else out.push_back(c);
    }
    return out;
}

}  // namespace

Value* Value::get(const std::string& key) {
    for (auto& kv : map)
        if (kv.first == key) return &kv.second;
    return nullptr;
}

const Value* Value::get(const std::string& key) const {
    for (auto& kv : map)
        if (kv.first == key) return &kv.second;
    return nullptr;
}

Value* Value::getCI(const std::string& key) {
    if (Value* exact = get(key)) return exact;
    std::string kl = toLower(key);
    for (auto& kv : map)
        if (toLower(kv.first) == kl) return &kv.second;
    return nullptr;
}

const Value* Value::getCI(const std::string& key) const {
    if (const Value* exact = get(key)) return exact;
    std::string kl = toLower(key);
    for (auto& kv : map)
        if (toLower(kv.first) == kl) return &kv.second;
    return nullptr;
}

std::string Value::getStr(const std::string& key, const std::string& def) const {
    const Value* v = get(key);
    if (v && !v->is_map) return v->str;
    return def;
}

Value& Value::set(const std::string& key, Value v) {
    is_map = true;
    for (auto& kv : map) {
        if (kv.first == key) { kv.second = std::move(v); return kv.second; }
    }
    map.emplace_back(key, std::move(v));
    return map.back().second;
}

namespace {

Value parseBlock(const std::vector<Token>& tokens, size_t& pos) {
    Value obj = Value::makeMap();
    while (pos < tokens.size()) {
        const Token& tok = tokens[pos];
        if (tok.kind == Token::RBrace) { ++pos; break; }
        if (tok.kind != Token::Str) { ++pos; continue; }
        std::string key = tok.text;
        ++pos;
        if (pos >= tokens.size()) { obj.set(key, Value::makeStr("")); break; }
        const Token& nxt = tokens[pos];
        if (nxt.kind == Token::LBrace) {
            ++pos;
            obj.set(key, parseBlock(tokens, pos));
        } else if (nxt.kind == Token::Str) {
            obj.set(key, Value::makeStr(nxt.text));
            ++pos;
        } else {
            obj.set(key, Value::makeStr(""));
        }
    }
    return obj;
}

}  // namespace

Value loads(const std::string& text) {
    auto tokens = tokenize(text);
    size_t pos = 0;
    return parseBlock(tokens, pos);
}

Value load(const std::string& path) {
    std::ifstream fh(path, std::ios::binary);
    if (!fh) return Value::makeMap();
    std::ostringstream ss;
    ss << fh.rdbuf();
    return loads(ss.str());
}

std::string dumps(const Value& obj, int indent) {
    std::string pad(static_cast<size_t>(indent), '\t');
    std::vector<std::string> lines;
    for (const auto& kv : obj.map) {
        const std::string& key = kv.first;
        const Value& val = kv.second;
        if (val.is_map) {
            lines.push_back(pad + "\"" + escape(key) + "\"");
            lines.push_back(pad + "{");
            lines.push_back(dumps(val, indent + 1));
            lines.push_back(pad + "}");
        } else {
            lines.push_back(pad + "\"" + escape(key) + "\"\t\t\"" + escape(val.str) + "\"");
        }
    }
    std::string out;
    for (size_t i = 0; i < lines.size(); ++i) {
        out += lines[i];
        if (i + 1 < lines.size()) out += "\n";
    }
    return out;
}

bool dump(const Value& obj, const std::string& path) {
    std::ofstream fh(path, std::ios::binary | std::ios::trunc);
    if (!fh) return false;
    fh << dumps(obj) << "\n";
    return static_cast<bool>(fh);
}

}  // namespace ss::vdf
