#pragma once
#include <SDL3/SDL.h>
#include <string>
#include <unordered_map>
#include <Tbx/Graphics/Buffers.h>
#include <Tbx/Graphics/Material.h>

namespace SDLRendering
{
    struct SDLCachedTexture
    {
        SDLCachedTexture() = default;
        SDLCachedTexture(SDL_GPUTexture* texture, SDL_GPUSampler* sampler, SDL_GPUDevice* device);
        ~SDLCachedTexture();

        SDL_GPUTexture* Texture = nullptr;
        SDL_GPUSampler* Sampler = nullptr;
        SDL_GPUDevice* Device = nullptr;
    };

    struct SDLTextureCache
    {
    public:
        ~SDLTextureCache();

        void Add(const Tbx::Texture& texture, SDL_GPUDevice* device, SDL_GPUCommandBuffer* commandBuffer);
        const SDLCachedTexture& Get(const Tbx::Uid& texture);

        void Clear();

    private:
        std::unordered_map<Tbx::Uid, SDLCachedTexture> _cachedTextures;
    };

    SDL_Surface* SDLMakeSurface(const Tbx::Texture& texture);

    SDL_GPUSampler* SDLMakeSampler(const Tbx::Texture& texture, SDL_GPUDevice* device);

    SDL_GPUTexture* SDLCreateTexture(const SDL_Surface* surface, SDL_GPUDevice* device, SDL_GPUCommandBuffer* commandBuffer);

    void SDLUploadTexture(SDL_GPUTexture* texture, Uint32 textureSize, const void* textureData, Uint32 textureWidth, Uint32 textureHeight, SDL_GPUDevice* device, SDL_GPUCommandBuffer* commandBuffer);
}
