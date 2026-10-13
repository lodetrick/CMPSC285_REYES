/*
 * ECE/CS 285 – HW1 Intermediate Scene: Buzz
 * Buzz Lightyear (copyright Pixar; 8,148 triangles) on a floor of matte
 * grey and polished black checks that mirror him. His helmet is a
 * transparent RiSphere.
 *
 * Output: renders/2_IntermediateScenes/Buzz.png
 */

#include "Ri.h"
#include "models.h"
#include "obj.h"
#include "sceneutil.h"

#include <cmath>
#include <cstring>

namespace {

const double EYE[3] = {1.9, 1.35, -3.4}, TARGET[3] = {0, 0.95, 0};
RtMatrix CAMERA;

/* Body and face textured, the helmet and the wrist lenses glass
 * (models.h draws the helmet as a sphere). */
void DrawBuzz(const ObjMesh *buzz)
{
    RtColor white = {1, 1, 1}, opaque = {1, 1, 1};
    RtColor glass = {0.85f, 0.92f, 1.0f}, glassOpacity = {0.2f, 0.2f, 0.2f};
    TextureParams tp = { 0 };
    for (int k = 0; buzz && k < buzz->ngroups; ++k) {
        const ObjGroup &g = buzz->groups[k];
        if (std::strcmp(g.material, "dome") == 0) {
            RiColor(glass);
            RiOpacity(glassOpacity);
            RiSurface(NULL, NULL, 0);
            BuzzLenses(g);
            BuzzHelmet();
        } else { /* body and face */
            RiColor(white);
            RiOpacity(opaque);
            RiSurface(TexturedStandard, &tp, sizeof tp);
            RiPointsPolygons(g.npolys, g.nverts, g.verts, g.P, g.N, g.st);
        }
    }
    RiOpacity(opaque);
}

} // namespace

void Buzz(void)
{
    float fov = 40.0f;
    RtColor opaque = {1, 1, 1};

    LookAt(EYE, TARGET, CAMERA);

    RiBegin(RI_NULL);
        RiFormat(960, 720);
        RiPixelSamples(4, 4);
        RiDisplay("2_IntermediateScenes/Buzz.png", "", "");
        RiFrameBegin(0);
            RiProjection(RI_PERSPECTIVE, "fov", &fov);
            RiConcatTransform(CAMERA);
            RiMakeTexture("models/buzz/Buzz_Diffuse.png", 0);
            RiWorldBegin();
                RiOpacity(opaque);

                /* the floor: matte grey checks and polished black ones */
                CheckerParams cp = { 2, 2, Material, &MATTE_GREY, Material, &POLISHED_BLACK };
                RiSurface(CHECKERBOARD, &cp, sizeof cp);
                Floor(24, 0.5, CAMERA); /* 0.5 m tiles over x, z in [-6, 6] */

                ObjMesh *buzz = obj_load("models/buzz/buzz.obj");
                DrawBuzz(buzz);

                /* his mirror image under the floor */
                RiTransformBegin();
                    RiScale(1, -1, 1);
                    DrawBuzz(buzz);
                RiTransformEnd();

                /* below that, black, past his mirrored head */
                RiTransformBegin();
                    MirrorBacking(EYE, 1.93);
                    Floor(24, 0.5, CAMERA);
                RiTransformEnd();
                obj_free(buzz);
            RiWorldEnd();
        RiFrameEnd();
    RiEnd();
}
