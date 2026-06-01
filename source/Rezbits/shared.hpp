#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <source_location>
#include <type_traits>
#include <concepts>
#include <span>
#include <bit>
#include <cstring>

namespace rez {

enum class Error {
    none,
    bad_argument,
    unsupported_feature,
    bad_formed_data,
    corrupted_data,
    unexpected_eof,
};

class Exception : public std::exception {
public:
    Exception(Error error, std::source_location sl = std::source_location::current());
    
    const char* what() const noexcept final { return m_message.c_str(); }
    Error error() const noexcept { return m_error; }
    std::source_location source_location() const noexcept { return m_source_location; }
private:
    std::string m_message;
    std::source_location m_source_location;
    Error m_error;
};

} // namespace rez

namespace rez::impl {

template<std::integral T>
constexpr T byteswap(T value) noexcept
{
    if constexpr (sizeof(T) == 1u) return value;

    using unsigned_type = std::make_unsigned_t<T>;

    const unsigned_type copy {static_cast<unsigned_type>(value)};
    unsigned_type result {0u};

    constexpr int bytes_in_type {static_cast<int>(sizeof(T))};
    for (int i = 0; i < bytes_in_type; ++i) {
        result |= ((copy >> (8 * i)) & 0xFFu) << (8 * (bytes_in_type - 1 - i));
    }

    return static_cast<T>(result);
}

template<std::integral T>
constexpr T reverse_bits(T value)
{
    if constexpr (sizeof(T) == 1) {
        std::uint32_t copy {value};
        // Devised by Axel Walthelm (https://github.com/AxelWalthelm/Bit-Twiddling)
        return static_cast<T>((((copy * 0x01010101u) & 0x10488224u) * 0x10400411u >> 25) | (copy << 7));
    }
    else if constexpr (sizeof(T) == 2) {
        std::uint32_t lo {static_cast<std::uint32_t>(value) & 0xFFu};
        std::uint32_t hi {static_cast<std::uint32_t>(value) >> 8};

        lo = ((((lo * 0x01010101u) & 0x10488224u) * 0x10400411u >> 25) | (lo << 7)) & 0xFFu;
        hi = ((((hi * 0x01010101u) & 0x10488224u) * 0x10400411u >> 25) | (hi << 7)) & 0xFFu;
        return static_cast<T>(hi | (lo << 8));
    }
    else if constexpr (sizeof(T) == 4) {
        std::uint32_t copy {static_cast<std::uint32_t>(value)};
        // From https://aggregate.org/MAGIC/
        copy = ((copy & 0xAAAAAAAAu) >> 1) | ((copy & 0x55555555u) << 1);
        copy = ((copy & 0xCCCCCCCCu) >> 2) | ((copy & 0x33333333u) << 2);
        copy = ((copy & 0xF0F0F0F0u) >> 4) | ((copy & 0x0F0F0F0Fu) << 4);
        copy = ((copy & 0xFF00FF00u) >> 8) | ((copy & 0x00FF00FFu) << 8);
        return static_cast<T>((copy >> 16) | (copy << 16));
    }
    else if constexpr (sizeof(T) == 8) {
        std::uint64_t copy {static_cast<std::uint64_t>(value)};
        copy = ((copy & 0xAAAAAAAAAAAAAAAAull) >> 1) | ((copy & 0x5555555555555555ull) << 1);
        copy = ((copy & 0xCCCCCCCCCCCCCCCCull) >> 2) | ((copy & 0x3333333333333333ull) << 2);
        copy = ((copy & 0xF0F0F0F0F0F0F0F0ull) >> 4) | ((copy & 0x0F0F0F0F0F0F0F0Full) << 4);
        copy = ((copy & 0xFF00FF00FF00FF00ull) >> 8) | ((copy & 0x00FF00FF00FF00FFull) << 8);
        copy = ((copy & 0xFFFF0000FFFF0000ull) >> 16) | ((copy & 0x0000FFFF0000FFFFull) << 16);
        return static_cast<T>((copy >> 32) | (copy << 32));
    }
}

template<std::integral T>
constexpr T reverse_bits_from_lsbit(const T value, int amount)
{
    if (amount < 2) return value;

    constexpr int size_in_bits {static_cast<int>(sizeof(T)) * 8};
    if (amount > size_in_bits) throw Exception(Error::bad_argument);
    if (amount == size_in_bits) return reverse_bits(value);

    using unsigned_type = std::make_unsigned_t<T>;

    const unsigned_type copy {static_cast<unsigned_type>(value)};
    unsigned_type result {0u};
    constexpr unsigned_type one {1u};

    for (int i = 0; i < amount; ++i) {
        result |= ((copy & (one << i)) >> i) << (amount - 1 - i);
    }

    const unsigned_type mask {static_cast<unsigned_type>(static_cast<unsigned_type>(-1) << amount)};
    return static_cast<T>(result | (copy & mask));
}

class ByteReader {
public:
    ByteReader() noexcept {}

    ByteReader(std::span<const std::uint8_t> source) noexcept
        : m_source(source),
        m_source_size {static_cast<std::int64_t>(source.size())}
    {
        // nothing
    }

    void plug_source(std::span<const std::uint8_t> source) noexcept;
    void rewind() noexcept { m_byte_index = 0; }

    template<std::integral T>
    T read_value_as_is();

    template<std::integral T>
    T read_little_endian_value();

    template<std::integral T>
    T read_big_endian_value();

    std::uint8_t read_byte();
    std::span<const std::uint8_t> read_bytes(std::int32_t amount);

    void skip_bytes(std::int32_t amount);
private:
    std::span<const std::uint8_t> m_source;
    std::int64_t m_source_size {0};
    // index for the current byte
    std::int64_t m_byte_index {0};
};

template<std::integral T>
T ByteReader::read_value_as_is()
{
    if (m_byte_index + sizeof(T) > m_source.size()) {
        throw Exception(Error::bad_formed_data);
    }

    T result;
    std::memcpy(&result, &m_source[m_byte_index], sizeof(T));
    m_byte_index += sizeof(T);

    return result;
}

template<std::integral T>
T ByteReader::read_little_endian_value()
{
    T result {read_value_as_is<T>()};
    if constexpr (std::endian::native != std::endian::little) {
        result = byteswap(result);
    }

    return result;
}

template<std::integral T>
T ByteReader::read_big_endian_value()
{
    T result {read_value_as_is<T>()};
    if constexpr (std::endian::native != std::endian::big) {
        result = byteswap(result);
    }

    return result;
}

enum class BitStreamFormat {
    /* in the examples, 'a' is a 10 bits value and 'b' is a 6 bits value.
    * 'a' is the first value in the bit-stream and 'b' is the second.
    * the numbers inside the parenthesis indicate the significant bits
    * of the values, 0 is the less significant bit. */
    gif, // byte 0: aaaaaaaa (76543210), byte 1: bbbbbbaa (54321098)
    jpg // byte 0: aaaaaaaa (98765432), byte 1: aabbbbbb (10543210)
};

template<BitStreamFormat Format>
class BitReader {
public:
    BitReader() noexcept {}

    BitReader(std::span<const std::uint8_t> source) noexcept
        : m_source(source),
        m_byte_index {0},
        m_last_valid_index {static_cast<std::int64_t>(source.size()) - 1},
        m_bits_remaining {static_cast<std::int64_t>(source.size()) * 8},
        m_useful_bits_in_current_byte {8}

    {
        // nothing
    }

    void plug_source(std::span<const std::uint8_t> source) noexcept;
    void rewind() noexcept;

    std::int32_t read_bits(int amount);
    std::int32_t peek_bits(int amount);
    void skip_bits(int amount);
    void skip_until_next_byte_boundary();

    std::span<const std::uint8_t> read_bytes(int amount);

    std::int64_t bits_remaining() noexcept { return m_bits_remaining; }
private:
    // [verb]_bits_FORMAT
    //std::uint32_t read_bits_lsbit(const std::uint32_t amount);

    std::span<const std::uint8_t> m_source;
    // index for the current byte
    std::int64_t m_byte_index {-1};
    std::int64_t m_last_valid_index {-1};
    std::int64_t m_bits_remaining {0};
    int m_useful_bits_in_current_byte {0};
};

} // namespace rez::impl
