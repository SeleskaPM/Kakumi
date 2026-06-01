#include "shared.hpp"
#include <sstream>

kak::Exception::Exception(const Error error, std::source_location sl)
    : m_error(error), m_source_location(sl)
{
    std::ostringstream ostr;
    ostr << "KAKUMI-EXCEPTION REPORT:\n"
        << "Function name: " << sl.function_name() << '\n'
        << "File name: " << sl.file_name() << '\n'
        << "Line: " << sl.line() << '\n'
        << "Column: " << sl.column() << '\n';

    m_message = ostr.str();
}

std::uint8_t kak::impl::read_byte(std::ifstream& source)
{
    uint8_t byte = static_cast<uint8_t>(source.get());
    if (not source.good()) throw Exception(Error::read_file_failed);
    return byte;
}

std::vector<std::uint8_t> kak::impl::read_bytes(std::ifstream& source, std::streamsize amount)
{
    std::vector<uint8_t> buffer(amount);
    source.read(reinterpret_cast<char*>(buffer.data()), amount);
    if (not source.good()) throw Exception(Error::read_file_failed);
    return buffer;
}

void kak::impl::read_bytes_into(std::ifstream& source, std::span<uint8_t> dest)
{
    source.read(reinterpret_cast<char*>(dest.data()), static_cast<std::streamsize>(dest.size()));
    if (not source.good()) throw Exception(Error::read_file_failed);
}

void kak::impl::read_bytes_into(std::ifstream& source, std::span<uint8_t> dest, std::streamsize amount)
{
    source.read(reinterpret_cast<char*>(dest.data()), amount);
    if (not source.good()) throw Exception(Error::read_file_failed);
}

void kak::impl::skip_bytes(std::ifstream& source, std::streamsize amount)
{
    source.ignore(amount);
    if (not source.good()) throw Exception(Error::read_file_failed);
}

void kak::impl::validate_image_dimensions(int32_t width, int32_t height)
{
    const bool above_zero = width > 0 && height > 0;
    const bool within_limit = width <= max_pixel_dimension && height <= max_pixel_dimension;
    if (not (above_zero && within_limit)) {
        throw Exception(Error::unsupported_feature);
    }
}

double kak::impl::compute_multiplier_to_8bits_range(int sample_bitdepth)
{
    /* this function is a helper that computes the multiplier
    * needed to map a sample value from its original range
    * [0, std::pow(2, sample_bitdepth) - 1]
    * to the usual 8-bits range [0, 255] */

    // 1 << some_number == std::pow(2, some_number)
    return 255.0 / static_cast<double>((1 << sample_bitdepth) - 1);
}
