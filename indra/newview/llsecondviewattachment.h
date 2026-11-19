#pragma once

class LLJoint;
class LLViewerObject;

namespace LLSecondViewAttachment
{
    inline bool viewActive(bool visible, bool minimized)
    {
        return visible && !minimized;
    }

    inline bool followPoint(bool active, bool hud) { return active && !hud; }

    // Each root enters the existing drawable/bridge move path at most once.
    bool requestMove(bool normal_move, bool follow_point, LLJoint& joint, const LLViewerObject& root);
}
