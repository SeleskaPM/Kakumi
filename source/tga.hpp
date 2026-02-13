#include "shared.hpp"

#include <filesystem>
#include <span>

namespace kak::tga {
    Image decode_image(const std::filesystem::path& filepath);
}

namespace kak::impl::tga {
    /* The commented-out structures are a reference of the order in
    * which the information/bytes appear inside a TGA file according
    * to the TGA 2.0 specification.
    *
    * You will see that the structures that are used in the code, do
    * not incorporate all the data-members that their commented out
    * counterpart have. This is because those data-members are
    * useless in general */

    /*
    struct Header {
        std::uint8_t id_length;
        std::uint8_t color_map_type;
        std::uint8_t image_type;

        // Color Map Specification
        std::uint16_t field_entry_index;
        std::uint16_t color_map_length;
        std::uint8_t color_map_entry_size;

        // Image Specification
        std::uint16_t x_origin_of_image;
        std::uint16_t y_origin_of_image;

        std::uint16_t image_width;
        std::uint16_t image_height;
        
        std::uint8_t pixel_depth;
        std::uint8_t image_descriptor;
    };
    */

    struct Header {
        int id_length;
        int color_map_type;
        int image_type;

        int color_map_entry_size;

        int image_width;
        int image_height;

        /* for color-mapped images, Pixel Depth indicates
        * the number of bits per color-map index */
        int pixel_depth;
        int image_descriptor;

        /* in the TGA 2.0 specification, there is a field
        * named Field Entry Index and it seems that its
        * original name is Color Map Origin. I prefer the
        * original name because it is consistent with the
        * names of the other related fields */
        std::uint16_t color_map_origin;
        std::uint16_t color_map_length;
    };

    void read_header(std::ifstream& ifs, Header& header);

    std::vector<std::uint8_t> read_color_map(std::ifstream& ifs, const Header& header);
    std::int32_t read_color_map_index_1byte(std::ifstream& ifs);
    std::int32_t read_color_map_index_2bytes(std::ifstream& ifs);

    void read_rgb_5bits(std::vector<std::uint8_t>& image_data, std::ifstream& ifs);
    void read_rgb_5bits(std::vector<std::uint8_t>& image_data, std::span<const std::uint8_t> color_map, const std::int32_t index);

    using Read_image_data_func = std::vector<std::uint8_t>(*)(std::ifstream&, const Header&);

    std::vector<std::uint8_t> read_image_area(std::ifstream& ifs, const Header& header, Read_image_data_func read_image_data_func);
    std::vector<std::uint8_t> read_image_area_mapped(std::ifstream& ifs, const Header& header);
    std::vector<std::uint8_t> read_image_area_mapped_compressed(std::ifstream& ifs, const Header& header);

    std::vector<std::uint8_t> read_image_data_truecolor(std::ifstream& ifs, const Header& header);
    std::vector<std::uint8_t> read_image_data_truecolor_compressed(std::ifstream& ifs, const Header& header);
    std::vector<std::uint8_t> read_image_data_greyscale(std::ifstream& ifs, const Header& header);
    std::vector<std::uint8_t> read_image_data_greyscale_compressed(std::ifstream& ifs, const Header& header);
}