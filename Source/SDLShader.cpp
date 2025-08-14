#include "SDLShader.h"
#include "SDLRenderer.h"
#include <Tbx/Debug/Debugging.h>
#include <SDL3_shadercross/SDL_shadercross.h>

namespace SDLRendering
{
    SDLCachedShader::SDLCachedShader()
    {
        Shader = nullptr;
    }

    void SDLCachedShader::Release(SDL_GPUDevice* device)
    {
        if (Shader != nullptr)
        {
            SDL_ReleaseGPUShader(device, Shader);
            Shader = nullptr;
        }
    }

    void SDLCachedShaderManager::Release(SDL_GPUDevice* device)
    {
        for (auto i = _cachedShaders.begin(); i != _cachedShaders.end(); i++)
        {
            SDLCachedShader& cachedShader = i->second;
            cachedShader.Release(device);
        }
        _cachedShaders.clear();
    }

    const SDLCachedShader& SDLCachedShaderManager::GetVert(const Tbx::Uid& shader)
    {
        return _cachedShaders.find(shader)->second;
    }

    const SDLCachedShader& SDLCachedShaderManager::GetFrag(const Tbx::Uid& shader)
    {
        return _cachedShaders.find(shader)->second;
    }

    void SDLCachedShaderManager::AddVert(SDL_GPUDevice* device, const Tbx::Shader& shader)
    {
        if (shader.GetType() != Tbx::ShaderType::Vertex)
        {
            TBX_ASSERT(false, "Wrong type of shader given!");
            return;
        }

        const auto i = _cachedShaders.find(shader.GetId());
        if (i == _cachedShaders.end())
        {
            SDL_ShaderCross_HLSL_Info info = {};
            info.source = shader.GetSource().c_str();
            info.entrypoint = "main";
            info.include_dir = nullptr;
            info.defines = nullptr;
            info.shader_stage = SDL_SHADERCROSS_SHADERSTAGE_VERTEX;
            info.enable_debug = true;
            info.name = nullptr;

            size_t size = 0;
            void* data = SDL_ShaderCross_CompileSPIRVFromHLSL(&info, &size);
            TBX_ASSERT(data != nullptr && size != 0, "Failed to compile vertex shader: {}", SDL_GetError());

            SDL_ShaderCross_SPIRV_Info vertexInfo = {};
            vertexInfo.bytecode = (Uint8*)data;
            vertexInfo.bytecode_size = size;
            vertexInfo.entrypoint = "main";
            vertexInfo.shader_stage = SDL_SHADERCROSS_SHADERSTAGE_VERTEX;
            vertexInfo.enable_debug = true;

            SDL_ShaderCross_GraphicsShaderMetadata vertexMetadata = {};
            vertexMetadata.num_uniform_buffers = 1;
            vertexMetadata.num_samplers = 0;
            vertexMetadata.num_storage_textures = 0;
            vertexMetadata.num_storage_buffers = 0;
            vertexMetadata.num_inputs = 0;
            vertexMetadata.num_outputs = 0;
            vertexMetadata.inputs = nullptr;
            vertexMetadata.outputs = nullptr;

            SDLCachedShader cachedShader = {};
            SDL_GPUShader* vertexShader = SDL_ShaderCross_CompileGraphicsShaderFromSPIRV(device, &vertexInfo, &vertexMetadata, 0);
            cachedShader.Shader = vertexShader;

            _cachedShaders[shader.GetId()] = cachedShader;

            SDL_free(data);
        }
    }

    void SDLCachedShaderManager::AddFrag(SDL_GPUDevice* device, const Tbx::Shader& shader)
    {
        if (shader.GetType() != Tbx::ShaderType::Fragment)
        {
            TBX_ASSERT(false, "Wrong type of shader given!");
            return;
        }

        const auto i = _cachedShaders.find(shader.GetId());
        if (i == _cachedShaders.end())
        {
            SDL_ShaderCross_HLSL_Info info = {};
            info.source = shader.GetSource().c_str();
            info.entrypoint = "main";
            info.include_dir = nullptr;
            info.defines = nullptr;
            info.shader_stage = SDL_SHADERCROSS_SHADERSTAGE_FRAGMENT;
            info.enable_debug = true;
            info.name = nullptr;

            size_t size = 0;
            void* data = SDL_ShaderCross_CompileSPIRVFromHLSL(&info, &size);
            TBX_ASSERT(data != nullptr && size != 0, "Failed to compile vertex shader: {}", SDL_GetError());

            SDL_ShaderCross_SPIRV_Info fragmentInfo = {};
            fragmentInfo.bytecode = (Uint8*)data;
            fragmentInfo.bytecode_size = size;
            fragmentInfo.entrypoint = "main";
            fragmentInfo.shader_stage = SDL_SHADERCROSS_SHADERSTAGE_FRAGMENT;
            fragmentInfo.enable_debug = true;

            SDL_ShaderCross_GraphicsShaderMetadata fragmentMetadata = {};
            fragmentMetadata.num_samplers = 1;
            fragmentMetadata.num_storage_buffers = 0;
            fragmentMetadata.num_storage_textures = 0;
            fragmentMetadata.num_uniform_buffers = 1;

            SDLCachedShader cachedShader = {};
            SDL_GPUShader* fragmentShader = SDL_ShaderCross_CompileGraphicsShaderFromSPIRV(device, &fragmentInfo, &fragmentMetadata, 0);
            cachedShader.Shader = fragmentShader;

            _cachedShaders[shader.GetId()] = cachedShader;

            SDL_free(data);
        }
    }

    void SDLCreateVertexAttributes(std::vector<SDL_GPUVertexAttribute>& vertexAttributes, const Tbx::BufferLayout& bufferLayout)
    {
        const std::vector<Tbx::BufferElement>& bufferElements = bufferLayout.GetElements();

        vertexAttributes.clear();
        Uint32 offset = 0;

        for (size_t i = 0; i < bufferElements.size(); i++)
        {
            const Tbx::BufferElement& bufferElement = bufferElements[i];
            Tbx::ShaderDataType type = bufferElement.GetType();
            Uint32 size = (Uint32)bufferElement.GetSize();

            vertexAttributes.resize(vertexAttributes.size() + 1);
            SDL_GPUVertexAttribute& vertexAttribute = vertexAttributes.back();

            vertexAttribute.buffer_slot = 0;
            vertexAttribute.location = (Uint32)i;
            switch(type)
            {
                case Tbx::ShaderDataType::Float2:
                    vertexAttribute.format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
                    break;
                case Tbx::ShaderDataType::Float3:
                    vertexAttribute.format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
                    break;
                case Tbx::ShaderDataType::Float4:
                    vertexAttribute.format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
                    break;
                default:
                    SDL_assert(0);
                    break;
            }
            vertexAttribute.offset = offset;

            offset += size;
        }
    }

    void SDLCreateVertexBufferDescriptions(std::vector<SDL_GPUVertexBufferDescription>& vertexBufferDesctiptions, const Tbx::BufferLayout& bufferLayout)
    {
        Uint32 stride = bufferLayout.GetStride();

        vertexBufferDesctiptions.clear();
        vertexBufferDesctiptions.resize(vertexBufferDesctiptions.size() + 1);

        SDL_GPUVertexBufferDescription& vertexBufferDesctiption = vertexBufferDesctiptions.back();
        vertexBufferDesctiption.slot = 0;
        vertexBufferDesctiption.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
        vertexBufferDesctiption.instance_step_rate = 0;
        vertexBufferDesctiption.pitch = stride;
    }

    SDL_GPUBuffer* SDLCreateBuffer(SDL_GPUDevice* device, const SDL_GPUBufferCreateInfo& bufferCreateInfo)
    {
        SDL_GPUBuffer* buffer = SDL_CreateGPUBuffer(device, &bufferCreateInfo);
        return buffer;
    }

    void SDLUploadBuffer(SDL_GPUDevice* device, SDL_GPUCommandBuffer* commandBuffer, SDL_GPUBuffer* buffer, Uint32 sourceSize, const void* sourceData)
    {
        SDL_GPUTransferBufferCreateInfo transferBufferCreateInfo = {};
        transferBufferCreateInfo.size = sourceSize;
        transferBufferCreateInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        SDL_GPUTransferBuffer* transferBuffer = SDL_CreateGPUTransferBuffer(device, &transferBufferCreateInfo);

        void* targetData = SDL_MapGPUTransferBuffer(device, transferBuffer, false);
        SDL_memcpy(targetData, sourceData, sourceSize);
        SDL_UnmapGPUTransferBuffer(device, transferBuffer);

        SDL_GPUTransferBufferLocation transferBufferLocation = {};
        transferBufferLocation.transfer_buffer = transferBuffer;
        transferBufferLocation.offset = 0;

        SDL_GPUBufferRegion bufferRegion = {};
        bufferRegion.buffer = buffer;
        bufferRegion.size = sourceSize;
        bufferRegion.offset = 0;

        SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(commandBuffer);
        SDL_UploadToGPUBuffer(copyPass, &transferBufferLocation, &bufferRegion, true);
        SDL_EndGPUCopyPass(copyPass);
        SDL_ReleaseGPUTransferBuffer(device, transferBuffer);
    }
}
