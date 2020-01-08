#include "PlayGround.h"

static const glm::vec4 kClearColor(0.38f, 0.52f, 0.10f, 1);
static const std::string skDefaultModel = "Arcade/Arcade.fscene";

CameraController& PlayGround::getActiveCameraController()
{
    return mModelViewCameraController;
}

void PlayGround::setPerFrameVars(const Fbo* pTargetFbo)
{
    PROFILE("setPerFrameVars");
    GraphicsVars* pVars = mpRtVars->getGlobalVars().get();
    ConstantBuffer::SharedPtr pCB = pVars->getConstantBuffer("PerFrameCB");
    pCB["invView"] = glm::inverse(mpCamera->getViewMatrix());
    pCB["viewportDims"] = vec2(pTargetFbo->getWidth(), pTargetFbo->getHeight());
    float fovY = focalLengthToFovY(mpCamera->getFocalLength(), Camera::kDefaultFrameHeight);
    pCB["tanHalfFovY"] = tanf(fovY * 0.5f);
}

void PlayGround::onLoad(SampleCallbacks* pSample, RenderContext* pRenderContext)
{
    if (gpDevice->isFeatureSupported(Device::SupportedFeatures::Raytracing) == false)
    {
        logErrorAndExit("Device does not support raytracing!", true);
    }

    RtProgram::Desc rtProgDesc;
    rtProgDesc.addShaderLibrary("PlayGround.rt.hlsl").setRayGen("rayGen");
    rtProgDesc.addHitGroup(0, "primaryClosestHit", "").addMiss(0, "primaryMiss");
    rtProgDesc.addHitGroup(1, "", "shadowAnyHit").addMiss(1, "shadowMiss");

    mpRaytraceProgram = RtProgram::create(rtProgDesc);

    mpRtState = RtState::create();
    mpRtState->setProgram(mpRaytraceProgram);
    mpRtState->setMaxTraceRecursionDepth(3); // 1 for calling TraceRay from RayGen, 1 for calling it from the primary-ray ClosestHitShader for reflections, 1 for reflection ray tracing a shadow ray


    mpCamera = Camera::create();
    mpProgram = GraphicsProgram::createFromFile("PlayGround.ps.hlsl", "", "main");

    mModelViewCameraController.attachCamera(mpCamera);
    mFirstPersonCameraController.attachCamera(mpCamera);
    m6DoFCameraController.attachCamera(mpCamera);

    //create sampler
    Sampler::Desc samplerDesc;
    samplerDesc.setFilterMode(Sampler::Filter::Point, Sampler::Filter::Point, Sampler::Filter::Point);
    mpPointSampler = Sampler::create(samplerDesc);
    samplerDesc.setFilterMode(Sampler::Filter::Linear, Sampler::Filter::Linear, Sampler::Filter::Linear);
    mpLinearSampler = Sampler::create(samplerDesc);

    mpDirLight = DirectionalLight::create();
    mpPointLight = PointLight::create();
    mpDirLight->setWorldDirection(glm::vec3(0.13f, 0.27f, -0.9f));

    //load model
    mpScene = RtScene::loadFromFile(skDefaultModel.c_str(), RtBuildFlags::None, Model::LoadFlags::RemoveInstancing);
    mpModel = mpScene->getModel(0);


    mpRtVars = RtProgramVars::create(mpRaytraceProgram, mpScene);
    mpRtRenderer = RtSceneRenderer::create(mpScene);

    mpProgramVars = GraphicsVars::create(mpProgram->getReflector());
    mpGraphicsState = GraphicsState::create();
    mpGraphicsState->setProgram(mpProgram);

    if (mpModel == nullptr)
    {
        msgBox("Could not load model");
        return;
    }
    resetCamera();

    float radius = mpModel->getRadius();
    float lightHeight = max(1.0f + radius, radius * 1.25f);
    mpPointLight->setWorldPosition(glm::vec3(0, lightHeight, 0));

    mpModel->bindSamplerToMaterials(mUseTriLinearFiltering ? mpLinearSampler : mpPointSampler);

    resetCamera();
}

void PlayGround::onFrameRender(SampleCallbacks* pSample, RenderContext* pRenderContext, const Fbo::SharedPtr& pTargetFbo)
{
    const glm::vec4 clearColor(0.38f, 0.52f, 0.10f, 1);
    pRenderContext->clearFbo(pTargetFbo.get(), clearColor, 1.0f, 0, FboAttachmentType::All);
   

    if (mpModel)
    {
        mpGraphicsState->setFbo(pTargetFbo);

        mpCamera->setDepthRange(mNearZ, mFarZ);
        getActiveCameraController().update();

        if (mRayTrace)
            renderRayTrace(pRenderContext, pTargetFbo.get());
        else
            renderRater(pRenderContext);
    }
}


void PlayGround::onResizeSwapChain(SampleCallbacks* pSample, uint32_t width, uint32_t height)
{
    float h = (float)height;
    float w = (float)width;

    mpCamera->setFocalLength(18);
    float aspectRatio = (w / h);
    mpCamera->setAspectRatio(aspectRatio);

    mpRtOut = Texture::create2D(width, height, ResourceFormat::RGBA16Float, 1, 1, nullptr, Resource::BindFlags::UnorderedAccess | Resource::BindFlags::ShaderResource);
}

bool PlayGround::onKeyEvent(SampleCallbacks* pSample, const KeyboardEvent& keyEvent)
{
    bool bHandled = getActiveCameraController().onKeyEvent(keyEvent);
    if (bHandled == false)
    {
        if (keyEvent.type == KeyboardEvent::Type::KeyPressed)
        {
            switch (keyEvent.key)
            {
            case KeyboardEvent::Key::R:
                resetCamera();
                bHandled = true;
                break;
            }
        }
    }
    return bHandled;
}

bool PlayGround::onMouseEvent(SampleCallbacks* pSample, const MouseEvent& mouseEvent)
{
    return getActiveCameraController().onMouseEvent(mouseEvent);
}

void PlayGround::onGuiRender(SampleCallbacks* pSample, Gui* pGui)
{
    pGui->addCheckBox("Ray Trace", mRayTrace);
    if (pGui->addButton("Reset Camera"))
    {
        resetCamera();
    }
}

void PlayGround::renderRater(RenderContext* pRenderContext)
{
    mpProgramVars["PerFrameCB"]["gConstColor"] = false;

    ConstantBuffer* pCB = mpProgramVars["PerFrameCB"].get();
    mpDirLight->setIntoProgramVars(mpProgramVars.get(), pCB, "gDirLight");
    mpPointLight->setIntoProgramVars(mpProgramVars.get(), pCB, "gPointLight");

    mpGraphicsState->setProgram(mpProgram);
    pRenderContext->setGraphicsState(mpGraphicsState);
    pRenderContext->setGraphicsVars(mpProgramVars);
    ModelRenderer::render(pRenderContext, mpModel, mpCamera.get());
}

void PlayGround::renderRayTrace(RenderContext* pRenderContext, Fbo* pTargetFbo)
{
    PROFILE("renderRT");
    setPerFrameVars(pTargetFbo);

    pRenderContext->clearUAV(mpRtOut->getUAV().get(), kClearColor);
    mpRtVars->getRayGenVars()->setTexture("gOutput", mpRtOut);

    mpRtRenderer->renderScene(pRenderContext, mpRtVars, mpRtState, uvec3(pTargetFbo->getWidth(), pTargetFbo->getHeight(), 1), mpCamera.get());
    pRenderContext->blit(mpRtOut->getSRV(), pTargetFbo->getRenderTargetView(0));
}

void PlayGround::resetCamera()
{
    if (mpModel)
    {
        // update the camera position
        float Radius = mpModel->getRadius();
        const glm::vec3& ModelCenter = mpModel->getCenter();
        glm::vec3 CamPos = ModelCenter;
        CamPos.z += Radius * 5;

        mpCamera->setPosition(CamPos);
        mpCamera->setTarget(ModelCenter);
        mpCamera->setUpVector(glm::vec3(0, 1, 0));

        // Update the controllers
        mModelViewCameraController.setModelParams(ModelCenter, Radius, 3.5f);
        mFirstPersonCameraController.setCameraSpeed(Radius * 0.25f);
        m6DoFCameraController.setCameraSpeed(Radius * 0.25f);

        mNearZ = std::max(0.1f, mpModel->getRadius() / 750.0f);
        mFarZ = Radius * 10;
    }
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
