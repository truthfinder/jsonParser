#pragma once

#include <algorithm>
#include <cassert>
#include <charconv>
#include <memory>
#include <mutex>
#include <numeric>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

#include "strs.h"
#include "fnv.h"

// []{}:,. eE+-\"\t\r\n
// ' '=x20, '\t'=x09, '\r'=x0A, '\n'=x0D
// '['=x5B, ']'=x5D, '{'=x7B, '}'=x7D, ':'=x3A, ','=x2C
// '+'=x2B, '-'=x2D, '0'=x30, 'e'=x65, 'E'=x45
// '"'=x22, '\'=x5C, '/'=x2F, 'b'=x62, 'f'=x66, 'n'=x6E, 'r'=x72, 't'=x74, 'u'=x75

//value=false,true,null,object,array,number,string
//array='[' [ value *(',' value) ] ']'
//member=string:value
//object='{' [ member *(',' member) ] '}'

// hide core classes => doc_t::[value_t, pair_t, object_t, array_t], api: json::doc_t, json::jpath_t
// bjson
// replace type_t::empty with type_t::null
// replace type_t with std::variant::index()
// ?mm
// type requires inited variant
//   - is parse int/real slow?
//   - maybe load int/real => replace type with var::idx

/* todo
    - no type_t so -4 bytes
        - but type will be variant::index => must initialize value on json::load => no lazy values load
            - maybe for lazy: if index==null_t && !raw_sv.empty() => parse_raw_to_variant
    - explicit int
    - same sizeof(var) == 24
 
    struct null_t {};
    using data_t = std::variant<null_t, bool, int, int64_t, double, std::string_view>;
    size_t q = sizeof(data_t);
    data_t d;
    size_t idx = d.index();
    d = true;
    idx = d.index();
    d = 10;
    idx = d.index();
    d = 10ll;
    idx = d.index();
    d = 10;
    idx = d.index();
*/

namespace json {
    std::vector<std::string_view> split(std::string_view const src, char const sep);
    std::vector<std::string_view> split(std::string_view const src, std::string_view const sep);

    inline constexpr bool is_escaped(char const ch) noexcept { return std::string_view("\"\\/bfnrtu").find(ch) != std::string_view::npos; } // \"\\\/\b\f\n\r\t\uFFFF
    inline constexpr bool is_unescaped(char const ch) noexcept { return (ch >= '\x20' && ch <= '\x21') || (ch >= '\x23' && ch <= '\x5B') || (ch >= '\x23' && ch <= '\xFF'); } // xFF->x10FFFF
    inline constexpr bool is_char(char const ch) noexcept { return is_unescaped(ch) || is_escaped(ch); }
    inline constexpr bool is_digit(char const ch) noexcept { return ch >= '0' && ch <= '9'; }
    inline constexpr bool is_digit1_9(char const ch) noexcept { return ch >= '1' && ch <= '9'; }
    inline constexpr bool is_hex(char const ch) noexcept { return is_digit(ch) || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F'); }
    inline constexpr bool is_dot(char const ch) noexcept { return ch == '.'; }
    inline constexpr bool is_ws(char const ch) noexcept { return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n'; }

    std::pair<std::string, bool> unescape(std::string_view const sv);
    std::string unescape(std::string_view s, bool escaped);
    std::string escape(std::string_view s);

    class reader_t {
    public:
        reader_t(std::string_view src) : data(src) { reset(); }

        std::string_view substr(size_t const ofs, size_t const count) const { return data.substr(ofs, count); }
        size_t size() const noexcept { return data.size(); }
        size_t position() const noexcept { return pos; }
        size_t rest() const noexcept { return data.size() - pos; }
        void set_position(size_t const position) { pos = position; }
        bool done(size_t const ofs = 0) const { return pos + ofs >= data.size(); }
        bool has(std::string_view ref) const { return ref.size() <= rest() && ref == substr(position(), ref.size()); }
        bool has(char const ch) const { return rest() && ch == get(); }
        template <typename P> bool has(P p) const { return rest() && p(get()); }
        char get() const { return data[position()]; }
        char const* get_base_ptr(size_t const ofs = 0) const noexcept { return data.data() + ofs; }
        char const* get_pos_ptr(size_t const ofs = 0) const noexcept { return data.data() + pos + ofs; }
        char read() { check(); get() == '\n' ? (line++, column = 1) : column++; return data[pos++]; }
        bool skip(char const ch) { return !has(ch) ? false : (read(), true); }
        template <typename P> bool skip(P p) { return !has(p) ? false : (read(), true); }
        bool skip(std::string_view ref) { return !has(ref) ? false : (skip(ref.size()), true); }
        void reset() { pos = 0; line = 1; column = 1; }
        bool skip(size_t const count) { return done() || count >= rest() ? false : (pos += count, true); }
        void skip_while(char const ch) { for (; !done() && ch == get(); read()); } // mm
        void skip_until(char const ch) { for (; !done() && ch != get(); read()); } // mm
        template <typename P> void skip_while(P p) { for (; !done() && p(get()); read()); }
        template <typename P> void skip_until(P p) { for (; !done() && !p(get()); read()); }
        void skip_while(std::string_view s) { for (; !done() && s.find_first_of(get()) != s.npos; read()); }

        void skip_line(size_t const count) {
            for (int i = static_cast<int>(count); i--; ) {
                for (; !done() && get() != '\n'; read());
                if (!done()) read();
            }
        }

        std::string_view read_while(char const ch) {
            size_t const start = position();
            for (; !done() && get() == ch; read());
            return data.substr(start, position() - start);
        }

        std::string_view read_until(char const ch) {
            size_t const start = position();
            for (; !done() && get() != ch; read());
            return data.substr(start, position() - start);
        }

        template <typename P> std::string_view read_while(P p) {
            size_t const start = position();
            for (; !done() && p(get()); read());
            return data.substr(start, position() - start);
        }

        template <typename P> std::string_view read_until(P p) {
            size_t const start = position();
            for (; !done() && !p(get()); read());
            return data.substr(start, position() - start);
        }

        std::string_view read_line() {
            size_t start = position();
            while (!done() && get() != '\r' && get() != '\n') read();
            auto r = data.substr(start, position() - start);
            if (!done() && get() == '\r') read();
            if (!done() && get() == '\n') read();
            return r;
        }

    private:
        void check_base(size_t const ofs = 0) const {
            assert(ofs <= data.size());
            if (ofs >= data.size())
                throw std::exception("check failed");
        }

        void check(size_t const ofs = 0) const {
            assert(pos + ofs <= data.size());
            if (done(ofs))
                throw std::exception("check failed");
        }

    private:
        std::string_view data;
        size_t pos;
        size_t line;
        size_t column;
        // ??? char read() -> char const* read() { return check() ? data.data() : nullptr; }, iterators
        // ptr will help to check string since the current position symbol
    };

    class array_t;
    class object_t;

    using record_t = std::pair<std::string_view, std::variant<bool, int64_t, double, std::string_view>>;

    class value_t {
        using data_t = std::variant<bool, int64_t, double, std::string_view, std::unique_ptr<array_t>, std::unique_ptr<object_t>>;

    public:
        enum class type_t { empty, null, boolean, number, string, object, array };
        // remove
        data_t const& data() const { return value; }
        data_t& data() { return value; }

    public:
        value_t() = delete;
        value_t(value_t&& v) = default;
        value_t(value_t const& v) : store(v.get_raw_str()) {
            type = v.type;
            escaped = v.escaped;

            switch (type) {
            case type_t::array:
                value = std::make_unique<array_t>(*std::get<std::unique_ptr<array_t>>(v.value));
                break;
            case type_t::object:
                value = std::make_unique<object_t>(*std::get<std::unique_ptr<object_t>>(v.value));
                break;
            }
        }

        explicit value_t(type_t const _type, data_t&& _data) : type{ _type }, value{ std::move(_data) } {}
        explicit value_t(type_t const _type, std::string_view const _source, bool const _escaped = false, bool const local = false)
            : type{ _type }, escaped{ _escaped } { if (local) store = _source; else source = _source; }
        explicit value_t(std::unique_ptr<array_t> array) : type{ type_t::array }, value{ std::move(array) } {}
        explicit value_t(std::unique_ptr<object_t> object) : type{ type_t::object }, value{ std::move(object) } {}

        value_t& operator = (value_t&&) = default;

        size_t memory() const; // ?capacity too in pair
        void reindex() const;

        //enum class type_t { null, boolean, int64_t, double, string, object, array }; // no empty
        //variant<bool, int64_t, double, std::string_view, std::unique_ptr<array_t>, std::unique_ptr<object_t>>
        //type_t get_type() const { return static_cast<type_t>(value.index()); }
        //bool is_null() const { return value.index() == type_t::empty; }
        //bool is_bool() const { return value.index() == type_t::boolean; }
        //bool is_int() const { return value.index() == type_t::int; }
        //bool is_double() const { return value.index() == type_t::double; }
        //bool is_number() const { return is_int() || is_double(); }
        //bool is_string() const { return value.index() == type_t::string; }

        bool is_empty() const { return type == type_t::empty; } // replace with null
        //bool is_null() const { return type == type_t::null; }
        bool is_bool() const { return type == type_t::boolean; }
        bool is_number() const { return type == type_t::number; }
        bool is_string() const { return type == type_t::string; }
        bool is_object() const { return type == type_t::object; }
        bool is_array() const { return type == type_t::array; }

        type_t get_type() const { return type; }
        std::string_view get_raw_str() const { return store.size() ? store : source; }
        object_t const& get_object() const { return *std::get<std::unique_ptr<object_t>>(value); }
        array_t const& get_array() const { return *std::get<std::unique_ptr<array_t>>(value); }
        object_t& get_object() { return *std::get<std::unique_ptr<object_t>>(value); }
        array_t& get_array() { return *std::get<std::unique_ptr<array_t>>(value); }
        object_t const* get_if_object() const { auto* ptr = std::get_if<std::unique_ptr<object_t>>(&value); return ptr ? ptr->get() : nullptr; }
        array_t const* get_if_array() const { auto* ptr = std::get_if<std::unique_ptr<array_t>>(&value); return ptr ? ptr->get() : nullptr; }
        object_t* get_if_object() { auto* ptr = std::get_if<std::unique_ptr<object_t>>(&value); return ptr ? ptr->get() : nullptr; }
        array_t* get_if_array() { auto* ptr = std::get_if<std::unique_ptr<array_t>>(&value); return ptr ? ptr->get() : nullptr; }

        template <typename T> T const* get_if() const { return std::get_if<T>(&value); }
        template <> int const* get_if<int>() const { auto* v = std::get_if<int64_t>(&value); return v ? reinterpret_cast<int const*>(v) :nullptr ; }

        value_t const* operator () (size_t const i) const { return _find(i); }
        value_t* operator () (size_t const i) { return const_cast<value_t*>(_find(i)); }
        value_t const* operator () (int const i) const { return _find(i); }
        value_t* operator () (int const i) { return const_cast<value_t*>(_find(i)); }
        value_t const* operator () (std::string_view const name) const { return _find(name); }
        value_t* operator () (std::string_view const name) { return const_cast<value_t*>(_find(name)); }
        value_t const* operator () (std::vector<record_t> const& recs) const { return _find(recs); }
        value_t* operator () (std::vector<record_t> const& recs) { return const_cast<value_t*>(_find(recs)); }

        template <typename T> T get(T const def = T()) const { assert(0 && "value_t::get unknown type"); return T(); }
        template <> std::string_view get<std::string_view>(std::string_view const def) const { return get_value<std::string_view>(def); }
        template <> int get<int>(int const def) const { return static_cast<int>(get_value<int64_t>(def)); }
        template <> int64_t get<int64_t>(int64_t const def) const { return get_value<int64_t>(def); }
        template <> double get<double>(double const def) const { return get_value<double>(def); }
        template <> bool get<bool>(bool const def) const { return get_value<bool>(def); }

        template <typename T> T str_as(T const def = T()) const;
        template <> bool str_as<bool>(bool const def) const { return get_as<bool>(def); }
        template <> int str_as<int>(int const def) const { return static_cast<int>(get_as<int64_t>(def)); }
        template <> int64_t str_as<int64_t>(int64_t const def) const { return get_as<int64_t>(def); }
        template <> double str_as<double>(double const def) const { return get_as<double>(def); }
        template <> std::string_view str_as<std::string_view>(std::string_view const def) const {
            auto const src = get_raw_str();
            assert(src.size() > 1);
            return src.size() < 2 ? def : src.substr(1, src.size() - 2);
        }

        //set<int>(10) => 10, set_str_as<int>(10) => "10"
        bool set(std::string const& v);
        bool set_add(std::string const& v);
        template <typename T> bool set(T const v);
        template <typename T> bool set_add(T const v);
        bool set_str_as(std::string const& v);
        bool set_add_str_as(std::string const& v);
        template <typename T> bool set_str_as(T const v);
        template <typename T> bool set_add_str_as(T const v);

    private:
        template <typename T> T get_value(T const def = T()) const {
            if (auto const* cached = std::get_if<T>(&value))
                return *cached;

            if constexpr (std::is_same_v<T, std::string_view>)
                value = str_as<std::string_view>();
            else if constexpr (std::is_same_v<T, bool>)
                value = get_number<bool>(get_raw_str());
            //else if constexpr (std::is_same_v<T, int>)
            //    value = get_number<int>(get_raw_str());
            else if constexpr (std::is_same_v<T, int64_t>)
                value = get_number<int64_t>(get_raw_str());
            else if constexpr (std::is_same_v<T, double>)
                value = get_number<double>(get_raw_str());
            else
                static_assert(0 && "type not supported");

            return std::get<T>(value);
        }

        template <typename T> T get_as(T const def = T()) const {
            return get_number<T>(get_value<std::string_view>());
        }

        template <typename T, std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool>, bool> = true>
        T get_number(std::string_view const src, T const def = T()) const {
            if (src.empty())
                return def;
            T val;
            auto const [ptr, ec] = std::from_chars(src.data(), src.data()+src.size(), val, 10);
            if (ec != std::errc()) // checked on parse stage
                return def;
            value = val;
            return std::get<T>(value);
        }

        template <typename T, std::enable_if_t<std::is_floating_point_v<T>, bool> = true>
        T get_number(std::string_view const src, T const def = T()) const {
            if (src.empty())
                return def;
            T val;
            auto const [ptr, ec] = std::from_chars(src.data(), src.data() + src.size(), val);
            if (ec != std::errc()) // checked on parse stage
                return def;
            value = val;
            return std::get<T>(value);
        }

        template <typename T, std::enable_if_t<std::is_same_v<T, bool>, bool> = true>
        T get_number(std::string_view const src, T const def = T()) const {
            return src == "true" ? true : src == "false" ? false : def;
        }

        value_t const* _find(size_t const i) const;
        value_t const* _find(int const i) const;
        value_t const* _find(std::string_view const name) const;
        value_t const* _find(std::vector<record_t> const& recs) const;

        static std::string escape(std::string_view const src);
        static std::string unescape(std::string_view const src);
        static std::string unescape(std::string_view const src, bool const escaped);

    private:
        mutable data_t value;
        std::string_view source;
        std::string store;
        type_t type{ type_t::empty };
        bool escaped{ false };
    };

    class pair_t {
    public:
        pair_t() = default;
        pair_t(std::string_view n, std::unique_ptr<value_t>&& v, bool const local = false) : value{ std::move(v) } {
            if (local) raw_name_store = n; else raw_name = n;
        }
        pair_t(pair_t&&) = default;
        pair_t(pair_t const& p) {
            raw_name_store = p.get_raw_name();
            value = std::make_unique<value_t>(*p.value);
        }

        size_t memory() const {
            size_t mem = sizeof(pair_t);
            if (raw_name_store.capacity())
                mem += raw_name_store.capacity();
            if (value)
                mem += value->memory();
            return mem;
        }

        pair_t& operator = (pair_t&&) = default;

        std::string_view get_raw_name() const { return raw_name.size() ? raw_name : raw_name_store; }
        std::string_view get_name() const { auto r = get_raw_name(); return r.size() >= 2 ? r.substr(1, r.size() - 2) : std::string_view(); }
        value_t const& get_value() const { return *value; }
        value_t& get_value() { return *value; }

        value_t* operator () (std::string_view const name, std::vector<record_t> const& recs, bool const _explicit = true);

    private:
        std::string_view raw_name;
        std::string raw_name_store;
        std::unique_ptr<value_t> value;
    };

    class indices_t {
    public:
        indices_t() = default;

        size_t memory() const { return 0; }
        void add(std::string_view const id, uint16_t const index) {
            remap[fnva16hash<std::string_view>()(id)] = index;
        }

        std::pair<uint16_t, bool> get(uint16_t const index) const {
            auto it = remap.find(index);
            return it != remap.end() ? std::make_pair(it->second, true) : std::make_pair(0ui16, false);
        }

    private:
        std::unordered_map<uint16_t, uint16_t> remap;
        //todo: vector:table + vector:remap + reindex:sort - less memory
    };

    class object_t {
    public:
        using const_iterator = std::vector<pair_t>::const_iterator;
        using iterator = std::vector<pair_t>::iterator;

    public:
        object_t() = default;
        object_t(object_t&&) = default;
        object_t(object_t const& a) {
            pairs.reserve(a.pairs.size());
            for (auto const& p : a.pairs)
                pairs.push_back(pair_t(p));
        }

        size_t memory() const {
            //auto h = std::hash<std::string>()("123");
            size_t mem = sizeof(object_t);
            for (auto const& p : pairs)
                mem += p.memory();
            if (indices)
                mem += indices->memory();
            return mem;
        }

        void reindex() const {
            indices.reset();
            indices = std::make_unique<indices_t>();
            assert(indices);
            if (pairs.size() < 100)
                return;
            for (size_t i=0; i<pairs.size(); i++)
                indices->add(pairs[i].get_name(), static_cast<uint16_t>(i));
        }

        void add(pair_t&& p) { pairs.push_back(std::move(p)); }

        size_t size() const { return pairs.size(); }
        const_iterator begin() const { return pairs.begin(); }
        const_iterator end() const { return pairs.end(); }
        iterator begin() { return pairs.begin(); }
        iterator end() { return pairs.end(); }

        pair_t& operator [] (size_t const i) { return pairs[i]; }
        pair_t const& operator [] (size_t const i) const { return pairs[i]; }

        pair_t* operator () (size_t const i) { return const_cast<pair_t*>(_find(i)); }
        pair_t* operator () (int i) { return const_cast<pair_t*>(_find(i)); }
        pair_t* operator () (std::string_view const name) { return const_cast<pair_t*>(_find(name)); }

        pair_t const* operator () (size_t const i) const { return _find(i); }
        pair_t const* operator () (int i) const { return _find(i); }
        pair_t const* operator () (std::string_view const name) const { return _find(name); }

        bool has(std::vector<record_t> const& recs, bool const _explicit = true) const {
            for (auto const& rec : recs) {
                auto it = std::find_if(pairs.begin(), pairs.end(), [&rec, expl=_explicit](pair_t const& p) {
                    if (p.get_name() != rec.first)
                        return false;
                    if (auto const* b = std::get_if<bool>(&rec.second)) {
                        if (!p.get_value().is_bool()) return false;
                        auto const m = p.get_value().get<bool>();
                        return m == *b;
                    } else if (auto* i = std::get_if<int64_t>(&rec.second)) {
                        if (!p.get_value().is_number()) return false;
                        auto const m = p.get_value().get<int64_t>();
                        return m == *i;
                    } else if (auto d = std::get_if<double>(&rec.second)) {
                        if (!p.get_value().is_number()) return false;
                        auto const m = p.get_value().get<double>();
                        return m == *d;
                    } else if (auto* s = std::get_if<std::string_view>(&rec.second)) {
                        if (!p.get_value().is_string()) return false;
                        auto const m = p.get_value().get<std::string_view>();
                        return m == *s;
                    }
                    return true; // only pair name case
                });
                if (it == pairs.end())
                    return false;
            }
            return true;
        }
    private:
        pair_t const* _find(size_t const i) const { return i <= pairs.size() ? &pairs[i] : nullptr; }

        pair_t const* _find(int i) const {
            if (i < 0) i = static_cast<int>(pairs.size()) + i;
            return i >= 0 && static_cast<size_t>(i) <= pairs.size() ? &pairs[i] : nullptr;
        }

        pair_t const* _find(std::string_view const name) const {
            auto it = std::find_if(pairs.begin(), pairs.end(), [name](pair_t const& p) { return p.get_name() == name; });
            return it != pairs.end() ? &*it : nullptr;
        }

    private:
        std::vector<pair_t> pairs;
        mutable std::unique_ptr<indices_t> indices;
    };

    class array_t {
    public:
        using const_iterator = std::vector<std::unique_ptr<value_t>>::const_iterator;
        using iterator = std::vector<std::unique_ptr<value_t>>::iterator;

    public:
        array_t() = default;
        array_t(array_t&&) = default;
        array_t(array_t const& a) {
            values.reserve(a.values.size());
            for (auto const& v : a.values)
                values.push_back(std::make_unique<value_t>(*v));
        }

        size_t memory() const {
            size_t mem = sizeof(array_t);
            //mem = std::accumulate(values.begin(), values.end(), mem, [](size_t mem, std::unique_ptr<value_t> const& v) {
            //    return v ? mem + v->memory() : mem;
            //});
            for (auto const& v : values) // thumb up for std::accumulate short&clean implementation
                if (v) mem += v->memory();
            if (indices)
                mem += indices->memory();
            return mem;
        }

        void reindex() const {
            indices.reset();
            indices = std::make_unique<indices_t>();
            assert(indices);
            //for (auto const& v : values) { v->is_string(); }
            //for (size_t i=0; i<pairs.size(); i++)
            //    indices->add(pairs[i].get_name(), static_cast<uint16_t>(i));
        }

        void add(std::unique_ptr<value_t>&& v) { values.push_back(std::move(v)); }

        size_t size() const { return values.size(); }
        const_iterator begin() const { return values.begin(); }
        const_iterator end() const { return values.end(); }
        iterator begin() { return values.begin(); }
        iterator end() { return values.end(); }

        value_t const& operator [] (size_t const i) const { return *values[i]; }
        value_t& operator [] (size_t const i) { return *values[i]; }

        value_t* operator () (size_t const i) { return const_cast<value_t*>(_find(i)); }
        value_t* operator () (int i) { return const_cast<value_t*>(_find(i)); }
        value_t* operator () (std::vector<record_t> const& recs, bool const _explicit = true) { return const_cast<value_t*>(_find(recs, _explicit)); }

        value_t const* operator () (size_t const i) const { return _find(i); }
        value_t const* operator () (int i) const { return _find(i); }
        value_t const* operator () (std::vector<record_t> const& recs, bool const _explicit = true) const { return _find(recs, _explicit); }

    private:
        value_t const* _find(size_t const i) const { return i < values.size() ? values[i].get() : nullptr; }

        value_t const* _find(int i) const {
            if (i < 0) i = static_cast<int>(values.size()) + i;
            return i >= 0 && static_cast<size_t>(i) < values.size() ? values[i].get() : nullptr;
        }

        value_t const* _find(std::vector<record_t> const& recs, bool const _explicit = true) const {
            auto vit = std::find_if(values.begin(), values.end(), [recs, _explicit](std::unique_ptr<value_t> const& v) {
                if (auto const* obj = v->get_if_object())
                    return obj->has(recs);
                return false;
            });
            return vit != values.end() ? vit->get() : nullptr;
        }

    private:
        std::vector<std::unique_ptr<value_t>> values;
        mutable std::unique_ptr<indices_t> indices;
    };

    inline size_t value_t::memory() const {
        size_t mem = sizeof(value_t);
        if (store.capacity())
            mem += store.capacity();
        if (auto* obj = get_if_object())
            mem += obj->memory();
        else if (auto* arr = get_if_array())
            mem += arr->memory();
        return mem;
    }

    inline void value_t::reindex() const {
        if (auto* obj = get_if_object())
            obj->reindex();
        else if (auto* arr = get_if_array())
            arr->reindex();
    }

    inline value_t const* value_t::_find(size_t const i) const {
        if (auto* a = get_if_array())
            return (*a)(i);
        return nullptr;
    }

    inline value_t const* value_t::_find(int const i) const {
        if (auto* a = get_if_array())
            return (*a)(i);
        return nullptr;
    }

    inline value_t const* value_t::_find(std::string_view const name) const {
        if (auto* o = get_if_object()) {
            auto* p = (*o)(name);
            return p ? &p->get_value() : nullptr;
        } else if (auto* a = get_if_array()) {
            for (auto const& v : *a) { // _find
                if (v->get_if_object())
                    return (*v)(name);
            }
        }
        return nullptr;
    }

    inline value_t const* value_t::_find(std::vector<record_t> const& recs) const {
        if (auto* o = get_if_object()) {
            //???
        } else if (auto* a = get_if_array()) {
            return (*a)(recs);
        }
        return nullptr;
    }

    inline value_t* pair_t::operator () (std::string_view const name, std::vector<record_t> const& recs, bool const _explicit) {
        if (get_name() == name)
            return nullptr;
        if (auto* arr = get_value().get_if_array())
            return (*arr)(recs, _explicit);
        return nullptr;
    }

    class jpath_t {
        struct stub { int stub; };
    
    public:
        value_t const& v() const { return *value; }
        jpath_t find(std::string_view const path) { // auto tags = split(path, "/@"); if (tag.starts_with('@'))
            if (!value)
                return jpath_t(value);

            //var = nul,bol,num,str
            //val = var,obj,arr
            //arr = [*val]
            //mbr = str:val
            //obj = {*mbr}

            // mbr=str:arr[int]

            // /str/ obj[mbr:str=str], arr[obj:mbr[0]:str=str], any mbr:str=str, obj[] | arr[].obj[]

            value_t const* v = value;
            for (auto const sc: su::split(path, '/')) {
                if (!v) break;
                auto const dog = su::split(sc, '@');
                if (dog.empty() || dog.size() > 2)
                    break;
                if (dog.size() == 1) {
                    v = (*v)(dog.front());
                } else if (auto const eq = su::split(dog.back(), '='); eq.size() == 2) {
                    std::vector<record_t> recs;
                    recs.emplace_back(eq.front(), eq.back()); // support diff types
                    v = (*v)(recs);
                } else {
                    int index;
                    auto const [ptr, ec] = std::from_chars(dog.back().data(), dog.back().data()+dog.back().size(), index, 10);
                    if (ec == std::errc())
                        v = (*v)(index);
                    else
                        v = (*v)(dog.back());
                }
            }

            return jpath_t(v);
        }

        operator int stub::* () const { // explicit bool
            return value && !value->is_empty() ? &stub::stub : 0;
        }

        object_t const* get_object() const { return _get_object(); }
        object_t* get_object() { return const_cast<object_t*>(_get_object()); }
        array_t const* get_array() const { return _get_array(); }
        array_t* get_array() { return const_cast<array_t*>(_get_array()); }

        template <typename T> T get() const {
            if (auto* obj = value->get_if_object()) // maybe like: value is object with name == class name
                return T(*this); // T() due to T::cstr is private
            return T();
        }

        template <typename T, typename R> std::vector<std::unique_ptr<R>> create() const {
            std::vector<std::unique_ptr<R>> res;
            if (auto* root_obj = value->get_if_object()) {
                for (auto const& p : *root_obj) {
                    if (p.get_value().is_object())
                        res.push_back(T::create(p.get_name(), jpath_t(&p.get_value())));
                }
            }
            return res;
        }

        template <typename T> T get(std::string_view const name) const {
            if (value) {
                if (auto const* obj = value->get_if_object()) {
                    if (auto const* p = (*obj)(name)) {
                        return p->get_value().get<T>();
                    }
                }
            }
            return T();
        }

        //remap from value, make jpath_t public API only
        //operator []
        //operator ()
        //is_*
        //get<*>
        //str_as<*>

    private:
        friend class doc_t;
        jpath_t(value_t const* v, std::mutex* m = nullptr) : value(v), locker(m) {}

        object_t const* _get_object() const {
            auto* ptr = std::get_if<std::unique_ptr<object_t>>(&v().data());
            return ptr ? ptr->get() : nullptr;
        }

        array_t const* _get_array() const {
            auto* ptr = std::get_if<std::unique_ptr<array_t>>(&v().data());
            return ptr ? ptr->get() : nullptr;
        }

    private:
        value_t const* value;
        std::mutex* locker;
    };

    class doc_t {
        doc_t(std::unique_ptr<value_t>&& v) : root(std::move(v)) {}

    public:
        //enum storage_mode_t { local, external };

        doc_t() = default;
        doc_t(doc_t&&) = default;
        doc_t(doc_t const&) = delete;
        doc_t(std::string&& src) : text{ std::move(src) } { if (auto [ok, v] = parse(text, false); ok) root = std::move(v); }
        explicit doc_t(std::string_view const src, bool const local = true) { if (auto [ok, v] = parse(src, local); ok) root = std::move(v); }
        explicit doc_t(std::string const& src, bool const local = true) { if (auto [ok, v] = parse(src, local); ok) root = std::move(v); }

        void serialize(FILE* f);

        jpath_t find(std::string_view const path) const { return jpath_t(root.get()).find(path); }
        doc_t clone() const { return doc_t(std::make_unique<value_t>(*root)); }

        size_t memory() const { return root ? root->memory() : 0; }
        void reindex() const { if (root) root->reindex(); }
        bool make_index(std::string_view const path); // islands/territories/buildings/_id ; "buildings:[{_id:value1},{id:value2}]"

        // ?move to jpath_t
        template <typename T> std::vector<T> get_array(std::string_view const path, bool const _explicit = true) {
            std::vector<T> res;
            if (auto arr = find(path).get_array()) {
                for (auto const& v : *arr) {
                    if (auto* obj = v->get_if_object())
                        res.push_back(T(*obj)); // T() due to T::cstr is private
                }
            }
            return res;
        }

        template <> std::vector<int> get_array<int>(std::string_view const path, bool const _explicit) {
            std::vector<int> res;
            if (auto arr = find(path).get_array()) {
                if (_explicit) {
                    for (auto const& v : *arr) {
                        if (v->is_number())
                            res.emplace_back(v->get<int>());
                    }
                } else {
                    for (auto const& v : *arr) {
                        if (v->is_number())
                            res.emplace_back(v->get<int>());
                        else if (v->is_string())
                            res.emplace_back(v->str_as<int>());
                    }
                }
            }
            return res;
        }

        template <> std::vector<int64_t> get_array<int64_t>(std::string_view const path, bool const _explicit) {
            std::vector<int64_t> res;
            if (auto arr = find(path).get_array()) {
                if (_explicit) {
                    for (auto const& v : *arr) {
                        if (v->is_number())
                            res.emplace_back(v->get<int64_t>());
                    }
                } else {
                    for (auto const& v : *arr) {
                        if (v->is_number())
                            res.emplace_back(v->get<int64_t>());
                        else if (v->is_string())
                            res.emplace_back(v->str_as<int64_t>());
                    }
                }
            }
            return res;
        }

        template <> std::vector<double> get_array<double>(std::string_view const path, bool const _explicit) {
            std::vector<double> res;
            if (auto arr = find(path).get_array()) {
                if (_explicit) {
                    for (auto const& v : *arr) {
                        if (v->is_number())
                            res.emplace_back(v->get<double>());
                    }
                } else {
                    for (auto const& v : *arr) {
                        if (v->is_number())
                            res.emplace_back(v->get<double>());
                        else if (v->is_string())
                            res.emplace_back(v->str_as<double>());
                    }
                }
            }
            return res;
        }

        template <> std::vector<std::string_view> get_array<std::string_view>(std::string_view const path, bool const _explicit) {
            std::vector<std::string_view> res;
            if (auto arr = find(path).get_array()) {
                if (_explicit) {
                    for (auto const& v : *arr) {
                        if (v->is_string())
                            res.emplace_back(v->get<std::string_view>());
                    }
                } else {
                    for (auto const& v : *arr) {
                        if (v->is_string())
                            res.emplace_back(v->get<std::string_view>());
                        else if (v->is_number())
                            res.emplace_back(v->str_as<std::string_view>());
                    }
                }
            }
            return res;
        }

    private:
        void makeError(std::string_view error, reader_t const& reader) const;

        std::pair<bool, std::unique_ptr<value_t>> parse(std::string_view const data, bool const local);
        bool parse_ws(reader_t& rd);
        bool parse_comma(reader_t& rd);
        std::pair<bool, std::string_view> parse_null(reader_t& rd);
        std::pair<bool, std::string_view> parse_false(reader_t& rd);
        std::pair<bool, std::string_view> parse_true(reader_t& rd);
        std::pair<bool, std::string_view> parse_bool(reader_t& rd);
        std::pair<bool, std::string_view> parse_number(reader_t& rd);
        std::tuple<bool, std::string_view, bool> parse_string(reader_t& rd);
        std::pair<bool, std::unique_ptr<value_t>> parse_value(reader_t& rd, bool const local);
        std::pair<bool, std::unique_ptr<array_t>> parse_array(reader_t& rd, bool const local);
        std::pair<bool, pair_t> parse_member(reader_t& rd, bool const local);
        std::pair<bool, std::unique_ptr<object_t>> parse_object(reader_t& rd, bool const local);

        void serialize(FILE* f, std::string indent, std::string_view const value);
        void serialize(FILE* f, std::string indent, bool const value);
        void serialize(FILE* f, std::string indent, value_t const& value, bool const skipIndent = false);
        void serialize(FILE* f, std::string indent, array_t const& array);
        void serialize(FILE* f, std::string indent, pair_t const& pair);
        void serialize(FILE* f, std::string indent, object_t const& object);

    private:
        std::unique_ptr<value_t> root;
        std::string text; // if all sv's are empty -> text.clear
        std::mutex locker;
        //std::vector<std::string> storage; // remove store from value
    };

}
