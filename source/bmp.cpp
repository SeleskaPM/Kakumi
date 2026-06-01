#include "bmp.hpp"

#include <cmath>
#include <utility>
#include <cstring>
#include <type_traits>
#include <limits>

#include "Rezbits/shared.hpp"

namespace kak::bmp {

Image decode_image(const std::filesystem::path& filepath)
{
    using namespace kak::impl;
    using namespace kak::impl::bmp;

    std::ifstream bmp_file(filepath, std::ios_base::binary);
    if (not bmp_file.good()) throw Exception(Error::open_file_failed);

    Bmp_header bmp_header;
    const Dib_type dib_type = read_bmp_header(bmp_file, bmp_header);

    switch (dib_type) {
        case Dib_type::bitmapcoreheader:
            return read_bitmapcoreheader_file(bmp_file, bmp_header);
        case Dib_type::bitmapinfoheader:
            return read_bitmapinfoheader_file(bmp_file, bmp_header, false, 0);
        case Dib_type::bitmapv2infoheader:
            return read_bitmapinfoheader_file(bmp_file, bmp_header, true, 0);
        case Dib_type::bitmapv3infoheader:
            return read_bitmapinfoheader_file(bmp_file, bmp_header, true, 4);
        case Dib_type::os22xbitmapheader:
            return read_os22xbitmapheader_file(bmp_file, bmp_header, 64);
        case Dib_type::os22xbitmapheader_16:
            return read_os22xbitmapheader_file(bmp_file, bmp_header, 16);
        case Dib_type::bitmapv4header:
            return read_bitmapinfoheader_file(bmp_file, bmp_header, true, 56);
        case Dib_type::bitmapv5header:
            return read_bitmapinfoheader_file(bmp_file, bmp_header, true, 72);
        default:
            throw Exception(Error::bad_formed_file);
    }
}

} // namespace kak::bmp

namespace kak::impl::bmp {

int32_t compute_stride(int32_t image_width, int32_t bits_per_pixel)
{
    // from wingdi.h
    return ((image_width * bits_per_pixel + 31) & ~31) >> 3; // size in bytes (including padding)
}

void flip_image_data(std::vector<uint8_t>& image_data, int32_t image_height, int32_t stride)
{
    std::vector<uint8_t> row_backup(stride);
    uint8_t* backup = row_backup.data();

    uint8_t* dest = image_data.data();
    uint8_t* source = dest + ((image_height - 1) * stride);

    for (int32_t row = 0; row < image_height / 2; ++row) {
        std::memcpy(backup, dest, stride);
        std::memcpy(dest, source, stride);
        std::memcpy(source, backup, stride);

        dest += stride;
        source -= stride;
    }
}

std::vector<uint8_t> read_color_table(std::ifstream& bmp_file, int bits_per_pixel, int bytes_payload)
{
    std::vector<uint8_t> color_table;

    if (bytes_payload != 0) {
        if (bits_per_pixel < 16) {
            color_table = read_bytes(bmp_file, bytes_payload);
        }
        else {
            skip_bytes(bmp_file, bytes_payload);
        }
    }

    return color_table;
}

std::vector<uint8_t> depalettize_image_data(std::ifstream& bmp_file, std::span<const uint8_t> color_table, int color_table_entry_size, const Dib_general& dib_header)
{
    std::vector<uint8_t> scanline(dib_header.stride);
    rez::impl::BitReader<rez::impl::BitStreamFormat::jpg> bit_reader(scanline);

    std::vector<uint8_t> image_data;
    // depalettized image data is BGR
    image_data.reserve(dib_header.width * dib_header.height * 3);

    // it's safe. the max possible size of this color table span is 4000 bytes
    const int last_valid_byte_index = static_cast<int>(color_table.size()) - 1;

    for (int32_t row = 0; row < dib_header.height; ++row) {
        read_bytes_into(bmp_file, scanline, dib_header.stride);

        for (int32_t pixel = 0; pixel < dib_header.width; ++pixel) {
            const int32_t index = bit_reader.read_bits(dib_header.bits_per_pixel) * color_table_entry_size;
            if (index + 2 > last_valid_byte_index) throw Exception(Error::bad_formed_file);

            image_data.push_back(color_table[index]); // blue
            image_data.push_back(color_table[index + 1]); // green
            image_data.push_back(color_table[index + 2]); // red
        }

        bit_reader.rewind();
    }

    return image_data;
}

std::vector<uint8_t> read_rgb_16bpp_image_data(std::ifstream& bmp_file, const Dib_general& dib_header)
{
    std::vector<uint8_t> image_data;
    // output image data is BGR
    image_data.reserve(dib_header.width * dib_header.height * 3);

    std::vector<uint8_t> scanline(dib_header.stride);
    rez::impl::ByteReader byte_reader(scanline);

    // for 16bpp, when biCompression == BI_RGB, the format is always X1R5G5B5 (X is ignored)
    const double multiplier = compute_multiplier_to_8bits_range(5);

    uint8_t red = 0;
    uint8_t green = 0;
    uint8_t blue = 0;

    for (int32_t row = 0; row < dib_header.height; ++row) {
        read_bytes_into(bmp_file, scanline, dib_header.stride);

        for (int32_t pixel = 0; pixel < dib_header.width; ++pixel) {
            const uint32_t the_pixel = byte_reader.read_little_endian_value<uint16_t>();

            red = (the_pixel & 0x7C00u) >> 10;
            green = (the_pixel & 0x03E0u) >> 5;
            blue = the_pixel & 0x1Fu;

            image_data.push_back(static_cast<uint8_t>(static_cast<double>(blue) * multiplier));
            image_data.push_back(static_cast<uint8_t>(static_cast<double>(green) * multiplier));
            image_data.push_back(static_cast<uint8_t>(static_cast<double>(red) * multiplier));
        }

        byte_reader.rewind();
    }

    return image_data;
}

std::vector<uint8_t> read_bitfield_16bpp_image_data(std::ifstream& bmp_file, const Dib_general& dib_header)
{
    std::vector<uint8_t> image_data;
    // output image data is 3 bytes per pixel, BGR
    image_data.reserve(dib_header.width * dib_header.height * 3);

    std::vector<uint8_t> scanline(dib_header.stride);
    rez::impl::ByteReader byte_reader(scanline);

    /* use 32 bits because channels of more than 8 bits are possible
    * and uint16_t would require conversions */
    uint32_t red = 0;
    uint32_t green = 0;
    uint32_t blue = 0;

    const int red_bits = std::popcount(dib_header.red_mask);
    const int green_bits = std::popcount(dib_header.green_mask);
    const int blue_bits = std::popcount(dib_header.blue_mask);

    const double red_multiplier = compute_multiplier_to_8bits_range(red_bits);
    const double green_multiplier = compute_multiplier_to_8bits_range(green_bits);
    const double blue_multiplier = compute_multiplier_to_8bits_range(blue_bits);

    const int red_shift = std::countr_zero(dib_header.red_mask);
    const int green_shift = std::countr_zero(dib_header.green_mask);
    const int blue_shift = std::countr_zero(dib_header.blue_mask);

    for (int32_t row = 0; row < dib_header.height; ++row) {
        read_bytes_into(bmp_file, scanline, dib_header.stride);

        for (int32_t pixel = 0; pixel < dib_header.width; ++pixel) {
            const uint32_t the_pixel = byte_reader.read_little_endian_value<uint16_t>();

            red = (the_pixel & dib_header.red_mask) >> red_shift;
            green = (the_pixel & dib_header.green_mask) >> green_shift;
            blue = (the_pixel & dib_header.blue_mask) >> blue_shift;

            image_data.push_back(static_cast<uint8_t>(static_cast<double>(blue) * blue_multiplier));
            image_data.push_back(static_cast<uint8_t>(static_cast<double>(green) * green_multiplier));
            image_data.push_back(static_cast<uint8_t>(static_cast<double>(red) * red_multiplier));
        }

        byte_reader.rewind();
    }

    return image_data;
}

std::vector<uint8_t> read_24bpp_image_data(std::ifstream& bmp_file, const Dib_general& dib_header)
{
    std::vector<uint8_t> image_data;
    // output image data is BGR
    image_data.reserve(dib_header.width * dib_header.height * 3);

    std::vector<uint8_t> scanline(dib_header.stride);
    auto scanline_begin = scanline.begin();
    auto scanline_end = scanline_begin + dib_header.stride_no_padding;

    for (int32_t row = 0; row < dib_header.height; ++row) {
        read_bytes_into(bmp_file, scanline, dib_header.stride);
        image_data.insert(image_data.end(), scanline_begin, scanline_end);
    }

    return image_data;
}

std::vector<uint8_t> read_rgb_32bpp_image_data(std::ifstream& bmp_file, const Dib_general& dib_header)
{
    std::vector<uint8_t> image_data;
    // output image data is BGR
    image_data.reserve(dib_header.width * dib_header.height * 3);

    std::vector<uint8_t> scanline(dib_header.stride);
    rez::impl::ByteReader byte_reader(scanline);

    uint8_t red = 0;
    uint8_t green = 0;
    uint8_t blue = 0;

    for (int32_t row = 0; row < dib_header.height; ++row) {
        read_bytes_into(bmp_file, scanline, dib_header.stride);

        for (int32_t pixel = 0; pixel < dib_header.width; ++pixel) {
            const uint32_t the_pixel = byte_reader.read_little_endian_value<uint32_t>();

            red = (the_pixel & 0xFF0000u) >> 16;
            green = (the_pixel & 0xFF00u) >> 8;
            blue = the_pixel & 0xFFu;

            image_data.push_back(blue);
            image_data.push_back(green);
            image_data.push_back(red);
        }

        byte_reader.rewind();
    }

    return image_data;
}

std::vector<uint8_t> read_bitfield_32bpp_image_data(std::ifstream& bmp_file, const Dib_general& dib_header)
{
    std::vector<uint8_t> image_data;
    // output image data is BGR
    image_data.reserve(dib_header.width * dib_header.height * 3);

    std::vector<uint8_t> scanline(dib_header.stride);
    rez::impl::ByteReader byte_reader(scanline);

    // use 32 bits because channels of more than 16 bits are possible
    uint32_t red = 0;
    uint32_t green = 0;
    uint32_t blue = 0;

    const int red_bits = std::popcount(dib_header.red_mask);
    const int green_bits = std::popcount(dib_header.green_mask);
    const int blue_bits = std::popcount(dib_header.blue_mask);

    const double red_multiplier = compute_multiplier_to_8bits_range(red_bits);
    const double green_multiplier = compute_multiplier_to_8bits_range(green_bits);
    const double blue_multiplier = compute_multiplier_to_8bits_range(blue_bits);

    const int red_shift = std::countr_zero(dib_header.red_mask);
    const int green_shift = std::countr_zero(dib_header.green_mask);
    const int blue_shift = std::countr_zero(dib_header.blue_mask);

    for (int32_t row = 0; row < dib_header.height; ++row) {
        read_bytes_into(bmp_file, scanline, dib_header.stride);

        for (int32_t pixel = 0; pixel < dib_header.width; ++pixel) {
            const uint32_t the_pixel = byte_reader.read_little_endian_value<uint32_t>();

            red = (the_pixel & dib_header.red_mask) >> red_shift;
            green = (the_pixel & dib_header.green_mask) >> green_shift;
            blue = (the_pixel & dib_header.blue_mask) >> blue_shift;

            image_data.push_back(static_cast<uint8_t>(static_cast<double>(blue) * blue_multiplier));
            image_data.push_back(static_cast<uint8_t>(static_cast<double>(green) * green_multiplier));
            image_data.push_back(static_cast<uint8_t>(static_cast<double>(red) * red_multiplier));
        }

        byte_reader.rewind();
    }

    return image_data;
}

Dib_type read_bmp_header(std::ifstream& ifs, Bmp_header& header)
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

    /* read 18 bytes instead of 14 to include the first field
    * of the DIB header (its Size field) */
    uint8_t buffer[18];
    read_bytes_into(ifs, buffer);

    rez::impl::ByteReader reader(buffer);

    // Type must be 0x424D ("BM")
    if (reader.read_big_endian_value<uint16_t>() != 0x424Du) {
        throw Exception(Error::unsupported_feature);
    }

    header.file_size = reader.read_little_endian_value<uint32_t>();
    reader.skip_bytes(4); // ignore Reserved1 and Reserved2
    header.bitmap_offset = reader.read_little_endian_value<uint32_t>();

    // read the Size field of the DIB header
    const uint32_t dib_size = reader.read_little_endian_value<uint32_t>();
    if (not std::in_range<int>(dib_size))
        throw Exception(Error::bad_formed_file);
    return static_cast<Dib_type>(dib_size);
}

void read_bitmapcoreheader(std::ifstream& ifs, Dib_general& header)
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

    uint8_t buffer[8];
    read_bytes_into(ifs, buffer);

    rez::impl::ByteReader reader(buffer);

    /* the Width and Height fields are uint16_t in Microsoft's
    * documentation, but they are int16_t in the Encyclopedia of
    * Graphics File Formats, Second Edition (page 576). The
    * encyclopedia informs that if the value of Height is positive
    * then the BMP image is a bottom-top image and if the value is
    * negative then it's a top-down image. Microsoft's documentation
    * informs the same for BITMAPINFOHEADER, BITMAPV4HEADER and
    * BITMAPV5HEADER but not for BITMAPCOREHEADER. I am going to go
    * with what the encyclopedia informs */
    header.width = reader.read_little_endian_value<int16_t>();
    header.height = reader.read_little_endian_value<int16_t>();

    header.bottom_top = header.height > 0;
    ensure_abs_representable(header.height);
    header.height = std::abs(header.height);

    validate_image_dimensions(header.width, header.height);

    reader.skip_bytes(2); // ignore Planes
    header.bits_per_pixel = reader.read_little_endian_value<uint16_t>();
    switch (header.bits_per_pixel) {
        case 1:
        case 4:
        case 8:
        case 24:
            break;
        default: throw Exception(Error::bad_formed_file);
    }

    // initialize the helper data-members
    header.stride = compute_stride(header.width, header.bits_per_pixel);
    header.stride_no_padding = div8_ceil(header.width * header.bits_per_pixel);
    header.padding = header.stride - header.stride_no_padding;
}

Image read_bitmapcoreheader_file(std::ifstream& ifs, Bmp_header& bmp_header)
{
    Dib_general dib_header;
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
    const uint32_t palette_entries_count = (bmp_header.bitmap_offset - 26u) / 3u;
    if (palette_entries_count > 256u) throw Exception(Error::bad_formed_file);
    // BGR palette
    const int32_t palette_bytesize = static_cast<int32_t>(palette_entries_count) * 3;

    // read or ignore the color table
    const std::vector<uint8_t> palette = read_color_table(ifs, dib_header.bits_per_pixel, palette_bytesize);

    // read the image data
    std::vector<uint8_t> image_data;

    if (dib_header.bits_per_pixel != 24) {
        if (palette.empty()) throw Exception(Error::bad_formed_file);

        image_data = depalettize_image_data(ifs, palette, 3, dib_header);
    }
    else {
        image_data = read_24bpp_image_data(ifs, dib_header);
    }

    if (dib_header.bottom_top) {
        flip_image_data(image_data, dib_header.height, dib_header.width * 3);
    }

    Image result;
    result.pixels = std::move(image_data);
    result.pixel_format = PixelFormat::bgr;
    result.width = dib_header.width;
    result.height = dib_header.height;

    return result;
}

void read_bitmapinfoheader(std::ifstream& ifs, Dib_general& header)
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

    uint8_t buffer[36];
    read_bytes_into(ifs, buffer);

    rez::impl::ByteReader reader(buffer);

    header.width = reader.read_little_endian_value<int32_t>();
    header.height = reader.read_little_endian_value<int32_t>();

    header.bottom_top = header.height > 0;
    ensure_abs_representable(header.height);
    header.height = std::abs(header.height);

    validate_image_dimensions(header.width, header.height);

    reader.skip_bytes(2); // ignore Planes
    header.bits_per_pixel = reader.read_little_endian_value<uint16_t>();
    switch (header.bits_per_pixel) {
        case 1:
        case 4:
        case 8:
        case 16:
        case 24:
        case 32:
            break;
        default: throw Exception(Error::bad_formed_file);
    }

    const uint32_t compression = reader.read_little_endian_value<uint32_t>();
    if (not std::in_range<int>(compression))
        throw Exception(Error::bad_formed_file);
    header.compression = static_cast<Compression>(compression);
    switch (header.compression) {
        case Compression::rgb:
            break;
        case Compression::bitfields:
            if (header.bits_per_pixel != 16 && header.bits_per_pixel != 32)
                throw Exception(Error::bad_formed_file);
            break;
        default:
            throw Exception(Error::unsupported_feature);
    }

    reader.skip_bytes(12); // ignore SizeImage, XPelsPerMeter, YPelsPerMeter

    const uint32_t color_table_entries = reader.read_little_endian_value<uint32_t>();
    if (not std::in_range<int32_t>(color_table_entries))
        throw Exception(Error::bad_formed_file);
    header.palette_entries = static_cast<int32_t>(color_table_entries);
    /* let's be lenient and put 1000 in there because why not? all that
    * matters is that header.palette_entries doesn't overflow when
    * multiplied by 4 (the maximum size in bytes of a color table entry) */
    if (header.palette_entries > 1000) {
        throw Exception(Error::bad_formed_file);
    }
    else if (header.palette_entries == 0 && header.bits_per_pixel < 16) {
        // 1 << some_number == 2^some_number
        header.palette_entries = 1 << header.bits_per_pixel;
    }

    // initialize the helper data-members
    header.stride = compute_stride(header.width, header.bits_per_pixel);
    header.stride_no_padding = div8_ceil(header.width * header.bits_per_pixel);
    header.padding = header.stride - header.stride_no_padding;
}

Image read_bitmapinfoheader_file(std::ifstream& ifs, Bmp_header& bmp_header, bool masks_in_header, int ignore_amount)
{
    Dib_general dib_header;
    read_bitmapinfoheader(ifs, dib_header);

    if (dib_header.compression == Compression::bitfields || masks_in_header) {
        uint8_t buffer[12];
        read_bytes_into(ifs, buffer);

        rez::impl::ByteReader reader(buffer);
        dib_header.red_mask = reader.read_little_endian_value<uint32_t>();
        dib_header.green_mask = reader.read_little_endian_value<uint32_t>();
        dib_header.blue_mask = reader.read_little_endian_value<uint32_t>();
    }

    // bytes to ignore from the DIB header
    if (ignore_amount != 0) {
        skip_bytes(ifs, ignore_amount);
    }

    // read or ignore the color table
    // each color table entry is 4 bytes (BGRX, X being 0 and should be ignored)
    const int color_table_entry_size = 4;
    const std::vector<uint8_t> color_table = read_color_table(ifs, dib_header.bits_per_pixel, dib_header.palette_entries * color_table_entry_size);

    // read the image data
    std::vector<uint8_t> image_data;

    if (dib_header.compression == Compression::rgb) {
        if (dib_header.bits_per_pixel < 16) {
            if (color_table.empty()) throw Exception(Error::bad_formed_file);

            image_data = depalettize_image_data(ifs, color_table, color_table_entry_size, dib_header);
        }
        else if (dib_header.bits_per_pixel == 16) {
            image_data = read_rgb_16bpp_image_data(ifs, dib_header);
        }
        else if (dib_header.bits_per_pixel == 24) {
            image_data = read_24bpp_image_data(ifs, dib_header);
        }
        // 32 bits per pixel
        else {
            image_data = read_rgb_32bpp_image_data(ifs, dib_header);
        }
    }
    // Compression::bitfields
    else {
        if (dib_header.bits_per_pixel == 16) {
            image_data = read_bitfield_16bpp_image_data(ifs, dib_header);
        }
        // 32 bits per pixel
        else {
            image_data = read_bitfield_32bpp_image_data(ifs, dib_header);
        }
    }

    Image image;
    image.pixels = std::move(image_data);
    image.pixel_format = PixelFormat::bgr;
    image.width = dib_header.width;
    image.height = dib_header.height;

    if (dib_header.bottom_top) {
        flip_image_data(image.pixels, dib_header.height, dib_header.width * 3);
    }

    return image;
}

void read_os22xbitmapheader(std::ifstream& ifs, Dib_general& header, int os22xheader_size)
{
    /*
    struct OS22XBITMAPHEADER (64 bytes) {
        uint32_t Size; // Size of this structure in bytes (already read)
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
        uint16_t Units; // Type of units used to measure resolution (the only valid value is 0, indicating pixels per meter)
        uint16_t Reserved; // Pad structure to 4-byte boundary
        uint16_t Recording; // Recording algorithm (the only valid value is 0, meaning bottom-top scanlines)
        uint16_t Rendering; // Halftoning algorithm used
        uint32_t Size1; // Reserved for halftoning algorithm use
        uint32_t Size2; // Reserved for halftoning algorithm use
        uint32_t ColorEncoding; // Color model used in bitmap (the only valid value is 0, indicating the RGB encoding scheme)
        uint32_t Identifier; // Reserved for application use
    };
    */

    // minus 4 because the Size field was already read
    const int buffer_size = os22xheader_size - 4;
    std::vector<uint8_t> buffer(buffer_size);
    read_bytes_into(ifs, buffer, buffer_size);

    rez::impl::ByteReader reader(buffer);

    /* the dimensions are read as signed integers because Kakumi's limits
    * are within signed limits anyway */
    header.width = reader.read_little_endian_value<int32_t>();
    header.height = reader.read_little_endian_value<int32_t>();
    validate_image_dimensions(header.width, header.height);
    header.bottom_top = true;

    reader.skip_bytes(2); // ignore Planes
    header.bits_per_pixel = reader.read_little_endian_value<uint16_t>();
    switch (header.bits_per_pixel) {
        case 1:
        case 4:
        case 8:
        case 24:
            break;
        default: throw Exception(Error::bad_formed_file);
    }

    if (os22xheader_size == 16) {
        header.compression = Compression::rgb;
        header.palette_entries = (header.bits_per_pixel < 24) ? 1 << header.bits_per_pixel : 0;

        header.stride = compute_stride(header.width, header.bits_per_pixel);
        header.stride_no_padding = div8_ceil(header.width * header.bits_per_pixel);
        header.padding = header.stride - header.stride_no_padding;

        return;
    }

    const uint32_t compression = reader.read_little_endian_value<uint32_t>();
    if (compression != 0) throw Exception(Error::unsupported_feature);
    header.compression = Compression::rgb;

    // ignore ImageDataSize, XResolution, YResolution
    reader.skip_bytes(12);

    const uint32_t palette_entries = reader.read_little_endian_value<uint32_t>();
    if (palette_entries > 256u) throw Exception(Error::bad_formed_file);
    header.palette_entries = static_cast<int32_t>(palette_entries);
    if (header.palette_entries == 0 && header.bits_per_pixel < 16) {
        // 1 << some_number == 2^some_number
        header.palette_entries = 1 << header.bits_per_pixel;
    }

    // ignore everything else
    reader.skip_bytes(28);

    header.stride = compute_stride(header.width, header.bits_per_pixel);
    header.stride_no_padding = div8_ceil(header.width * header.bits_per_pixel);
    header.padding = header.stride - header.stride_no_padding;
}

Image read_os22xbitmapheader_file(std::ifstream& ifs, Bmp_header& bmp_header, int os22xheader_size)
{
    Dib_general dib_header;
    read_os22xbitmapheader(ifs, dib_header, os22xheader_size);

    // read or ignore the color table
    // each color table entry is 4 bytes (BGRX, X being 0 and should be ignored)
    const int color_table_entry_size = 4;
    const std::vector<uint8_t> color_table = read_color_table(ifs, dib_header.bits_per_pixel, dib_header.palette_entries * color_table_entry_size);

    // read the image data
    std::vector<uint8_t> image_data;

    if (dib_header.bits_per_pixel < 24) {
        if (color_table.empty()) throw Exception(Error::bad_formed_file);

        image_data = depalettize_image_data(ifs, color_table, color_table_entry_size, dib_header);
    }
    // 24 bits per pixel
    else {
        image_data = read_24bpp_image_data(ifs, dib_header);
    }
    
    Image image;
    image.pixels = std::move(image_data);
    image.pixel_format = PixelFormat::bgr;
    image.width = dib_header.width;
    image.height = dib_header.height;

    flip_image_data(image.pixels, dib_header.height, dib_header.width * 3);

    return image;
}

} // namespace kak::impl::bmp
