// Hymnal - a small JSON reader and writer (ACE-Step's requests and results)
#pragma once
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace Json {

struct Value {
    enum Type { NUL, BOOL, NUM, STR, ARR, OBJ } type = NUL;
    bool b = false;
    double n = 0;
    std::string s;
    std::vector<Value> a;
    std::vector<std::pair<std::string, Value>> o;
    const Value* get(const std::string& k) const { for (const auto& kv : o) if (kv.first == k) return &kv.second; return nullptr; }
    std::string str(const std::string& k, const std::string& def = "") const {
        const Value* v = get(k);
        if (!v) return def;
        if (v->type == STR) return v->s;
        if (v->type == NUM) { char b[64]; snprintf(b, sizeof b, "%g", v->n); return b; }
        return def;
    }
    double num(const std::string& k, double def = 0) const { const Value* v = get(k); return v && v->type == NUM ? v->n : (v && v->type == STR ? atof(v->s.c_str()) : def); }
};

inline void skip(const std::string& t, size_t& i) { while (i < t.size() && (t[i] == ' ' || t[i] == '\n' || t[i] == '\r' || t[i] == '\t')) ++i; }

inline bool parse(const std::string& t, size_t& i, Value& v);

inline bool parseString(const std::string& t, size_t& i, std::string& out) {
    if (i >= t.size() || t[i] != '"') return false;
    ++i;
    while (i < t.size() && t[i] != '"') {
        char c = t[i++];
        if (c == '\\' && i < t.size()) {
            const char e = t[i++];
            if (e == 'n') out += '\n'; else if (e == 't') out += '\t'; else if (e == 'r') out += '\r';
            else if (e == 'b') out += '\b'; else if (e == 'f') out += '\f';
            else if (e == 'u' && i + 4 <= t.size()) {
                unsigned cp = (unsigned)strtoul(t.substr(i, 4).c_str(), nullptr, 16); i += 4;
                if (cp >= 0xD800 && cp < 0xDC00 && i + 6 <= t.size() && t[i] == '\\' && t[i + 1] == 'u') {
                    const unsigned lo = (unsigned)strtoul(t.substr(i + 2, 4).c_str(), nullptr, 16); i += 6;
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                }
                if (cp < 0x80) out += (char)cp;
                else if (cp < 0x800) { out += (char)(0xC0 | (cp >> 6)); out += (char)(0x80 | (cp & 0x3F)); }
                else if (cp < 0x10000) { out += (char)(0xE0 | (cp >> 12)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
                else { out += (char)(0xF0 | (cp >> 18)); out += (char)(0x80 | ((cp >> 12) & 0x3F)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
            } else out += e;
        } else out += c;
    }
    if (i >= t.size()) return false;
    ++i;
    return true;
}

inline bool parse(const std::string& t, size_t& i, Value& v) {
    skip(t, i);
    if (i >= t.size()) return false;
    const char c = t[i];
    if (c == '{') {
        v.type = Value::OBJ; ++i; skip(t, i);
        if (i < t.size() && t[i] == '}') { ++i; return true; }
        for (;;) {
            skip(t, i);
            std::string k;
            if (!parseString(t, i, k)) return false;
            skip(t, i);
            if (i >= t.size() || t[i] != ':') return false;
            ++i;
            Value x;
            if (!parse(t, i, x)) return false;
            v.o.emplace_back(k, std::move(x));
            skip(t, i);
            if (i < t.size() && t[i] == ',') { ++i; continue; }
            if (i < t.size() && t[i] == '}') { ++i; return true; }
            return false;
        }
    }
    if (c == '[') {
        v.type = Value::ARR; ++i; skip(t, i);
        if (i < t.size() && t[i] == ']') { ++i; return true; }
        for (;;) {
            Value x;
            if (!parse(t, i, x)) return false;
            v.a.push_back(std::move(x));
            skip(t, i);
            if (i < t.size() && t[i] == ',') { ++i; continue; }
            if (i < t.size() && t[i] == ']') { ++i; return true; }
            return false;
        }
    }
    if (c == '"') { v.type = Value::STR; return parseString(t, i, v.s); }
    if (t.compare(i, 4, "true") == 0) { v.type = Value::BOOL; v.b = true; i += 4; return true; }
    if (t.compare(i, 5, "false") == 0) { v.type = Value::BOOL; v.b = false; i += 5; return true; }
    if (t.compare(i, 4, "null") == 0) { v.type = Value::NUL; i += 4; return true; }
    char* end = nullptr;
    v.type = Value::NUM;
    v.n = strtod(t.c_str() + i, &end);
    if (end == t.c_str() + i) return false;
    i = (size_t)(end - t.c_str());
    return true;
}

inline bool parse(const std::string& t, Value& v) { size_t i = 0; return parse(t, i, v); }

inline std::string quote(const std::string& s) {
    std::string o = "\"";
    for (unsigned char c : s) {
        if (c == '"') o += "\\\""; else if (c == '\\') o += "\\\\"; else if (c == '\n') o += "\\n"; else if (c == '\r') o += "\\r";
        else if (c == '\t') o += "\\t"; else if (c < 0x20) { char b[8]; snprintf(b, sizeof b, "\\u%04x", c); o += b; }
        else o += (char)c;
    }
    return o + "\"";
}

// a flat object, written field by field
struct Writer {
    std::string out = "{";
    bool first = true;
    Writer& key(const char* k) { if (!first) out += ","; first = false; out += quote(k) + ":"; return *this; }
    Writer& s(const char* k, const std::string& v) { key(k); out += quote(v); return *this; }
    Writer& i(const char* k, long long v) { key(k); out += std::to_string(v); return *this; }
    Writer& f(const char* k, double v) { key(k); char b[64]; snprintf(b, sizeof b, "%.6g", v); out += b; return *this; }
    Writer& b(const char* k, bool v) { key(k); out += v ? "true" : "false"; return *this; }
    std::string str() const { return out + "}"; }
};

}  // namespace Json
