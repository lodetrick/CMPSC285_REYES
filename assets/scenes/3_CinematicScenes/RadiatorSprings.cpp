/*
 * ECE/CS 285 – HW1 Cinematic Scene: RadiatorSprings
 * Lightning McQueen (copyright Pixar) parked on a desert road at sunset,
 * among mesas, cacti and long shadows from the low sun, a light the scene
 * adds to STANDARD through an orthographic depth pass.
 *
 * Output: renders/3_CinematicScenes/RadiatorSprings.png
 */

#include "Ri.h"
#include "obj.h"
#include "sceneutil.h"
#include "shadowmap.h"

#include <cmath>
#include <cstring>

namespace {

const double PI = 3.14159265358979323846;

/* ------------------------------------------------------------- camera */

/* Low, ahead of the car and to its left: the car fills the right half and
 * the road runs past it to a vanishing point in the left third. */
const double EYE[3] = {-2.6, 0.75, -8.5};
const double TARGET[3] = {0.3, 2.0, 6.0};

RtMatrix CAMERA;
double CAMERA_TO_WORLD[4][4];

/* --------------------------------------------------------- sky and haze */

/* The sun: 3.5 degrees up, just left of the road's vanishing point. */
const double SUN[3] = {-0.0522, 0.0610, 0.9968};

/* The sunlight: a distant light from the sun's direction, but 10 degrees up
 * rather than 3.5 so it reaches the ground at a useful angle (at 3.5 the
 * shadows would be sixteen times as long as things are tall). Warm, and
 * strong enough to show against STANDARD's rig. */
const double SUN_ELEVATION = 10 * PI / 180;
const double SUN_COLOR[3] = {1.0, 0.62, 0.32}, SUN_INTENSITY = 2.5;
double SUNLIGHT[3]; /* unit vector toward the sun, world space; set in RadiatorSprings() */

/* The sun's depth image, set between the two passes (NULL during the depth
 * pass, where nothing is lit by it yet). */
const ZFile *SUN_MAP = NULL;

/* STANDARD, plus the sunlight where the sun's depth image says nothing
 * stands between the point and the sun: diffuse as in STANDARD, specular
 * with coefficient ks. The ground's is a third of STANDARD's: the low sun's
 * glare down the road is what makes the shadows across it show, but at
 * full strength it washes the asphalt out. */
void Lit(RtShaderContext *c, double ks = 0.1)
{
    STANDARD(c);
    if (!SUN_MAP)
        return;
    double L[3];
    for (int j = 0; j < 3; ++j) /* the sunlight in camera space */
        L[j] = SUNLIGHT[0] * CAMERA[0][j] + SUNLIGHT[1] * CAMERA[1][j] + SUNLIGHT[2] * CAMERA[2][j];
    double diffuse, specular;
    PhongTerm(c, L, 32, diffuse, specular);
    if (diffuse <= 0)
        return; /* facing away from the sun: no need to look up the shadow */
    double Pw[3];
    for (int i = 0; i < 3; ++i)
        Pw[i] = CAMERA_TO_WORLD[i][0] * c->P[0] + CAMERA_TO_WORLD[i][1] * c->P[1] +
                CAMERA_TO_WORLD[i][2] * c->P[2] + CAMERA_TO_WORLD[i][3];
    double visible = zfile_visibility(SUN_MAP, Pw);
    if (visible <= 0)
        return;
    for (int i = 0; i < 3; ++i)
        c->Ci[i] += (RtFloat)(visible * SUN_INTENSITY * SUN_COLOR[i] * (0.8 * c->Cs[i] * diffuse + ks * specular));
}

/* Sky color seen along a world direction d (unit length). */
void SkyColor(const double d[3], RtColor out)
{
    const double horizon[3] = {1.00, 0.58, 0.30}, middle[3] = {0.80, 0.47, 0.50};
    const double zenith[3] = {0.13, 0.17, 0.42};
    double e = std::max(0.0, d[1]);
    double a = Smooth(0.0, 0.16, e), b = Smooth(0.10, 0.65, e);
    double g = std::max(0.0, d[0] * SUN[0] + d[1] * SUN[1] + d[2] * SUN[2]);
    double glow = 0.55 * std::pow(g, 14.0) + 0.6 * std::pow(g, 300.0);
    double disk = Smooth(0.99985, 0.99992, g) * 2.0;
    const double sun[3] = {1.0, 0.82, 0.55};
    for (int i = 0; i < 3; ++i) {
        double c = Mix(Mix(horizon[i], middle[i], a), zenith[i], b);
        out[i] = (RtFloat)(c + (glow + disk) * sun[i]);
    }
}

/* the world direction from the eye to a world point */
void ViewDirection(const RtFloat Po[3], double d[3])
{
    for (int i = 0; i < 3; ++i)
        d[i] = Po[i] - EYE[i];
    double l = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    for (int i = 0; i < 3; ++i)
        d[i] /= l;
}

/* Fade toward the sky behind the point, more with distance. Shaders here
 * place surfaces in world space with no transform, so Po is the world
 * position. */
void Haze(RtShaderContext *c)
{
    double dist = std::sqrt(c->P[0] * c->P[0] + c->P[1] * c->P[1] + c->P[2] * c->P[2]);
    double f = 1 - std::exp(-dist / 550.0);
    double d[3];
    RtColor sky;
    ViewDirection(c->Po, d);
    d[1] = std::max(d[1], 0.0) * 0.5; /* the haze is the low sky */
    SkyColor(d, sky);
    for (int i = 0; i < 3; ++i)
        c->Ci[i] = (RtFloat)Mix(c->Ci[i], sky[i], f);
}

/* The sky: unlit, its color from the view direction alone. */
void Sky(RtShaderContext *c)
{
    double d[3];
    ViewDirection(c->Po, d);
    SkyColor(d, c->Ci);
    c->Oi[0] = c->Oi[1] = c->Oi[2] = 1;
}

/* -------------------------------------------------------- environment */

/* Cheap smooth value variation from sines, for color only. */
double Wobble(double x, double z)
{
    return 0.5 + 0.25 * std::sin(0.37 * x + 1.3 * std::sin(0.11 * z)) +
           0.25 * std::sin(0.23 * z + 1.1 * std::sin(0.17 * x));
}

void Sand(RtShaderContext *c)
{
    double w = Wobble(c->Po[0], c->Po[2]);
    c->Cs[0] = (RtFloat)Mix(0.70, 0.86, w);
    c->Cs[1] = (RtFloat)Mix(0.46, 0.60, w);
    c->Cs[2] = (RtFloat)Mix(0.28, 0.38, w);
    Lit(c);
    Haze(c);
}

/* Asphalt with white edge lines and a dashed yellow center line. s runs
 * across the road, and the dashes are laid out along world z, 3.6 m long
 * every 9 m, one of them running off the bottom of the picture
 * (z = -6.4 to -2.8). */
void Road(RtShaderContext *c)
{
    double s = c->u, z = c->Po[2];
    double w = Wobble(3.1 * c->Po[0], 2.7 * z);
    double col[3] = {0.24 + 0.06 * w, 0.23 + 0.06 * w, 0.24 + 0.06 * w};
    /* worn tire tracks, a little darker, two in each lane */
    double lane = std::fmod(s, 0.5) - 0.25;
    if (std::fabs(std::fabs(lane) - 0.1) < 0.035)
        for (double &x : col)
            x *= 0.82;
    bool edge = (s > 0.035 && s < 0.052) || (s > 0.948 && s < 0.965);
    bool center = std::fabs(s - 0.5) < 0.0095 && std::fmod(z + 996.4, 9.0) < 3.6;
    if (edge)
        col[0] = col[1] = col[2] = 0.85;
    if (center)
        col[0] = 0.95, col[1] = 0.75, col[2] = 0.15;
    for (int i = 0; i < 3; ++i)
        c->Cs[i] = (RtFloat)col[i];
    Lit(c);
    Haze(c);
}

/* Sandstone strata: wavy horizontal bands of red, light and dark rock. */
void Sandstone(RtShaderContext *c)
{
    double y = c->Po[1], x = c->Po[0], z = c->Po[2];
    double b = 0.5 + 0.5 * std::sin(0.55 * y + 0.9 * std::sin(0.031 * x + 0.027 * z));
    double fine = 0.5 + 0.5 * std::sin(2.3 * y + 0.6 * std::sin(0.08 * x));
    const double red[3] = {0.66, 0.29, 0.15}, light[3] = {0.85, 0.55, 0.33}, dark[3] = {0.42, 0.17, 0.10};
    for (int i = 0; i < 3; ++i) {
        double cband = Mix(dark[i], red[i], Smooth(0.0, 0.6, b));
        cband = Mix(cband, light[i], Smooth(0.75, 1.0, b));
        c->Cs[i] = (RtFloat)(cband * (0.88 + 0.12 * fine));
    }
    Lit(c);
    Haze(c);
}

/* Saguaro green with vertical ribs. */
void Cactus(RtShaderContext *c)
{
    double rib = 0.5 + 0.5 * std::cos(2 * PI * 14 * c->u);
    c->Cs[0] = (RtFloat)(0.20 * (0.75 + 0.25 * rib));
    c->Cs[1] = (RtFloat)(0.40 * (0.75 + 0.25 * rib));
    c->Cs[2] = (RtFloat)(0.17 * (0.75 + 0.25 * rib));
    Lit(c);
    Haze(c);
}

/* A traffic cone: orange with a white band (v runs from base to tip). */
void TrafficCone(RtShaderContext *c)
{
    bool band = c->v > 0.42 && c->v < 0.62;
    c->Cs[0] = band ? 0.92f : 0.95f;
    c->Cs[1] = band ? 0.92f : 0.38f;
    c->Cs[2] = band ? 0.90f : 0.08f;
    Lit(c);
}

/* Lit, then hazed: for poles and rocks, which use the current color. */
void Distant(RtShaderContext *c)
{
    Lit(c);
    Haze(c);
}

/* A soft contact shadow: black, its opacity falling off from the middle
 * (s = 0) to the rim (s = 1). The sun's shadow stretches out in front of
 * the car; this darkens the road right under it, where little light of any
 * kind reaches. */
void ContactShadow(RtShaderContext *c)
{
    double r = c->u, o = 0.78 * (1 - Smooth(0.68, 1.0, r));
    c->Ci[0] = c->Ci[1] = c->Ci[2] = 0;
    c->Oi[0] = c->Oi[1] = c->Oi[2] = (RtFloat)o;
}

/* An ellipse a little above the road, as a fan of triangles around its
 * center, with s = 0 at the center and 1 on the rim. */
void ShadowBlob(double rx, double rz)
{
    enum { NS = 40 };
    RtPoint P[NS + 1];
    RtFloat st[2 * (NS + 1)];
    RtInt nverts[NS], verts[3 * NS];
    P[0][0] = 0, P[0][1] = 0.01f, P[0][2] = 0;
    st[0] = 0, st[1] = 0;
    for (int i = 0; i < NS; ++i) {
        double a = 2 * PI * i / NS;
        P[i + 1][0] = (RtFloat)(rx * std::cos(a));
        P[i + 1][1] = 0.01f;
        P[i + 1][2] = (RtFloat)(rz * std::sin(a));
        st[2 * (i + 1)] = 1;
        st[2 * (i + 1) + 1] = 0;
        nverts[i] = 3;
        verts[3 * i] = 0;
        verts[3 * i + 1] = i + 1;
        verts[3 * i + 2] = (i + 1) % NS + 1;
    }
    RiSurface(ContactShadow, NULL, 0);
    RiPointsPolygons(NS, nverts, verts, P, NULL, st);
}

/* ------------------------------------------------------- the car */

/* The car's paint, eyes and tires: the texture in a slot (the params),
 * then lit. */
void CarPaint(RtShaderContext *c)
{
    RiTexture(*(const RtInt *)c->params, c->u, c->v, c->Cs);
    Lit(c, 0.3);
}

void Car(const ObjMesh *car)
{
    RtColor white = {1, 1, 1}, opaque = {1, 1, 1};
    RtColor glass = {0.55f, 0.65f, 0.75f}, glassOpacity = {0.45f, 0.45f, 0.45f};
    for (int k = 0; car && k < car->ngroups; ++k) {
        const ObjGroup &g = car->groups[k];
        RtInt slot = -1;
        if (std::strcmp(g.material, "body") == 0)
            slot = 0;
        else if (std::strcmp(g.material, "eyes") == 0)
            slot = 1;
        else if (std::strcmp(g.material, "tires") == 0)
            slot = 2;
        if (slot >= 0) {
            RiColor(white);
            RiOpacity(opaque);
            RiSurface(CarPaint, &slot, sizeof slot);
        } else {
            RiColor(glass);
            RiOpacity(glassOpacity);
            RiSurface(NULL, NULL, 0);
        }
        RiPointsPolygons(g.npolys, g.nverts, g.verts, g.P, g.N, g.st);
    }
    RiOpacity(opaque);
}

/* The car's place: in the right lane, its rear tire just inside the edge
 * line, turned to show its side, its nose (-z in the model) 22 degrees left
 * of pointing at the camera. */
void PlaceCar(void)
{
    RiTranslate(-0.35f, 0, 1.0f);
    RiRotate(38, 0, 1, 0);
}

/* ------------------------------------------------------- ground */

const double ROAD_LEFT = -5.6, ROAD_RIGHT = 1.9; /* the car sits in the right lane */

/* Quads at y = 0 covering x0..x1 (nx tiles across) and z0..z1 (tiles
 * dz long), with s running across the whole width and t along each tile,
 * as one RiPointsPolygons call. Tiles that come near the eye plane are
 * left out. Every tile has its corners in the same order and meets its
 * neighbors edge to edge, so displacement leaves no cracks. */
void Tiles(double x0, double x1, int nx, double z0, double z1, double dz, bool withST)
{
    int nz = (int)std::ceil((z1 - z0) / dz), n = nx * nz;
    RtPoint *P = new RtPoint[4 * n];
    RtFloat *st = new RtFloat[8 * n];
    RtInt *nverts = new RtInt[n], *verts = new RtInt[4 * n];
    int npolys = 0;
    for (int j = 0; j < nz; ++j)
        for (int i = 0; i < nx; ++i) {
            double xa = x0 + (x1 - x0) * i / nx, xb = x0 + (x1 - x0) * (i + 1) / nx;
            double za = z0 + j * dz, zb = za + dz;
            RtFloat corner[4][3] = {{(RtFloat)xa, 0, (RtFloat)za}, {(RtFloat)xb, 0, (RtFloat)za},
                                    {(RtFloat)xb, 0, (RtFloat)zb}, {(RtFloat)xa, 0, (RtFloat)zb}};
            bool nearEye = false;
            for (int k = 0; k < 4; ++k)
                nearEye = nearEye || CameraZ(CAMERA, corner[k]) < 0.3;
            if (nearEye)
                continue;
            const double s[4] = {(double)i / nx, (double)(i + 1) / nx, (double)(i + 1) / nx, (double)i / nx};
            const double t[4] = {0, 0, 1, 1};
            for (int k = 0; k < 4; ++k) {
                int v = 4 * npolys + k;
                for (int c = 0; c < 3; ++c)
                    P[v][c] = corner[k][c];
                st[2 * v] = (RtFloat)s[k];
                st[2 * v + 1] = (RtFloat)t[k];
                verts[v] = v;
            }
            nverts[npolys++] = 4;
        }
    RiPointsPolygons(npolys, nverts, verts, P, NULL, withST ? st : NULL);
    delete[] P;
    delete[] st;
    delete[] nverts;
    delete[] verts;
}

/* Sand on both sides of the road, in 2 m tiles out to the far distance.
 * Most of them are off screen and cost only a bound test. */
void Desert(void)
{
    Tiles(-300, ROAD_LEFT, (int)((ROAD_LEFT + 300) / 2), -12, 300, 2, false);
    Tiles(ROAD_RIGHT, 400, (int)((400 - ROAD_RIGHT) / 2), -12, 300, 2, false);
    /* beyond 300 m, 10 m tiles (the haze hides any seam at the change) */
    Tiles(-450, ROAD_LEFT, 44, 300, 1500, 10, false);
    Tiles(ROAD_RIGHT, 1200, 120, 300, 1500, 10, false);
}

/* ------------------------------------------------------- mesas */

/* A butte: an irregular outline, a talus slope that steepens into cliffs
 * that narrow by the fraction taper toward the top, as quads with smooth
 * vertex normals (averaged face normals), so BUMPY moves neighboring
 * polygons together along their shared edges. */
void Mesa(double cx, double cz, double radius, double height, double taper, double phase)
{
    enum { NA = 72, NR = 9 };
    const double ringY[NR] = {-0.06, 0.04, 0.12, 0.2, 0.28, 0.36, 0.6, 0.85, 1.0};
    const double ringR[NR] = {1.75, 1.55, 1.38, 1.24, 1.12, 1.03, 1.0, 0.985, 0.97};
    static RtPoint P[NA * NR];
    static RtNormal N[NA * NR];
    static RtInt nverts[NA * (NR - 1)], verts[4 * NA * (NR - 1)];

    for (int a = 0; a < NA; ++a) {
        double th = 2 * PI * a / NA;
        double r = radius * (1 + 0.22 * std::sin(2 * th + phase) + 0.12 * std::sin(5 * th + 2.1 * phase) +
                             0.06 * std::sin(11 * th + 0.7 * phase));
        for (int k = 0; k < NR; ++k) {
            double narrow = 1 - taper * std::max(0.0, (ringY[k] - 0.36) / 0.64);
            RtFloat *p = P[k * NA + a];
            p[0] = (RtFloat)(cx + r * ringR[k] * narrow * std::cos(th));
            p[1] = (RtFloat)(height * ringY[k]);
            p[2] = (RtFloat)(cz + r * ringR[k] * narrow * std::sin(th));
        }
    }

    int npolys = 0;
    for (int v = 0; v < NA * NR; ++v)
        N[v][0] = N[v][1] = N[v][2] = 0;
    for (int k = 0; k + 1 < NR; ++k)
        for (int a = 0; a < NA; ++a) {
            int a1 = (a + 1) % NA;
            int q[4] = {k * NA + a, k * NA + a1, (k + 1) * NA + a1, (k + 1) * NA + a};
            /* face normal from the diagonals, added to each corner */
            double d1[3], d2[3];
            for (int c = 0; c < 3; ++c) {
                d1[c] = P[q[2]][c] - P[q[0]][c];
                d2[c] = P[q[3]][c] - P[q[1]][c];
            }
            double n[3] = {d1[1] * d2[2] - d1[2] * d2[1], d1[2] * d2[0] - d1[0] * d2[2],
                           d1[0] * d2[1] - d1[1] * d2[0]};
            for (int i = 0; i < 4; ++i) {
                for (int c = 0; c < 3; ++c)
                    N[q[i]][c] += (RtFloat)n[c];
                verts[4 * npolys + i] = q[i];
            }
            nverts[npolys++] = 4;
        }
    RiPointsPolygons(npolys, nverts, verts, P, N, NULL);
}

/* ------------------------------------------------------- props */

/* A saguaro at (x, z): trunk, cap, and arms that leave the trunk at
 * heights armH[i] on the side angle[i] (degrees), bend up through a
 * quarter torus and rise armL[i]. */
void Saguaro(double x, double z, double height, double r, int narms, const double *armH,
             const double *angle, const double *armL)
{
    RiTransformBegin();
        RiTranslate((RtFloat)x, 0, (RtFloat)z);
        RiRotate(-90, 1, 0, 0); /* z up */
        RiCylinder((RtFloat)r, -0.3f, (RtFloat)height, 360);
        RiTransformBegin();
            RiTranslate(0, 0, (RtFloat)height);
            RiSphere((RtFloat)r, 0, (RtFloat)r, 360);
        RiTransformEnd();
        for (int i = 0; i < narms; ++i) {
            double ra = 0.72 * r, bend = 2.2 * r;
            RiTransformBegin();
                RiRotate((RtFloat)angle[i], 0, 0, 1);
                RiTransformBegin(); /* elbow: from inside the trunk out and up */
                    RiTranslate(0, 0, (RtFloat)(armH[i] + bend));
                    RiRotate(90, 1, 0, 0);
                    RiRotate(-90, 0, 0, 1);
                    RiTorus((RtFloat)bend, (RtFloat)ra, 0, 360, 90);
                RiTransformEnd();
                RiTranslate((RtFloat)bend, 0, (RtFloat)(armH[i] + bend));
                RiCylinder((RtFloat)ra, 0, (RtFloat)armL[i], 360);
                RiTranslate(0, 0, (RtFloat)armL[i]);
                RiSphere((RtFloat)ra, 0, (RtFloat)ra, 360);
            RiTransformEnd();
        }
    RiTransformEnd();
}

/* A telephone pole with a crossbar, at (x, z). */
void Pole(double x, double z)
{
    RiTransformBegin();
        RiTranslate((RtFloat)x, 0, (RtFloat)z);
        RiTransformBegin();
            RiRotate(-90, 1, 0, 0);
            RiCylinder(0.13f, -0.2f, 8.5f, 360);
        RiTransformEnd();
        RiTransformBegin();
            RiTranslate(0, 7.9f, 0);
            RiRotate(90, 0, 1, 0);
            RiCylinder(0.07f, -1.3f, 1.3f, 360);
        RiTransformEnd();
    RiTransformEnd();
}

void Cone(double x, double z)
{
    RiTransformBegin();
        RiTranslate((RtFloat)x, 0, (RtFloat)z);
        RiRotate(-90, 1, 0, 0);
        RiCone(0.72f, 0.19f, 360);
    RiTransformEnd();
}

void Rock(double x, double z, double r, double sy, double turn)
{
    RiTransformBegin();
        RiTranslate((RtFloat)x, (RtFloat)(0.15 * r), (RtFloat)z);
        RiRotate((RtFloat)turn, 0, 1, 0);
        RiScale(1.3f, (RtFloat)sy, 1);
        RiSphere((RtFloat)r, (RtFloat)-r, (RtFloat)r, 360);
    RiTransformEnd();
}

/* Everything standing on the ground, which casts the sun's shadows: drawn
 * in both passes. unit is how many meters one unit of camera space is: 1
 * in the picture, more in the sun's depth pass, whose camera is scaled to
 * fit its window (see RadiatorSprings()). BUMPY displaces by its amplitude
 * in camera space, so the rocks' amplitude is divided by it to give them
 * the same shape in both passes; otherwise they would swell into spiky
 * boulders meters across in the depth image and cast shadows where no rock
 * stands. */
void Casters(const ObjMesh *car, double unit)
{
    /* telephone poles along the right shoulder */
    {
        RtColor wood = {0.36f, 0.25f, 0.16f};
        RiColor(wood);
        RiSurface(Distant, NULL, 0);
        for (int i = 0; i < 14; ++i)
            Pole(4.8, 14 + 32.0 * i);
    }

    /* cacti */
    {
        RiSurface(Cactus, NULL, 0);
        const double h1[2] = {1.5, 2.2}, a1[2] = {20, 200}, l1[2] = {1.2, 0.9};
        Saguaro(-9.5, 22, 4.2, 0.3, 2, h1, a1, l1);
        const double h2[1] = {1.8}, a2[1] = {200}, l2[1] = {1.1};
        Saguaro(-14, 55, 3.6, 0.27, 1, h2, a2, l2);
        const double h3[2] = {2.0, 2.6}, a3[2] = {10, 190}, l3[2] = {1.4, 0.8};
        Saguaro(40, 55, 5.0, 0.32, 2, h3, a3, l3);
        const double h4[2] = {1.6, 2.4}, a4[2] = {160, 340}, l4[2] = {1.0, 1.3};
        Saguaro(-30, 110, 4.4, 0.3, 2, h4, a4, l4);
        Saguaro(-8, 150, 3.8, 0.28, 1, h2, a1, l2);
    }

    /* rocks */
    {
        RtColor stone = {0.50f, 0.36f, 0.27f};
        BumpParams lumps = { (RtFloat)(0.18 / unit), 0, 3 };
        RiColor(stone);
        RiSurface(Distant, NULL, 0);
        RiDisplacement(BUMPY, &lumps, sizeof lumps);
        Rock(-6.8, 10, 0.55, 0.6, 20);
        Rock(-8.0, 14, 0.35, 0.7, 70);
        Rock(-12, 30, 0.8, 0.6, 10);
        Rock(30, 40, 0.9, 0.6, 140);
        RiDisplacement(NULL, NULL, 0);
    }

    /* traffic cones down the middle of the road behind the car */
    RiSurface(TrafficCone, NULL, 0);
    Cone(-3.7, 6.5);
    Cone(-2.9, 11.0);
    Cone(-2.1, 15.5);

    /* the car */
    RiTransformBegin();
        PlaceCar();
        Car(car);
    RiTransformEnd();
}

} // namespace

void RadiatorSprings(void)
{
    float fov = 40.0f;
    RtColor opaque = {1, 1, 1};

    LookAt(EYE, TARGET, CAMERA, CAMERA_TO_WORLD);

    /* The sunlight's direction: the sun's, raised to SUN_ELEVATION. The
     * sun's depth image looks back along it, orthographically, from 800 m
     * out, at a window 84 m across centered on the ground 200 m down the
     * road: that takes in everything that casts a shadow and everywhere the
     * shadows fall, from the car's to the far poles' (found by projecting
     * them all). */
    double across = std::sqrt(SUN[0] * SUN[0] + SUN[2] * SUN[2]);
    SUNLIGHT[0] = SUN[0] / across * std::cos(SUN_ELEVATION);
    SUNLIGHT[1] = std::sin(SUN_ELEVATION);
    SUNLIGHT[2] = SUN[2] / across * std::cos(SUN_ELEVATION);
    const double SUN_VIEW_CENTER[3] = {-1, 0, 197.5}, SUN_VIEW_HALF_WIDTH = 42;
    double sunEye[3];
    for (int i = 0; i < 3; ++i)
        sunEye[i] = SUN_VIEW_CENTER[i] + 800 * SUNLIGHT[i];
    RtMatrix sunCamera;
    LookAt(sunEye, SUN_VIEW_CENTER, sunCamera);
    const RtFloat toWindow = (RtFloat)(1 / SUN_VIEW_HALF_WIDTH); /* the window is [-1, 1] */

    ObjMesh *car = obj_load("assets/models/mcqueen/mcqueen.obj");

    RiBegin(RI_NULL);
        RiMakeTexture("assets/models/mcqueen/McQueenLivery.jpg", 0);
        RiMakeTexture("assets/models/mcqueen/Eyes_McQueen_Eyes.jpg", 1);
        RiMakeTexture("assets/models/mcqueen/McQ_Tires.png", 2);

        /* pass 1: compute shadow map of everything on ground from sun */
        RiFrameBegin(0);
            RiFormat(2048, 2048);
            RiPixelSamples(1, 1);
            RiDisplay("3_CinematicScenes/RadiatorSprings_sun.z", "zfile", "z");
            RiProjection(RI_ORTHOGRAPHIC);
            RiScale(toWindow, toWindow, toWindow); /* after the camera: last in, first applied */
            RiConcatTransform(sunCamera);
            RiWorldBegin();
                RiOpacity(opaque);
                Casters(car, SUN_VIEW_HALF_WIDTH);
            RiWorldEnd();
        RiFrameEnd();

        /* pass 2: the picture, in the sunlight */
        ZFile *map = zfile_load("3_CinematicScenes/RadiatorSprings_sun.z", sunCamera, 0, toWindow);
        SUN_MAP = map;
        RiFrameBegin(1);
            RiFormat(960, 720);
            RiPixelSamples(4, 4);
            RiDisplay("3_CinematicScenes/RadiatorSprings.png", "", "");
            RiProjection(RI_PERSPECTIVE, "fov", &fov);
            RiConcatTransform(CAMERA);
            RiWorldBegin();
                RiOpacity(opaque);

                /* sky: one big polygon far behind everything, facing the eye */
                {
                    RiSurface(Sky, NULL, 0);
                    double f[3] = {TARGET[0] - EYE[0], TARGET[1] - EYE[1], TARGET[2] - EYE[2]};
                    double l = std::sqrt(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]);
                    double cx = EYE[0] + 4000 * f[0] / l, cz = EYE[2] + 4000 * f[2] / l;
                    double sx = f[2] / l, sz = -f[0] / l; /* sideways, horizontal */
                    RtPoint P[4] = {{(RtFloat)(cx - 4000 * sx), -300, (RtFloat)(cz - 4000 * sz)},
                                    {(RtFloat)(cx + 4000 * sx), -300, (RtFloat)(cz + 4000 * sz)},
                                    {(RtFloat)(cx + 4000 * sx), 3500, (RtFloat)(cz + 4000 * sz)},
                                    {(RtFloat)(cx - 4000 * sx), 3500, (RtFloat)(cz - 4000 * sz)}};
                    RtInt nverts[1] = {4}, verts[4] = {0, 1, 2, 3};
                    RiPointsPolygons(1, nverts, verts, P, NULL, NULL);
                }

                /* ground: sand, and the road with its markings */
                {
                    BumpParams ripples = { 0.05f, 0, 2 };
                    RiSurface(Sand, NULL, 0);
                    RiDisplacement(BUMPY, &ripples, sizeof ripples);
                    Desert();
                    RiDisplacement(NULL, NULL, 0);
                    RiSurface(Road, NULL, 0);
                    /* fine near the camera, so no visible tile meets the
                     * eye-plane rule; the road is flat, so the change of
                     * tile size leaves no gap */
                    Tiles(ROAD_LEFT, ROAD_RIGHT, 1, -12, 4, 0.5, true);
                    Tiles(ROAD_LEFT, ROAD_RIGHT, 1, 4, 1500, 2, true);
                }

                /* mesas on the horizon */
                {
                    BumpParams rock = { 3.0f, -4, 0 };
                    RiSurface(Sandstone, NULL, 0);
                    RiDisplacement(BUMPY, &rock, sizeof rock);
                    Mesa(-109, 541, 26, 105, 0.35, 0.4); /* a tall butte, the sun right of it */
                    Mesa(76, 888, 75, 75, 0.05, 1.9);    /* a wide mesa past the vanishing point */
                    Mesa(195, 600, 32, 115, 0.25, 3.1);  /* a butte above the car's roof */
                    Mesa(257, 442, 50, 55, 0.1, 4.4);    /* a low mesa on the right */
                    Mesa(450, 1150, 150, 60, 0.05, 5.6); /* far and hazy, behind the others */
                    RiDisplacement(NULL, NULL, 0);
                }

                Casters(car, 1);
                RiTransformBegin();
                    PlaceCar();
                    ShadowBlob(1.62, 2.95);
                RiTransformEnd();
            RiWorldEnd();
        RiFrameEnd();
        SUN_MAP = NULL;
        zfile_free(map);
    RiEnd();
    obj_free(car);
}
