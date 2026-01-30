#include "adler32.hpp"

std::uint32_t rez::adler32(std::span<const std::uint8_t> bytes)
{
    std::uint32_t sum1 {1u};
    std::uint32_t sum2 {0u};

    for(std::span<const std::uint8_t>::size_type i = 0; i < bytes.size(); ++i) {
        sum1 = (sum1 + bytes[i]) % 65521u;
        sum2 = (sum2 + sum1) % 65521u;
    }

    return (sum2 << 16u) + sum1;
}