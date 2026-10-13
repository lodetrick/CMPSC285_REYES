/*
 * shaders.h: the four required shaders, their parameter structs, and the
 * noise function they and the scenes share.
 *
 * Scenes bind these by name, so keep the names and structs exactly as they
 * are. A shader reads its parameters by casting c->params to its struct.
 */
#ifndef SHADERS_H
#define SHADERS_H

#include "Ri.h"

#ifdef __cplusplus
extern "C" {
#endif

/* CHECKERBOARD: sizex checks across u and sizey across v, alternating
 * between two surface shaders, light (the check at u = v = 0) and dark,
 * each run with its own parameter block. The scene picks the two when it
 * binds CHECKERBOARD:
 *     CheckerParams cp = { 2, 2, Material, &MATTE_GREY,
 *                                Material, &POLISHED_BLACK };
 *     RiSurface(CHECKERBOARD, &cp, sizeof cp);
 * A NULL light shader is STANDARD; a NULL dark shader is STANDARD with a
 * black surface color. So CheckerParams cp = { 8, 4 }; gives lit Cs and
 * black checks. RiSurface copies this struct, but not what light_params
 * and dark_params point to: keep those alive until RiWorldEnd. */
typedef struct {
    RtInt sizex, sizey;
    RtShaderFn light;
    const void *light_params;
    RtShaderFn dark;
    const void *dark_params;
} CheckerParams;

/* BUMPY: displace P along N by
 *   h = amplitude * sum(w_e * noise(2^e * Po)) / sum(w_e),
 * summed over e = min_freq_exp .. max_freq_exp, with w_e = 2^(min_freq_exp - e):
 * each octave counts half as much as the one below it, and |h| <= amplitude.
 * noise is Ken Perlin's improved noise (2002), about [-1, 1]. */
typedef struct {
    RtFloat amplitude;
    RtInt min_freq_exp, max_freq_exp;
} BumpParams;

void CONSTANT(RtShaderContext *c);     /* surface: the color, unlit (Ci = Cs, Oi = Os) */
void STANDARD(RtShaderContext *c);     /* surface: ambient + diffuse + Phong */
void CHECKERBOARD(RtShaderContext *c); /* surface, CheckerParams */
void BUMPY(RtShaderContext *c);        /* displacement, BumpParams */

/* Ken Perlin's improved noise (2002) at (x, y, z): smooth, signed, about
 * [-1, 1], zero at integer coordinates, with features about one unit
 * apart. BUMPY sums octaves of it, and the scene files' own shaders use it
 * too (sceneutil.h's Fbm is an octave sum of it). */
double perlinNoise(double x, double y, double z);

#ifdef __cplusplus
}
#endif

#endif /* SHADERS_H */
