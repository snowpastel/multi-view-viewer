#pragma once

#include "llviewercamera.h"
#include "pipeline.h"

// Bind the scene targets and visibility slot as a pair for the whole inset pass.
class LLSecondViewPass final
{
public:
    LLSecondViewPass()
        : mTargets(gPipeline.mRT), mCamera(LLViewerCamera::sCurCameraID),
          mOcclusion(LLPipeline::sUseOcclusion)
    {
        gPipeline.mRT = &gPipeline.mSecondViewRT;
        LLViewerCamera::sCurCameraID = LLViewerCamera::CAMERA_SECOND_VIEW;
        LLPipeline::sUseOcclusion = 0;
    }

    ~LLSecondViewPass()
    {
        gPipeline.mRT = mTargets;
        LLViewerCamera::sCurCameraID = mCamera;
        LLPipeline::sUseOcclusion = mOcclusion;
    }

    LLSecondViewPass(const LLSecondViewPass&) = delete;
    LLSecondViewPass& operator=(const LLSecondViewPass&) = delete;

private:
    LLPipeline::RenderTargetPack* mTargets;
    LLViewerCamera::eCameraID mCamera;
    S32 mOcclusion;
};
