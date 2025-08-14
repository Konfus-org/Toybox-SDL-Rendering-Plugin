#pragma once
#include <SDL3/SDL.h>
#include <string>
#include <unordered_map>
#include <Tbx/Graphics/Buffers.h>
#include <Tbx/Graphics/Material.h>

namespace SDLRendering
{
    struct SDLCachedShader
    {
        SDLCachedShader() = default;
        SDLCachedShader(SDL_GPUShader* shader, SDL_GPUDevice* device);
        ~SDLCachedShader();

        SDL_GPUShader* Shader = nullptr;
        SDL_GPUDevice* Device = nullptr;
    };

    struct SDLShaderCache
    {
    public:
        ~SDLShaderCache();

        void Add(const Tbx::Shader& shader, SDL_GPUDevice* device);
        const SDLCachedShader& Get(const Tbx::Uid& shader);

        void Clear();

    private:
        std::unordered_map<Tbx::Uid, SDLCachedShader> _cachedShaders;

    };

    std::vector<SDL_GPUVertexAttribute> SDLCreateVertexAttributes(const Tbx::BufferLayout& bufferLayout);

    std::vector<SDL_GPUVertexBufferDescription> SDLCreateVertexBufferDescriptions(const Tbx::BufferLayout& bufferLayout);

    SDL_GPUBuffer* SDLCreateBuffer(const SDL_GPUBufferCreateInfo& bufferCreateInfo, SDL_GPUDevice* device);

    void SDLUploadBuffer(SDL_GPUBuffer* buffer, Uint32 sourceSize, const void* sourceData, SDL_GPUDevice* device, SDL_GPUCommandBuffer* commandBuffer);
}
