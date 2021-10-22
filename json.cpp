#include "json.h"

#include <array>
#include <charconv>

using namespace json;

std::pair<std::string, bool> json::unescape(std::string_view const sv) {
    std::string s;
    s.resize(sv.size());
    size_t dst = 0;
    for (size_t src = 0; src < sv.size();) {
        size_t end = sv.find('\\', src);
        if (end == sv.npos) {
            memcpy(s.data() + dst, sv.data() + src, sv.size() - src);
            dst += sv.size() - src;
            break;
        }
        memcpy(s.data() + dst, sv.data() + src, end - src);
        dst += end - src;
        src = end + 1; // skip '\\'
        assert(src != sv.size());
        if (src == sv.size())
            return { "%failed to unescape: char after '\\' expected%", false };
        if (sv[src] == 'u') {
            src++;
            assert(src + 4 <= sv.size());
            if (src + 4 > sv.size())
                return { "%failed to unescape: 4 HEXDIG chars expected%", false };
            wchar_t wch = 0;
            for (size_t i = 4; i--; src++) {
                if (!is_hex(sv[src]))
                    return { "%failed to unescape: HEXDIG char expected%", false };
                wchar_t const ch = sv[src]>='0'&&sv[src]<='9'?sv[src]-'0':10+(sv[src]>='A'&&sv[src]<='F'?sv[src]-'A':sv[src]-'a');
                wch |= ch << (i * 4);
            }
            s[dst++] = static_cast<char>(wch);
        } else {
            constexpr std::array<char, 8> cvt{ '\"', '\\', '/', '\x08', '\x0C', '\n', '\r', '\t' };
            size_t const idx = std::string_view("\"\\/bfnrt").find(sv[src++]);
            if (idx == sv.npos)
                return { "%failed to unescape: unknown escape char%", false };
            s[dst++] = cvt[idx];
        }
    }
    s.resize(dst);
    return { s, true };
}

std::string json::unescape(std::string_view s, bool escaped) {
    return escaped ? unescape(s).first : std::string(s);
}

std::string json::escape(std::string_view s) {
    return std::string(s); // todo: implement
}

////////////////////////////////////////////////////////////////////////////////

void doc_t::makeError(std::string_view error, reader_t const& reader) const {
    throw std::exception(std::string(error).c_str());
}

std::pair<bool, std::unique_ptr<value_t>> doc_t::parse(std::string_view data, bool const local) {
    reader_t rd{ data };
    parse_ws(rd);
    auto [res, val] = parse_value(rd, local);
    parse_ws(rd);
    return { res, std::move(val) };
}

bool doc_t::parse_ws(reader_t& rd) {
    rd.skip_while(is_ws);
    return true;
}

bool doc_t::parse_comma(reader_t& rd) {
    parse_ws(rd);
    if (!rd.skip(','))
        return false;
    parse_ws(rd);
    return true;
}

std::pair<bool, std::string_view> doc_t::parse_null(reader_t& rd) {
    static const std::string strNull("null");
    if (rd.skip(std::string_view(strNull)))
        return { true, std::string_view(strNull) };
    return {};
}

std::pair<bool, std::string_view> doc_t::parse_false(reader_t& rd) {
    static const std::string strFalse("false");
    if (rd.skip(std::string_view(strFalse)))
        return { true, std::string_view(strFalse) };
    return {};
}

std::pair<bool, std::string_view> doc_t::parse_true(reader_t& rd) {
    static const std::string strTrue("true");
    if (rd.skip(std::string_view(strTrue)))
        return { true, std::string_view(strTrue) };
    return {};
}

std::pair<bool, std::string_view> doc_t::parse_bool(reader_t& rd) {
    if (auto res = parse_true(rd); std::get<bool>(res))
        return res;
    if (auto res = parse_false(rd); std::get<bool>(res))
        return res;
    return {};
}

std::pair<bool, std::string_view> doc_t::parse_number(reader_t& rd) {
    auto const start = rd.position();
    rd.skip('-');
    if (!rd.skip('0')) {
        rd.skip(is_digit1_9);
        rd.skip_while(is_digit);
    }

    bool hasFrac = false;
    if (rd.has('.')) {
        rd.read();
        if (!rd.has(is_digit))
            makeError("frac digit expected", rd);
        rd.skip_while(is_digit);
        hasFrac = true;
    }

    bool hasExp = false;
    if (rd.has('e') || rd.has('E')) {
        rd.read();
        if (!rd.skip('-')) rd.skip('+');
        if (!rd.has(is_digit))
            makeError("exp digit expected", rd);
        rd.skip_while(is_digit);
        hasExp = true;
    }

    if (rd.position() == start)
        return {};

    return { true, std::string_view(rd.get_base_ptr(start), rd.position() - start) };
}

std::tuple<bool, std::string_view, bool> doc_t::parse_string(reader_t& rd) {
    auto start = rd.position();
    if (!rd.skip('\"'))
        return {};
    bool escaped = false;
    while (true) {
        rd.skip_until([](char const ch) { return ch == '\"' || ch == '\\'; });
        if (rd.has('\"'))
            break;
        if (rd.skip('\\')) {
            escaped = true;
            if (rd.skip('u')) {
                for (size_t i = 4; i--; ) {
                    if (!rd.skip(is_hex))
                        makeError("parseString: hex symbol expected", rd);
                }
            }
            else if (!rd.skip(is_escaped)) {
                makeError("parseString: escaped symbol expected", rd);
            }
        }
    }
    if (!rd.skip('\"'))
        makeError("Document::parseString", rd);
    std::string_view s(rd.get_base_ptr(start), rd.position() - start);
    return { true, s, escaped };
}

std::pair<bool, std::unique_ptr<value_t>> doc_t::parse_value(reader_t& rd, bool const local) {
    parse_ws(rd);
    if (auto [hasNull, source] = parse_null(rd); hasNull)
        return { true, std::make_unique<value_t>(value_t::type_t::null, source) };
    if (auto [hasBool, source] = parse_bool(rd); hasBool)
        return { true, std::make_unique<value_t>(value_t::type_t::boolean, source) };
    if (auto [hasNumber, source] = parse_number(rd); hasNumber)
        return { true, std::make_unique<value_t>(value_t::type_t::number, source, false, local) };
    if (auto [hasString, source, escaped] = parse_string(rd); hasString)
        return { true, std::make_unique<value_t>(value_t::type_t::string, source, escaped, local) };
    if (auto [hasArray, aVal] = parse_array(rd, local); hasArray)
        return { true, std::make_unique<value_t>(std::move(aVal)) };
    if (auto [hasObject, oVal] = parse_object(rd, local); hasObject)
        return { true, std::make_unique<value_t>(std::move(oVal)) };
    parse_ws(rd);
    return {};
}

std::pair<bool, std::unique_ptr<array_t>> doc_t::parse_array(reader_t& rd, bool const local) {
    parse_ws(rd);
    if (!rd.skip('['))
        return { false, nullptr };
    parse_ws(rd);

    auto array = std::make_unique<array_t>();
    for (auto res = parse_value(rd, local); res.first; res = parse_value(rd, local)) {
        array->add(std::move(res.second));
        if (!parse_comma(rd))
            break;
    }

    parse_ws(rd);
    if (!rd.skip(']'))
        makeError("parseArray: ']' expected", rd);
    parse_ws(rd);

    return { true, std::move(array) };
}

std::pair<bool, pair_t> doc_t::parse_member(reader_t& rd, bool const local) {
    auto [hasName, name, escaped] = parse_string(rd);
    if (!hasName)
        return { false, pair_t() };

    parse_ws(rd);
    if (!rd.skip(':'))
        makeError("parseMember: ':' expected", rd);
    parse_ws(rd);

    auto [hasValue, value] = parse_value(rd, local);
    if (!hasValue)
        makeError("parseMember: 'value' expected", rd);

    return { true, pair_t(name, std::move(value), local) };
}

std::pair<bool, std::unique_ptr<object_t>> doc_t::parse_object(reader_t& rd, bool const local) {
    parse_ws(rd);
    if (!rd.skip('{'))
        return { false, nullptr };
    parse_ws(rd);

    auto object = std::make_unique<object_t>();
    for (auto res = parse_member(rd, local); res.first; res = parse_member(rd, local)) {
        object->add(std::move(res.second));
        if (!parse_comma(rd))
            break;
    }

    parse_ws(rd);
    if (!rd.skip('}'))
        makeError("parseObject: '}' expected", rd);
    parse_ws(rd);
    return { true, std::move(object) };
}

void doc_t::serialize(FILE* f) {
    std::string indent;
    serialize(f, indent, *root.get());
}

void doc_t::serialize(FILE* f, std::string indent, std::string_view const value) {
    fwrite(value.data(), value.size(), 1, f);
}

void doc_t::serialize(FILE* f, std::string indent, bool const value) {
    fprintf(f, "%s", (value ? "true" : "false"));
}

void doc_t::serialize(FILE* f, std::string indent, value_t const& value, bool const skipIndent) {
    if (!skipIndent)
        fprintf(f, "%s", indent.c_str());
    if (value.is_object())
        serialize(f, indent, *value.get_if_object());
    else if (value.is_array())
        serialize(f, indent, *value.get_if_array());
    else
        serialize(f, indent, value.get_raw_str());
}

void doc_t::serialize(FILE* f, std::string indent, pair_t const& pair) {
    fprintf(f, "%s", indent.c_str());
    serialize(f, indent, pair.get_raw_name());
    fprintf(f, ": ");
    serialize(f, indent, pair.get_value(), true);
}

void doc_t::serialize(FILE* f, std::string indent, object_t const& object) {
    fprintf(f, "{");
    if (!object.size()) {
        fprintf(f, "}");
        return;
    }
    fprintf(f, "\n");
    for (size_t i = 0; i < object.size(); i++) {
        if (i) fprintf(f, ",\n");
        serialize(f, indent + "\t", object[i]);
    }
    fprintf(f, "\n%s}", indent.c_str());
}

void doc_t::serialize(FILE* f, std::string indent, array_t const& array) {
    fprintf(f, "[");
    if (!array.size()) {
        fprintf(f, "]");
        return;
    }

    bool skipIndent = false;
    if (array.size()) {
        skipIndent = true;
        if (array[0].is_object() || array[0].is_array())
            skipIndent = false;
        else for (auto const& a : array) {
            if (array[0].get_type() != a->get_type()) {
                skipIndent = false;
                break;
            }
        }
    }

    //skipIndent = false; // force new line for array element, or calculate string length
    if (!skipIndent)
        fprintf(f, "\n");

    for (size_t i = 0; i < array.size(); i++) {
        if (i) {
            if (skipIndent)
                fprintf(f, ",");
            else
                fprintf(f, ",\n");
        }
        serialize(f, indent + "\t", array[i], skipIndent);
    }

    if (skipIndent)
        fprintf(f, "]");
    else
        fprintf(f, "\n%s]", indent.c_str());
}
