/*
 * ECE/CS 285 – HW1 Intermediate Scene: Gumbo
 * Two carved elephants on a board, after the wood shot in Apodaca and
 * Gritz's "Advanced RenderMan": Gumbo (their elephant, 128 Bezier patches
 * evaluated into models/gumbo/gumbo.obj) in dark walnut and in oak, on an
 * oak board, every grain the book's oak shader, with shadows from the key
 * light's depth map.
 *
 * Output: renders/2_IntermediateScenes/Gumbo.png
 */

#include "Ri.h"
#include "obj.h"
#include "sceneutil.h"
#include "shadowmap.h"

#include <cmath>
#include <cstring>

namespace {

/* ------------------------------------------------------------ shadows */

/* The key light's shadow map, set between the two passes (NULL during the
 * depth pass, where nothing is shadowed yet). */
ShadowParams KEY_SHADOW;

/* ------------------------------------------------------------- camera */

/* Fitted to the book's picture: a long lens from in front and to the left,
 * a little above the elephants' heads. */
const double EYE[3] = {-1.895, 1.103, -3.837}, TARGET[3] = {-0.352, 0.259, 1.488};
const float FOV = 21.75f;
const int XRES = 960, YRES = 720;
RtMatrix CAMERA;
double CAMERA_TO_WORLD[4][4];

/* ------------------------------------------------------------- lights */

/* The book's lighting rather than STANDARD's: a warm key light high on the
 * right and in front, whose shadows fall to the left and back (STANDARD's
 * key is on the left), a dim cool fill from the left, and ambient. Positions in camera space; the key's
 * shadows come from KEY_SHADOW's depth map. */
const double KEY[3] = {9, 12, -1}, FILL[3] = {-7, 2, -3};
const double KEY_COLOR[3] = {1.0, 0.96, 0.9}, FILL_COLOR[3] = {0.55, 0.62, 0.75};
const double KA = 0.22, KD = 0.85, KS = 0.35, EXPONENT = 24;

void BookLight(RtShaderContext *c)
{
    double lit = 1;
    if (KEY_SHADOW.map) {
        const double (*M)[4] = KEY_SHADOW.cameraToWorld;
        double Pw[3];
        for (int i = 0; i < 3; ++i)
            Pw[i] = M[i][0] * c->P[0] + M[i][1] * c->P[1] + M[i][2] * c->P[2] + M[i][3];
        lit = zfile_visibility(KEY_SHADOW.map, Pw);
    }
    double diffuse[3], specular[3];
    for (int i = 0; i < 3; ++i)
        diffuse[i] = specular[i] = 0;
    const double *pos[2] = {KEY, FILL}, *color[2] = {KEY_COLOR, FILL_COLOR};
    const double gain[2] = {lit, 0.45};
    for (int k = 0; k < 2; ++k) {
        double L[3] = {pos[k][0] - c->P[0], pos[k][1] - c->P[1], pos[k][2] - c->P[2]};
        double l = std::sqrt(L[0] * L[0] + L[1] * L[1] + L[2] * L[2]);
        for (double &x : L)
            x /= l;
        double d, sp;
        PhongTerm(c, L, EXPONENT, d, sp);
        for (int i = 0; i < 3; ++i) {
            diffuse[i] += gain[k] * d * color[k][i];
            specular[i] += gain[k] * sp * color[k][i];
        }
    }
    for (int i = 0; i < 3; ++i) {
        c->Ci[i] = (RtFloat)(c->Cs[i] * (KA + KD * diffuse[i]) + KS * specular[i]);
        c->Oi[i] = c->Os[i];
    }
}

/* --------------------------------------------------------------- wood */

/* The pieces, for sceneutil.h's Wood and Plank shaders: walnut and oak
 * carvings whose pith sits several units off, so the rings cross them as
 * stripes, and the board, flat-sawn: its log's pith runs under the front
 * half, a unit below the top, 3 degrees off the board's length, so the top
 * shows nested arches off to the front. (Chosen from a dozen cuts,
 * 2026-10-07.) The shadow map is filled in between the two passes. */
const float PIXEL = 2 * std::tan(FOV / 2 * 3.14159265f / 180) / YRES;
WoodParams WALNUT = {{0.66f, 0.44f, 0.26f}, {0.27f, 0.17f, 0.09f}, 2.6f, 5.0f, 1.5f, 0, 0.45f, 0.05f, 0.06f, 0.008f, PIXEL, &KEY_SHADOW, BookLight};
WoodParams OAK = {{0.87f, 0.61f, 0.38f}, {0.55f, 0.35f, 0.20f}, 3.6f, 3.9f, 1.3f, 0, 0.5f, 0.05f, 0.07f, 0.008f, PIXEL, &KEY_SHADOW, BookLight};
WoodParams BOARD = {{0.90f, 0.63f, 0.405f}, {0.69f, 0.427f, 0.2405f}, 4.2f, 3.78f, 4.2f, 3.0f, 0.3f, 0.05f, 0.025f, 0.008f, PIXEL, &KEY_SHADOW, BookLight};

/* The tusks: ivory, lit like everything else. */
void Ivory(RtShaderContext *c)
{
    c->Cs[0] = 0.86f; c->Cs[1] = 0.85f; c->Cs[2] = 0.78f;
    BookLight(c);
}

/* --------------------------------------------------------------- scene */

/* A box from (x0, y0, z0) to (x1, y1, z1), six quads. */
void Box(float x0, float y0, float z0, float x1, float y1, float z1)
{
    RtPoint P[8] = {{x0, y0, z0}, {x1, y0, z0}, {x1, y1, z0}, {x0, y1, z0},
                    {x0, y0, z1}, {x1, y0, z1}, {x1, y1, z1}, {x0, y1, z1}};
    RtInt nverts[6] = {4, 4, 4, 4, 4, 4};
    RtInt verts[24] = {0, 1, 2, 3, 5, 4, 7, 6, 4, 0, 3, 7, 1, 5, 6, 2, 3, 2, 6, 7, 4, 5, 1, 0};
    RiPointsPolygons(6, nverts, verts, P, NULL, NULL);
}

void Gumbo(const ObjMesh *gumbo, const WoodParams *wood)
{
    for (int k = 0; gumbo && k < gumbo->ngroups; ++k) {
        const ObjGroup &g = gumbo->groups[k];
        if (std::strcmp(g.name, "tusks") == 0)
            RiSurface(Ivory, NULL, 0);
        else
            RiSurface(Wood, wood, sizeof *wood);
        RiPointsPolygons(g.npolys, g.nverts, g.verts, g.P, g.N, g.st);
    }
}

/* The still life, as in the book's picture (the numbers are fitted to it):
 * a slab of oak seen from its front right corner, its long side across
 * the bottom of the frame and its right end, which shows the end grain,
 * coming toward the camera; the walnut elephant near its front, turned to
 * face front left, and the oak one just behind and to the right, facing
 * the camera. */
const float BOARD_X = -0.112f, BOARD_Z = 1.766f, BOARD_YAW = 42.586f;
const float BOARD_L = 2.675f, BOARD_W = 3.988f, BOARD_T = 0.15f; /* along its grain, across, thick */

void Objects(const ObjMesh *gumbo)
{
    RtColor opaque = {1, 1, 1}, grey = {0.86f, 0.86f, 0.86f};
    RiOpacity(opaque);

    /* the board, its top at y = 0; Plank runs the grain along its x */
    RiSurface(Plank, &BOARD, sizeof BOARD);
    RiTransformBegin();
        RiTranslate(BOARD_X, 0, BOARD_Z);
        RiRotate(BOARD_YAW, 0, 1, 0);
        Box(-BOARD_L / 2, -BOARD_T, -BOARD_W / 2, BOARD_L / 2, 0.0f, BOARD_W / 2);
    RiTransformEnd();

    /* first elephant */
    RiTransformBegin();
        RiTranslate(-1.194f, 0, 0.551f);
        RiRotate(50, 0, 1, 0);
        Gumbo(gumbo, &WALNUT);
    RiTransformEnd();

    /* second elephant */
    RiTransformBegin();
        RiTranslate(0.367f, 0, 1.151f);
        RiRotate(15, 0, 1, 0);
        Gumbo(gumbo, &OAK);
    RiTransformEnd();

    /* the grey of the book's page behind everything: an unlit floor the
     * board rests on, and walls far off where the floor ends */
    RiColor(grey);
    RiSurface(CONSTANT, NULL, 0);
    {
        const float L = 200, H = 200, Y = -BOARD_T;
        RtPoint P[8] = {{-L, Y, -L}, {L, Y, -L}, {L, Y, L}, {-L, Y, L}, {-L, H, -L}, {L, H, -L}, {L, H, L}, {-L, H, L}};
        RtInt nverts[5] = {4, 4, 4, 4, 4};
        RtInt verts[20] = {0, 1, 2, 3, 0, 1, 5, 4, 1, 2, 6, 5, 2, 3, 7, 6, 3, 0, 4, 7};
        RiPointsPolygons(5, nverts, verts, P, NULL, NULL);
    }
}

} // namespace

void Gumbo(void)
{
    float lightFov = 30.0f, fov = FOV;
    LookAt(EYE, TARGET, CAMERA, CAMERA_TO_WORLD);

    /* the key light's position in the world, and a camera there */
    double key[3];
    for (int i = 0; i < 3; ++i)
        key[i] = CAMERA_TO_WORLD[i][3] + KEY[0] * CAMERA_TO_WORLD[i][0] + KEY[1] * CAMERA_TO_WORLD[i][1] +
                 KEY[2] * CAMERA_TO_WORLD[i][2];
    const double target[3] = {BOARD_X, 0.3, BOARD_Z};
    RtMatrix lightCamera;
    LookAt(key, target, lightCamera);
    std::memcpy(KEY_SHADOW.cameraToWorld, CAMERA_TO_WORLD, sizeof CAMERA_TO_WORLD);
    KEY_SHADOW.light = 0;
    KEY_SHADOW.map = NULL;

    ObjMesh *gumbo = obj_load("assets/models/gumbo/gumbo.obj");

    RiBegin(RI_NULL);
        /* pass 1: depth seen from the key light */
        RiFrameBegin(0);
            RiFormat(1024, 1024);
            RiPixelSamples(1, 1);
            RiDisplay("2_IntermediateScenes/Gumbo_key.z", "zfile", "z");
            RiProjection(RI_PERSPECTIVE, "fov", &lightFov);
            RiConcatTransform(lightCamera);
            RiWorldBegin();
                Objects(gumbo);
            RiWorldEnd();
        RiFrameEnd();

        /* pass 2: the picture */
        ZFile *map = zfile_load("2_IntermediateScenes/Gumbo_key.z", lightCamera, lightFov);
        KEY_SHADOW.map = map;
        RiFrameBegin(1);
            RiFormat(XRES, YRES);
            RiPixelSamples(4, 4);
            RiDisplay("2_IntermediateScenes/Gumbo.png", "", "");
            RiProjection(RI_PERSPECTIVE, "fov", &fov);
            RiConcatTransform(CAMERA);
            RiWorldBegin();
                Objects(gumbo);
            RiWorldEnd();
        RiFrameEnd();
        zfile_free(map);
    RiEnd();
    obj_free(gumbo);
}
