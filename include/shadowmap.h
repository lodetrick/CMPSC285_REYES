/*
 * shadowmap.h: shadow maps for scene files, built on the renderer's depth
 * output. Like obj.h, the renderer never calls this file; it is C++
 * because the scene files always are.
 *
 * A scene renders twice. First from a light's position, writing depth:
 *     RiDisplay("key.z", "zfile", "z");   ... the scene ...   RiWorldEnd();
 * RiWorldEnd writes renders/key.z, so the scene can load it right away
 * with zfile_load("key.z", lightCamera, lightFov), handing back the camera
 * matrix and projection it rendered that pass with. Then it renders the
 * real picture with shaders that call ShadowedStandard() instead of
 * STANDARD: the same lighting, with the chosen light dimmed where the
 * depth image says something stands between it and the point being
 * shaded.
 *
 * The depth file (written by the renderer, little-endian):
 *   char  magic[4] = "Rz01"
 *   int32 width, height
 *   float depth[height][width]  top row first: per pixel, the camera-space z
 *                               of the nearest surface at least half opaque,
 *                               the smallest over the pixel's samples;
 *                               +infinity where there is none
 */
#ifndef SHADOWMAP_H
#define SHADOWMAP_H

#include "Ri.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

typedef struct {
    int width, height;
    double worldToCamera[4][4];  /* the light's camera: P_camera = M (P_world, 1) */
    double cameraToRaster[4][4]; /* (x, y, z, w) = M (P_camera, 1); raster = (x / w, y / w) */
    float *depth;                /* width * height, top row first */
} ZFile;

/* The raster mapping the handout defines, as a homogeneous matrix: the
 * screen window, [-a, a] x [-1, 1] for a landscape or square map
 * (a = width / height >= 1) and [-1, 1] x [-1/a, 1/a] for a portrait one,
 * stretched over the image, y down. Perspective with fov (degrees) over
 * the shorter side, or orthographic when fov is 0. */
inline void zfile_raster_matrix(ZFile *z, float fov)
{
    double a = (double)z->width / z->height;
    double halfw = a >= 1 ? a : 1, halfh = a >= 1 ? 1 : 1 / a; /* the window's half extents */
    double sx = z->width / (2 * halfw), sy = z->height / (2 * halfh); /* pixels per screen unit */
    double (*R)[4] = z->cameraToRaster;
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c)
            R[r][c] = 0;
    if (fov > 0) {
        double focal = 1 / std::tan(fov / 2 * 3.14159265358979 / 180);
        R[0][0] = focal * sx, R[0][2] = halfw * sx;  /* x = (focal px / pz + halfw) sx */
        R[1][1] = -focal * sy, R[1][2] = halfh * sy; /* y = (halfh - focal py / pz) sy */
        R[3][2] = 1;                                 /* w = z */
    } else {
        R[0][0] = sx, R[0][3] = halfw * sx;          /* x = (px + halfw) sx */
        R[1][1] = -sy, R[1][3] = halfh * sy;         /* y = (halfh - py) sy */
        R[3][3] = 1;
    }
}

/* Load a depth file, looking for it as given, then in renders/ (where
 * RiDisplay puts it), together with the camera the depth pass used: the
 * RtMatrix the scene gave RiConcatTransform (row vectors), times scale if
 * it also scaled the camera (RadiatorSprings fits an orthographic window
 * that way), and the projection's fov in degrees, or 0 for orthographic.
 * NULL plus a message on stderr on failure. */
inline ZFile *zfile_load(const char *path, const RtMatrix worldToCamera, float fov, float scale = 1)
{
    std::FILE *f = std::fopen(path, "rb");
    if (!f)
        f = std::fopen((std::string("renders/") + path).c_str(), "rb");
    if (!f) {
        std::fprintf(stderr, "zfile_load: cannot open %s or renders/%s\n", path, path);
        return NULL;
    }
    char magic[4];
    int size[2];
    ZFile *z = new ZFile;
    z->depth = NULL;
    bool ok = std::fread(magic, 1, 4, f) == 4 && std::memcmp(magic, "Rz01", 4) == 0 &&
              std::fread(size, sizeof size[0], 2, f) == 2 && size[0] > 0 && size[1] > 0;
    if (ok) {
        z->width = size[0];
        z->height = size[1];
        z->depth = new float[(size_t)size[0] * size[1]];
        ok = std::fread(z->depth, sizeof(float), (size_t)size[0] * size[1], f) == (size_t)size[0] * size[1];
    }
    std::fclose(f);
    if (!ok) {
        std::fprintf(stderr, "zfile_load: %s is not a depth file\n", path);
        delete[] z->depth;
        delete z;
        return NULL;
    }
    /* RtMatrix is for row vectors (p' = p M); transpose it for column vectors */
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c)
            z->worldToCamera[r][c] = (r < 3 ? scale : 1) * worldToCamera[c][r];
    zfile_raster_matrix(z, fov);
    return z;
}

inline void zfile_free(ZFile *z)
{
    if (z) {
        delete[] z->depth;
        delete z;
    }
}

/* How much of a light at the depth image's eye reaches a world point: 1 in
 * full light, 0 in shadow. Points outside the image, or behind its eye,
 * count as lit.
 *
 * A point is lit where its depth from the light is no greater than the
 * stored depth plus a bias. The bias (1.5 texels at that distance, plus,
 * for a perspective map, a little in proportion to it) keeps a surface from
 * shadowing itself where its own depth and the stored one differ by
 * rounding and by the texel's extent ("shadow acne"). An orthographic map,
 * for a distant light like the sun, sits far from what it sees, so a share
 * of its depth would be far too much. The test is made at 16 points spread over 3 x 3
 * texels and averaged ("percentage closer filtering"), which turns the
 * stair steps of the texels into a soft edge. */
inline float zfile_visibility(const ZFile *z, const double P[3])
{
    const double (*M)[4] = z->worldToCamera, (*R)[4] = z->cameraToRaster;
    double pc[3];
    for (int i = 0; i < 3; ++i)
        pc[i] = M[i][0] * P[0] + M[i][1] * P[1] + M[i][2] * P[2] + M[i][3];
    if (pc[2] <= 0.01)
        return 1;
    double h[4];
    for (int i = 0; i < 4; ++i)
        h[i] = R[i][0] * pc[0] + R[i][1] * pc[1] + R[i][2] * pc[2] + R[i][3];
    double x = h[0] / h[3], y = h[1] / h[3];

    /* one texel's size in the light's camera space at this depth: raster x
     * grows by R[0][0] / w per unit of camera x (w = 1 in an orthographic
     * map, w = z in a perspective one) */
    double texel = std::fabs(h[3] / R[0][0]);
    bool perspective = R[3][2] != 0;
    double bias = 1.5 * texel + (perspective ? 0.002 * pc[2] : 0.0);

    int lit = 0;
    for (int j = 0; j < 4; ++j)
        for (int i = 0; i < 4; ++i) {
            int col = (int)std::floor(x + 0.75 * (i - 1.5)), row = (int)std::floor(y + 0.75 * (j - 1.5));
            if (col < 0 || row < 0 || col >= z->width || row >= z->height ||
                pc[2] - bias <= z->depth[(size_t)row * z->width + col])
                ++lit;
        }
    return lit / 16.0f;
}

/* STANDARD with one light's share dimmed by a shadow map. The shaded point
 * c->P is in the camera space of the picture being rendered, so the
 * parameters say how to take it to world space. light is 0 for the key,
 * 1 the fill, 2 the rim. With map NULL this is STANDARD. */
typedef struct {
    const ZFile *map;
    double cameraToWorld[4][4]; /* row-major, of the picture being rendered */
    int light;
} ShadowParams;

/* STANDARD's lights, fixed in camera space: 0 the key, 1 the fill, 2 the
 * rim. Their positions, colors and intensities, from the handout. */
static const double STANDARD_LIGHTS[3][3] = {{-6, 8, -4}, {7, 3, -2}, {0, 5, 18}};
static const double STANDARD_LIGHT_COLORS[3][3] = {{1.0, 0.95, 0.85}, {0.6, 0.7, 1.0}, {1, 1, 1}};
static const double STANDARD_LIGHT_INTENSITIES[3] = {1.0, 0.4, 0.5};

/* Where one of those lights is in the world, for a picture whose camera
 * space goes to the world by cameraToWorld (sceneutil.h's LookAt makes
 * both). A depth pass for that light's shadows looks from here. */
inline void StandardLightPosition(const double cameraToWorld[4][4], int light, double out[3])
{
    const double *L = STANDARD_LIGHTS[light];
    for (int i = 0; i < 3; ++i)
        out[i] = cameraToWorld[i][3] + L[0] * cameraToWorld[i][0] + L[1] * cameraToWorld[i][1] +
                 L[2] * cameraToWorld[i][2];
}

/* The two factors of Phong lighting at the shaded point for one light that
 * shines from the unit camera-space direction L: diffuse = max(N.L, 0) and
 * specular = max(R.V, 0)^exponent, R being L mirrored about N and V the
 * direction to the eye. The light's color and intensity, and the surface's
 * coefficients, are the caller's to apply. STANDARD's lights are point
 * lights (L = normalize(position - P), see StandardLightTerm); a distant
 * light like a sun has the same L everywhere. */
inline void PhongTerm(const RtShaderContext *c, const double L[3], double exponent,
                      double &diffuse, double &specular)
{
    double N[3] = {c->N[0], c->N[1], c->N[2]}, V[3] = {-c->P[0], -c->P[1], -c->P[2]};
    double ln = std::sqrt(N[0] * N[0] + N[1] * N[1] + N[2] * N[2]);
    double lv = std::sqrt(V[0] * V[0] + V[1] * V[1] + V[2] * V[2]);
    for (int i = 0; i < 3; ++i) {
        N[i] /= ln;
        V[i] /= lv;
    }
    double ndotl = N[0] * L[0] + N[1] * L[1] + N[2] * L[2];
    double R[3] = {2 * ndotl * N[0] - L[0], 2 * ndotl * N[1] - L[1], 2 * ndotl * N[2] - L[2]};
    diffuse = std::fmax(ndotl, 0.0);
    specular = std::pow(std::fmax(R[0] * V[0] + R[1] * V[1] + R[2] * V[2], 0.0), exponent);
}

/* PhongTerm for one of STANDARD's lights (0 the key, 1 the fill, 2 the rim). */
inline void StandardLightTerm(const RtShaderContext *c, int light, double exponent,
                              double &diffuse, double &specular)
{
    double L[3] = {STANDARD_LIGHTS[light][0] - c->P[0], STANDARD_LIGHTS[light][1] - c->P[1],
                   STANDARD_LIGHTS[light][2] - c->P[2]};
    double ll = std::sqrt(L[0] * L[0] + L[1] * L[1] + L[2] * L[2]);
    for (double &x : L)
        x /= ll;
    PhongTerm(c, L, exponent, diffuse, specular);
}

inline void ShadowedStandard(RtShaderContext *c, const ShadowParams *s)
{
    STANDARD(c);
    if (!s || !s->map)
        return;
    double Pw[3];
    for (int i = 0; i < 3; ++i)
        Pw[i] = s->cameraToWorld[i][0] * c->P[0] + s->cameraToWorld[i][1] * c->P[1] +
                s->cameraToWorld[i][2] * c->P[2] + s->cameraToWorld[i][3];
    double shadow = 1 - zfile_visibility(s->map, Pw);
    if (shadow <= 0)
        return;

    /* The light's own share of STANDARD, from the handout:
     * Cs kd max(N.L, 0) I C + ks max(R.V, 0)^32 I C, taken away in proportion
     * to how much of the light is blocked. */
    const double KD = 0.8, KS = 0.3, EXPONENT = 32;
    int k = s->light;
    double diffuse, specular;
    StandardLightTerm(c, k, EXPONENT, diffuse, specular);
    for (int i = 0; i < 3; ++i) {
        double I = STANDARD_LIGHT_INTENSITIES[k] * STANDARD_LIGHT_COLORS[k][i];
        c->Ci[i] -= (RtFloat)(shadow * (c->Cs[i] * KD * diffuse * I + KS * specular * I));
    }
}

#endif /* SHADOWMAP_H */
