#ifndef LL_LLSECONDVIEWLOD_H
#define LL_LLSECONDVIEWLOD_H

#include "lluuid.h"
#include <map>
#include <algorithm>

class LLJoint;

// Both views use the same FOV, radius and monotonically distance-based LOD
// curve. The nearer valid distance therefore gives the finer shared mesh.
class LLSecondViewLOD
{
public:
    static bool isHeadJoint(LLJoint* joint, const LLJoint* head);

    using Requests = std::map<LLUUID, F32>;

    F32 distanceFor(const LLUUID& id, F32 main_distance) const
    {
        auto it = mRequests.find(id);
        if (it == mRequests.end())
        {
            return main_distance;
        }
        return main_distance > 0.f ? std::min(main_distance, it->second) : it->second;
    }

    template<typename Refresh>
    void replace(Requests next, Refresh refresh)
    {
        // Publish before updating geometry: calcLOD reads the new request.
        mRequests.swap(next);
        for (const auto& entry : mRequests)
        {
            refresh(entry.first);
        }
        for (const auto& entry : next)
        {
            if (!mRequests.count(entry.first))
            {
                refresh(entry.first);
            }
        }
    }

private:
    Requests mRequests;
};

// Extra distance applies only to the mesh LOD decision, never visibility,
// animation, alpha sorting or streaming the world from the main camera.
F32 secondViewMeshLODDistance(const LLUUID& id, F32 main_distance);

#endif
