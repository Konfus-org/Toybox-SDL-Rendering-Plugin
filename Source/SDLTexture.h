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
        SDL_GPUTexture* Texture;
        SDL_GPUSampler* Sampler;

        SDLCachedTexture();
        void Release(SDL_GPUDevice* device);
    };

    struct SDLCachedTextureManager
    {
    public:
        void Release(SDL_GPUDevice* device);

        void Add(SDL_GPUDevice* device, SDL_GPUCommandBuffer* commandBuffer, const Tbx::Texture& texture);
        const SDLCachedTexture& Get(const Tbx::Uid& texture);

    private:
        std::unordered_map<Tbx::Uid, SDLCachedTexture> _cachedTextures;
    };

    SDL_Surface* SDLMakeSurface(const Tbx::Texture& texture);

    SDL_GPUSampler* SDLMakeSampler(const Tbx::Texture& texture, SDL_GPUDevice* device);

    SDL_GPUTexture* SDLCreateTexture(const SDL_Surface* surface, SDL_GPUDevice* device);

    void SDLUploadTexture(SDL_GPUTexture* texture, Uint32 textureSize, const void* textureData, Uint32 textureWidth, Uint32 textureHeight, SDL_GPUDevice* device, SDL_GPUCommandBuffer* commandBuffer);
}
