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

use glam::Vec3;

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
    (*c).Ci = (*c).Cs;
    (*c).Oi = (*c).Os;
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
pub unsafe extern "C" fn STANDARD(c: &mut RtShaderContext) {
    let (ka, kd, ks) = (0.15, 0.8, 0.3);

    struct Light {
        pos: Vec3,
        col: Vec3,
        int: f32,
    }

    let lights = [
        Light {
            pos: Vec3::new(-6., 8., -4.),
            col: Vec3::new(1.0, 0.95, 0.85),
            int: 1.0,
        },
        Light {
            pos: Vec3::new(7., 3., -2.),
            col: Vec3::new(0.6, 0.7, 1.0),
            int: 0.4,
        },
        Light {
            pos: Vec3::new(0., 5., 18.),
            col: Vec3::new(1., 1., 1.0),
            int: 0.5,
        },
    ];

    let mut light_diffuse = Vec3::ZERO;
    let mut light_specular = Vec3::ZERO;
    for light in lights {
        let L = (light.pos - c.P).normalize();
        let V = (-c.P).normalize();
        let R = 2. * (c.N.dot(L)) * c.N - L;

        light_diffuse += light.col * (L.dot(c.N).max(0.) * light.int);
        light_specular += light.col * (R.dot(V).max(0.0).powi(32) * light.int);
    }

    (*c).Ci = c.Cs * (ka + kd * light_diffuse + ks * light_specular);
    (*c).Oi = c.Os;
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
pub extern "C" fn CHECKERBOARD(c: &mut RtShaderContext) {
    let params: &CheckerParams = unsafe { &*(c.params as *const CheckerParams) };

    let (i, j) = (
        (c.u * params.sizex as f32) as i32,
        (c.v * params.sizey as f32) as i32,
    );

    let light_color = (i + j) % 2 == 0;
    c.params = if light_color {
        params.light_params
    } else {
        params.dark_params
    };

    if let Some(shader_fn) = if light_color {
        params.light
    } else {
        params.dark
    } {
        unsafe {
            shader_fn(c);
        }
    } else {
        let Cs = c.Cs.clone();
        if !light_color {
            c.Cs = Vec3::ZERO;
        }
        unsafe {
            STANDARD(c);
        }
        c.Cs = Cs;
    }
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
    fn lerp(t: f64, a: f64, b: f64) -> f64 {
        a + t * (b - a)
    }

    fn fade(t: f64) -> f64 {
        t * t * t * (t * (t * 6.0 - 15.0) + 10.0)
    }

    fn grad(hash: usize, x: f64, y: f64, z: f64) -> f64 {
        let h = hash & 15;
        let u = if h < 8 { x } else { y };
        let v = if h < 4 {
            y
        } else {
            if h == 12 || h == 14 {
                x
            } else {
                z
            }
        };

        (if (h & 1) == 0 { u } else { -u }) + (if (h & 2) == 0 { v } else { -v })
    }

    const P: [usize; 512] = [
        151, 160, 137, 91, 90, 15, 131, 13, 201, 95, 96, 53, 194, 233, 7, 225, 140, 36, 103, 30,
        69, 142, 8, 99, 37, 240, 21, 10, 23, 190, 6, 148, 247, 120, 234, 75, 0, 26, 197, 62, 94,
        252, 219, 203, 117, 35, 11, 32, 57, 177, 33, 88, 237, 149, 56, 87, 174, 20, 125, 136, 171,
        168, 68, 175, 74, 165, 71, 134, 139, 48, 27, 166, 77, 146, 158, 231, 83, 111, 229, 122, 60,
        211, 133, 230, 220, 105, 92, 41, 55, 46, 245, 40, 244, 102, 143, 54, 65, 25, 63, 161, 1,
        216, 80, 73, 209, 76, 132, 187, 208, 89, 18, 169, 200, 196, 135, 130, 116, 188, 159, 86,
        164, 100, 109, 198, 173, 186, 3, 64, 52, 217, 226, 250, 124, 123, 5, 202, 38, 147, 118,
        126, 255, 82, 85, 212, 207, 206, 59, 227, 47, 16, 58, 17, 182, 189, 28, 42, 223, 183, 170,
        213, 119, 248, 152, 2, 44, 154, 163, 70, 221, 153, 101, 155, 167, 43, 172, 9, 129, 22, 39,
        253, 19, 98, 108, 110, 79, 113, 224, 232, 178, 185, 112, 104, 218, 246, 97, 228, 251, 34,
        242, 193, 238, 210, 144, 12, 191, 179, 162, 241, 81, 51, 145, 235, 249, 14, 239, 107, 49,
        192, 214, 31, 181, 199, 106, 157, 184, 84, 204, 176, 115, 121, 50, 45, 127, 4, 150, 254,
        138, 236, 205, 93, 222, 114, 67, 29, 24, 72, 243, 141, 128, 195, 78, 66, 215, 61, 156, 180,
        151, 160, 137, 91, 90, 15, 131, 13, 201, 95, 96, 53, 194, 233, 7, 225, 140, 36, 103, 30,
        69, 142, 8, 99, 37, 240, 21, 10, 23, 190, 6, 148, 247, 120, 234, 75, 0, 26, 197, 62, 94,
        252, 219, 203, 117, 35, 11, 32, 57, 177, 33, 88, 237, 149, 56, 87, 174, 20, 125, 136, 171,
        168, 68, 175, 74, 165, 71, 134, 139, 48, 27, 166, 77, 146, 158, 231, 83, 111, 229, 122, 60,
        211, 133, 230, 220, 105, 92, 41, 55, 46, 245, 40, 244, 102, 143, 54, 65, 25, 63, 161, 1,
        216, 80, 73, 209, 76, 132, 187, 208, 89, 18, 169, 200, 196, 135, 130, 116, 188, 159, 86,
        164, 100, 109, 198, 173, 186, 3, 64, 52, 217, 226, 250, 124, 123, 5, 202, 38, 147, 118,
        126, 255, 82, 85, 212, 207, 206, 59, 227, 47, 16, 58, 17, 182, 189, 28, 42, 223, 183, 170,
        213, 119, 248, 152, 2, 44, 154, 163, 70, 221, 153, 101, 155, 167, 43, 172, 9, 129, 22, 39,
        253, 19, 98, 108, 110, 79, 113, 224, 232, 178, 185, 112, 104, 218, 246, 97, 228, 251, 34,
        242, 193, 238, 210, 144, 12, 191, 179, 162, 241, 81, 51, 145, 235, 249, 14, 239, 107, 49,
        192, 214, 31, 181, 199, 106, 157, 184, 84, 204, 176, 115, 121, 50, 45, 127, 4, 150, 254,
        138, 236, 205, 93, 222, 114, 67, 29, 24, 72, 243, 141, 128, 195, 78, 66, 215, 61, 156, 180,
    ];

    let (X, Y, Z) = (
        x.floor() as usize & 255,
        y.floor() as usize & 255,
        z.floor() as usize & 255,
    );

    let (x, y, z) = (x - x.floor(), y - y.floor(), z - z.floor());
    let (u, v, w) = (fade(x), fade(y), fade(z));

    let (A, B) = (P[X] + Y, P[X + 1] + Y);
    let (AA, AB) = (P[A] + Z, P[A + 1] + Z);
    let (BA, BB) = (P[B] + Z, P[B + 1] + Z);

    lerp(
        w,
        lerp(
            v,
            lerp(u, grad(P[AA], x, y, z), grad(P[BA], x - 1., y, z)),
            lerp(u, grad(P[AB], x, y - 1., z), grad(P[BB], x - 1., y - 1., z)),
        ),
        lerp(
            v,
            lerp(
                u,
                grad(P[AA + 1], x, y, z - 1.),
                grad(P[BA + 1], x - 1., y, z - 1.),
            ),
            lerp(
                u,
                grad(P[AB + 1], x, y - 1., z - 1.),
                grad(P[BB + 1], x - 1., y - 1., z - 1.),
            ),
        ),
    )
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
pub unsafe extern "C" fn BUMPY(c: &mut RtShaderContext) {
    let params: &BumpParams = unsafe { &*(c.params as *const BumpParams) };

    let mut sum_we = 0.0;
    let mut sum_perlin = 0.0;
    for e in params.min_freq_exp..=params.max_freq_exp {
        let w_e = 2.0f32.powi(params.min_freq_exp - e);
        let point_to_perlin: Vec3 = 2.0f32.powi(e) * c.Po;
        sum_perlin += w_e as f64
            * perlinNoise(
                point_to_perlin.x as f64,
                point_to_perlin.y as f64,
                point_to_perlin.z as f64,
            );
        sum_we += w_e;
    }

    let h = params.amplitude * (sum_perlin as f32) / sum_we;
    c.P += h * c.N;
}
