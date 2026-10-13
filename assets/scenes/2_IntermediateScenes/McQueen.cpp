/*
 * ECE/CS 285 – HW1 Intermediate Scene: McQueen
 * Lightning McQueen (copyright Pixar; 19,416 triangles, three textures,
 * semi-transparent glass) on a floor of matte grey and polished black
 * checks that mirror him: the scene that exercises RiPointsPolygons for
 * real. About 160 million micropolygons at full size, so debug at 320 x 240
 * with 1 x 1 samples.
 *
 * Output: renders/2_IntermediateScenes/McQueen.png
 */

#include "Ri.h"
#include "obj.h"
#include "sceneutil.h"

#include <cstring>

/* World to camera, as row vectors (p_cam = [p 1] M), the RiConcatTransform
 * convention: the eye at (3.3, 1.6, -4.3) looking at (0, 0.6, 0), y up. */
static const double EYE[3] = {3.3, 1.6, -4.3};
static RtMatrix CAMERA = {
    {0.7933f, -0.1105f, -0.5987f, 0},
    {0.0000f, 0.9834f, -0.1814f, 0},
    {0.6088f, 0.1439f, 0.7801f, 0},
    {0.0000f, -0.5900f, 5.6207f, 1},
};

static float CameraZ(const RtFloat p[3])
{
    return p[0] * CAMERA[0][2] + p[1] * CAMERA[1][2] + p[2] * CAMERA[2][2] + CAMERA[3][2];
}

/* 1 m tiles over x, z in [-12, 12] at y = 0, as one RiPointsPolygons call.
 * Every tile has its corners in the same order (u along +x, v along +z), so
 * the checks line up from tile to tile. */
static void Floor(void)
{
    enum { N = 24, NV = (N + 1) * (N + 1) };
    static RtPoint P[NV];
    static RtInt nverts[N * N], verts[4 * N * N];
    for (int j = 0; j <= N; ++j)
        for (int i = 0; i <= N; ++i) {
            RtFloat *p = P[j * (N + 1) + i];
            p[0] = (RtFloat)(i - N / 2);
            p[1] = 0;
            p[2] = (RtFloat)(j - N / 2);
        }

    int npolys = 0, nindices = 0;
    for (int j = 0; j < N; ++j)
        for (int i = 0; i < N; ++i) {
            int corner[4] = {j * (N + 1) + i, j * (N + 1) + i + 1, (j + 1) * (N + 1) + i + 1,
                             (j + 1) * (N + 1) + i};
            bool nearEye = false;
            for (int k = 0; k < 4; ++k)
                nearEye = nearEye || CameraZ(P[corner[k]]) < 0.3f;
            if (nearEye)
                continue; /* nothing may cross the eye plane */
            nverts[npolys++] = 4;
            for (int k = 0; k < 4; ++k)
                verts[nindices++] = corner[k];
        }
    RiPointsPolygons(npolys, nverts, verts, P, NULL, NULL);
}

/* The car's four groups: body, eyes and tires textured, the glass
 * semi-transparent. */
static void DrawCar(const ObjMesh *car)
{
    RtColor white = {1, 1, 1}, opaque = {1, 1, 1};
    RtColor glass = {0.6f, 0.8f, 1.0f}, glassOpacity = {0.35f, 0.35f, 0.35f};
    for (int k = 0; car && k < car->ngroups; ++k) {
        const ObjGroup &g = car->groups[k];
        TextureParams tp = { -1 }; /* the slot of the group's texture; TexturedStandard
                                    * (sceneutil.h) puts it in Cs and calls STANDARD */
        if (std::strcmp(g.material, "body") == 0)
            tp.slot = 0;
        else if (std::strcmp(g.material, "eyes") == 0)
            tp.slot = 1;
        else if (std::strcmp(g.material, "tires") == 0)
            tp.slot = 2;

        if (tp.slot >= 0) {
            RiColor(white);
            RiOpacity(opaque);
            RiSurface(TexturedStandard, &tp, sizeof tp);
        } else { /* glass */
            RiColor(glass);
            RiOpacity(glassOpacity);
            RiSurface(NULL, NULL, 0);
        }
        RiPointsPolygons(g.npolys, g.nverts, g.verts, g.P, g.N, g.st);
    }
    RiOpacity(opaque);
}

void McQueen(void)
{
    float fov = 40.0f;
    RtColor opaque = {1, 1, 1};

    RiBegin(RI_NULL);
        RiFormat(960, 720);
        RiPixelSamples(4, 4);
        RiDisplay("2_IntermediateScenes/McQueen.png", "", "");
        RiFrameBegin(0);
            RiProjection(RI_PERSPECTIVE, "fov", &fov);
            RiConcatTransform(CAMERA);
            RiMakeTexture("models/mcqueen/McQueenLivery.jpg", 0);
            RiMakeTexture("models/mcqueen/Eyes_McQueen_Eyes.jpg", 1);
            RiMakeTexture("models/mcqueen/McQ_Tires.png", 2);
            RiWorldBegin();
                RiOpacity(opaque);

                /* the floor: matte grey checks and polished black ones */
                CheckerParams cp = { 2, 2, Material, &MATTE_GREY, Material, &POLISHED_BLACK };
                RiSurface(CHECKERBOARD, &cp, sizeof cp);
                Floor();

                ObjMesh *car = obj_load("models/mcqueen/mcqueen.obj");
                DrawCar(car);

                /* its mirror image under the floor */
                RiTransformBegin();
                    RiScale(1, -1, 1);
                    DrawCar(car);
                RiTransformEnd();

                /* below that, black, past the mirrored roof */
                RiTransformBegin();
                    MirrorBacking(EYE, 1.7);
                    Floor();
                RiTransformEnd();
                obj_free(car);
            RiWorldEnd();
        RiFrameEnd();
    RiEnd();
}
