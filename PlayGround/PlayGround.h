#pragma once
#include "Falcor.h"
#include "FalcorExperimental.h"
#include "Utils/Picking/Picking.h"

using namespace Falcor;

class PlayGround : public Renderer
{
public:
    void onLoad(SampleCallbacks* pSample, RenderContext* pRenderContext) override;
    void onFrameRender(SampleCallbacks* pSample, RenderContext* pRenderContext, const Fbo::SharedPtr& pTargetFbo) override;
    void onResizeSwapChain(SampleCallbacks* pSample, uint32_t width, uint32_t height) override;
    bool onKeyEvent(SampleCallbacks* pSample, const KeyboardEvent& keyEvent) override;
    bool onMouseEvent(SampleCallbacks* pSample, const MouseEvent& mouseEvent) override;
    void onGuiRender(SampleCallbacks* pSample, Gui* pGui) override;

private:
    Camera::SharedPtr mpCamera;
    RasterizerState::SharedPtr mpWireframeRS = nullptr;

    bool mUseTriLinearFiltering = true;
    Sampler::SharedPtr mpPointSampler = nullptr;
    Sampler::SharedPtr mpLinearSampler = nullptr;

    RtScene::SharedPtr mpScene;
    Model::SharedPtr mpModel = nullptr;
    ModelViewCameraController mModelViewCameraController;  
    FirstPersonCameraController mFirstPersonCameraController;
    SixDoFCameraController m6DoFCameraController;
     
    DirectionalLight::SharedPtr mpDirLight;
    PointLight::SharedPtr mpPointLight;

    GraphicsProgram::SharedPtr mpProgram = nullptr;
    GraphicsVars::SharedPtr mpProgramVars = nullptr;
    GraphicsState::SharedPtr mpGraphicsState = nullptr;

    RtProgram::SharedPtr mpRaytraceProgram = nullptr;
    RtProgramVars::SharedPtr mpRtVars;
    RtState::SharedPtr mpRtState;
    RtSceneRenderer::SharedPtr mpRtRenderer;
    Texture::SharedPtr mpRtOut;

    CameraController& getActiveCameraController();
    void setPerFrameVars(const Fbo* pTargetFbo);
    void renderRater(RenderContext* pRenderContext);
    void renderRayTrace(RenderContext* pRenderContext, Fbo* pTargetFbo);
    void resetCamera();

    float mNearZ;
    float mFarZ;

    bool mRayTrace=true;
};
