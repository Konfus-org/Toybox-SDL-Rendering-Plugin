#include "SDLTexture.h"
#include "SDLRenderer.h"
#include <Tbx/Debug/Debugging.h>
#include <SDL3_shadercross/SDL_shadercross.h>

namespace SDLRendering
{
    SDLCachedTexture::SDLCachedTexture(SDL_GPUTexture* texture, SDL_GPUSampler* sampler, SDL_GPUDevice* device)
    {
        Texture = texture;
        Sampler = sampler;
        Device = device;
    }

    SDLCachedTexture::~SDLCachedTexture()
    {
        if (Texture != nullptr)
        {
            SDL_ReleaseGPUTexture(Device, Texture);
            Texture = nullptr;
        }

        if (Sampler != nullptr)
        {
            SDL_ReleaseGPUSampler(Device, Sampler);
            Sampler = nullptr;
        }
    }

    SDLTextureCache::~SDLTextureCache()
    {
        Clear();
    }

    void SDLTextureCache::Add(const Tbx::Texture& texture, SDL_GPUDevice* device, SDL_GPUCommandBuffer* commandBuffer)
    {
        const auto i = _cachedTextures.find(texture.GetId());
        if (i == _cachedTextures.end())
        {
            Tbx::TextureFilter textureFilter = texture.GetFilter();
            Tbx::TextureWrap textureWrap = texture.GetWrap();

            auto* surface = SDLMakeSurface(texture);
            if (surface != nullptr)
            {
                _cachedTextures.emplace(
                    std::piecewise_construct,
                    std::forward_as_tuple(texture.GetId()),
                    std::forward_as_tuple(SDLCreateTexture(surface, device, commandBuffer), SDLMakeSampler(texture, device), device));
                SDL_DestroySurface(surface);
            }
            else
            {
                TBX_ASSERT(false, "Failed to create SDL_Surface: {}", SDL_GetError());
                return;
            }
        }
    }

    const SDLCachedTexture& SDLTextureCache::Get(const Tbx::Uid& texture)
    {
        return _cachedTextures.find(texture)->second;
    }

    void SDLTextureCache::Clear()
    {
        _cachedTextures.clear();
    }

    SDL_GPUTexture* SDLCreateTexture(const SDL_Surface* surface, SDL_GPUDevice* device, SDL_GPUCommandBuffer* commandBuffer)
    {
        const Uint32 textureWidth = static_cast<Uint32>(surface->w);
        const Uint32 textureHeight = static_cast<Uint32>(surface->h);

        // After normalization we only expect RGBA32 here
        if (surface->format != SDL_PIXELFORMAT_RGBA32)
        {
            TBX_ASSERT(false, "Unexpected surface format; expected RGBA32 after conversion.");
            return nullptr;
        }

        // Setup creation info
        SDL_GPUTextureCreateInfo info = {};
        info.type = SDL_GPU_TEXTURETYPE_2D;
        info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        info.width = textureWidth;
        info.height = textureHeight;
        info.layer_count_or_depth = 1;
        info.num_levels = 1;
        info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;

        // Create and upload
        auto* texture = SDL_CreateGPUTexture(device, &info);
        SDLUploadTexture(texture, surface->pitch * surface->h, surface->pixels, surface->w, surface->h, device, commandBuffer);
        return texture;
    }

    void SDLUploadTexture(SDL_GPUTexture* texture, Uint32 textureSize, const void* textureData, Uint32 textureWidth, Uint32 textureHeight, SDL_GPUDevice* device, SDL_GPUCommandBuffer* commandBuffer)
    {
        SDL_GPUTransferBufferCreateInfo transferBufferCreateInfo = {};
        transferBufferCreateInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        transferBufferCreateInfo.size = textureSize;
        SDL_GPUTransferBuffer* transferBuffer = SDL_CreateGPUTransferBuffer(device, &transferBufferCreateInfo);

        void* targetData = SDL_MapGPUTransferBuffer(device, transferBuffer, false);
        SDL_memcpy(targetData, textureData, textureSize);
        SDL_UnmapGPUTransferBuffer(device, transferBuffer);

        SDL_GPUTextureTransferInfo textureTransferInfo = {};
        textureTransferInfo.transfer_buffer = transferBuffer;
        textureTransferInfo.offset = 0;

        SDL_GPUTextureRegion textureRegion = {};
        textureRegion.texture = texture;
        textureRegion.w = textureWidth;
        textureRegion.h = textureHeight;
        textureRegion.d = 1;

        SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(commandBuffer);
        SDL_UploadToGPUTexture(copyPass, &textureTransferInfo, &textureRegion, false);
        SDL_EndGPUCopyPass(copyPass);
        SDL_ReleaseGPUTransferBuffer(device, transferBuffer);
    }
    
    SDL_Surface* SDLMakeSurface(const Tbx::Texture& texture)
    {
        auto width = texture.GetWidth();
        auto height = texture.GetHeight();
        auto pixelData = (void*)texture.GetPixels().data();
        auto pitch = texture.GetChannels() * width;

        // Convert tbx format to sdl pixel format
        SDL_PixelFormat format;
        switch (texture.GetFormat())
        {
            case Tbx::TextureFormat::RGB:
                format = SDL_PIXELFORMAT_RGB24;
                break;
            case Tbx::TextureFormat::RGBA:
                format = SDL_PIXELFORMAT_RGBA32;
                break;
            default:
                TBX_ASSERT(false, "Unsupported texture format!");
                return nullptr;
        }

        // Create surface from texture data
        auto* surface = SDL_CreateSurfaceFrom(width, height, format, pixelData, pitch);
        if (surface == nullptr)
        {
            TBX_ASSERT("Failed to load BMP: {}", SDL_GetError());
            return nullptr;
        }

        // Normalize to RGBA32 so upload format and data agree
        if (surface->format != SDL_PIXELFORMAT_RGBA32)
        {
            SDL_Surface* converted = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
            SDL_DestroySurface(surface);
            surface = converted;
            if (!surface)
            {
                TBX_ASSERT(false, "Failed to convert surface: {}", SDL_GetError());
                return nullptr;
            }
        }

        return surface;
    }

    SDL_GPUSampler* SDLMakeSampler(const Tbx::Texture& texture, SDL_GPUDevice* device)
    {
        Tbx::TextureFilter textureFilter = texture.GetFilter();
        Tbx::TextureWrap textureWrap = texture.GetWrap();

        SDL_GPUSamplerCreateInfo samplerCreateInfo = {};
        switch (textureFilter)
        {
            case Tbx::TextureFilter::Nearest:
                samplerCreateInfo.min_filter = SDL_GPU_FILTER_NEAREST;
                samplerCreateInfo.mag_filter = SDL_GPU_FILTER_NEAREST;
                break;
            case Tbx::TextureFilter::Linear:
                samplerCreateInfo.min_filter = SDL_GPU_FILTER_LINEAR;
                samplerCreateInfo.mag_filter = SDL_GPU_FILTER_LINEAR;
                break;
        }

        samplerCreateInfo.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
        switch (textureWrap)
        {
            case Tbx::TextureWrap::ClampToEdge:
                samplerCreateInfo.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
                samplerCreateInfo.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
                samplerCreateInfo.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
                break;
            case Tbx::TextureWrap::MirroredRepeat:
                samplerCreateInfo.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_MIRRORED_REPEAT;
                samplerCreateInfo.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_MIRRORED_REPEAT;
                samplerCreateInfo.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_MIRRORED_REPEAT;
                break;
            case Tbx::TextureWrap::Repeat:
                samplerCreateInfo.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
                samplerCreateInfo.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
                samplerCreateInfo.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
                break;
        }
        
        return SDL_CreateGPUSampler(device, &samplerCreateInfo);
    }
}
