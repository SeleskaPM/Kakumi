#pragma once

#include <cstdint>
#include <stdexcept>
#include <source_location>
#include <string>
#include <vector>
#include <fstream>
#include <concepts>
#include <bit>

#include "Rezbits/shared.hpp"

namespace kak {
    using std::int8_t; using std::uint8_t;
    using std::int16_t; using std::uint16_t;
    using std::int32_t; using std::uint32_t;
    using std::int64_t; using std::uint64_t;

    enum class Error {
        none,
        // general errors
        open_file_failed,
        read_file_failed,
        bad_formed_file,
        corrupted_file,
        unsupported_feature,
        memory_alloc_failed
    };

    class Exception : public std::exception {
    public:
        Exception(const Error error, std::source_location sl = std::source_location::current());
        const char* what() const noexcept final { return m_message.c_str(); }
        Error error() const noexcept { return m_error; }
        std::source_location source_location() const noexcept { return m_source_location; }
    private:
        std::string m_message;
        std::source_location m_source_location;
        Error m_error;
    };

    enum class PixelFormat {
        undefined,
        // 8-bits per channel
        grey, grey_alpha,
        rgb, rgba,
        bgr, bgra,
        // 16-bits per channel
        grey16, grey_alpha16,
        rgb16, rgba16
    };

    struct Image {
        std::vector<uint8_t> pixels;
        PixelFormat pixel_format {PixelFormat::undefined};
        int32_t width {0};
        int32_t height {0};
        bool premultiplied_alpha {false};
    };
}

namespace kak::impl {
    //constexpr bool network_byte_order {std::endian::native == std::endian::big};

    /* imagine an image that is 11585 x 11585 pixels and has 16
    * bytes per channel (RGBA). The total number of bytes of
    * the pixel data can be represented with an int32_t. For an
    * image that is 11586 x 11586 that is no longer the case.
    * Why does that matter? I just think it's a nice property.
    * Maybe I will personalize it for each image format but for
    * now, this stays as a global limit */
    constexpr int32_t max_pixel_dimension {11585};
    void check_against_max_dimension(int32_t width, int32_t height);

    template<std::integral T>
    T get_little_endian_value(std::ifstream& source)
    {
        T variable;
        source.read(reinterpret_cast<char*>(&variable), sizeof(T));
        if(not source.good()) throw Exception {Error::read_file_failed};
        if constexpr(std::endian::native != std::endian::little) { variable = rez::impl::byteswap(variable); }
        return variable;
    }

    template<std::integral T>
    T get_big_endian_value(std::ifstream& source)
    {
        T variable;
        source.read(reinterpret_cast<char*>(&variable), sizeof(T));
        if(not source.good()) throw Exception {Error::read_file_failed};
        if constexpr(std::endian::native != std::endian::big) { variable = rez::impl::byteswap(variable); }
        return variable;
    }

    uint8_t get_byte(std::ifstream& source);
    std::vector<uint8_t> get_bytes(std::ifstream& source, std::streamsize amount);

    template<std::integral T>
    constexpr T div8_ceil(T number) // originally called bits_to_bytes_per_whatever
    {
        /* The below (some_number + 7) >> 3 is clever code for dividing
        * some_number by 8 and (if necessary) rounding up */
        return (number + static_cast<T>(7)) >> 3;
    }
}