/*
 * ECE/CS 285 – HW1 Custom Scene
 *
 * Output: renders/Castle.png
 */

#include "Ri.h"
#include "models.h"
#include "obj.h"
#include "sceneutil.h"
#include "shadowmap.h"

#include <cmath>
#include <cstring>

namespace {

const double EYE[3] = {1.9, 1.35, -3.4}, TARGET[3] = {0, 0.95, 0};
RtMatrix CAMERA;

static void ShadowLit(RtShaderContext *c)
{
    ShadowedStandard(c, (const ShadowParams *)c->params);
}

/* Body and face textured, the helmet and the wrist lenses glass
 * (models.h draws the helmet as a sphere). */
void DrawCastle(const ObjMesh *castle)
{
    RiTransformBegin();
        RiScale(0.6, 0.6, 0.6);
        RiRotate(-160, 0, 1, 0);
        RiTranslate(5, -1.3, 0);
        RtColor white = {1, 1, 1}, opaque = {1, 1, 1};
        RtColor glass = {0.85f, 0.92f, 1.0f}, glassOpacity = {0.2f, 0.2f, 0.2f};
        TextureParams tp = { 0 };
        for (int k = 0; castle && k < castle->ngroups; ++k) {
            const ObjGroup &g = castle->groups[k];
            RiColor(white);
            RiOpacity(opaque);
            RiSurface(TexturedStandard, &tp, sizeof tp);
            RiPointsPolygons(g.npolys, g.nverts, g.verts, g.P, g.N, g.st);
        }

        RiSurface(NULL, NULL, 0);
        RiTranslate(0, 0.4, 0);
        RiColor(glass);
        RiOpacity(glassOpacity);
        RiSphere(1.3, -1.3, 1.3, 360);
        RiOpacity(opaque);
    RiTransformEnd();
}

/* A large blue cylinder surrounding the scene.
 * Its axis is vertical, and its radius is large enough to fill the view.
 */
void DrawSky()
{
    RtColor sky = {0.24f, 0.52f, 0.88f};
    RtColor opaque = {1, 1, 1};

    RiTransformBegin();
        RiSurface(CONSTANT, NULL, 0);
        RiColor(sky);
        RiOpacity(opaque);

        /* Cylinder's native axis is z; rotate it to the y axis. */
        RiRotate(-90, 1, 0, 0);
        RiTranslate(0, 0, -120);
        RiCylinder(180, 0, 300, 360);
    RiTransformEnd();
}

/* A wide, flat, brown terrain surface.
 * Use a flattened sphere to avoid a visible circular cylinder edge.
 */
void DrawGround(const ShadowParams *sp)
{
    RtColor earth = {0.34f, 0.19f, 0.085f};
    RtColor opaque = {1, 1, 1};

    RiTransformBegin();
        // RiSurface(GROUND_SURFACE, NULL, 0);
        RiSurface(ShadowLit, sp, sizeof *sp);
        BumpParams bp1 = { 1.4f, 3, 5 };
        RiDisplacement(BUMPY, &bp1, sizeof bp1);
        RiColor(earth);
        RiOpacity(opaque);

        RiTranslate(0, -1.15, 15);
        RiScale(40, 0.7, 40);
        RiSphere(1, -1, 1, 120);
    RiTransformEnd();
}

void DrawSun() {
    RiTransformBegin();
        RiSurface(NULL, NULL, NULL);
        RtColor yellow = {0.95,0.95,0.3};
        RiColor(yellow);
        RiTranslate(-35,20,100);
        RiSphere(4, 4, -4, 360);
    RiTransformEnd();
}

void DrawMeteor() {
    RiTransformBegin();
        RiTranslate(-35, 8, 40);
        RiScale(1.4,1.4,1.4);
        TextureParams tp = {1};
        RiSurface(TexturedStandard, &tp, sizeof tp);
        RiSphere(1, 1, -1, 360);
        BumpParams bp1 = { 0.3f, 2, 6 };
        RiDisplacement(BUMPY, &bp1, sizeof bp1);
        RiSurface(NULL, NULL, NULL);
        RtColor opacity = {.4,.4,.4};
        RiOpacity(opacity);

        RtColor red = {1, 0, 0};
        RiColor(red);
        RiSphere(1.3, 1.3, -1.3, 360);
        RiTransformBegin();
            RiRotate(190, 1, 0, 0);
            RiCone(8, 1.3, 360);
        RiTransformEnd();
    RiTransformEnd();
}


/*
 * Procedurally generate a layered mountain range.
 * The peaks are generated from a mathematical function,
 * so no individual mountain placement is necessary.
 */
void DrawMountains()
{
    RtColor opaque = {1, 1, 1};

    const int NUM_PEAKS = 25;
    const double START_X = -90.0;
    const double END_X = 90.0;
    const double STEP = (END_X - START_X) / (NUM_PEAKS - 1);

    // Mountain layers: heights, depth, and color.
    const double baseY = -3.0;
    const double heights[] = {8.0, 12.0, 16.0};
    const double depths[] = {35.0, 55.0, 75.0};

    RtColor colors[] = {
        {0.12f, 0.22f, 0.34f},  // Near: dark blue
        {0.20f, 0.32f, 0.48f},  // Middle
        {0.30f, 0.43f, 0.61f}   // Far: light blue
    };

    RiTransformBegin();
        RiSurface(NULL, NULL, 0);
        BumpParams bp1 = { 0.3f, -1, 3 };
        RiDisplacement(BUMPY, &bp1, sizeof bp1);
        RiOpacity(opaque);

        for (int layer = 0; layer < 3; ++layer) {
            RiColor(colors[layer]);

            for (int i = 0; i < NUM_PEAKS; ++i) {
                double x = START_X + i * STEP;

                // Combine sine waves to create irregular peak heights.
                double variation =
                    0.55 * (0.5 + 0.5 * sin(i * 1.73 + layer)) +
                    0.45 * (0.5 + 0.5 * sin(i * 0.67 + 2.0 * layer));

                double height = heights[layer] * (0.45 + variation);

                // Wider peaks in the far distance, narrower in front.
                double radius = STEP * (0.85 + 0.25 *
                    sin(i * 2.31 + layer)) * height * 0.12;

                RiTransformBegin();
                    RiTranslate(x, baseY, depths[layer]);
                    RiRotate(-90, 1, 0, 0);
                    RiCone(height, radius, 360);
                RiTransformEnd();
            }
        }

    RiTransformEnd();
}


} // namespace

void CustomScene(void)
{
    float light_fov = 100.0f, fov = 40.0f;
    RtColor opaque = {1, 1, 1};

    const double key[3] = {-35, 8, 40}; //, target[3] = {0.5, -0.5, 10};
    RtMatrix lightCamera;
    LookAt(key, EYE, lightCamera);

    LookAt(EYE, TARGET, CAMERA);

    ShadowParams sp;
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c)
            sp.cameraToWorld[r][c] = r == c ? 1 : 0;
    sp.light = 0;
    sp.map = NULL;

    RiBegin(RI_NULL);
        ObjMesh *castle = obj_load("assets/models/castle/castle.obj");
        RiFormat(960, 720);
        RiPixelSamples(4, 4);
        RiFrameBegin(0);
            RiDisplay("castle.z", "zfile", "z");
            RiProjection(RI_PERSPECTIVE, "fov", &light_fov);
            RiConcatTransform(CAMERA);
            RiWorldBegin();
                DrawSky();
                DrawGround(&sp);
                DrawMountains();
                DrawSun();
                DrawMeteor();
                RiDisplacement(NULL, NULL, NULL);
                DrawCastle(castle);
            RiWorldEnd();
        RiFrameEnd();

        RiDisplay("Castle.png", "", "");
        RiFrameBegin(0);
            RiProjection(RI_PERSPECTIVE, "fov", &fov);
            RiConcatTransform(CAMERA);
            RiMakeTexture("assets/models/castle/castle_tex.jpg", 0);
            RiMakeTexture("assets/textures/moon.png", 1);
            RiWorldBegin();
                /* Background and landscape */
                DrawSky();
                DrawGround(&sp);
                DrawMountains();
                DrawSun();
                DrawMeteor();
                RiDisplacement(NULL, NULL, NULL);
                DrawCastle(castle);
            RiWorldEnd();
        RiFrameEnd();
        obj_free(castle);
    RiEnd();
}
