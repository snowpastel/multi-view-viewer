#pragma once

#include <algorithm>
#include <cmath>
#include "v3math.h"

// Orbit coordinates relative to the avatar's horizontal facing direction.
class LLSecondViewOrbit
{
public:
    explicit LLSecondViewOrbit(F32 distance = 2.6f) : mDistance(distance) {}

    void pan(S32 dx, S32 dy, S32 height, F32 vertical_fov)
    {
        // Drag the subject with the mouse. Store the translation in body-local
        // coordinates so an adjusted framing follows the wearer as they turn.
        const F32 units_per_pixel = 2.f * mDistance * std::tan(vertical_fov * 0.5f) / std::max(1, height);
        const F32 c = std::cos(mYaw), s = std::sin(mYaw);
        const LLVector3 right(-s, c, 0.f);
        const LLVector3 up(-std::sin(mPitch)*c, -std::sin(mPitch)*s, std::cos(mPitch));
        mPan -= (right * F32(dx) + up * F32(dy)) * units_per_pixel;
    }

    LLVector3 focus(const LLVector3& anchor, F32 heading) const
    {
        const F32 c = std::cos(heading), s = std::sin(heading);
        return anchor + LLVector3(c*mPan.mV[VX] - s*mPan.mV[VY], s*mPan.mV[VX] + c*mPan.mV[VY], mPan.mV[VZ]);
    }

    void rotate(F32 horizontal, F32 vertical)
    {
        mYaw = std::remainder(mYaw + horizontal, F_TWO_PI);
        mPitch = std::clamp(mPitch + vertical, -1.48f, 1.48f);
    }

    void zoom(F32 factor)
    {
        mDistance = std::clamp(mDistance * factor, 0.35f, 32.f);
    }

    LLVector3 offset(F32 heading) const
    {
        F32 horizontal = mDistance * std::cos(mPitch);
        return LLVector3(horizontal * std::cos(heading + mYaw),
                         horizontal * std::sin(heading + mYaw),
                         mDistance * std::sin(mPitch));
    }

private:
    F32 mYaw = 0.f;
    F32 mPitch = 0.f;
    F32 mDistance;
    LLVector3 mPan;
};

// Each preset retains its own orbit and pan for this floater instance.
class LLSecondViewFraming
{
public:
    bool faceSelected() const { return mFaceSelected; }
    void selectFace(bool selected) { mFaceSelected = selected; }
    LLSecondViewOrbit& current() { return mFaceSelected ? mFace : mBody; }

    void reset()
    {
        current() = mFaceSelected ? LLSecondViewOrbit(FACE_DISTANCE) : LLSecondViewOrbit();
    }

    void compose(F32 heading, const LLVector3& body, const LLVector3* head,
                 LLVector3& eye, LLVector3& focus) const
    {
        const bool use_face = mFaceSelected && head;
        const LLSecondViewOrbit& orbit = use_face ? mFace : mBody;
        focus = orbit.focus(use_face ? *head : body, heading);
        eye = focus + orbit.offset(heading);
    }

private:
    static constexpr F32 FACE_DISTANCE = 0.55f;
    LLSecondViewOrbit mBody;
    LLSecondViewOrbit mFace{FACE_DISTANCE};
    bool mFaceSelected = false;
};
