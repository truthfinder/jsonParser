#pragma once

#include <cstdint>
#include <string>

// uint32_t FNV_prime = 16777619u
// uint32_t offset_basis = 2166136261u // FNV1_32_INIT
// uint64_t FNV_prime = 1099511628211ull
// uint64_t offset_basis = 14695981039346656037ull // FNV1_64_INIT

/*
    FNV1:
    hash = offset_basis
    for each octet_of_data to be hashed
        hash = hash * FNV_prime
        hash = hash xor octet_of_data
    return hash

    FNV1a:
    hash = offset_basis
    for each octet_of_data to be hashed
        hash = hash xor octet_of_data
        hash = hash * FNV_prime
    return hash

    fnv16 = (fnv32>>16) ^ (fnv32&0xffff);
    fnv24 = (fnv32>>24) ^ (fnv32&0xffffff);
    fnv56 = (fnv64>>56) ^ (fnv64&MASK56);
*/

constexpr uint32_t FNV_32_prime = 16777619u; // 0x01000193, (hval<<1)+(hval<<4)+(hval<<7)+(hval<<8)+(hval<<24)
constexpr uint32_t FNV_32_offset_basis = 2166136261u;
constexpr uint64_t FNV_64_prime = 1099511628211ull; // (hval<<1)+(hval<<4)+(hval<<5)+(hval<<7)+(hval<<8)+(hval<<40)
constexpr uint64_t FNV_64_offset_basis = 14695981039346656037ull;

inline constexpr uint32_t fnv1_32(void const* buf, uint32_t hval = FNV_32_offset_basis) {
    for (uint8_t const* b = static_cast<uint8_t const*>(buf); *b;) {
        hval *= FNV_32_prime;
        hval ^= static_cast<uint32_t>(*b++);
    }
    return hval;
}

inline constexpr uint32_t fnv1_32_2(void const* buf, size_t const len, uint32_t hval = FNV_32_offset_basis) {
    for (uint8_t const *b = static_cast<uint8_t const*>(buf), *e = b + len; b < e;) {
        hval *= FNV_32_prime;
        hval ^= static_cast<uint32_t>(*b++);
    }
    return hval;
}

inline constexpr uint32_t fnv1a_32(void const* buf, uint32_t hval = FNV_32_offset_basis) {
    for (uint8_t const* b = static_cast<uint8_t const*>(buf); *b;) {
        hval ^= static_cast<uint32_t>(*b++);
        hval *= FNV_32_prime;
    }
    return hval;
}

inline constexpr uint32_t fnv1a_32_2(void const* buf, size_t const len, uint32_t hval = FNV_32_offset_basis) {
    for (uint8_t const *b = static_cast<uint8_t const*>(buf), *e = b + len; b < e;) {
        hval ^= static_cast<uint32_t>(*b++);
        hval *= FNV_32_prime;
    }
    return hval;
}

inline constexpr uint16_t fnv1_16(void const* buf, uint32_t hval = FNV_32_offset_basis) {
    uint32_t const _32 = fnv1_32(buf, hval);
    return static_cast<uint16_t>((_32 >> 16) ^ (_32 & 0xffff));
}

inline constexpr uint16_t fnv1_16_2(void const* buf, size_t const len, uint32_t hval = FNV_32_offset_basis) {
    uint32_t const _32 = fnv1_32_2(buf, len, hval);
    return static_cast<uint16_t>((_32 >> 16) ^ (_32 & 0xffff));
}

inline constexpr uint32_t fnv1a_16(void const* buf, uint32_t hval = FNV_32_offset_basis) {
    uint32_t const _32 = fnv1a_32(buf, hval);
    return static_cast<uint16_t>((_32 >> 16) ^ (_32 & 0xffff));
}

inline constexpr uint32_t fnv1a_16_2(void const* buf, size_t const len, uint32_t hval = FNV_32_offset_basis) {
    uint32_t const _32 = fnv1a_32_2(buf, len, hval);
    return static_cast<uint16_t>((_32 >> 16) ^ (_32 & 0xffff));
}

inline constexpr uint16_t fnv1_24(void const* buf, uint32_t hval = FNV_32_offset_basis) {
    uint32_t const _32 = fnv1_32(buf, hval);
    return (_32 >> 24) ^ (_32 & 0xffffff);
}

inline constexpr uint16_t fnv1_24_2(void const* buf, size_t const len, uint32_t hval = FNV_32_offset_basis) {
    uint32_t const _32 = fnv1_32_2(buf, len, hval);
    return (_32 >> 24) ^ (_32 & 0xffffff);
}

inline constexpr uint32_t fnv1a_24(void const* buf, uint32_t hval = FNV_32_offset_basis) {
    uint32_t const _32 = fnv1a_32(buf, hval);
    return (_32 >> 24) ^ (_32 & 0xffffff);
}

inline constexpr uint32_t fnv1a_24_2(void const* buf, size_t const len, uint32_t hval = FNV_32_offset_basis) {
    uint32_t const _32 = fnv1a_32_2(buf, len, hval);
    return (_32 >> 24) ^ (_32 & 0xffffff);
}

inline constexpr uint64_t fnv1_64(void const* buf, uint64_t hval = FNV_64_offset_basis) {
    for (uint8_t const* b = static_cast<uint8_t const*>(buf); *b; ) {
        hval *= FNV_64_prime;
        hval ^= static_cast<uint64_t>(*b++);
    }
    return hval;
}

inline constexpr uint64_t fnv1_64_2(void const* buf, size_t const len, uint64_t hval = FNV_64_offset_basis) {
    for (uint8_t const *b = static_cast<uint8_t const*>(buf), *e = b + len;  b < e; ) {
        hval *= FNV_64_prime;
        hval ^= static_cast<uint64_t>(*b++);
    }
    return hval;
}

inline constexpr uint64_t fnv1a_64(void const* buf, uint64_t hval = FNV_64_offset_basis) {
    constexpr uint64_t FNV_64_prime = 1099511628211ull;
    for (uint8_t const* b = static_cast<uint8_t const*>(buf); *b; ) {
        hval ^= static_cast<uint64_t>(*b++);
        hval *= FNV_64_prime;
    }
    return hval;
}

inline constexpr uint64_t fnv1a_64_2(void const* buf, size_t const len, uint64_t hval = FNV_64_offset_basis) {
    constexpr uint64_t FNV_64_prime = 1099511628211ull;
    for (uint8_t const *b = static_cast<uint8_t const*>(buf), *e = b + len; b < e; ) {
        hval ^= static_cast<uint64_t>(*b++);
        hval *= FNV_64_prime;
    }
    return hval;
}

//constexpr inline auto operator"" _fnv1_16(const char* s, size_t sz) { return fnv1_16(s, sz); }
//constexpr inline auto operator"" _fnv1_24(const char* s, size_t sz) { return fnv1_24(s, sz); }
//constexpr inline auto operator"" _fnv1_32(const char* s, size_t sz) { return fnv1_32(s, sz); }
//constexpr inline auto operator"" _fnv1_64(const char* s, size_t sz) { return fnv1_64(s, sz); }

constexpr inline auto operator"" _fnv1a16(const char* s, size_t sz) { return fnv1a_16_2(s, sz, FNV_32_offset_basis); }
constexpr inline auto operator"" _fnv1a24(const char* s, size_t sz) { return fnv1a_24_2(s, sz, FNV_32_offset_basis); }
constexpr inline auto operator"" _fnv1a32(const char* s, size_t sz) { return fnv1a_32_2(s, sz, FNV_32_offset_basis); }
constexpr inline auto operator"" _fnv1a64(const char* s, size_t sz) { return fnv1a_64_2(s, sz, FNV_64_offset_basis); }

template <typename T> struct fnva16hash;
template <> struct fnva16hash<char const* const> {
    uint16_t operator () (char const* s) const noexcept { return fnv1a_16(s, FNV_32_offset_basis); }
};
template <> struct fnva16hash<std::string> {
    uint16_t operator () (std::string const& s) const noexcept { return fnv1a_16_2(s.data(), s.size(), FNV_32_offset_basis); }
};
template <> struct fnva16hash<std::string_view> {
    uint16_t operator () (std::string_view const s) const noexcept { return fnv1a_16_2(s.data(), s.size(), FNV_32_offset_basis); }
};

template <typename T> class fnva24hash;
template <> class fnva24hash<char const* const> {
public:
    uint32_t operator () (char const* s) const noexcept { return fnv1a_24(s, FNV_32_offset_basis); }
};
template <> class fnva24hash<std::string> {
public:
    uint32_t operator () (std::string const& s) const noexcept { return fnv1a_24_2(s.data(), s.size(), FNV_32_offset_basis); }
};
template <> class fnva24hash<std::string_view> {
public:
    uint32_t operator () (std::string_view const s) const noexcept { return fnv1a_24_2(s.data(), s.size(), FNV_32_offset_basis); }
};

template <typename T> class fnva32hash;
template <> class fnva32hash<char const* const> {
public:
    uint32_t operator () (char const* s) const noexcept { return fnv1a_32(s, FNV_32_offset_basis); }
};
template <> class fnva32hash<std::string> {
public:
    uint32_t operator () (std::string const& s) const noexcept { return fnv1a_32_2(s.data(), s.size(), FNV_32_offset_basis); }
};
template <> class fnva32hash<std::string_view> {
public:
    uint32_t operator () (std::string_view const s) const noexcept { return fnv1a_32_2(s.data(), s.size(), FNV_32_offset_basis); }
};

template <typename T> class fnva64hash;
template <> class fnva64hash<char const* const> {
public:
    uint64_t operator () (char const* s) const noexcept { return fnv1a_64(s, FNV_64_offset_basis); }
};
template <> class fnva64hash<std::string> {
public:
    uint64_t operator () (std::string const& s) const noexcept { return fnv1a_64_2(s.data(), s.size(), FNV_64_offset_basis); }
};
template <> class fnva64hash<std::string_view> {
public:
    uint64_t operator () (std::string_view const s) const noexcept { return fnv1a_64_2(s.data(), s.size(), FNV_64_offset_basis); }
};
