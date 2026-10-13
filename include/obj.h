/*
 * obj.h: a small Wavefront OBJ loader for scene files.
 *
 * Scene files load a mesh with obj_load() and hand each group to
 * RiPointsPolygons(). The renderer never calls this file, so it is not part
 * of the Ri interface; it is C++ because the scene files always are.
 *
 * It reads v, vt, vn, f (three or more corners, written v, v/vt, v//vn or
 * v/vt/vn, negative indices allowed), g and usemtl, and ignores the rest
 * (mtllib, o, s, comments).
 *
 * Every distinct (v, vt, vn) triple becomes one output vertex ("unwelding"),
 * so each group has parallel P, N and st arrays and one index per corner,
 * which is what RiPointsPolygons takes. A new group starts at every g or
 * usemtl line; empty groups are dropped.
 *
 * OBJ puts texture coordinate t = 0 at the bottom of the image; RiTexture,
 * like RenderMan, puts it at the top. So st holds (s, 1 - t), and textures
 * come out the right way up with no change to the file.
 */
#ifndef OBJ_H
#define OBJ_H

#include "Ri.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <string>
#include <tuple>
#include <vector>

typedef struct {
    char name[64];     /* from g */
    char material[64]; /* from usemtl, "" if none */
    RtInt npolys;
    RtInt *nverts;     /* npolys entries */
    RtInt *verts;      /* sum(nverts) entries */
    RtInt nvertices;
    RtPoint *P;
    RtNormal *N;       /* NULL unless every corner in the group has a vn */
    RtFloat *st;       /* 2 * nvertices, NULL unless every corner has a vt */
} ObjGroup;

typedef struct {
    int ngroups;
    ObjGroup *groups;
} ObjMesh;

namespace obj_detail {

struct Vec {
    float x, y, z;
};

/* A group while it is being read. */
struct Builder {
    std::string name, material;
    std::map<std::tuple<int, int, int>, RtInt> index; /* (v, vt, vn) -> output vertex */
    std::vector<Vec> P, N;
    std::vector<float> st;
    std::vector<RtInt> nverts, verts;
    bool allN = true, allST = true;
};

inline void copyName(const std::string &from, char to[64])
{
    size_t n = from.copy(to, 63);
    to[n] = '\0';
}

/* The rest of the line after the keyword, without surrounding blanks. */
inline std::string rest(const std::string &line, size_t from)
{
    size_t b = line.find_first_not_of(" \t", from);
    if (b == std::string::npos)
        return "";
    size_t e = line.find_last_not_of(" \t");
    return line.substr(b, e - b + 1);
}

/* An OBJ index (1-based, or negative counting back from the end) as a
 * 0-based one; -1 if absent or out of range. */
inline int resolve(long i, size_t count)
{
    long r = i > 0 ? i - 1 : (long)count + i;
    return (i != 0 && r >= 0 && r < (long)count) ? (int)r : -1;
}

} // namespace obj_detail

/* NULL plus a message on stderr on failure. */
inline ObjMesh *obj_load(const char *path)
{
    using namespace obj_detail;
    std::ifstream file(path);
    if (!file) {
        std::fprintf(stderr, "obj_load: cannot open %s (run from the repo root)\n", path);
        return NULL;
    }

    std::vector<Vec> v, vn;
    std::vector<float> vt;
    std::vector<Builder> groups(1);
    std::string line, name, material;
    int lineno = 0;

    while (std::getline(file, line)) {
        ++lineno;
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        size_t b = line.find_first_not_of(" \t");
        if (b == std::string::npos || line[b] == '#')
            continue;
        size_t e = line.find_first_of(" \t", b);
        std::string key = line.substr(b, e == std::string::npos ? std::string::npos : e - b);
        const char *p = line.c_str() + (e == std::string::npos ? line.size() : e);
        char *end;

        if (key == "v" || key == "vn") {
            Vec a;
            a.x = std::strtof(p, &end);
            a.y = std::strtof(end, &end);
            a.z = std::strtof(end, &end);
            (key == "v" ? v : vn).push_back(a);
        } else if (key == "vt") {
            float s = std::strtof(p, &end), t = std::strtof(end, &end);
            vt.push_back(s);
            vt.push_back(1 - t); /* OBJ's t runs up the image, ours down */
        } else if (key == "g" || key == "usemtl") {
            (key == "g" ? name : material) = rest(line, e == std::string::npos ? line.size() : e);
            groups.emplace_back();
            groups.back().name = name;
            groups.back().material = material;
        } else if (key == "f") {
            Builder &g = groups.back();
            RtInt corners = 0;
            while (true) {
                /* one corner: v, v/vt, v//vn or v/vt/vn */
                long iv = std::strtol(p, &end, 10), ivt = 0, ivn = 0;
                if (end == p)
                    break; /* no more corners */
                p = end;
                if (*p == '/') {
                    ++p;
                    if (*p != '/') {
                        ivt = std::strtol(p, &end, 10);
                        p = end;
                    }
                    if (*p == '/') {
                        ivn = std::strtol(p + 1, &end, 10);
                        p = end;
                    }
                }
                int a = resolve(iv, v.size()), t = resolve(ivt, vt.size() / 2);
                int n = resolve(ivn, vn.size());
                if (a < 0 || (ivt != 0 && t < 0) || (ivn != 0 && n < 0)) {
                    std::fprintf(stderr, "obj_load: %s:%d: index out of range\n", path, lineno);
                    return NULL;
                }
                g.allST = g.allST && t >= 0;
                g.allN = g.allN && n >= 0;

                auto key3 = std::make_tuple(a, t, n);
                auto found = g.index.find(key3);
                RtInt out;
                if (found != g.index.end()) {
                    out = found->second;
                } else {
                    out = (RtInt)g.P.size();
                    g.index[key3] = out;
                    g.P.push_back(v[a]);
                    g.N.push_back(n >= 0 ? vn[n] : Vec{0, 0, 0});
                    g.st.push_back(t >= 0 ? vt[2 * t] : 0);
                    g.st.push_back(t >= 0 ? vt[2 * t + 1] : 0);
                }
                g.verts.push_back(out);
                ++corners;
            }
            if (corners < 3) {
                std::fprintf(stderr, "obj_load: %s:%d: a face needs 3 corners\n", path, lineno);
                return NULL;
            }
            g.nverts.push_back(corners);
        }
    }

    ObjMesh *mesh = new ObjMesh;
    mesh->ngroups = 0;
    for (const Builder &g : groups)
        mesh->ngroups += !g.nverts.empty();
    mesh->groups = new ObjGroup[mesh->ngroups];

    int k = 0;
    for (const Builder &g : groups) {
        if (g.nverts.empty())
            continue;
        ObjGroup &o = mesh->groups[k++];
        copyName(g.name, o.name);
        copyName(g.material, o.material);
        o.npolys = (RtInt)g.nverts.size();
        o.nverts = new RtInt[g.nverts.size()];
        o.verts = new RtInt[g.verts.size()];
        o.nvertices = (RtInt)g.P.size();
        o.P = new RtPoint[g.P.size()];
        o.N = g.allN ? new RtNormal[g.P.size()] : NULL;
        o.st = g.allST ? new RtFloat[g.st.size()] : NULL;
        for (size_t i = 0; i < g.nverts.size(); ++i)
            o.nverts[i] = g.nverts[i];
        for (size_t i = 0; i < g.verts.size(); ++i)
            o.verts[i] = g.verts[i];
        for (size_t i = 0; i < g.P.size(); ++i) {
            o.P[i][0] = g.P[i].x;
            o.P[i][1] = g.P[i].y;
            o.P[i][2] = g.P[i].z;
            if (o.N) {
                o.N[i][0] = g.N[i].x;
                o.N[i][1] = g.N[i].y;
                o.N[i][2] = g.N[i].z;
            }
        }
        if (o.st)
            for (size_t i = 0; i < g.st.size(); ++i)
                o.st[i] = g.st[i];
    }
    return mesh;
}

inline void obj_free(ObjMesh *m)
{
    if (!m)
        return;
    for (int k = 0; k < m->ngroups; ++k) {
        ObjGroup &g = m->groups[k];
        delete[] g.nverts;
        delete[] g.verts;
        delete[] g.P;
        delete[] g.N;
        delete[] g.st;
    }
    delete[] m->groups;
    delete m;
}

#endif /* OBJ_H */
