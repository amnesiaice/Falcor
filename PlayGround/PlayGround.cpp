#include "PlayGround.h"

void PlayGround::onLoad(SampleCallbacks* pSample, RenderContext* pRenderContext)
{

}

void PlayGround::onFrameRender(SampleCallbacks* pSample, RenderContext* pRenderContext, const Fbo::SharedPtr& pTargetFbo)
{

}


#ifdef _WIN32
int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nShowCmd)
#else
int main(int argc, char** argv)
#endif
{
    PlayGround::UniquePtr pRenderer = std::make_unique<PlayGround>();

    SampleConfig config;
    config.windowDesc.title = "Falcor PlayGround";
    config.windowDesc.resizableWindow = true;
#ifdef _WIN32
    Sample::run(config, pRenderer);
#else
    config.argc = (uint32_t)argc;
    config.argv = argv;
    Sample::run(config, pRenderer);
#endif
    return 0;
}
