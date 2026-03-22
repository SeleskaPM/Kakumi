# Kakumi
Kakumi is an image decoding library. It currently supports the
following formats:

- PNG (able to correctly decode most images in the internet.
Does not support APNG yet)
- GIF (ignores the Plain Text Extension)
- TGA (ignores the TGA Footer)
- BMP (only BITMAPCOREHEADER for now)

Kakumi only has one dependency: the C++ library
[Rezbits](https://github.com/SeleskaPM/Rezbits),
which does not have any dependencies.

## Usage
This is an `#include` only library. The idea is for you to copy
the source folder, rename it to Kakumi, and add it to your
project. Then you would include the header files that you want
like this: #include "Kakumi/png.hpp".

Kakumi has two public namespaces: `kak` and `kak::SOMETHING`.
The "SOMETHING" is the image format, for example: `kak::png`,
`kak::gif`, etc. Inside those "SOMETHING" namespaces exists a
function called `decode_image`, for example:
`kak::png::decode_image`. That function will decode a static
image (as opposed to animated/multiple frames), for example,
if you call `kak::gif::decode_image` you will get only the
first frame of the GIF file.

For the TGA and BMP formats `decode_image` is all that's needed
and the same applies for most PNG files (a .png file can still
be an animated PNG) and GIF files that aren't animations.
`decode_image` returns an `kak::Image` struct instance and its
data-members are as follows:

```
struct Image {
    std::vector<uint8_t> pixels;
    PixelFormat pixel_format;
    int32_t width; // in pixels
    int32_t height; // in pixels
    bool premultiplied_alpha;
};
```

`PixelFormat` is an `enum class` and you can check its values
in the `shared.hpp` header file.

Sometimes you want more than just a static image, what about an
animated GIF file? In those cases (cases where `kak::Image` is
not enough), you will find a struct named like its namespace,
for example, the `Gif` struct: `kak::gif::Gif`. That struct is
defined as follows:

```
struct Frame {
    std::vector<uint8_t> data; // pixel data
    PixelFormat pixel_format;
    int32_t left; // coordinate in pixels
    int32_t top; // coordinate in pixels
    int32_t width; // width in pixels
    int32_t height; // height in pixels

    int32_t disposal_method;
    uint16_t delay_time;
    bool user_input_flag;
};

struct Gif {
    std::vector<Frame> frames;
    int32_t background_width; // in pixels
    int32_t background_height; // in pixels
    uint8_t background_color_red;
    uint8_t background_color_green;
    uint8_t background_color_blue;
};
```

As you can see, the idea is for you to have what's needed to
display the contents of a file properly if you want more than
`kak::Image`. The functions that "give more than `kak::Image`"
are named simply `decode` and live inside their respective
namespaces, for example: `kak::gif::decode`.

If something goes wrong, Kakumi will throw an exception of type
`class Exception : public std::exception`. `Exception` is
defined in the `kak` namespace and it overrides
`std::exception::what`. The information it provides tells you
exactly what went wrong precisely because this is an `#include`
only library: it leverages C++20's `std::source_location`. You
can also use the `kak::Exception` class and it also provides
the `error` member-function which returns an instance of the
enum class `kak::Error` if you would like to be more specific
towards the user (for example:
`kak::Error::unsupported_feature`). You can see all of this in
`shared.hpp`.
