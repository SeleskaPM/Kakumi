#include "shared.hpp"

#include <filesystem>

/*
Currently unsupported files (BMP Suite version 2.8):
All RLE
q/pal1huffmsb.bmp

q/pal2.bmp
q/pal2color.bmp
q/pal8offs.bmp

q/rgba32abf.bmp
q/rgba64.bmp

q/rgb24jpeg.bmp
q/rgb24png.bmp

q/rgb24prof2.bmp (colors are not displayed correctly)

x/ba-bm.bmp
*/

namespace kak::bmp {
    
Image decode_image(const std::filesystem::path& filepath);

} // namespace kak::bmp

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

enum class Compression {
    rgb = 0,
    bitfields = 3,
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
    Compression compression;
    int32_t palette_entries; // total number of palette entries
    
    uint32_t red_mask;
    uint32_t green_mask;
    uint32_t blue_mask;
    uint32_t alpha_mask;

    // helper data-members
    int32_t stride; // size in bytes of a scanline including padding
    int32_t stride_no_padding; // size in bytes of a scanline without padding
    int32_t padding; // the amount of padding, in bytes, in a scanline
    bool bottom_top;
};

// maybe?: uint8_t range_map_4bits_to_8bits[16];

int32_t compute_stride(int32_t image_width, int32_t bits_per_pixel);
// vertical flip
void flip_image_data(std::vector<uint8_t>& image_data, int32_t image_height, int32_t stride);

std::vector<uint8_t> read_color_table(std::ifstream& bmp_file, int bits_per_pixel, int bytes_payload);
std::vector<uint8_t> depalettize_image_data(std::ifstream& bmp_file, std::span<const uint8_t> color_table, int color_table_entry_size, const Dib_general& dib_header);

std::vector<uint8_t> read_rgb_16bpp_image_data(std::ifstream& bmp_file, const Dib_general& dib_header);
std::vector<uint8_t> read_bitfield_16bpp_image_data(std::ifstream& bmp_file, const Dib_general& dib_header);
std::vector<uint8_t> read_24bpp_image_data(std::ifstream& bmp_file, const Dib_general& dib_header);
std::vector<uint8_t> read_rgb_32bpp_image_data(std::ifstream& bmp_file, const Dib_general& dib_header);
std::vector<uint8_t> read_bitfield_32bpp_image_data(std::ifstream& bmp_file, const Dib_general& dib_header);

Dib_type read_bmp_header(std::ifstream& ifs, Bmp_header& header);
// DIB versions...
void read_bitmapcoreheader(std::ifstream& ifs, Dib_general& header);
void read_bitmapinfoheader(std::ifstream& ifs, Dib_general& header);
//void read_bitmapv2infoheader(std::ifstream& ifs, Dib_general& header);
//void read_bitmapv3infoheader(std::ifstream& ifs, Dib_general& header);
void read_os22xbitmapheader(std::ifstream& ifs, Dib_general& header, int os22xheader_size);
//void read_bitmapv4header(std::ifstream& ifs, Dib_general& header);
//void read_bitmapv5header(std::ifstream& ifs, Dib_general& header);

Image read_bitmapcoreheader_file(std::ifstream& ifs, Bmp_header& bmp_header);
Image read_bitmapinfoheader_file(std::ifstream& ifs, Bmp_header& bmp_header, bool masks_in_header, int ignore_amount);
Image read_os22xbitmapheader_file(std::ifstream& ifs, Bmp_header& bmp_header, int os22xheader_size);

} // namespace kak::impl::bmp

/*

struct BITMAPV2INFOHEADER (52 bytes) {
    uint32_t Size; (already read)
    int32_t Width;
    int32_t Height;
    uint16_t Planes; // value is always 1
    uint16_t BitCount; (bits per pixel)
    uint32_t Compression;
    uint32_t SizeImage;
    int32_t XPelsPerMeter;
    int32_t YPelsPerMeter;
    uint32_t ClrUsed;
    uint32_t ClrImportant;
    uint32_t RedMask;
    uint32_t GreenMask;
    uint32_t BlueMask;
};

struct BITMAPV3INFOHEADER (56 bytes) {
    uint32_t Size; (already read)
    int32_t Width;
    int32_t Height;
    uint16_t Planes; // value is always 1
    uint16_t BitCount; (bits per pixel)
    uint32_t Compression;
    uint32_t SizeImage;
    int32_t XPelsPerMeter;
    int32_t YPelsPerMeter;
    uint32_t ClrUsed;
    uint32_t ClrImportant;
    uint32_t RedMask;
    uint32_t GreenMask;
    uint32_t BlueMask;
    uint32_t AlphaMask;
};

struct BITMAPV4HEADER (108 bytes) {
    uint32_t Size; (already read)
    int32_t Width;
    int32_t Height;
    uint16_t Planes; // value is always 1
    uint16_t BitCount; (bits per pixel)
    uint32_t V4Compression;
    uint32_t SizeImage;
    int32_t XPelsPerMeter;
    int32_t YPelsPerMeter;
    uint32_t ClrUsed;
    uint32_t ClrImportant;
    uint32_t RedMask;
    uint32_t GreenMask;
    uint32_t BlueMask;
    uint32_t AlphaMask;
    uint32_t CSType;
    int32_t RedEndpointX;
    int32_t RedEndpointY;
    int32_t RedEndpointZ;
    int32_t GreenEndpointX;
    int32_t GreenEndpointY;
    int32_t GreenEndpointZ;
    int32_t BlueEndpointX;
    int32_t BlueEndpointY;
    int32_t BlueEndpointZ;
    uint32_t GammaRed;
    uint32_t GammaGreen;
    uint32_t GammaBlue;
};

struct BITMAPV5HEADER (124 bytes) {
    uint32_t Size; (already read)
    int32_t Width;
    int32_t Height;
    uint16_t Planes; // value is always 1
    uint16_t BitCount; (bits per pixel)
    uint32_t V4Compression;
    uint32_t SizeImage;
    int32_t XPelsPerMeter;
    int32_t YPelsPerMeter;
    uint32_t ClrUsed;
    uint32_t ClrImportant;
    uint32_t RedMask;
    uint32_t GreenMask;
    uint32_t BlueMask;
    uint32_t AlphaMask;
    uint32_t CSType;
    int32_t RedEndpointX;
    int32_t RedEndpointY;
    int32_t RedEndpointZ;
    int32_t GreenEndpointX;
    int32_t GreenEndpointY;
    int32_t GreenEndpointZ;
    int32_t BlueEndpointX;
    int32_t BlueEndpointY;
    int32_t BlueEndpointZ;
    uint32_t GammaRed;
    uint32_t GammaGreen;
    uint32_t GammaBlue;
    uint32_t Intent;
    uint32_t ProfileData;
    uint32_t ProfileSize;
    uint32_t Reserved;
};

*/
