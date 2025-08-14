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
        SDLCachedShader();
        void Release(SDL_GPUDevice* device);

        SDL_GPUShader* Shader;
    };

    struct SDLCachedShaderManager
    {
    public:
        void Release(SDL_GPUDevice* device);

        const SDLCachedShader& GetVert(const Tbx::Uid& shader);
        const SDLCachedShader& GetFrag(const Tbx::Uid& shader);

        void AddVert(SDL_GPUDevice* device, const Tbx::Shader& shader);
        void AddFrag(SDL_GPUDevice* device, const Tbx::Shader& shader);

    private:
        std::unordered_map<Tbx::Uid, SDLCachedShader> _cachedShaders;

    };

    void SDLCreateVertexAttributes(std::vector<SDL_GPUVertexAttribute>& vertexAttributes, const Tbx::BufferLayout& bufferLayout);

    void SDLCreateVertexBufferDescriptions(std::vector<SDL_GPUVertexBufferDescription>& vertexBufferDesctiptions, const Tbx::BufferLayout& bufferLayout);

    SDL_GPUBuffer* SDLCreateBuffer(SDL_GPUDevice* device, const SDL_GPUBufferCreateInfo& bufferCreateInfo);

    void SDLUploadBuffer(SDL_GPUDevice* device, SDL_GPUCommandBuffer* commandBuffer, SDL_GPUBuffer* buffer, Uint32 sourceSize, const void* sourceData);
}
