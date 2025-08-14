#include "SDLTexture.h"
#include "SDLRenderer.h"
#include <Tbx/Debug/Debugging.h>
#include <SDL3_shadercross/SDL_shadercross.h>

namespace SDLRendering
{
    static const SDLCachedTexture _emptyTexture;

    SDLCachedTexture::SDLCachedTexture()
    {
        Texture = nullptr;
        Sampler = nullptr;
    }

    void SDLCachedTexture::Release(SDL_GPUDevice* device)
    {
        if (Texture != nullptr)
        {
            SDL_ReleaseGPUTexture(device, Texture);
            Texture = nullptr;
        }

        if (Sampler != nullptr)
        {
            SDL_ReleaseGPUSampler(device, Sampler);
            Sampler = nullptr;
        }
    }

    void SDLCachedTextureManager::Release(SDL_GPUDevice* device)
    {
        for (auto i = _cachedTextures.begin(); i != _cachedTextures.end(); i++)
        {
            SDLCachedTexture& cachedTexture = i->second;
            cachedTexture.Release(device);
        }
        _cachedTextures.clear();
    }

    void SDLCachedTextureManager::Add(SDL_GPUDevice* device, SDL_GPUCommandBuffer* commandBuffer, const Tbx::Texture& texture)
    {
        const auto i = _cachedTextures.find(texture.GetId());
        if (i == _cachedTextures.end())
        {
            Tbx::TextureFilter textureFilter = texture.GetFilter();
            Tbx::TextureWrap textureWrap = texture.GetWrap();

            SDLCachedTexture cachedTexture;

            // create the texture
            SDL_Surface* surface = SDL_CreateSurfaceFrom(
                texture.GetWidth(), texture.GetHeight(), SDL_PIXELFORMAT_ABGR8888, texture.GetPixels().get(), texture.GetWidth() * texture.GetChannels());
            if (!surface)
            {
                TBX_ASSERT(false, "Failed to create SDL_Surface: {}", SDL_GetError());
                return;
            }

            if (surface != nullptr)
            {
                Uint32 textureWidth = static_cast<Uint32>(surface->w);
                Uint32 textureHeight = static_cast<Uint32>(surface->h);
                Uint32 textureSize = textureWidth * textureHeight * texture.GetChannels();
                const void* textureData = surface->pixels;

                SDL_GPUTextureCreateInfo textureCreateInfo = {};
                textureCreateInfo.type = SDL_GPU_TEXTURETYPE_2D;
                textureCreateInfo.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
                textureCreateInfo.width = textureWidth;
                textureCreateInfo.height = textureHeight;
                textureCreateInfo.layer_count_or_depth = 1;
                textureCreateInfo.num_levels = 1;
                textureCreateInfo.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
                cachedTexture.Texture = SDLCreateTexture(device, textureCreateInfo);

                SDLUploadTexture(device, commandBuffer, cachedTexture.Texture, textureSize, textureData, textureWidth, textureHeight);
            }

            // create the sampler
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
            cachedTexture.Sampler = SDL_CreateGPUSampler(device, &samplerCreateInfo);

            _cachedTextures.insert(std::make_pair(texture.GetId(), cachedTexture));
        }
    }

    const SDLCachedTexture& SDLCachedTextureManager::Get(const Tbx::Uid& texture)
    {
        const auto i = _cachedTextures.find(texture);
        if (i != _cachedTextures.end())
        {
            return i->second;
        }
        else
        {
            return _emptyTexture;
        }
    }

    SDL_GPUTexture* SDLCreateTexture(SDL_GPUDevice* device, const SDL_GPUTextureCreateInfo& textureCreateInfo)
    {
        SDL_GPUTexture* texture = SDL_CreateGPUTexture(device, &textureCreateInfo);
        return texture;
    }

    void SDLUploadTexture(SDL_GPUDevice* device, SDL_GPUCommandBuffer* commandBuffer, SDL_GPUTexture* texture, Uint32 textureSize, const void* textureData, Uint32 textureWidth, Uint32 textureHeight)
    {
        SDL_GPUTransferBufferCreateInfo transferBufferCreateInfo = {};
        transferBufferCreateInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        transferBufferCreateInfo.size = textureSize;
        SDL_GPUTransferBuffer* transferBuffer = SDL_CreateGPUTransferBuffer(device, &transferBufferCreateInfo);

        void* targetData = SDL_MapGPUTransferBuffer(device, transferBuffer, false);
        SDL_memcpy(targetData, textureData, textureSize);
        SDL_UnmapGPUTransferBuffer(device, transferBuffer);

        SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(commandBuffer);

        SDL_GPUTextureTransferInfo textureTransferInfo = {};
        textureTransferInfo.transfer_buffer = transferBuffer;
        textureTransferInfo.offset = 0;

        SDL_GPUTextureRegion textureRegion = {};
        textureRegion.texture = texture;
        textureRegion.w = textureWidth;
        textureRegion.h = textureHeight;
        textureRegion.d = 1;

        SDL_UploadToGPUTexture(copyPass, &textureTransferInfo, &textureRegion, false);
        SDL_EndGPUCopyPass(copyPass);
        SDL_ReleaseGPUTransferBuffer(device, transferBuffer);
    }

    SDL_Surface* SDLLoadSurface(const char* surfacePath, SDL_PixelFormat pixelFormat)
    {
        SDL_Surface* surface = SDL_LoadBMP(surfacePath);
        if (surface == nullptr)
        {
            TBX_ASSERT("Failed to load BMP: {}", SDL_GetError());
            return nullptr;
        }
        if (surface->format != pixelFormat)
        {
            SDL_Surface* newSurface = SDL_ConvertSurface(surface, pixelFormat);
            SDL_DestroySurface(surface);
            surface = newSurface;
        }
        return surface;
    }
}
