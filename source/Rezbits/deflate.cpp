#include "deflate.hpp"

#include <algorithm>

std::vector<std::uint8_t> rez::decompress_deflate(std::span<const std::uint8_t> deflate_data)
{
    impl::deflate::Deflate_bitstream bitstream {deflate_data};
    std::vector<std::uint8_t> inflated_data;
    inflated_data.reserve(25000); // 25KB
    //inflated_data.reserve(110596800); // only to test

    /* 1444 is the sum of 852 and 592. 852 is the maximum number of
    * entries that the decoding table for the literal+length alphabet
    * can have when the bit-width of its first area for that alphabet
    * is 9. 592 is the maximum for the decoding table of the distance
    * alphabet when the bit-width of the first area of its decoding
    * table is 6 */
    std::vector<impl::deflate::Huffman_entry> mother_buffer(1444);

    int bfinal {0};
    int btype {0};
    do {
        bfinal = bitstream.read_bits(1);
        btype = bitstream.read_bits(2);
        switch(btype) {
            case 0: // no compression
                bitstream.skip_until_next_byte_boundary();
                impl::deflate::decompress_uncompressed(inflated_data, bitstream);
                break;
            case 1: // fixed Huffman codes
                impl::deflate::decompress_fixed(inflated_data, bitstream);
                break;
            case 2: // dynamic Huffman codes
                impl::deflate::decompress_dynamic(inflated_data, mother_buffer, bitstream);
                break;
            default:
                throw Exception {Error::bad_formed_data};
        }
    } while(not bfinal);

    return inflated_data;
}

void rez::impl::deflate::decompress_uncompressed(std::vector<std::uint8_t>& inflated_data, Deflate_bitstream& bitstream)
{
    const std::int32_t len {bitstream.read_bits(16)};
    const std::int32_t nlen {bitstream.read_bits(16)};

    if((~len & 0xFFFF) != nlen) throw Exception {Error::bad_formed_data};
    if(len == 0) return; // zero length is allowed

    std::span<const std::uint8_t> uncompressed_data {bitstream.read_bytes(len)};
    inflated_data.insert(inflated_data.end(), uncompressed_data.begin(), uncompressed_data.end());
}

void rez::impl::deflate::decompress_fixed(std::vector<std::uint8_t>& inflated_data, Deflate_bitstream& bitstream)
{
    /* I don't want to allocate the mother buffer for fixed blocks on
    * the stack because the amount of memory required is not little
    * (relatively speaking). And I don't want to allocate it before
    * the decompressing loop because (I think) fixed blocks are rare,
    * so it would create overhead for something that is not going to
    * be used most of the time */

    std::vector<Huffman_entry> mother_buffer {
        // does your editor have code folding? ;A;
        {Huffman_entry::Category::symbol, 256, 7},
        {Huffman_entry::Category::symbol, 80, 8},
        {Huffman_entry::Category::symbol, 16, 8},
        {Huffman_entry::Category::symbol, 280, 8},
        {Huffman_entry::Category::symbol, 272, 7},
        {Huffman_entry::Category::symbol, 112, 8},
        {Huffman_entry::Category::symbol, 48, 8},
        {Huffman_entry::Category::symbol, 192, 9},
        {Huffman_entry::Category::symbol, 264, 7},
        {Huffman_entry::Category::symbol, 96, 8},
        {Huffman_entry::Category::symbol, 32, 8},
        {Huffman_entry::Category::symbol, 160, 9},
        {Huffman_entry::Category::symbol, 0, 8},
        {Huffman_entry::Category::symbol, 128, 8},
        {Huffman_entry::Category::symbol, 64, 8},
        {Huffman_entry::Category::symbol, 224, 9},
        {Huffman_entry::Category::symbol, 260, 7},
        {Huffman_entry::Category::symbol, 88, 8},
        {Huffman_entry::Category::symbol, 24, 8},
        {Huffman_entry::Category::symbol, 144, 9},
        {Huffman_entry::Category::symbol, 276, 7},
        {Huffman_entry::Category::symbol, 120, 8},
        {Huffman_entry::Category::symbol, 56, 8},
        {Huffman_entry::Category::symbol, 208, 9},
        {Huffman_entry::Category::symbol, 268, 7},
        {Huffman_entry::Category::symbol, 104, 8},
        {Huffman_entry::Category::symbol, 40, 8},
        {Huffman_entry::Category::symbol, 176, 9},
        {Huffman_entry::Category::symbol, 8, 8},
        {Huffman_entry::Category::symbol, 136, 8},
        {Huffman_entry::Category::symbol, 72, 8},
        {Huffman_entry::Category::symbol, 240, 9},
        {Huffman_entry::Category::symbol, 258, 7},
        {Huffman_entry::Category::symbol, 84, 8},
        {Huffman_entry::Category::symbol, 20, 8},
        {Huffman_entry::Category::symbol, 284, 8},
        {Huffman_entry::Category::symbol, 274, 7},
        {Huffman_entry::Category::symbol, 116, 8},
        {Huffman_entry::Category::symbol, 52, 8},
        {Huffman_entry::Category::symbol, 200, 9},
        {Huffman_entry::Category::symbol, 266, 7},
        {Huffman_entry::Category::symbol, 100, 8},
        {Huffman_entry::Category::symbol, 36, 8},
        {Huffman_entry::Category::symbol, 168, 9},
        {Huffman_entry::Category::symbol, 4, 8},
        {Huffman_entry::Category::symbol, 132, 8},
        {Huffman_entry::Category::symbol, 68, 8},
        {Huffman_entry::Category::symbol, 232, 9},
        {Huffman_entry::Category::symbol, 262, 7},
        {Huffman_entry::Category::symbol, 92, 8},
        {Huffman_entry::Category::symbol, 28, 8},
        {Huffman_entry::Category::symbol, 152, 9},
        {Huffman_entry::Category::symbol, 278, 7},
        {Huffman_entry::Category::symbol, 124, 8},
        {Huffman_entry::Category::symbol, 60, 8},
        {Huffman_entry::Category::symbol, 216, 9},
        {Huffman_entry::Category::symbol, 270, 7},
        {Huffman_entry::Category::symbol, 108, 8},
        {Huffman_entry::Category::symbol, 44, 8},
        {Huffman_entry::Category::symbol, 184, 9},
        {Huffman_entry::Category::symbol, 12, 8},
        {Huffman_entry::Category::symbol, 140, 8},
        {Huffman_entry::Category::symbol, 76, 8},
        {Huffman_entry::Category::symbol, 248, 9},
        {Huffman_entry::Category::symbol, 257, 7},
        {Huffman_entry::Category::symbol, 82, 8},
        {Huffman_entry::Category::symbol, 18, 8},
        {Huffman_entry::Category::symbol, 282, 8},
        {Huffman_entry::Category::symbol, 273, 7},
        {Huffman_entry::Category::symbol, 114, 8},
        {Huffman_entry::Category::symbol, 50, 8},
        {Huffman_entry::Category::symbol, 196, 9},
        {Huffman_entry::Category::symbol, 265, 7},
        {Huffman_entry::Category::symbol, 98, 8},
        {Huffman_entry::Category::symbol, 34, 8},
        {Huffman_entry::Category::symbol, 164, 9},
        {Huffman_entry::Category::symbol, 2, 8},
        {Huffman_entry::Category::symbol, 130, 8},
        {Huffman_entry::Category::symbol, 66, 8},
        {Huffman_entry::Category::symbol, 228, 9},
        {Huffman_entry::Category::symbol, 261, 7},
        {Huffman_entry::Category::symbol, 90, 8},
        {Huffman_entry::Category::symbol, 26, 8},
        {Huffman_entry::Category::symbol, 148, 9},
        {Huffman_entry::Category::symbol, 277, 7},
        {Huffman_entry::Category::symbol, 122, 8},
        {Huffman_entry::Category::symbol, 58, 8},
        {Huffman_entry::Category::symbol, 212, 9},
        {Huffman_entry::Category::symbol, 269, 7},
        {Huffman_entry::Category::symbol, 106, 8},
        {Huffman_entry::Category::symbol, 42, 8},
        {Huffman_entry::Category::symbol, 180, 9},
        {Huffman_entry::Category::symbol, 10, 8},
        {Huffman_entry::Category::symbol, 138, 8},
        {Huffman_entry::Category::symbol, 74, 8},
        {Huffman_entry::Category::symbol, 244, 9},
        {Huffman_entry::Category::symbol, 259, 7},
        {Huffman_entry::Category::symbol, 86, 8},
        {Huffman_entry::Category::symbol, 22, 8},
        {Huffman_entry::Category::symbol, 286, 8},
        {Huffman_entry::Category::symbol, 275, 7},
        {Huffman_entry::Category::symbol, 118, 8},
        {Huffman_entry::Category::symbol, 54, 8},
        {Huffman_entry::Category::symbol, 204, 9},
        {Huffman_entry::Category::symbol, 267, 7},
        {Huffman_entry::Category::symbol, 102, 8},
        {Huffman_entry::Category::symbol, 38, 8},
        {Huffman_entry::Category::symbol, 172, 9},
        {Huffman_entry::Category::symbol, 6, 8},
        {Huffman_entry::Category::symbol, 134, 8},
        {Huffman_entry::Category::symbol, 70, 8},
        {Huffman_entry::Category::symbol, 236, 9},
        {Huffman_entry::Category::symbol, 263, 7},
        {Huffman_entry::Category::symbol, 94, 8},
        {Huffman_entry::Category::symbol, 30, 8},
        {Huffman_entry::Category::symbol, 156, 9},
        {Huffman_entry::Category::symbol, 279, 7},
        {Huffman_entry::Category::symbol, 126, 8},
        {Huffman_entry::Category::symbol, 62, 8},
        {Huffman_entry::Category::symbol, 220, 9},
        {Huffman_entry::Category::symbol, 271, 7},
        {Huffman_entry::Category::symbol, 110, 8},
        {Huffman_entry::Category::symbol, 46, 8},
        {Huffman_entry::Category::symbol, 188, 9},
        {Huffman_entry::Category::symbol, 14, 8},
        {Huffman_entry::Category::symbol, 142, 8},
        {Huffman_entry::Category::symbol, 78, 8},
        {Huffman_entry::Category::symbol, 252, 9},
        {Huffman_entry::Category::symbol, 256, 7},
        {Huffman_entry::Category::symbol, 81, 8},
        {Huffman_entry::Category::symbol, 17, 8},
        {Huffman_entry::Category::symbol, 281, 8},
        {Huffman_entry::Category::symbol, 272, 7},
        {Huffman_entry::Category::symbol, 113, 8},
        {Huffman_entry::Category::symbol, 49, 8},
        {Huffman_entry::Category::symbol, 194, 9},
        {Huffman_entry::Category::symbol, 264, 7},
        {Huffman_entry::Category::symbol, 97, 8},
        {Huffman_entry::Category::symbol, 33, 8},
        {Huffman_entry::Category::symbol, 162, 9},
        {Huffman_entry::Category::symbol, 1, 8},
        {Huffman_entry::Category::symbol, 129, 8},
        {Huffman_entry::Category::symbol, 65, 8},
        {Huffman_entry::Category::symbol, 226, 9},
        {Huffman_entry::Category::symbol, 260, 7},
        {Huffman_entry::Category::symbol, 89, 8},
        {Huffman_entry::Category::symbol, 25, 8},
        {Huffman_entry::Category::symbol, 146, 9},
        {Huffman_entry::Category::symbol, 276, 7},
        {Huffman_entry::Category::symbol, 121, 8},
        {Huffman_entry::Category::symbol, 57, 8},
        {Huffman_entry::Category::symbol, 210, 9},
        {Huffman_entry::Category::symbol, 268, 7},
        {Huffman_entry::Category::symbol, 105, 8},
        {Huffman_entry::Category::symbol, 41, 8},
        {Huffman_entry::Category::symbol, 178, 9},
        {Huffman_entry::Category::symbol, 9, 8},
        {Huffman_entry::Category::symbol, 137, 8},
        {Huffman_entry::Category::symbol, 73, 8},
        {Huffman_entry::Category::symbol, 242, 9},
        {Huffman_entry::Category::symbol, 258, 7},
        {Huffman_entry::Category::symbol, 85, 8},
        {Huffman_entry::Category::symbol, 21, 8},
        {Huffman_entry::Category::symbol, 285, 8},
        {Huffman_entry::Category::symbol, 274, 7},
        {Huffman_entry::Category::symbol, 117, 8},
        {Huffman_entry::Category::symbol, 53, 8},
        {Huffman_entry::Category::symbol, 202, 9},
        {Huffman_entry::Category::symbol, 266, 7},
        {Huffman_entry::Category::symbol, 101, 8},
        {Huffman_entry::Category::symbol, 37, 8},
        {Huffman_entry::Category::symbol, 170, 9},
        {Huffman_entry::Category::symbol, 5, 8},
        {Huffman_entry::Category::symbol, 133, 8},
        {Huffman_entry::Category::symbol, 69, 8},
        {Huffman_entry::Category::symbol, 234, 9},
        {Huffman_entry::Category::symbol, 262, 7},
        {Huffman_entry::Category::symbol, 93, 8},
        {Huffman_entry::Category::symbol, 29, 8},
        {Huffman_entry::Category::symbol, 154, 9},
        {Huffman_entry::Category::symbol, 278, 7},
        {Huffman_entry::Category::symbol, 125, 8},
        {Huffman_entry::Category::symbol, 61, 8},
        {Huffman_entry::Category::symbol, 218, 9},
        {Huffman_entry::Category::symbol, 270, 7},
        {Huffman_entry::Category::symbol, 109, 8},
        {Huffman_entry::Category::symbol, 45, 8},
        {Huffman_entry::Category::symbol, 186, 9},
        {Huffman_entry::Category::symbol, 13, 8},
        {Huffman_entry::Category::symbol, 141, 8},
        {Huffman_entry::Category::symbol, 77, 8},
        {Huffman_entry::Category::symbol, 250, 9},
        {Huffman_entry::Category::symbol, 257, 7},
        {Huffman_entry::Category::symbol, 83, 8},
        {Huffman_entry::Category::symbol, 19, 8},
        {Huffman_entry::Category::symbol, 283, 8},
        {Huffman_entry::Category::symbol, 273, 7},
        {Huffman_entry::Category::symbol, 115, 8},
        {Huffman_entry::Category::symbol, 51, 8},
        {Huffman_entry::Category::symbol, 198, 9},
        {Huffman_entry::Category::symbol, 265, 7},
        {Huffman_entry::Category::symbol, 99, 8},
        {Huffman_entry::Category::symbol, 35, 8},
        {Huffman_entry::Category::symbol, 166, 9},
        {Huffman_entry::Category::symbol, 3, 8},
        {Huffman_entry::Category::symbol, 131, 8},
        {Huffman_entry::Category::symbol, 67, 8},
        {Huffman_entry::Category::symbol, 230, 9},
        {Huffman_entry::Category::symbol, 261, 7},
        {Huffman_entry::Category::symbol, 91, 8},
        {Huffman_entry::Category::symbol, 27, 8},
        {Huffman_entry::Category::symbol, 150, 9},
        {Huffman_entry::Category::symbol, 277, 7},
        {Huffman_entry::Category::symbol, 123, 8},
        {Huffman_entry::Category::symbol, 59, 8},
        {Huffman_entry::Category::symbol, 214, 9},
        {Huffman_entry::Category::symbol, 269, 7},
        {Huffman_entry::Category::symbol, 107, 8},
        {Huffman_entry::Category::symbol, 43, 8},
        {Huffman_entry::Category::symbol, 182, 9},
        {Huffman_entry::Category::symbol, 11, 8},
        {Huffman_entry::Category::symbol, 139, 8},
        {Huffman_entry::Category::symbol, 75, 8},
        {Huffman_entry::Category::symbol, 246, 9},
        {Huffman_entry::Category::symbol, 259, 7},
        {Huffman_entry::Category::symbol, 87, 8},
        {Huffman_entry::Category::symbol, 23, 8},
        {Huffman_entry::Category::symbol, 287, 8},
        {Huffman_entry::Category::symbol, 275, 7},
        {Huffman_entry::Category::symbol, 119, 8},
        {Huffman_entry::Category::symbol, 55, 8},
        {Huffman_entry::Category::symbol, 206, 9},
        {Huffman_entry::Category::symbol, 267, 7},
        {Huffman_entry::Category::symbol, 103, 8},
        {Huffman_entry::Category::symbol, 39, 8},
        {Huffman_entry::Category::symbol, 174, 9},
        {Huffman_entry::Category::symbol, 7, 8},
        {Huffman_entry::Category::symbol, 135, 8},
        {Huffman_entry::Category::symbol, 71, 8},
        {Huffman_entry::Category::symbol, 238, 9},
        {Huffman_entry::Category::symbol, 263, 7},
        {Huffman_entry::Category::symbol, 95, 8},
        {Huffman_entry::Category::symbol, 31, 8},
        {Huffman_entry::Category::symbol, 158, 9},
        {Huffman_entry::Category::symbol, 279, 7},
        {Huffman_entry::Category::symbol, 127, 8},
        {Huffman_entry::Category::symbol, 63, 8},
        {Huffman_entry::Category::symbol, 222, 9},
        {Huffman_entry::Category::symbol, 271, 7},
        {Huffman_entry::Category::symbol, 111, 8},
        {Huffman_entry::Category::symbol, 47, 8},
        {Huffman_entry::Category::symbol, 190, 9},
        {Huffman_entry::Category::symbol, 15, 8},
        {Huffman_entry::Category::symbol, 143, 8},
        {Huffman_entry::Category::symbol, 79, 8},
        {Huffman_entry::Category::symbol, 254, 9},
        {Huffman_entry::Category::symbol, 256, 7},
        {Huffman_entry::Category::symbol, 80, 8},
        {Huffman_entry::Category::symbol, 16, 8},
        {Huffman_entry::Category::symbol, 280, 8},
        {Huffman_entry::Category::symbol, 272, 7},
        {Huffman_entry::Category::symbol, 112, 8},
        {Huffman_entry::Category::symbol, 48, 8},
        {Huffman_entry::Category::symbol, 193, 9},
        {Huffman_entry::Category::symbol, 264, 7},
        {Huffman_entry::Category::symbol, 96, 8},
        {Huffman_entry::Category::symbol, 32, 8},
        {Huffman_entry::Category::symbol, 161, 9},
        {Huffman_entry::Category::symbol, 0, 8},
        {Huffman_entry::Category::symbol, 128, 8},
        {Huffman_entry::Category::symbol, 64, 8},
        {Huffman_entry::Category::symbol, 225, 9},
        {Huffman_entry::Category::symbol, 260, 7},
        {Huffman_entry::Category::symbol, 88, 8},
        {Huffman_entry::Category::symbol, 24, 8},
        {Huffman_entry::Category::symbol, 145, 9},
        {Huffman_entry::Category::symbol, 276, 7},
        {Huffman_entry::Category::symbol, 120, 8},
        {Huffman_entry::Category::symbol, 56, 8},
        {Huffman_entry::Category::symbol, 209, 9},
        {Huffman_entry::Category::symbol, 268, 7},
        {Huffman_entry::Category::symbol, 104, 8},
        {Huffman_entry::Category::symbol, 40, 8},
        {Huffman_entry::Category::symbol, 177, 9},
        {Huffman_entry::Category::symbol, 8, 8},
        {Huffman_entry::Category::symbol, 136, 8},
        {Huffman_entry::Category::symbol, 72, 8},
        {Huffman_entry::Category::symbol, 241, 9},
        {Huffman_entry::Category::symbol, 258, 7},
        {Huffman_entry::Category::symbol, 84, 8},
        {Huffman_entry::Category::symbol, 20, 8},
        {Huffman_entry::Category::symbol, 284, 8},
        {Huffman_entry::Category::symbol, 274, 7},
        {Huffman_entry::Category::symbol, 116, 8},
        {Huffman_entry::Category::symbol, 52, 8},
        {Huffman_entry::Category::symbol, 201, 9},
        {Huffman_entry::Category::symbol, 266, 7},
        {Huffman_entry::Category::symbol, 100, 8},
        {Huffman_entry::Category::symbol, 36, 8},
        {Huffman_entry::Category::symbol, 169, 9},
        {Huffman_entry::Category::symbol, 4, 8},
        {Huffman_entry::Category::symbol, 132, 8},
        {Huffman_entry::Category::symbol, 68, 8},
        {Huffman_entry::Category::symbol, 233, 9},
        {Huffman_entry::Category::symbol, 262, 7},
        {Huffman_entry::Category::symbol, 92, 8},
        {Huffman_entry::Category::symbol, 28, 8},
        {Huffman_entry::Category::symbol, 153, 9},
        {Huffman_entry::Category::symbol, 278, 7},
        {Huffman_entry::Category::symbol, 124, 8},
        {Huffman_entry::Category::symbol, 60, 8},
        {Huffman_entry::Category::symbol, 217, 9},
        {Huffman_entry::Category::symbol, 270, 7},
        {Huffman_entry::Category::symbol, 108, 8},
        {Huffman_entry::Category::symbol, 44, 8},
        {Huffman_entry::Category::symbol, 185, 9},
        {Huffman_entry::Category::symbol, 12, 8},
        {Huffman_entry::Category::symbol, 140, 8},
        {Huffman_entry::Category::symbol, 76, 8},
        {Huffman_entry::Category::symbol, 249, 9},
        {Huffman_entry::Category::symbol, 257, 7},
        {Huffman_entry::Category::symbol, 82, 8},
        {Huffman_entry::Category::symbol, 18, 8},
        {Huffman_entry::Category::symbol, 282, 8},
        {Huffman_entry::Category::symbol, 273, 7},
        {Huffman_entry::Category::symbol, 114, 8},
        {Huffman_entry::Category::symbol, 50, 8},
        {Huffman_entry::Category::symbol, 197, 9},
        {Huffman_entry::Category::symbol, 265, 7},
        {Huffman_entry::Category::symbol, 98, 8},
        {Huffman_entry::Category::symbol, 34, 8},
        {Huffman_entry::Category::symbol, 165, 9},
        {Huffman_entry::Category::symbol, 2, 8},
        {Huffman_entry::Category::symbol, 130, 8},
        {Huffman_entry::Category::symbol, 66, 8},
        {Huffman_entry::Category::symbol, 229, 9},
        {Huffman_entry::Category::symbol, 261, 7},
        {Huffman_entry::Category::symbol, 90, 8},
        {Huffman_entry::Category::symbol, 26, 8},
        {Huffman_entry::Category::symbol, 149, 9},
        {Huffman_entry::Category::symbol, 277, 7},
        {Huffman_entry::Category::symbol, 122, 8},
        {Huffman_entry::Category::symbol, 58, 8},
        {Huffman_entry::Category::symbol, 213, 9},
        {Huffman_entry::Category::symbol, 269, 7},
        {Huffman_entry::Category::symbol, 106, 8},
        {Huffman_entry::Category::symbol, 42, 8},
        {Huffman_entry::Category::symbol, 181, 9},
        {Huffman_entry::Category::symbol, 10, 8},
        {Huffman_entry::Category::symbol, 138, 8},
        {Huffman_entry::Category::symbol, 74, 8},
        {Huffman_entry::Category::symbol, 245, 9},
        {Huffman_entry::Category::symbol, 259, 7},
        {Huffman_entry::Category::symbol, 86, 8},
        {Huffman_entry::Category::symbol, 22, 8},
        {Huffman_entry::Category::symbol, 286, 8},
        {Huffman_entry::Category::symbol, 275, 7},
        {Huffman_entry::Category::symbol, 118, 8},
        {Huffman_entry::Category::symbol, 54, 8},
        {Huffman_entry::Category::symbol, 205, 9},
        {Huffman_entry::Category::symbol, 267, 7},
        {Huffman_entry::Category::symbol, 102, 8},
        {Huffman_entry::Category::symbol, 38, 8},
        {Huffman_entry::Category::symbol, 173, 9},
        {Huffman_entry::Category::symbol, 6, 8},
        {Huffman_entry::Category::symbol, 134, 8},
        {Huffman_entry::Category::symbol, 70, 8},
        {Huffman_entry::Category::symbol, 237, 9},
        {Huffman_entry::Category::symbol, 263, 7},
        {Huffman_entry::Category::symbol, 94, 8},
        {Huffman_entry::Category::symbol, 30, 8},
        {Huffman_entry::Category::symbol, 157, 9},
        {Huffman_entry::Category::symbol, 279, 7},
        {Huffman_entry::Category::symbol, 126, 8},
        {Huffman_entry::Category::symbol, 62, 8},
        {Huffman_entry::Category::symbol, 221, 9},
        {Huffman_entry::Category::symbol, 271, 7},
        {Huffman_entry::Category::symbol, 110, 8},
        {Huffman_entry::Category::symbol, 46, 8},
        {Huffman_entry::Category::symbol, 189, 9},
        {Huffman_entry::Category::symbol, 14, 8},
        {Huffman_entry::Category::symbol, 142, 8},
        {Huffman_entry::Category::symbol, 78, 8},
        {Huffman_entry::Category::symbol, 253, 9},
        {Huffman_entry::Category::symbol, 256, 7},
        {Huffman_entry::Category::symbol, 81, 8},
        {Huffman_entry::Category::symbol, 17, 8},
        {Huffman_entry::Category::symbol, 281, 8},
        {Huffman_entry::Category::symbol, 272, 7},
        {Huffman_entry::Category::symbol, 113, 8},
        {Huffman_entry::Category::symbol, 49, 8},
        {Huffman_entry::Category::symbol, 195, 9},
        {Huffman_entry::Category::symbol, 264, 7},
        {Huffman_entry::Category::symbol, 97, 8},
        {Huffman_entry::Category::symbol, 33, 8},
        {Huffman_entry::Category::symbol, 163, 9},
        {Huffman_entry::Category::symbol, 1, 8},
        {Huffman_entry::Category::symbol, 129, 8},
        {Huffman_entry::Category::symbol, 65, 8},
        {Huffman_entry::Category::symbol, 227, 9},
        {Huffman_entry::Category::symbol, 260, 7},
        {Huffman_entry::Category::symbol, 89, 8},
        {Huffman_entry::Category::symbol, 25, 8},
        {Huffman_entry::Category::symbol, 147, 9},
        {Huffman_entry::Category::symbol, 276, 7},
        {Huffman_entry::Category::symbol, 121, 8},
        {Huffman_entry::Category::symbol, 57, 8},
        {Huffman_entry::Category::symbol, 211, 9},
        {Huffman_entry::Category::symbol, 268, 7},
        {Huffman_entry::Category::symbol, 105, 8},
        {Huffman_entry::Category::symbol, 41, 8},
        {Huffman_entry::Category::symbol, 179, 9},
        {Huffman_entry::Category::symbol, 9, 8},
        {Huffman_entry::Category::symbol, 137, 8},
        {Huffman_entry::Category::symbol, 73, 8},
        {Huffman_entry::Category::symbol, 243, 9},
        {Huffman_entry::Category::symbol, 258, 7},
        {Huffman_entry::Category::symbol, 85, 8},
        {Huffman_entry::Category::symbol, 21, 8},
        {Huffman_entry::Category::symbol, 285, 8},
        {Huffman_entry::Category::symbol, 274, 7},
        {Huffman_entry::Category::symbol, 117, 8},
        {Huffman_entry::Category::symbol, 53, 8},
        {Huffman_entry::Category::symbol, 203, 9},
        {Huffman_entry::Category::symbol, 266, 7},
        {Huffman_entry::Category::symbol, 101, 8},
        {Huffman_entry::Category::symbol, 37, 8},
        {Huffman_entry::Category::symbol, 171, 9},
        {Huffman_entry::Category::symbol, 5, 8},
        {Huffman_entry::Category::symbol, 133, 8},
        {Huffman_entry::Category::symbol, 69, 8},
        {Huffman_entry::Category::symbol, 235, 9},
        {Huffman_entry::Category::symbol, 262, 7},
        {Huffman_entry::Category::symbol, 93, 8},
        {Huffman_entry::Category::symbol, 29, 8},
        {Huffman_entry::Category::symbol, 155, 9},
        {Huffman_entry::Category::symbol, 278, 7},
        {Huffman_entry::Category::symbol, 125, 8},
        {Huffman_entry::Category::symbol, 61, 8},
        {Huffman_entry::Category::symbol, 219, 9},
        {Huffman_entry::Category::symbol, 270, 7},
        {Huffman_entry::Category::symbol, 109, 8},
        {Huffman_entry::Category::symbol, 45, 8},
        {Huffman_entry::Category::symbol, 187, 9},
        {Huffman_entry::Category::symbol, 13, 8},
        {Huffman_entry::Category::symbol, 141, 8},
        {Huffman_entry::Category::symbol, 77, 8},
        {Huffman_entry::Category::symbol, 251, 9},
        {Huffman_entry::Category::symbol, 257, 7},
        {Huffman_entry::Category::symbol, 83, 8},
        {Huffman_entry::Category::symbol, 19, 8},
        {Huffman_entry::Category::symbol, 283, 8},
        {Huffman_entry::Category::symbol, 273, 7},
        {Huffman_entry::Category::symbol, 115, 8},
        {Huffman_entry::Category::symbol, 51, 8},
        {Huffman_entry::Category::symbol, 199, 9},
        {Huffman_entry::Category::symbol, 265, 7},
        {Huffman_entry::Category::symbol, 99, 8},
        {Huffman_entry::Category::symbol, 35, 8},
        {Huffman_entry::Category::symbol, 167, 9},
        {Huffman_entry::Category::symbol, 3, 8},
        {Huffman_entry::Category::symbol, 131, 8},
        {Huffman_entry::Category::symbol, 67, 8},
        {Huffman_entry::Category::symbol, 231, 9},
        {Huffman_entry::Category::symbol, 261, 7},
        {Huffman_entry::Category::symbol, 91, 8},
        {Huffman_entry::Category::symbol, 27, 8},
        {Huffman_entry::Category::symbol, 151, 9},
        {Huffman_entry::Category::symbol, 277, 7},
        {Huffman_entry::Category::symbol, 123, 8},
        {Huffman_entry::Category::symbol, 59, 8},
        {Huffman_entry::Category::symbol, 215, 9},
        {Huffman_entry::Category::symbol, 269, 7},
        {Huffman_entry::Category::symbol, 107, 8},
        {Huffman_entry::Category::symbol, 43, 8},
        {Huffman_entry::Category::symbol, 183, 9},
        {Huffman_entry::Category::symbol, 11, 8},
        {Huffman_entry::Category::symbol, 139, 8},
        {Huffman_entry::Category::symbol, 75, 8},
        {Huffman_entry::Category::symbol, 247, 9},
        {Huffman_entry::Category::symbol, 259, 7},
        {Huffman_entry::Category::symbol, 87, 8},
        {Huffman_entry::Category::symbol, 23, 8},
        {Huffman_entry::Category::symbol, 287, 8},
        {Huffman_entry::Category::symbol, 275, 7},
        {Huffman_entry::Category::symbol, 119, 8},
        {Huffman_entry::Category::symbol, 55, 8},
        {Huffman_entry::Category::symbol, 207, 9},
        {Huffman_entry::Category::symbol, 267, 7},
        {Huffman_entry::Category::symbol, 103, 8},
        {Huffman_entry::Category::symbol, 39, 8},
        {Huffman_entry::Category::symbol, 175, 9},
        {Huffman_entry::Category::symbol, 7, 8},
        {Huffman_entry::Category::symbol, 135, 8},
        {Huffman_entry::Category::symbol, 71, 8},
        {Huffman_entry::Category::symbol, 239, 9},
        {Huffman_entry::Category::symbol, 263, 7},
        {Huffman_entry::Category::symbol, 95, 8},
        {Huffman_entry::Category::symbol, 31, 8},
        {Huffman_entry::Category::symbol, 159, 9},
        {Huffman_entry::Category::symbol, 279, 7},
        {Huffman_entry::Category::symbol, 127, 8},
        {Huffman_entry::Category::symbol, 63, 8},
        {Huffman_entry::Category::symbol, 223, 9},
        {Huffman_entry::Category::symbol, 271, 7},
        {Huffman_entry::Category::symbol, 111, 8},
        {Huffman_entry::Category::symbol, 47, 8},
        {Huffman_entry::Category::symbol, 191, 9},
        {Huffman_entry::Category::symbol, 15, 8},
        {Huffman_entry::Category::symbol, 143, 8},
        {Huffman_entry::Category::symbol, 79, 8},
        {Huffman_entry::Category::symbol, 255, 9},
        {Huffman_entry::Category::symbol, 0, 5},
        {Huffman_entry::Category::symbol, 16, 5},
        {Huffman_entry::Category::symbol, 8, 5},
        {Huffman_entry::Category::symbol, 24, 5},
        {Huffman_entry::Category::symbol, 4, 5},
        {Huffman_entry::Category::symbol, 20, 5},
        {Huffman_entry::Category::symbol, 12, 5},
        {Huffman_entry::Category::symbol, 28, 5},
        {Huffman_entry::Category::symbol, 2, 5},
        {Huffman_entry::Category::symbol, 18, 5},
        {Huffman_entry::Category::symbol, 10, 5},
        {Huffman_entry::Category::symbol, 26, 5},
        {Huffman_entry::Category::symbol, 6, 5},
        {Huffman_entry::Category::symbol, 22, 5},
        {Huffman_entry::Category::symbol, 14, 5},
        {Huffman_entry::Category::none, 0, 0},
        {Huffman_entry::Category::symbol, 1, 5},
        {Huffman_entry::Category::symbol, 17, 5},
        {Huffman_entry::Category::symbol, 9, 5},
        {Huffman_entry::Category::symbol, 25, 5},
        {Huffman_entry::Category::symbol, 5, 5},
        {Huffman_entry::Category::symbol, 21, 5},
        {Huffman_entry::Category::symbol, 13, 5},
        {Huffman_entry::Category::symbol, 29, 5},
        {Huffman_entry::Category::symbol, 3, 5},
        {Huffman_entry::Category::symbol, 19, 5},
        {Huffman_entry::Category::symbol, 11, 5},
        {Huffman_entry::Category::symbol, 27, 5},
        {Huffman_entry::Category::symbol, 7, 5},
        {Huffman_entry::Category::symbol, 23, 5},
        {Huffman_entry::Category::symbol, 15, 5},
        {Huffman_entry::Category::none, 0, 0}
    };

    Decoding_table literal_length_alphabet;
    literal_length_alphabet.entries = std::span<Huffman_entry> {mother_buffer.begin(), 512};
    literal_length_alphabet.first_area_bitwidth = 9;

    Decoding_table distance_alphabet;
    distance_alphabet.entries = std::span<Huffman_entry> {mother_buffer.begin() + 512, 32};
    distance_alphabet.first_area_bitwidth = 5;

    process_symbols(inflated_data, literal_length_alphabet, distance_alphabet, bitstream);
}

void rez::impl::deflate::decompress_dynamic(std::vector<std::uint8_t>& inflated_data, std::vector<Huffman_entry>& mother_buffer, Deflate_bitstream& bitstream)
{
    const int hlit {static_cast<int>(bitstream.read_bits(5) + 257)};
    const int hdist {static_cast<int>(bitstream.read_bits(5) + 1)};
    const int hclen {static_cast<int>(bitstream.read_bits(4) + 4)};
    if(hlit > 286 or hdist > 30) throw Exception {Error::bad_formed_data};

    /* the order of slots with which to place the bit-lengths of the
    * codes of the code bit-length alphabet */
    static constexpr std::array<int, 19> ordered_indexes {
        16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
    };

    std::array<int, 19> code_length_alphabet_code_lengths {};
    for(int i = 0; i < hclen; ++i) {
        code_length_alphabet_code_lengths[ordered_indexes[i]] = bitstream.read_bits(3);
    }

    /* 128 is the maximum number of entries for the decoding table of
    * the code-length alphabet when using 7 as the bit-width */
    Decoding_table code_length_alphabet;
    code_length_alphabet.entries = std::span<Huffman_entry>(mother_buffer.begin(), 128);
    code_length_alphabet.first_area_bitwidth = fill_decoding_table_from_code_lengths(code_length_alphabet_code_lengths, code_length_alphabet.entries, 7);

    // code-lengths of both the literal+length alphabet and the distance alphabet
    std::vector<int> alphabets_code_lengths;
    const int hlit_hdist {hlit + hdist};
    alphabets_code_lengths.reserve(hlit_hdist);
    // cannot be like this: for(int i = 0; i < hlit_hdist; ++i)
    while(alphabets_code_lengths.size() < hlit_hdist) {
        const int symbol {fetch_symbol(code_length_alphabet, bitstream)};
        if(symbol < 16) { alphabets_code_lengths.push_back(symbol); }
        else if(symbol == 16) {
            if(alphabets_code_lengths.empty()) throw Exception {Error::bad_formed_data};
            const int value_to_copy {alphabets_code_lengths.back()};
            const int times_to_copy {static_cast<int>(bitstream.read_bits(2) + 3)};
            alphabets_code_lengths.insert(alphabets_code_lengths.end(), times_to_copy, value_to_copy);
        }
        else if(symbol == 17) {
            const int times_to_copy {static_cast<int>(bitstream.read_bits(3) + 3)};
            alphabets_code_lengths.insert(alphabets_code_lengths.end(), times_to_copy, 0);
        }
        else if(symbol == 18) {
            const int times_to_copy {static_cast<int>(bitstream.read_bits(7) + 11)};
            alphabets_code_lengths.insert(alphabets_code_lengths.end(), times_to_copy, 0);
        }
        else { throw Exception {Error::bad_formed_data}; }
    }
    if(alphabets_code_lengths.size() != hlit_hdist) throw Exception {Error::bad_formed_data};

    std::span<const int> literal_length_alphabet_code_lengths(alphabets_code_lengths.begin(), hlit);
    /* the bit-width of the first area is 9 because that is the value
    * used by zlib for the literal+length alphabet. I could use the
    * same value that zlib-ng uses, but I decided to be more
    * conservative with memory. */
    Decoding_table literal_length_alphabet;
    literal_length_alphabet.entries = std::span<Huffman_entry>(mother_buffer.begin(), 852);
    literal_length_alphabet.first_area_bitwidth = fill_decoding_table_from_code_lengths(literal_length_alphabet_code_lengths, literal_length_alphabet.entries, 9);
    /* this is so silly: the case in where the amount of code-lengths
    * for the distance alphabet is 1 and that lonely bit-length happens
    * to be zero is valid, it means that the data to decompress is all
    * literals and there aren't length or distance codes. It's silly
    * because the "no compression" block (BTYPE == 0) already exists,
    * this was completely unnecessary */
    std::span<const int> distance_alphabet_code_lengths(alphabets_code_lengths.begin() + hlit, hdist);
    Decoding_table distance_alphabet;
    if(distance_alphabet_code_lengths.size() == 1 and distance_alphabet_code_lengths[0] == 0) { /* do nothing */ }
    /* the bit-width of the first area is 6 because that is the value
    * used by zlib for the distance alphabet. I could use the same
    * value that zlib-ng uses, but I decided to be more conservative
    * with memory */
    else {
        distance_alphabet.entries = std::span<Huffman_entry>(mother_buffer.begin() + 852, 592);
        distance_alphabet.first_area_bitwidth = fill_decoding_table_from_code_lengths(distance_alphabet_code_lengths, distance_alphabet.entries, 6);
    }

    if(distance_alphabet.entries.empty()) {
        int symbol {fetch_symbol(literal_length_alphabet, bitstream)};
        while(symbol != 256) {
            if(symbol > 256) throw Exception {Error::bad_formed_data};
            inflated_data.push_back(static_cast<std::uint8_t>(symbol));
            symbol = fetch_symbol(literal_length_alphabet, bitstream);
        }    
    }
    else { process_symbols(inflated_data, literal_length_alphabet, distance_alphabet, bitstream); }
}

int rez::impl::deflate::fill_decoding_table_from_code_lengths(std::span<const int> code_lengths, std::span<Huffman_entry> decoding_table, int first_area_bitwidth)
{
    /* The purpose of this function is to construct a decoding table
    * that can be indexed with raw bits from the DEFLATE stream and
    * return an entry that contains either a symbol or information
    * used to find another entry that will contain a symbol */

    // bl_count[7 (for example)] == number of codes that have 7 bits
    std::array<int, 16> bl_count {}; // [0,15] bits

    /* for this use, symbol_count can also be named "code_count" but
    * the name symbol_count is more general as the code uses the
    * variable multiple times */
    const int symbol_count {static_cast<int>(code_lengths.size())};
    for(int symbol = 0; symbol < symbol_count; ++symbol) {
        bl_count[code_lengths[symbol]] += 1;
    }

    // get the minimum and maximum bit-lengths for clamping
    int min {1};
    while(min < 16) {
        if(bl_count[min] != 0) break;
        ++min;
    }
    int max {15};
    while(max > 0) {
        if(bl_count[max] != 0) break;
        --max;
    }
    first_area_bitwidth = std::clamp(first_area_bitwidth, min, max);

    /* compute an offset for the first symbol of each code-length.
    * the offsets will be used to index into a symbol table for the
    * purpose of initialising it in a sorted manner. At the end,
    * a symbol that pertains to a code that has a bit-length of 1,
    * will have a smaller offset than a symbol that pertains to a
    * code that has a bit-length of 2 and so on */
    std::array<int, 16> offsets {};
    for(int code_length = min; code_length < max; ++code_length) {
        offsets[code_length + 1] = offsets[code_length] + bl_count[code_length];
    }

    /* initialise the symbols table. Thanks to the 'offsets' array, at
    * the end of the initialision, the 'symbols' table is ordered like
    * this:
    * [symbols that pertain to codes that have bit-length 1] and then
    * [symbols that pertain to codes that have bit-length 2] and then
    * [symbols that pertain to codes that have bit-length 3] and so on,
    * and within each set of symbols, the symbols are ordered in
    * sequential order, like this: 0, 1, 2, 3 and so on */
    std::vector<int> symbols;
    symbols.resize(symbol_count - bl_count[0]);
    for(int symbol = 0; symbol < symbol_count; ++symbol) {
        const int code_length {code_lengths[symbol]};
        if(code_length == 0) continue;
        symbols[offsets[code_length]] = symbol;
        offsets[code_length] += 1;
    }

    // now comes the loop that fills the decoding table with entries
    int current_area_bitwidth {first_area_bitwidth};
    int code {0}; // the smallest valid Huffman code (bit-reversed)
    unsigned int icode {0}; // will help in computing the next bit-reversed Huffman code. i stands for "intermediate"
    int current_code_bit_length = min;
    int area_offset {0};
    int bits_consumed_by_previous_area {0};
    /* impossible initial value on purpose to go to the next area of
    * the decoding table when the bit-length of the current code
    * becomes greater than the bit-width of the first area */
    int current_prefix {0x7FFF}; // impossible initial value on purpose
    const int prefix_mask {(1 << first_area_bitwidth) - 1};

    const int actual_symbol_count {static_cast<int>(symbols.size())};
    for(int i = 0; i < actual_symbol_count; ++i) {
        // create table entry
        Huffman_entry entry;
        entry.category = Huffman_entry::Category::symbol;
        entry.value = symbols[i];
        entry.bits_to_consume = current_code_bit_length - bits_consumed_by_previous_area;

        /* the following code computes all the decoding table indices
        * that can correspond to the current Huffman code (reversed
        * because that is how it comes in the DEFLATE stream). The
        * core of the idea is bit-patterns: imagine you need to read
        * a Huffman code that has a bit-length of 2, but that you
        * index the decoding table with 6 bits, so you grab 6 bits
        * from the DEFLATE stream; in that scenario, you have this
        * layout: 0000'00. The right part are the bits that pertain
        * to the Huffman code and the left part are bits that do not.
        * The bit-pattern of the left part could be whatever and due
        * to that, the same symbol must be assigned to all of those
        * bit-patterns.
        *
        * the code traverses the bit-patterns in reverse order, like
        * this:
        * decoding_table[1111'00]
        * decoding_table[1110'00]
        * decoding_table[1101'00]
        * until
        * decoding_table[0000'00]
        *
        * the code leverages the following dynamic: if you want to
        * have a certain number of high bits set to 1, and a certain
        * number of low bits set to 0, then you can do this:
        *
        * int a = 1 << 8; // 0b1'0000'0000
        * int b = 1 << 4; // 0b0'0001'0000
        * int c = a - b; //..0b0'1111'0000
        *
        * the previous numbers mean "I want 8 bits, but the first 4
        * low bits must be zeros". And if you keep subtracting b from
        * c, then you will traverse the following bit-patterns:
        *
        * c -= b; // 0b1110'0000
        * c -= b; // 0b1101'0000
        * c -= b; // 0b1100'0000
        * and so on */

        // the bits of the "right part" in the previous explanation
        const int code_bits {code >> bits_consumed_by_previous_area};
        int a {1 << current_area_bitwidth};
        const int b {1 << (current_code_bit_length - bits_consumed_by_previous_area)};
        do {
            a -= b;
            decoding_table[area_offset + (a + code_bits)] = entry;
        } while(a != 0);

        /* this code increases the Huffman code to the next valid
        * value (bit-reversed). Thanks to the way in which the
        * 'symbols' vector is sorted, getting the Huffman code for
        * the next symbol just means increasing the Huffman code to
        * the next valid value.
        *
        * the code leverages the way in which binary addition works
        * by doing the increment in the most significant bits and
        * then simply reversing the bits. Imagine we are in the code
        * 1 and the current code-length is 4:
        * 
        * 0001'0000'0000'0000 // icode
        * 0001'0000'0000'0000 // addend
        * 0010'0000'0000'0000 // result
        * 0000'0000'0000'0100 // bit-reversed (code 2, length 4) */
        icode += 1u << (16 - current_code_bit_length);
        code = static_cast<int>(reverse_bits(static_cast<std::uint16_t>(icode)));

        // do some book-keeping
        bl_count[current_code_bit_length] -= 1; // one code less to take care off
        if(bl_count[current_code_bit_length] == 0) {
            // go to the next bit-length
            if(current_code_bit_length == max) break;
            /* we cannot just write ++current_code_bit_length because
            * there can be bl_counts that are equal to zero between
            * bl_counts that aren't */
            current_code_bit_length = code_lengths[symbols[i + 1]];
        }

        /* go to the next area of the decoding table if needed. It is
        * needed every time the prefix changes and we cannot stay in
        * the first area */
        if(current_code_bit_length > first_area_bitwidth and (code & prefix_mask) != current_prefix) {
            // do some book-keeping
            current_prefix = code & prefix_mask;
            area_offset += 1 << current_area_bitwidth; // go to the next area
            // despite the variable name, it's always the bit-width of the first area
            bits_consumed_by_previous_area = first_area_bitwidth;

            // determine the bit-width of the next area
            current_area_bitwidth = current_code_bit_length - bits_consumed_by_previous_area;
            int free_nodes {1 << current_area_bitwidth}; // simulate a Huffman tree
            int bit_length {current_code_bit_length};
            while(bit_length < max) {
                free_nodes -= bl_count[bit_length];
                if(free_nodes <= 0) break;
                ++bit_length;
                ++current_area_bitwidth;
                free_nodes <<= 1;
            }

            // adjust the entry
            entry.category = Huffman_entry::Category::bridge;
            entry.value = area_offset;
            entry.bits_to_consume = current_area_bitwidth;
            decoding_table[current_prefix] = entry;
        }
    }
    return first_area_bitwidth;
}

int rez::impl::deflate::fetch_symbol(const Decoding_table& decoding_table, Deflate_bitstream& bitstream)
{
    Huffman_entry entry;
    if(bitstream.bits_remaining() < decoding_table.first_area_bitwidth) {
        entry = decoding_table.entries[bitstream.peek_bits(bitstream.bits_remaining())];
    }
    else {
        entry = decoding_table.entries[bitstream.peek_bits(decoding_table.first_area_bitwidth)];
    }

    switch(entry.category) {
        case Huffman_entry::Category::symbol:
            bitstream.skip_bits(entry.bits_to_consume);
            return entry.value;
        case Huffman_entry::Category::bridge: {
            bitstream.skip_bits(decoding_table.first_area_bitwidth);
            Huffman_entry entry2 {decoding_table.entries[entry.value + bitstream.peek_bits(entry.bits_to_consume)]};
            bitstream.skip_bits(entry2.bits_to_consume);
            return entry2.value;
        }
        default:
            throw Exception {Error::bad_formed_data};
    }
}

void rez::impl::deflate::process_symbols(std::vector<std::uint8_t>& inflated_data, const Decoding_table& literal_length_alphabet, const Decoding_table& distance_alphabet, Deflate_bitstream& bitstream)
{
    int symbol {fetch_symbol(literal_length_alphabet, bitstream)};
    while(symbol != 256) {
        if(symbol < 256) { inflated_data.push_back(static_cast<std::uint8_t>(symbol)); }
        else {
            if(symbol > 285) throw Exception {Error::bad_formed_data};
            symbol -= 257;
            const int length {length_bases[symbol] + static_cast<int>(bitstream.read_bits(length_extra_bits[symbol]))};

            symbol = fetch_symbol(distance_alphabet, bitstream);
            if(symbol > 29) throw Exception {Error::bad_formed_data};
            const std::int32_t distance {distance_bases[symbol] + bitstream.read_bits(distance_extra_bits[symbol])};
            if(distance > inflated_data.size()) throw Exception {Error::bad_formed_data};

            lz77_copy(inflated_data, length, distance);
        }

        symbol = fetch_symbol(literal_length_alphabet, bitstream);
    }
}

void rez::impl::deflate::lz77_copy(std::vector<std::uint8_t>& inflated_data, int length, const std::int32_t distance)
{
    std::int64_t destination_index {static_cast<std::int64_t>(inflated_data.size())};
    const std::int64_t copy_start_index {destination_index - distance};
    inflated_data.resize(destination_index + length);

    if(distance == 1) { std::memset(&inflated_data[destination_index], inflated_data[destination_index - 1], length); }
    else if(length > distance) { // cyclic copy
        while(length > distance) {
            std::memcpy(&inflated_data[destination_index], &inflated_data[copy_start_index], distance);
            destination_index += distance;
            length -= distance;
        }
        std::memcpy(&inflated_data[destination_index], &inflated_data[copy_start_index], length);
    }
    else { // plain copy
        std::memcpy(&inflated_data[destination_index], &inflated_data[copy_start_index], length);
    }    
}
