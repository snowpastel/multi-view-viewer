#include "llviewerprecompiledheaders.h"
#include "llsecondviewtrace.h"

#include "llappviewer.h"
#include "lldir.h"
#include "lldrawable.h"
#include "llfile.h"
#include "llface.h"
#include "indra_constants.h"
#include "llframetimer.h"
#include "llinventorymodel.h"
#include "llviewercamera.h"
#include "llviewerinventory.h"
#include "llviewerjointattachment.h"
#include "llviewerregion.h"
#include "llvoavatar.h"
#include "llvovolume.h"

#include <cstdlib>
#include <iomanip>
#include <locale>
#include <map>

namespace LLSecondViewTrace
{
namespace
{
struct BakedFace
{
    const LLVertexBuffer* buffer;
    LLMatrix4 local;
    U32 frame;
};
std::map<const LLFace*, BakedFace> baked_faces;
bool enabled()
{
    static const bool on = [] {
        const char* value = std::getenv("PJ0225_FACE_TRACE");
        return value && std::string_view(value) == "1";
    }();
    return on;
}
llofstream output;
bool active = false;
bool failed = false;
FrameGates gates;
F64 started = 0.0;
F64 last_flush = 0.0;

void poseColumns(std::ostream& out, const Pose* pose)
{
    if (!pose)
    {
        out << ",,,,,,,";
        return;
    }
    for (F32 component : pose->position.mV) out << ',' << component;
    for (F32 component : pose->rotation.mQ) out << ',' << component;
}

void partRows(const Frame& frame, const Gate& gate, const LLUUID& root,
              const std::string& name, LLViewerObject& object)
{
    if (object.isDead()) return;
    LLDrawable* drawable = object.mDrawable;
    LLVOVolume* volume = drawable ? drawable->getVOVolume() : nullptr;
    Pose cached, rendered;
    const Pose* geometry = nullptr;
    S32 face_index = -1;
    S64 bake_frame = -1;
    if (drawable)
    {
        cached = {drawable->getWorldPosition(), drawable->getWorldRotation()};
    }
    if (volume && !volume->isRiggedMesh() && volume->getVolume())
    {
        // Baked geometry can lag behind the drawable's current local transform.
        for (S32 i = 0; i < drawable->getNumFaces(); ++i)
        {
            LLFace* face = drawable->getFace(i);
            auto bake = baked_faces.find(face);
            if (bake == baked_faces.end() || !face->getGeomCount() ||
                bake->second.buffer != face->getVertexBuffer()) continue;
            const LLMatrix4* model = nullptr;
            if (drawable->isState(LLDrawable::ANIMATED_CHILD))
                model = &drawable->getWorldMatrix();
            else if (drawable->isActive())
                model = &drawable->getRenderMatrix();
            else if (drawable->getRegion())
                model = &drawable->getRegion()->mRenderMatrix;
            if (!model) continue;
            rendered = geometryPose(bake->second.local, *model);
            geometry = &rendered;
            face_index = i;
            bake_frame = bake->second.frame;
            break;
        }
    }
    writeRow(output, frame, gate, root, object.getID(), name,
             volume ? S32(volume->isRiggedMesh()) : -1,
             volume ? volume->getLOD() : -1, drawable ? drawable->getGeneration() : -1,
             drawable ? &cached : nullptr, geometry, face_index, bake_frame);
    for (const auto& child : object.getChildren())
    {
        if (child) partRows(frame, gate, root, name, *child.get());
    }
}
}

Pose cachedJoint(LLJoint& joint)
{
    // The ordinary world getters update the hierarchy on demand.
    return {joint.getLastWorldPosition(), joint.getLastWorldRotation()};
}

Pose geometryPose(const LLMatrix4& local, const LLMatrix4& model)
{
    // LLMatrix4 uses row vectors. Local geometry can include non-uniform scale.
    LLMatrix4 combined = local;
    combined *= model;
    LLVector3 x(combined.mMatrix[0]), y(combined.mMatrix[1]), z(combined.mMatrix[2]);
    x.normalize();
    y.normalize();
    z.normalize();
    LLMatrix3 axes;
    axes.setRows(x, y, z);
    return {combined.getTranslation(), axes.quaternion()};
}

void writeHeader(std::ostream& out)
{
    out << "update_frame,display_frame,elapsed_s,head_in_main_fov,main_distance_m,"
           "detailed_update,avatar_visible,needs_anim_update,root_move_requested,"
           "root_id,part_id,item_name,rigged,lod,drawable_move_generation,face_index,vertex_bake_frame";
    for (const char* source : {"head", "skull", "drawable", "render"})
        for (const char* axis : {"x", "y", "z", "qx", "qy", "qz", "qw"})
            out << ',' << source << '_' << axis;
    out << ",view_mode,attachment_point_id,attachment_point_name";
    for (const char* axis : {"x", "y", "z", "qx", "qy", "qz", "qw"})
        out << ",attachment_" << axis;
    out << '\n';
}

void writeRow(std::ostream& out, const Frame& frame, const Gate& gate,
              const LLUUID& root, const LLUUID& part, const std::string& name,
              S32 rigged, S32 lod, S32 generation, const Pose* drawable, const Pose* render, S32 face_index, S64 bake_frame)
{
    out << frame.update_frame << ',' << frame.display_frame << ',' << std::setprecision(12)
        << frame.elapsed_s << ',' << int(frame.head_in_main) << ',' << frame.main_distance
        << ',' << gate.detailed << ',' << gate.visible << ',' << gate.needs_anim
        << ',' << gate.requested << ',' << root << ',' << part << ",\"";
    for (char c : name)
    {
        if (c == '"') out << '"';
        out << c;
    }
    out << "\"," << rigged << ',' << lod << ',' << generation << ',' << face_index << ',' << bake_frame;
    poseColumns(out, &frame.head);
    poseColumns(out, frame.has_skull ? &frame.skull : nullptr);
    poseColumns(out, drawable);
    poseColumns(out, render);
    out << ',' << (frame.face_view ? "face" : "body") << ',';
    if (frame.point)
        out << frame.point->id << ',' << std::quoted(frame.point->name, '"', '"');
    else
        out << ',';
    poseColumns(out, frame.point ? &frame.point->pose : nullptr);
    out << '\n';
}

void baked(LLFace& face, const LLMatrix4& local)
{
    if (!enabled()) return;
    LLViewerObject* object = face.getViewerObject();
    if (!object) return;
    LLViewerObject* root = object->getRootEdit();
    if (!root || !tracksPoint(ATTACHMENT_ID_FROM_STATE(root->getAttachmentState()))) return;
    auto* avatar = root->getAvatar();
    if (avatar && avatar->isSelf())
        baked_faces.insert_or_assign(&face, BakedFace{face.getVertexBuffer(), local, LLFrameTimer::getFrameCount()});
}

void forget(const LLFace& face)
{
    if (!baked_faces.empty()) baked_faces.erase(&face);
}

void idle(bool detailed, bool visible, bool needs_anim)
{
    if (!active) return;
    gates.begin(LLFrameTimer::getFrameCount(), detailed, visible, needs_anim);
}

void attachment(const LLUUID& root, bool requested)
{
    if (active) gates.mark(root, requested);
}

void stop()
{
    if (output.is_open()) output.close();
    active = false;
    failed = false;
    gates = {};
}

void sample(LLVOAvatar& avatar, const LLViewerCamera& main_camera, bool face_view)
{
    if (!enabled() || failed) return;
    if (!avatar.mHeadp) return;

    const F64 now = LLFrameTimer::getElapsedSeconds();
    if (!active)
    {
        const std::string path = gDirUtilp->getExpandedFilename(LL_PATH_LOGS,
            "face-pose-" + LLUUID::generateNewID().asString() + ".csv");
        output.clear();
        output.open(path);
        if (!output)
        {
            LL_WARNS() << "Cannot open face pose trace " << path << LL_ENDL;
            failed = true;
            return;
        }
        output.imbue(std::locale::classic());
        writeHeader(output);
        started = last_flush = now;
        active = true;
        LL_INFOS() << "Face pose trace " << path << LL_ENDL;
    }

    const Pose head = cachedJoint(*avatar.mHeadp);
    const LLVector3 delta = head.position - main_camera.getOrigin();
    const F32 depth = delta * main_camera.getAtAxis();
    const F32 half_height = depth * tanf(main_camera.getView() * 0.5f);
    const bool in_view = depth >= main_camera.getNear() && depth <= main_camera.getFar()
        && fabsf(delta * main_camera.getLeftAxis()) <= half_height * main_camera.getAspect()
        && fabsf(delta * main_camera.getUpAxis()) <= half_height;
    auto skull = avatar.mAttachmentPoints.find(SKULL);
    const bool has_skull = skull != avatar.mAttachmentPoints.end() && skull->second;
    Frame frame{LLFrameTimer::getFrameCount(), gFrameCount, now - started, in_view, delta.length(),
                head, has_skull ? cachedJoint(*skull->second) : Pose{}, face_view, nullptr, has_skull};
    bool wrote_root = false;
    for (S32 id : {SKULL, NOSE})
    {
        auto it = avatar.mAttachmentPoints.find(id);
        if (it == avatar.mAttachmentPoints.end() || !it->second || it->second->mAttachedObjects.empty()) continue;
        AttachmentPoint point{id, it->second->getName(), cachedJoint(*it->second)};
        frame.point = &point;
        for (const auto& pointer : it->second->mAttachedObjects)
        {
            LLViewerObject* root = pointer.get();
            if (!root || root->isDead()) continue;
            const Gate gate = gates.read(frame.update_frame, root->getID());
            auto* item = gInventory.getItem(root->getAttachmentItemID());
            partRows(frame, gate, root->getID(), item ? item->getName() : "", *root);
            wrote_root = true;
        }
        frame.point = nullptr;
    }
    if (!wrote_root)
        writeRow(output, frame, gates.read(frame.update_frame, LLUUID::null), LLUUID::null, LLUUID::null, "", -1, -1, -1, nullptr, nullptr, -1, -1);
    if (now - last_flush >= 1.0)
    {
        output.flush();
        last_flush = now;
    }
    if (!output)
    {
        LL_WARNS() << "Write failed in face pose trace" << LL_ENDL;
        stop();
        failed = true;
    }
}
}
