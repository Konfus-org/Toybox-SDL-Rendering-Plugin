#include "SDLRendererFactory.h"
#include "SDLRenderer.h"
#include <SDL3/SDL.h>
#include <Tbx/Events/EventCoordinator.h>

namespace SDLRendering
{
    void SDLRendererFactory::OnLoad()
    {
        // Do nothing...
    }

    void SDLRendererFactory::OnUnload()
    {
        // Do nothing...
    }

    std::shared_ptr<Tbx::IRenderer> SDLRendererFactory::Create(std::shared_ptr<Tbx::IRenderSurface> surface)
    {
        auto renderer = std::shared_ptr<Tbx::IRenderer>(New(), [this](Tbx::IRenderer* renderer) { Delete(renderer); });
        renderer->Initialize(surface);
        return renderer;
    }

    Tbx::IRenderer* SDLRendererFactory::New()
    {
        auto* renderer = new SDLRenderer();
        return renderer;
    }

    void SDLRendererFactory::Delete(Tbx::IRenderer* renderer)
    {
        delete renderer;
    }
}
