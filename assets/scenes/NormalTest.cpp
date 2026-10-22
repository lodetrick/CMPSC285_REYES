/*
 * ECE/CS 285 – HW1 Test Scene: NormalTest
 * Three quadrics drawn four times, to check the shading normal one step
 * at a time: as transformed to camera space, as turned toward the eye,
 * which side of the surface it ended up on, and lit by STANDARD. Compare
 * each picture with the reference render of the same name.
 *
 * Output: renders/1_TestScenes/NormalTest_original.png, _flipped.png,
 *         _sides.png and _shaded.png
 *
 * To add it to your kit: copy this file into scenes/, add it to
 * SCENE_SOURCES in CMakeLists.txt (the Rust build finds it by itself),
 * and in scenes/0_scene_table.cpp declare `void NormalTest(void);` and
 * add {"NormalTest", NormalTest, "test"} to RI_SCENES. Then run
 * `reyes NormalTest`. It needs steps 14 and 15 of the Suggested Workflow
 * and nothing later: quadrics, the transform stack, RiSurface with a
 * parameter block, the normal in the shader context, and STANDARD. No
 * textures, no displacement, no transparency.
 *
 * The three objects, left to right, each catch one mistake:
 *   - an open tube, looked into from above its near end, so that its
 *     floor shows through the opening: the normals there must be turned
 *     around before STANDARD lights it;
 *   - a squashed sphere (RiScale with unequal factors, then rotated): its
 *     normals come out right only through the inverse transpose of the
 *     modelview matrix, not through the matrix itself;
 *   - a whole sphere near the edge of the frame. "Facing the eye" means
 *     N . (-P) >= 0, with -P the direction from the point to the eye, not
 *     N.z <= 0, the direction down the view axis. Away from the middle of
 *     the picture the two disagree: along the edge of this sphere nearest
 *     the middle, and over a wedge of the tube's inside and a strip of
 *     its outside. A renderer that tests N.z turns those normals the
 *     wrong way and lights them from behind.
 *
 * A normal is drawn as the color (N + 1) / 2: x to red, y to green, z to
 * blue. So a normal pointing straight at the camera, (0, 0, -1), is dark
 * yellow (0.5, 0.5, 0); one pointing away is light blue (0.5, 0.5, 1);
 * +x is pink, +y light green, -y purple.
 *   original  N before the renderer turned it toward the eye. A surface
 *             shader is only ever handed the turned normal, so the shader
 *             here undoes the turn: the scene knows which way each
 *             object's outside points (it placed them), and puts N back
 *             on that side. For a quadric that is the analytic normal
 *             through the inverse transpose, so only a wrong transform
 *             changes this picture; the turn cannot. Correct: the colors
 *             change smoothly over every object, and the tube's floor,
 *             seen through the opening, is blue and purple (its normals
 *             point down and away from the eye).
 *   flipped   N exactly as the surface shader receives it. Correct: the
 *             same as original, except that the tube's floor is now light
 *             green and pink (turned up, toward the eye). The left edge
 *             of the right sphere shades toward blue in both pictures,
 *             and that is right: those normals have N.z > 0 yet face the
 *             eye, which is off to their left.
 *   sides     which side of the surface the shader's N is on: green on
 *             the outside, red on the inside, and blue mixed in where it
 *             is on neither, off the surface's normal line (half blue
 *             for 5 degrees off, full blue beyond about 12). Correct:
 *             all green except the tube's floor, which is red; no blue
 *             anywhere.
 *   shaded    the same objects lit by STANDARD. Correct: the tube's floor
 *             is lit, pale green like its outside.
 *
 * What a wrong picture means (each one rendered with that single bug):
 *   - original differs on the squashed sphere only: its colors shift and
 *     a rim of another color runs along its edge; sides shows it cyan
 *     with a magenta rim, and shaded moves its highlight and darkens its
 *     lower left edge. The normals go through the modelview matrix
 *     instead of its inverse transpose. (Colors too strong or too pale on
 *     that sphere alone: they are not renormalized afterwards.) Wrong on
 *     all three objects: they are not transformed at all, or are
 *     transformed as points.
 *   - flipped is the same picture as original, sides is all green, and
 *     shaded shows the tube's floor dark: N is never turned toward the
 *     eye.
 *   - flipped shows the tube's floor split by a sharp line, blue and
 *     purple on the left and light green on the right; sides shows a
 *     green wedge there, a red strip down the tube's right outside wall
 *     and a red line along the left edge of each sphere; shaded has a
 *     dark wedge on the tube's floor. The test is N.z, not N . (-P).
 *   - original, flipped and sides all match, shaded does not: the bug is
 *     inside STANDARD, not in the normals. Check that V = normalize(-P)
 *     at every vertex, L = normalize(light - P), R = 2 (N . L) N - L with
 *     N, L and V unit vectors, and the lights' positions and colors.
 */

#include "Ri.h"

#include <math.h>

/* ----------------------------------------------------- the scene's side */

/* How an object was placed, so that a shader can work out from Po (the
 * object-space point the renderer hands every shader) which way the
 * outside of the surface points in camera space: the outward normal from
 * the quadric's equation, through the inverse transpose of the scene's own
 * transform, which for a rotation is the rotation itself and for a scale
 * divides by the factors. A translation does not move a normal. */
enum Shape { SPHERE, TUBE };

typedef struct {
    int shape;
    RtFloat scale[3];       /* the RiScale applied to it (1, 1, 1 for none) */
    int nrotations;         /* the RiRotates applied to it, in the order */
    RtFloat rotation[2][4]; /* they were called: angle, then the axis */
} Placement;

static const double PI = 3.14159265358979323846;

/* v rotated by angle degrees about the axis (ax, ay, az), the way RiRotate
 * turns the object (Rodrigues' formula). */
static void Rotate(RtFloat angle, RtFloat ax, RtFloat ay, RtFloat az, double v[3])
{
    double len = sqrt(ax * ax + ay * ay + az * az);
    double kx = ax / len, ky = ay / len, kz = az / len;
    double c = cos(angle * PI / 180), s = sin(angle * PI / 180);
    double d = kx * v[0] + ky * v[1] + kz * v[2];                 /* k . v */
    double cx = ky * v[2] - kz * v[1], cy = kz * v[0] - kx * v[2], /* k x v */
           cz = kx * v[1] - ky * v[0];
    double r[3] = {v[0] * c + cx * s + kx * d * (1 - c),
                   v[1] * c + cy * s + ky * d * (1 - c),
                   v[2] * c + cz * s + kz * d * (1 - c)};
    v[0] = r[0];
    v[1] = r[1];
    v[2] = r[2];
}

static void Normalize(double v[3])
{
    double len = sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (len > 0) {
        v[0] /= len;
        v[1] /= len;
        v[2] /= len;
    }
}

/* The outward unit normal, in camera space, at the object-space point Po
 * of an object placed as p says. */
static void Outward(const Placement *p, const RtPoint Po, double n[3])
{
    /* in object space: a sphere's normal is its point, a cylinder's is the
     * point's direction from the axis */
    n[0] = Po[0];
    n[1] = Po[1];
    n[2] = p->shape == TUBE ? 0 : Po[2];
    /* through the inverse transpose: the scale divides, the rotations
     * rotate, in the order the calls act on the object, which is the
     * reverse of the order they were made (the call nearest the primitive
     * acts first) */
    for (int i = 0; i < 3; ++i)
        n[i] /= p->scale[i];
    for (int r = p->nrotations - 1; r >= 0; --r)
        Rotate(p->rotation[r][0], p->rotation[r][1], p->rotation[r][2], p->rotation[r][3], n);
    Normalize(n);
}

/* ------------------------------------------------------------- shaders */

/* (n + 1) / 2 as a color, and the surface's own opacity. */
static void NormalColor(const double n[3], RtShaderContext *c)
{
    for (int i = 0; i < 3; ++i) {
        c->Ci[i] = (RtFloat)(0.5 * (n[i] + 1));
        c->Oi[i] = c->Os[i];
    }
}

/* flipped: the normal exactly as the renderer handed it over. */
void ShowFlipped(RtShaderContext *c)
{
    double n[3] = {c->N[0], c->N[1], c->N[2]};
    NormalColor(n, c);
}

/* original: the renderer's normal, put back on the outside of the surface
 * if the renderer turned it away from there. */
void ShowOriginal(RtShaderContext *c)
{
    double n[3] = {c->N[0], c->N[1], c->N[2]}, out[3];
    Outward((const Placement *)c->params, c->Po, out);
    if (n[0] * out[0] + n[1] * out[1] + n[2] * out[2] < 0)
        for (int i = 0; i < 3; ++i)
            n[i] = -n[i];
    NormalColor(n, c);
}

/* sides: green where the renderer's N is the outward normal, red where it
 * is the inward one, and blue mixed in for the angle it makes with that
 * line (sin of the angle, times 5, so a few degrees show). */
void ShowSides(RtShaderContext *c)
{
    double n[3] = {c->N[0], c->N[1], c->N[2]}, out[3];
    Normalize(n);
    Outward((const Placement *)c->params, c->Po, out);
    double d = n[0] * out[0] + n[1] * out[1] + n[2] * out[2];
    double off = sqrt(fmax(0.0, 1 - d * d));
    c->Ci[0] = (RtFloat)fmax(-d, 0.0);
    c->Ci[1] = (RtFloat)fmax(d, 0.0);
    c->Ci[2] = (RtFloat)fmin(1.0, 5 * off);
    for (int i = 0; i < 3; ++i)
        c->Oi[i] = c->Os[i];
}

/* --------------------------------------------------------------- scene */

/* The same three objects, drawn with the given surface shader (NULL for
 * STANDARD). Each one's Placement must list exactly the RiRotate and
 * RiScale calls made for it, in order. */
static void Objects(RtShaderFn shader)
{
    static const Placement tube = {TUBE, {1, 1, 1}, 2, {{-25, 1, 0, 0}, {160, 0, 1, 0}}};
    static const Placement squashed = {SPHERE, {1.1f, 0.5f, 1.1f}, 2, {{-25, 1, 0, 0}, {40, 0, 0, 1}}};
    static const Placement sphere = {SPHERE, {1, 1, 1}, 0, {{0, 0, 0, 1}, {0, 0, 0, 1}}};
    RtColor green = {0.6f, 0.85f, 0.55f}, orange = {0.95f, 0.6f, 0.25f}, blue = {0.55f, 0.7f, 0.95f};

    /* an open tube, its near end toward the camera, right and down, so
     * that we look along it and see its floor */
    RiTransformBegin();
        RiColor(green);
        if (shader) RiSurface(shader, &tube, sizeof tube); else RiSurface(NULL, NULL, 0);
        RiTranslate(-2.6f, 0.0f, 6.0f);
        RiRotate(-25, 1, 0, 0);
        RiRotate(160, 0, 1, 0);
        RiCylinder(0.9f, -1.4f, 1.4f, 360);
    RiTransformEnd();

    /* a sphere squashed to half its height, then tilted */
    RiTransformBegin();
        RiColor(orange);
        if (shader) RiSurface(shader, &squashed, sizeof squashed); else RiSurface(NULL, NULL, 0);
        RiTranslate(0.3f, 0.2f, 6.5f);
        RiRotate(-25, 1, 0, 0);
        RiRotate(40, 0, 0, 1);
        RiScale(1.1f, 0.5f, 1.1f);
        RiSphere(1, -1, 1, 360);
    RiTransformEnd();

    /* a whole sphere, well off the view axis */
    RiTransformBegin();
        RiColor(blue);
        if (shader) RiSurface(shader, &sphere, sizeof sphere); else RiSurface(NULL, NULL, 0);
        RiTranslate(2.7f, -0.4f, 6.0f);
        RiSphere(1.2f, -1.2f, 1.2f, 360);
    RiTransformEnd();
}

void NormalTest(void)
{
    static const char *const names[4] = {"1_TestScenes/NormalTest_original.png",
                                         "1_TestScenes/NormalTest_flipped.png",
                                         "1_TestScenes/NormalTest_sides.png",
                                         "1_TestScenes/NormalTest_shaded.png"};
    static const RtShaderFn shaders[4] = {ShowOriginal, ShowFlipped, ShowSides, NULL};
    float fov = 60.0f;
    RtColor opaque = {1, 1, 1};

    RiBegin(RI_NULL);
        RiFormat(640, 480);
        RiPixelSamples(4, 4);
        for (int pass = 0; pass < 4; ++pass) {
            RiFrameBegin(pass);
                RiDisplay(names[pass], "", "");
                RiProjection(RI_PERSPECTIVE, "fov", &fov);
                RiWorldBegin();
                    RiOpacity(opaque);
                    Objects(shaders[pass]);
                RiWorldEnd();
            RiFrameEnd();
        }
    RiEnd();
}
