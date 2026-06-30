// Minimal parser/writer for Valve's KeyValues text format (.vdf / .acf).
//
// C++ port of core/vdf.py. Kept dependency-free (std only) so the whole `core/`
// layer compiles headless without Qt — mirroring the Python rule that core stays
// stdlib-only. The format:
//
//     "key"
//     {
//         "subkey"   "value"
//         "nested"   { "a" "b" }
//     }
//
// Handles quoted tokens, escape sequences (\\, \", \n, \t) and `//` line comments
// — enough for loginusers.vdf, libraryfolders.vdf, config.vdf and appmanifest_*.acf.
//
// A node is EITHER a leaf string OR an ordered map of child nodes. Order is
// preserved and a repeated key overwrites in place (matching Python dict
// semantics) so writes round-trip Valve's files faithfully.

#pragma once

#include <string>
#include <utility>
#include <vector>

namespace ss::vdf {

struct Value {
    bool is_map = false;
    std::string str;                                  // leaf value
    std::vector<std::pair<std::string, Value>> map;   // children (ordered)

    static Value makeMap() { Value v; v.is_map = true; return v; }
    static Value makeStr(std::string s) { Value v; v.str = std::move(s); return v; }

    // Exact-case child lookup; nullptr if absent or this isn't a map.
    Value* get(const std::string& key);
    const Value* get(const std::string& key) const;

    // Case-insensitive child lookup (localconfig/config casing varies by Steam
    // version) — the C++ analogue of accounts._ci_get / switcher._child.
    Value* getCI(const std::string& key);
    const Value* getCI(const std::string& key) const;

    // Convenience: child leaf string, or `def` when missing / not a leaf.
    std::string getStr(const std::string& key, const std::string& def = "") const;

    // Insert or overwrite a child, preserving position of an existing key.
    Value& set(const std::string& key, Value v);
    Value& setStr(const std::string& key, const std::string& s) { return set(key, makeStr(s)); }
};

// Parse VDF text into a map node (top level may carry one or more keys).
Value loads(const std::string& text);

// Read + parse a file (utf-8, lenient). Returns an empty map on read failure.
Value load(const std::string& path);

// Serialize back to Valve-style text (tabs for indentation).
std::string dumps(const Value& obj, int indent = 0);

// Write dumps(obj) + trailing newline to a file. Returns false on write failure.
bool dump(const Value& obj, const std::string& path);

}  // namespace ss::vdf
