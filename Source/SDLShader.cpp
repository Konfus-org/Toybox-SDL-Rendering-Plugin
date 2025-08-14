#include "SDLShader.h"
#include "SDLRenderer.h"
#include <Tbx/Debug/Debugging.h>
#include <SDL3_shadercross/SDL_shadercross.h>

namespace SDLRendering
{
    SDLCachedShader::SDLCachedShader(SDL_GPUShader* shader, SDL_GPUDevice* device)
    {
        Shader = shader;
        Device = device;
    }

    SDLCachedShader::~SDLCachedShader()
    {
        if (Shader != nullptr)
        {
            SDL_ReleaseGPUShader(Device, Shader);
            Shader = nullptr;
        }
    }

    SDLShaderCache::~SDLShaderCache()
    {
        Clear();
    }

    void SDLShaderCache::Add(const Tbx::Shader& shader, SDL_GPUDevice* device)
    {
        const auto i = _cachedShaders.find(shader.GetId());
        if (i == _cachedShaders.end())
        {
#ifdef TBX_DEBUG
            auto debug = true;
#else
            auto debug = false;
#endif
            const auto entryPointFunc = "main";

            SDL_ShaderCross_ShaderStage stage;
            const auto& shaderType = shader.GetType();
            if (shaderType == Tbx::ShaderType::Vertex)
            {
                stage = SDL_SHADERCROSS_SHADERSTAGE_VERTEX;
            }
            else if (shaderType == Tbx::ShaderType::Fragment)
            {
                stage = SDL_SHADERCROSS_SHADERSTAGE_FRAGMENT;
            }
            else
            {
                TBX_ASSERT(false, "Unsupported shader type: {}", (int)shaderType);
                return;
            }

            SDL_ShaderCross_HLSL_Info info = {};
            info.source = shader.GetSource().c_str();
            info.entrypoint = entryPointFunc;
            info.shader_stage = stage;
            info.enable_debug = debug;
            info.include_dir = nullptr;
            info.defines = nullptr;
            info.name = nullptr;

            size_t size = 0;
            void* data = SDL_ShaderCross_CompileSPIRVFromHLSL(&info, &size);
            TBX_ASSERT(data != nullptr && size != 0, "Failed to compile shader: {}", SDL_GetError());

            SDL_ShaderCross_SPIRV_Info vertexInfo = {};
            vertexInfo.entrypoint = entryPointFunc;
            vertexInfo.bytecode = (Uint8*)data;
            vertexInfo.bytecode_size = size;
            vertexInfo.shader_stage = stage;
            vertexInfo.enable_debug = debug;

            SDL_ShaderCross_GraphicsShaderMetadata shaderMetadata = {};
            shaderMetadata.num_uniform_buffers = 1;
            shaderMetadata.num_storage_textures = 0;
            shaderMetadata.num_storage_buffers = 0;
            shaderMetadata.num_inputs = 0;
            shaderMetadata.num_outputs = 0;
            shaderMetadata.inputs = nullptr;
            shaderMetadata.outputs = nullptr;
            if (shaderType == Tbx::ShaderType::Vertex)
            {
                shaderMetadata.num_samplers = 0;
            }
            else if (shaderType == Tbx::ShaderType::Fragment)
            {
                shaderMetadata.num_samplers = 1;
            }

            SDL_GPUShader* compiledShader = SDL_ShaderCross_CompileGraphicsShaderFromSPIRV(device, &vertexInfo, &shaderMetadata, 0);
            TBX_ASSERT(compiledShader != nullptr && size != 0, "Failed to compile shader: {}", SDL_GetError());

            _cachedShaders.emplace(
                std::piecewise_construct,
                std::forward_as_tuple(shader.GetId()),
                std::forward_as_tuple(compiledShader, device));
            SDL_free(data);
        }
    }

    const SDLCachedShader& SDLShaderCache::Get(const Tbx::Uid& shader)
    {
        return _cachedShaders.find(shader)->second;
    }

    void SDLShaderCache::Clear()
    {
        _cachedShaders.clear();
    }

    std::vector<SDL_GPUVertexAttribute> SDLCreateVertexAttributes(const Tbx::BufferLayout& bufferLayout)
    {
        const std::vector<Tbx::BufferElement>& bufferElements = bufferLayout.GetElements();
        auto vertexAttributes = std::vector<SDL_GPUVertexAttribute>();

        Uint32 offset = 0;
        for (size_t i = 0; i < bufferElements.size(); i++)
        {
            const Tbx::BufferElement& bufferElement = bufferElements[i];
            Tbx::ShaderUniformType type = bufferElement.GetType();
            Uint32 size = (Uint32)bufferElement.GetSize();

            vertexAttributes.resize(vertexAttributes.size() + 1);
            SDL_GPUVertexAttribute& vertexAttribute = vertexAttributes.back();

            vertexAttribute.buffer_slot = 0;
            vertexAttribute.location = (Uint32)i;
            switch(type)
            {
                case Tbx::ShaderUniformType::Float2:
                    vertexAttribute.format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
                    break;
                case Tbx::ShaderUniformType::Float3:
                    vertexAttribute.format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
                    break;
                case Tbx::ShaderUniformType::Float4:
                    vertexAttribute.format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
                    break;
                default:
                    TBX_ASSERT(false, "Unsupported shader uniform data type!");
                    break;
            }
            vertexAttribute.offset = offset;

            offset += size;
        }

        return vertexAttributes;
    }

    std::vector<SDL_GPUVertexBufferDescription> SDLCreateVertexBufferDescriptions(const Tbx::BufferLayout& bufferLayout)
    {
        Uint32 stride = bufferLayout.GetStride();

        auto vertexBufferDesctiptions = std::vector<SDL_GPUVertexBufferDescription>();
        vertexBufferDesctiptions.resize(vertexBufferDesctiptions.size() + 1);

        SDL_GPUVertexBufferDescription& vertexBufferDesctiption = vertexBufferDesctiptions.back();
        vertexBufferDesctiption.slot = 0;
        vertexBufferDesctiption.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
        vertexBufferDesctiption.instance_step_rate = 0;
        vertexBufferDesctiption.pitch = stride;

        return vertexBufferDesctiptions;
    }

    SDL_GPUBuffer* SDLCreateBuffer(const SDL_GPUBufferCreateInfo& bufferCreateInfo, SDL_GPUDevice* device)
    {
        SDL_GPUBuffer* buffer = SDL_CreateGPUBuffer(device, &bufferCreateInfo);
        return buffer;
    }

    void SDLUploadBuffer(SDL_GPUBuffer* buffer, Uint32 sourceSize, const void* sourceData, SDL_GPUDevice* device, SDL_GPUCommandBuffer* commandBuffer)
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
