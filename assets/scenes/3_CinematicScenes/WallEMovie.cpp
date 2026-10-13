/*
 * ECE/CS 285 – HW1 Cinematic Scene: WallEMovie
 * Wall-E (copyright Pixar) rolls past towers of trash and a sprout in a
 * boot, lowers his arm and turns his head: 72 frames (3 s at 24 fps), each
 * a depth pass for the shadows and then the picture. Make the movie with
 *   ffmpeg -framerate 24 -i renders/3_CinematicScenes/WallEMovie/WallEMovie_%02d.jpg
 *          -c:v libx264 -pix_fmt yuv420p -crf 18
 *          renders/3_CinematicScenes/WallEMovie.mp4
 *
 * Output: renders/3_CinematicScenes/WallEMovie/WallEMovie_00.jpg to
 *         WallEMovie_71.jpg
 */

#include "Ri.h"
#include "anim.h"
#include "models.h"
#include "obj.h"
#include "sceneutil.h"
#include "shadowmap.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>

namespace {

const int CLIP_FIRST = 50, FRAMES = 72; /* movie frame i shows clip frame CLIP_FIRST + i - 1 */
const int XRES = 960, YRES = 540, SAMPLES = 3;

unsigned HashU(int a, int b, int c)
{
    unsigned h = ((unsigned)a * 73856093u) ^ ((unsigned)b * 19349663u) ^ ((unsigned)c * 83492791u);
    h ^= h >> 13;
    h *= 0x5bd1e995u;
    h ^= h >> 15;
    return h;
}

double Hash(int n) { return (HashU(n, 7, 13) & 0xffffff) / 16777216.0; }

/* ------------------------------------------------------------- camera */

const double EYE[3] = {1.25, 0.42, -2.35};
const double TARGET[3] = {-0.15, 0.62, 1.6};
const float FOV = 38.0f;
const double PI = 3.14159265358979;
RtMatrix CAMERA;

/* The key light's shadow map, set between the passes of each frame. Its
 * cameraToWorld (the picture's camera) also lets shaders find the world
 * point they shade. */
ShadowParams KEY_SHADOW;

/* Whether any of a quad can be in the picture: false if all four corners
 * lie beyond one edge of the frame, or one is close to the eye plane (the
 * ground there is below the frame). */
bool InView(const double c[4][3])
{
    const double ty = std::tan(FOV / 2 * PI / 180) * 1.05, tx = ty * XRES / YRES;
    int left = 0, right = 0, below = 0, above = 0;
    for (int k = 0; k < 4; ++k) {
        double q[3];
        TransformPoint(CAMERA, c[k], q);
        if (q[2] < 0.15)
            return false;
        left += q[0] < -tx * q[2];
        right += q[0] > tx * q[2];
        below += q[1] < -ty * q[2];
        above += q[1] > ty * q[2];
    }
    return left < 4 && right < 4 && below < 4 && above < 4;
}

/* ------------------------------------------------------ sky, haze, light */

/* The sun, low and to the left, behind the dust. */
const double SUN[3] = {-0.55, 0.16, 0.82};

void SkyColor(const double d[3], RtColor out)
{
    const double horizon[3] = {0.93, 0.70, 0.42}, zenith[3] = {0.66, 0.52, 0.38};
    double e = std::max(0.0, d[1]);
    double g = std::max(0.0, d[0] * SUN[0] + d[1] * SUN[1] + d[2] * SUN[2]);
    double glow = 0.35 * std::pow(g, 10.0) + 0.45 * std::pow(g, 120.0) + 0.9 * Smooth(0.9993, 0.9996, g);
    for (int i = 0; i < 3; ++i)
        out[i] = (RtFloat)(Mix(horizon[i], zenith[i], Smooth(0.0, 0.5, e)) + glow * (i == 2 ? 0.8 : 1.0));
}

/* The world point being shaded, and the world direction to it from the
 * eye. (Po is no help: most things here are drawn in frames of their own.) */
void WorldPoint(const RtShaderContext *c, double Pw[3])
{
    const double (*M)[4] = KEY_SHADOW.cameraToWorld;
    for (int i = 0; i < 3; ++i)
        Pw[i] = M[i][0] * c->P[0] + M[i][1] * c->P[1] + M[i][2] * c->P[2] + M[i][3];
}

void ViewDirection(const RtShaderContext *c, double d[3])
{
    const double (*M)[4] = KEY_SHADOW.cameraToWorld;
    for (int i = 0; i < 3; ++i)
        d[i] = M[i][0] * c->P[0] + M[i][1] * c->P[1] + M[i][2] * c->P[2];
    double l = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    for (int i = 0; i < 3; ++i)
        d[i] /= l;
}

/* Dust: fade toward the low sky behind the point, more with distance. */
void Haze(RtShaderContext *c, double scale = 60.0)
{
    double dist = std::sqrt(c->P[0] * c->P[0] + c->P[1] * c->P[1] + c->P[2] * c->P[2]);
    double f = 1 - std::exp(-dist / scale);
    double d[3];
    RtColor sky;
    ViewDirection(c, d);
    d[1] = std::max(d[1], 0.0) * 0.4;
    SkyColor(d, sky);
    for (int i = 0; i < 3; ++i)
        c->Ci[i] = (RtFloat)Mix(c->Ci[i], sky[i], f);
}

/* Where Wall-E is in this frame (for the tread marks behind him). */
double CURRENT_Z = 0;

void Lit(RtShaderContext *c) { ShadowedStandard(c, &KEY_SHADOW); }

void Sky(RtShaderContext *c)
{
    double d[3];
    ViewDirection(c, d);
    SkyColor(d, c->Ci);
    c->Oi[0] = c->Oi[1] = c->Oi[2] = 1;
}

/* ------------------------------------------------------------- ground */

void Dust(RtShaderContext *c)
{
    double Pw[3];
    WorldPoint(c, Pw);
    double x = Pw[0], z = Pw[2];
    double n = Fbm(0.35 * x, 0.0, 0.35 * z, 4), fine = Fbm(3.1 * x, 1.7, 3.1 * z, 3);
    /* Wall-E's tread marks, from under him back along his path: two bands
     * 0.18 m wide at x = +-0.27 m, with the links' cleats across them */
    double tracks = 0;
    if (z > CURRENT_Z - 0.25)
        for (int side = -1; side <= 1; side += 2)
            tracks += (1 - Smooth(0.07, 0.09, std::fabs(x - side * 0.27))) * (0.5 + 0.5 * std::sin(z * 160));
    const double light[3] = {0.80, 0.62, 0.40}, dark[3] = {0.55, 0.40, 0.26};
    double t = 0.5 + 0.9 * n + 0.25 * fine;
    for (int i = 0; i < 3; ++i)
        c->Cs[i] = (RtFloat)(Mix(dark[i], light[i], std::max(0.0, std::min(1.0, t))) * (1 - 0.18 * tracks));
    Lit(c);
    Haze(c);
}

/* The ground as rings around the point under the eye, cut into sectors so
 * that every tile comes out about square on the screen: seen from this
 * low, a sector's sides run nearly straight up the picture, and rings that
 * deepen with distance stay a steady height in pixels. (Square tiles of one
 * size would be thin slivers near the horizon, and the dice rate, set by
 * the diagonal of a tile's bound on screen, would spend far more
 * micropolygons than each covers.) Each tile is drawn in a frame turned to
 * its own direction, because the renderer bounds a polygon by its box in
 * object space and a slanted sliver's box would be many times its size.
 * Only tiles that can be in the picture are made. */
void GroundTiles(void)
{
    const double h = EYE[1], focal = YRES / 2 / std::tan(FOV / 2 * PI / 180);
    const double view = std::atan2(TARGET[0] - EYE[0], TARGET[2] - EYE[2]), halfWidth = 0.75;
    RtInt nverts[1] = {4}, verts[4] = {0, 1, 2, 3};
    for (double r0 = 0.7, r1; r0 < 400; r0 = r1) {
        /* rings at least 15% deeper than their radius, and 2.5 pixels deep
         * where that is more; the last one reaches out to 450 m */
        double k = 2.5 * r0 / (focal * h);
        r1 = k >= 0.8 ? 450 : r0 * std::max(1.15, 1 / (1 - k));
        double step = std::max(0.003, std::min(0.03, h * (1 - r0 / r1) / r0));
        int n = (int)std::ceil(2 * halfWidth / step);
        double half = halfWidth / n;
        /* Neighbors are cut into different numbers of sectors, so their
         * straight edges don't meet: each tile reaches a little inside the
         * ring before it and past its sides, leaving no cracks to see
         * through. */
        double inner = r0 * std::cos(0.03) * 0.9995, side = half * 1.002;
        for (int i = 0; i < n; ++i) {
            double a = view - halfWidth + (2 * i + 1) * half, ca = std::cos(a), sa = std::sin(a);
            RtPoint P[4] = {{(RtFloat)(-inner * std::sin(side)), 0, (RtFloat)(inner * std::cos(side))},
                            {(RtFloat)(inner * std::sin(side)), 0, (RtFloat)(inner * std::cos(side))},
                            {(RtFloat)(r1 * std::sin(side)), 0, (RtFloat)(r1 * std::cos(side))},
                            {(RtFloat)(-r1 * std::sin(side)), 0, (RtFloat)(r1 * std::cos(side))}};
            RtMatrix M = {{(RtFloat)ca, 0, (RtFloat)-sa, 0}, {0, 1, 0, 0},
                          {(RtFloat)sa, 0, (RtFloat)ca, 0}, {(RtFloat)EYE[0], 0, (RtFloat)EYE[2], 1}};
            double c[4][3];
            for (int j = 0; j < 4; ++j)
                for (int m = 0; m < 3; ++m)
                    c[j][m] = P[j][0] * M[0][m] + P[j][1] * M[1][m] + P[j][2] * M[2][m] + M[3][m];
            if (!InView(c))
                continue;
            RiTransformBegin();
                RiConcatTransform(M);
                RiPointsPolygons(1, nverts, verts, P, NULL, NULL);
            RiTransformEnd();
        }
    }
}

void Ground(void)
{
    RiSurface(Dust, NULL, 0);
    GroundTiles();
    RiSurface(Sky, NULL, 0);
    double f[3] = {TARGET[0] - EYE[0], 0, TARGET[2] - EYE[2]};
    double l = std::sqrt(f[0] * f[0] + f[2] * f[2]);
    double cx = EYE[0] + 2500 * f[0] / l, cz = EYE[2] + 2500 * f[2] / l, sx = f[2] / l, sz = -f[0] / l;
    RtPoint P[4] = {{(RtFloat)(cx - 3000 * sx), -100, (RtFloat)(cz - 3000 * sz)},
                    {(RtFloat)(cx + 3000 * sx), -100, (RtFloat)(cz + 3000 * sz)},
                    {(RtFloat)(cx + 3000 * sx), 2500, (RtFloat)(cz + 3000 * sz)},
                    {(RtFloat)(cx - 3000 * sx), 2500, (RtFloat)(cz - 3000 * sz)}};
    RtInt nverts[1] = {4}, verts[4] = {0, 1, 2, 3};
    RiPointsPolygons(1, nverts, verts, P, NULL, NULL);
}

/* ------------------------------------------------------- trash cubes */

/* Compacted trash: blotches of rust, grey metal and faded colors pressed
 * together, darker in the creases, a little different on every cube
 * (params: its seed). Po is the unit cube's own space. */
void TrashCube(RtShaderContext *c)
{
    const double *seed = (const double *)c->params;
    double x = c->Po[0] * 4 + *seed, y = c->Po[1] * 4 - *seed, z = c->Po[2] * 4 + 0.5 * *seed;
    double n1 = Fbm(x, y, z, 3), n2 = Fbm(2.7 * x + 5, 2.7 * y, 2.7 * z, 3);
    static const double palette[5][3] = {{0.55, 0.32, 0.18}, {0.50, 0.48, 0.45}, {0.62, 0.52, 0.30},
                                         {0.35, 0.40, 0.42}, {0.58, 0.25, 0.20}};
    int k = (int)std::floor((n1 + 0.6) * 4.0 + 3 * *seed) % 5;
    k = k < 0 ? k + 5 : k;
    double crease = Smooth(-0.05, 0.25, n2);
    for (int i = 0; i < 3; ++i)
        c->Cs[i] = (RtFloat)(palette[k][i] * (0.55 + 0.45 * crease));
    Lit(c);
    Haze(c);
}

/* A cube of side `size` resting at (x, y0, z), turned by angle. */
void Cube(double x, double y0, double z, double size, double angle, double seed)
{
    static const double C[8][3] = {{-0.5, 0, -0.5}, {0.5, 0, -0.5}, {0.5, 1, -0.5}, {-0.5, 1, -0.5},
                                   {-0.5, 0, 0.5},  {0.5, 0, 0.5},  {0.5, 1, 0.5},  {-0.5, 1, 0.5}};
    static const int F[6][4] = {{0, 1, 2, 3}, {5, 4, 7, 6}, {4, 0, 3, 7}, {1, 5, 6, 2}, {3, 2, 6, 7}, {4, 5, 1, 0}};
    RtPoint P[8];
    for (int i = 0; i < 8; ++i)
        for (int j = 0; j < 3; ++j)
            P[i][j] = (RtFloat)C[i][j];
    RtInt nverts[6] = {4, 4, 4, 4, 4, 4}, verts[24];
    for (int f = 0; f < 6; ++f)
        for (int k = 0; k < 4; ++k)
            verts[4 * f + k] = F[f][k];
    RiTransformBegin();
        RiTranslate((RtFloat)x, (RtFloat)y0, (RtFloat)z);
        RiRotate((RtFloat)angle, 0, 1, 0);
        RiScale((RtFloat)size, (RtFloat)size, (RtFloat)size);
        RiSurface(TrashCube, &seed, sizeof seed);
        RiPointsPolygons(6, nverts, verts, P, NULL, NULL);
    RiTransformEnd();
}

/* A wall of compacted trash: rows of Wall-E-sized cubes (0.45 m), each row
 * shifted like brickwork, each cube blotched with two colors of rust, dirt
 * and faded paint, with dark seams between them and its top edge catching
 * the light (t runs up the face). s and t are meters along
 * the face, so one box draws thousands of cubes. Seams and blotches finer
 * than a pixel fade out instead of sparkling. */
void TrashStack(RtShaderContext *c)
{
    const double CELL = 0.45;
    const int *seed = (const int *)c->params;
    double v = c->v / CELL;
    int j = (int)std::floor(v);
    double u = c->u / CELL + Hash(j * 31 + *seed);
    int i = (int)std::floor(u);
    double fu = u - i, fv = v - j;
    static const double palette[7][3] = {{0.42, 0.24, 0.14}, {0.31, 0.23, 0.16}, {0.38, 0.36, 0.32},
                                         {0.50, 0.39, 0.22}, {0.22, 0.19, 0.16}, {0.32, 0.33, 0.33},
                                         {0.47, 0.22, 0.15}};
    unsigned h = HashU(i, j, *seed);
    const double *a = palette[h % 7], *b = palette[(h >> 4) % 7];
    double dist = std::sqrt(c->P[0] * c->P[0] + c->P[1] * c->P[1] + c->P[2] * c->P[2]);
    double pixel = dist * 0.0012 / CELL;              /* a pixel, in cells */
    double detail = 1 - Smooth(0.08, 0.3, pixel);     /* blotches visible at all? */
    double blotch = Smooth(-0.15, 0.15, Fbm(2.5 * u + 0.37 * (h & 63), 2.5 * v, 0.1 * *seed, 3)) * detail + 0.5 * (1 - detail);
    double shade = 0.7 + 0.4 * ((h >> 8) & 255) / 255.0;
    double seamWidth = 0.05, edge = std::min(std::min(fu, 1 - fu), std::min(fv, 1 - fv));
    double fade = 1 - Smooth(0.15, 0.5, pixel);
    double seam = (1 - Smooth(seamWidth - pixel, seamWidth + pixel, edge)) * fade;
    double bevel = 1 + fade * (0.25 * Smooth(0.8, 0.9, fv) - 0.25 * (1 - Smooth(0.08, 0.2, fv)));
    for (int k = 0; k < 3; ++k)
        c->Cs[k] = (RtFloat)(Mix(a[k], b[k], blotch) * shade * bevel * (1 - 0.7 * seam));
    Lit(c);
    Haze(c, 45.0);
}

/* The faces of a box from (x0, y0, z0) to (x1, y1, z1) that can face the
 * camera (its back and top never can from down here), each in tiles about
 * 40 pixels square (no smaller than 3 m), with s and t in meters along the
 * face. One polygon per face would be diced by the face's whole diagonal on
 * screen in both directions; tiles keep each grid's micropolygons close to
 * half a pixel. */
void Box(double x0, double y0, double z0, double x1, double y1, double z1)
{
    double center[3] = {(x0 + x1) / 2 - EYE[0], (y0 + y1) / 2 - EYE[1], (z0 + z1) / 2 - EYE[2]};
    double tile = std::max(3.0, 0.05 * std::sqrt(center[0] * center[0] + center[1] * center[1] + center[2] * center[2]));
    struct Face { double o[3], a[3], b[3]; bool seen; } faces[5] = {
        {{x0, y0, z0}, {x1 - x0, 0, 0}, {0, y1 - y0, 0}, EYE[2] < z0},   /* front, facing -z */
        {{x1, y0, z1}, {x0 - x1, 0, 0}, {0, y1 - y0, 0}, EYE[2] > z1},   /* back */
        {{x0, y0, z1}, {0, 0, z0 - z1}, {0, y1 - y0, 0}, EYE[0] < x0},   /* left, facing -x */
        {{x1, y0, z0}, {0, 0, z1 - z0}, {0, y1 - y0, 0}, EYE[0] > x1},   /* right */
        {{x0, y1, z0}, {x1 - x0, 0, 0}, {0, 0, z1 - z0}, EYE[1] > y1},   /* top */
    };
    for (const Face &f : faces) {
        if (!f.seen)
            continue;
        double la = std::sqrt(f.a[0] * f.a[0] + f.a[1] * f.a[1] + f.a[2] * f.a[2]);
        double lb = std::sqrt(f.b[0] * f.b[0] + f.b[1] * f.b[1] + f.b[2] * f.b[2]);
        int na = std::max(1, (int)std::round(la / tile)), nb = std::max(1, (int)std::round(lb / tile));
        double s0 = f.o[0] + f.o[2], t0 = f.o[1]; /* so neighboring faces don't repeat */
        for (int j = 0; j < nb; ++j)
            for (int i = 0; i < na; ++i) {
                RtPoint P[4];
                RtFloat st[8];
                const double u[4] = {(double)i / na, (double)(i + 1) / na, (double)(i + 1) / na, (double)i / na};
                const double v[4] = {(double)j / nb, (double)j / nb, (double)(j + 1) / nb, (double)(j + 1) / nb};
                for (int k = 0; k < 4; ++k) {
                    for (int c = 0; c < 3; ++c)
                        P[k][c] = (RtFloat)(f.o[c] + u[k] * f.a[c] + v[k] * f.b[c]);
                    st[2 * k] = (RtFloat)(s0 + u[k] * la);
                    st[2 * k + 1] = (RtFloat)(t0 + v[k] * lb);
                }
                RtInt nverts[1] = {4}, verts[4] = {0, 1, 2, 3};
                RiPointsPolygons(1, nverts, verts, P, NULL, st);
            }
    }
}

/* A tower of trash: slabs stacked with setbacks and shifts as it rises. */
void Tower(double x, double z, double width, double height, int seed)
{
    RiSurface(TrashStack, &seed, sizeof seed);
    double y = 0, w = width, d = width * (0.7 + 0.5 * Hash(seed));
    int slabs = 3 + (int)(4 * Hash(seed + 1));
    for (int k = 0; k < slabs; ++k) {
        double h = height / slabs * (0.7 + 0.6 * Hash(seed * 10 + k));
        double ox = (Hash(seed * 10 + k + 3) - 0.5) * 0.3 * w, oz = (Hash(seed * 10 + k + 7) - 0.5) * 0.3 * d;
        Box(x + ox - w / 2, y, z + oz - d / 2, x + ox + w / 2, y + h, z + oz + d / 2);
        y += h;
        w *= 0.72 + 0.2 * Hash(seed * 10 + k + 11);
        d *= 0.72 + 0.2 * Hash(seed * 10 + k + 13);
    }
}

/* Distant skyscrapers: plain boxes, lost in the dust. */
void Building(double x, double z, double w, double d, double h)
{
    RtColor grey = {0.45f, 0.40f, 0.36f};
    RiColor(grey);
    Box(x - w, 0, z - d, x + w, h, z + d);
}

void Distant(RtShaderContext *c)
{
    Lit(c);
    Haze(c);
}

/* Lit and hazed by distance: for props placed with transforms. */
void Prop(RtShaderContext *c)
{
    Lit(c);
    Haze(c);
}

void Skyline(void)
{
    RiSurface(Distant, NULL, 0);
    for (int i = 0; i < 26; ++i) {
        double a = -1.0 + 2.0 * i / 25 + 0.05 * Hash(i), r = 180 + 160 * Hash(i + 40);
        double x = EYE[0] + r * std::sin(a * 0.75), z = EYE[2] + r * std::cos(a * 0.75);
        Building(x, z, 6 + 10 * Hash(i + 3), 6 + 8 * Hash(i + 5), 40 + 110 * Hash(i + 9));
    }
}

/* Towers of trash at many distances, fading into the dust. */
void Towers(void)
{
    struct { double x, z, width, height; int seed; } t[] = {
        {-11, 18, 5, 16, 1},   {7, 22, 6, 24, 2},     {-3, 34, 7, 34, 3},    {18, 38, 8, 30, 4},
        {-22, 40, 8, 42, 5},   {4, 55, 10, 55, 6},    {-34, 62, 10, 48, 7},  {30, 64, 9, 60, 8},
        {-14, 80, 12, 70, 9},  {48, 95, 12, 58, 10},  {-50, 105, 12, 66, 11}, {16, 115, 14, 80, 12},
        {-2, 150, 16, 90, 13}, {70, 140, 14, 70, 14}, {-75, 150, 15, 75, 15},
    };
    for (const auto &w : t)
        Tower(w.x, w.z, w.width, w.height, w.seed);
}

/* ------------------------------------------------------------- props */

/* A can lying on its side at (x, z), turned by angle. */
void Can(double x, double z, double angle)
{
    RiTransformBegin();
        RiTranslate((RtFloat)x, 0.035f, (RtFloat)z);
        RiRotate((RtFloat)angle, 0, 1, 0);
        RiCylinder(0.035f, -0.065f, 0.065f, 360);
    RiTransformEnd();
}

/* Wall-E's freshly made cubes near his path, and the rubbish he has not
 * got to yet, thickest in the foreground: cubes, cans, a tire, a bottle, a
 * hubcap, crumpled paper and a bent pipe. */
void NearProps(void)
{
    Cube(-1.25, 0, 1.9, 0.42, 12, 1.3);
    Cube(-1.15, 0.42, 1.95, 0.42, -6, 2.9);
    Cube(-1.75, 0, 2.3, 0.42, 30, 4.1);
    Cube(1.05, 0, 2.6, 0.42, -18, 5.7);
    Cube(-0.9, 0, 3.6, 0.42, 40, 6.2);
    Cube(1.3, 0, -1.1, 0.28, 35, 7.3); /* bottom right corner, half out of frame */
    Cube(1.6, 0, 0.0, 0.40, -22, 8.1);
    Cube(-1.55, 0, 0.9, 0.34, 12, 9.4);

    RtColor rubber = {0.12f, 0.11f, 0.10f}, tin = {0.62f, 0.60f, 0.55f};
    RtColor glass = {0.22f, 0.45f, 0.30f}, chrome = {0.58f, 0.58f, 0.60f};
    RtColor paper = {0.72f, 0.68f, 0.60f}, rust = {0.45f, 0.28f, 0.18f};
    RiSurface(Prop, NULL, 0);

    /* an old tire, lying on its side */
    RiColor(rubber);
    RiTransformBegin();
        RiTranslate(1.1f, 0.07f, 1.0f);
        RiRotate(-90, 1, 0, 0);
        RiTorus(0.2f, 0.07f, 0, 360, 360);
    RiTransformEnd();

    /* cans, one standing */
    RiColor(tin);
    Can(0.75, 0.2, 70);
    Can(0.2, -0.6, 110);
    Can(1.15, -0.3, 20);
    Can(-1.3, 0.35, 75);
    RiTransformBegin();
        RiTranslate(-0.3f, 0.065f, -0.75f);
        RiRotate(-90, 1, 0, 0);
        RiCylinder(0.035f, -0.065f, 0.065f, 360);
    RiTransformEnd();

    /* a bottle on its side: body, shoulder, neck */
    RiColor(glass);
    RiTransformBegin();
        RiTranslate(0.95f, 0.04f, -0.75f);
        RiRotate(-35, 0, 1, 0);
        RiCylinder(0.04f, -0.11f, 0.11f, 360);
        RiTranslate(0, 0, 0.11f);
        RiCone(0.07f, 0.04f, 360);
        RiTranslate(0, 0, 0.04f);
        RiCylinder(0.014f, 0, 0.06f, 360);
    RiTransformEnd();

    /* a hubcap, face up: a shallow cone with a rim */
    RiColor(chrome);
    RiTransformBegin();
        RiTranslate(0.0f, 0.0f, -1.0f);
        RiRotate(-90, 1, 0, 0);
        RiCone(0.02f, 0.13f, 360);
        RiTranslate(0, 0, 0.02f);
        RiTorus(0.13f, 0.02f, 0, 360, 360);
    RiTransformEnd();

    /* crumpled paper: small spheres under a little BUMPY */
    RiColor(paper);
    BumpParams crumple = {0.012f, 3, 6};
    RiDisplacement(BUMPY, &crumple, sizeof crumple);
    RiTransformBegin();
        RiTranslate(-0.45f, 0.055f, -0.15f);
        RiSphere(0.055f, -0.055f, 0.055f, 360);
    RiTransformEnd();
    RiTransformBegin();
        RiTranslate(1.35f, 0.045f, 0.55f);
        RiSphere(0.045f, -0.045f, 0.045f, 360);
    RiTransformEnd();
    RiDisplacement(NULL, NULL, 0);

    /* a bent pipe, lying flat */
    RiColor(rust);
    RiTransformBegin();
        RiTranslate(-1.1f, 0.03f, -0.2f);
        RiRotate(-90, 1, 0, 0);
        RiRotate(20, 0, 0, 1);
        RiTorus(0.25f, 0.03f, 0, 360, 120);
    RiTransformEnd();
}

void Leaf(RtShaderContext *c)
{
    c->Cs[0] = 0.25f, c->Cs[1] = 0.62f, c->Cs[2] = 0.18f;
    Lit(c);
}

/* The sprout: two small leaves on a thin stem, growing in an old boot. */
void BootPlant(double x, double z)
{
    RtColor leather = {0.30f, 0.18f, 0.10f}, stem = {0.30f, 0.50f, 0.18f};
    RiTransformBegin();
        RiTranslate((RtFloat)x, 0, (RtFloat)z);
        RiRotate(35, 0, 1, 0);
        RiSurface(Prop, NULL, 0);
        RiColor(leather);
        RiTransformBegin(); /* the shaft */
            RiRotate(-90, 1, 0, 0);
            RiCylinder(0.06f, 0, 0.2f, 360);
        RiTransformEnd();
        RiTransformBegin(); /* the foot */
            RiTranslate(0, 0.04f, -0.07f);
            RiScale(0.065f, 0.045f, 0.13f);
            RiSphere(1, -1, 1, 360);
        RiTransformEnd();
        RiColor(stem);
        RiTransformBegin();
            RiTranslate(0, 0.15f, 0);
            RiRotate(-90, 1, 0, 0);
            RiCylinder(0.004f, 0, 0.12f, 360);
        RiTransformEnd();
        RiSurface(Leaf, NULL, 0);
        for (int s = -1; s <= 1; s += 2) {
            RiTransformBegin();
                RiTranslate((RtFloat)(0.028 * s), 0.27f, 0);
                RiRotate((RtFloat)(25 * s), 0, 0, 1);
                RiScale(0.03f, 0.008f, 0.018f);
                RiSphere(1, -1, 1, 360);
            RiTransformEnd();
        }
    RiTransformEnd();
}

/* ---------------------------------------------------------- Wall-E */

/* Where Wall-E is at a frame: rolling along -z at his treads' speed. */
const double START_Z = 1.45, SPEED = 0.00905;

/* What casts shadows on the ground: Wall-E, the near cubes and props. */
void Casters(const ObjMesh *walle, const RigidAnim *anim, int frame)
{
    RtColor opaque = {1, 1, 1};
    RiOpacity(opaque);
    NearProps();
    BootPlant(-0.75, -0.45);
    RiTransformBegin();
        RiTranslate(0, 0, (RtFloat)(START_Z - SPEED * (frame - 1)));
        DrawWallE(walle, anim, frame, 0, &KEY_SHADOW);
    RiTransformEnd();
}

} // namespace

void WallEMovie(void)
{
    float fov = FOV, lightFov = 40.0f;
    LookAt(EYE, TARGET, CAMERA, KEY_SHADOW.cameraToWorld);

    /* STANDARD's key light, fixed in camera space, in the world; the
     * shadow camera looks from there at Wall-E's path */
    double key[3];
    StandardLightPosition(KEY_SHADOW.cameraToWorld, 0, key);
    const double path[3] = {-0.2, 0.3, 1.2};
    RtMatrix lightCamera;
    LookAt(key, path, lightCamera);
    KEY_SHADOW.light = 0;

    std::error_code ignored;
    std::filesystem::create_directories("renders/3_CinematicScenes/WallEMovie", ignored);
    ObjMesh *walle = obj_load("models/walle/walle.obj");
    RigidAnim *anim = anim_load("models/walle/walle_anim.txt");

    RiBegin(RI_NULL);
        RiMakeTexture("models/walle/walle_color.png", 0);
        RiMakeTexture("models/walle/walle_ao.png", 1);
        for (int i = 1; i <= FRAMES; ++i) {
            int frame = CLIP_FIRST + i - 1;
            CURRENT_Z = START_Z - SPEED * (frame - 1);
            /* pass 1: compute shadow map from the key light */
            KEY_SHADOW.map = NULL;
            RiFrameBegin(2 * frame);
                RiFormat(1024, 1024);
                RiPixelSamples(1, 1);
                RiDisplay("3_CinematicScenes/WallEMovie/key.z", "zfile", "z");
                RiProjection(RI_PERSPECTIVE, "fov", &lightFov);
                RiConcatTransform(lightCamera);
                RiWorldBegin();
                    Casters(walle, anim, frame);
                RiWorldEnd();
            RiFrameEnd();

            /* pass 2: the picture */
            ZFile *map = zfile_load("3_CinematicScenes/WallEMovie/key.z", lightCamera, lightFov);
            KEY_SHADOW.map = map;
            char name[64];
            std::snprintf(name, sizeof name, "3_CinematicScenes/WallEMovie/WallEMovie_%02d.jpg", i - 1);
            RiFrameBegin(2 * frame + 1);
                RiFormat(XRES, YRES);
                RiPixelSamples(SAMPLES, SAMPLES);
                RiDisplay(name, "", "");
                RiProjection(RI_PERSPECTIVE, "fov", &fov);
                RiConcatTransform(CAMERA);
                RiWorldBegin();
                    RtColor opaque = {1, 1, 1};
                    RiOpacity(opaque);
                    Ground();
                    Skyline();
                    Towers();
                    Casters(walle, anim, frame);
                RiWorldEnd();
            RiFrameEnd();
            KEY_SHADOW.map = NULL;
            zfile_free(map);
        }
    RiEnd();
    anim_free(anim);
    obj_free(walle);
}
