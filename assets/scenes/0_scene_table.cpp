/*
 * 0_scene_table.cpp: every scene the reyes executable can render, by name
 * and tier, in order: the test scenes (1_TestScenes.cpp, in the order the
 * Suggested Workflow needs them), then 2_IntermediateScenes/, then
 * 3_CinematicScenes/.
 *
 * Not a scene file. The scene entry points are C++ functions, so this table
 * is how main.cpp (or the Rust main.rs) reaches them. To add a scene, declare
 * its function here and add one line to RI_SCENES.
 */
#include "scenes.h"

/* 1_TestScenes.cpp */
void FirstPrimitive(const char *primitive); /* "sphere", "cylinder", "cone" or "torus" */
void OrthoVsPersp(int perspective);
void SimpleAnimation(int frame); /* one of 100 frames, or all if negative, into renders/1_TestScenes/SimpleAnimation/ */
void AliasingTest(int samples);
void DiceRateTest(void);
void QuadricZoo(void);
void Tunnel(void);
void ShaderTest(void);
void UVTest(void);
void Earth(void);
void DisplacementTest(void);
void BumpVsDisplacement(void);
void Tornado(void);
void TransparencyTest(void);
void TransparencyOrder(void);
void PolygonTest(void);
void ShadowTest(void);
void NormalTest(void);
void CustomScene(void);

/* 2_IntermediateScenes/ */
void Rocket(void);
void Explorer(void);
void McQueen(void);
void Buzz(void);
void WallE(void);
void Gumbo(void);

/* 3_CinematicScenes/ */
void RadiatorSprings(void);
void AndysRoom(void);
void WallEMovie(void); /* 72 frames, into renders/3_CinematicScenes/WallEMovie/ */

/* wrappers for scenes that take an argument */
static void FirstPrimitive_sphere(void) { FirstPrimitive("sphere"); }
static void FirstPrimitive_cylinder(void) { FirstPrimitive("cylinder"); }
static void FirstPrimitive_cone(void) { FirstPrimitive("cone"); }
static void FirstPrimitive_torus(void) { FirstPrimitive("torus"); }
static void SimpleAnimation_middle(void) { SimpleAnimation(75); } /* the ball in the middle */
static void SimpleAnimation_movie(void) { SimpleAnimation(-1); }
static void OrthoVsPersp_ortho(void) { OrthoVsPersp(0); }
static void OrthoVsPersp_persp(void) { OrthoVsPersp(1); }
static void AliasingTest_1x1(void) { AliasingTest(1); }
static void AliasingTest_4x4(void) { AliasingTest(4); }

extern "C" const RtSceneEntry RI_SCENES[] = {
    /* 1_TestScenes.cpp */
    {"FirstPrimitive_sphere", FirstPrimitive_sphere, "test"},
    {"FirstPrimitive_cylinder", FirstPrimitive_cylinder, "test"},
    {"FirstPrimitive_cone", FirstPrimitive_cone, "test"},
    {"FirstPrimitive_torus", FirstPrimitive_torus, "test"},
    {"OrthoVsPersp_ortho", OrthoVsPersp_ortho, "test"},
    {"OrthoVsPersp_persp", OrthoVsPersp_persp, "test"},
    {"SimpleAnimation", SimpleAnimation_middle, "test"},
    {"SimpleAnimationMovie", SimpleAnimation_movie, "test"},
    {"AliasingTest_1x1", AliasingTest_1x1, "test"},
    {"AliasingTest_4x4", AliasingTest_4x4, "test"},
    {"DiceRateTest", DiceRateTest, "test"},
    {"QuadricZoo", QuadricZoo, "test"},
    {"Tunnel", Tunnel, "test"},
    {"ShaderTest", ShaderTest, "test"},
    {"UVTest", UVTest, "test"},
    {"Earth", Earth, "test"},
    {"DisplacementTest", DisplacementTest, "test"},
    {"BumpVsDisplacement", BumpVsDisplacement, "test"},
    {"Tornado", Tornado, "test"},
    {"TransparencyTest", TransparencyTest, "test"},
    {"TransparencyOrder", TransparencyOrder, "test"},
    {"PolygonTest", PolygonTest, "test"},
    {"ShadowTest", ShadowTest, "test"},
    /* 2_IntermediateScenes/ */
    {"Rocket", Rocket, "intermediate"},
    {"Explorer", Explorer, "intermediate"},
    {"McQueen", McQueen, "intermediate"},
    {"Buzz", Buzz, "intermediate"},
    {"WallE", WallE, "intermediate"},
    {"Gumbo", Gumbo, "intermediate"},
    /* 3_CinematicScenes/ */
    {"RadiatorSprings", RadiatorSprings, "cinematic"},
    {"AndysRoom", AndysRoom, "cinematic"},
    {"WallEMovie", WallEMovie, "cinematic"},
    {"NormalTest", NormalTest, "test"},
    {"Custom", CustomScene, ""},
    {NULL, NULL, NULL}
};
