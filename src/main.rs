//! main.rs: render one scene by name, or a set of them. Nothing to change
//! here; it is the Rust twin of the C++ kit's src/main.cpp.
//!
//!   cargo run --release -- QuadricZoo
//!   cargo run --release -- all            every scene
//!   cargo run --release -- stills         every scene but the two that render a movie's frames
//!   cargo run --release -- test           every scene of one tier (test, intermediate or
//!   cargo run --release -- intermediate   cinematic: the three folders of scenes/)
//!   cargo run --release -- cinematic
//!
//! The scenes are C++ functions, reached through the table that
//! scenes/0_scene_table.cpp defines (include/scenes.h). The renderer
//! itself is in ri.rs and shaders.rs.

mod ri;
mod shaders;

use std::ffi::{c_char, CStr};
use std::path::Path;

/// One entry of the scene table (scenes.h's RtSceneEntry, same layout).
#[repr(C)]
struct RtSceneEntry {
    name: *const c_char,                    /* what you type: e.g., reyes QuadricZoo */
    function: Option<unsafe extern "C" fn()>, /* the scene's entry point */
    tier: *const c_char,                    /* "test", "intermediate" or "cinematic" */
}

extern "C" {
    /// The table: {name, function, tier} entries ending with a null name.
    /// Declared as its first entry; scenes() walks from there.
    static RI_SCENES: RtSceneEntry;
}

struct Scene {
    name: &'static str,
    tier: &'static str,
    function: unsafe extern "C" fn(),
}

fn scenes() -> Vec<Scene> {
    let mut out = Vec::new();
    // SAFETY: the table is a C array ending with a null name, and every
    // entry before it has a valid function and tier.
    unsafe {
        let mut entry = std::ptr::addr_of!(RI_SCENES);
        while !(*entry).name.is_null() {
            out.push(Scene {
                name: CStr::from_ptr((*entry).name).to_str().unwrap_or("?"),
                tier: CStr::from_ptr((*entry).tier).to_str().unwrap_or("?"),
                function: (*entry).function.expect("a scene without a function"),
            });
            entry = entry.add(1);
        }
    }
    out
}

/// The scenes that render a movie's numbered frames, which "stills" skips.
const MOVIES: [&str; 2] = ["SimpleAnimationMovie", "WallEMovie"];

fn is_movie(name: &str) -> bool {
    MOVIES.contains(&name)
}

/// Whether `what` (the command line argument) selects this scene.
fn selected(what: &str, s: &Scene) -> bool {
    match what {
        "all" => true,
        "stills" => !is_movie(s.name),
        _ => what == s.tier || what == s.name,
    }
}

fn usage(scenes: &[Scene]) {
    eprintln!("usage: reyes <scene> | all | stills | test | intermediate | cinematic");
    let mut tier = "";
    for s in scenes {
        if s.tier != tier {
            tier = s.tier;
            eprintln!("{} scenes:", tier);
        }
        eprintln!("  {}{}", s.name, if is_movie(s.name) { " (movie)" } else { "" });
    }
}

/// The scenes write into renders/ by tier: RiDisplay("1_TestScenes/UVTest.png")
/// lands in renders/1_TestScenes/UVTest.png. The folders are made here, so a
/// renderer only ever has to write a file.
fn make_render_folders() {
    for folder in ["renders/1_TestScenes", "renders/2_IntermediateScenes", "renders/3_CinematicScenes"] {
        let _ = std::fs::create_dir_all(folder);
    }
}

fn main() {
    // The scenes look for textures/ and models/ and write into renders/
    // relative to the kit root, the folder above this Cargo project. Go
    // there, so that `cargo run` works from rust/ or from the kit root.
    if let Some(kit) = Path::new(env!("CARGO_MANIFEST_DIR")).parent() {
        if kit.join("assets/scenes").is_dir() {
            let _ = std::env::set_current_dir(kit);
        }
    }
    if !Path::new("assets/scenes").is_dir() {
        eprintln!("run reyes from the kit root, the folder that contains scenes/");
    }

    let scenes = scenes();
    let args: Vec<String> = std::env::args().collect();
    if args.len() != 2 {
        usage(&scenes);
        std::process::exit(1);
    }
    make_render_folders();

    let mut ran = 0;
    for s in &scenes {
        if selected(&args[1], s) {
            println!("== {}", s.name);
            // SAFETY: the C++ scene functions take no arguments and return nothing.
            unsafe { (s.function)() };
            ran += 1;
        }
    }

    if ran == 0 {
        eprintln!("unknown scene '{}'", args[1]);
        usage(&scenes);
        std::process::exit(1);
    }
}
