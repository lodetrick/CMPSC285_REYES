NormalTest: a debugging scene for the shading normal
ECE/CS 285, HW1 (the REYES renderer)

WHAT THIS IS

Several of you see small differences from the reference renders after
transforming the surface normals to camera space and turning them to face
the eye. This scene draws three simple quadrics four times, and each
picture shows one step of that work, so you can see which step differs
instead of guessing from the lit picture.

The four PNGs in this folder are what the reference renderer draws for
this scene. They are the correct result: your renderer should reproduce
them. (Pixels right on a silhouette may differ a little, since sample
positions are jittered; everything inside an object should match.)

FILES

  NormalTest.cpp            the scene, a C++ scene file like the kit's
  NormalTest_original.png   reference: the normals before the turn toward
                            the eye
  NormalTest_flipped.png    reference: the normals as the surface shader
                            receives them
  NormalTest_sides.png      reference: which side of the surface N is on
  NormalTest_shaded.png     reference: the same objects lit by STANDARD
  README.txt                this file

HOW TO RUN IT

  1. Copy NormalTest.cpp into scenes/ in your kit.
  2. C++ or C: add scenes/NormalTest.cpp to SCENE_SOURCES in
     CMakeLists.txt. Rust: nothing to do, build.rs compiles every .cpp
     in scenes/.
  3. In scenes/0_scene_table.cpp, declare the scene next to the others,
         void NormalTest(void);
     and add one line to RI_SCENES, before the {NULL, NULL, NULL} entry:
         {"NormalTest", NormalTest, "test"},
  4. Rebuild and run from the kit root (the folder with scenes/ in it):
         ./build/reyes NormalTest            (.\build\Release\reyes.exe
                                              NormalTest on Windows)
         cd rust && cargo run --release -- NormalTest
     Your pictures land in renders/1_TestScenes/NormalTest_*.png. Compare
     each one with the PNG of the same name in this folder.

The scene needs only what steps 14 and 15 of the Suggested Workflow
build: the quadrics, the transform stack, RiSurface with a parameter
block, the normal N in the shader context, and STANDARD. No textures, no
displacement, no transparency. It writes no depth file.

THE SCENE

Three objects, left to right. Each one catches one mistake:

  - An open tube (a cylinder), looked into from above its near end so
    that its floor shows through the opening. The floor is the inside of
    the surface: its normals point away from the eye and must be turned
    around before STANDARD lights it.
  - A sphere squashed to half its height with RiScale, then rotated. Its
    normals come out right only through the inverse transpose of the
    modelview matrix; the matrix itself bends them off the surface.
  - A whole sphere near the edge of the frame. "Facing the eye" means
    N . (-P) >= 0, where -P is the direction from the point to the eye
    (the eye sits at the origin of camera space). It does not mean
    N.z <= 0, the direction down the view axis. Near the middle of the
    picture the two tests agree; away from it they do not, and a renderer
    that tests N.z turns some normals the wrong way: along the edge of
    this sphere nearest the middle, and over part of the tube.

THE PICTURES

The first three pictures draw the camera-space normal as a color,
(N + 1) / 2: x to red, y to green, z to blue. So a normal pointing
straight at the camera, (0, 0, -1), is dark yellow (0.5, 0.5, 0); one
pointing away from it is light blue (0.5, 0.5, 1); +x is pink, +y light
green, -y purple.

NormalTest_original.png
  N before the renderer turned it toward the eye. A surface shader is
  only ever handed the turned normal, so the scene's shader turns it
  back: the scene knows which way each object's outside points (it placed
  them), computes that direction from Po, and puts your N on that side.
  So this picture depends on how you transform normals, and not at all
  on how you turn them. Correct: the colors change smoothly over every
  object, and the tube's floor, seen through the opening, is blue and
  purple (its normals point down and away from the eye).

  To see your own untouched normals instead, temporarily disable the
  turn in your renderer and render again: that build's
  NormalTest_flipped.png must then equal NormalTest_original.png.

NormalTest_flipped.png
  N exactly as your surface shader receives it. Correct: the same as
  original, except that the tube's floor is now light green and pink
  (turned up, toward the eye). The left edge of the right sphere shades
  toward blue in both pictures, and that is right: those normals have
  N.z > 0 yet face the eye, which is off to their left.

NormalTest_sides.png
  Which side of the surface your N is on. Green: the outside. Red: the
  inside. Blue mixed in: neither, so N is off the surface's normal line
  (half blue for 5 degrees off, full blue beyond about 12). Correct: all
  green except the tube's floor, which is red, and no blue anywhere.

NormalTest_shaded.png
  The same objects lit by STANDARD. Correct: the tube's floor is lit,
  pale green like its outside; the squashed sphere has one highlight at
  its upper left; the right sphere is lit from the upper left with a cool
  fill on its right.

WHAT A WRONG PICTURE MEANS

Each of these was checked by putting that single bug into the reference
renderer and rendering the scene again.

  - original differs on the squashed sphere only: its colors shift and a
    rim of another color runs along its edge; sides shows it cyan with a
    magenta rim; shaded moves its highlight and darkens its lower left
    edge. Your normals go through the modelview matrix instead of its
    inverse transpose. If only the colors on that sphere are too strong
    or too pale, the normals are not renormalized after the transform.
    If original is wrong on all three objects, the normals are not
    transformed at all, or are transformed as points (the translation
    gets added in).

  - flipped is the same picture as original, sides is all green, and
    shaded shows the tube's floor dark. N is never turned toward the eye.

  - flipped shows the tube's floor split by a sharp line, blue and purple
    on the left and light green on the right; sides shows a green wedge
    there, a red strip down the tube's right outside wall and a red line
    along the left edge of each sphere; shaded has a dark wedge on the
    tube's floor. Your test is N.z, not N . (-P). This bug only shows
    away from the middle of the picture, on surfaces seen at a grazing
    angle, which is why it looks like a small difference in most scenes.

  - original, flipped and sides all match the reference and shaded does
    not. The bug is inside STANDARD, not in the normals. Check that at
    every grid vertex V = normalize(-P), L = normalize(light - P) for
    each light, R = 2 (N . L) N - L with N, L and V unit vectors, and
    that the three lights' positions, colors and intensities are the
    handout's.

Everything here is in camera space: the eye at the origin, looking down
+z, +y up, as in the handout. The lights are fixed in camera space too.
