#pragma once
#include "Falcor.h"
#include "FalcorExperimental.h"

using namespace Falcor;

class PlayGround : public Renderer
{
public:
    void onLoad(SampleCallbacks* pSample, RenderContext* pRenderContext) override;
    void onFrameRender(SampleCallbacks* pSample, RenderContext* pRenderContext, const Fbo::SharedPtr& pTargetFbo) override;
};
