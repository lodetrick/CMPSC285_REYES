/*
 * ECE/CS 285 – HW1 Intermediate Scene: WallE
 * Wall-E (copyright Pixar; 43 rigid parts, posed with anim.h and
 * RiConcatTransform) at four frames of his animation, on a mirroring
 * checkered floor. Correct image: the four differ in head, arm and tread
 * links, and every part is in place; a part floating off means a group got
 * another part's matrix.
 *
 * Output: renders/2_IntermediateScenes/WallE.png
 */

#include "Ri.h"
#include "anim.h"
#include "models.h"
#include "obj.h"
#include "sceneutil.h"

void WallE(void)
{
    float fov = 35.0f;
    RtColor opaque = {1, 1, 1};
    const double eye[3] = {0.9, 1.0, -3.9}, target[3] = {0.02, 0.38, 0};
    RtMatrix camera;
    LookAt(eye, target, camera);

    RiBegin(RI_NULL);
        RiFormat(960, 540);
        RiPixelSamples(4, 4);
        RiDisplay("2_IntermediateScenes/WallE.png", "", "");
        RiFrameBegin(0);
            RiProjection(RI_PERSPECTIVE, "fov", &fov);
            RiConcatTransform(camera);
            RiMakeTexture("models/walle/walle_color.png", 0);
            RiMakeTexture("models/walle/walle_ao.png", 1);
            RiWorldBegin();
                RiOpacity(opaque);
                /* the floor: matte grey checks and polished black ones, which
                 * mirror more than the other scenes' (the robots are small
                 * and dark, and seen from higher up) */
                MaterialParams polished = POLISHED_BLACK;
                polished.reflectance = 0.4f;
                CheckerParams cp = { 2, 2, Material, &MATTE_GREY, Material, &polished };
                RiSurface(CHECKERBOARD, &cp, sizeof cp);
                Floor(32, 0.25); /* 0.25 m tiles over x, z in [-4, 4] */

                ObjMesh *walle = obj_load("models/walle/walle.obj");
                RigidAnim *anim = anim_load("models/walle/walle_anim.txt");
                const int frames[4] = {1, 70, 140, 210};
                /* the four Wall-Es, then their mirror images under the floor */
                for (int mirror = 0; mirror < 2; ++mirror)
                    for (int i = 0; i < 4; ++i) {
                        RiTransformBegin();
                            if (mirror)
                                RiScale(1, -1, 1);
                            RiTranslate((RtFloat)(-1.35 + 0.9 * i), 0, (RtFloat)(0.25 * i));
                            RiRotate(-20, 0, 1, 0);
                            DrawWallE(walle, anim, frames[i], 0, NULL);
                        RiTransformEnd();
                    }
                /* below them, black, past their mirrored heads */
                RiTransformBegin();
                    MirrorBacking(eye, 1.0);
                    Floor(32, 0.25, camera);
                RiTransformEnd();
                anim_free(anim);
                obj_free(walle);
            RiWorldEnd();
        RiFrameEnd();
    RiEnd();
}
