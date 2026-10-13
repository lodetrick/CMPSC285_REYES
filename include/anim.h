/*
 * anim.h: rigid-part animation for scene files (Wall-E's walle_anim.txt).
 *
 * The file gives, for every frame and every part of a model, the 3 x 4
 * matrix that moves the part from its pose in the model's OBJ file to its
 * pose in that frame. A scene draws each OBJ group (a part) inside
 * RiTransformBegin / RiConcatTransform(matrix) / RiTransformEnd. Like obj.h,
 * the renderer never calls this file.
 *
 * Format (text): '#' comment lines, then
 *   frames F parts P fps R
 *   frame 1
 *   <part name> m00 m01 m02 m03 m10 m11 m12 m13 m20 m21 m22 m23
 *   ... (P lines)
 *   frame 2
 *   ...
 * The matrices are row-major for column vectors: p' = M (p, 1).
 */
#ifndef ANIM_H
#define ANIM_H

#include "Ri.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

struct RigidAnim {
    int frames = 0, fps = 24;
    std::vector<std::string> parts;
    std::map<std::string, int> index;   /* part name -> position in parts */
    std::vector<float> m;               /* frames x parts x 12 */
};

/* NULL plus a message on stderr on failure. */
inline RigidAnim *anim_load(const char *path)
{
    std::ifstream in(path);
    if (!in) {
        std::fprintf(stderr, "anim_load: cannot open %s (run from the repo root)\n", path);
        return NULL;
    }
    RigidAnim *a = new RigidAnim;
    std::string line, word;
    int nparts = 0, frame = -1, k = 0;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#')
            continue;
        std::istringstream ss(line);
        ss >> word;
        if (word == "frames") {
            std::string w2, w3;
            ss >> a->frames >> w2 >> nparts >> w3 >> a->fps;
            a->m.assign((size_t)a->frames * nparts * 12, 0.0f);
        } else if (word == "frame") {
            ss >> frame;
            --frame;
            k = 0;
        } else if (frame >= 0 && frame < a->frames && k < nparts) {
            if (frame == 0) {
                a->index[word] = (int)a->parts.size();
                a->parts.push_back(word);
            }
            float *dst = &a->m[((size_t)frame * nparts + k) * 12];
            for (int i = 0; i < 12; ++i)
                ss >> dst[i];
            ++k;
        }
    }
    if (a->frames <= 0 || (int)a->parts.size() != nparts) {
        std::fprintf(stderr, "anim_load: %s is not a rigid animation file\n", path);
        delete a;
        return NULL;
    }
    return a;
}

inline void anim_free(RigidAnim *a) { delete a; }

/* The part's matrix at frame f (1-based, clamped), as an RtMatrix for
 * RiConcatTransform (row vectors, translation in the bottom row). Returns
 * false, with the identity, if the part has no animation. */
inline bool anim_matrix(const RigidAnim *a, int f, const char *part, RtMatrix out)
{
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            out[i][j] = i == j ? 1.0f : 0.0f;
    auto it = a ? a->index.find(part) : std::map<std::string, int>::const_iterator();
    if (!a || it == a->index.end())
        return false;
    f = f < 1 ? 1 : (f > a->frames ? a->frames : f);
    const float *m = &a->m[((size_t)(f - 1) * a->parts.size() + it->second) * 12];
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c)
            out[c][r] = m[4 * r + c]; /* transpose: RenderMan uses row vectors */
        out[3][r] = m[4 * r + 3];
    }
    return true;
}

#endif /* ANIM_H */
