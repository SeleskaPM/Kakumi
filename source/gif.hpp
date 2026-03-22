#pragma once

#include "shared.hpp"

#include <filesystem>

namespace kak::gif {
    Image decode_image(const std::filesystem::path& filepath);

    struct Frame {
        std::vector<std::uint8_t> data; // pixel data
        PixelFormat pixel_format {PixelFormat::undefined};
        int left {0};
        int top {0};
        int width {0};
        int height {0};

        int disposal_method {0};
        std::uint16_t delay_time {0};
        bool user_input_flag {false};
    };

    struct Gif {
        std::vector<Frame> frames;
        int background_width {0};
        int background_height {0};
        std::uint8_t background_color_red {0};
        std::uint8_t background_color_green {0};
        std::uint8_t background_color_blue {0};
    };

    Gif decode(const std::filesystem::path& filepath);
}

namespace kak::impl::gif {
    /* The commented-out structures are a reference of the order in
    * which the information/bytes appear inside a GIF file according
    * to the GIF specification. To optimize the size of the
    * structures that will be compiled, the data-members are declared
    * from biggest to smaller */

    /*
    struct Logical_screen_descriptor {
        uint16 width;
        uint16 height;
        uint8 packed_fields;
            bool gctf; // global color table flag
            uint8 color_resolution;
            bool sort_flag;
            uint8 gcts; // global color table size
        uint8 bg_color_index;
        uint8 pixel_aspect_ratio;
    };
    */

    struct Logical_screen_descriptor {
        int width;
        int height;
        int color_resolution;
        int size_of_global_color_table; // number of entries, not size in bytes
        int background_color_index;
        int pixel_aspect_ratio;
        bool global_color_table_flag;
        bool sort_flag;
    };

    /*
    struct Graphic_control_extension {
        uint8 packed_fields;
            uint8 disposal_method;
            bool user_input_flag;
            bool transparent_color_flag;
        uint16 delay_time;
        uint8 transparent_color_index;
    };
    */

    struct Graphic_control_extension {
        int disposal_method;
        int transparent_color_index;
        std::uint16_t delay_time;
        bool user_input_flag;
        bool transparent_color_flag;
    };

    /*
    struct Image_descriptor {
        uint16 left;
        uint16 top;
        uint16 width;
        uint16 height;
        uint8 packed_fields;
            bool local_color_table_flag;
            bool interlace_flag;
            bool sort_flag;
            uint8 reserved;
            uint8 size_of_local_color_table;
    };
    */

    struct Image_descriptor {
        int left;
        int top;
        int width;
        int height;
        int size_of_local_color_table; // number of entries, not size in bytes
        bool local_color_table_flag;
        bool interlace_flag;
        bool sort_flag;
    };

    /*
    struct Plain_text_extension {
        uint16 left;
        uint16 top;
        uint16 width;
        uint16 height;
        uint8 cell_width;
        uint8 cell_height;
        uint8 fg_color_index;
        uint8 bg_color_index;
        std::string text;
    };
    */

    /*
    struct Comment_extension {
        std::string comment_data;
    };
    */

    /*
    struct Application_extension {
        char application_identifier[8];
        uint8 application_auth_code[3];
        std::vector<uint8_t> application_data;
    };
    */

    void read_signature(std::ifstream& ifs);
    bool read_version_and_check_if_87a(std::ifstream& ifs);

    void read_logical_screen_descriptor(std::ifstream& ifs, Logical_screen_descriptor& lsd);
    void read_image_descriptor(std::ifstream& ifs, Image_descriptor& descriptor);
    void read_graphic_control_extension(std::ifstream& ifs, Graphic_control_extension& gce);

    std::vector<std::uint8_t> decompress_image_data(std::ifstream& ifs, const int color_count);
    std::vector<std::uint8_t> process_compression_codes(const std::vector<std::uint8_t>& compression_codes, const int minimum_lzw_code_size, const int color_count);

    std::vector<std::uint8_t> deinterlace(const std::vector<std::uint8_t>& image_data, const int image_width, const int image_height);

    std::vector<std::uint8_t> depalettise(const std::vector<std::uint8_t>& image_data, const std::vector<std::uint8_t>& color_table);
    std::vector<std::uint8_t> depalettise(const std::vector<std::uint8_t>& image_data, const std::vector<std::uint8_t>& color_table, const int transparency_index);
}
