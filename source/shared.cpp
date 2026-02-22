#include "shared.hpp"
#include <sstream>

kak::Exception::Exception(const Error error, std::source_location sl)
    : m_error {error}, m_source_location {sl}
{
    std::ostringstream ostr;
    ostr << "KAKUMI-EXCEPTION REPORT:\n"
        << "Function name: " << sl.function_name() << '\n'
        << "File name: " << sl.file_name() << '\n'
        << "Line: " << sl.line() << '\n'
        << "Column: " << sl.column() << '\n';

    m_message = ostr.str();
}

std::uint8_t kak::impl::get_byte(std::ifstream& source)
{
    uint8_t byte {static_cast<uint8_t>(source.get())};
    if(not source.good()) throw Exception {Error::read_file_failed};
    return byte;
}

std::vector<std::uint8_t> kak::impl::get_bytes(std::ifstream& source, std::streamsize amount)
{
    std::vector<uint8_t> buffer(amount);
    source.read(reinterpret_cast<char*>(buffer.data()), amount);
    if(not source.good()) throw Exception {Error::read_file_failed};
    return buffer;
}

void kak::impl::check_against_max_dimension(int32_t width, int32_t height)
{
    if(width > max_pixel_dimension || height > max_pixel_dimension) {
        throw Exception {Error::unsupported_feature};
    }
}
