#include "llviewerprecompiledheaders.h"
#include "llfloatersecondview.h"
#include "llfocusmgr.h"
#include "llcheckboxctrl.h"
#include "llbutton.h"
#include "llviewercamera.h"
#include "llrender2dutils.h"
#include "llviewerwindow.h"
#include "llvoavatarself.h"
#include "pipeline.h"

bool LLFloaterSecondView::postBuild()
{
    mSunShadows = getChild<LLCheckBoxCtrl>("sun_shadows");
    getChild<LLButton>("body")->setCommitCallback([this](LLUICtrl*, const LLSD&) { selectFace(false); });
    getChild<LLButton>("face")->setCommitCallback([this](LLUICtrl*, const LLSD&) { selectFace(true); });
    getChild<LLButton>("reset")->setCommitCallback([this](LLUICtrl*, const LLSD&) { mFraming.reset(); });
    selectFace(false);
    return LLFloater::postBuild();
}

void LLFloaterSecondView::selectFace(bool face)
{
    mFraming.selectFace(face);
    getChild<LLButton>("body")->setToggleState(!face);
    getChild<LLButton>("face")->setToggleState(face);
}

bool LLFloaterSecondView::sunShadowsEnabled() const
{
    return mSunShadows->get();
}

LLRect LLFloaterSecondView::imageRect() const
{
    // Leave the native edge grips, title and the two rows of controls clickable.
    LLRect rect(4, getRect().getHeight() - 22, getRect().getWidth() - 4, 52);
    // On an ultrawide display a very short window can exceed LLCamera's
    // supported aspect ratios. Center the largest undistorted image in that case.
    const LLVector2& scale = gViewerWindow->getDisplayScale();
    const F32 pixel_ratio = scale.mV[VX] / scale.mV[VY];
    const S32 width = llmin(rect.getWidth(), llfloor(rect.getHeight() * MAX_ASPECT_RATIO / pixel_ratio));
    const S32 height = llmin(rect.getHeight(), llfloor(rect.getWidth() * pixel_ratio / MIN_ASPECT_RATIO));
    rect.mLeft += (rect.getWidth() - width) / 2;
    rect.mBottom += (rect.getHeight() - height) / 2;
    rect.mRight = rect.mLeft + width;
    rect.mTop = rect.mBottom + height;
    return rect;
}

bool LLFloaterSecondView::updateRenderTarget()
{
    const LLRect image = imageRect();
    const LLVector2& scale = gViewerWindow->getDisplayScale();
    const F32 width = llmax(1.f, image.getWidth() * scale.mV[VX]);
    const F32 height = llmax(1.f, image.getHeight() * scale.mV[VY]);
    mDisplayAspect = width / height;
    const S32 limit = llmin(2048, gGLManager.mGLMaxTextureSize);
    const F32 factor = llmin(1.f, F32(limit) / llmax(width, height));
    const S32 target_width = llmax(1, ll_round(width * factor));
    const S32 target_height = llmax(1, ll_round(height * factor));
    if (target_width != mTargetWidth || target_height != mTargetHeight)
    {
        mTargetWidth = target_width;
        mTargetHeight = target_height;
        mResizeTimer.start();
    }
    // Use the new projection immediately. Avoid reallocating all scene textures
    // on every mouse event while the window is being resized.
    if (gPipeline.mSecondViewImage.isComplete() && mResizeTimer.getElapsedTimeF32() < 0.15f)
    {
        return true;
    }
    return gPipeline.ensureSecondViewTargets(mTargetWidth, mTargetHeight);
}

void LLFloaterSecondView::onOpen(const LLSD&)
{
    if (!mPositioned && getParent())
    {
        const LLRect area = gFloaterView->getSnapRect();
        setOrigin(area.mRight - getRect().getWidth() - 8, area.mTop - getRect().getHeight() - 8);
        gFloaterView->adjustToFitScreen(this, false);
        mPositioned = true;
    }
}

void LLFloaterSecondView::onClose(bool app_quitting)
{
    if (hasMouseCapture())
    {
        gFocusMgr.setMouseCapture(nullptr);
    }
    gPipeline.releaseSecondViewTargets();
    LLFloater::onClose(app_quitting);
}

void LLFloaterSecondView::draw()
{
    LLFloater::draw();
    if (isMinimized() || !isAgentAvatarValid() || !gPipeline.mSecondViewImage.isComplete())
    {
        return;
    }

    LLGLDisable blend(GL_BLEND);
    gPipeline.mSecondViewImage.bindTexture(0, 0);
    gGL.color4f(1.f, 1.f, 1.f, 1.f);
    gGL.pushMatrix();
    const LLRect image = imageRect();
    gGL.translatef((F32)image.mLeft, (F32)image.mBottom, 0.f);
    gl_rect_2d_simple_tex(image.getWidth(), image.getHeight());
    gGL.popMatrix();
    gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
}

bool LLFloaterSecondView::handleMouseDown(S32 x, S32 y, MASK mask)
{
    if (isMinimized() || !imageRect().pointInRect(x, y))
    {
        return LLFloater::handleMouseDown(x, y, mask);
    }
    bringToFront(x, y);
    mMouseX = x;
    mMouseY = y;
    gFocusMgr.setMouseCapture(this);
    return true;
}

bool LLFloaterSecondView::handleMouseUp(S32 x, S32 y, MASK mask)
{
    if (!hasMouseCapture())
    {
        return LLFloater::handleMouseUp(x, y, mask);
    }
    gFocusMgr.setMouseCapture(nullptr);
    return true;
}

bool LLFloaterSecondView::handleHover(S32 x, S32 y, MASK mask)
{
    if (!hasMouseCapture())
    {
        return LLFloater::handleHover(x, y, mask);
    }
    const LLRect image = imageRect();
    const F32 radians_per_pixel = F_TWO_PI / image.getWidth();
    const S32 dx = x - mMouseX;
    const S32 dy = y - mMouseY;
    const bool alt_zoom = mask == MASK_ALT;
    LLSecondViewOrbit& orbit = mFraming.current();
    if (mask == MASK_SHIFT)
    {
        orbit.pan(dx, dy, image.getHeight(), LLViewerCamera::instance().getDefaultFOV());
    }
    else
    {
        orbit.rotate(-dx * radians_per_pixel, alt_zoom ? 0.f : -dy * radians_per_pixel);
    }
    if (alt_zoom)
    {
        orbit.zoom(std::pow(0.99f, static_cast<F32>(dy)));
    }
    mMouseX = x;
    mMouseY = y;
    return true;
}

bool LLFloaterSecondView::handleScrollWheel(S32 x, S32 y, S32 clicks)
{
    if (isMinimized() || (!hasMouseCapture() && !imageRect().pointInRect(x, y)))
    {
        return LLFloater::handleScrollWheel(x, y, clicks);
    }
    mFraming.current().zoom(std::pow(1.1892071f, static_cast<F32>(clicks)));
    return true;
}
