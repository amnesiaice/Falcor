#include "PlayGround.h"

const std::string PlayGround::skDefaultModel = "Arcade/Arcade.fbx";

CameraController& PlayGround::getActiveCameraController()
{
    return mModelViewCameraController;
}

void PlayGround::onLoad(SampleCallbacks* pSample, RenderContext* pRenderContext)
{
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

    mpProgramVars = GraphicsVars::create(mpProgram->getReflector());
    mpGraphicsState = GraphicsState::create();
    mpGraphicsState->setProgram(mpProgram);

    //load model
    mpModel = Model::createFromFile(skDefaultModel.c_str(), Model::LoadFlags::None);
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
    mpGraphicsState->setFbo(pTargetFbo);

    if (mpModel)
    {
        mpCamera->setDepthRange(mNearZ, mFarZ);
        getActiveCameraController().update();

        mpProgramVars["PerFrameCB"]["gConstColor"] = false;

        ConstantBuffer* pCB = mpProgramVars["PerFrameCB"].get();
        mpDirLight->setIntoProgramVars(mpProgramVars.get(), pCB, "gDirLight");
        mpPointLight->setIntoProgramVars(mpProgramVars.get(), pCB, "gPointLight");

        mpGraphicsState->setProgram(mpProgram);
        pRenderContext->setGraphicsState(mpGraphicsState);
        pRenderContext->setGraphicsVars(mpProgramVars);
        ModelRenderer::render(pRenderContext, mpModel, mpCamera.get());
    }
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
