#pragma once
#include <Tbx/PluginAPI/RegisterPlugin.h>
#include <Tbx/Events/RenderEvents.h>

namespace SDLRendering
{
    class SDLRendererFactory : public Tbx::IRendererFactoryPlugin
    {
    public:
        void OnLoad() override;
        void OnUnload() override;

        std::shared_ptr<Tbx::IRenderer> Create(std::shared_ptr<Tbx::IRenderSurface> surface) override;

    private:
        Tbx::IRenderer* New();
        void Delete(Tbx::IRenderer* renderer);
    };

    TBX_REGISTER_PLUGIN(SDLRendererFactory);
}
