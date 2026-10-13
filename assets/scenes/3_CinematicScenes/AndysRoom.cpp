/*
 * ECE/CS 285 – HW1 Cinematic Scene: AndysRoom
 * Buzz Lightyear flying past Woody in a toy's-eye view of Andy's room, with
 * the Pixar ball (all copyright Pixar): cloud wallpaper, an oak floor after
 * the Advanced RenderMan oak shader (sceneutil.h), alphabet blocks, and shadows from the
 * key light's depth map.
 *
 * Output: renders/3_CinematicScenes/AndysRoom.png
 */

#include "Ri.h"
#include "models.h"
#include "obj.h"
#include "sceneutil.h"
#include "shadowmap.h"

#include <cmath>
#include <cstring>

namespace {

const double PI = 3.14159265358979323846;

/* a repeatable pseudo-random number in [0, 1) for an integer */
double Hash(int n)
{
    unsigned h = (unsigned)n * 2654435761u;
    h ^= h >> 15;
    h *= 2246822519u;
    h ^= h >> 13;
    return (h & 0xffffff) / 16777216.0;
}

/* ------------------------------------------------------------ shadows */

/* The key light's shadow map, set between the two passes (NULL during the
 * depth pass, where nothing is shadowed yet). Every shader here lights with
 * Lit() instead of STANDARD, so they all take the shadow. */
ShadowParams KEY_SHADOW;

void Lit(RtShaderContext *c) { ShadowedStandard(c, &KEY_SHADOW); }

/* ------------------------------------------------------------- camera */

const double EYE[3] = {-1.2, 1.1, -4.6};
const double TARGET[3] = {-0.6, 1.9, 2.0};
RtMatrix CAMERA;

/* A rectangle of quads in a plane: corner o, edges a and b (world vectors),
 * na x nb tiles, with st running 0..1 over the whole rectangle. Tiles that
 * come near the eye plane are left out. */
void Panel(const double o[3], const double a[3], const double b[3], int na, int nb, bool withST)
{
    int n = na * nb;
    RtPoint *P = new RtPoint[4 * n];
    RtFloat *st = new RtFloat[8 * n];
    RtInt *nverts = new RtInt[n], *verts = new RtInt[4 * n];
    int npolys = 0;
    for (int j = 0; j < nb; ++j)
        for (int i = 0; i < na; ++i) {
            const double s[4] = {(double)i / na, (double)(i + 1) / na, (double)(i + 1) / na, (double)i / na};
            const double t[4] = {(double)j / nb, (double)j / nb, (double)(j + 1) / nb, (double)(j + 1) / nb};
            RtFloat corner[4][3];
            bool nearEye = false;
            for (int k = 0; k < 4; ++k) {
                for (int c = 0; c < 3; ++c)
                    corner[k][c] = (RtFloat)(o[c] + s[k] * a[c] + t[k] * b[c]);
                nearEye = nearEye || CameraZ(CAMERA, corner[k]) < 0.3;
            }
            if (nearEye)
                continue;
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

/* -------------------------------------------------------------- room */

const double BACK_Z = 7.0, LEFT_X = -6.5, RIGHT_X = 9.0, NEAR_Z = -7.0, HEIGHT = 10.0;

/* STANDARD without its specular term: run it again on a black surface,
 * which leaves only the highlights, and take them away. For paper. */
void Matte(RtShaderContext *c)
{
    RtColor Cs = {c->Cs[0], c->Cs[1], c->Cs[2]}, specular;
    c->Cs[0] = c->Cs[1] = c->Cs[2] = 0;
    Lit(c);
    for (int i = 0; i < 3; ++i) {
        specular[i] = c->Ci[i];
        c->Cs[i] = Cs[i];
    }
    Lit(c);
    for (int i = 0; i < 3; ++i)
        c->Ci[i] -= specular[i];
}

/* Andy's wallpaper: puffy white clouds on sky blue, in a staggered repeat.
 * h runs along both walls continuously (x + z is the same at the corner),
 * so the pattern wraps around the corner. */
double CloudCoverage(double h, double y)
{
    const double W = 3.0, H = 2.2;
    int row = (int)std::floor(y / H);
    double hh = h + (row % 2 ? 0.5 * W : 0) + 0.37 * Hash(row);
    int col = (int)std::floor(hh / W);
    double lx = hh - (col + 0.5) * W, ly = y - (row + 0.5) * H;
    double scale = 0.85 + 0.3 * Hash(31 * row + col);
    lx /= scale, ly /= scale;
    if (col % 3 == 1 && row % 2 == 0)
        return 0; /* leave some cells empty */
    /* a cloud: a union of circles over a flat bottom */
    const double cx[5] = {0, -0.55, 0.58, -0.22, 0.25}, cy[5] = {0.05, -0.12, -0.1, 0.3, 0.27};
    const double cr[5] = {0.52, 0.38, 0.4, 0.33, 0.36};
    double inside = 0;
    for (int k = 0; k < 5; ++k) {
        double d = std::sqrt((lx - cx[k]) * (lx - cx[k]) + (ly - cy[k]) * (ly - cy[k]));
        inside = std::max(inside, 1 - Smooth(cr[k] - 0.025, cr[k] + 0.025, d));
    }
    return inside * Smooth(-0.36, -0.31, ly);
}

void Wallpaper(RtShaderContext *c)
{
    double h = c->Po[0] + c->Po[2], y = c->Po[1];
    double cloud = CloudCoverage(h, y);
    const double sky[3] = {0.40, 0.62, 0.92}, white[3] = {0.98, 0.98, 1.0};
    for (int i = 0; i < 3; ++i)
        c->Cs[i] = (RtFloat)Mix(sky[i], white[i], cloud);
    Matte(c);
}

/* A pixel's size in shading units on a surface seen edge-on, where its
 * footprint stretches along the view. On the floor and the skirting, the
 * view runs roughly along the boards, and the pattern changes several
 * times more slowly that way than across them (the arches are long, the
 * pores stretched), so only part of the stretch counts. */
double BoardFilterWidth(const RtShaderContext *c, double K)
{
    double dist = std::sqrt(c->P[0] * c->P[0] + c->P[1] * c->P[1] + c->P[2] * c->P[2]);
    double facing = std::fabs(c->N[0] * c->P[0] + c->N[1] * c->P[1] + c->N[2] * c->P[2]) / dist;
    double across = K * dist * 0.00115; /* 2 tan(fov / 2) / yres */
    return std::max(across, 0.15 * across / std::max(0.02, facing));
}

/* Floor boards along z, 0.9 m wide (15 cm at real size), joints
 * staggered. Each board is a cut a few degrees off the axis of its own
 * log, well away from the pith, laid one way round or the other. Honey
 * oak colors (Gritz's are darker and redder) and dark seams. */
void Planks(RtShaderContext *c)
{
    /* K = 6: rings about 2 cm apart here, 3.5 mm at real size */
    const double WIDTH = 0.9, LENGTH = 7.0, K = 6.0; /* shading units per meter */
    double x = c->Po[0], z = c->Po[2];
    int p = (int)std::floor(x / WIDTH);
    double zz = z + LENGTH * Hash(p);
    int q = (int)std::floor(zz / LENGTH);
    double fx = x / WIDTH - p, fz = zz / LENGTH - q;
    int board = 1000 * p + q;

    double a = (fx - 0.5) * WIDTH * K, b = (fz - 0.5) * LENGTH * K;
    if (Hash(board + 41) < 0.5)
        b = -b;
    double alpha = 2.5 + 3 * Hash(board + 17);
    double wood = OakCut(a, b, alpha, 3 * (Hash(board + 7) - 0.5), 7.5 + 6.0 * Hash(board + 13),
                         200 * Hash(board + 29), BoardFilterWidth(c, K));

    const double light[3] = {0.82, 0.60, 0.37}, dark[3] = {0.64, 0.42, 0.22}, seamc[3] = {0.10, 0.06, 0.03};
    double shade = 0.9 + 0.2 * Hash(board + 3);
    double seam = std::min(Smooth(0.0, 0.012, fx) * Smooth(1.0, 0.988, fx),
                           Smooth(0.0, 0.003, fz) * Smooth(1.0, 0.997, fz));
    for (int i = 0; i < 3; ++i)
        c->Cs[i] = (RtFloat)Mix(seamc[i], shade * Mix(light[i], dark[i], wood), seam);
    Lit(c);
}

/* Skirting boards: the same oak stained dark walnut, in 4 m lengths along
 * both walls (h = x + z runs continuously around the corner, as for the
 * wallpaper), with a dark line along the top. */
void Skirting(RtShaderContext *c)
{
    const double HEIGHT = 0.75, LENGTH = 4.0, K = 6.0;
    double h = c->Po[0] + c->Po[2], y = c->Po[1];
    int n = (int)std::floor(h / LENGTH);
    double fh = h / LENGTH - n;
    double a = (y / HEIGHT - 0.5) * HEIGHT * K, b = (fh - 0.5) * LENGTH * K;
    double wood = OakCut(a, b, 3 + 2 * Hash(n + 5), 0.9 * (Hash(n + 9) - 0.5), 9.0 + 3.0 * Hash(n + 11),
                         300 + 100 * Hash(n), BoardFilterWidth(c, K));
    const double light[3] = {0.47, 0.28, 0.14}, dark[3] = {0.35, 0.19, 0.09}, line[3] = {0.12, 0.06, 0.03};
    double top = Smooth(HEIGHT - 0.05, HEIGHT - 0.035, y) * (1 - Smooth(HEIGHT - 0.02, HEIGHT - 0.005, y));
    double joint = Smooth(0.0, 0.004, fh) * Smooth(1.0, 0.996, fh);
    for (int i = 0; i < 3; ++i)
        c->Cs[i] = (RtFloat)Mix(line[i], Mix(light[i], dark[i], wood), joint * (1 - top));
    Lit(c);
}

void Room(void)
{
    RiSurface(Planks, NULL, 0);
    {
        double o[3] = {LEFT_X, 0, NEAR_Z}, a[3] = {RIGHT_X - LEFT_X, 0, 0}, b[3] = {0, 0, BACK_Z - NEAR_Z};
        Panel(o, a, b, 31, 28, false);
    }
    RiSurface(Wallpaper, NULL, 0);
    {
        double o[3] = {LEFT_X, 0, BACK_Z}, a[3] = {RIGHT_X - LEFT_X, 0, 0}, b[3] = {0, HEIGHT, 0};
        Panel(o, a, b, 12, 8, false);
        double o2[3] = {LEFT_X, 0, NEAR_Z}, a2[3] = {0, 0, BACK_Z - NEAR_Z}, b2[3] = {0, HEIGHT, 0};
        Panel(o2, a2, b2, 12, 8, false);
    }
    /* dark wooden skirting boards along both walls */
    RiSurface(Skirting, NULL, 0);
    {
        double o[3] = {LEFT_X, 0, BACK_Z - 0.06}, a[3] = {RIGHT_X - LEFT_X, 0, 0}, b[3] = {0, 0.75, 0};
        Panel(o, a, b, 12, 1, false);
        double o2[3] = {LEFT_X + 0.06, 0, NEAR_Z}, a2[3] = {0, 0, BACK_Z - NEAR_Z}, b2[3] = {0, 0.75, 0};
        Panel(o2, a2, b2, 12, 1, false);
    }
}

/* ------------------------------------------------------------- props */

/* An alphabet block face: a natural wood border around a colored panel
 * with a light disk in the middle. */
void BlockFace(RtShaderContext *c)
{
    double s = c->u - 0.5, t = c->v - 0.5;
    double edge = std::max(std::fabs(s), std::fabs(t));
    double panel = 1 - Smooth(0.36, 0.38, edge);
    double disk = 1 - Smooth(0.2, 0.22, std::sqrt(s * s + t * t));
    const double wood[3] = {0.86, 0.68, 0.45}, cream[3] = {0.97, 0.93, 0.80};
    for (int i = 0; i < 3; ++i) {
        double col = Mix(wood[i], c->Cs[i], panel);
        c->Cs[i] = (RtFloat)Mix(col, cream[i], disk * panel);
    }
    Lit(c);
}

/* Block colors: red, blue, green, yellow. */
const RtFloat PALETTE[4][3] = {{0.85f, 0.15f, 0.12f}, {0.15f, 0.35f, 0.85f}, {0.20f, 0.65f, 0.25f}, {0.95f, 0.75f, 0.10f}};

/* A cube block standing on the floor at (x, y0, z), turned by angle about
 * y, each face colored PALETTE[colors[face]]. */
void Block(double x, double y0, double z, double size, double angle, const int colors[6])
{
    /* corners of a unit cube, faces as quads in cycle order */
    static const double C[8][3] = {{-0.5, 0, -0.5}, {0.5, 0, -0.5}, {0.5, 1, -0.5}, {-0.5, 1, -0.5},
                                   {-0.5, 0, 0.5},  {0.5, 0, 0.5},  {0.5, 1, 0.5},  {-0.5, 1, 0.5}};
    static const int F[6][4] = {{0, 1, 2, 3}, {5, 4, 7, 6}, {4, 0, 3, 7}, {1, 5, 6, 2}, {3, 2, 6, 7}, {4, 5, 1, 0}};
    RiTransformBegin();
        RiTranslate((RtFloat)x, (RtFloat)y0, (RtFloat)z);
        RiRotate((RtFloat)angle, 0, 1, 0);
        RiScale((RtFloat)size, (RtFloat)size, (RtFloat)size);
        RiSurface(BlockFace, NULL, 0);
        for (int f = 0; f < 6; ++f) {
            RtPoint P[4];
            for (int k = 0; k < 4; ++k)
                for (int c = 0; c < 3; ++c)
                    P[k][c] = (RtFloat)C[F[f][k]][c];
            RtFloat st[8] = {0, 0, 1, 0, 1, 1, 0, 1};
            RtInt nverts[1] = {4}, verts[4] = {0, 1, 2, 3};
            RtColor color = {PALETTE[colors[f]][0], PALETTE[colors[f]][1], PALETTE[colors[f]][2]};
            RiColor(color);
            RiPointsPolygons(1, nverts, verts, P, NULL, st);
        }
    RiTransformEnd();
}

/* The Pixar ball: yellow, with a wide blue band around its equator and a
 * big red five-pointed star centered on each pole. The axis and the star's
 * orientation come in as parameters (world directions; the ball is only
 * translated, so Po is a world direction from its center). */
typedef struct {
    double axis[3]; /* through the poles */
    double e1[3];   /* in the equator's plane: where one star point aims */
    double e2[3];   /* axis x e1 */
    double radius;
} BallParams;

void Ball(RtShaderContext *c)
{
    const BallParams *b = (const BallParams *)c->params;
    double p[3] = {c->Po[0], c->Po[1], c->Po[2]};
    double l = std::sqrt(p[0] * p[0] + p[1] * p[1] + p[2] * p[2]);
    for (double &a : p)
        a /= l;

    /* edges a pixel wide (in radians on the ball), so they don't alias */
    double dist = std::sqrt(c->P[0] * c->P[0] + c->P[1] * c->P[1] + c->P[2] * c->P[2]);
    double w = 0.7 * dist * 0.00115 / b->radius;

    const double yellow[3] = {0.98, 0.80, 0.07}, blue[3] = {0.10, 0.24, 0.68}, red[3] = {0.90, 0.17, 0.10};
    double h = p[0] * b->axis[0] + p[1] * b->axis[1] + p[2] * b->axis[2]; /* sin(latitude) */
    double lat = std::asin(std::max(-1.0, std::min(1.0, h)));
    const double BAND = 23 * PI / 180; /* half-width of the band */
    double band = 1 - Smooth(BAND - w, BAND + w, std::fabs(lat));

    /* the star around the nearer pole, in polar coordinates there: radius
     * = angle from the pole, angle = azimuth from e1 */
    double r = PI / 2 - std::fabs(lat);
    double th = std::atan2(p[0] * b->e2[0] + p[1] * b->e2[1] + p[2] * b->e2[2],
                           p[0] * b->e1[0] + p[1] * b->e1[1] + p[2] * b->e1[2]);
    /* fold into half of one point, 0..36 degrees from a tip */
    const double sector = 2 * PI / 5;
    double phi = std::fmod(th + 10 * PI + sector / 2, sector) - sector / 2;
    phi = std::fabs(phi);
    const double OUTER = 38 * PI / 180, INNER = 0.43 * OUTER;
    double tx = OUTER, ty = 0, ix = INNER * std::cos(sector / 2), iy = INNER * std::sin(sector / 2);
    double px = r * std::cos(phi), py = r * std::sin(phi);
    /* signed distance to the edge from the tip to the inner corner,
     * positive on the center's side */
    double ex = ix - tx, ey = iy - ty;
    double sd = (ex * (py - ty) - ey * (px - tx)) / std::sqrt(ex * ex + ey * ey);
    double star = Smooth(-w, w, sd);

    for (int i = 0; i < 3; ++i)
        c->Cs[i] = (RtFloat)Mix(Mix(yellow[i], blue[i], band), red[i], star);
    Lit(c);
}

/* Soft shadow on the floor: black, opacity from `dark` in the middle
 * (s = 0) to nothing at the rim (s = 1). */
void SoftShadow(RtShaderContext *c)
{
    const double *dark = (const double *)c->params;
    double o = *dark * (1 - Smooth(0.25, 1.0, c->u));
    c->Ci[0] = c->Ci[1] = c->Ci[2] = 0;
    c->Oi[0] = c->Oi[1] = c->Oi[2] = (RtFloat)o;
}

/* An ellipse just above the floor at (x, z), turned by angle about y. */
void Shadow(double x, double z, double rx, double rz, double angle, double dark)
{
    enum { NS = 48 };
    RtPoint P[NS + 1];
    RtFloat st[2 * (NS + 1)];
    RtInt nverts[NS], verts[3 * NS];
    double ca = std::cos(angle * PI / 180), sa = std::sin(angle * PI / 180);
    P[0][0] = (RtFloat)x, P[0][1] = 0.01f, P[0][2] = (RtFloat)z;
    st[0] = st[1] = 0;
    for (int i = 0; i < NS; ++i) {
        double t = 2 * PI * i / NS, lx = rx * std::cos(t), lz = rz * std::sin(t);
        P[i + 1][0] = (RtFloat)(x + ca * lx + sa * lz);
        P[i + 1][1] = 0.01f;
        P[i + 1][2] = (RtFloat)(z - sa * lx + ca * lz);
        st[2 * (i + 1)] = 1;
        st[2 * (i + 1) + 1] = 0;
        nverts[i] = 3;
        verts[3 * i] = 0, verts[3 * i + 1] = i + 1, verts[3 * i + 2] = (i + 1) % NS + 1;
    }
    RiSurface(SoftShadow, &dark, sizeof dark);
    RiPointsPolygons(NS, nverts, verts, P, NULL, st);
}

/* A soft shadow around a block's square footprint: darkest at the block's
 * edge, fading out over `reach` beyond it. The quad is laid out in the
 * block's own frame, so Po's x and z are measured from its center. */
typedef struct {
    double half, reach, dark;
} BoxShadowParams;

void BoxShadowShader(RtShaderContext *c)
{
    const BoxShadowParams *p = (const BoxShadowParams *)c->params;
    double dx = std::max(std::fabs(c->Po[0]) - p->half, 0.0);
    double dz = std::max(std::fabs(c->Po[2]) - p->half, 0.0);
    double o = p->dark * (1 - Smooth(0, p->reach, std::sqrt(dx * dx + dz * dz)));
    c->Ci[0] = c->Ci[1] = c->Ci[2] = 0;
    c->Oi[0] = c->Oi[1] = c->Oi[2] = (RtFloat)o;
}

void BoxShadow(double x, double z, double size, double angle, double reach, double dark)
{
    BoxShadowParams bp = {size / 2, reach, dark};
    RtFloat e = (RtFloat)(size / 2 + reach);
    RtPoint P[4] = {{-e, 0, -e}, {e, 0, -e}, {e, 0, e}, {-e, 0, e}};
    RtInt nverts[1] = {4}, verts[4] = {0, 1, 2, 3};
    RiTransformBegin();
        RiTranslate((RtFloat)x, 0.01f, (RtFloat)z);
        RiRotate((RtFloat)angle, 0, 1, 0);
        RiSurface(BoxShadowShader, &bp, sizeof bp);
        RiPointsPolygons(1, nverts, verts, P, NULL, NULL);
    RiTransformEnd();
}

/* -------------------------------------------------------------- Buzz */

/* Glass: nearly clear face-on, reflective at grazing angles (Schlick's
 * Fresnel), with the specular highlights of the standard lights and the
 * model's force-field texture looked up by the camera-space normal, so its
 * bright rim lands on the silhouette. Ci is written straight: the hider
 * multiplies by Oi, so Ci = (what the glass adds) / Oi. */
void HelmetGlass(RtShaderContext *c)
{
    double N[3] = {c->N[0], c->N[1], c->N[2]};
    double ln = std::sqrt(N[0] * N[0] + N[1] * N[1] + N[2] * N[2]);
    double fresnel = Fresnel(c, 0.04);

    /* STANDARD on a black surface leaves only the specular highlights */
    c->Cs[0] = c->Cs[1] = c->Cs[2] = 0;
    Lit(c);
    RtColor field;
    RiTexture(1, (RtFloat)(0.5 + 0.48 * N[0] / ln), (RtFloat)(0.5 - 0.48 * N[1] / ln), field);

    const double tint[3] = {0.75, 0.88, 1.0};
    double alpha = std::min(1.0, 0.07 + 0.75 * fresnel);
    for (int i = 0; i < 3; ++i) {
        double add = c->Ci[i] * 1.6 + 0.45 * field[i] * tint[i] + 0.5 * fresnel * tint[i];
        c->Ci[i] = (RtFloat)(add / alpha);
        c->Oi[i] = (RtFloat)alpha;
    }
}

void BuzzModel(void)
{
    RtColor white = {1, 1, 1}, opaque = {1, 1, 1};
    ObjMesh *buzz = obj_load("models/buzz/buzz.obj");
    TextureParams tp = {0, &KEY_SHADOW};
    for (int k = 0; buzz && k < buzz->ngroups; ++k) {
        const ObjGroup &g = buzz->groups[k];
        if (std::strcmp(g.material, "dome") == 0) {
            RiSurface(HelmetGlass, NULL, 0);
            BuzzLenses(g);
            BuzzHelmet();
        } else {
            RiColor(white);
            RiOpacity(opaque);
            RiSurface(TexturedStandard, &tp, sizeof tp);
            RiPointsPolygons(g.npolys, g.nverts, g.verts, g.P, g.N, g.st);
        }
    }
    obj_free(buzz);
    RiOpacity(opaque);
}

/* ------------------------------------------------------------- Woody */

/* The hat's braided trim: a cutout, brown where its mask (texture slot 7)
 * is white and see-through elsewhere. */
void HatTrim(RtShaderContext *c)
{
    RtColor mask;
    RiTexture(7, c->u, c->v, mask);
    c->Cs[0] = 0.62f, c->Cs[1] = 0.33f, c->Cs[2] = 0.15f;
    Lit(c);
    c->Oi[0] = c->Oi[1] = c->Oi[2] = mask[0] > 0.5f ? 1.0f : 0.0f;
}

/* Woody from models/woody/. Each group is textured from its slot; the
 * buckle and the small straps get plain colors, which keeps the scene
 * within the renderer's 8 texture slots. */
void WoodyModel(void)
{
    struct { const char *material; int slot; RtFloat color[3]; } looks[] = {
        {"head", 2, {1, 1, 1}},        {"clothes", 3, {1, 1, 1}},       {"leather", 4, {1, 1, 1}},
        {"bandana", 5, {1, 1, 1}},     {"eyes", 6, {1, 1, 1}},          {"buckle", -1, {0.665f, 0.309f, 0.109f}},
        {"buckle_detail", -1, {0.80f, 0.62f, 0.25f}}, {"straps", -1, {0.45f, 0.25f, 0.12f}},
    };
    RtColor opaque = {1, 1, 1};
    RiOpacity(opaque);
    ObjMesh *woody = obj_load("models/woody/woody.obj");
    for (int k = 0; woody && k < woody->ngroups; ++k) {
        const ObjGroup &g = woody->groups[k];
        if (std::strcmp(g.material, "band") == 0) {
            RiSurface(HatTrim, NULL, 0);
        } else {
            for (const auto &look : looks)
                if (std::strcmp(g.material, look.material) == 0) {
                    RtColor color = {look.color[0], look.color[1], look.color[2]};
                    RiColor(color);
                    if (look.slot >= 0) {
                        TextureParams tp = {look.slot, &KEY_SHADOW};
                        RiSurface(TexturedStandard, &tp, sizeof tp);
                    } else {
                        RiSurface(NULL, NULL, 0);
                    }
                }
        }
        RiPointsPolygons(g.npolys, g.nverts, g.verts, g.P, g.N, g.st);
    }
    obj_free(woody);
}

/* The toys, which cast the shadows. The room only receives them: the key
 * light is outside the room, beyond the left wall's plane, so the walls
 * stay out of the depth pass (and the floor and walls never test against
 * their own depth). */
void Toys(void)
{
    RtColor opaque = {1, 1, 1};
    RiOpacity(opaque);

    /* alphabet blocks: a stack of two and one on its own */
    const int b1[6] = {0, 1, 2, 3, 1, 0}, b2[6] = {1, 3, 0, 2, 2, 1}, b3[6] = {2, 0, 3, 1, 0, 3};
    Block(-3.4, 0, 3.6, 1.3, 20, b1);
    Block(-3.3, 1.3, 3.55, 1.3, -8, b2);
    Block(-1.6, 0, 4.6, 1.3, 40, b3);

    /* the ball, its star toward the camera and tilted up, as in the classic
     * picture of it */
    {
        const double C[3] = {3.1, 0.85, 3.8};
        double toEye[3], right[3], up[3];
        for (int i = 0; i < 3; ++i) {
            toEye[i] = EYE[i] - C[i];
            right[i] = CAMERA[i][0]; /* the camera's x and y axes in world space */
            up[i] = CAMERA[i][1];
        }
        double le = std::sqrt(toEye[0] * toEye[0] + toEye[1] * toEye[1] + toEye[2] * toEye[2]);
        BallParams bp;
        double la = 0;
        for (int i = 0; i < 3; ++i) {
            toEye[i] /= le;
            bp.axis[i] = 0.62 * up[i] + 0.69 * toEye[i] + 0.25 * right[i];
            la += bp.axis[i] * bp.axis[i];
        }
        la = std::sqrt(la);
        double ta = 0, l1 = 0;
        for (int i = 0; i < 3; ++i) {
            bp.axis[i] /= la;
            ta += toEye[i] * bp.axis[i];
        }
        for (int i = 0; i < 3; ++i) { /* one star point aims at the camera */
            bp.e1[i] = toEye[i] - ta * bp.axis[i];
            l1 += bp.e1[i] * bp.e1[i];
        }
        for (int i = 0; i < 3; ++i)
            bp.e1[i] /= std::sqrt(l1);
        bp.e2[0] = bp.axis[1] * bp.e1[2] - bp.axis[2] * bp.e1[1];
        bp.e2[1] = bp.axis[2] * bp.e1[0] - bp.axis[0] * bp.e1[2];
        bp.e2[2] = bp.axis[0] * bp.e1[1] - bp.axis[1] * bp.e1[0];
        bp.radius = C[1];
        RiSurface(Ball, &bp, sizeof bp);
        RiTransformBegin();
            RiTranslate((RtFloat)C[0], (RtFloat)C[1], (RtFloat)C[2]);
            RiSphere((RtFloat)C[1], (RtFloat)-C[1], (RtFloat)C[1], 360);
        RiTransformEnd();
    }

    /* Woody, standing to the left of Buzz, turned a little toward him */
    RiTransformBegin();
        RiTranslate(-1.7f, 0, 1.0f);
        RiRotate(-20, 0, 1, 0);
        WoodyModel();
    RiTransformEnd();

    /* Buzz, flying toward the camera and to the left */
    RiTransformBegin();
        RiTranslate(0.1f, 1.25f, 0.2f);
        RiRotate(15, 0, 1, 0);   /* heading a little left */
        RiRotate(-8, 0, 0, 1);   /* banking */
        RiRotate(-22, 1, 0, 0);  /* leaning into the flight */
        BuzzModel();
    RiTransformEnd();
}

/* Soft contact shadows under the toys, darkening the floor where it
 * meets them (the shadow map gives the cast shadows). */
void ContactShadows(void)
{
    BoxShadow(-3.4, 3.6, 1.3, 20, 0.5, 0.6);
    BoxShadow(-1.6, 4.6, 1.3, 40, 0.5, 0.6);
    Shadow(3.1, 3.8, 0.95, 0.95, 0, 0.6);
    Shadow(-1.7, 1.0, 0.55, 0.45, -20, 0.55);
}

} // namespace

void AndysRoom(void)
{
    float fov = 45.0f, lightFov = 32.0f;

    LookAt(EYE, TARGET, CAMERA, KEY_SHADOW.cameraToWorld);

    /* STANDARD's key light is fixed in camera space, so in the world it
     * moves with the camera. The shadow camera sits there and looks at the
     * toys. */
    double key[3];
    StandardLightPosition(KEY_SHADOW.cameraToWorld, 0, key);
    const double toys[3] = {-0.4, 1.2, 2.4};
    RtMatrix lightCamera;
    LookAt(key, toys, lightCamera);
    KEY_SHADOW.light = 0;
    KEY_SHADOW.map = NULL;

    RiBegin(RI_NULL);
        RiMakeTexture("models/buzz/Buzz_Diffuse.png", 0);
        RiMakeTexture("models/buzz/buzz_forcefeild.png", 1);
        RiMakeTexture("models/woody/woody_head.jpg", 2);
        RiMakeTexture("models/woody/woody_clothes.jpg", 3);
        RiMakeTexture("models/woody/woody_leather.jpg", 4);
        RiMakeTexture("models/woody/woody_bandana.jpg", 5);
        RiMakeTexture("models/woody/woody_eyes.png", 6);
        RiMakeTexture("models/woody/woody_band_mask.png", 7);

        /* pass 1: the toys' depth, seen from the key light */
        RiFrameBegin(0);
            RiFormat(1024, 1024);
            RiPixelSamples(1, 1);
            RiDisplay("3_CinematicScenes/AndysRoom_key.z", "zfile", "z");
            RiProjection(RI_PERSPECTIVE, "fov", &lightFov);
            RiConcatTransform(lightCamera);
            RiWorldBegin();
                Toys();
            RiWorldEnd();
        RiFrameEnd();

        /* pass 2: the picture, with the key light shadowed */
        ZFile *map = zfile_load("3_CinematicScenes/AndysRoom_key.z", lightCamera, lightFov);
        KEY_SHADOW.map = map;
        RiFrameBegin(1);
            RiFormat(960, 720);
            RiPixelSamples(4, 4);
            RiDisplay("3_CinematicScenes/AndysRoom.png", "", "");
            RiProjection(RI_PERSPECTIVE, "fov", &fov);
            RiConcatTransform(CAMERA);
            RiWorldBegin();
                Room();
                Toys();
                ContactShadows();
            RiWorldEnd();
        RiFrameEnd();
        KEY_SHADOW.map = NULL;
        zfile_free(map);
    RiEnd();
}
