#include "gif.hpp"

#include <cstring>
#include <utility>
#include <optional>

kak::Image kak::gif::decode_image(const std::filesystem::path& filepath)
{
    using namespace kak::impl;
    using namespace kak::impl::gif;

    std::ifstream gif_file {filepath, std::ios_base::binary};
    if(not gif_file.good()) throw Exception {Error::open_file_failed};

    read_signature(gif_file);
    bool v87a {read_version_and_check_if_87a(gif_file)};

    Logical_screen_descriptor logical_screen_descriptor;
    read_logical_screen_descriptor(gif_file, logical_screen_descriptor);

    // after the Logical Screen Descriptor, there could be a Global Color Table
    std::vector<std::uint8_t> global_color_table;
    if(logical_screen_descriptor.global_color_table_flag) {
        global_color_table = read_bytes(gif_file, logical_screen_descriptor.size_of_global_color_table * 3);
    }

    // after the Global Color Table, a series of blocks follow until the file ends
    bool image_descriptor_found {false}; // this function reads only 1 Image Descriptor
    std::vector<std::uint8_t> image_data;
    std::optional<Graphic_control_extension> graphic_control_extension;
    PixelFormat pixel_format {PixelFormat::undefined};
    int image_width {0};
    int image_height {0};

    int block_label {read_little_endian_value<std::uint8_t>(gif_file)};
    // 0x3B == End-of-file
    while(block_label != 0x3B) {
        switch(block_label) {
            case 0x2C: { // Image Descriptor
                Image_descriptor image_descriptor;
                read_image_descriptor(gif_file, image_descriptor);
                image_width = image_descriptor.width;
                image_height = image_descriptor.height;

                // after an Image Descriptor, a Local Color Table could follow
                if(image_descriptor.local_color_table_flag) {
                    std::vector<std::uint8_t> local_color_table = read_bytes(gif_file, image_descriptor.size_of_local_color_table * 3);
                    image_data = decompress_image_data(gif_file, image_descriptor.size_of_local_color_table);

                    if(image_descriptor.interlace_flag) {
                        image_data = deinterlace(image_data, image_descriptor.width, image_descriptor.height);
                    }

                    if(graphic_control_extension.has_value() && graphic_control_extension->transparent_color_flag) {
                        image_data = depalettise(image_data, local_color_table, graphic_control_extension->transparent_color_index);
                        pixel_format = PixelFormat::rgba;
                    }
                    else {
                        image_data = depalettise(image_data, local_color_table);
                        pixel_format = PixelFormat::rgb;
                    }
                }
                else {
                    image_data = decompress_image_data(gif_file, logical_screen_descriptor.size_of_global_color_table);
                    if(image_descriptor.interlace_flag) {
                        image_data = deinterlace(image_data, image_descriptor.width, image_descriptor.height);
                    }

                    if(graphic_control_extension.has_value() && graphic_control_extension->transparent_color_flag) {
                        image_data = depalettise(image_data, global_color_table, graphic_control_extension->transparent_color_index);
                        pixel_format = PixelFormat::rgba;
                    }
                    else {
                        image_data = depalettise(image_data, global_color_table);
                        pixel_format = PixelFormat::rgb;
                    }
                }
                
                image_descriptor_found = true;
                break;
            }

            case 0x21: { // Extension block
                block_label = read_little_endian_value<std::uint8_t>(gif_file);
                switch(block_label) {
                    case 0xFF: { // Application Extension
                        gif_file.ignore(12);
                        if(not gif_file.good()) throw Exception {Error::read_file_failed};

                        std::uint8_t block_size {read_little_endian_value<std::uint8_t>(gif_file)};
                        while(block_size != 0) {
                            gif_file.ignore(block_size);
                            if(not gif_file.good()) throw Exception {Error::read_file_failed};

                            block_size = read_little_endian_value<std::uint8_t>(gif_file);
                        }
                        break;
                    }

                    case 0xF9: { // Graphic Control Extension
                        gif_file.ignore(1); // ignore the block size, it's a constant
                        if(not gif_file.good()) throw Exception {Error::read_file_failed};

                        Graphic_control_extension gce;
                        read_graphic_control_extension(gif_file, gce);
                        graphic_control_extension = gce;

                        gif_file.ignore(1); // ignore the block terminator
                        if(not gif_file.good()) throw Exception {Error::read_file_failed};
                        break;
                    }

                    case 0xFE: { // Comment Extension
                        std::uint8_t block_size {read_little_endian_value<std::uint8_t>(gif_file)};
                        while(block_size != 0) {
                            gif_file.ignore(block_size);
                            if(not gif_file.good()) throw Exception {Error::read_file_failed};

                            block_size = read_little_endian_value<std::uint8_t>(gif_file);
                        }
                        break;
                    }

                    case 0x01: { // Plain Text Extension
                        gif_file.ignore(13);
                        if(not gif_file.good()) throw Exception {Error::read_file_failed};

                        std::uint8_t block_size {read_little_endian_value<std::uint8_t>(gif_file)};
                        while(block_size != 0) {
                            gif_file.ignore(block_size);
                            if(not gif_file.good()) throw Exception {Error::read_file_failed};

                            block_size = read_little_endian_value<std::uint8_t>(gif_file);
                        }
                        break;
                    }

                    default: throw Exception {Error::bad_formed_file};
                }
                break;
            }

            default: throw Exception {Error::bad_formed_file};
        }
        if(image_descriptor_found) break;
        block_label = read_little_endian_value<std::uint8_t>(gif_file);
    }

    Image image;
    image.pixels = std::move(image_data);
    image.pixel_format = pixel_format;
    image.width = image_width;
    image.height = image_height;
    return image;
}

kak::gif::Gif kak::gif::decode(const std::filesystem::path& filepath)
{
    using namespace kak::impl;
    using namespace kak::impl::gif;

    std::ifstream gif_file {filepath, std::ios_base::binary};
    if(not gif_file.good()) throw Exception {Error::open_file_failed};

    read_signature(gif_file);
    bool v87a {read_version_and_check_if_87a(gif_file)};

    Logical_screen_descriptor logical_screen_descriptor;
    read_logical_screen_descriptor(gif_file, logical_screen_descriptor);

    Gif result;
    result.background_width = logical_screen_descriptor.width;
    result.background_height = logical_screen_descriptor.height;
    
    // after the Logical Screen Descriptor, there could be a Global Color Table
    std::vector<std::uint8_t> global_color_table;
    if(logical_screen_descriptor.global_color_table_flag) {
        global_color_table = read_bytes(gif_file, logical_screen_descriptor.size_of_global_color_table * 3);

        const int index {logical_screen_descriptor.background_color_index * 3};
        if(index > static_cast<int>(global_color_table.size()) - 3) {
            throw Exception {Error::bad_formed_file};
        }

        result.background_color_red = global_color_table[index];
        result.background_color_green = global_color_table[index + 1];
        result.background_color_blue = global_color_table[index + 2];
    }

    // after the Global Color Table, a series of blocks follow until the file ends
    std::optional<Graphic_control_extension> graphic_control_extension;

    int block_label {read_little_endian_value<std::uint8_t>(gif_file)};
    // 0x3B == End-of-file
    while(block_label != 0x3B) {
        switch(block_label) {
            case 0x2C: { // Image Descriptor
                Image_descriptor image_descriptor;
                read_image_descriptor(gif_file, image_descriptor);

                Frame frame;
                frame.left = image_descriptor.left;
                frame.top = image_descriptor.top;
                frame.width = image_descriptor.width;
                frame.height = image_descriptor.height;
                // after an Image Descriptor, a Local Color Table could follow
                if(image_descriptor.local_color_table_flag) {
                    std::vector<std::uint8_t> local_color_table = read_bytes(gif_file, image_descriptor.size_of_local_color_table * 3);
                    frame.data = decompress_image_data(gif_file, image_descriptor.size_of_local_color_table);

                    if(image_descriptor.interlace_flag) {
                        frame.data = deinterlace(frame.data, image_descriptor.width, image_descriptor.height);
                    }

                    if(graphic_control_extension.has_value()) {
                        frame.disposal_method = graphic_control_extension->disposal_method;
                        frame.delay_time = graphic_control_extension->delay_time;
                        frame.user_input_flag = graphic_control_extension->user_input_flag;

                        if(graphic_control_extension->transparent_color_flag) {
                            frame.data = depalettise(frame.data, local_color_table, graphic_control_extension->transparent_color_index);
                            frame.pixel_format = PixelFormat::rgba;
                        }
                        else {
                            frame.data = depalettise(frame.data, local_color_table);
                            frame.pixel_format = PixelFormat::rgb;
                        }

                        // only 1 Graphic Control Extension per Image Descriptor is allowed
                        graphic_control_extension.reset();
                    }
                    else {
                        frame.data = depalettise(frame.data, local_color_table);
                        frame.pixel_format = PixelFormat::rgb;
                    }
                }
                else {
                    frame.data = decompress_image_data(gif_file, logical_screen_descriptor.size_of_global_color_table);
                    if(image_descriptor.interlace_flag) {
                        frame.data = deinterlace(frame.data, image_descriptor.width, image_descriptor.height);
                    }

                    if(graphic_control_extension.has_value()) {
                        frame.disposal_method = graphic_control_extension->disposal_method;
                        frame.delay_time = graphic_control_extension->delay_time;
                        frame.user_input_flag = graphic_control_extension->user_input_flag;

                        if(graphic_control_extension->transparent_color_flag) {
                            frame.data = depalettise(frame.data, global_color_table, graphic_control_extension->transparent_color_index);
                            frame.pixel_format = PixelFormat::rgba;    
                        }
                        else {
                            frame.data = depalettise(frame.data, global_color_table);
                            frame.pixel_format = PixelFormat::rgb;    
                        }
                    }
                    else {
                        frame.data = depalettise(frame.data, global_color_table);
                        frame.pixel_format = PixelFormat::rgb;
                    }
                }

                result.frames.push_back(std::move(frame));
                break;
            }

            case 0x21: { // Extension block
                block_label = read_little_endian_value<std::uint8_t>(gif_file);
                switch(block_label) {
                    case 0xFF: { // Application Extension
                        gif_file.ignore(12);
                        if(not gif_file.good()) throw Exception {Error::read_file_failed};

                        std::uint8_t block_size {read_little_endian_value<std::uint8_t>(gif_file)};
                        while(block_size != 0) {
                            gif_file.ignore(block_size);
                            if(not gif_file.good()) throw Exception {Error::read_file_failed};

                            block_size = read_little_endian_value<std::uint8_t>(gif_file);
                        }
                        break;
                    }

                    case 0xF9: { // Graphic Control Extension
                        gif_file.ignore(1); // ignore the block size, it's a constant
                        if(not gif_file.good()) throw Exception {Error::read_file_failed};

                        Graphic_control_extension gce;
                        read_graphic_control_extension(gif_file, gce);
                        graphic_control_extension = gce;

                        gif_file.ignore(1); // ignore the block terminator
                        if(not gif_file.good()) throw Exception {Error::read_file_failed};
                        break;
                    }

                    case 0xFE: { // Comment Extension
                        std::uint8_t block_size {read_little_endian_value<std::uint8_t>(gif_file)};
                        while(block_size != 0) {
                            gif_file.ignore(block_size);
                            if(not gif_file.good()) throw Exception {Error::read_file_failed};

                            block_size = read_little_endian_value<std::uint8_t>(gif_file);
                        }
                        break;
                    }

                    case 0x01: { // Plain Text Extension
                        gif_file.ignore(13);
                        if(not gif_file.good()) throw Exception {Error::read_file_failed};

                        std::uint8_t block_size {read_little_endian_value<std::uint8_t>(gif_file)};
                        while(block_size != 0) {
                            gif_file.ignore(block_size);
                            if(not gif_file.good()) throw Exception {Error::read_file_failed};

                            block_size = read_little_endian_value<std::uint8_t>(gif_file);
                        }
                        break;
                    }

                    default: throw Exception {Error::bad_formed_file};
                }
                break;
            }

            default: throw Exception {Error::bad_formed_file};
        }
        
        block_label = read_little_endian_value<std::uint8_t>(gif_file);
    }

    return result;
}

void kak::impl::gif::read_signature(std::ifstream& ifs)
{
    /* the signature of a GIF file is the ASCII string "GIF". Here I
    * am reading 4 bytes instead of 3, which is fine because the 4th
    * byte will always be "8", because that 4th byte is part of the
    * field that comes after the signature, which is the version of
    * the GIF standard and only two strings are valid: "87a" and "89a" */
    const std::int32_t signature {read_big_endian_value<std::int32_t>(ifs)};
    if(signature != 0x47494638) throw Exception {Error::bad_formed_file};
}

bool kak::impl::gif::read_version_and_check_if_87a(std::ifstream& ifs)
{
    /* 4 bytes were read when reading the signature so in here we
    * only need to read 2 bytes */
    const std::int16_t version {read_big_endian_value<std::int16_t>(ifs)};
    switch(version) {
        case 0x3761: return true; // 87a
        case 0x3961: return false; // 89a
        default: throw Exception {Error::bad_formed_file};
    }
}

void kak::impl::gif::read_logical_screen_descriptor(std::ifstream& ifs, Logical_screen_descriptor& lsd)
{
    const std::uint16_t width {read_little_endian_value<std::uint16_t>(ifs)};
    const std::uint16_t height {read_little_endian_value<std::uint16_t>(ifs)};
    if(width > 32767u || height > 32767u) throw Exception {Error::unsupported_feature};

    lsd.width = static_cast<int>(width);
    lsd.height = static_cast<int>(height);
    if(lsd.width == 0 || lsd.height == 0) throw Exception {Error::bad_formed_file};

    const int packed_fields {read_little_endian_value<std::uint8_t>(ifs)};
    lsd.global_color_table_flag = packed_fields & 0b1000'0000;
    lsd.color_resolution = ((packed_fields & 0b0111'0000) >> 4) + 1;
    lsd.sort_flag = packed_fields & 0b0000'1000;
    // 1 << some_value == std::pow(2, some_value)
    lsd.size_of_global_color_table = 1 << ((packed_fields & 0b0000'0111) + 1);

    lsd.background_color_index = read_little_endian_value<std::uint8_t>(ifs);
    lsd.pixel_aspect_ratio = read_little_endian_value<std::uint8_t>(ifs);
}

void kak::impl::gif::read_image_descriptor(std::ifstream& ifs, Image_descriptor& descriptor)
{
    const std::uint16_t left {read_little_endian_value<std::uint16_t>(ifs)};
    const std::uint16_t top {read_little_endian_value<std::uint16_t>(ifs)};
    if(left > 32767u || top > 32767u) throw Exception {Error::unsupported_feature};

    descriptor.left = static_cast<int>(left);
    descriptor.top = static_cast<int>(top);

    /* I should also check if the image fits within the logical
    * screen, but somewhere (I don't remember where) I read that
    * there are GIF files out there with Image Descriptors whose
    * size doesn't fit, yet they can be displayed if that error
    * is ignored */
    const std::uint16_t width {read_little_endian_value<std::uint16_t>(ifs)};
    const std::uint16_t height {read_little_endian_value<std::uint16_t>(ifs)};
    if(width > 32767u || height > 32767u) throw Exception {Error::unsupported_feature};

    descriptor.width = static_cast<int>(width);
    descriptor.height = static_cast<int>(height);
    if(descriptor.width == 0 || descriptor.height == 0) throw Exception {Error::bad_formed_file};

    const int packed_fields {read_little_endian_value<std::uint8_t>(ifs)};
    descriptor.local_color_table_flag = packed_fields & 0b1000'0000;
    descriptor.interlace_flag = packed_fields & 0b0100'0000;
    descriptor.sort_flag = packed_fields & 0b0010'0000;
    // 1 << some_number == std::pow(2, some_number)
    descriptor.size_of_local_color_table = 1 << ((packed_fields & 0b0000'0111) + 1);
}

void kak::impl::gif::read_graphic_control_extension(std::ifstream& ifs, Graphic_control_extension& gce)
{
    const int packed_fields {read_little_endian_value<std::uint8_t>(ifs)};
    gce.disposal_method = (packed_fields & 0b0001'1100) >> 2;
    /* the disposal method cannot have a value bigger than 3, so
    * maybe I should throw an exception, but I decided to instead
    * overwrite the value because this would be a minor mistake and
    * the GIF file could still be displayed. The value 0 means
    * 'no disposal specified' and it's a valid value */
    if(gce.disposal_method > 3) gce.disposal_method = 0;
    gce.user_input_flag = packed_fields & 0b0000'0010;
    gce.transparent_color_flag = packed_fields & 0b0000'0001;

    gce.delay_time = read_little_endian_value<std::uint16_t>(ifs);
    gce.transparent_color_index = read_little_endian_value<std::uint8_t>(ifs);
}

std::vector<std::uint8_t> kak::impl::gif::decompress_image_data(std::ifstream& ifs, const int color_count)
{
    /* the GIF specification calls "Image Data" what is going to be
    * decompressed. That isn't accurate: what is going to be
    * decompressed is index data into a color table */
    
    const int minimum_lzw_code_size {read_little_endian_value<std::uint8_t>(ifs)};
    /* the maximum number of entries in a color table is 256,
    * therefore the minimum LZW code size cannot be bigger than 8
    * because what 'minimum_lzw_code_size' represents is the minimum
    * number of bits necessary to be able to represent any value that
    * is a valid index into the active color table.
    *
    * it's true that not all color tables will have 256 entries, so
    * comparing with 8 all the time is not precise, however, there
    * might be GIF files out there that are also not precise in this
    * aspect (it's only my imagination) */
    if(minimum_lzw_code_size == 0 || minimum_lzw_code_size > 8) throw Exception {Error::bad_formed_file};

    std::vector<std::uint8_t> compression_codes;
    std::vector<std::uint8_t>::size_type insertion_index;
    int block_size {read_little_endian_value<std::uint8_t>(ifs)};
    while(block_size != 0) {
        insertion_index = compression_codes.size();
        compression_codes.resize(insertion_index + block_size);
        ifs.read(reinterpret_cast<char*>(&compression_codes[insertion_index]), block_size);
        if(not ifs.good()) throw Exception {Error::read_file_failed};
    
        block_size = read_little_endian_value<std::uint8_t>(ifs);
    }
    if(compression_codes.empty()) throw Exception {Error::bad_formed_file};

    return process_compression_codes(compression_codes, minimum_lzw_code_size, color_count);
}

std::vector<std::uint8_t> kak::impl::gif::process_compression_codes(const std::vector<std::uint8_t>& compression_codes, const int minimum_lzw_code_size, const int color_count)
{
    /* compression codes are just indexes into a
    * compression/decompression table (it's the same table for
    * compression and decompression). When decompressing, part of the
    * process is the reconstruction of the table that was built when
    * the data was being compressed.
    *
    * a table's entry contains either an index or a string of indexes
    * into a color table. The first entries of the decompression
    * table contain single indexes to the color table, for example,
    * imagine a color table with 256 entries, the decompression table
    * starts like this:
    *
    * decompression_table[0] = index to color_table[0]
    * decompression_table[1] = index to color_table[1]
    * decompression_table[2] = index to color_table[2]
    * ...
    * decompression_table[255] = index to color_table[255]
    *
    * the rest of the entries contain multiple indexes into the color
    * table. These entries are constructed/reconstructed according to
    * the GIF variation of the LZW decompression algorithm */

    std::vector<std::vector<std::uint8_t>> code_map; // the decompression table
    code_map.reserve(4096); // maximum number of entries

    for(int i = 0; i < color_count; ++i) {
        code_map.push_back(std::vector<std::uint8_t> {static_cast<std::uint8_t>(i)});
    }

    // things needed to process the compression codes
    const int clear_code {1 << minimum_lzw_code_size};
    const int end_code {clear_code + 1};
    code_map.push_back(std::vector<std::uint8_t> {static_cast<std::uint8_t>(clear_code)});
    code_map.push_back(std::vector<std::uint8_t> {static_cast<std::uint8_t>(end_code)});
    const int code_map_initial_size {color_count + 2};

    int previous_code {6699}; // impossible value for a code
    bool first_code {true};

    int current_lzw_code_size {minimum_lzw_code_size + 1};
    // 1 << some_number == std::pow(2, some_number)
    int current_max_representable_indexes {1 << current_lzw_code_size};

    std::vector<std::uint8_t> index_data;
    rez::impl::BitReader<rez::impl::BitStreamFormat::gif> bitstream {compression_codes};
    int code {bitstream.read_bits(current_lzw_code_size)};

    while(code != end_code) {
        int to_test = 0;
        if(code == clear_code) {
            // reset decompression state
            current_lzw_code_size = minimum_lzw_code_size + 1;
            current_max_representable_indexes = 1 << current_lzw_code_size;
            first_code = true;
            //previous_code = 6699; <- not needed in this codebase

            // erase elements beyond the initial ones
            auto erase_begin {code_map.begin()};
            erase_begin += code_map_initial_size;
            auto erase_end {code_map.end()};

            if(erase_begin != erase_end) {
                code_map.erase(erase_begin, erase_end);
            }

            // done resetting, get next code
            code = bitstream.read_bits(current_lzw_code_size);
            continue;
        }

        if(first_code) {
            /* the first code is a special case: the value of the
            * entry matching the code must be output without doing
            * anything else */

            if(code > color_count - 1) throw Exception {Error::bad_formed_file};
            const std::vector<std::uint8_t>& indexes {code_map[code]};

            index_data.push_back(indexes[0]);
            previous_code = code;

            first_code = false;
            code = bitstream.read_bits(current_lzw_code_size);
            continue;
        }

        // we know this compression code
        if(code < code_map.size()) {
            const std::vector<std::uint8_t>& indexes {code_map[code]};
            const int index_count {static_cast<int>(indexes.size())};

            for(int i = 0; i < index_count; ++i) {
                index_data.push_back(indexes[i]);
            }

            // check if the decompression table can have more entries
            if(code_map.size() != 4096) {
                std::vector<std::uint8_t> new_entry(code_map[previous_code]);
                new_entry.push_back(indexes[0]);
                code_map.push_back(std::move(new_entry));    
            }

            previous_code = code;
        }
        // we don't know this compression code
        else {
            const std::vector<std::uint8_t>& indexes {code_map[previous_code]};

            std::vector<std::uint8_t> new_entry(indexes);
            new_entry.push_back(indexes[0]);

            const int index_count {static_cast<int>(new_entry.size())};
            for(int i = 0; i < index_count; ++i) {
                index_data.push_back(new_entry[i]);
            }

            // check if the decompression table can have more entries
            if(code_map.size() != 4096) {
                code_map.push_back(std::move(new_entry));
                if(code != code_map.size() - 1) throw Exception {Error::bad_formed_file};
                previous_code = code;
            }
        }

        if(code_map.size() == current_max_representable_indexes) {
            ++current_lzw_code_size;
            if(current_lzw_code_size == 13) --current_lzw_code_size;
            current_max_representable_indexes = 1 << current_lzw_code_size;
        }

        code = bitstream.read_bits(current_lzw_code_size);
    }

    return index_data;
}

std::vector<std::uint8_t> kak::impl::gif::deinterlace(const std::vector<std::uint8_t>& image_data, const int image_width, const int image_height)
{
    if(image_height < 5) throw Exception {Error::bad_formed_file};

    std::vector<std::uint8_t> result(image_data.size());
    const std::uint8_t* const source = image_data.data();
    std::uint8_t* const dest = result.data();

    // GIF interlacing passes
    constexpr int row_start[4] {0,4,2,1};
    constexpr int row_steps[4] {8,8,4,2};

    int source_row = 0;
    for(int pass = 0; pass < 4; ++pass) {
        for(int dest_row = row_start[pass]; dest_row < image_height; dest_row += row_steps[pass]) {
            std::memcpy(dest + (dest_row * image_width), source + (source_row * image_width), image_width);
            ++source_row;
        }
    }

    return result;
}

std::vector<std::uint8_t> kak::impl::gif::depalettise(const std::vector<std::uint8_t>& image_data, const std::vector<std::uint8_t>& color_table)
{
    std::vector<std::uint8_t> pixel_data;
    pixel_data.reserve(image_data.size() * 3);

    for(const int index : image_data)
    {
        pixel_data.push_back(color_table[index * 3]);
        pixel_data.push_back(color_table[index * 3 + 1]);
        pixel_data.push_back(color_table[index * 3 + 2]);
    }

    return pixel_data;
}

std::vector<std::uint8_t> kak::impl::gif::depalettise(const std::vector<std::uint8_t>& image_data, const std::vector<std::uint8_t>& color_table, const int transparency_index)
{
    if(transparency_index * 3 > color_table.size() - 3) {
        throw Exception {Error::bad_formed_file};
    }
    
    std::vector<std::uint8_t> pixel_data;
    pixel_data.reserve(image_data.size() * 4);

    for(const int index : image_data)
    {
        if(index == transparency_index)
        {
            pixel_data.push_back(color_table[index * 3]);
            pixel_data.push_back(color_table[index * 3 + 1]);
            pixel_data.push_back(color_table[index * 3 + 2]);
            pixel_data.push_back(0);
            continue;
        }

        pixel_data.push_back(color_table[index * 3]);
        pixel_data.push_back(color_table[index * 3 + 1]);
        pixel_data.push_back(color_table[index * 3 + 2]);
        pixel_data.push_back(255);
    }

    return pixel_data;
}
