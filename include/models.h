/*
 * models.h: drawing the provided models that more than one scene uses
 * (Buzz's helmet, Wall-E). Like obj.h, the renderer never calls this file;
 * it is C++ because the scene files always are.
 */
#ifndef MODELS_H
#define MODELS_H

#include "Ri.h"
#include "anim.h"
#include "obj.h"
#include "sceneutil.h"

#include <cmath>
#include <cstring>

/* ---------------------------------------------------------------- Buzz */

/* Buzz's helmet bubble was modeled with flat facets, in the Dome group with
 * the two wrist lenses. The scenes draw the bubble as what it is, a
 * transparent RiSphere fitted to the facets (within about 1 cm), and keep
 * only the lenses from the group. Bind the glass shader, then call both. */
inline void BuzzHelmet(void)
{
    const RtFloat CENTER_Y = 1.48f, RADIUS = 0.30f, BOTTOM = -0.168f; /* cut where it meets the collar */
    RiTransformBegin();
        RiTranslate(0, CENTER_Y, 0);
        RiRotate(-90, 1, 0, 0); /* the sphere's z axis up */
        RiSphere(RADIUS, BOTTOM, RADIUS, 360);
    RiTransformEnd();
}

/* The Dome group's polygons that are not helmet facets: the wrist lenses,
 * the triangles whose center is well away from the head. */
inline void BuzzLenses(const ObjGroup &dome)
{
    RtInt *nverts = new RtInt[dome.npolys], *verts = new RtInt[3 * dome.npolys];
    int npolys = 0, nindices = 0;
    const RtInt *v = dome.verts;
    for (int i = 0; i < dome.npolys; v += dome.nverts[i], ++i) {
        double x = 0;
        for (int k = 0; k < dome.nverts[i]; ++k)
            x += dome.P[v[k]][0] / dome.nverts[i];
        if (std::fabs(x) < 0.6 || dome.nverts[i] != 3)
            continue; /* a helmet facet: the sphere replaces it */
        nverts[npolys++] = 3;
        for (int k = 0; k < 3; ++k)
            verts[nindices++] = v[k];
    }
    RiPointsPolygons(npolys, nverts, verts, dome.P, dome.N, dome.st);
    delete[] nverts;
    delete[] verts;
}

/* -------------------------------------------------------------- Wall-E */

/* Wall-E's paint: the color texture, darkened by the baked ambient
 * occlusion texture, then STANDARD (through the shadow map, if any). The
 * params are a TextureParams whose slot holds the color texture, with the
 * occlusion texture in the next slot. */
inline void WallEPaint(RtShaderContext *c)
{
    const TextureParams *p = (const TextureParams *)c->params;
    RtColor occlusion;
    RiTexture(p->slot, c->u, c->v, c->Cs);
    RiTexture(p->slot + 1, c->u, c->v, occlusion);
    for (int i = 0; i < 3; ++i)
        c->Cs[i] *= 0.35f + 0.65f * occlusion[0];
    ShadowedStandard(c, p->shadow);
}

/* Wall-E at one frame of his animation: every part (an OBJ group) moved by
 * its matrix for that frame, from anim.h. His color texture must be in
 * slot colorSlot and his occlusion texture in the slot after it. shadow is
 * NULL for no shadow map. */
inline void DrawWallE(const ObjMesh *walle, const RigidAnim *anim, int frame, RtInt colorSlot,
                      const ShadowParams *shadow)
{
    RtColor white = {1, 1, 1}, lens = {0.04f, 0.04f, 0.05f};
    TextureParams paint = {colorSlot, shadow}, plain = {-1, shadow};
    for (int k = 0; walle && k < walle->ngroups; ++k) {
        const ObjGroup &g = walle->groups[k];
        if (std::strcmp(g.material, "eyes") == 0) {
            RiColor(lens);
            RiSurface(TexturedStandard, &plain, sizeof plain);
        } else {
            RiColor(white);
            RiSurface(WallEPaint, &paint, sizeof paint);
        }
        RtMatrix m;
        anim_matrix(anim, frame, g.name, m);
        RiTransformBegin();
            RiConcatTransform(m);
            RiPointsPolygons(g.npolys, g.nverts, g.verts, g.P, g.N, g.st);
        RiTransformEnd();
    }
}

#endif /* MODELS_H */
