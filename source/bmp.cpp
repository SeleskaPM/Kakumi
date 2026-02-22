#include "bmp.hpp"

#include <cmath>
#include <utility>
#include <cstring>

std::int32_t kak::impl::bmp::compute_row_size(int32_t image_width, int32_t bits_per_pixel)
{
    // from wingdi.h
    return ((image_width * bits_per_pixel + 31) & ~31) >> 3; // size in bytes
}

void kak::impl::bmp::flip_image_data(std::vector<uint8_t>& image_data, int32_t image_height, int32_t stride)
{
    std::vector<uint8_t> row_backup(stride);
    uint8_t* backup = row_backup.data();

    uint8_t* dest = image_data.data();
    uint8_t* source = dest + ((image_height - 1) * stride);

    for(int32_t row = 0; row < image_height / 2; ++row) {
        std::memcpy(backup, dest, stride);
        std::memcpy(dest, source, stride);
        std::memcpy(source, backup, stride);

        dest += stride;
        source -= stride;
    }
}

kak::Image kak::bmp::decode_image(const std::filesystem::path& filepath)
{
    using namespace kak::impl;
    using namespace kak::impl::bmp;

    std::ifstream bmp_file {filepath, std::ios_base::binary};
    if(not bmp_file.good()) throw Exception {Error::open_file_failed};

    Bmp_header bmp_header;
    read_bmp_header(bmp_file, bmp_header);

    /* read the DIB header */
    Dib_general dib_header;
    const uint32_t dib_header_size {get_little_endian_value<uint32_t>(bmp_file)};
    switch(static_cast<Dib_type>(dib_header_size)) {
        case Dib_type::bitmapcoreheader:
            return read_bitmapcoreheader_file(bmp_file, bmp_header, dib_header);
        case Dib_type::bitmapinfoheader:
            throw Exception {Error::unsupported_feature};
        case Dib_type::bitmapv2infoheader:
            throw Exception {Error::unsupported_feature};
        case Dib_type::bitmapv3infoheader:
            throw Exception {Error::unsupported_feature};
        case Dib_type::os22xbitmapheader:
            throw Exception {Error::unsupported_feature};
        case Dib_type::os22xbitmapheader_16:
            throw Exception {Error::unsupported_feature};
        case Dib_type::bitmapv4header:
            throw Exception {Error::unsupported_feature};
        case Dib_type::bitmapv5header:
            throw Exception {Error::unsupported_feature};
        default: throw Exception {Error::bad_formed_file};
    }

    return Image();
}

void kak::impl::bmp::read_bmp_header(std::ifstream& ifs, Bmp_header& header)
{
    /*
    struct BITMAPFILEHEADER (14 bytes) {
        uint16_t Type;
        uint32_t Size; (size of the BMP file in bytes)
        uint16_t Reserved1;
        uint16_t Reserved2;
        uint32_t OffBits; (offset, in bytes, from the beginning of the file to the first byte of the bitmap pixel array)
    };
    */

    // read the type (0x424D == "BM")
    if(get_big_endian_value<uint16_t>(ifs) != 0x424Du) {
        throw Exception {Error::bad_formed_file};
    }

    header.file_size = get_little_endian_value<uint32_t>(ifs);
    ifs.ignore(4); // ignore fields Reserved1 and Reserved2
    if(not ifs.good()) throw Exception {Error::bad_formed_file};
    header.bitmap_offset = get_little_endian_value<uint32_t>(ifs);
}

void kak::impl::bmp::read_bitmapcoreheader(std::ifstream& ifs, Dib_general& header)
{
    /*
    struct BITMAPCOREHEADER (12 bytes) {
        uint32_t Size; (already read)
        uint16_t Width;
        uint16_t Height;
        uint16_t Planes; // value is always 1
        uint16_t BitCount; (bits per pixel)
    };
    */

    /* the Width and Height fields are uint16_t in Microsoft's
    * documentation, but they are int16_t in the Encyclopedia of
    * Graphics File Formats, Second Edition (page 576). The
    * encyclopedia informs that if the value of Height is positive
    * then the BMP image is a bottom-top image and if the value is
    * negative then it's a top-down image. Microsoft's documentation
    * informs the same for BITMAPINFOHEADER, BITMAPV4HEADER and
    * BITMAPV5HEADER but not for BITMAPCOREHEADER. I am going to go
    * with what the encyclopedia informs */
    header.width = get_little_endian_value<int16_t>(ifs);
    header.height = get_little_endian_value<int16_t>(ifs);
    if(header.width < 1 || header.height == 0) throw Exception {Error::bad_formed_file};

    header.bottom_top = header.height > 0;
    header.height = std::abs(header.height);
    check_against_max_dimension(header.width, header.height);

    ifs.ignore(2); // ignore Planes
    header.bits_per_pixel = get_little_endian_value<uint16_t>(ifs);
    switch(header.bits_per_pixel) {
        case 1:
        case 4:
        case 8:
        case 24:
            break;
        default: throw Exception {Error::bad_formed_file};
    }

    // initialize the helper data-members
    const int32_t stride {compute_row_size(header.width, header.bits_per_pixel)};
    header.raw_stride_no_padding = div8_ceil(header.width * header.bits_per_pixel);
    header.padding = stride - header.raw_stride_no_padding;
}

void kak::impl::bmp::read_bitmapinfoheader(std::ifstream& ifs, Dib_general& header)
{
    /*
    struct BITMAPINFOHEADER (40 bytes) {
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
    };
    */
}

void kak::impl::bmp::read_bitmapv2infoheader(std::ifstream& ifs, Dib_general& header)
{
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
    */
}

void kak::impl::bmp::read_bitmapv3infoheader(std::ifstream& ifs, Dib_general& header)
{
    /*
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
    */
}

void kak::impl::bmp::read_os22xbitmapheader(std::ifstream& ifs, Dib_general& header)
{
    /*
    struct OS22XBITMAPHEADER (64 bytes) {
        uint32_t Size; // Size of this structure in bytes
        uint32_t Width; // Bitmap width in pixels
        uint32_t Height; // Bitmap height in pixels
        uint16_t Planes; // value is always 1
        uint16_t BitsPerPixel; // Number of bits per pixel per plane
        uint32_t Compression; // Bitmap compression scheme
        uint32_t ImageDataSize; // Size of bitmap data in bytes
        uint32_t XResolution; // X resolution of display device
        uint32_t YResolution; // Y resolution of display device
        uint32_t ColorsUsed; // Number of color table indices used
        uint32_t ColorsImportant; // Number of important color indices
        uint16_t Units; // Type of units used to measure resolution
        uint16_t Reserved; // Pad structure to 4-byte boundary
        uint16_t Recording; // Recording algorithm
        uint16_t Rendering; // Halftoning algorithm used
        uint32_t Size1; // Reserved for halftoning algorithm use
        uint32_t Size2; // Reserved for halftoning algorithm use
        uint32_t ColorEncoding; // Color model used in bitmap
        uint32_t Identifier; // Reserved for application use
    };
    */
}

void kak::impl::bmp::read_os22xbitmapheader_16(std::ifstream& ifs, Dib_general& header)
{
    /*
    struct OS22XBITMAPHEADER (16 bytes) {
        uint32_t Size; // Size of this structure in bytes
        uint32_t Width; // Bitmap width in pixels
        uint32_t Height; // Bitmap height in pixels
        uint16_t Planes; // value is always 1
        uint16_t BitsPerPixel; // Number of bits per pixel per plane
    };
    */
}

void kak::impl::bmp::read_bitmapv4header(std::ifstream& ifs, Dib_general& header)
{
    /*
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
    */
}

void kak::impl::bmp::read_bitmapv5header(std::ifstream& ifs, Dib_general& header)
{
    /*
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
}

kak::Image kak::impl::bmp::read_bitmapcoreheader_file(std::ifstream& ifs, Bmp_header& bmp_header, Dib_general& dib_header)
{
    read_bitmapcoreheader(ifs, dib_header);

    /* calculate the size of the palette (if there is none, the
    * result will be zero). It's done this way because there could be
    * a palette even if the number of bits per pixel is 24. Well...
    * technically, not really, Microsoft's documentation states
    * that there could be a palette even if the number of bits per
    * pixel is more than 8 and that you must check the field ClrUsed
    * to know if there is a palette, but BITMAPCOREHEADER doesn't
    * have that member, therefore you can infer that the scenario of
    * a 24bpp BITMAPCOREHEADER file having a palette is not legal,
    * but... just in case... */
    // 26 == BITMAPFILEHEADER + BITMAPCOREHEADER
    const uint32_t palette_entries_count {(bmp_header.bitmap_offset - 26u) / 3u};
    if(palette_entries_count > 256u) throw Exception {Error::bad_formed_file};
    const int32_t palette_bytesize {static_cast<int32_t>(palette_entries_count) * 3}; // BGR palette

    std::vector<uint8_t> image_data;
    image_data.reserve(dib_header.width * dib_header.height * 3);

    if(dib_header.bits_per_pixel != 24) {
        if(palette_bytesize == 0) throw Exception {Error::bad_formed_file};
        std::vector<uint8_t> palette = get_bytes(ifs, palette_bytesize);
        const int32_t palette_last_valid_index {static_cast<int32_t>(palette.size()) - 1};

        rez::impl::Bitstream<rez::impl::Bitstream_format::jpg> bitstream;
        std::vector<uint8_t> scanline;
        for(int32_t row = 0; row < dib_header.height; ++row) {
            scanline = get_bytes(ifs, dib_header.raw_stride_no_padding);
            ifs.ignore(dib_header.padding);
            if(not ifs.good()) throw Exception {Error::read_file_failed};
            bitstream.plug_source(scanline);
            for(int32_t pixel = 0; pixel < dib_header.width; ++pixel) {
                // size of an index == bits_per_pixel
                int32_t index {bitstream.read_bits(dib_header.bits_per_pixel)};
                index *= 3;
                if(index + 2 > palette_last_valid_index) throw Exception {Error::bad_formed_file};

                image_data.push_back(palette[index]); // blue
                image_data.push_back(palette[index + 1]); // green
                image_data.push_back(palette[index + 2]); // red
            }
        }
    }
    else {
        if(palette_bytesize > 0) {
            // ignore the palette
            ifs.ignore(palette_bytesize);
            if(not ifs.good()) throw Exception {Error::read_file_failed};
        }

        // read the image data
        std::vector<uint8_t> scanline;
        for(int32_t row = 0; row < dib_header.height; ++row) {
            scanline = get_bytes(ifs, dib_header.raw_stride_no_padding);
            image_data.insert(image_data.end(), scanline.begin(), scanline.end());
            ifs.ignore(dib_header.padding);
            if(not ifs.good()) throw Exception {Error::read_file_failed};
        }
    }

    if(dib_header.bottom_top) flip_image_data(image_data, dib_header.height, dib_header.width * 3);
    Image result;
    result.pixel_data = std::move(image_data);
    result.pixel_format = Pixel_format::bgr;
    result.image_width = dib_header.width;
    result.image_height = dib_header.height;

    return result;
}
