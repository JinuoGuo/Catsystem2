#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace catsystem2
{

using Bytes = std::vector<std::uint8_t>;

Bytes read_file(const std::filesystem::path& path);
void write_file(const std::filesystem::path& path, std::span<const std::uint8_t> data);

template <typename T>
concept LittleEndianInteger = std::is_integral_v<T> && std::is_unsigned_v<T>;

class ByteReader final
{
  public:
    explicit ByteReader(std::span<const std::uint8_t> data) : data_(data)
    {
    }

    [[nodiscard]] std::size_t position() const noexcept
    {
        return position_;
    }

    [[nodiscard]] std::size_t remaining() const noexcept
    {
        return data_.size() - position_;
    }

    void seek(std::size_t position);
    void skip(std::size_t count);
    std::span<const std::uint8_t> read_bytes(std::size_t count);

    template <LittleEndianInteger T> T read_le()
    {
        const auto bytes = read_bytes(sizeof(T));
        if constexpr(sizeof(T) == 1U)
        {
            return static_cast<T>(bytes[0]);
        }
        T value = 0;
        for(std::size_t i = 0; i < sizeof(T); ++i)
        {
            value = static_cast<T>(value | static_cast<T>(static_cast<T>(bytes[i]) << (i * 8U)));
        }
        return value;
    }

  private:
    std::span<const std::uint8_t> data_;
    std::size_t position_ = 0;
};

class ByteWriter final
{
  public:
    [[nodiscard]] const Bytes& data() const noexcept
    {
        return data_;
    }

    [[nodiscard]] Bytes take() && noexcept
    {
        return std::move(data_);
    }

    [[nodiscard]] std::size_t size() const noexcept
    {
        return data_.size();
    }

    void write_bytes(std::span<const std::uint8_t> bytes);
    void write_text(std::string_view text);

    template <LittleEndianInteger T> void write_le(T value)
    {
        for(std::size_t i = 0; i < sizeof(T); ++i)
        {
            data_.push_back(static_cast<std::uint8_t>((value >> (i * 8U)) & 0xffU));
        }
    }

    template <LittleEndianInteger T> void patch_le(std::size_t offset, T value)
    {
        if(offset > data_.size() || sizeof(T) > data_.size() - offset)
        {
            throw std::out_of_range("binary patch offset is outside the output buffer");
        }
        for(std::size_t i = 0; i < sizeof(T); ++i)
        {
            data_[offset + i] = static_cast<std::uint8_t>((value >> (i * 8U)) & 0xffU);
        }
    }

  private:
    Bytes data_;
};

}
