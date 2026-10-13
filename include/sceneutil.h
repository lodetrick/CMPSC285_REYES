/*
 * sceneutil.h: helpers the scene files share. A camera built from an eye
 * point and a target (and its inverse, which shadowmap.h needs), a test for
 * leaving out polygons that would cross the eye plane, a tiled floor, two
 * texture shaders (lit and unlit), materials lit by STANDARD's lights with
 * coefficients of their own, two blending functions, an octave sum of
 * the renderer's perlinNoise, and the oak wood of Advanced RenderMan
 * as a pattern (AndysRoom's floor) and as two shaders, Wood and Plank
 * (Gumbo's elephants and board). Like obj.h, the
 * renderer never calls this file; it is C++ because the scene files always
 * are. The shaders here are ordinary functions, like the ones a scene file
 * writes for itself: read the inputs in the RtShaderContext, write Ci and
 * Oi, and call STANDARD for the lighting.
 */
#ifndef SCENEUTIL_H
#define SCENEUTIL_H

#include "Ri.h"
#include "shadowmap.h"

#include <algorithm>
#include <cmath>
#include <vector>

/* a + (b - a) t */
inline double Mix(double a, double b, double t) { return a + (b - a) * t; }

/* 0 below e0, 1 above e1, and a smooth S-curve between (smoothstep) */
inline double Smooth(double e0, double e1, double x)
{
    double t = std::max(0.0, std::min(1.0, (x - e0) / (e1 - e0)));
    return t * t * (3 - 2 * t);
}

/* The renderer's perlinNoise (shaders.h) summed over octaves ("fractional
 * Brownian motion"): each octave about twice as fine (2.03, so the
 * lattices never line up) and half as strong. About [-1, 1]. */
inline double Fbm(double x, double y, double z, int octaves)
{
    double sum = 0, amp = 0.5;
    for (int i = 0; i < octaves; ++i, amp *= 0.5, x *= 2.03, y *= 2.03, z *= 2.03)
        sum += amp * perlinNoise(x, y, z);
    return sum;
}

/* ------------------------------------------------------------ oak */

/* smoothstep(e0, e1, x) - smoothstep(e2, e3, x), repeated with period p */
inline double SmoothPulseTrain(double e0, double e1, double e2, double e3, double p, double x)
{
    double m = x - p * std::floor(x / p);
    return Smooth(e0, e1, m) - Smooth(e2, e3, m);
}

/* Larry Gritz's oak from Advanced RenderMan (Apodaca and Gritz, chapter
 * 12, "oaktexture"), with its default parameters. Growth rings are
 * cylinders around the z axis of P (the log's axis), warped by noise, with
 * a trunk that wobbles as it goes up and rings of uneven width; fine pores
 * (the grain) are noise stretched along the log. Returns 0 for light wood
 * and 1 for dark. fw is the size of a pixel in P's units: detail finer
 * than that fades out instead of aliasing, as the original does with
 * filterwidth(). */
inline double OakTexture(double Px, double Py, double Pz, double fw, double grainy = 1)
{
    const double ringfreq = 8, ringunevenness = 0.5, grainfreq = 25;
    const double ringnoise = 0.02, ringnoisefreq = 1;
    const double trunkwobble = 0.15, trunkwobblefreq = 0.025;
    const double angularwobble = 0.1, angularwobblefreq = 1.5;
    const double ringy = 1;

    /* general warping of the domain: vector fBm, 2 octaves, lacunarity 4 */
    double off[3] = {0, 0, 0}, amp = 1, f = ringnoisefreq;
    for (int o = 0; o < 2; ++o, amp *= 0.5, f *= 4) {
        off[0] += amp * perlinNoise(Px * f, Py * f, Pz * f);
        off[1] += amp * perlinNoise(Px * f + 31.7, Py * f + 11.3, Pz * f + 7.1);
        off[2] += amp * perlinNoise(Px * f + 5.9, Py * f + 23.1, Pz * f + 41.3);
    }
    double rx = Px + ringnoise * off[0], ry = Py + ringnoise * off[1], rz = Pz + ringnoise * off[2];

    /* the trunk isn't steady in x and y as it goes up in z */
    rx += trunkwobble * perlinNoise(Pz * trunkwobblefreq, 0.37, 0.71);
    ry += trunkwobble * perlinNoise(Pz * trunkwobblefreq, 4.13, 9.29);

    /* distance from the center, in rings, with noise around the trunk and
     * rings of unequal width */
    double r = std::sqrt(rx * rx + ry * ry) * ringfreq;
    r += angularwobble * Smooth(0, 5, r) *
         perlinNoise(angularwobblefreq * rx, angularwobblefreq * ry, 0.1 * angularwobblefreq * rz);
    r += ringunevenness * perlinNoise(r, 0.31, 0.53);
    double inring = SmoothPulseTrain(0.1, 0.55, 0.7, 0.95, 1, r);
    inring = Mix(inring, 0.5, Smooth(0.2, 0.6, fw * ringfreq)); /* too fine to see: the average */

    /* the grain: two octaves of pores, stretched along the log */
    double gx = Px * grainfreq, gy = Py * grainfreq, gz = Pz * grainfreq * 0.05, dg = fw * grainfreq;
    double grain = 0;
    for (int i = 0; i < 2; ++i, gx *= 2, gy *= 2, gz *= 2, dg *= 2) {
        double valid = 1 - Smooth(0.2, 0.6, dg);
        if (valid <= 0)
            break;
        double g = valid * perlinNoise(gx, gy, gz);
        g *= 0.3 + 0.7 * inring;
        g = std::pow(std::max(0.0, std::min(1.0, 0.8 - g)), 2);
        g = grainy * Smooth(0.5, 1, g);
        if (i == 0)
            inring *= 1 - 0.4 * valid;
        grain = std::max(grain, g);
    }
    return Mix(inring * ringy, 1, grain);
}

/* One flat cut through a log whose rings circle its z axis. On the cut, a
 * runs across (always perpendicular to the axis) and b runs at angle alpha
 * to the axis: 90 degrees cuts straight across the log and shows the rings
 * as concentric circles (end grain); a few degrees cuts nearly along it, as
 * boards are, and stretches them into long nested arches. The cut's origin
 * sits at (ox, oy, oz) in the log. grainy is how strongly the pores show
 * (1 is Gritz's; a polished carving shows them less). */
inline double OakCut(double a, double b, double alpha, double ox, double oy, double oz, double fw,
                     double grainy = 1)
{
    double sa = std::sin(alpha * 3.14159265358979323846 / 180), ca = std::cos(alpha * 3.14159265358979323846 / 180);
    return OakTexture(ox + a, oy + b * sa, oz + b * ca, fw, grainy);
}

/* The two wood shaders take these: the two colors the oak pattern mixes
 * between (light wood and the dark rings and pores), the grain's scale K
 * (wood units per unit of the object: more is finer grain; the rings are
 * 1/8 wood unit apart), where the log's pith runs relative to the piece
 * (ox, oz, in wood units: far away, and the rings cross the piece as
 * nearly straight stripes; near, as arches), for a plank the angle in
 * degrees between the log's axis and its length (tilt: a few degrees
 * gives a flat-sawn board's nested arches, the "cathedral" figure; Wood
 * ignores it), how strongly the pores show
 * (grain, 1 for Gritz's oak, less for a polished carving), how far the
 * rings meander (wobble, in wood units: 0 for Gritz's, 0.05 to 0.15 for
 * a visibly wavy grain), the fibers (fiber: how strongly, 0 for none;
 * fibersize: how wide, in the object's units, about 3 pixels for them to
 * show), how fine a pixel is in world units at distance 1
 * (2 tan(fov / 2) / yres, so the pattern can fade out detail finer than a
 * pixel instead of aliasing), the key light's shadow map, or NULL for
 * none, and how to light the wood: NULL for STANDARD (through the shadow
 * map), or a scene's own lighting shader, which gets the wood's color in
 * Cs. */
typedef struct {
    RtFloat light[3], dark[3];
    RtFloat K;
    RtFloat ox, oz;
    RtFloat tilt;
    RtFloat grain;
    RtFloat wobble;
    RtFloat fiber, fibersize;
    RtFloat pixel;
    const ShadowParams *shadow;
    RtShaderFn lit;
} WoodParams;

/* Light the wood's color, as WoodParams says. */
inline void LightWood(RtShaderContext *c, const WoodParams *w)
{
    if (w->lit)
        w->lit(c);
    else
        ShadowedStandard(c, w->shadow);
}

/* A pixel's size in wood units at the shaded point. */
inline double WoodFilterWidth(const RtShaderContext *c, const WoodParams *w)
{
    double dist = std::sqrt(c->P[0] * c->P[0] + c->P[1] * c->P[1] + c->P[2] * c->P[2]);
    return w->K * dist * w->pixel;
}

/* The wood at (x, y) across the log and z along it, in wood units: 0 light,
 * 1 dark. Gritz's oak, with the rings warped by a slow noise (wobble), and
 * fibers: fine noise stretched along the log, which breaks up the rings'
 * exact curves the way real wood does. Fibers finer than a pixel fade out,
 * like the pattern's own detail. */
inline double WoodAt(double x, double y, double z, const WoodParams *w, double fw)
{
    if (w->wobble > 0) {
        double dx = perlinNoise(0.5 * x + 3.1, 0.5 * y + 7.7, 0.1 * z);
        double dy = perlinNoise(0.5 * x + 11.3, 0.5 * y + 1.9, 0.1 * z + 5.3);
        x += w->wobble * dx;
        y += w->wobble * dy;
    }
    double wood = OakTexture(x, y, z, fw, w->grain);
    if (w->fiber > 0 && w->fibersize > 0) {
        double f = 1 / (w->fibersize * w->K); /* fibers per wood unit, across the log */
        double fade = 1 - Smooth(0.2, 0.6, fw * f);
        if (fade > 0) {
            double n = perlinNoise(x * f, y * f, z * f * 0.07) +
                       0.5 * perlinNoise(2 * x * f + 9.1, 2 * y * f + 4.3, 2 * z * f * 0.07);
            wood = std::max(0.0, std::min(1.0, wood + w->fiber * fade * n));
        }
    }
    return wood;
}

/* A carving: the log's axis runs along the piece's own y, offset to the
 * pith, so a flank shows the rings as stripes up and down and a cut
 * across it shows arches. */
inline void Wood(RtShaderContext *c)
{
    const WoodParams *w = (const WoodParams *)c->params;
    double wood = WoodAt(c->Po[0] * w->K + w->ox, c->Po[2] * w->K + w->oz, c->Po[1] * w->K, w,
                         WoodFilterWidth(c, w));
    for (int i = 0; i < 3; ++i)
        c->Cs[i] = (RtFloat)Mix(w->light[i], w->dark[i], wood);
    LightWood(c, w);
}

/* A plank: its length runs along the piece's own x, tilt degrees off the
 * log's axis, with the pith at (ox, oz) wood units across (z) and below
 * (y) the piece's origin. With the pith below the board and a few degrees
 * of tilt, the top cuts through ring after ring along its length: nested
 * arches (flat-sawn); the ends show the rings as arcs (end grain). */
inline void Plank(RtShaderContext *c)
{
    const WoodParams *w = (const WoodParams *)c->params;
    const double tilt = w->tilt * 3.14159265358979323846 / 180;
    double along = c->Po[0] * w->K;
    double wood = WoodAt(c->Po[2] * w->K + w->ox, c->Po[1] * w->K + w->oz + along * std::sin(tilt),
                         along * std::cos(tilt), w, WoodFilterWidth(c, w));
    for (int i = 0; i < 3; ++i)
        c->Cs[i] = (RtFloat)Mix(w->light[i], w->dark[i], wood);
    LightWood(c, w);
}

/* World to camera for an eye looking at a target, y up, as row vectors
 * (p_cam = [p 1] M), the RiConcatTransform convention. Left-handed: x is
 * up x z. If cameraToWorld is given, it gets the inverse, row-major for
 * column vectors (P_world = M (P_camera, 1)), the form ShadowParams takes:
 * the camera's axes as columns, then the eye. */
inline void LookAt(const double eye[3], const double target[3], RtMatrix M,
                   double cameraToWorld[4][4] = NULL)
{
    double z[3] = {target[0] - eye[0], target[1] - eye[1], target[2] - eye[2]};
    double lz = std::sqrt(z[0] * z[0] + z[1] * z[1] + z[2] * z[2]);
    for (double &a : z)
        a /= lz;
    double lx = std::sqrt(z[2] * z[2] + z[0] * z[0]);
    double x[3] = {z[2] / lx, 0, -z[0] / lx}; /* (0, 1, 0) x z */
    double y[3] = {z[1] * x[2] - z[2] * x[1], z[2] * x[0] - z[0] * x[2], z[0] * x[1] - z[1] * x[0]};
    for (int r = 0; r < 3; ++r) {
        M[r][0] = (RtFloat)x[r];
        M[r][1] = (RtFloat)y[r];
        M[r][2] = (RtFloat)z[r];
        M[r][3] = 0;
    }
    M[3][0] = (RtFloat)-(eye[0] * x[0] + eye[1] * x[1] + eye[2] * x[2]);
    M[3][1] = (RtFloat)-(eye[0] * y[0] + eye[1] * y[1] + eye[2] * y[2]);
    M[3][2] = (RtFloat)-(eye[0] * z[0] + eye[1] * z[1] + eye[2] * z[2]);
    M[3][3] = 1;
    if (cameraToWorld) {
        for (int r = 0; r < 3; ++r) {
            for (int c = 0; c < 3; ++c)
                cameraToWorld[r][c] = M[r][c];
            cameraToWorld[r][3] = eye[r];
            cameraToWorld[3][r] = 0;
        }
        cameraToWorld[3][3] = 1;
    }
}

/* A point through a matrix in the RenderMan convention (row vectors,
 * translation in the bottom row), e.g. a world point into camera space. */
inline void TransformPoint(const RtMatrix M, const double p[3], double out[3])
{
    for (int i = 0; i < 3; ++i)
        out[i] = p[0] * M[0][i] + p[1] * M[1][i] + p[2] * M[2][i] + M[3][i];
}

/* The camera-space depth of a world point, for leaving out polygons that
 * would cross the eye plane (the renderer doesn't split them). */
inline double CameraZ(const RtMatrix worldToCamera, const RtFloat p[3])
{
    return p[0] * worldToCamera[0][2] + p[1] * worldToCamera[1][2] + p[2] * worldToCamera[2][2] +
           worldToCamera[3][2];
}

/* A floor of n x n square tiles, each `size` across, centered on the origin
 * at y = 0, as one RiPointsPolygons call. With a camera, tiles that come
 * within 0.3 of its eye plane are left out. */
inline void Floor(int n, double size, const RtMatrix worldToCamera = NULL)
{
    std::vector<RtFloat> coords(3 * (size_t)(n + 1) * (n + 1));
    const RtPoint *P = (const RtPoint *)coords.data();
    std::vector<RtInt> nverts, verts;
    for (int j = 0; j <= n; ++j)
        for (int i = 0; i <= n; ++i) {
            RtFloat *p = &coords[3 * ((size_t)j * (n + 1) + i)];
            p[0] = (RtFloat)(size * (i - n / 2));
            p[1] = 0;
            p[2] = (RtFloat)(size * (j - n / 2));
        }
    for (int j = 0; j < n; ++j)
        for (int i = 0; i < n; ++i) {
            int corner[4] = {j * (n + 1) + i, j * (n + 1) + i + 1, (j + 1) * (n + 1) + i + 1,
                             (j + 1) * (n + 1) + i};
            bool nearEye = false;
            for (int k = 0; k < 4 && worldToCamera; ++k)
                nearEye = nearEye || CameraZ(worldToCamera, P[corner[k]]) < 0.3;
            if (nearEye)
                continue;
            nverts.push_back(4);
            verts.insert(verts.end(), corner, corner + 4);
        }
    RiPointsPolygons((RtInt)nverts.size(), nverts.data(), verts.data(), P, NULL, NULL);
}

/* The two texture shaders take these parameters:
 *     TextureParams tp = {slot};            RiSurface(TexturedStandard, &tp, sizeof tp);
 *     TextureParams tp = {slot, &shadow};   ... the same, with a shadow map
 * RiSurface copies the struct but not the ShadowParams it points to, so a
 * scene can set that shadow map's depth image later. */
typedef struct {
    RtInt slot;                 /* the texture, loaded by RiMakeTexture; -1 for none */
    const ShadowParams *shadow; /* NULL for no shadow map (shadowmap.h) */
} TextureParams;

/* The texture replaces the surface color, then STANDARD lights it (through
 * the shadow map, if any). With no texture (slot -1) this is STANDARD on
 * the current color, with the shadow. */
inline void TexturedStandard(RtShaderContext *c)
{
    const TextureParams *p = (const TextureParams *)c->params;
    if (p->slot >= 0)
        RiTexture(p->slot, c->u, c->v, c->Cs);
    ShadowedStandard(c, p->shadow);
}

/* The texture is the color itself, unlit: for a sky, or a test pattern that
 * must come out exactly as it is in the file. */
inline void UnlitTexture(RtShaderContext *c)
{
    const TextureParams *p = (const TextureParams *)c->params;
    RiTexture(p->slot, c->u, c->v, c->Ci);
    c->Oi[0] = c->Os[0];
    c->Oi[1] = c->Os[1];
    c->Oi[2] = c->Os[2];
}

/* What STANDARD's three lights deliver at the shaded point: the sums
 * diffuse = sum max(N.L, 0) I C and specular = sum max(R.V, 0)^exponent I C.
 * STANDARD is Ci = Cs (0.15 + 0.8 diffuse) + 0.3 specular with exponent 32;
 * a material with another BRDF combines them with other coefficients. */
inline void StandardLighting(const RtShaderContext *c, double exponent, double diffuse[3], double specular[3])
{
    for (int i = 0; i < 3; ++i)
        diffuse[i] = specular[i] = 0;
    for (int k = 0; k < 3; ++k) {
        double d, s;
        StandardLightTerm(c, k, exponent, d, s);
        for (int i = 0; i < 3; ++i) {
            double I = STANDARD_LIGHT_INTENSITIES[k] * STANDARD_LIGHT_COLORS[k][i];
            diffuse[i] += d * I;
            specular[i] += s * I;
        }
    }
}

/* How much of the light a smooth surface mirrors toward the eye: f0
 * face-on, rising to 1 at grazing angles (Schlick's approximation). */
inline double Fresnel(const RtShaderContext *c, double f0)
{
    double N[3] = {c->N[0], c->N[1], c->N[2]}, V[3] = {-c->P[0], -c->P[1], -c->P[2]};
    double cosine = std::fabs(N[0] * V[0] + N[1] * V[1] + N[2] * V[2]) /
                    std::sqrt((N[0] * N[0] + N[1] * N[1] + N[2] * N[2]) * (V[0] * V[0] + V[1] * V[1] + V[2] * V[2]));
    return f0 + (1 - f0) * std::pow(1 - cosine, 5.0);
}

/* A surface of its own: a color, lit by STANDARD's lights with its own
 * coefficients (ka 0.15, kd 0.8, ks 0, any exponent is a matte chalk; a
 * large ks and exponent, a polished stone), and if reflectance is above 0,
 * a mirror finish. reflectance is the share of light the surface mirrors
 * face-on (Schlick's F0); it mirrors more at grazing angles. The surface
 * comes out transparent by the mirrored share, so that a mirror image of
 * the scene, which the scene draws beneath it, shows through as the
 * reflection. The highlights are the lights' own reflections, so the
 * over operator must not dim them: they are divided by the opacity here. */
typedef struct {
    RtFloat color[3];
    RtFloat ka, kd, ks, exponent;
    RtFloat reflectance;
} MaterialParams;

inline void Material(RtShaderContext *c)
{
    const MaterialParams *m = (const MaterialParams *)c->params;
    double diffuse[3], specular[3];
    StandardLighting(c, m->exponent, diffuse, specular);
    double opacity = m->reflectance > 0 ? 1 - Fresnel(c, m->reflectance) : 1;
    for (int i = 0; i < 3; ++i) {
        c->Cs[i] = m->color[i];
        c->Ci[i] = (RtFloat)(m->color[i] * (m->ka + m->kd * diffuse[i]) + m->ks * specular[i] / opacity);
        c->Oi[i] = (RtFloat)(c->Os[i] * opacity);
    }
}

/* The floor the model scenes (McQueen, Buzz, WallE) stand on: CHECKERBOARD
 * alternating these two materials,
 *     CheckerParams cp = { 2, 2, Material, &MATTE_GREY, Material, &POLISHED_BLACK };
 * with the model drawn a second time, mirrored under the floor
 * (RiScale(1, -1, 1)), for the polished checks to show as a subtle
 * reflection, and below that black (MirrorBacking). */
static const MaterialParams MATTE_GREY = {{0.65f, 0.65f, 0.65f}, 0.15f, 0.8f, 0, 1, 0};
static const MaterialParams POLISHED_BLACK = {{0.02f, 0.02f, 0.02f}, 0.15f, 0.8f, 0.8f, 150, 0.2f};

/* Unlit black: the empty background (which renders black), as a mirroring
 * floor shows it. */
inline void Black(RtShaderContext *c)
{
    c->Ci[0] = c->Ci[1] = c->Ci[2] = 0;
    c->Oi[0] = c->Oi[1] = c->Oi[2] = 1;
}

/* For the black under a mirroring floor, which the floor shows where it
 * mirrors no model: binds Black and scales about the eye, so that the floor
 * drawn next (at y = 0, then RiTransformEnd) lies about depth below y = 0.
 * Scaled about the eye, it fills just what can be seen through the floor
 * (a hair less) and nothing past the floor's edge. depth must be more than
 * the mirrored model reaches. */
inline void MirrorBacking(const double eye[3], double depth)
{
    double s = 0.995 * (eye[1] + depth) / eye[1];
    RiSurface(Black, NULL, 0);
    RiTranslate((RtFloat)eye[0], (RtFloat)eye[1], (RtFloat)eye[2]);
    RiScale((RtFloat)s, (RtFloat)s, (RtFloat)s);
    RiTranslate((RtFloat)-eye[0], (RtFloat)-eye[1], (RtFloat)-eye[2]);
}

#endif /* SCENEUTIL_H */
