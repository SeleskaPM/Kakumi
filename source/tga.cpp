/* TGA files have 5 areas:
1) TGA File Header
2) Image/Color Map Data
3) Developer Area
4) Extension Area
5) TGA File Footer

The only areas that are acknowledged in this decoder are two:
TGA File Header and Image/Color Map Data.

> The TGA File Footer exists only to be able to find the Developer Area
and the Extension Area.
> The Developer Area would always be ignored anyway.
> The Extension Area contains mostly useless information (metadata
like "Job Name/ID" or "Job Time").

The word "mostly" implies that there is useful information in the
Extension Area and yes, there is, but it's rarely used and the
usefulness is very debatable. Said information is the following:
> Key Color (background color)
> Pixel Aspect Ratio
> Gamma Value
> Attributes Type
> Color Correction Table

Out of all those fields, only Attributes Type is truly of interest,
because it can tell us if the image data contains an alpha channel
and if this one is straight or premultiplied. However, in practice,
it's better to assume transparency from the fields 4.3 (Color map
Entry Size) and 5.5 (Pixel Depth) and assume that the alpha is
always straight. Due to that, the entire Extension Area is ignored.

Another thing to mention is the alpha in the 5 bits per primary color
format (ARRRRRGG GGGBBBBB). The first bit is supposed to indicate
transparency: if its value is 1 then the pixel is transparent.
Unfortunately, in practice, that is not reliable and due to that
this decoder assumes that 5 bits per primary is always opaque.
*/

#include "tga.hpp"

#include <utility>

kak::Image kak::decode_image(const std::filesystem::path& filepath)
{
    using namespace kak::impl;
    using namespace kak::impl::tga;

    std::ifstream tga_file {filepath, std::ios_base::binary};
    if(not tga_file.good()) throw Exception {Error::open_file_failed};

    Header header;
    read_header(tga_file, header);

    // determine the pixel format
    Pixel_format pixel_format {Pixel_format::undefined};
    bool compressed {false};
    bool needs_color_map {false};
    /* the TGA 2.0 specification states that it's better to check the
    * field Image Type rather than the field Color Map Type to know
    * if a color-map is needed because there are images that contain
    * a color-map when the image type does not need it */
    switch(header.image_type) {
        case 1: // Uncompressed, Color-mapped Image
        case 9: // Run-length encoded, Color-mapped Image
            if(header.color_map_type == 0) throw Exception {Error::bad_formed_file};

            if(header.color_map_entry_size < 32) {
                pixel_format = Pixel_format::bgr;
            }
            else { pixel_format = Pixel_format::bgra; }

            // leverage the oportunity to do this validation
            if(header.pixel_depth > 16) {
                /* for color-mapped images, Pixel Depth indicates
                * the number of bits per color-map index and its
                * value can be only 8 or 16 */
                throw Exception {Error::bad_formed_file};
            }

            needs_color_map = true;
            compressed = header.image_type == 9;
            break;

        case 2: // Uncompressed, True-color Image
        case 10: // Run-length encoded, True-color Image
            switch(header.pixel_depth) {
                case 16: // 5 bits per primary color
                case 24:
                    pixel_format = Pixel_format::bgr;
                    break;
                case 32:
                    pixel_format = Pixel_format::bgra;
                    break;
                default: throw Exception {Error::bad_formed_file};
            }

            compressed = header.image_type == 10;
            break;

        case 3: // Uncompressed, Black-and-white Image
        case 11: // Run-length encoded, Black-and-white Image
            switch(header.pixel_depth) {
                case 8:
                    pixel_format = Pixel_format::grey;
                    break;
                case 16:
                    pixel_format = Pixel_format::grey_alpha;
                    break;
                default: throw Exception {Error::bad_formed_file};
            }

            compressed = header.image_type == 11;
            break;

        default: throw Exception {Error::bad_formed_file};
    }

    /* what's being ignored is the field Image ID, the TGA 2.0
    * specification states that it contains "identifying information
    * about the image"; that information is meant to be
    * external/user/application-specific usage information and
    * is useless for decoding */
    if(header.id_length != 0) {
        tga_file.ignore(header.id_length);
        if(not tga_file.good()) throw Exception {Error::read_file_failed};
    }

    Image image;
    image.pixel_format = pixel_format;
    image.image_width = header.image_width;
    image.image_height = header.image_height;

    // TGA files are quite awkward...
    if(needs_color_map) {
        if(compressed) {
            image.pixel_data = read_image_area_mapped_compressed(tga_file, header);
        }
        else { image.pixel_data = read_image_area_mapped(tga_file, header); }
    }
    else {
        Read_image_data_func read_image_data_func;

        if(compressed) {
            switch(header.pixel_depth) {
                case 8:
                    read_image_data_func = read_image_data_greyscale_compressed;
                    break;

                case 16:
                    if(pixel_format == Pixel_format::grey_alpha) { read_image_data_func = read_image_data_greyscale_compressed; }
                    else { read_image_data_func = read_image_data_truecolor_compressed; }
                    break;

                case 24:
                case 32:
                    read_image_data_func = read_image_data_truecolor_compressed;
                    break;
            }    
        }
        else {
            switch(header.pixel_depth) {
                case 8:
                    read_image_data_func = read_image_data_greyscale;
                    break;

                case 16:
                    if(pixel_format == Pixel_format::grey_alpha) { read_image_data_func = read_image_data_greyscale; }
                    else { read_image_data_func = read_image_data_truecolor; }
                    break;

                case 24:
                case 32:
                    read_image_data_func = read_image_data_truecolor;
                    break;
            }    
        }

        image.pixel_data = read_image_area(tga_file, header, read_image_data_func);
    }

    return image;
}

void kak::impl::tga::read_header(std::ifstream& ifs, Header& header)
{
    header.id_length = get_little_endian_value<std::uint8_t>(ifs);
    header.color_map_type = get_little_endian_value<std::uint8_t>(ifs);
    header.image_type = get_little_endian_value<std::uint8_t>(ifs);
    switch(header.image_type) {
        case 1: // Uncompressed, Color - mapped Image
        case 2: // Uncompressed, True-color Image
        case 3: // Uncompressed, Black-and-white Image
        case 9: // Run-length encoded, Color-mapped Image
        case 10: // Run-length encoded, True-color Image
        case 11: // Run-length encoded, Black-and-white Image
            break;
        default: throw Exception {Error::bad_formed_file};
    }

    // Color Map Specification
    header.color_map_origin = get_little_endian_value<std::uint16_t>(ifs);
    header.color_map_length = get_little_endian_value<std::uint16_t>(ifs);
    header.color_map_entry_size = get_little_endian_value<std::uint8_t>(ifs);

    // validate the Color Map Specification if there is a color-map
    if(header.color_map_type != 0) {
        if(header.color_map_length == 0) throw Exception {Error::bad_formed_file};

        /* the TGA 2.0 specification does not specify if
        * Field Entry Index is zero-based. I am going to assume it is */
        if(not (header.color_map_origin < header.color_map_length)) {
            throw Exception {Error::bad_formed_file};
        }

        /* the TGA 2.0 specification states "Typically 15, 16, 24 or
        * 32-bit values are used". Probably, in practice, one should
        * act as if only those values are valid */
        switch(header.color_map_entry_size) {
            case 15: break;
            case 16: break;
            case 24: break;
            case 32: break;
            default: throw Exception {Error::bad_formed_file};
        }
    }

    // Image Specification
    /* ignore the following fields:
    * X-origin of Image (2 bytes)
    * Y-origin of Image (2 bytes) */
    ifs.ignore(4);
    if(not ifs.good()) throw Exception {Error::read_file_failed};

    const std::uint16_t width {get_little_endian_value<std::uint16_t>(ifs)};
    const std::uint16_t height {get_little_endian_value<std::uint16_t>(ifs)};
    if(width > 32767u || height > 32767u) throw Exception {Error::unsupported_feature};

    header.image_width = width;
    header.image_height = height;
    if(header.image_width == 0 || header.image_height == 0) throw Exception {Error::bad_formed_file};

    header.pixel_depth = get_little_endian_value<std::uint8_t>(ifs);
    /* the TGA 2.0 specification states that "common values are 8,
    * 16, 24 and 32 but other pixel depths could be used". Probably,
    * in practice, one should act as if only those values are valid */
    switch(header.pixel_depth) {
        case 8: break;
        case 16: break;
        case 24: break;
        case 32: break;
        default: throw Exception {Error::unsupported_feature};
    }
    header.image_descriptor = get_little_endian_value<std::uint8_t>(ifs);
}

std::vector<std::uint8_t> kak::impl::tga::read_color_map(std::ifstream& ifs, const Header& header)
{
    /* The below (some_number + 7) >> 3 is clever code for dividing
    * some_number by 8 and (if necessary) rounding up */
    const int entry_bytesize {(header.color_map_entry_size + 7) >> 3};

    // ignore the bytes pertaining to unused color-map entries
    if(header.color_map_origin != 0) {
        ifs.ignore(static_cast<std::streamsize>(header.color_map_origin) * entry_bytesize);
        if(not ifs.good()) throw Exception {Error::read_file_failed};
    }

    // read the color-map
    return get_bytes(ifs, entry_bytesize * static_cast<std::streamsize>(header.color_map_length - header.color_map_origin));
}

std::int32_t kak::impl::tga::read_color_map_index_1byte(std::ifstream& ifs)
{
    return get_byte(ifs);
}

std::int32_t kak::impl::tga::read_color_map_index_2bytes(std::ifstream& ifs)
{
    return get_little_endian_value<std::uint16_t>(ifs);
}

void kak::impl::tga::read_rgb_5bits(std::vector<std::uint8_t>& image_data, std::ifstream& ifs)
{
    /* a color comes like this: GGGBBBBB (byte 1),
    * ARRRRRGG (byte 2) */
    const std::uint8_t byte1 {get_byte(ifs)};
    const std::uint8_t byte2 {get_byte(ifs)};
    const int color {byte1 | (byte2 << 8)};
    // blue
    image_data.push_back(color & 0b0000'0000'0001'1111);
    // green
    image_data.push_back((color & 0b0000'0011'1110'0000) >> 5);
    // red
    image_data.push_back((color & 0b0111'1100'0000'0000) >> 10);
}

void kak::impl::tga::read_rgb_5bits(std::vector<std::uint8_t>& image_data, std::span<const std::uint8_t> color_map, const std::int32_t index)
{
    /* an entry comes like this: GGGBBBBB (byte 1),
    * ARRRRRGG (byte 2) */
    const std::uint8_t byte1 {color_map[index]};
    const std::uint8_t byte2 {color_map[index + 1]};
    const int entry {byte1 | (byte2 << 8)};
    // blue
    image_data.push_back(entry & 0b0000'0000'0001'1111);
    // green
    image_data.push_back((entry & 0b0000'0011'1110'0000) >> 5);
    // red
    image_data.push_back((entry & 0b0111'1100'0000'0000) >> 10);
}

std::vector<std::uint8_t> kak::impl::tga::read_image_area(std::ifstream& ifs, const Header& header, Read_image_data_func read_image_data_func)
{
    /* even if the image type doesn't need a color-map, there might
    * be one in the file; if that is the case, ignore it */
    if(header.color_map_type != 0) {
        /* The below (some_number + 7) >> 3 is clever code for dividing
        * some_number by 8 and (if necessary) rounding up */
        const int entry_bytesize {(header.color_map_entry_size + 7) >> 3};
        ifs.ignore(entry_bytesize * static_cast<std::streamsize>(header.color_map_length));
        if(not ifs.good()) throw Exception {Error::bad_formed_file};
    }

    return read_image_data_func(ifs, header);
}

std::vector<std::uint8_t> kak::impl::tga::read_image_data_truecolor(std::ifstream& ifs, const Header& header)
{
    const std::int32_t pixel_count {static_cast<std::int32_t>(header.image_width) * header.image_height};
    std::vector<std::uint8_t> image_data;

    // BGR (5 bits per channel)
    if(header.pixel_depth == 16) {
        for(std::int32_t i = 0; i < pixel_count; ++i) {
            read_rgb_5bits(image_data, ifs);
        }
    }
    // BGR (1 byte per channel)
    else if(header.pixel_depth == 24) {
        image_data = get_bytes(ifs, static_cast<std::streamsize>(pixel_count) * 3);
    }
    // BGRA (1 byte per channel)
    else {
        image_data = get_bytes(ifs, static_cast<std::streamsize>(pixel_count) * 4);
    }

    return image_data;
}

std::vector<std::uint8_t> kak::impl::tga::read_image_data_truecolor_compressed(std::ifstream& ifs, const Header& header)
{
    const std::int32_t pixel_count {static_cast<std::int32_t>(header.image_width) * header.image_height};
    std::vector<std::uint8_t> image_data;

    int repetition_count;
    std::int32_t decompressed_pixels {0};
    // BGR (5 bits per channel)
    if(header.pixel_depth == 16) {
        while(decompressed_pixels < pixel_count) {
            repetition_count = get_byte(ifs);
            // if true, it's a run-length packet
            if(repetition_count & 0b1000'0000) {
                repetition_count = (repetition_count & 0b0111'1111) + 1;

                /* a color comes like this: GGGBBBBB (byte 1),
                * ARRRRRGG (byte 2) */
                const std::uint8_t byte1 {get_byte(ifs)};
                const std::uint8_t byte2 {get_byte(ifs)};
                const int color {byte1 | (byte2 << 8)};

                const std::uint8_t blue {color & 0b0000'0000'0001'1111};
                const std::uint8_t green {(color & 0b0000'0011'1110'0000) >> 5};
                const std::uint8_t red {(color & 0b0111'1100'0000'0000) >> 10};

                for(int i = 0; i < repetition_count; ++i) {
                    image_data.push_back(blue);
                    image_data.push_back(green);
                    image_data.push_back(red);
                }
            }
            // otherwise, raw packet
            else {
                repetition_count = (repetition_count & 0b0111'1111) + 1;
                for(int i = 0; i < repetition_count; ++i) {
                    read_rgb_5bits(image_data, ifs);
                }
            }

            decompressed_pixels += repetition_count;
        }
    }
    // BGR (1 byte per channel)
    else if(header.pixel_depth == 24) {
        while(decompressed_pixels < pixel_count) {
            repetition_count = get_byte(ifs);
            // if true, it's a run-length packet
            if(repetition_count & 0b1000'0000) {
                repetition_count = (repetition_count & 0b0111'1111) + 1;

                const std::uint8_t blue {get_byte(ifs)};
                const std::uint8_t green {get_byte(ifs)};
                const std::uint8_t red {get_byte(ifs)};

                for(int i = 0; i < repetition_count; ++i) {
                    image_data.push_back(blue);
                    image_data.push_back(green);
                    image_data.push_back(red);
                }
            }
            // otherwise, raw packet
            else {
                repetition_count = (repetition_count & 0b0111'1111) + 1;
                for(int i = 0; i < repetition_count; ++i) {
                    // blue
                    image_data.push_back(get_byte(ifs));
                    // green
                    image_data.push_back(get_byte(ifs));
                    // red
                    image_data.push_back(get_byte(ifs));
                }
            }

            decompressed_pixels += repetition_count;
        }
    }
    // BGRA (1 byte per channel)
    else {
        while(decompressed_pixels < pixel_count) {
            repetition_count = get_byte(ifs);
            // if true, it's a run-length packet
            if(repetition_count & 0b1000'0000) {
                repetition_count = (repetition_count & 0b0111'1111) + 1;

                const std::uint8_t blue {get_byte(ifs)};
                const std::uint8_t green {get_byte(ifs)};
                const std::uint8_t red {get_byte(ifs)};
                const std::uint8_t alpha {get_byte(ifs)};

                for(int i = 0; i < repetition_count; ++i) {
                    image_data.push_back(blue);
                    image_data.push_back(green);
                    image_data.push_back(red);
                    image_data.push_back(alpha);
                }
            }
            // otherwise, raw packet
            else {
                repetition_count = (repetition_count & 0b0111'1111) + 1;
                for(int i = 0; i < repetition_count; ++i) {
                    // blue
                    image_data.push_back(get_byte(ifs));
                    // green
                    image_data.push_back(get_byte(ifs));
                    // red
                    image_data.push_back(get_byte(ifs));
                    // alpha
                    image_data.push_back(get_byte(ifs));
                }
            }

            decompressed_pixels += repetition_count;
        }
    }

    if(decompressed_pixels != pixel_count) throw Exception {Error::bad_formed_file};
    return image_data;
}

std::vector<std::uint8_t> kak::impl::tga::read_image_data_greyscale(std::ifstream& ifs, const Header& header)
{
    const std::int32_t pixel_count {static_cast<std::int32_t>(header.image_width) * header.image_height};
    std::vector<std::uint8_t> image_data;

    // greyscale
    if(header.pixel_depth == 8) {
        image_data = get_bytes(ifs, pixel_count);
    }
    // greyscale-alpha
    else { image_data = get_bytes(ifs, pixel_count * 2); } // safe multiplication

    return image_data;
}

std::vector<std::uint8_t> kak::impl::tga::read_image_data_greyscale_compressed(std::ifstream& ifs, const Header& header)
{
    const std::int32_t pixel_count {static_cast<std::int32_t>(header.image_width) * header.image_height};
    std::vector<std::uint8_t> image_data;

    int repetition_count;
    std::int32_t decompressed_pixels {0};
    // greyscale
    if(header.pixel_depth == 8) {
        while(decompressed_pixels < pixel_count) {
            repetition_count = get_byte(ifs);
            // if true, it's a run-length packet
            if(repetition_count & 0b1000'0000) {
                repetition_count = (repetition_count & 0b0111'1111) + 1;

                const std::uint8_t grey {get_byte(ifs)};
                for(int i = 0; i < repetition_count; ++i) {
                    image_data.push_back(grey);
                }
            }
            // otherwise, raw packet
            else {
                repetition_count = (repetition_count & 0b0111'1111) + 1;
                for(int i = 0; i < repetition_count; ++i) {
                    image_data.push_back(get_byte(ifs));
                }
            }

            decompressed_pixels += repetition_count;
        }
    }
    // greyscale-alpha
    else {
        while(decompressed_pixels < pixel_count) {
            repetition_count = get_byte(ifs);
            // if true, it's a run-length packet
            if(repetition_count & 0b1000'0000) {
                repetition_count = (repetition_count & 0b0111'1111) + 1;

                const std::uint8_t grey {get_byte(ifs)};
                const std::uint8_t alpha {get_byte(ifs)};
                for(int i = 0; i < repetition_count; ++i) {
                    image_data.push_back(grey);
                    image_data.push_back(alpha);
                }
            }
            // otherwise, raw packet
            else {
                repetition_count = (repetition_count & 0b0111'1111) + 1;
                for(int i = 0; i < repetition_count; ++i) {
                    image_data.push_back(get_byte(ifs));
                    image_data.push_back(get_byte(ifs));
                }
            }

            decompressed_pixels += repetition_count;
        }
    }

    if(decompressed_pixels != pixel_count) throw Exception {Error::bad_formed_file};
    return image_data;
}

std::vector<std::uint8_t> kak::impl::tga::read_image_area_mapped(std::ifstream& ifs, const Header& header)
{
    const std::vector<std::uint8_t> color_map = read_color_map(ifs, header);
    const std::int32_t color_map_last_valid_index {static_cast<std::int32_t>(color_map.size()) - 1};
    /* The below (some_number + 7) >> 3 is clever code for dividing
    * some_number by 8 and (if necessary) rounding up */
    const int entry_bytesize {(header.color_map_entry_size + 7) >> 3};

    // read the image data (color-map indices)
    const std::int32_t pixel_count {static_cast<std::int32_t>(header.image_width) * header.image_height};
    /* the TGA 2.0 specification doesn't specify if the indices
    * found in the image data area are relative to the field
    * First Entry Index, for example, imagining that First Entry
    * Index has a value of 5, does an index value of 2 means the
    * index 7 of the color-map or is it an error? Throughout this
    * switch statement, I'll assume the former is true */
    std::vector<std::uint8_t> image_data;
    std::int32_t index;

    /* for color-mapped images, Pixel Depth indicates
    * the number of bits per color-map index and its
    * value can be only 8 or 16 */
    std::int32_t (*read_color_map_index)(std::ifstream& ifs);
    if(header.pixel_depth == 8) { read_color_map_index = read_color_map_index_1byte; }
    else { read_color_map_index = read_color_map_index_2bytes; }

    if(entry_bytesize == 2) {
        for(std::int32_t i = 0; i < pixel_count; ++i) {
            index = read_color_map_index(ifs);
            index *= entry_bytesize;

            if(index + 1 > color_map_last_valid_index) {
                throw Exception {Error::bad_formed_file};
            }
            read_rgb_5bits(image_data, color_map, index);
        }
    }
    else if(entry_bytesize == 3) {
        for(std::int32_t i = 0; i < pixel_count; ++i) {
            index = read_color_map_index(ifs);
            index *= entry_bytesize;

            if(index + 2 > color_map_last_valid_index) {
                throw Exception {Error::bad_formed_file};
            }
            image_data.push_back(color_map[index]); // blue
            image_data.push_back(color_map[index + 1]); // green
            image_data.push_back(color_map[index + 2]); // red
        }
    }
    // entry_bytesize == 4
    else {
        for(std::int32_t i = 0; i < pixel_count; ++i) {
            index = read_color_map_index(ifs);
            index *= entry_bytesize;

            if(index + 3 > color_map_last_valid_index) {
                throw Exception {Error::bad_formed_file};
            }
            image_data.push_back(color_map[index]); // blue
            image_data.push_back(color_map[index + 1]); // green
            image_data.push_back(color_map[index + 2]); // red
            image_data.push_back(color_map[index + 3]); // alpha
        }
    }

    return image_data;
}

std::vector<std::uint8_t> kak::impl::tga::read_image_area_mapped_compressed(std::ifstream& ifs, const Header& header)
{
    const std::vector<std::uint8_t> color_map = read_color_map(ifs, header);
    const std::int32_t color_map_last_valid_index {static_cast<std::int32_t>(color_map.size()) - 1};
    /* The below (some_number + 7) >> 3 is clever code for dividing
    * some_number by 8 and (if necessary) rounding up */
    const int entry_bytesize {(header.color_map_entry_size + 7) >> 3};

    // read the image data (color-map indices)
    const std::int32_t pixel_count {static_cast<std::int32_t>(header.image_width) * header.image_height};
    /* the TGA 2.0 specification doesn't specify if the indices
    * found in the image data area are relative to the field
    * First Entry Index, for example, imagining that First Entry
    * Index has a value of 5, does an index value of 2 means the
    * index 7 of the color-map or is it an error? Throughout this
    * switch statement, I'll assume the former is true */
    std::vector<std::uint8_t> image_data;
    std::int32_t decompressed_indices_count {0};
    int repetition_count;
    std::int32_t index;

    /* for color-mapped images, Pixel Depth indicates
    * the number of bits per color-map index and its
    * value can be only 8 or 16 */
    std::int32_t (*read_color_map_index)(std::ifstream& ifs);
    if(header.pixel_depth == 8) { read_color_map_index = read_color_map_index_1byte; }
    else { read_color_map_index = read_color_map_index_2bytes; }

    if(entry_bytesize == 2) {
        while(decompressed_indices_count < pixel_count) {
            repetition_count = get_byte(ifs);
            // if true, it's a run-length packet
            if(repetition_count & 0b1000'0000) {
                repetition_count = (repetition_count & 0b0111'1111) + 1;
                index = read_color_map_index(ifs);
                index *= entry_bytesize;
                if(index + 1 > color_map_last_valid_index) {
                    throw Exception {Error::bad_formed_file};
                }

                /* an entry comes like this: GGGBBBBB (byte 1),
                * ARRRRRGG (byte 2) */
                const std::uint8_t byte1 {color_map[index]};
                const std::uint8_t byte2 {color_map[index + 1]};
                const int entry {byte1 | (byte2 << 8)};

                const std::uint8_t blue {static_cast<std::uint8_t>(entry & 0b0000'0000'0001'1111)};
                const std::uint8_t green {static_cast<std::uint8_t>((entry & 0b0000'0011'1110'0000) >> 5)};
                const std::uint8_t red {static_cast<std::uint8_t>((entry & 0b0111'1100'0000'0000) >> 10)};

                for(int i = 0; i < repetition_count; ++i) {
                    image_data.push_back(blue);
                    image_data.push_back(green);
                    image_data.push_back(red);
                }
            }
            // otherwise, raw packet
            else {
                repetition_count = (repetition_count & 0b0111'1111) + 1;
                for(int i = 0; i < repetition_count; ++i) {
                    index = read_color_map_index(ifs);
                    index *= entry_bytesize;
                    if(index + 1 > color_map_last_valid_index) {
                        throw Exception {Error::bad_formed_file};
                    }

                    read_rgb_5bits(image_data, color_map, index);
                }
            }

            decompressed_indices_count += repetition_count;
        }
    }
    else if(entry_bytesize == 3) {
        while(decompressed_indices_count < pixel_count) {
            repetition_count = get_byte(ifs);
            // if true, it's a run-length packet
            if(repetition_count & 0b1000'0000) {
                repetition_count = (repetition_count & 0b0111'1111) + 1;
                index = read_color_map_index(ifs);
                index *= entry_bytesize;
                if(index + 2 > color_map_last_valid_index) {
                    throw Exception {Error::bad_formed_file};
                }

                const std::uint8_t blue {color_map[index]};
                const std::uint8_t green {color_map[index + 1]};
                const std::uint8_t red {color_map[index + 2]};

                for(int i = 0; i < repetition_count; ++i) {
                    image_data.push_back(blue);
                    image_data.push_back(green);
                    image_data.push_back(red);
                }
            }
            // otherwise, raw packet
            else {
                repetition_count = (repetition_count & 0b0111'1111) + 1;
                for(int i = 0; i < repetition_count; ++i) {
                    index = read_color_map_index(ifs);
                    index *= entry_bytesize;
                    if(index + 2 > color_map_last_valid_index) {
                        throw Exception {Error::bad_formed_file};
                    }

                    image_data.push_back(color_map[index]); // blue
                    image_data.push_back(color_map[index + 1]); // green
                    image_data.push_back(color_map[index + 2]); // red
                }
            }

            decompressed_indices_count += repetition_count;
        }
    }
    // entry_bytesize == 4
    else {
        while(decompressed_indices_count < pixel_count) {
            repetition_count = get_byte(ifs);
            // if true, it's a run-length packet
            if(repetition_count & 0b1000'0000) {
                repetition_count = (repetition_count & 0b0111'1111) + 1;
                index = read_color_map_index(ifs);
                index *= entry_bytesize;
                if(index + 3 > color_map_last_valid_index) {
                    throw Exception {Error::bad_formed_file};
                }

                const std::uint8_t blue {color_map[index]};
                const std::uint8_t green {color_map[index + 1]};
                const std::uint8_t red {color_map[index + 2]};
                const std::uint8_t alpha {color_map[index + 3]};

                for(int i = 0; i < repetition_count; ++i) {
                    image_data.push_back(blue);
                    image_data.push_back(green);
                    image_data.push_back(red);
                    image_data.push_back(alpha);
                }
            }
            // otherwise, raw packet
            else {
                repetition_count = (repetition_count & 0b0111'1111) + 1;
                for(int i = 0; i < repetition_count; ++i) {
                    index = read_color_map_index(ifs);
                    index *= entry_bytesize;
                    if(index + 3 > color_map_last_valid_index) {
                        throw Exception {Error::bad_formed_file};
                    }

                    image_data.push_back(color_map[index]); // blue
                    image_data.push_back(color_map[index + 1]); // green
                    image_data.push_back(color_map[index + 2]); // red
                    image_data.push_back(color_map[index + 3]); // alpha
                }
            }

            decompressed_indices_count += repetition_count;
        }
    }

    if(decompressed_indices_count != pixel_count) throw Exception {Error::bad_formed_file};
    return image_data;
}
