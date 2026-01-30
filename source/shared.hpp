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

    enum class Pixel_format {
        undefined,
        // 8-bits per channel
        grey, grey_alpha,
        rgb, rgba, argb,
        bgr, bgra, abgr,

        // 16-bits per channel
        grey16, grey_alpha16,
        rgb16, rgba16
    };

    struct Image {
        std::vector<std::uint8_t> pixel_data;
        Pixel_format pixel_format {Pixel_format::undefined};
        int image_width {0};
        int image_height {0};
        bool premultiplied_alpha {false};
    };
}

namespace kak::impl {
    //constexpr bool network_byte_order {std::endian::native == std::endian::big};

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

    std::vector<std::uint8_t> get_bytes(std::ifstream& source, std::streamsize amount);
}