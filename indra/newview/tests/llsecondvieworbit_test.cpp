#include "linden_common.h"
#include "../test/lltut.h"
#include "../llsecondvieworbit.h"
#include "../llsecondviewlod.h"

namespace tut
{
    struct second_view_data {};
    typedef test_group<second_view_data> orbit_group;
    orbit_group second_view_tests("Second camera orbit");
    typedef orbit_group::object orbit_test;

    template<> template<> void orbit_test::test<1>()
    {
        LLSecondViewOrbit camera;
        ensure("starts in front", camera.offset(0.f).mV[VX] > 2.f);
        ensure("turn follows avatar", (camera.offset(F_PI_BY_TWO) - LLVector3(0.f, 2.6f, 0.f)).length() < 0.001f);
    }

    template<> template<> void orbit_test::test<2>()
    {
        LLSecondViewOrbit first, second;
        const LLVector3 original = first.offset(0.f);
        second.rotate(F_PI_BY_TWO, 0.3f);
        second.zoom(0.5f);
        ensure("first camera remains independent", first.offset(0.f) == original);
        ensure("zoom changes distance", std::abs(second.offset(0.f).length() - 1.3f) < 0.001f);
        ensure("orbit moves sideways and upwards", second.offset(0.f).mV[VY] > 0.f && second.offset(0.f).mV[VZ] > 0.f);
    }

    template<> template<> void orbit_test::test<3>()
    {
        LLSecondViewOrbit camera;
        camera.rotate(400.f, 100.f);
        camera.zoom(0.f);
        auto offset = camera.offset(0.f);
        ensure("orbit avoids pole", std::abs(offset.mV[VZ]) < offset.length());
        ensure("near zoom bound", std::abs(offset.length() - 0.35f) < 0.001f);
        camera.zoom(10000.f);
        ensure("far zoom bound", std::abs(camera.offset(0.f).length() - 32.f) < 0.001f);
    }

    template<> template<> void orbit_test::test<4>()
    {
        LLSecondViewFraming framing;
        const LLVector3 body(4.f, 2.f, 1.f), head(4.1f, 2.f, 1.8f);
        LLVector3 eye, focus;
        framing.compose(0.f, body, &head, eye, focus);
        ensure("opens with the body anchor", focus == body);
        framing.current().rotate(0.75f, 0.2f);
        framing.current().zoom(0.8f);
        framing.current().pan(13, -9, 288, 1.f);
        LLVector3 body_eye, body_focus;
        framing.compose(0.f, body, &head, body_eye, body_focus);
        framing.selectFace(true);
        framing.compose(0.f, body, &head, eye, focus);
        ensure("face anchor", focus == head);
        ensure("close-up in front", (eye - head - LLVector3(0.55f,0,0)).length() < 0.0001f);
        framing.current().rotate(-0.3f, 0.5f);
        framing.current().zoom(0.75f);
        framing.current().pan(-8, 7, 288, 1.f);
        LLVector3 face_eye, face_focus;
        framing.compose(0.f, body, &head, face_eye, face_focus);
        framing.selectFace(false);
        framing.compose(0.f, body, &head, eye, focus);
        ensure("body composition retained", eye == body_eye && focus == body_focus);
        framing.selectFace(true);
        framing.compose(0.f, body, &head, eye, focus);
        ensure("face composition retained", eye == face_eye && focus == face_focus);
        framing.compose(0.f, body, nullptr, eye, focus);
        ensure("missing head falls back to body", eye == body_eye && focus == body_focus);
        const LLVector3 step(0.1f, -0.2f, -0.6f);
        const LLVector3 moved = head + step;
        framing.compose(0.f, body, &moved, eye, focus);
        ensure("tracks animated head", (eye - face_eye - step).length() < 0.0001f &&
               (focus - face_focus - step).length() < 0.0001f);
        framing.reset();
        framing.compose(0.f, body, &head, eye, focus);
        ensure("reset face pan, angle and zoom", focus == head &&
               (eye - head - LLVector3(0.55f,0,0)).length() < 0.0001f);
        framing.selectFace(false);
        framing.compose(0.f, body, &head, eye, focus);
        ensure("reset applies only to selected preset", eye == body_eye && focus == body_focus);
    }

    template<> template<> void orbit_test::test<5>()
    {
        LLSecondViewOrbit camera;
        camera.pan(10, 20, 100, F_PI_BY_TWO);
        const LLVector3 pan = camera.focus(LLVector3::zero, 0.f);
        ensure("grab moves image right and up", (pan - LLVector3(0.f, -0.52f, -1.04f)).length() < 0.0001f);
        ensure("pan follows body turn", (camera.focus(LLVector3::zero, F_PI_BY_TWO)
               - LLVector3(0.52f, 0.f, -1.04f)).length() < 0.0001f);
        LLSecondViewOrbit tilted;
        tilted.rotate(1.3f, 0.7f);
        const LLVector3 before = tilted.offset(0.f);
        tilted.pan(-25, 14, 576, 1.1f);
        ensure("pan preserves orbit and lies in image plane",
               before == tilted.offset(0.f) && std::abs(before * tilted.focus(LLVector3::zero, 0.f)) < 0.0001f);
        LLSecondViewOrbit large;
        large.pan(20, 40, 200, F_PI_BY_TWO);
        ensure("relative drag consistent after resize", (large.focus(LLVector3::zero, 0.f) - pan).length() < 0.0001f);
    }

    template<> template<> void orbit_test::test<6>()
    {
        LLSecondViewLOD lod;
        const LLUUID bangs("00000000-0000-0000-0000-000000000001");
        const LLUUID shop("00000000-0000-0000-0000-000000000002");
        int refreshed = 0;
        auto refresh = [&](const LLUUID& id)
        {
            ensure("refreshes only worn face mesh", id == bangs);
            ++refreshed;
        };
        lod.replace({{bangs, 0.55f}}, refresh);
        ensure("close inset wins even if main is far", lod.distanceFor(bangs, 120.f) == 0.55f);
        ensure("main can request finer mesh", lod.distanceFor(bangs, 0.1f) == 0.1f);
        ensure("works before a valid world distance", lod.distanceFor(bangs, 0.f) == 0.55f);
        ensure("shop keeps main LOD", lod.distanceFor(shop, 120.f) == 120.f);
        lod.replace({{bangs, 2.7f}}, refresh);
        ensure("camera adjustment uses new distance", lod.distanceFor(bangs, 100.f) == 2.7f);
        ensure_equals("inset refresh independent of main cull", refreshed, 2);
        lod.replace({}, [&](const LLUUID& id)
        {
            ensure("release published before rebuilding", lod.distanceFor(id, 120.f) == 120.f);
            ++refreshed;
        });
        lod.replace({}, refresh);
        ensure_equals("release once on leaving face mode", refreshed, 3);
    }

    template<> template<> void orbit_test::test<7>()
    {
        LLSecondViewLOD lod;
        LLSecondViewFraming framing;
        framing.selectFace(true);
        const LLVector3 body(2.f, 3.f, 4.f), head(2.f, 3.f, 4.8f);
        const LLUUID hair("00000000-0000-0000-0000-000000000011");
        const LLUUID eyes("00000000-0000-0000-0000-000000000012");
        LLVector3 eye, focus;
        framing.compose(0.f, body, &head, eye, focus);
        lod.replace({{hair,(eye-head).length()}}, [](const LLUUID&){});
        framing.current().pan(80, -50, 288, 1.f);
        framing.current().zoom(2.f);
        framing.compose(0.f, body, &head, eye, focus);
        const F32 distance = (eye-head).length();
        lod.replace({{eyes,distance}}, [&](const LLUUID& id)
        {
            ensure("detached hair releases, newly worn eyes follow composed eye",
                   lod.distanceFor(id, 90.f) == (id == hair ? 90.f : distance));
        });
        ensure("zoom/pan affects physical distance", distance > 1.1f);
    }
}
