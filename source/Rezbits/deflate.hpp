#pragma once

#include "shared.hpp"

#include <vector>
#include <array>

namespace rez {
    std::vector<std::uint8_t> decompress_deflate(std::span<const std::uint8_t> deflate_data);
}

namespace rez::impl::deflate {
    // base lengths for length symbols (257~285)
    constexpr std::array<int, 29> length_bases {
        3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
        35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258
    };

    // extra bits for length symbols
    constexpr std::array<int, 29> length_extra_bits {
        0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
        3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0
    };

    // base distances for distance symbols (0~29)
    constexpr std::array<int, 30> distance_bases {
        1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
        257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145,
        8193, 12289, 16385, 24577
    };

    // extra bits for distance symbols
    constexpr std::array<int, 30> distance_extra_bits {
        0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
        7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13
    };

    struct Huffman_entry {
        enum class Category {
            none, symbol, bridge
        };

        Category category {Category::none};
        /* category == Category::symbol
        *      value == a symbol
        *      bits_to_consume == bit-length of the Huffman code corresponding to the symbol
        *
        * category == Category::bridge
        *     value == an offset to some area of the decoding table
        *     bits_to_consume == the bit-width of said area */
        int value {0};
        int bits_to_consume {0};
    };

    /* this struct will act as a view into a mother buffer that will
    * contain all entries for all alphabets */
    struct Decoding_table {
        std::span<Huffman_entry> entries;
        int first_area_bitwidth;
    };

    using DeflateBitReader = BitReader<BitStreamFormat::gif>;

    void decompress_uncompressed(std::vector<std::uint8_t>& inflated_data, DeflateBitReader& bitstream);
    void decompress_fixed(std::vector<std::uint8_t>& inflated_data, DeflateBitReader& bitstream);
    void decompress_dynamic(std::vector<std::uint8_t>& inflated_data, std::vector<Huffman_entry>& mother_buffer, DeflateBitReader& bitstream);

    int fill_decoding_table_from_code_lengths(std::span<const int> code_lengths, std::span<Huffman_entry> decoding_table, int first_area_bitwidth);
    int fetch_symbol(const Decoding_table& decoding_table, DeflateBitReader& bitstream);
    void process_symbols(std::vector<std::uint8_t>& inflated_data, const Decoding_table& literal_length_alphabet, const Decoding_table& distance_alphabet, DeflateBitReader& bitstream);
    void lz77_copy(std::vector<std::uint8_t>& inflated_data, int length, const std::int32_t distance);
}
