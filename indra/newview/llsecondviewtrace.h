#pragma once

#include "lluuid.h"
#include "llquaternion.h"
#include "v3math.h"
#include <iosfwd>
#include <map>

class LLVOAvatar;
class LLViewerCamera;
class LLMatrix4;
class LLFace;
class LLJoint;

namespace LLSecondViewTrace
{
    constexpr S32 SKULL = 2, NOSE = 17; // avatar_lad.xml
    inline bool tracksPoint(S32 id) { return id == SKULL || id == NOSE; }
    struct Pose
    {
        LLVector3 position;
        LLQuaternion rotation;
    };
    struct AttachmentPoint
    {
        S32 id;
        std::string name;
        Pose pose;
    };
    struct Frame
    {
        U32 update_frame;
        U32 display_frame;
        F64 elapsed_s;
        bool head_in_main;
        F32 main_distance;
        Pose head;
        Pose skull;
        bool face_view = true;
        const AttachmentPoint* point = nullptr;
        bool has_skull = true;
    };
    struct Gate
    {
        S32 detailed = -1;
        S32 visible = -1;
        S32 needs_anim = -1;
        S32 requested = -1;
    };
    class FrameGates
    {
    public:
        void begin(U32 frame, bool detailed, bool visible, bool needs_anim)
        {
            mFrame = frame;
            mGate = {S32(detailed), S32(visible), S32(needs_anim), -1};
            mRequests.clear();
        }
        void mark(const LLUUID& root, bool requested) { mRequests[root] = requested; }
        Gate read(U32 frame, const LLUUID& root) const
        {
            if (frame != mFrame) return {};
            Gate result = mGate;
            auto it = mRequests.find(root);
            if (it != mRequests.end()) result.requested = S32(it->second);
            return result;
        }
    private:
        U32 mFrame = 0;
        Gate mGate;
        std::map<LLUUID, bool> mRequests;
    };
    void idle(bool detailed, bool visible, bool needs_anim);
    void attachment(const LLUUID& root, bool requested);
    void sample(LLVOAvatar& avatar, const LLViewerCamera& main_camera, bool face_view);
    void stop();
    void baked(LLFace& face, const LLMatrix4& local);
    void forget(const LLFace& face);

    Pose cachedJoint(LLJoint& joint);
    Pose geometryPose(const LLMatrix4& local, const LLMatrix4& model);
    void writeHeader(std::ostream& out);
    void writeRow(std::ostream& out, const Frame& frame, const Gate& gate,
                  const LLUUID& root, const LLUUID& part, const std::string& item_name,
                  S32 rigged, S32 lod, S32 generation, const Pose* drawable, const Pose* render, S32 face_index, S64 bake_frame);
}
