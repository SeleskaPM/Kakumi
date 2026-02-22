#include "shared.hpp"

#include <filesystem>

namespace kak::bmp {
    Image decode_image(const std::filesystem::path& filepath);
}

namespace kak::impl::bmp {
    struct Bmp_header {
        uint32_t file_size;
        uint32_t bitmap_offset;
    };

    enum class Dib_type {
        bitmapcoreheader = 12,
        bitmapinfoheader = 40,

        // https://web.archive.org/web/20150127132443/https://forums.adobe.com/message/3272950
        bitmapv2infoheader = 52,
        bitmapv3infoheader = 56,

        // Encyclopedia of Graphics File Formats, Second Edition (page 635)
        // https://www.fileformat.info/format/os2bmp/egff.htm (same as the encyclopedia but in HTML)
        os22xbitmapheader = 64,
        os22xbitmapheader_16 = 16,
        /* a 40 bytes version exists, but it's the same as
        * BITMAPINFOHEADER with the only difference being that
        * the Width and Height fields of BITMAPINFOHEADER are
        * signed */
        // os22xbitmapheader_40

        bitmapv4header = 108,
        bitmapv5header = 124
    };

    /* BMP has two headers: BITMAPFILEHEADER (the beginning of the
    * file) and a DIB header. The DIB header follows BITMAPFILEHEADER
    * but there are 8 different versions of the DIB header; due to
    * that, instead of creating structures for each version, I am
    * going to create a general structure that works for every version */
    struct Dib_general {
        int32_t width;
        int32_t height;
        int32_t bits_per_pixel;

        // helper data-members
        int32_t raw_stride_no_padding; // stride for in-file pixel data
        int32_t padding;
        bool bottom_top;
    };

    int32_t compute_row_size(int32_t image_width, int32_t bits_per_pixel);
    // vertical flip
    void flip_image_data(std::vector<uint8_t>& image_data, int32_t image_height, int32_t stride);

    void read_bmp_header(std::ifstream& ifs, Bmp_header& header);
    // DIB versions...
    void read_bitmapcoreheader(std::ifstream& ifs, Dib_general& dib_header);
    void read_bitmapinfoheader(std::ifstream& ifs, Dib_general& header);
    void read_bitmapv2infoheader(std::ifstream& ifs, Dib_general& header);
    void read_bitmapv3infoheader(std::ifstream& ifs, Dib_general& header);
    void read_os22xbitmapheader(std::ifstream& ifs, Dib_general& header);
    void read_os22xbitmapheader_16(std::ifstream& ifs, Dib_general& header);
    void read_bitmapv4header(std::ifstream& ifs, Dib_general& header);
    void read_bitmapv5header(std::ifstream& ifs, Dib_general& header);

    Image read_bitmapcoreheader_file(std::ifstream& ifs, Bmp_header& bmp_header, Dib_general& dib_header);
    //Image read_bitmapinfoheader_file(std::ifstream& ifs, Bmp_header& bmp_header, Dib_general& dib_header);
}