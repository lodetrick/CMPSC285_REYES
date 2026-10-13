/*
 * Ri.h: the HW1 RenderMan-like C interface.
 *
 * The scene files compile against exactly these declarations, so do not
 * change them. Everything here has C linkage, so the renderer behind it can
 * be written in C, C++ or Rust (ri.rs mirrors this file). Angles are in
 * degrees throughout.
 *
 * Departures from Pixar's ri.h, on purpose:
 *   - no variadic token/value parameter lists (Rust cannot define variadic
 *     functions); RiProjection takes at most one token/value pair
 *   - shaders are C function pointers with a parameter block
 *     (RiSurface, RiDisplacement, RtShaderContext)
 *   - our own texture calls (RiMakeTexture, RiTexture)
 *   - RtToken is const char * and the RI_ tokens are string literals, so
 *     scenes can pass "fov" and nothing has to export global variables
 *   - RiFormat has no pixel aspect ratio and RiPixelSamples takes integers;
 *     there is no RiFrameAspectRatio, RiIdentity or RiTransform (no scene
 *     needs them)
 */
#ifndef RI_H
#define RI_H

#include <stddef.h> /* size_t, NULL */

#ifdef __cplusplus
extern "C" {
#endif

typedef void RtVoid;
typedef float RtFloat;
typedef int RtInt;
typedef const char *RtToken;
typedef void *RtPointer;
typedef RtFloat RtColor[3];
typedef RtFloat RtPoint[3];
typedef RtFloat RtVector[3];
typedef RtFloat RtNormal[3];
typedef RtFloat RtMatrix[4][4];

#define RI_NULL NULL
#define RI_PERSPECTIVE "perspective"
#define RI_ORTHOGRAPHIC "orthographic"

/* ------------------------------------------------------------------ shaders */

/* What a shader sees at one grid vertex. The renderer fills in the inputs;
 * a displacement shader moves P, a surface shader writes Ci and Oi. */
typedef struct RtShaderContext {
    /* inputs, filled by the renderer at each grid vertex */
    RtPoint P;            /* position of grid vertex in camera/eye space */
    RtPoint Po;           /* same point in object space */
    RtNormal N;           /* unit normal, recomputed after displacement */
    RtFloat u, v;         /* surface parameters in [0,1], or a mesh's st */
    RtColor Cs, Os;       /* current RiColor and RiOpacity */
    const void *params;   /* this shader's parameter block, or NULL */
    /* outputs */
    RtColor Ci, Oi;       /* written by surface shaders */
    /* displacement shaders modify P in place */
} RtShaderContext;

typedef void (*RtShaderFn)(RtShaderContext *ctx);

/* ---------------------------------------------------------- graphics state */

RtVoid RiBegin(RtToken name);
RtVoid RiEnd(void);
RtVoid RiFormat(RtInt xres, RtInt yres);
RtVoid RiPixelSamples(RtInt xsamples, RtInt ysamples); /* xsamples x ysamples jittered samples per pixel */
/* RiDisplay(name, "", "") names the picture. RiDisplay(name, "zfile", "z")
 * writes the depth image instead (the format is in the handout), for
 * shadow maps: see include/shadowmap.h. */
RtVoid RiDisplay(const char *name, RtToken type, RtToken mode);
RtVoid RiFrameBegin(RtInt frame);
RtVoid RiFrameEnd(void);
RtVoid RiWorldBegin(void);
RtVoid RiWorldEnd(void);

/* RiProjection(RI_PERSPECTIVE, "fov", &fov) with fov in degrees, or
 * RiProjection(RI_ORTHOGRAPHIC). C++ callers may omit the token and value. */
#ifdef __cplusplus
RtVoid RiProjection(RtToken name, RtToken token = NULL, RtPointer value = NULL);
#else
RtVoid RiProjection(RtToken name, RtToken token, RtPointer value);
#endif

/* ----------------------------------------------------------- transformations */

RtVoid RiTransformBegin(void);
RtVoid RiTransformEnd(void);
RtVoid RiConcatTransform(RtMatrix transform); /* row vectors: p' = p M, translation in the bottom row */
RtVoid RiTranslate(RtFloat dx, RtFloat dy, RtFloat dz);
RtVoid RiRotate(RtFloat angle, RtFloat dx, RtFloat dy, RtFloat dz);
RtVoid RiScale(RtFloat sx, RtFloat sy, RtFloat sz);

/* ---------------------------------------------------------------- primitives */

RtVoid RiSphere(RtFloat radius, RtFloat zmin, RtFloat zmax, RtFloat thetamax);
RtVoid RiCylinder(RtFloat radius, RtFloat zmin, RtFloat zmax, RtFloat thetamax);
RtVoid RiCone(RtFloat height, RtFloat radius, RtFloat thetamax);
RtVoid RiTorus(RtFloat majorradius, RtFloat minorradius,
               RtFloat phimin, RtFloat phimax, RtFloat thetamax);

/* A mesh of npolys planar, convex polygons; polygon i has nverts[i]
 * vertices. verts holds the vertex indices of all the polygons, one after
 * another (sum of nverts[] entries). P, N and st are per-vertex arrays
 * indexed by those indices (st has two floats per vertex, t = 0 at the top
 * row of a texture). N and st may be NULL. Not variadic, unlike the RiSpec
 * call: the three arrays are the only parameters. */
RtVoid RiPointsPolygons(RtInt npolys, const RtInt nverts[], const RtInt verts[],
                        const RtPoint P[], const RtNormal N[], const RtFloat st[]);

/* ------------------------------------------------------------------ shading */

RtVoid RiColor(RtColor color);
RtVoid RiOpacity(RtColor opacity);

/* Bind a shader to the primitives that follow. nbytes of params are copied
 * now, so the caller's struct may go out of scope. RiSurface(NULL, NULL, 0)
 * binds STANDARD; RiDisplacement(NULL, NULL, 0) removes displacement. */
RtVoid RiSurface(RtShaderFn fn, const void *params, size_t nbytes);
RtVoid RiDisplacement(RtShaderFn fn, const void *params, size_t nbytes);

/* Load an image file (PNG, JPEG, BMP) into texture slot 0..7. */
RtVoid RiMakeTexture(const char *file, RtInt slot);

/* Bilinear lookup with repeat wrap. s runs left to right and t top to
 * bottom: t = 0 is the top row of the image, as in RenderMan. */
RtVoid RiTexture(RtInt slot, RtFloat s, RtFloat t, RtColor result);

#ifdef __cplusplus
}
#endif

/* The scenes include only Ri.h but use the shaders and their parameter
 * structs, so bring those in too. */
#include "shaders.h"

#endif /* RI_H */
