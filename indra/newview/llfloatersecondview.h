#pragma once

#include "llfloater.h"
#include "llframetimer.h"
#include "llsecondvieworbit.h"

class LLCheckBoxCtrl;

class LLFloaterSecondView final : public LLFloater
{
public:
    explicit LLFloaterSecondView(const LLSD& key) : LLFloater(key) {}
    bool postBuild() override;
    static bool isSecondViewActive();
    bool sunShadowsEnabled() const;
    void onOpen(const LLSD& key) override;
    void onClose(bool app_quitting) override;
    void draw() override;
    bool handleMouseDown(S32 x, S32 y, MASK mask) override;
    bool handleMouseUp(S32 x, S32 y, MASK mask) override;
    bool handleHover(S32 x, S32 y, MASK mask) override;
    bool handleScrollWheel(S32 x, S32 y, S32 clicks) override;
    const LLSecondViewFraming& framing() const { return mFraming; }
    bool updateRenderTarget();
    F32 imageAspect() const { return mDisplayAspect; }

private:
    LLRect imageRect() const;
    void selectFace(bool face);
    LLCheckBoxCtrl* mSunShadows = nullptr;
    LLSecondViewFraming mFraming;
    LLFrameTimer mResizeTimer;
    F32 mDisplayAspect = 4.f / 3.f;
    S32 mTargetWidth = 1;
    S32 mTargetHeight = 1;
    S32 mMouseX = 0;
    S32 mMouseY = 0;
    bool mPositioned = false;
};
