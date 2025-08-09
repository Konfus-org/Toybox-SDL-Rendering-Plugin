#include "SDLRenderer.h"
#include <Tbx/Debug/Debugging.h>
#include <Tbx/Graphics/Material.h>
#include <Tbx/Graphics/Mesh.h>
#include <Tbx/App/App.h>

namespace SDLRendering
{
    SDLRenderer::~SDLRenderer()
    {
        Shutdown();
    }

    void SDLRenderer::Initialize(const std::shared_ptr<Tbx::IRenderSurface>& surface)
    {
        auto* window = (SDL_Window*)surface->GetNativeWindow();
        _surface = surface;
        TBX_ASSERT(_surface, "No surface to render to was given!");

#ifdef TBX_DEBUG
        bool debug_mode = true;
#else
        bool debug_mode = false;
#endif
        // create a device for either VULKAN, METAL, or DX12 with debugging enabled and choose the best driver
        _device = std::shared_ptr<SDL_GPUDevice>(SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_MSL | SDL_GPU_SHADERFORMAT_DXBC, debug_mode, NULL), [](SDL_GPUDevice* deviceToDelete)
        {
            SDL_DestroyGPUDevice(deviceToDelete);
        });
        TBX_ASSERT(_device, "Failed to create SDL_Renderer: {}", SDL_GetError());
        SDL_ClaimWindowForGPUDevice(_device.get(), window);

        // Init size and resolution
        int w, h;
        SDL_GetWindowSize(window, &w, &h);
        _resolution = { w, h };
        _viewport = { { 0, 0 }, { w, h } };

        // Check for errors
        CheckForErrors();
    }

    void SDLRenderer::CheckForErrors()
    {
        std::string err = SDL_GetError();
        //TBX_ASSERT(err.empty(), "An error from SDL has occured: {}", err);
    }

    void SDLRenderer::Shutdown()
    {
        _cachedTextureManager.Release(_device.get());
        _cachedShaderManager.Release(_device.get());
        _device.reset();
    }

    Tbx::GraphicsDevice SDLRenderer::GetGraphicsDevice()
    {
        return _device.get();
    }

    void SDLRenderer::SetApi(Tbx::GraphicsApi api)
    {
        _api = api;
    }

    Tbx::GraphicsApi SDLRenderer::GetApi()
    {
        return _api;
    }

    void SDLRenderer::SetViewport(const Tbx::Viewport& viewport)
    {
        _viewport = viewport;

        std::string err = SDL_GetError();
        TBX_ASSERT(err.empty(), "An error from SDL has occured: {}", err);
    }

    const Tbx::Viewport& SDLRenderer::GetViewport()
    {
        return _viewport;
    }

    void SDLRenderer::SetResolution(const Tbx::Size& size)
    {
        _resolution = size;
    }

    const Tbx::Size& SDLRenderer::GetResolution()
    {
        return _resolution;
    }

    void SDLRenderer::SetVSyncEnabled(bool enabled)
    {
        _vsyncEnabled = enabled;
    }

    bool SDLRenderer::GetVSyncEnabled()
    {
        return _vsyncEnabled;
    }

    void SDLRenderer::Flush()
    {
        EndRenderPass();
        SubmitCommandBuffer();
    }

    void SDLRenderer::Clear(const Tbx::Color& color)
    {
        _currColorTarget = {};
        _currColorTarget.clear_color = { color.R, color.G, color.B, color.A };
        _currColorTarget.load_op = SDL_GPU_LOADOP_CLEAR;
        _currColorTarget.store_op = SDL_GPU_STOREOP_STORE;
        _currColorTarget.texture = _currSwapchainTexture;
    }

    void SDLRenderer::Draw(const Tbx::FrameBuffer& buffer)
    {
        SDL_Window* window = (SDL_Window*)_surface.get()->GetNativeWindow();
        if (!TryBeginDraw(window))
        {
            return;
        }

        for (const auto& cmd : buffer.GetCommands())
        {
            switch (cmd.GetType())
            {
                case Tbx::DrawCommandType::Clear:
                {
                    const auto& color = std::any_cast<const Tbx::Color&>(cmd.GetPayload());
                    Clear(color);
                    break;
                }
                case Tbx::DrawCommandType::CompileMaterial:
                {
                    CompileMaterial(cmd);
                    break;
                }
                case Tbx::DrawCommandType::SetMaterial:
                {
                    SetMaterial(cmd);
                    break;
                }
                case Tbx::DrawCommandType::UploadMaterialData:
                {
                    UploadShaderData(cmd);
                    break;
                }
                case Tbx::DrawCommandType::DrawMesh:
                {
                    DrawMesh(cmd, window);
                    break;
                }
                default:
                    break;
            }
        }

        EndDraw();
    }

    bool SDLRenderer::TryBeginDraw(SDL_Window* window)
    {
        // Acquire the command buffer
        _currCommandBuffer = SDL_AcquireGPUCommandBuffer(_device.get());

        // Get the swapchain texture
        Uint32 width, height;
        SDL_WaitAndAcquireGPUSwapchainTexture(_currCommandBuffer, window, &_currSwapchainTexture, &width, &height);

        // End the frame early if a swapchain texture is not available
        if (_currSwapchainTexture == nullptr)
        {
            // you must always submit the command buffer
            SDL_SubmitGPUCommandBuffer(_currCommandBuffer);
            return false;
        }
        
        // Clear screen
        Clear(Tbx::App::GetInstance()->GetGraphicsSettings().ClearColor);
        BeginRenderPass();

        return true;
    }

    void SDLRenderer::EndDraw()
    {
        Flush();
        CheckForErrors();
    }

    void SDLRenderer::BeginRenderPass()
    {
        _currRenderPass = SDL_BeginGPURenderPass(_currCommandBuffer, &_currColorTarget, 1, NULL);
    }

    void SDLRenderer::EndRenderPass()
    {
        if (_currRenderPass)
        {
            SDL_EndGPURenderPass(_currRenderPass);
            _currRenderPass = nullptr;
        }
    }

    void SDLRenderer::SubmitCommandBuffer()
    {
        if (_currCommandBuffer)
        {
            SDL_SubmitGPUCommandBuffer(_currCommandBuffer);
            _currCommandBuffer = nullptr;
        }
    }

    void SDLRenderer::CompileMaterial(const Tbx::DrawCommand& cmd)
    {
        // Clear any passes to allow for future gpu passes
        EndRenderPass();

        // Get the current material
        auto material = std::any_cast<const Tbx::Material&>(cmd.GetPayload());
        _currentMaterial = material;

        // Upload, set, and compile shaders (if not already)
        const Tbx::Shader shader = _currentMaterial.GetShader();
        _cachedShaderManager.AddVert(_device.get(), shader);
        _cachedShaderManager.AddFrag(_device.get(), shader);

        // Upload textures (if not already)
        const std::vector<Tbx::Texture>& textures = _currentMaterial.GetTextures();
        for (size_t i = 0; i < textures.size(); i++)
        {
            const Tbx::Texture& texture = textures[i];
            _cachedTextureManager.Add(_device.get(), _currCommandBuffer, texture);
        }
    }

    void SDLRenderer::SetMaterial(const Tbx::DrawCommand& cmd)
    {
        auto material = std::any_cast<const Tbx::Material&>(cmd.GetPayload());
        _currentMaterial = material;
        _shaderDatas.clear();
    }

    void SDLRenderer::UploadShaderData(const Tbx::DrawCommand& cmd)
    {
        const auto& data = std::any_cast<const Tbx::ShaderData&>(cmd.GetPayload());
        _shaderDatas.push_back(data);
    }

    void SDLRenderer::DrawMesh(const Tbx::DrawCommand& cmd, SDL_Window* window)
    {
        // Get all required data for rendering...
        const Tbx::Shader shader = _currentMaterial.GetShader();
        const std::vector<Tbx::Texture>& textures = _currentMaterial.GetTextures();
        const auto& mesh = std::any_cast<const Tbx::Mesh&>(cmd.GetPayload());
        const Tbx::VertexBuffer& meshVertexBuffer = mesh.GetVertexBuffer();
        const Tbx::BufferLayout& meshBufferLayout = meshVertexBuffer.GetLayout();

        // get the vertices from the mesh
        const std::vector<float>& vertices = meshVertexBuffer.GetVertices();
        Tbx::uint32 verticesSize = static_cast<Uint32>(sizeof(float) * vertices.size());
        const void* verticesPtr = &vertices[0];

        // get the indices from the mesh
        const std::vector<Tbx::uint32>& indices = mesh.GetIndices();
        Tbx::uint32 indicesSize = static_cast<Uint32>(sizeof(Tbx::uint32) * indices.size());
        Uint32 indexCount = static_cast<Uint32>(indices.size());
        const void* pIndices = &indices[0];

        std::vector<SDL_GPUVertexAttribute> vertexAttributes;
        SDLCreateVertexAttributes(vertexAttributes, meshBufferLayout);

        std::vector<SDL_GPUVertexBufferDescription> vertexBufferDesctiptions;
        SDLCreateVertexBufferDescriptions(vertexBufferDesctiptions, meshBufferLayout);

        SDL_GPUColorTargetDescription colorTargetDescriptions[1];
        colorTargetDescriptions[0] = {};
        colorTargetDescriptions[0].format = SDL_GetGPUSwapchainTextureFormat(_device.get(), window);

        // create the graphics pipeline
        SDL_GPUGraphicsPipelineCreateInfo graphicsPipelineInfo = {};
        graphicsPipelineInfo.vertex_shader = _cachedShaderManager.GetVert(shader)._shader;
        graphicsPipelineInfo.fragment_shader = _cachedShaderManager.GetFrag(shader)._shader;
        graphicsPipelineInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        graphicsPipelineInfo.vertex_input_state.num_vertex_attributes = (Uint32)vertexAttributes.size();
        graphicsPipelineInfo.vertex_input_state.vertex_attributes = &vertexAttributes[0];
        graphicsPipelineInfo.vertex_input_state.num_vertex_buffers = (Uint32)vertexBufferDesctiptions.size();
        graphicsPipelineInfo.vertex_input_state.vertex_buffer_descriptions = &vertexBufferDesctiptions[0];
        graphicsPipelineInfo.target_info.num_color_targets = 1;
        graphicsPipelineInfo.target_info.color_target_descriptions = colorTargetDescriptions;
        //graphicsPipelineInfo.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
        SDL_GPUGraphicsPipeline* graphicsPipeline = SDL_CreateGPUGraphicsPipeline(_device.get(), &graphicsPipelineInfo);

        // create the vertex buffer and upload the vertices
        SDL_GPUBufferCreateInfo vertexBufferCreateInfo = {};
        vertexBufferCreateInfo.size = verticesSize;
        vertexBufferCreateInfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
        SDL_GPUBuffer* vertexBuffer = SDLCreateBuffer(_device.get(), vertexBufferCreateInfo);
        SDLUploadBuffer(_device.get(), _currCommandBuffer, vertexBuffer, verticesSize, verticesPtr);

        // create the index buffer and upload the indices
        SDL_GPUBufferCreateInfo indexBufferCreateInfo = {};
        indexBufferCreateInfo.size = indicesSize;
        indexBufferCreateInfo.usage = SDL_GPU_BUFFERUSAGE_INDEX;
        SDL_GPUBuffer* indexBuffer = SDLCreateBuffer(_device.get(), indexBufferCreateInfo);
        SDLUploadBuffer(_device.get(), _currCommandBuffer, indexBuffer, indicesSize, pIndices);

        // Start render pass for the mesh
        _currColorTarget.load_op = SDL_GPU_LOADOP_LOAD; // don't clear color target
        BeginRenderPass();
        SDL_BindGPUGraphicsPipeline(_currRenderPass, graphicsPipeline);

        // bind the vertex buffer
        SDL_GPUBufferBinding vertexBufferBindings[1];
        vertexBufferBindings[0] = {};
        vertexBufferBindings[0].buffer = vertexBuffer;
        vertexBufferBindings[0].offset = 0;
        SDL_BindGPUVertexBuffers(_currRenderPass, 0, vertexBufferBindings, 1);

        // bind the index buffer
        SDL_GPUBufferBinding indexBufferBindings[1];
        indexBufferBindings[0] = {};
        indexBufferBindings[0].buffer = indexBuffer;
        indexBufferBindings[0].offset = 0;
        SDL_BindGPUIndexBuffer(_currRenderPass, indexBufferBindings, SDL_GPU_INDEXELEMENTSIZE_32BIT);

        // HACK: upload the uniform data to the shaders
        for (size_t i = 0; i<_shaderDatas.size(); i++)
        {
            const Tbx::ShaderData& shaderData = _shaderDatas[i];
            if (shaderData._isFragment)
            {
                SDL_PushGPUFragmentUniformData(_currCommandBuffer, shaderData._uniformSlot, shaderData._uniformData, shaderData._uniformSize);
            }
            else
            {
                SDL_PushGPUVertexUniformData(_currCommandBuffer, shaderData._uniformSlot, shaderData._uniformData, shaderData._uniformSize);
            }
        }

        // bind the textures to the fragment shader
        for (size_t i = 0; i<textures.size(); i++)
        {
            const Tbx::Texture& texture = textures[i];
            SDLCachedTexture cachedTexture = _cachedTextureManager.Get(texture);
            if (cachedTexture._sampler != nullptr)
            {
                SDL_GPUTextureSamplerBinding textureSamplerBinding = {};
                textureSamplerBinding.texture = cachedTexture._texture;
                textureSamplerBinding.sampler = cachedTexture._sampler;
                SDL_BindGPUFragmentSamplers(_currRenderPass, 0, &textureSamplerBinding, 1);
            }
        }

        // draw the mesh
        SDL_DrawGPUIndexedPrimitives(_currRenderPass, indexCount, 1, 0, 0, 0);

        // release the resources created above
        SDL_ReleaseGPUGraphicsPipeline(_device.get(), graphicsPipeline);
        SDL_ReleaseGPUBuffer(_device.get(), indexBuffer);
        SDL_ReleaseGPUBuffer(_device.get(), vertexBuffer);
    }
}
