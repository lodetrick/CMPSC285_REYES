//! shaders.rs: the four required shaders (handout, Section 5), their
//! parameter structs (include/shaders.h, same layout), and the noise
//! function BUMPY is built on.
//!
//! CONSTANT, STANDARD, CHECKERBOARD, perlinNoise and BUMPY are stubs for you
//! to fill in, in that order. Every scene file binds the shaders by name
//! (RiSurface(CHECKERBOARD, &cp, sizeof cp)), and some call perlinNoise
//! directly, so keep the names, the signatures and the structs as they are.
//!
//! A shader is an ordinary function. The renderer fills in the inputs of the
//! RtShaderContext (ri.rs) at every grid vertex and calls the shader; a
//! surface shader writes Ci and Oi, a displacement shader moves P. The
//! context arrives as a raw pointer from C; `let c = &mut *c;` turns it into
//! a reference for the rest of the function.

#![allow(non_snake_case)] // the shader names are the handout's
#![allow(dead_code)] // the stubs do not read their parameter structs yet

use crate::ri::{RtFloat, RtInt, RtShaderContext, RtShaderFn};
use std::ffi::c_void;

/// CHECKERBOARD's parameters: sizex checks across u and sizey across v,
/// alternating between two surface shaders, light (the check at u = v = 0)
/// and dark, each run with its own parameter block. A scene writes
/// `CheckerParams cp = { 8, 4 };` (both shaders NULL, so None here) or
/// `{ 2, 2, Material, &MATTE_GREY, Material, &POLISHED_BLACK }`.
#[repr(C)]
pub struct CheckerParams {
    pub sizex: RtInt,
    pub sizey: RtInt,
    pub light: RtShaderFn,
    pub light_params: *const c_void,
    pub dark: RtShaderFn,
    pub dark_params: *const c_void,
}

/// BUMPY's parameters: see BUMPY below.
#[repr(C)]
pub struct BumpParams {
    pub amplitude: RtFloat,
    pub min_freq_exp: RtInt,
    pub max_freq_exp: RtInt,
}

/* ------------------------------------------------------------- CONSTANT */

/// The surface color, unlit: Ci = Cs and Oi = Os. The simplest shader, and
/// the first to write. FirstPrimitive binds it, so that its picture is the
/// plain RiColor whether or not your renderer runs shaders yet: once you
/// add shading, FirstPrimitive must look exactly as it did before.
#[no_mangle]
pub unsafe extern "C" fn CONSTANT(c: *mut RtShaderContext) {
    // TODO: copy the color and the opacity.
    let _ = c;
}

/* ------------------------------------------------------------- STANDARD */

/// Ambient, plus Lambertian diffuse, plus Phong specular, with the three
/// lights and the coefficients given in the handout (Section 5):
///   Ci = Cs (ka + kd sum max(N.L, 0) I C) + ks sum max(R.V, 0)^32 I C
///   Oi = Os
/// Everything is in camera space: the eye is at the origin, so the direction
/// from P toward the eye is -P. Normalize N and the light directions. Write
/// Ci as the color itself, not multiplied by Oi: the hider applies the
/// opacity when it composites (Section 7).
#[no_mangle]
pub unsafe extern "C" fn STANDARD(c: *mut RtShaderContext) {
    // TODO: the lighting. For now, the surface color, unlit.
    CONSTANT(c);
}

/* --------------------------------------------------------- CHECKERBOARD */

/// The check (u, v) falls in is (floor(u sizex), floor(v sizey)), with u = 1
/// and v = 1 counted in the last check. Checks where i + j is even are
/// "light" and run the light sub-shader, the others the dark one. A None
/// light shader means STANDARD; a None dark shader means STANDARD with
/// Cs = black, so that only its highlights show.
///
/// You don't choose the sub-shaders: the scene does, in the CheckerParams it
/// binds, just as it passes any shader its parameters. The model scenes'
/// floors, for example, do
///     CheckerParams cp = { 2, 2, Material, &MATTE_GREY,
///                                Material, &POLISHED_BLACK };
///     RiSurface(CHECKERBOARD, &cp, sizeof cp);
/// and most scenes write { 8, 4 }, leaving both NULL (None here). Your
/// RiSurface copied that struct, so here c.params points at it. A shader is
/// a plain function, so run a check's sub-shader by calling it, with
/// c.params pointed at the sub-shader's own parameters during the call:
///     let p = &*(c.params as *const CheckerParams);
///     if let Some(light) = p.light {
///         c.params = p.light_params;
///         light(c);
///         c.params = p as *const CheckerParams as *const c_void;  (put back for whoever called CHECKERBOARD)
///     }
#[no_mangle]
pub unsafe extern "C" fn CHECKERBOARD(c: *mut RtShaderContext) {
    // TODO: the checks. For now, every check is lit as the light one.
    STANDARD(c);
}

/* ---------------------------------------------------------- perlinNoise */

/// Ken Perlin's improved noise, from his paper "Improving Noise" (SIGGRAPH
/// 2002); his reference implementation, a page of Java, is at
/// https://mrl.cs.nyu.edu/~perlin/noise/. BUMPY sums octaves of it, and so
/// do some scene files' own shaders (Fbm in sceneutil.h), so until it works
/// those surfaces come out smooth and plain.
///
/// Follow his algorithm exactly: his permutation of 0..255, his fade curve
/// 6t^5 - 15t^4 + 10t^3 and his gradient function. Any other noise looks
/// just as good, but your bumps will not match reference_renders/. The
/// result is smooth and signed, about [-1, 1], and exactly 0 at every
/// integer point. Points may be negative or far from the origin, so make
/// sure the lattice index wraps into 0..255 for those too (f64 in, f64 out:
/// BUMPY's high octaves need the precision).
#[no_mangle]
pub extern "C" fn perlinNoise(x: f64, y: f64, z: f64) -> f64 {
    // TODO: the noise. For now, none, so BUMPY moves nothing.
    let _ = (x, y, z);
    0.0
}

/* ---------------------------------------------------------------- BUMPY */

/// Displace P along the unit normal N by
///   h = amplitude * sum(w_e * perlinNoise(2^e * Po)) / sum(w_e)
/// over e = min_freq_exp ..= max_freq_exp, with w_e = 2^(min_freq_exp - e):
/// each octave counts half as much as the one below it, and dividing by the
/// total weight keeps |h| <= amplitude whatever the octaves. Evaluate the
/// noise at Po (object space), so the bumps stick to the object as it
/// moves. The parameters are a BumpParams at c.params.
#[no_mangle]
pub unsafe extern "C" fn BUMPY(c: *mut RtShaderContext) {
    // TODO: the displacement. For now, P stays where it is.
    let _ = c;
}
