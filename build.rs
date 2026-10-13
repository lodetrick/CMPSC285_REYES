//! Compiles the C++ scene files into a static library that this crate links
//! against. Nothing to change here.
//!
//! The scene files are the inputs to your renderer (scenes/ in the kit, one
//! folder up from this Cargo project) and must not be edited. They call the
//! Ri* functions and the shaders, which src/ri.rs and src/shaders.rs export
//! with C linkage (#[no_mangle] extern "C"), so the link resolves them in
//! this binary. A scene of your own is a new .cpp anywhere in scenes/ (picked
//! up here automatically) and one line in scenes/0_scene_table.cpp.

use std::path::{Path, PathBuf};

fn main() {
    // The kit root: the folder that holds include/, scenes/, textures/ ...
    let manifest = std::env::var("CARGO_MANIFEST_DIR").expect("CARGO_MANIFEST_DIR");
    let kit = Path::new(&manifest).to_path_buf();
    let scenes = kit.join("assets/scenes");

    // Every .cpp in scenes/ (the scene table and the test scenes) and in the
    // two bigger tiers.
    let mut files: Vec<PathBuf> = Vec::new();
    for tier in ["", "2_IntermediateScenes", "3_CinematicScenes"] {
        let folder = scenes.join(tier);
        let mut found: Vec<PathBuf> = std::fs::read_dir(&folder)
            .unwrap_or_else(|e| panic!("cannot read {}: {}", folder.display(), e))
            .filter_map(|entry| entry.ok().map(|e| e.path()))
            .filter(|p| p.extension().map_or(false, |ext| ext == "cpp"))
            .collect();
        found.sort();
        files.extend(found);
    }

    for f in &files {
        println!("cargo:rerun-if-changed={}", f.display());
    }
    println!("cargo:rerun-if-changed={}", kit.join("include").display());

    cc::Build::new()
        .cpp(true)
        .std("c++17")
        .include(kit.join("include"))
        .files(&files)
        // The scene files are not ours to fix, so their warnings are off
        // (CheckerParams cp = { 8, 4 }; leaves fields zero on purpose).
        .warnings(false)
        .define("_CRT_SECURE_NO_WARNINGS", None) // MSVC: fopen and snprintf are fine
        .compile("scenes");
}
