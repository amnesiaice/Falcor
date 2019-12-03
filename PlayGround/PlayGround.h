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
    bool onKeyEvent(SampleCallbacks* pSample, const KeyboardEvent& keyEvent) override;
    bool onMouseEvent(SampleCallbacks* pSample, const MouseEvent& mouseEvent) override;

    void resetCamera();
private:
    Camera::SharedPtr mpCamera;
    RasterizerState::SharedPtr mpWireframeRS = nullptr;

    bool mUseTriLinearFiltering = true;
    Sampler::SharedPtr mpPointSampler = nullptr;
    Sampler::SharedPtr mpLinearSampler = nullptr;

    static const std::string skDefaultModel;
    Model::SharedPtr mpModel = nullptr;
    ModelViewCameraController mModelViewCameraController;
    FirstPersonCameraController mFirstPersonCameraController;
    SixDoFCameraController m6DoFCameraController;

    DirectionalLight::SharedPtr mpDirLight;
    PointLight::SharedPtr mpPointLight;

    GraphicsProgram::SharedPtr mpProgram = nullptr;
    GraphicsVars::SharedPtr mpProgramVars = nullptr;
    GraphicsState::SharedPtr mpGraphicsState = nullptr;
    CameraController& getActiveCameraController();
    float mNearZ;
    float mFarZ;
};
