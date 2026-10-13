/*
 * scenes.h: the table of scenes the executable can render, by name.
 *
 * The scene functions are C++ functions in the scene files, so a renderer in
 * any language finds them through this one C-linkage table, defined in
 * scenes/0_scene_table.cpp.
 */
#ifndef SCENES_H
#define SCENES_H

#include <stddef.h> /* NULL */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct RtSceneEntry {
    const char *name;       /* what you type: e.g., reyes QuadricZoo */
    void (*function)(void); /* the scene's entry point (function pointer) */
    const char *tier;       /* "test", "intermediate" or "cinematic": which of
                               the three folders of scenes/ and renders/ it is in */
} RtSceneEntry;

/* {name, function, tier} entries, ending with {NULL, NULL, NULL} */
extern const RtSceneEntry RI_SCENES[];

#ifdef __cplusplus
}
#endif

#endif /* SCENES_H */
