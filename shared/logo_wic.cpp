#include "logo_wic.h"

#include <windows.h>
#include <wincodec.h>

#include <mutex>

#pragma comment(lib, "windowscodecs.lib")

bool LoadPngRgba(const std::wstring& path, std::vector<uint8_t>& rgba, int& outW, int& outH) {
    static std::once_flag comOnce;
    std::call_once(comOnce, []() { CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED); });

    outW = outH = 0;
    rgba.clear();

    IWICImagingFactory* factory = nullptr;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory)))) {
        return false;
    }

    IWICBitmapDecoder* decoder = nullptr;
    HRESULT hr = factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad,
                                                    &decoder);
    if (FAILED(hr) || !decoder) {
        factory->Release();
        return false;
    }

    IWICBitmapFrameDecode* frame = nullptr;
    hr = decoder->GetFrame(0, &frame);
    decoder->Release();
    if (FAILED(hr) || !frame) {
        factory->Release();
        return false;
    }

    UINT srcW = 0, srcH = 0;
    frame->GetSize(&srcW, &srcH);
    if (srcW == 0 || srcH == 0) {
        frame->Release();
        factory->Release();
        return false;
    }

    IWICFormatConverter* converter = nullptr;
    hr = factory->CreateFormatConverter(&converter);
    if (FAILED(hr) || !converter) {
        frame->Release();
        factory->Release();
        return false;
    }
    hr = converter->Initialize(frame, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.f,
                               WICBitmapPaletteTypeCustom);
    frame->Release();
    if (FAILED(hr)) {
        converter->Release();
        factory->Release();
        return false;
    }

    outW = static_cast<int>(srcW);
    outH = static_cast<int>(srcH);
    rgba.resize(static_cast<size_t>(outW) * outH * 4);
    hr = converter->CopyPixels(nullptr, static_cast<UINT>(outW) * 4, static_cast<UINT>(rgba.size()), rgba.data());
    converter->Release();
    factory->Release();
    return SUCCEEDED(hr);
}

bool LoadImageRgbaFromBytes(const uint8_t* data, size_t size, std::vector<uint8_t>& rgba, int& outW, int& outH) {
    if (!data || size == 0) return false;

    static std::once_flag comOnce;
    std::call_once(comOnce, []() { CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED); });

    outW = outH = 0;
    rgba.clear();

    IWICImagingFactory* factory = nullptr;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory)))) {
        return false;
    }

    IWICStream* stream = nullptr;
    HRESULT hr = factory->CreateStream(&stream);
    if (FAILED(hr) || !stream) {
        factory->Release();
        return false;
    }

    hr = stream->InitializeFromMemory(const_cast<BYTE*>(reinterpret_cast<const BYTE*>(data)),
                                      static_cast<DWORD>(size));
    if (FAILED(hr)) {
        stream->Release();
        factory->Release();
        return false;
    }

    IWICBitmapDecoder* decoder = nullptr;
    hr = factory->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnLoad, &decoder);
    stream->Release();
    if (FAILED(hr) || !decoder) {
        factory->Release();
        return false;
    }

    IWICBitmapFrameDecode* frame = nullptr;
    hr = decoder->GetFrame(0, &frame);
    decoder->Release();
    if (FAILED(hr) || !frame) {
        factory->Release();
        return false;
    }

    UINT srcW = 0, srcH = 0;
    frame->GetSize(&srcW, &srcH);
    if (srcW == 0 || srcH == 0) {
        frame->Release();
        factory->Release();
        return false;
    }

    IWICFormatConverter* converter = nullptr;
    hr = factory->CreateFormatConverter(&converter);
    if (FAILED(hr) || !converter) {
        frame->Release();
        factory->Release();
        return false;
    }
    hr = converter->Initialize(frame, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.f,
                               WICBitmapPaletteTypeCustom);
    frame->Release();
    if (FAILED(hr)) {
        converter->Release();
        factory->Release();
        return false;
    }

    outW = static_cast<int>(srcW);
    outH = static_cast<int>(srcH);
    rgba.resize(static_cast<size_t>(outW) * outH * 4);
    hr = converter->CopyPixels(nullptr, static_cast<UINT>(outW) * 4, static_cast<UINT>(rgba.size()), rgba.data());
    converter->Release();
    factory->Release();
    return SUCCEEDED(hr);
}
