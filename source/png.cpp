#include "png.hpp"

#include <cmath>
#include <utility>
#include <cstring> // for memcpy

#include "Rezbits/deflate.hpp"
#include "Rezbits/adler32.hpp"

kak::Image kak::png::decode_image(const std::filesystem::path& filepath)
{
    using namespace kak::impl;
    using namespace kak::impl::png;

    std::ifstream png_file {filepath, std::ios_base::binary};
    if(not png_file.good()) throw Exception {Error::open_file_failed};

    read_and_verify_signature(png_file);

    // read the IHDR chunk
    verify_chunk_crc(read_big_endian_value<std::uint32_t>(png_file), png_file);
    read_and_verify_chunk_type(png_file, Chunk_type::IHDR);
    Ihdr ihdr;
    read_and_verify_chunk_data_ihdr(png_file, ihdr);
    png_file.ignore(4); // CRC already verified

    // what follows is a sequence of chunks until the file ends
    bool loop_should_continue {true};
    std::vector<std::uint8_t> image_data;

    // things that may appear in the PNG stream
    std::vector<std::uint8_t> palette;
    std::vector<std::uint8_t> trns; // will store the tRNS chunk's data

    // as said before: sequence of chunks until the file ends
    while(loop_should_continue) {
        std::uint32_t chunk_length_field {read_big_endian_value<std::uint32_t>(png_file)};
        verify_chunk_crc(chunk_length_field, png_file);

        // (┬┬﹏┬┬)
        const std::uint32_t chunk_type {read_big_endian_value<std::uint32_t>(png_file)};
        switch(static_cast<Chunk_type>(chunk_type)) {
            case Chunk_type::PLTE: {
                // PLTE is valid for color types 2 and 6 too
                switch(ihdr.color_type) {
                    case 2u: // Truecolor
                    case 3u: // Indexed-color
                    case 6u: // Truecolor with alpha
                        break;
                    default:
                        throw Exception {Error::bad_formed_file};
                }

                // PLTE data must be divisible by 3 (1 palette entry == 3 bytes)
                if(chunk_length_field % 3u != 0u) {
                    throw Exception {Error::bad_formed_file};
                }

                // 1 << some_number == std::pow(2, some_number)
                const std::uint32_t max_palette_size {(1u << ihdr.bit_depth) * 3u};
                /* the number of palette entries cannot exceed the range
                * that can be represented with the image bit depth */
                if(chunk_length_field > max_palette_size) {
                    throw Exception {Error::bad_formed_file};
                }

                palette.clear();
                palette.resize(chunk_length_field);
                png_file.read(reinterpret_cast<char*>(palette.data()), static_cast<std::streamsize>(chunk_length_field));
                break;
            }
            case Chunk_type::IDAT: {
                if(chunk_length_field == 0u) break;

                const std::size_t insert_position {image_data.size()};
                image_data.resize(image_data.size() + chunk_length_field);
                png_file.read(reinterpret_cast<char*>(&image_data[insert_position]), chunk_length_field);
                if(not png_file.good()) throw Exception {Error::read_file_failed};

                break;
            }
            case Chunk_type::IEND:
                loop_should_continue = false;
                break;
            case Chunk_type::acTL:
                png_file.ignore(static_cast<std::streamsize>(chunk_length_field));
                break;
            case Chunk_type::cHRM:
                png_file.ignore(static_cast<std::streamsize>(chunk_length_field));
                break;
            case Chunk_type::cICP:
                png_file.ignore(static_cast<std::streamsize>(chunk_length_field));
                break;
            case Chunk_type::gAMA:
                png_file.ignore(static_cast<std::streamsize>(chunk_length_field));
                break;
            case Chunk_type::iCCP:
                png_file.ignore(static_cast<std::streamsize>(chunk_length_field));
                break;
            case Chunk_type::mDCV:
                png_file.ignore(static_cast<std::streamsize>(chunk_length_field));
                break;
            case Chunk_type::cLLI:
                png_file.ignore(static_cast<std::streamsize>(chunk_length_field));
                break;
            case Chunk_type::sBIT:
                png_file.ignore(static_cast<std::streamsize>(chunk_length_field));
                break;
            case Chunk_type::sRGB:
                png_file.ignore(static_cast<std::streamsize>(chunk_length_field));
                break;
            case Chunk_type::bKGD:
                png_file.ignore(static_cast<std::streamsize>(chunk_length_field));
                break;
            case Chunk_type::hIST:
                png_file.ignore(static_cast<std::streamsize>(chunk_length_field));
                break;
            case Chunk_type::tRNS:
                switch(ihdr.color_type) {
                    case 0u: // Greyscale
                        if(chunk_length_field != 2u) throw Exception {Error::bad_formed_file};
                        break;
                    case 2u: // Truecolor
                        if(chunk_length_field != 6u) throw Exception {Error::bad_formed_file};
                        break;
                    case 3u: // Indexed-color
                        if(chunk_length_field == 0u) throw Exception {Error::bad_formed_file};
                        break;
                    default:
                        throw Exception {Error::bad_formed_file};
                }
                switch(ihdr.pixel_format) {
                    case PixelFormat::grey:
                        ihdr.pixel_format = PixelFormat::grey_alpha;
                        break;
                    case PixelFormat::rgb:
                        ihdr.pixel_format = PixelFormat::rgba;
                        break;
                    case PixelFormat::grey16:
                        ihdr.pixel_format = PixelFormat::grey_alpha16;
                        break;
                    case PixelFormat::rgb16:
                        ihdr.pixel_format = PixelFormat::rgba16;
                        break;
                    default:
                        throw Exception {Error::bad_formed_file};
                }
                trns.clear();
                trns.resize(chunk_length_field);
                png_file.read(reinterpret_cast<char*>(trns.data()), chunk_length_field);
                break;
            case Chunk_type::eXIf:
                png_file.ignore(static_cast<std::streamsize>(chunk_length_field));
                break;
            case Chunk_type::fcTL:
                png_file.ignore(static_cast<std::streamsize>(chunk_length_field));
                break;
            case Chunk_type::pHYs:
                png_file.ignore(static_cast<std::streamsize>(chunk_length_field));
                break;
            case Chunk_type::sPLT:
                png_file.ignore(static_cast<std::streamsize>(chunk_length_field));
                break;
            case Chunk_type::fdAT:
                png_file.ignore(static_cast<std::streamsize>(chunk_length_field));
                break;
            case Chunk_type::tIME:
                png_file.ignore(static_cast<std::streamsize>(chunk_length_field));
                break;
            case Chunk_type::iTXt:
                png_file.ignore(static_cast<std::streamsize>(chunk_length_field));
                break;
            case Chunk_type::tEXt:
                png_file.ignore(static_cast<std::streamsize>(chunk_length_field));
                break;
            case Chunk_type::zTXt:
                png_file.ignore(static_cast<std::streamsize>(chunk_length_field));
                break;
            default:
                // chunk_type & 0x20'00'00'00u == is the chunk ancillary?
                if(not (chunk_type & 0x20'00'00'00u)) {
                    throw Exception {Error::unsupported_feature};
                }
                png_file.ignore(static_cast<std::streamsize>(chunk_length_field));
                break;
        }
        png_file.ignore(4); // CRC already verified
    }

    image_data = decompress_image_data(image_data);
    // interlace_method can only be 0 (no interlace) or 1 (Adam7)
    if(ihdr.interlace_method) {
        image_data = unfilter_interlaced_image_data(image_data, ihdr);

        if(ihdr.bit_depth < 8u) {
            image_data = loosen_interlaced_image_data(image_data, ihdr);
        }

        image_data = deinterlace_image_data(image_data, ihdr);
    }
    else {
        image_data = unfilter_image_data(image_data, ihdr);

        if(ihdr.bit_depth < 8u) {
            image_data = loosen_image_data(image_data, ihdr);
        }
    }

    // Color type 3 == Indexed-color
    if(ihdr.color_type == 3u) {
        if(trns.empty()) {
            image_data = depalettise_image_data(image_data, palette);
        }
        else {
            image_data = depalettise_image_data(image_data, palette, trns);
        }
    }

    if(ihdr.bit_depth == 16u) {
        if constexpr(std::endian::native != std::endian::big) {
            image_data = to_little_endian_samples(image_data);
        }
    }

    /* start applying ancillary chunks here. I could have applied chunks
    * earlier, for example, I could have shoved the code to apply the
    * tRNS chunk inside the function to_little_endian_samples, but that
    * would make the codebase messy, plus it isn't worth it, many chunks
    * are rarely used, so it's better to keep the pipeline structure */

    // apply the tRNS chunk
    if(not trns.empty()) {
        switch(ihdr.color_type) {
            case 0u: // Greyscale
                image_data = apply_tRNS_chunk_to_greyscale(image_data, ihdr, trns);
                break;
            case 2u: // Truecolor
                image_data = apply_tRNS_chunk_to_truecolor(image_data, ihdr, trns);
                break;
        }    
    }

    Image result;
    result.pixels = std::move(image_data);
    result.pixel_format = ihdr.pixel_format;
    result.width = static_cast<int>(ihdr.width);
    result.height = static_cast<int>(ihdr.height);

    return result;
}

void kak::impl::png::verify_uint32_value(const std::uint32_t number)
{
    // the PNG specification defines the range of an uint32_t to be from 0 to 2147483647
    if(number > 2147483647u) throw Exception {Error::bad_formed_file};
}

void kak::impl::png::read_and_verify_signature(std::ifstream& ifs)
{
    const std::uint64_t signature {read_big_endian_value<std::uint64_t>(ifs)};
    if(signature != 0x89504E470D0A1A0Aull) throw Exception {Error::bad_formed_file};
}

void kak::impl::png::read_and_verify_chunk_type(std::ifstream& ifs, const Chunk_type type)
{
    const std::uint32_t chunk_type {read_big_endian_value<std::uint32_t>(ifs)};
    if(chunk_type != static_cast<std::uint32_t>(type)) throw Exception {Error::bad_formed_file};
}

void kak::impl::png::read_and_verify_chunk_data_ihdr(std::ifstream& ifs, Ihdr& ihdr)
{
    ihdr.width = read_big_endian_value<std::uint32_t>(ifs);
    ihdr.height = read_big_endian_value<std::uint32_t>(ifs);
    ihdr.bit_depth = read_big_endian_value<std::uint8_t>(ifs);
    ihdr.color_type = read_big_endian_value<std::uint8_t>(ifs);
    ihdr.compression_method = read_big_endian_value<std::uint8_t>(ifs);
    ihdr.filter_method = read_big_endian_value<std::uint8_t>(ifs);
    ihdr.interlace_method = read_big_endian_value<std::uint8_t>(ifs);

    /* verification */
    // verify width and height
    verify_uint32_value(ihdr.width);
    verify_uint32_value(ihdr.height);
    if(ihdr.width == 0u or ihdr.height == 0u) throw Exception {Error::bad_formed_file};

    // verify bit depth and color type
    static constexpr std::uint32_t valid_bit_depths[5] {1u, 2u, 4u, 8u, 16u};
    bool bit_depth_found {false};
    switch(ihdr.color_type) {
        case 0u: // Greyscale
            for(int i = 0; i < 5; ++i) {
                if(ihdr.bit_depth == valid_bit_depths[i]) {
                    bit_depth_found = true;
                    break;
                }
            }

            // take opportunity to initialise the helper data-members
            ihdr.bits_per_pixel = ihdr.bit_depth;
            ihdr.pixel_format = PixelFormat::grey;
            break;
        case 2u: // Truecolor
            if(ihdr.bit_depth == valid_bit_depths[3] or ihdr.bit_depth == valid_bit_depths[4]) {
                bit_depth_found = true;
            }

            // take opportunity to initialise the helper data-members
            ihdr.bits_per_pixel = ihdr.bit_depth * 3u;
            ihdr.pixel_format = PixelFormat::rgb;
            break;
        case 3u: // Indexed-color
            if(ihdr.bit_depth == valid_bit_depths[0] or ihdr.bit_depth == valid_bit_depths[1] or ihdr.bit_depth == valid_bit_depths[2] or ihdr.bit_depth == valid_bit_depths[3]) {
                bit_depth_found = true;
            }

            // take opportunity to initialise the helper data-members
            ihdr.bits_per_pixel = ihdr.bit_depth;
            ihdr.pixel_format = PixelFormat::rgb;
            break;
        case 4u: // Greyscale with alpha
            if(ihdr.bit_depth == valid_bit_depths[3] or ihdr.bit_depth == valid_bit_depths[4]) {
                bit_depth_found = true;
            }

            // take opportunity to initialise the helper data-members
            ihdr.bits_per_pixel = ihdr.bit_depth * 2u;
            ihdr.pixel_format = PixelFormat::grey_alpha;
            break;
        case 6u: // Truecolor with alpha
            if(ihdr.bit_depth == valid_bit_depths[3] or ihdr.bit_depth == valid_bit_depths[4]) {
                bit_depth_found = true;
            }

            // take opportunity to initialise the helper data-members
            ihdr.bits_per_pixel = ihdr.bit_depth * 4u;
            ihdr.pixel_format = PixelFormat::rgba;
            break;
        default:
            throw Exception {Error::bad_formed_file};
    }
    if(not bit_depth_found) throw Exception {Error::bad_formed_file};
    /* The below (some_variable + 7u) >> 3u is clever code for dividing
    * some_variable by 8 and (if necessary) rounding up */
    ihdr.bytes_per_pixel = (ihdr.bits_per_pixel + 7u) >> 3u;

    // verify compression method
    if(ihdr.compression_method != 0u) throw Exception {Error::bad_formed_file};

    // verify filter method
    if(ihdr.filter_method != 0u) throw Exception {Error::bad_formed_file};

    // verify interlace method
    switch(ihdr.interlace_method) {
        case 0u: break; // no interlace
        case 1u: break; // Adam7
        default: throw Exception {Error::bad_formed_file};
    }

    // lastly, finish the initialisation of ihdr.pixel_format
    if(ihdr.bit_depth == 16u) {
        switch(ihdr.pixel_format) {
            case PixelFormat::grey:
                ihdr.pixel_format = PixelFormat::grey16;
                break;
            case PixelFormat::rgb:
                ihdr.pixel_format = PixelFormat::rgb16;
                break;
            case PixelFormat::grey_alpha:
                ihdr.pixel_format = PixelFormat::grey_alpha16;
                break;
            case PixelFormat::rgba:
                ihdr.pixel_format = PixelFormat::rgba16;
                break;
        }
    }
}

std::vector<std::uint8_t> kak::impl::png::decompress_image_data(const std::vector<std::uint8_t>& image_data)
{
    /* image_data is a ZLIB stream */

    /*
    std::ofstream ofs {"C:/Users/Seleska/Documents/DEFLATE and ZLIB test suite/zlib-stream-4.bin", std::ios_base::binary};
    ofs.write(reinterpret_cast<const char*>(image_data.data()), image_data.size());
    ofs.close();
    */

    rez::impl::ByteReader bytestream {image_data};
    // CMF means Compression Method and flags
    const std::uint8_t cmf {bytestream.read_big_endian_value<std::uint8_t>()};
    const std::uint32_t compression_method {cmf & 0b00001111u};
    // the below != 8u means: if compression_method != DEFLATE
    if(compression_method != 8u) throw Exception {Error::bad_formed_file};

    /* when the compression method is DEFLATE (and it's always DEFLATE) then
    * compression_info (it's below) is "the base-2 logarithm of the LZ77 window
    * size, minus eight" or in other words: LZ77 window = 2^(compression_info + 8)
    * but I decided to not use the LZ77 window, because I think that no one does */
    const std::uint32_t compression_info {(cmf & 0b11110000u) >> 4u};
    // ZLIB specification doesn't allow compression_info to be more than 7
    if(compression_info > 7u) throw Exception {Error::bad_formed_file};

    const std::uint8_t flags {bytestream.read_big_endian_value<std::uint8_t>()};
    // ZLIB specification says so...
    if((cmf * 256u + flags) % 31u != 0u) throw Exception {Error::corrupted_file};
    // PNG specification doesn't allow the ZLIB stream to have a preset dictionary
    if(flags & 0b00100000u) throw Exception {Error::bad_formed_file};

    // decompress DEFLATE
    std::span<const std::uint8_t> deflate_stream {bytestream.read_bytes(image_data.size() - 6u)};

    /*
    ofs.open("C:/Users/Seleska/Documents/DEFLATE and ZLIB test suite/deflate-stream-4.bin", std::ios_base::binary);
    ofs.write(reinterpret_cast<const char*>(deflate_stream.data()), deflate_stream.size());
    ofs.close();
    */

    std::vector<std::uint8_t> inflated_stream(rez::decompress_deflate(deflate_stream));

    // verify ADLER32 checksum
    const std::uint32_t adler32_checksum {rez::adler32(inflated_stream)};
    const std::uint32_t original_adler32_checksum {bytestream.read_big_endian_value<std::uint32_t>()};
    if(adler32_checksum != original_adler32_checksum) {
        throw Exception {Error::corrupted_file};
    }

    return inflated_stream;
}

void kak::impl::png::unfilter_sub(std::span<const std::uint8_t> current_scanline, const std::uint32_t steps_for_left, std::span<std::uint8_t> output)
{
    std::uint8_t a {0u};
    for(std::size_t i = 0u; i < output.size(); ++i) {
        if(not (i < steps_for_left)) a = output[i - steps_for_left];
        output[i] = current_scanline[i] + a;
    }
}

void kak::impl::png::unfilter_up(std::span<const std::uint8_t> previous_scanline, std::span<const std::uint8_t> current_scanline, std::span<std::uint8_t> output)
{
    if(previous_scanline.empty()) {
        std::memcpy(&output[0], &current_scanline[0], output.size());
    }
    else {
        for(std::size_t i = 0u; i < output.size(); ++i) {
            output[i] = current_scanline[i] + previous_scanline[i];
        }
    }

}

void kak::impl::png::unfilter_average(std::span<const std::uint8_t> previous_scanline, std::span<const std::uint8_t> current_scanline, const std::uint32_t steps_for_left, std::span<std::uint8_t> output)
{
    std::uint8_t a {0u};

    if(previous_scanline.empty()) {
        for(std::size_t i = 0u; i < output.size(); ++i) {
            if(not (i < steps_for_left)) a = output[i - steps_for_left];
            output[i] = current_scanline[i] + (a >> 1u);
        }
    }
    else {
        for(std::size_t i = 0u; i < output.size(); ++i) {
            if(not (i < steps_for_left)) a = output[i - steps_for_left];
            output[i] = current_scanline[i] + ((a + previous_scanline[i]) >> 1u);
        }
    }
}

void kak::impl::png::unfilter_paeth(std::span<const std::uint8_t> previous_scanline, std::span<const std::uint8_t> current_scanline, const std::uint32_t steps_for_left, std::span<std::uint8_t> output)
{
    if(previous_scanline.empty()) {
        /* if there is no previous scanline, the paeth unfiltering
        * ends up being equal to the sub unfiltering */
        unfilter_sub(current_scanline, steps_for_left, output);
    }
    else {
        std::uint8_t a {0u};
        std::uint8_t c {0u};
        for(std::size_t i = 0u; i < output.size(); ++i) {
            if(not (i < steps_for_left)) {
                a = output[i - steps_for_left];
                c = previous_scanline[i - steps_for_left];
            }
            output[i] = current_scanline[i] + paeth_predictor(a, previous_scanline[i], c);
        }
    }
}

std::uint8_t kak::impl::png::paeth_predictor(const std::uint8_t a, const std::uint8_t b, const std::uint8_t c)
{
    const std::int32_t aa {static_cast<std::int32_t>(a)};
    const std::int32_t bb {static_cast<std::int32_t>(b)};
    const std::int32_t cc {static_cast<std::int32_t>(c)};

    const std::int32_t p {aa + bb - cc};
    const std::int32_t pa {std::abs(p - aa)};
    const std::int32_t pb {std::abs(p - bb)};
    const std::int32_t pc {std::abs(p - cc)};

    if(pa <= pb and pa <= pc) { return a; }
    else if(pb <= pc) { return b; }
    else { return c; }
}

std::vector<std::uint8_t> kak::impl::png::unfilter_image_data(const std::vector<std::uint8_t>& image_data, const Ihdr& ihdr)
{
    /* The below (some_variable + 7u) >> 3u is clever code for dividing
    * some_variable by 8 and (if necessary) rounding up.
    * bytes_per_scanline does not take into account the byte for the filter type. */
    const std::uint32_t bytes_per_scanline {(ihdr.bits_per_pixel * ihdr.width + 7u) >> 3u};
    std::vector<std::uint8_t> unfiltered_image_data;
    unfiltered_image_data.reserve(bytes_per_scanline * ihdr.height);

    std::span<const std::uint8_t> previous_scanline;
    std::size_t current_scanline_index {1u}; // start just after the filter type
    std::vector<std::uint8_t> unfiltered_scanline(bytes_per_scanline); // allocate once and reuse

    for(std::uint32_t i = 0u; i < ihdr.height; ++i) {
        std::span<const std::uint8_t> current_scanline {image_data.begin() + current_scanline_index, bytes_per_scanline};
        std::uint8_t filter_type {image_data[current_scanline_index - 1u]};
        switch(filter_type) {
            case 0u: // None
                /* I know I can avoid this memcpy, but I would have to
                * duplicate some of the code after the switch statement */
                std::memcpy(unfiltered_scanline.data(), current_scanline.data(), bytes_per_scanline);
                break;
            case 1u: // Sub
                unfilter_sub(current_scanline, ihdr.bytes_per_pixel, unfiltered_scanline);
                break;
            case 2u: // Up
                unfilter_up(previous_scanline, current_scanline, unfiltered_scanline);
                break;
            case 3u: // Average
                unfilter_average(previous_scanline, current_scanline, ihdr.bytes_per_pixel, unfiltered_scanline);
                break;
            case 4u: // Paeth
                unfilter_paeth(previous_scanline, current_scanline, ihdr.bytes_per_pixel, unfiltered_scanline);
                break;
            default:
                throw Exception {Error::bad_formed_file};
        }

        const std::size_t previous_scanline_offset = unfiltered_image_data.size();
        unfiltered_image_data.insert(unfiltered_image_data.end(), unfiltered_scanline.begin(), unfiltered_scanline.end());
        previous_scanline = std::span<const std::uint8_t> {unfiltered_image_data.begin() + previous_scanline_offset, bytes_per_scanline};
        current_scanline_index += bytes_per_scanline + 1u;
    }

    return unfiltered_image_data;
}

std::int32_t kak::impl::png::compute_reduced_image_dimension(const std::int32_t dimension, const std::int32_t adam7_start, const std::int32_t adam7_steps)
{
    /* compute the width or height of a reduced image in pixels.
    * To understand the math better, one must know the following:
    * for integer arithmetic: ceil(a / b) == (a + b - 1) / b
    * so the formula is just:
    * ceil((dimension - adam7_start) / adam7_steps) */
    return (dimension - adam7_start + adam7_steps - 1) / adam7_steps;
}

std::vector<std::uint8_t> kak::impl::png::unfilter_interlaced_image_data(const std::vector<std::uint8_t>& image_data, const Ihdr& ihdr)
{
    /* The below (some_variable + 7u) >> 3u is clever code for dividing
    * some_variable by 8 and (if necessary) rounding up.
    * bytes_per_scanline_whole_image does not take into account the byte for the filter type. */
    const std::uint32_t bytes_per_scanline_whole_image {(ihdr.bits_per_pixel * ihdr.width + 7u) >> 3u};

    std::vector<std::uint8_t> unfiltered_image_data;
    unfiltered_image_data.reserve(bytes_per_scanline_whole_image * ihdr.height);

    std::size_t current_scanline_index {1u}; // start just after the filter type

    for(int pass = 0; pass < 7; ++pass) {
        const std::int32_t reduced_image_width {compute_reduced_image_dimension(ihdr.width, adam7_column_start[pass], adam7_column_steps[pass])};
        const std::int32_t reduced_image_height {compute_reduced_image_dimension(ihdr.height, adam7_row_start[pass], adam7_row_steps[pass])};
        if(reduced_image_width < 1 or reduced_image_height < 1) continue;

        // bytes_per_scanline does not take into account the byte for the filter type
        const std::uint32_t bytes_per_scanline {(ihdr.bits_per_pixel * static_cast<std::uint32_t>(reduced_image_width) + 7u) >> 3u};

        std::span<const std::uint8_t> previous_scanline;
        std::vector<std::uint8_t> unfiltered_scanline(bytes_per_scanline); // allocate once and reuse

        for(std::int32_t i = 0; i < reduced_image_height; ++i) {
            std::span<const std::uint8_t> current_scanline {image_data.begin() + current_scanline_index, bytes_per_scanline};
            std::uint8_t filter_type {image_data[current_scanline_index - 1u]};
            switch(filter_type) {
                case 0u: // None
                    /* I know I can avoid this memcpy, but I would have to
                    * duplicate some of the code after the switch statement */
                    std::memcpy(unfiltered_scanline.data(), current_scanline.data(), bytes_per_scanline);
                    break;
                case 1u: // Sub
                    unfilter_sub(current_scanline, ihdr.bytes_per_pixel, unfiltered_scanline);
                    break;
                case 2u: // Up
                    unfilter_up(previous_scanline, current_scanline, unfiltered_scanline);
                    break;
                case 3u: // Average
                    unfilter_average(previous_scanline, current_scanline, ihdr.bytes_per_pixel, unfiltered_scanline);
                    break;
                case 4u: // Paeth
                    unfilter_paeth(previous_scanline, current_scanline, ihdr.bytes_per_pixel, unfiltered_scanline);
                    break;
                default:
                    throw Exception {Error::bad_formed_file};
            }

            const std::size_t previous_scanline_offset = unfiltered_image_data.size();
            unfiltered_image_data.insert(unfiltered_image_data.end(), unfiltered_scanline.begin(), unfiltered_scanline.end());
            previous_scanline = std::span<const std::uint8_t> {unfiltered_image_data.begin() + previous_scanline_offset, bytes_per_scanline};
            current_scanline_index += bytes_per_scanline + 1u;
        }
    }

    return unfiltered_image_data;
}

double kak::impl::png::compute_normalisation_multiplier(const int sample_bitdepth)
{
    /* this function is a helper to compute the multiplier needed
    * to map a sample value from its original range
    * [0, std::pow(2, sample_bitdepth) - 1]
    * to the usual 8-bits range [0, 255] */
    
    // 1 << some_number == std::pow(2, some_number)
    return 255.0 / static_cast<double>((1 << sample_bitdepth) - 1);
}

std::vector<std::uint8_t> kak::impl::png::loosen_image_data(const std::vector<std::uint8_t>& image_data, const Ihdr& ihdr)
{
    /* this function will be called only if the sample bit-depth is less than 8,
    * and due to that, we already know that the color type of the PNG image is
    * either Greyscale (color type 0) or Indexed-color (color type 3), and
    * thanks to that, we already know that "samples per pixel" and "bytes per
    * pixel" are both 1, so ihdr.width * ihdr.height is sufficient. */
    std::vector<std::uint8_t> loose_image_data;
    loose_image_data.reserve(ihdr.width * ihdr.height);

    const std::int32_t image_width {static_cast<std::int32_t>(ihdr.width)};
    const std::int32_t image_height {static_cast<std::int32_t>(ihdr.height)};

    const int samples_in_byte {static_cast<int>(8u / ihdr.bit_depth)};
    const std::uint32_t move_right_amount {8u - ihdr.bit_depth};
    const double normalisation_multiplier {(ihdr.color_type == 3u) ? 1.0 : compute_normalisation_multiplier(ihdr.bit_depth)};

    std::int32_t byte_index {-1};
    std::uint8_t byte;
    for(std::int32_t i = 0; i < image_height; ++i) {
        // width of the image == number of samples in the scanline
        for(std::int32_t j = 0; j < image_width; ++j) {
            if(j % samples_in_byte == 0) {
                ++byte_index;
                if(byte_index > image_data.size() - 1) throw Exception {Error::bad_formed_file};
                byte = image_data[byte_index];
            }

            double raw_sample {static_cast<double>(byte >> move_right_amount)};
            loose_image_data.push_back(static_cast<std::uint8_t>(raw_sample * normalisation_multiplier));
            byte <<= ihdr.bit_depth;
        }
    }

    return loose_image_data;
}

std::vector<std::uint8_t> kak::impl::png::loosen_interlaced_image_data(const std::vector<std::uint8_t>& image_data, const Ihdr& ihdr)
{
    /* this function will be called only if the sample bit-depth is less than 8,
    * and due to that, we already know that the color type of the PNG image is
    * either Greyscale (color type 0) or Indexed-color (color type 3), and
    * thanks to that, we already know that "samples per pixel" and "bytes per
    * pixel" are both 1, so ihdr.width * ihdr.height is sufficient. */
    std::vector<std::uint8_t> loose_image_data;
    loose_image_data.reserve(ihdr.width * ihdr.height);

    const int samples_in_byte {static_cast<int>(8u / ihdr.bit_depth)};
    const std::uint32_t move_right_amount {8u - ihdr.bit_depth};
    const double normalisation_multiplier {(ihdr.color_type == 3u) ? 1.0 : compute_normalisation_multiplier(ihdr.bit_depth)};

    std::int32_t byte_index {-1};
    for(int pass = 0; pass < 7; ++pass) {
        const std::int32_t reduced_image_width {compute_reduced_image_dimension(ihdr.width, adam7_column_start[pass], adam7_column_steps[pass])};
        const std::int32_t reduced_image_height {compute_reduced_image_dimension(ihdr.height, adam7_row_start[pass], adam7_row_steps[pass])};
        if(reduced_image_width < 1 or reduced_image_height < 1) continue;

        std::uint8_t byte;
        for(std::int32_t i = 0; i < reduced_image_height; ++i) {
            // reduced_image_width == number of samples in the scanline
            for(std::int32_t j = 0; j < reduced_image_width; ++j) {
                if(j % samples_in_byte == 0) {
                    ++byte_index;
                    if(byte_index > image_data.size() - 1) throw Exception {Error::bad_formed_file};
                    byte = image_data[byte_index];
                }
    
                double raw_sample {static_cast<double>(byte >> move_right_amount)};
                loose_image_data.push_back(static_cast<std::uint8_t>(raw_sample * normalisation_multiplier));
                byte <<= ihdr.bit_depth;
            }
        }
    }

    return loose_image_data;
}

std::vector<std::uint8_t> kak::impl::png::deinterlace_image_data(const std::vector<std::uint8_t>& image_data, const Ihdr& ihdr)
{
    std::vector<std::uint8_t> deinterlaced_image_data;
    deinterlaced_image_data.resize(ihdr.bytes_per_pixel * ihdr.width * ihdr.height);

    std::int32_t source_offset {0};
    for(int pass = 0; pass < 7; ++pass) {
        const std::int32_t reduced_image_width {compute_reduced_image_dimension(ihdr.width, adam7_column_start[pass], adam7_column_steps[pass])};
        const std::int32_t reduced_image_height {compute_reduced_image_dimension(ihdr.height, adam7_row_start[pass], adam7_row_steps[pass])};
        if(reduced_image_width < 1 or reduced_image_height < 1) continue;

        const std::int32_t bytes_per_scanline {static_cast<std::int32_t>(ihdr.bytes_per_pixel) * reduced_image_width};

        for(std::int32_t row = 0; row < reduced_image_height; ++row) {
            for(std::int32_t column = 0; column < reduced_image_width; ++column) {
                const uint8_t* copy_source = &image_data[source_offset + (row * bytes_per_scanline) + (column * ihdr.bytes_per_pixel)];

                // dest = destination
                const std::int32_t dest_row {adam7_row_start[pass] + row * adam7_row_steps[pass]};
                const std::int32_t dest_column {adam7_column_start[pass] + column * adam7_column_steps[pass]};
                const std::int32_t dest_index = (dest_row * ihdr.width + dest_column) * ihdr.bytes_per_pixel;
                uint8_t* copy_destination = &deinterlaced_image_data[dest_index];

                std::memcpy(copy_destination, copy_source, ihdr.bytes_per_pixel);
            }
        }

        /* this was my initial solution, it works, but I arrived at
        * a better one (the one above). I am leaving this here because
        * I am a bit proud of this (I'll delete this after the initial git commit)
        for(std::int32_t row = 0; row < reduced_image_height; ++row) {
            for(std::int32_t column = 0; column < reduced_image_width; ++column) {
                const uint8_t* copy_source = &image_data[source_offset + (row * bytes_per_scanline) + (column * ihdr.bytes_per_pixel)];

                // dest = destination
                const std::int32_t dest_row {(adam7_row_start[pass] * bytes_per_scanline_whole_image) + (row * adam7_row_steps[pass] * bytes_per_scanline_whole_image)};
                const std::int32_t dest_column {(adam7_column_start[pass] * static_cast<std::int32_t>(ihdr.bytes_per_pixel)) + (column * adam7_column_steps[pass] * static_cast<std::int32_t>(ihdr.bytes_per_pixel))};
                uint8_t* copy_destination = &deinterlaced_image_data[dest_row + dest_column];

                std::memcpy(copy_destination, copy_source, ihdr.bytes_per_pixel);
            }
        }
        */

        source_offset += bytes_per_scanline * reduced_image_height;
    }

    return deinterlaced_image_data;
}

std::vector<std::uint8_t> kak::impl::png::depalettise_image_data(const std::vector<std::uint8_t>& image_data, const std::vector<std::uint8_t>& palette)
{
    if(palette.empty()) throw Exception {Error::bad_formed_file};

    std::vector<std::uint8_t> depalettised_image_data;
    depalettised_image_data.reserve(image_data.size() * 3u);

    const std::int32_t last_valid_byte_index {static_cast<std::int32_t>(palette.size()) - 1};
    for(const std::uint8_t palette_index : image_data) {
        const std::int32_t byte_index {static_cast<std::int32_t>(palette_index) * 3};
        if(byte_index + 2 > last_valid_byte_index) throw Exception {Error::bad_formed_file};

        depalettised_image_data.push_back(palette[byte_index]);
        depalettised_image_data.push_back(palette[byte_index + 1]);
        depalettised_image_data.push_back(palette[byte_index + 2]);
    }

    return depalettised_image_data;
}

std::vector<std::uint8_t> kak::impl::png::depalettise_image_data(const std::vector<std::uint8_t>& image_data, const std::vector<std::uint8_t>& palette, std::vector<std::uint8_t>& trns)
{
    if(palette.empty()) throw Exception {Error::bad_formed_file};
    /* checking that trns isn't empty is done outside this function.
    * that is how we know that a tRNS chunk was read in the first place */

    std::vector<std::uint8_t> depalettised_image_data;
    depalettised_image_data.reserve(image_data.size() * 4u);

    trns.resize(256u, 255u);

    const std::int32_t last_valid_byte_index {static_cast<std::int32_t>(palette.size()) - 1};
    for(const std::uint8_t palette_index : image_data) {
        const std::int32_t byte_index {static_cast<std::int32_t>(palette_index) * 3};
        if(byte_index + 2 > last_valid_byte_index) throw Exception {Error::bad_formed_file};

        depalettised_image_data.push_back(palette[byte_index]);
        depalettised_image_data.push_back(palette[byte_index + 1]);
        depalettised_image_data.push_back(palette[byte_index + 2]);
        depalettised_image_data.push_back(trns[palette_index]);
    }

    return depalettised_image_data;
}

std::vector<std::uint8_t> kak::impl::png::to_little_endian_samples(const std::vector<std::uint8_t>& image_data)
{
    if(image_data.size() % 2u != 0u) throw Exception {Error::bad_formed_file};

    std::vector<std::uint8_t> converted_image_data;
    converted_image_data.reserve(image_data.size());

    for(std::size_t i = 0u; i < image_data.size(); i += 2u) {
        converted_image_data.push_back(image_data[i + 1u]);
        converted_image_data.push_back(image_data[i]);
    }

    return converted_image_data;
}

std::vector<std::uint8_t> kak::impl::png::apply_tRNS_chunk_to_greyscale(const std::vector<std::uint8_t>& image_data, const Ihdr& ihdr, const std::vector<std::uint8_t>& trns)
{
    std::vector<std::uint8_t> transformed_image_data;
    transformed_image_data.reserve(image_data.size() * 2u);

    if(ihdr.bit_depth == 16u) {
        if(image_data.size() % 2u != 0u) throw Exception {Error::bad_formed_file};

        std::uint16_t transparency_value {trns[1]};
        transparency_value |= (trns[0] << 8u);

        for(std::size_t i = 0u; i < image_data.size(); i += 2u) {
            // copy the values in the order already given
            transformed_image_data.push_back(image_data[i]);
            transformed_image_data.push_back(image_data[i + 1]);
            
            std::uint16_t sample {0u};
            std::memcpy(&sample, &image_data[i], 2u); // the endianness is already taken care of

            if(sample == transparency_value) {
                transformed_image_data.push_back(0u);
                transformed_image_data.push_back(0u);
            }
            else {
                if constexpr(std::endian::native == std::endian::big) {
                    transformed_image_data.push_back(0u);
                    transformed_image_data.push_back(255u);
                }
                else {
                    transformed_image_data.push_back(255u);
                    transformed_image_data.push_back(0u);
                }
            }
        }
    }
    else {
        std::uint8_t transparency_value {trns[1]};
        if(ihdr.bit_depth < 8u) {
            const double normalisation_multiplier {compute_normalisation_multiplier(ihdr.bit_depth)};
            transparency_value = static_cast<std::uint8_t>(static_cast<double>(transparency_value) * normalisation_multiplier);
        }

        for(const std::uint8_t sample : image_data) {
            transformed_image_data.push_back(sample);
            if(sample == transparency_value) { transformed_image_data.push_back(0u); }
            else { transformed_image_data.push_back(255u); }
        }
    }

    return transformed_image_data;
}

std::vector<std::uint8_t> kak::impl::png::apply_tRNS_chunk_to_truecolor(const std::vector<std::uint8_t>& image_data, const Ihdr& ihdr, const std::vector<std::uint8_t>& trns)
{
    std::vector<std::uint8_t> transformed_image_data;
    const std::int32_t rgb_container_size {static_cast<std::int32_t>(image_data.size())};

    if(ihdr.bit_depth == 16u) {
        if(rgb_container_size % 6 != 0) throw Exception {Error::bad_formed_file};
        // reserve enough to contain the RGBA pixel data
        transformed_image_data.reserve(rgb_container_size + 2 * (rgb_container_size / 6));

        const std::uint16_t transparent_red_sample {static_cast<std::uint16_t>(trns[1] | (trns[0] << 8u))};
        const std::uint16_t transparent_green_sample {static_cast<std::uint16_t>(trns[3] | (trns[2] << 8u))};
        const std::uint16_t transparent_blue_sample {static_cast<std::uint16_t>(trns[5] | (trns[4] << 8u))};

        for(std::int32_t i = 0; i < rgb_container_size; i += 6) {
            // copy the values in the order already given (should I use a memcpy approach?)
            transformed_image_data.push_back(image_data[i]);
            transformed_image_data.push_back(image_data[i + 1]);
            transformed_image_data.push_back(image_data[i + 2]);
            transformed_image_data.push_back(image_data[i + 3]);
            transformed_image_data.push_back(image_data[i + 4]);
            transformed_image_data.push_back(image_data[i + 5]);

            std::uint16_t red_sample {0u};
            std::uint16_t green_sample {0u};
            std::uint16_t blue_sample {0u};
            // the endianness is already taken care of
            std::memcpy(&red_sample, &image_data[i], 2u);
            std::memcpy(&green_sample, &image_data[i + 2], 2u);
            std::memcpy(&blue_sample, &image_data[i + 4], 2u);

            if(red_sample == transparent_red_sample and green_sample == transparent_green_sample and blue_sample == transparent_blue_sample) {
                transformed_image_data.push_back(0u);
                transformed_image_data.push_back(0u);
            }
            else {
                if constexpr(std::endian::native == std::endian::big) {
                    transformed_image_data.push_back(0u);
                    transformed_image_data.push_back(255u);
                }
                else {
                    transformed_image_data.push_back(255u);
                    transformed_image_data.push_back(0u);
                }
            }
        }
    }
    else {
        if(rgb_container_size % 3 != 0) throw Exception {Error::bad_formed_file};
        // reserve enough to contain the RGBA pixel data
        transformed_image_data.reserve(rgb_container_size + (rgb_container_size / 3));

        std::uint8_t transparent_red_sample {trns[1]};
        std::uint8_t transparent_green_sample {trns[3]};
        std::uint8_t transparent_blue_sample {trns[5]};
        if(ihdr.bit_depth < 8u) {
            const double normalisation_multiplier {compute_normalisation_multiplier(ihdr.bit_depth)};
            transparent_red_sample = static_cast<std::uint8_t>(static_cast<double>(transparent_red_sample) * normalisation_multiplier);
            transparent_green_sample = static_cast<std::uint8_t>(static_cast<double>(transparent_green_sample) * normalisation_multiplier);
            transparent_blue_sample = static_cast<std::uint8_t>(static_cast<double>(transparent_blue_sample) * normalisation_multiplier);
        }

        for(std::int32_t i = 0; i < rgb_container_size; i += 3) {
            const std::uint8_t red_sample {image_data[i]};
            const std::uint8_t green_sample {image_data[i + 1]};
            const std::uint8_t blue_sample {image_data[i + 2]};

            // copy the values in the order already given
            transformed_image_data.push_back(red_sample);
            transformed_image_data.push_back(green_sample);
            transformed_image_data.push_back(blue_sample);

            if(red_sample == transparent_red_sample and green_sample == transparent_green_sample and blue_sample == transparent_blue_sample) {
                transformed_image_data.push_back(0u);
            }
            else { transformed_image_data.push_back(255u); }
        }
    }

    return transformed_image_data;
}

void kak::impl::png::verify_chunk_crc(const std::uint32_t chunk_length_field, std::ifstream& data_source)
{
    verify_uint32_value(chunk_length_field);

    /* the layout of a chunk is as follows:
    * Length (4 bytes),
    * Chunk Type (4 bytes),
    * Chunk Data (amount of bytes == value of Length)
    * CRC (4 bytes)
    */
    // the "4u" is the Chunk Type field
    const std::vector<std::uint8_t> bytes = read_bytes(data_source, static_cast<std::streamsize>(4u + chunk_length_field));

    // CRC-32 computation
    std::uint32_t crc = 0xFFFFFFFFu;
    for(const std::uint8_t byte : bytes) {
        crc = crc32_table[(crc ^ byte) & 0xFFu] ^ (crc >> 8u);
    }
    crc ^= 0xFFFFFFFFu;

    const std::uint32_t in_file_crc {read_big_endian_value<std::uint32_t>(data_source)};
    if(in_file_crc != crc) throw Exception {Error::corrupted_file};

    // the "8u" is the Chunk Type field + the CRC field
    data_source.seekg(static_cast<std::streamoff>(8u + chunk_length_field) * -1, std::ios_base::cur);
}
