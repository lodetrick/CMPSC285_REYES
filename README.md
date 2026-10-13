# ECE/CS 285 HW1: a REYES renderer

This is the starter kit for HW1. It builds and runs as it is: until you
fill in the stubs, every picture is black and fully transparent (in PNG and
TGA; JPEG and BMP carry no alpha) and every depth file is +infinity
throughout. Your job is to replace the stubs in `src/ri.cpp` and
`src/shaders.cpp` with a REYES renderer, following the handout and the
Suggested Workflow. `reference_renders/` has every scene as the reference
renderer draws it: that is what your images should look like.

## Build and run

The renderer may be written in C++, C or Rust. The scene files are C++
either way, and everything they call has C linkage, so all three link
against the same unchanged scenes.

**C++ or C.** You need CMake (3.16 or newer) and a C++17 compiler (gcc,
clang or Visual Studio). A C renderer replaces `src/ri.cpp` and
`src/shaders.cpp` with `.c` files listed in `CMakeLists.txt`; the two
complete functions at the end of `ri.cpp` (`writeImage`, `writeDepth`) are
a few lines to port, and the stb headers are C already.

    cmake -B build
    cmake --build build
    ./build/reyes FirstPrimitive_sphere

`CMakeLists.txt` picks a Release (optimized) build when no build type is
given, so these three lines are all there is on Linux and macOS. Visual
Studio builds every configuration from one project, so on Windows name the
configuration when building and running:

    cmake -B build
    cmake --build build --config Release
    .\build\Release\reyes.exe FirstPrimitive_sphere

Either way, run the executable from this folder, the one that contains
`scenes/`, `textures/` and `renders/`: that is where it looks for textures
and models and where it writes the images.

**Rust.** You need a Rust toolchain (rustup) and a C++17 compiler for the
scene files. `rust/` is a Cargo project: `build.rs` compiles `../scenes`
with the `cc` crate, `src/ri.rs` and `src/shaders.rs` are the stubs
(the same types as `Ri.h` and `shaders.h`, `#[repr(C)]`, and the same
functions exported with `#[no_mangle] extern "C"`), and `src/main.rs` is
the scene picker. The `image` crate stands in for the stb headers.

    cd rust
    cargo run --release -- FirstPrimitive_sphere

The executable is `rust/target/release/reyes`; it changes to the kit root
by itself, so it can be run from anywhere. Debug builds optimize a little
(`opt-level = 1`); use `--release` for the final images.

Either way:

    reyes <scene>        one scene, by name (reyes with no argument lists them)
    reyes test           every test scene
    reyes intermediate   every intermediate scene
    reyes cinematic      every cinematic scene
    reyes stills         everything but the two movies
    reyes all            everything

## What is where

    include/Ri.h          the interface your renderer implements; do not change it
    include/shaders.h     the four shaders and their parameter structs; do not change it
    include/scenes.h      the table of scenes (filled in by scenes/0_scene_table.cpp)
    include/*.h (others)  helpers the scene files use: obj.h loads OBJ models,
                          shadowmap.h does shadow lookups from your depth output,
                          sceneutil.h, models.h and anim.h are shared scene code.
                          Your renderer never calls them.
    src/main.cpp          picks the scene from the table; nothing to change
    src/ri.cpp            the renderer: stubs for you to fill in
    src/shaders.cpp       CONSTANT, STANDARD, CHECKERBOARD, BUMPY and the
                          Perlin noise BUMPY uses: stubs to fill in
    rust/                 the same kit for Rust: Cargo.toml, build.rs (compiles
                          the scenes), src/main.rs (nothing to change),
                          src/ri.rs and src/shaders.rs (the stubs)
    scenes/               the scene files, in three tiers (see below)
    renders/              where your images go, in the same three folders
    reference_renders/    the same images from the reference renderer:
                          compare yours against these
    textures/, models/    what the scenes load
    third_party/          stb_image.h and stb_image_write.h

## The scenes

`scenes/1_TestScenes.cpp` holds the small test scenes, in the order the
Suggested Workflow needs them; each one's comment says what a correct image
looks like, and `reference_renders/` shows it. Start with
`FirstPrimitive_sphere`: one flat-colored sphere, the first picture your
renderer can draw (`_cylinder`, `_cone` and `_torus` are the same test for
the other quadrics). `scenes/2_IntermediateScenes/` and `scenes/3_CinematicScenes/`
hold the bigger ones, one file each. The scene files are the inputs to your
renderer and must not be edited (the handout says what to hand in). To add
a scene of your own, put it in a new file under `scenes/`, add the file to
`CMakeLists.txt` (the Rust build finds it by itself), and add one line to
`scenes/0_scene_table.cpp`.

The two scenes that render a movie (`SimpleAnimationMovie` and
`WallEMovie`) write numbered frames. Turn them into an MP4 with ffmpeg (the
same command is in the comment at the top of each scene):

    ffmpeg -framerate 24 -i renders/1_TestScenes/SimpleAnimation/SimpleAnimation_%02d.png \
           -c:v libx264 -pix_fmt yuv420p -crf 18 renders/1_TestScenes/SimpleAnimation.mp4
    ffmpeg -framerate 24 -i renders/3_CinematicScenes/WallEMovie/WallEMovie_%02d.jpg \
           -c:v libx264 -pix_fmt yuv420p -crf 18 renders/3_CinematicScenes/WallEMovie.mp4

Both scenes have a background behind everything, so every frame is opaque
and the movie needs nothing more. (An MP4 has no alpha: frames with
transparent parts would need `-vf premultiply=inplace=1` first, or ffmpeg
drops the alpha without applying it and the edges turn jagged.)
