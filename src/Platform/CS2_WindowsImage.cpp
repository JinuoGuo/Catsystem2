#include "catsystem2/CS2_Image.h"

#include "catsystem2/CS2_Error.h"

#include <objbase.h>
#include <wincodec.h>
#include <windows.h>

#include <algorithm>
#include <limits>
#include <string>
#include <utility>

namespace catsystem2
{
namespace
{

void check_hresult(const HRESULT result, const char* operation)
{
    if(FAILED(result))
    {
        throw Error(std::string(operation) + " failed (HRESULT " + std::to_string(static_cast<unsigned long>(result)) + ")");
    }
}

template <typename T> class ComPtr final
{
  public:
    ComPtr() = default;
    ~ComPtr()
    {
        reset();
    }
    ComPtr(const ComPtr&) = delete;
    ComPtr& operator=(const ComPtr&) = delete;
    ComPtr(ComPtr&& other) noexcept : pointer_(std::exchange(other.pointer_, nullptr))
    {
    }
    ComPtr& operator=(ComPtr&& other) noexcept
    {
        if(this != &other)
        {
            reset();
            pointer_ = std::exchange(other.pointer_, nullptr);
        }
        return *this;
    }

    [[nodiscard]] T* get() const noexcept
    {
        return pointer_;
    }

    [[nodiscard]] T* operator->() const noexcept
    {
        return pointer_;
    }
    T** put() noexcept
    {
        reset();
        return &pointer_;
    }

  private:
    void reset() noexcept
    {
        if(pointer_ != nullptr)
        {
            pointer_->Release();
            pointer_ = nullptr;
        }
    }
    T* pointer_ = nullptr;
};

class ComApartment final
{
  public:
    ComApartment()
    {
        const HRESULT result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if(result == RPC_E_CHANGED_MODE)
        {
            return;
        }
        check_hresult(result, "COM initialization");
        initialized_ = true;
    }
    ~ComApartment()
    {
        if(initialized_)
        {
            CoUninitialize();
        }
    }
    ComApartment(const ComApartment&) = delete;
    ComApartment& operator=(const ComApartment&) = delete;

  private:
    bool initialized_ = false;
};

ComPtr<IWICImagingFactory> make_factory()
{
    ComPtr<IWICImagingFactory> factory;
    check_hresult(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_IWICImagingFactory,
                                   reinterpret_cast<void**>(factory.put())),
                  "WIC factory creation");
    return factory;
}

std::size_t checked_pixel_bytes(const std::uint32_t width, const std::uint32_t height)
{
    constexpr std::size_t channels = 4;
    if(width == 0 || height == 0 || static_cast<std::size_t>(width) > std::numeric_limits<std::size_t>::max() / channels / height)
    {
        throw Error("invalid or unsupported image dimensions");
    }
    return static_cast<std::size_t>(width) * height * channels;
}

Image decode_frame(IWICImagingFactory* factory, IWICBitmapFrameDecode* frame)
{
    UINT width = 0;
    UINT height = 0;
    check_hresult(frame->GetSize(&width, &height), "reading image dimensions");
    const std::size_t byte_count = checked_pixel_bytes(width, height);
    if(byte_count > std::numeric_limits<UINT>::max())
    {
        throw Error("image is too large for WIC");
    }

    ComPtr<IWICFormatConverter> converter;
    check_hresult(factory->CreateFormatConverter(converter.put()), "WIC format converter creation");
    check_hresult(
        converter->Initialize(frame, GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom),
        "converting image to BGRA");

    Bytes top_down(byte_count);
    const UINT stride = width * 4U;
    check_hresult(converter->CopyPixels(nullptr, stride, static_cast<UINT>(top_down.size()), top_down.data()), "decoding image pixels");

    Image image{width, height, Bytes(byte_count)};
    const std::size_t row_size = stride;
    for(std::uint32_t row = 0; row < height; ++row)
    {
        const auto source = top_down.begin() + static_cast<std::ptrdiff_t>(row * row_size);
        const auto destination = image.bgra.begin() + static_cast<std::ptrdiff_t>((height - row - 1U) * row_size);
        std::copy_n(source, static_cast<std::ptrdiff_t>(row_size), destination);
    }
    return image;
}

}

Image decode_image_file(const std::filesystem::path& path)
{
    ComApartment apartment;
    auto factory = make_factory();
    ComPtr<IWICBitmapDecoder> decoder;
    check_hresult(factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, decoder.put()),
                  "opening image file");
    ComPtr<IWICBitmapFrameDecode> frame;
    check_hresult(decoder->GetFrame(0, frame.put()), "reading image frame");
    return decode_frame(factory.get(), frame.get());
}

Image decode_jpeg(const std::span<const std::uint8_t> encoded, const std::span<const std::uint8_t> alpha)
{
    if(encoded.empty() || encoded.size() > std::numeric_limits<DWORD>::max())
    {
        throw Error("invalid JPEG data size");
    }
    ComApartment apartment;
    auto factory = make_factory();
    ComPtr<IWICStream> stream;
    check_hresult(factory->CreateStream(stream.put()), "WIC memory stream creation");
    check_hresult(stream->InitializeFromMemory(const_cast<BYTE*>(encoded.data()), static_cast<DWORD>(encoded.size())),
                  "initializing JPEG memory stream");
    ComPtr<IWICBitmapDecoder> decoder;
    check_hresult(factory->CreateDecoderFromStream(stream.get(), nullptr, WICDecodeMetadataCacheOnLoad, decoder.put()),
                  "decoding JPEG stream");
    ComPtr<IWICBitmapFrameDecode> frame;
    check_hresult(decoder->GetFrame(0, frame.put()), "reading JPEG frame");
    Image image = decode_frame(factory.get(), frame.get());
    if(!alpha.empty())
    {
        const std::size_t pixels = static_cast<std::size_t>(image.width) * image.height;
        if(alpha.size() != pixels)
        {
            throw Error("JPEG alpha channel size does not match the image dimensions");
        }

        for(std::uint32_t row = 0; row < image.height; ++row)
        {
            for(std::uint32_t column = 0; column < image.width; ++column)
            {
                const std::size_t source = static_cast<std::size_t>(row) * image.width + column;
                const std::size_t destination = (static_cast<std::size_t>(image.height - row - 1U) * image.width + column) * 4U + 3U;
                image.bgra[destination] = alpha[source];
            }
        }
    }
    else
    {
        for(std::size_t index = 3; index < image.bgra.size(); index += 4)
        {
            image.bgra[index] = 255;
        }
    }
    return image;
}

void encode_png(const std::filesystem::path& path, const Image& image)
{
    const std::size_t byte_count = checked_pixel_bytes(image.width, image.height);
    if(image.bgra.size() != byte_count || byte_count > std::numeric_limits<UINT>::max())
    {
        throw Error("BGRA buffer size does not match the image dimensions");
    }
    if(!path.parent_path().empty())
    {
        std::filesystem::create_directories(path.parent_path());
    }

    Bytes top_down(byte_count);
    const std::size_t row_size = static_cast<std::size_t>(image.width) * 4U;
    for(std::uint32_t row = 0; row < image.height; ++row)
    {
        const auto source = image.bgra.begin() + static_cast<std::ptrdiff_t>((image.height - row - 1U) * row_size);
        const auto destination = top_down.begin() + static_cast<std::ptrdiff_t>(row * row_size);
        std::copy_n(source, static_cast<std::ptrdiff_t>(row_size), destination);
    }

    ComApartment apartment;
    auto factory = make_factory();
    ComPtr<IWICStream> stream;
    check_hresult(factory->CreateStream(stream.put()), "WIC output stream creation");
    check_hresult(stream->InitializeFromFilename(path.c_str(), GENERIC_WRITE), "opening PNG output");
    ComPtr<IWICBitmapEncoder> encoder;
    check_hresult(factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, encoder.put()), "PNG encoder creation");
    check_hresult(encoder->Initialize(stream.get(), WICBitmapEncoderNoCache), "PNG encoder initialization");
    ComPtr<IWICBitmapFrameEncode> frame;
    check_hresult(encoder->CreateNewFrame(frame.put(), nullptr), "PNG frame creation");
    check_hresult(frame->Initialize(nullptr), "PNG frame initialization");
    check_hresult(frame->SetSize(image.width, image.height), "setting PNG dimensions");
    WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;
    check_hresult(frame->SetPixelFormat(&format), "setting PNG pixel format");
    if(format != GUID_WICPixelFormat32bppBGRA)
    {
        throw Error("the PNG encoder did not accept 32-bit BGRA pixels");
    }
    check_hresult(frame->WritePixels(image.height, image.width * 4U, static_cast<UINT>(top_down.size()), top_down.data()),
                  "writing PNG pixels");
    check_hresult(frame->Commit(), "committing PNG frame");
    check_hresult(encoder->Commit(), "committing PNG file");
}

}
