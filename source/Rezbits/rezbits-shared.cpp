#include "shared.hpp"
#include <sstream>

rez::Exception::Exception(Error error, std::source_location sl)
    : m_error(error), m_source_location(sl)
{
    std::ostringstream ostr;
    ostr << "REZBITS-EXCEPTION REPORT:\n"
        << "Function name: " << sl.function_name() << '\n'
        << "File name: " << sl.file_name() << '\n'
        << "Line: " << sl.line() << '\n'
        << "Column: " << sl.column() << '\n';

    m_message = ostr.str();
}

void rez::impl::ByteReader::plug_source(std::span<const std::uint8_t> source) noexcept
{
    m_source = source;
    m_source_size = static_cast<std::int64_t>(source.size());
    m_byte_index = 0;
}

std::uint8_t rez::impl::ByteReader::read_byte()
{
    if (m_byte_index + 1 > m_source_size) {
        throw Exception(Error::unexpected_eof);
    }

    return m_source[m_byte_index++];
}

std::span<const std::uint8_t> rez::impl::ByteReader::read_bytes(std::int32_t amount)
{
    if (m_byte_index + amount > m_source_size) {
        throw Exception(Error::unexpected_eof);
    }

    std::span<const std::uint8_t> result = m_source.subspan(m_byte_index, amount);
    m_byte_index += amount;
    return result;
}

void rez::impl::ByteReader::skip_bytes(std::int32_t amount)
{
    if (m_byte_index + amount > m_source_size) {
        throw Exception(Error::unexpected_eof);
    }

    m_byte_index += amount;
}

template<rez::impl::BitStreamFormat Format>
void rez::impl::BitReader<Format>::plug_source(std::span<const std::uint8_t> source) noexcept
{
    m_source = source;
    m_byte_index = 0;
    m_last_valid_index = static_cast<std::int64_t>(source.size()) - 1;
    m_bits_remaining = static_cast<std::int64_t>(source.size()) * 8;
    m_useful_bits_in_current_byte = 8;
}

template<rez::impl::BitStreamFormat Format>
void rez::impl::BitReader<Format>::rewind() noexcept
{
    m_byte_index = 0;
    m_bits_remaining = static_cast<std::int64_t>(m_source.size()) * 8;
    m_useful_bits_in_current_byte = 8;
}

template<rez::impl::BitStreamFormat Format>
std::int32_t rez::impl::BitReader<Format>::read_bits(int amount)
{
    if(amount < 1) return 0;

    int bits_taken {0};
    std::int32_t result {0};

    while(bits_taken < amount) {
        if(m_useful_bits_in_current_byte == 0) {
            // go to the next byte
            ++m_byte_index;
            if(m_byte_index > m_last_valid_index) {
                throw Exception {Error::unexpected_eof};
            }
            m_useful_bits_in_current_byte = 8;
        }

        int bits_to_take {amount - bits_taken};
        if(bits_to_take > m_useful_bits_in_current_byte) { bits_to_take = m_useful_bits_in_current_byte; }

        // to move outside/discard the bits that are not useful anymore
        const int discard_rotation {8 - m_useful_bits_in_current_byte};

        if constexpr (Format == BitStreamFormat::gif) {
            const int mask {(1 << bits_to_take) - 1};
            result |= ((m_source[m_byte_index] >> discard_rotation) & mask) << bits_taken;
        }
        else {
            const int dont_care_bits {8 - bits_to_take};
            // without the & 0xFF, the mask wouldn't be constrained to 8 bits
            const int mask {(0xFF << dont_care_bits) & 0xFF};
            int temp {(m_source[m_byte_index] << discard_rotation) & mask};
            temp >>= dont_care_bits;
            result = (result << bits_taken) | temp;
        }

        // book-keep
        bits_taken += bits_to_take;
        m_useful_bits_in_current_byte -= bits_to_take;
    }

    m_bits_remaining -= amount;
    return result;
}

template<rez::impl::BitStreamFormat Format>
std::int32_t rez::impl::BitReader<Format>::peek_bits(int amount)
{
    const std::int64_t copy1 {m_byte_index};
    const int copy2 {m_useful_bits_in_current_byte};
    const std::int64_t copy3 {m_bits_remaining};

    try {
        const std::int32_t result {read_bits(amount)};
        // "go back in time"
        m_byte_index = copy1;
        m_useful_bits_in_current_byte = copy2;
        m_bits_remaining = copy3;

        return result;
    }
    catch(const std::exception& e) {
        m_byte_index = copy1;
        m_useful_bits_in_current_byte = copy2;
        m_bits_remaining = copy3;
        throw;
    }
}

template<rez::impl::BitStreamFormat Format>
void rez::impl::BitReader<Format>::skip_bits(int amount)
{
    if(amount < 1) return;

    int bits_skipped {0};
    while(bits_skipped < amount) {
        if(m_useful_bits_in_current_byte == 0) {
            // go to the next byte
            ++m_byte_index;
            if(m_byte_index > m_last_valid_index) {
                throw Exception {Error::unexpected_eof};
            }
            m_useful_bits_in_current_byte = 8;
        }

        int bits_to_skip {amount - bits_skipped};
        if(bits_to_skip > m_useful_bits_in_current_byte) { bits_to_skip = m_useful_bits_in_current_byte; }

        // book-keep
        bits_skipped += bits_to_skip;
        m_useful_bits_in_current_byte -= bits_to_skip;
    }

    m_bits_remaining -= amount;
}

template<rez::impl::BitStreamFormat Format>
void rez::impl::BitReader<Format>::skip_until_next_byte_boundary()
{
    if(m_useful_bits_in_current_byte == 8) return;
    
    ++m_byte_index;
    if(m_byte_index > m_last_valid_index) {
        throw Exception {Error::unexpected_eof};
    }
    m_bits_remaining -= m_useful_bits_in_current_byte;
    m_useful_bits_in_current_byte = 8;
}

template<rez::impl::BitStreamFormat Format>
std::span<const std::uint8_t> rez::impl::BitReader<Format>::read_bytes(int amount)
{
    if(amount < 1) return {};

    if(m_byte_index + amount > m_source.size()) {
        throw Exception {Error::unexpected_eof};
    }

    std::span<const std::uint8_t> result = m_source.subspan(m_byte_index, amount);

    // book-keeping
    m_byte_index += amount;
    if(m_byte_index == m_source.size()) {
        m_useful_bits_in_current_byte = 0;
    }
    else { m_useful_bits_in_current_byte = 8; }

    while(m_bits_remaining % 8 != 0) {
        ++m_bits_remaining;
    }
    m_bits_remaining -= amount * 8;
    return result;
}

template class rez::impl::BitReader<rez::impl::BitStreamFormat::gif>;
template class rez::impl::BitReader<rez::impl::BitStreamFormat::jpg>;
