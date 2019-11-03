/***************************************************************************
# Copyright (c) 2015, NVIDIA CORPORATION. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#  * Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
#  * Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#  * Neither the name of NVIDIA CORPORATION nor the names of its
#    contributors may be used to endorse or promote products derived
#    from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
***************************************************************************/
#include "HybridRenderer.h"

const std::string HybridRenderer::skDefaultScene = "Arcade/Arcade.fscene";

Gui::DropdownList kSampleCountList =
{
    { 2, "2" },
    { 4, "4" },
    { 8, "8" },
};

const Gui::DropdownList aaModeList =
{
    { 0, "None"},
    { 1, "TAA" },
    { 2, "FXAA" }
};

const Gui::DropdownList renderPathList =
{
    { 0, "Deferred"},
    { 1, "Forward"},
};

void HybridRenderer::initShadowPass(uint32_t windowWidth, uint32_t windowHeight)
{
    mpShadowPass = CascadedShadowMaps::create(mpSceneRenderer->getScene()->getLight(0), 2048, 2048, windowWidth, windowHeight, mpSceneRenderer->getScene()->shared_from_this());
    mpShadowPass->setFilterMode(CsmFilterHwPcf);
    mpShadowPass->setVsmLightBleedReduction(0.3f);
    mpShadowPass->setVsmMaxAnisotropy(4);
    mpShadowPass->setEvsmBlur(7, 3);
}

void HybridRenderer::setSceneSampler(uint32_t maxAniso)
{
    Scene* pScene = mpSceneRenderer->getScene().get();
    Sampler::Desc samplerDesc;
    samplerDesc.setAddressingMode(Sampler::AddressMode::Wrap, Sampler::AddressMode::Wrap, Sampler::AddressMode::Wrap).setFilterMode(Sampler::Filter::Linear, Sampler::Filter::Linear, Sampler::Filter::Linear).setMaxAnisotropy(maxAniso);
    mpSceneSampler = Sampler::create(samplerDesc);
    pScene->bindSampler(mpSceneSampler);
}

void HybridRenderer::applyCustomSceneVars(const Scene* pScene, const std::string& filename)
{
    std::string folder = getDirectoryFromFile(filename);

    Scene::UserVariable var = pScene->getUserVariable("sky_box");
    if (var.type == Scene::UserVariable::Type::String) initSkyBox(folder + '/' + var.str);

    var = pScene->getUserVariable("opacity_scale");
    if (var.type == Scene::UserVariable::Type::Double) mOpacityScale = (float)var.d64;
}

void HybridRenderer::initScene(SampleCallbacks* pSample, Scene::SharedPtr pScene)
{
    if (pScene->getCameraCount() == 0)
    {
        // Place the camera above the center, looking slightly downwards
        const Model* pModel = pScene->getModel(0).get();
        Camera::SharedPtr pCamera = Camera::create();

        vec3 position = pModel->getCenter();
        float radius = pModel->getRadius();
        position.y += 0.1f * radius;
        pScene->setCameraSpeed(radius * 0.03f);

        pCamera->setPosition(position);
        pCamera->setTarget(position + vec3(0, -0.3f, -radius));
        pCamera->setDepthRange(0.1f, radius * 10);

        pScene->addCamera(pCamera);
    }

    if (pScene->getLightCount() == 0)
    {
        // Create a directional light
        DirectionalLight::SharedPtr pDirLight = DirectionalLight::create();
        pDirLight->setWorldDirection(vec3(-0.189f, -0.861f, -0.471f));
        pDirLight->setIntensity(vec3(1, 1, 0.985f) * 10.0f);
        pDirLight->setName("DirLight");
        pScene->addLight(pDirLight);
    }

    if (pScene->getLightProbeCount() > 0)
    {
        const LightProbe::SharedPtr& pProbe = pScene->getLightProbe(0);
        pProbe->setRadius(pScene->getRadius());
        pProbe->setPosW(pScene->getCenter());
        pProbe->setSampler(mpSceneSampler);
    }

    mpSceneRenderer = SceneRenderer::create(pScene);
    mpSceneRenderer->setCameraControllerType(SceneRenderer::CameraControllerType::FirstPerson);
    mpSceneRenderer->toggleStaticMaterialCompilation(mPerMaterialShader);
    setSceneSampler(mpSceneSampler ? mpSceneSampler->getMaxAnisotropy() : 4);
    setActiveCameraAspectRatio(pSample->getCurrentFbo()->getWidth(), pSample->getCurrentFbo()->getHeight());

    mpDepthPass = DepthPass::create();
    mpDepthPass->setScene(pScene);

    mpForwardPass = ForwardLightingPass::create();
    mpForwardPass->setScene(pScene);
    mpForwardPass->usePreGeneratedDepthBuffer(true);

    auto pTargetFbo = pSample->getCurrentFbo();
    initShadowPass(pTargetFbo->getWidth(), pTargetFbo->getHeight());
    initAA(pSample);

    mpSSAO = SSAO::create(float2(pTargetFbo->getWidth(), pTargetFbo->getHeight()));
    mpSSAO->setScene(pScene);

    mpBlitPass = BlitPass::create();
    
    mpToneMapper = ToneMapping::create(ToneMapping::Operator::Aces);

    pSample->setCurrentTime(0);
}

void HybridRenderer::resetScene()
{
    mpSceneRenderer = nullptr;
    mpSkyPass = nullptr;
}

void HybridRenderer::loadModel(SampleCallbacks* pSample, const std::string& filename, bool showProgressBar)
{
    Mesh::resetGlobalIdCounter();
    resetScene();

    ProgressBar::SharedPtr pBar;
    if (showProgressBar)
    {
        pBar = ProgressBar::create("Loading Model");
    }

    Model::SharedPtr pModel = Model::createFromFile(filename.c_str());
    if (!pModel) return;
    Scene::SharedPtr pScene = Scene::create();
    pScene->addModelInstance(pModel, "instance");

    initScene(pSample, pScene);
}

void HybridRenderer::loadScene(SampleCallbacks* pSample, const std::string& filename, bool showProgressBar)
{
    Mesh::resetGlobalIdCounter();
    resetScene();

    ProgressBar::SharedPtr pBar;
    if (showProgressBar)
    {
        pBar = ProgressBar::create("Loading Scene", 100);
    }

    Scene::SharedPtr pScene = Scene::loadFromFile(filename);

    if (pScene != nullptr)
    {
        initScene(pSample, pScene);
        applyCustomSceneVars(pScene.get(), filename);
        applyCsSkinningMode();
    }
}

void HybridRenderer::initSkyBox(const std::string& name)
{
    Sampler::Desc samplerDesc;
    samplerDesc.setFilterMode(Sampler::Filter::Linear, Sampler::Filter::Linear, Sampler::Filter::Linear);
    Sampler::SharedPtr pSampler = Sampler::create(samplerDesc);
    mpSkyPass = SkyBox::create(name, true, pSampler);
}

void HybridRenderer::updateLightProbe(const LightProbe::SharedPtr& pLight)
{
    Scene::SharedPtr pScene = mpSceneRenderer->getScene();

    // Remove existing light probes
    while (pScene->getLightProbeCount() > 0)
    {
        pScene->deleteLightProbe(0);
    }

    pLight->setRadius(pScene->getRadius());
    pLight->setPosW(pScene->getCenter());
    pLight->setSampler(mpSceneSampler);
    pScene->addLightProbe(pLight);
}

void HybridRenderer::initAA(SampleCallbacks* pSample)
{
    mTAA.pTAA = TemporalAA::create();
    mpFXAA = FXAA::create();
    applyAaMode(pSample);
}

void HybridRenderer::onLoad(SampleCallbacks* pSample, RenderContext* pRenderContext)
{
    loadScene(pSample, skDefaultScene, true);
}

void HybridRenderer::renderSkyBox(RenderContext* pContext, Fbo::SharedPtr pTargetFbo)
{
    if (mpSkyPass)
    {
        PROFILE("skyBox");
        mpSkyPass->render(pContext, mpSceneRenderer->getScene()->getActiveCamera().get(), pTargetFbo);
    }
}

void HybridRenderer::beginFrame(RenderContext* pContext, Fbo* pTargetFbo, uint64_t frameId)
{
    pContext->clearFbo(mpMainFbo.get(), glm::vec4(0.0f, 0.0f, 0.0f, 1.0f), 1, 0, FboAttachmentType::All);
    pContext->clearFbo(mpPostProcessFbo.get(), glm::vec4(), 1, 0, FboAttachmentType::Color);

    if (mAAMode == AAMode::TAA)
    {
        glm::vec2 targetResolution = glm::vec2(pTargetFbo->getWidth(), pTargetFbo->getHeight());
        pContext->clearRtv(mpMainFbo->getColorTexture(2)->getRTV().get(), vec4(0));

        //  Select the sample pattern and set the camera jitter

    }
}

void HybridRenderer::endFrame(RenderContext* pContext)
{
    pContext->popGraphicsState();
}

void HybridRenderer::toneMapping(RenderContext* pContext, Fbo::SharedPtr pTargetFbo)
{
    PROFILE("toneMapping");
    mpToneMapper->execute(pContext, mpResolveFbo->getColorTexture(0), pTargetFbo);
}

void HybridRenderer::depthPass(RenderContext* pContext, Fbo::SharedPtr pTargetFbo)
{
    PROFILE("depthPass");
    mpDepthPass->execute(pContext, pTargetFbo);
}

void HybridRenderer::forwardLightingPass(RenderContext* pContext, Fbo::SharedPtr pTargetFbo)
{
    PROFILE("lightingPass");
    mpForwardPass->execute(pContext, mpShadowPass->getVisibilityBuffer(), pTargetFbo);
}

void HybridRenderer::shadowPass(RenderContext* pContext, Texture::SharedPtr pDepthTexture)
{
    PROFILE("shadowPass");
    const Camera* pCamera = mpSceneRenderer->getScene()->getActiveCamera().get();
    mpShadowPass->generateVisibilityBuffer(pContext, pCamera, pDepthTexture);
    pContext->flush();
}

void HybridRenderer::runTAA(RenderContext* pContext, Fbo::SharedPtr pColorFbo)
{
    if(mAAMode == AAMode::TAA)
    {
        PROFILE("TAA");
        //  Get the Current Color and Motion Vectors
        const Texture::SharedPtr pCurColor = pColorFbo->getColorTexture(0);
        const Texture::SharedPtr pMotionVec = mpMainFbo->getColorTexture(2);

        //  Get the Previous Color
        const Texture::SharedPtr pPrevColor = mTAA.getInactiveFbo()->getColorTexture(0);

        //  Execute the Temporal Anti-Aliasing
        pContext->getGraphicsState()->pushFbo(mTAA.getActiveFbo());
        mTAA.pTAA->execute(pContext, pCurColor, pPrevColor, pMotionVec);
        pContext->getGraphicsState()->popFbo();

        //  Copy over the Anti-Aliased Color Texture
        pContext->blit(mTAA.getActiveFbo()->getColorTexture(0)->getSRV(0, 1), pColorFbo->getColorTexture(0)->getRTV());

        //  Swap the Fbos
        mTAA.switchFbos();
    }
}

void HybridRenderer::ambientOcclusion(RenderContext* pContext, Fbo::SharedPtr pTargetFbo)
{
    if (mEnableSSAO)
    {
        PROFILE("SSAO");
        const Camera* pCamera = mpSceneRenderer->getScene()->getActiveCamera().get();
        Texture::SharedPtr pDepthTex = mpResolveFbo->getDepthStencilTexture();
        Texture::SharedPtr pNormalTex = mpResolveFbo->getColorTexture(1);
        mpSSAO->execute(pContext, pCamera, mpPostProcessFbo->getColorTexture(0), pTargetFbo->getColorTexture(0), pDepthTex, pNormalTex);
    }
}

void HybridRenderer::postProcess(RenderContext* pContext, Fbo::SharedPtr pTargetFbo)
{
    PROFILE("postProcess");

    Fbo::SharedPtr pPostProcessDst = mEnableSSAO ? mpPostProcessFbo : pTargetFbo;
    toneMapping(pContext, pPostProcessDst);
    runTAA(pContext, pPostProcessDst); // This will only run if we are in TAA mode
    ambientOcclusion(pContext, pTargetFbo);
    executeFXAA(pContext, pTargetFbo);
}

void HybridRenderer::executeFXAA(RenderContext* pContext, Fbo::SharedPtr pTargetFbo)
{
    if(mAAMode == AAMode::FXAA)
    {
        PROFILE("FXAA");
        pContext->blit(pTargetFbo->getColorTexture(0)->getSRV(), mpResolveFbo->getRenderTargetView(0));
        mpFXAA->execute(pContext, mpResolveFbo->getColorTexture(0), pTargetFbo);
    }
}

void HybridRenderer::onFrameRender(SampleCallbacks* pSample, RenderContext* pRenderContext, const Fbo::SharedPtr& pTargetFbo)
{
    if (mpSceneRenderer)
    {
        beginFrame(pRenderContext, pTargetFbo.get(), pSample->getFrameID());
        {
            PROFILE("updateScene");
            mpSceneRenderer->update(pSample->getCurrentTime());
        }

        depthPass(pRenderContext, mpDepthPassFbo);
        shadowPass(pRenderContext, mpDepthPassFbo->getDepthStencilTexture());
        forwardLightingPass(pRenderContext, mpMainFbo);
        renderSkyBox(pRenderContext, mpMainFbo);
        postProcess(pRenderContext, pTargetFbo);

        endFrame(pRenderContext);
    }
    else
    {
        pRenderContext->clearFbo(pTargetFbo.get(), vec4(0.2f, 0.4f, 0.5f, 1), 1, 0);
    }

}

void HybridRenderer::applyCameraPathState()
{
    const Scene* pScene = mpSceneRenderer->getScene().get();
    if(pScene->getPathCount())
    {
        mUseCameraPath = mUseCameraPath;
        if (mUseCameraPath)
        {
            pScene->getPath(0)->attachObject(pScene->getActiveCamera());
        }
        else
        {
            pScene->getPath(0)->detachObject(pScene->getActiveCamera());
        }
    }
}

bool HybridRenderer::onKeyEvent(SampleCallbacks* pSample, const KeyboardEvent& keyEvent)
{
    if (mpSceneRenderer && keyEvent.type == KeyboardEvent::Type::KeyPressed)
    {
        switch (keyEvent.key)
        {
        case KeyboardEvent::Key::Minus:
            mUseCameraPath = !mUseCameraPath;
            applyCameraPathState();
            return true;
        case KeyboardEvent::Key::O:
            mPerMaterialShader = !mPerMaterialShader;
            mpSceneRenderer->toggleStaticMaterialCompilation(mPerMaterialShader);
            return true;
        }
    }

    return mpSceneRenderer ? mpSceneRenderer->onKeyEvent(keyEvent) : false;
}

void HybridRenderer::onDroppedFile(SampleCallbacks* pSample, const std::string& filename)
{
    if (hasSuffix(filename, ".fscene", false) == false)
    {
        msgBox("You can only drop a scene file into the window");
        return;
    }
    loadScene(pSample, filename, true);
}

bool HybridRenderer::onMouseEvent(SampleCallbacks* pSample, const MouseEvent& mouseEvent)
{
    return mpSceneRenderer ? mpSceneRenderer->onMouseEvent(mouseEvent) : true;
}

void HybridRenderer::onResizeSwapChain(SampleCallbacks* pSample, uint32_t width, uint32_t height)
{
    // Create the post-process FBO and AA resolve Fbo
    Fbo::Desc fboDesc;
    fboDesc.setColorTarget(0, ResourceFormat::RGBA8UnormSrgb);
    mpPostProcessFbo = FboHelper::create2D(width, height, fboDesc);

    applyAaMode(pSample);
    mpShadowPass->onResize(width, height);

    if(mpSceneRenderer)
    {
        setActiveCameraAspectRatio(width, height);
    }
}

void HybridRenderer::applyCsSkinningMode()
{
    if(mpSceneRenderer)
    {
        SkinningCache::SharedPtr pCache = mUseCsSkinning ? SkinningCache::create() : nullptr;
        mpSceneRenderer->getScene()->attachSkinningCacheToModels(pCache);
    }    
}

void HybridRenderer::setActiveCameraAspectRatio(uint32_t w, uint32_t h)
{
    mpSceneRenderer->getScene()->getActiveCamera()->setAspectRatio((float)w / (float)h);
}

void HybridRenderer::createTaaPatternGenerator(uint32_t fboWidth, uint32_t fboHeight)
{
    PatternGenerator::SharedPtr pGenerator;
    switch (mTAASamplePattern)
    {
    case SamplePattern::Halton:
        pGenerator = HaltonSamplePattern::create();
        break;
    case SamplePattern::DX11:
        pGenerator = DxSamplePattern::create();
        break;
    default:
        should_not_get_here();
        pGenerator = nullptr;
    }

    mpSceneRenderer->getScene()->getActiveCamera()->setPatternGenerator(pGenerator, 1.0f/vec2(fboWidth, fboHeight));
}

void HybridRenderer::applyAaMode(SampleCallbacks* pSample)
{
    uint32_t w = pSample->getCurrentFbo()->getWidth();
    uint32_t h = pSample->getCurrentFbo()->getHeight();

    // Common Depth FBO, shared by depth pass, GBuffer pass and forward pass
    Fbo::Desc depthFboDesc;
    depthFboDesc.setDepthStencilTarget(ResourceFormat::D32Float);
    mpDepthPassFbo = FboHelper::create2D(w, h, depthFboDesc);

    // Common FBO desc (2 color outputs - color and normal)
    Fbo::Desc fboDesc;
    fboDesc.setColorTarget(0, ResourceFormat::RGBA32Float).setColorTarget(1, ResourceFormat::RGBA8Unorm);

    // Release the TAA FBOs
    mTAA.resetFbos();

    if (mAAMode == AAMode::TAA)
    {
        fboDesc.setColorTarget(2, ResourceFormat::RG16Float);

        Fbo::Desc taaFboDesc;
        taaFboDesc.setColorTarget(0, ResourceFormat::RGBA8UnormSrgb);
        mTAA.createFbos(w, h, taaFboDesc);
        createTaaPatternGenerator(w, h);
    }
    else
    {
        mpSceneRenderer->getScene()->getActiveCamera()->setPatternGenerator(nullptr);

        if (mAAMode == AAMode::FXAA)
        {
            Fbo::Desc resolveDesc;
            resolveDesc.setColorTarget(0, pSample->getCurrentFbo()->getColorTexture(0)->getFormat());
            mpResolveFbo = FboHelper::create2D(w, h, resolveDesc);
        }
    }

    mpMainFbo = FboHelper::create2D(w, h, fboDesc);
    mpMainFbo->attachDepthStencilTarget(mpDepthPassFbo->getDepthStencilTexture());

    mpResolveFbo = mpMainFbo;
}

void HybridRenderer::onGuiRender(SampleCallbacks* pSample, Gui* pGui)
{
    static const FileDialogFilterVec kImageFilesFilter = { {"bmp"}, {"jpg"}, {"dds"}, {"png"}, {"tiff"}, {"tif"}, {"tga"}, {"hdr"}, {"exr"}  };

    if (pGui->addButton("Load Model"))
    {
        std::string filename;
        if (openFileDialog(Model::kFileExtensionFilters, filename))
        {
            loadModel(pSample, filename, true);
        }
    }

    if (pGui->addButton("Load Scene"))
    {
        std::string filename;
        if (openFileDialog(Scene::kFileExtensionFilters, filename))
        {
            loadScene(pSample, filename, true);
        }
    }

    if (mpSceneRenderer)
    {
        if (pGui->addButton("Load SkyBox Texture"))
        {
            std::string filename;
            if (openFileDialog(kImageFilesFilter, filename))
            {
                initSkyBox(filename);
            }
        }

        if (pGui->beginGroup("Scene Settings"))
        {
            Scene* pScene = mpSceneRenderer->getScene().get();
            float camSpeed = pScene->getCameraSpeed();
            if (pGui->addFloatVar("Camera Speed", camSpeed))
            {
                pScene->setCameraSpeed(camSpeed);
            }

            vec2 depthRange(pScene->getActiveCamera()->getNearPlane(), pScene->getActiveCamera()->getFarPlane());
            if (pGui->addFloat2Var("Depth Range", depthRange, 0, FLT_MAX))
            {
                pScene->getActiveCamera()->setDepthRange(depthRange.x, depthRange.y);
            }

            if (pScene->getPathCount() > 0)
            {
                if (pGui->addCheckBox("Camera Path", mUseCameraPath))
                {
                    applyCameraPathState();
                }
            }

            if (pScene->getLightCount() && pGui->beginGroup("Light Sources"))
            {
                for (uint32_t i = 0; i < pScene->getLightCount(); i++)
                {
                    Light* pLight = pScene->getLight(i).get();
                    pLight->renderUI(pGui, pLight->getName().c_str());
                }
                pGui->endGroup();
            }

            if (pGui->addCheckBox("Use CS for Skinning", mUseCsSkinning))
            {
                applyCsSkinningMode();
            }
            pGui->endGroup();
        }

        if (pGui->beginGroup("Renderer Settings"))
        {
            if (pGui->addCheckBox("Specialize Material Shaders", mPerMaterialShader))
            {
                mpSceneRenderer->toggleStaticMaterialCompilation(mPerMaterialShader);
            }
            pGui->addTooltip("Create a specialized version of the lighting program for each material in the scene");

            uint32_t maxAniso = mpSceneSampler->getMaxAnisotropy();
            if (pGui->addIntVar("Max Anisotropy", (int&)maxAniso, 1, 16))
            {
                setSceneSampler(maxAniso);
            }

            pGui->endGroup();
        }

        //  Anti-Aliasing Controls.
        if (pGui->beginGroup("Anti-Aliasing"))
        {
            bool reapply = false;
            reapply = reapply || pGui->addDropdown("AA Mode", aaModeList, (uint32_t&)mAAMode);

            //  Temporal Anti-Aliasing.
            if (mAAMode == AAMode::TAA)
            {
                if (pGui->beginGroup("TAA"))
                {
                    //  Render the TAA UI.
                    mTAA.pTAA->renderUI(pGui);

                    //  Choose the Sample Pattern for TAA.
                    Gui::DropdownList samplePatternList;
                    samplePatternList.push_back({ (uint32_t)SamplePattern::Halton, "Halton" });
                    samplePatternList.push_back({ (uint32_t)SamplePattern::DX11, "DX11" });
                    pGui->addDropdown("Sample Pattern", samplePatternList, (uint32_t&)mTAASamplePattern);

                    // Disable super-sampling
                    pGui->endGroup();
                }
            }

            if (mAAMode == AAMode::FXAA)
            {
                mpFXAA->renderUI(pGui, "FXAA");
            }

            if (reapply) applyAaMode(pSample);

            pGui->endGroup();
        }

        if (pGui->beginGroup("Light Probes"))
        {
            if (pGui->addButton("Add/Change Light Probe"))
            {
                std::string filename;
                if (openFileDialog(kImageFilesFilter, filename))
                {
                    updateLightProbe(LightProbe::create(pSample->getRenderContext(), filename, true, ResourceFormat::RGBA16Float));
                }
            }

            Scene::SharedPtr pScene = mpSceneRenderer->getScene();
            if (pScene->getLightProbeCount() > 0)
            {
                pGui->addSeparator();
                pScene->getLightProbe(0)->renderUI(pGui);
            }

            pGui->endGroup();
        }

        mpToneMapper->renderUI(pGui, "Tone-Mapping");

        if (pGui->beginGroup("Shadows"))
        {
            mpShadowPass->renderUI(pGui);
            if (pGui->addCheckBox("Visualize Cascades", mVisualizeCascades))
            {
                mpShadowPass->toggleCascadeVisualization(mVisualizeCascades);
            }
            pGui->endGroup();
        }

        if (pGui->beginGroup("SSAO"))
        {
            pGui->addCheckBox("Enable SSAO", mEnableSSAO);
            if (mEnableSSAO)
            {
                mpSSAO->renderUI(pGui);
            }
            pGui->endGroup();
        }

        if (pGui->beginGroup("Transparency"))
        {
            if (pGui->addCheckBox("Enable Transparency", mEnableTransparent))
            {
                // TODO
                assert(false);
            }
            pGui->addFloatVar("Opacity Scale", mOpacityScale, 0, 1);
            pGui->endGroup();
        }

        if (pGui->addCheckBox("Hashed-Alpha Test", mEnableAlphaTest))
        {
            // TODO
            assert(false);
        }
    }
}

#ifdef _WIN32
int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nShowCmd)
#else
int main(int argc, char** argv)
#endif
{
    HybridRenderer::UniquePtr pRenderer = std::make_unique<HybridRenderer>();
    SampleConfig config;
    config.windowDesc.title = "Hybrid Renderer";
    config.windowDesc.resizableWindow = false;
#ifdef _WIN32
    Sample::run(config, pRenderer);
#else
    config.argc = (uint32_t)argc;
    config.argv = argv;
    Sample::run(config, pRenderer);
#endif
    return 0;
}
