//! ri.rs: the HW1 RenderMan-like interface (include/Ri.h) in Rust, and the
//! renderer behind it.
//!
//! The scene files are C++ and compile against Ri.h; this file declares the
//! same types with the same memory layout (#[repr(C)]) and exports the same
//! functions with C linkage (#[no_mangle] extern "C"), so the unchanged
//! scene files link against your renderer. Keep every signature as it is.
//!
//! This is a stub. Every function the scenes call is here, does nothing
//! useful yet, and RiWorldEnd writes a black image of the right size and
//! name, so the whole kit builds and runs before you have written a line.
//! Replace the stubs with your REYES renderer, following the Suggested
//! Workflow: transformations and the camera first, then one primitive
//! (FirstPrimitive_sphere is the scene for it), then dicing, sampling, the
//! hider, the other primitives, shading, textures, displacement, opacity,
//! and depth output. The comment on each function says what the finished
//! version must do and where the handout describes it. Split the code into
//! more modules as it grows.
//!
//! The two functions at the end, write_image and write_depth, are complete:
//! they write the file RiDisplay named, in the format its extension says,
//! from a buffer you fill in. Nothing else here is.
//!
//! The shaders live in shaders.rs. Everything the scene files include is in
//! ../include/ and must not change: Ri.h, shaders.h and scenes.h are the
//! interface; obj.h, shadowmap.h, sceneutil.h, anim.h and models.h are
//! helpers for the scene files that never call back into the renderer
//! except through that interface.

#![allow(non_snake_case)] // the Ri names and the context's fields are RenderMan's
#![allow(dead_code)] // the parts of the interface the stub does not use yet

use std::ffi::{c_char, c_void, CStr};
use std::io::Write;
use std::path::Path;
use std::sync::Mutex;

/* ------------------------------------------------------------------ types */

pub type RtVoid = ();
pub type RtFloat = f32;
pub type RtInt = i32;
pub type RtToken = *const c_char;
pub type RtPointer = *mut c_void;
pub type RtColor = [RtFloat; 3];
pub type RtPoint = [RtFloat; 3];
pub type RtVector = [RtFloat; 3];
pub type RtNormal = [RtFloat; 3];

#[derive(Copy, Clone)]
pub struct RtMatrix {
    o: [[RtFloat; 4]; 4]
}

pub const RI_PERSPECTIVE: &str = "perspective";
pub const RI_ORTHOGRAPHIC: &str = "orthographic";

/// What a shader sees at one grid vertex (Ri.h's RtShaderContext, same
/// layout). The renderer fills in the inputs; a displacement shader moves
/// P, a surface shader writes Ci and Oi.
#[repr(C)]
pub struct RtShaderContext {
    /* inputs, filled by the renderer at each grid vertex */
    pub P: RtPoint,          /* position of grid vertex in camera/eye space */
    pub Po: RtPoint,         /* same point in object space */
    pub N: RtNormal,         /* unit normal, recomputed after displacement */
    pub u: RtFloat,          /* surface parameters in [0,1], or a mesh's st */
    pub v: RtFloat,
    pub Cs: RtColor,         /* current RiColor and RiOpacity */
    pub Os: RtColor,
    pub params: *const c_void, /* this shader's parameter block, or null */
    /* outputs */
    pub Ci: RtColor,         /* written by surface shaders */
    pub Oi: RtColor,
    /* displacement shaders modify P in place */
}

/// A shader: a C function pointer, or None where a scene passes NULL
/// (RiSurface(NULL, NULL, 0) means STANDARD).
pub type RtShaderFn = Option<unsafe extern "C" fn(*mut RtShaderContext)>;

/* --------------------------------------------------------- type implementations */

impl RtMatrix {
    const fn identity() -> Self {
        RtMatrix {
            o: [
                [1.,0.,0.,0.],
                [0.,1.,0.,0.],
                [0.,0.,1.,0.],
                [0.,0.,0.,1.],
               ]
        }
    }
}

/* --------------------------------------------------------- renderer state */

/// What the scene has asked for so far. The finished renderer keeps much
/// more here: the current transformation, its stack, the color, opacity and
/// bound shaders, the projection, the sample buffer ... (handout, Section 2).
struct Options {
    xres: i32,
    yres: i32,
    xsamples: i32,
    ysamples: i32,
    display: String,
    depth_display: bool, /* RiDisplay(name, "zfile", "z") */
    transformation_curr: RtMatrix,
    transformation_stack: Vec<RtMatrix>,
}

static OPTIONS: Mutex<Options> = Mutex::new(Options {
    xres: 640,
    yres: 480,
    xsamples: 2,
    ysamples: 2,
    display: String::new(),
    depth_display: false,
    transformation_curr: RtMatrix::identity(),
    transformation_stack: Vec::new(),
});

/// A C string argument as a Rust &str ("" for a null pointer).
unsafe fn token(s: *const c_char) -> &'static str {
    if s.is_null() {
        ""
    } else {
        CStr::from_ptr(s).to_str().unwrap_or("")
    }
}

/* ---------------------------------------------------------- graphics state */

/// RiBegin starts a rendering session and resets the graphics state to
/// RenderMan's defaults: color and opacity (1, 1, 1), surface shader
/// STANDARD, no displacement, orthographic projection, 640 x 480 pixels,
/// 2 x 2 samples. Scenes rely on these (FirstPrimitive never calls
/// RiOpacity). RiEnd ends the session (free textures and buffers here).
/// The name is always RI_NULL.
#[no_mangle]
pub extern "C" fn RiBegin(_name: RtToken) {
    let mut o = OPTIONS.lock().unwrap();
    o.xres = 640;
    o.yres = 480;
    o.xsamples = 2;
    o.ysamples = 2;
    o.display = String::new();
    o.depth_display = false;
    o.transformation_curr = RtMatrix::identity();
    o.transformation_stack = Vec::new();
}

#[no_mangle]
pub extern "C" fn RiEnd() {}

/// The image size in pixels. The screen window is [-a, a] x [-1, 1] for
/// a = xres / yres >= 1 and [-1, 1] x [-1/a, 1/a] for a < 1, so the field of
/// view spans the shorter side (Section 2).
#[no_mangle]
pub extern "C" fn RiFormat(xres: RtInt, yres: RtInt) {
    let mut o = OPTIONS.lock().unwrap();
    o.xres = xres;
    o.yres = yres;
}

/// xsamples x ysamples jittered samples per pixel (Section 4).
#[no_mangle]
pub extern "C" fn RiPixelSamples(xsamples: RtInt, ysamples: RtInt) {
    let mut o = OPTIONS.lock().unwrap();
    o.xsamples = xsamples.max(1);
    o.ysamples = ysamples.max(1);
}

/// The output file, written into renders/ by RiWorldEnd. RiDisplay(name, "",
/// "") names the picture; RiDisplay(name, "zfile", "z") asks for the depth
/// image instead (Section 4, "Depth output").
#[no_mangle]
pub unsafe extern "C" fn RiDisplay(name: *const c_char, type_: RtToken, mode: RtToken) {
    let mut o = OPTIONS.lock().unwrap();
    o.display = token(name).to_string();
    o.depth_display = token(type_) == "zfile" || token(mode) == "z";
}

/// RiFrameBegin/End bracket one frame of an animation: save the graphics
/// state at Begin and restore it at End, so that each frame starts afresh.
#[no_mangle]
pub extern "C" fn RiFrameBegin(_frame: RtInt) {}

#[no_mangle]
pub extern "C" fn RiFrameEnd() {}

/// RiWorldBegin: the transformation built so far is the camera transform
/// (world to camera); from here on transformations place objects. Set up
/// the sample buffer. RiWorldEnd: all primitives have been drawn, so filter
/// the samples into pixels and write the image (Sections 2 and 4).
#[no_mangle]
pub extern "C" fn RiWorldBegin() {}

#[no_mangle]
pub extern "C" fn RiWorldEnd() {
    // TODO: your pixels. For now, a black, fully transparent image (or an
    // empty depth image, +infinity everywhere).
    let (w, h, depth) = {
        let o = OPTIONS.lock().unwrap();
        (o.xres as usize, o.yres as usize, o.depth_display)
    };
    if depth {
        write_depth(&vec![f32::INFINITY; w * h]);
    } else {
        write_image(&vec![0.0f32; w * h * 4]);
    }
}

/// RiProjection(RI_PERSPECTIVE, "fov", &fov), fov in degrees over the
/// shorter side of the image, or RiProjection(RI_ORTHOGRAPHIC, NULL, NULL):
/// the C++ scenes omit the last two arguments, which default to NULL. It
/// also resets the current transformation to the identity: the camera
/// transform is built after it (Section 2). value points at an RtFloat.
#[no_mangle]
pub unsafe extern "C" fn RiProjection(name: RtToken, token: RtToken, value: RtPointer) {
    let (_name, _token, _value) = (name, token, value);
}

/* ----------------------------------------------------------- transformations */

/// The current transformation matrix (CTM) and its stack. Each call
/// post-multiplies the CTM, so the transformation called last is applied
/// to points first; RiTransformBegin pushes a copy and RiTransformEnd pops
/// it. Scenes nest these several deep. RiConcatTransform takes an RtMatrix
/// in RenderMan's row-vector form (p' = p M, translation in the bottom
/// row); in C an array argument decays to a pointer, so it arrives as a
/// pointer to four rows of four floats. Angles are degrees (Section 2).
#[no_mangle]
pub extern "C" fn RiTransformBegin() {
    let mut o = OPTIONS.lock().unwrap();
    let transformation_curr = o.transformation_curr.clone();
    o.transformation_stack.push(transformation_curr);
}

#[no_mangle]
pub extern "C" fn RiTransformEnd() {
    let mut o = OPTIONS.lock().unwrap();
    let transformation = o.transformation_stack.pop().unwrap();
    o.transformation_curr = transformation;
}

#[no_mangle]
pub unsafe extern "C" fn RiConcatTransform(transform: *const [RtFloat; 4]) {
    let _ = transform;
}

#[no_mangle]
pub extern "C" fn RiTranslate(dx: RtFloat, dy: RtFloat, dz: RtFloat) {
    let _ = (dx, dy, dz);
}

#[no_mangle]
pub extern "C" fn RiRotate(angle: RtFloat, dx: RtFloat, dy: RtFloat, dz: RtFloat) {
    let _ = (angle, dx, dy, dz);
}

#[no_mangle]
pub extern "C" fn RiScale(sx: RtFloat, sy: RtFloat, sz: RtFloat) {
    let _ = (sx, sy, sz);
    // [
    //     [],
    //     [],
    //     [],
    //     [],
    // ]
}

/* ---------------------------------------------------------------- primitives */

/// Each primitive goes through the REYES pipeline as soon as it is called:
/// bound it, choose a dice rate from the bound's size on screen, dice it
/// into a grid of micropolygons, shade the grid, and hand the micropolygons
/// to the hider (Section 3). The quadrics use the RenderMan parameterizations
/// (RiSpec section 5.4); thetamax, phimin and phimax are degrees.
///
/// Shade the grid in two passes (Section 5): the displacement shader at
/// every vertex, then N recomputed from the displaced grid, then the surface
/// shader at every vertex. Each call fills one vertex's RtShaderContext,
/// runs the shader and keeps what it wrote. A shader sees only its own
/// vertex, so the order you visit them in does not matter; only the new N
/// needs the neighbors, which is why it waits for the whole first pass.
#[no_mangle]
pub extern "C" fn RiSphere(radius: RtFloat, zmin: RtFloat, zmax: RtFloat, thetamax: RtFloat) {
    let _ = (radius, zmin, zmax, thetamax);
}

#[no_mangle]
pub extern "C" fn RiCylinder(radius: RtFloat, zmin: RtFloat, zmax: RtFloat, thetamax: RtFloat) {
    let _ = (radius, zmin, zmax, thetamax);
}

#[no_mangle]
pub extern "C" fn RiCone(height: RtFloat, radius: RtFloat, thetamax: RtFloat) {
    let _ = (height, radius, thetamax);
}

#[no_mangle]
pub extern "C" fn RiTorus(majorradius: RtFloat, minorradius: RtFloat, phimin: RtFloat, phimax: RtFloat, thetamax: RtFloat) {
    let _ = (majorradius, minorradius, phimin, phimax, thetamax);
}

/// A mesh of polygons: polygon i has nverts[i] corners, whose indices into
/// P (and N and st, when they are not null) follow one another in verts.
/// P and N hold three floats per vertex, st two. Treat every polygon as its
/// own primitive, bounded, diced, shaded and hidden before the next: a quad
/// is a bilinear patch, a triangle the same patch with its last corner
/// repeated, and a larger polygon a fan of triangles from its first vertex
/// (Section 3, "Polygon meshes"). Read the arrays with
/// std::slice::from_raw_parts.
#[no_mangle]
pub unsafe extern "C" fn RiPointsPolygons(
    npolys: RtInt,
    nverts: *const RtInt,
    verts: *const RtInt,
    P: *const RtPoint,
    N: *const RtNormal,
    st: *const RtFloat,
) {
    let _ = (npolys, nverts, verts, P, N, st);
}

/* ------------------------------------------------------------------ shading */

/// Attributes: they stay set until changed (RiTransformEnd does not restore
/// them). The color and opacity become Cs and Os in the shader context.
/// An RtColor argument arrives as a pointer to its three floats.
#[no_mangle]
pub unsafe extern "C" fn RiColor(color: *const RtFloat) {
    let _ = color;
}

#[no_mangle]
pub unsafe extern "C" fn RiOpacity(opacity: *const RtFloat) {
    let _ = opacity;
}

/// Bind a shader to the primitives that follow. Copy nbytes of params now
/// (std::slice::from_raw_parts(params as *const u8, nbytes).to_vec()): the
/// scene's struct may be gone by the time the shader runs. A None shader
/// means STANDARD for RiSurface and no displacement for RiDisplacement
/// (Section 5).
#[no_mangle]
pub unsafe extern "C" fn RiSurface(fn_: RtShaderFn, params: *const c_void, nbytes: usize) {
    let _ = (fn_, params, nbytes);
}

#[no_mangle]
pub unsafe extern "C" fn RiDisplacement(fn_: RtShaderFn, params: *const c_void, nbytes: usize) {
    let _ = (fn_, params, nbytes);
}

/// Load an image into one of 8 texture slots with the image crate
/// (image::open; look for the file as given, then under textures/), and
/// look it up bilinearly with repeat wrap: s runs left to right, t from the
/// TOP row (t = 0) to the bottom (Section 6). An empty slot reads black.
/// result points at three floats.
#[no_mangle]
pub unsafe extern "C" fn RiMakeTexture(file: *const c_char, slot: RtInt) {
    let _ = (token(file), slot);
}

#[no_mangle]
pub unsafe extern "C" fn RiTexture(slot: RtInt, s: RtFloat, t: RtFloat, result: *mut RtFloat) {
    let _ = (slot, s, t);
    for i in 0..3 {
        *result.add(i) = 0.0;
    }
}

/* ------------------------------------------------------------ output files */

/// Lowercase extension of a file name, without the dot ("" if none).
fn extension(name: &str) -> String {
    Path::new(name)
        .extension()
        .and_then(|e| e.to_str())
        .map(|e| e.to_ascii_lowercase())
        .unwrap_or_default()
}

/// A value clamped to [0, 1], as an 8-bit integer.
fn to8(v: f32) -> u8 {
    (v.clamp(0.0, 1.0) * 255.0 + 0.5).floor() as u8
}

/// Write the picture to renders/<RiDisplay name>, in the format its
/// extension names. Complete: call it from RiWorldEnd with your filtered
/// pixels, xres * yres * 4 floats, top row first: each pixel's color as
/// compositing leaves it, premultiplied by alpha (that is, over the black
/// background), then its alpha, the mean of its three opacity channels.
/// Don't clamp or convert them; this does, per format:
///   .png, .tga  keep alpha. Both store straight color, so each pixel's
///               color is divided by its alpha: a pixel half covered by a
///               white surface is stored as white with alpha one half, and
///               whatever shows the file multiplies the alpha back in.
///   .jpg, .bmp  are written without alpha: JPEG has none, and most
///               programs ignore a BMP's. They get the picture over the
///               black background, which is the premultiplied color itself.
/// Values are clamped to [0, 1] and rounded to 8 bits.
pub fn write_image(rgba: &[f32]) {
    let (w, h, name) = {
        let o = OPTIONS.lock().unwrap();
        (o.xres as u32, o.yres as u32, o.display.clone())
    };
    let path = format!("renders/{}", name);
    if rgba.len() != (w as usize) * (h as usize) * 4 {
        eprintln!("write_image: the buffer is not {} x {} x 4 floats", w, h);
        return;
    }
    let ext = extension(&name);
    let keep_alpha = match ext.as_str() {
        "png" | "tga" => true,
        "jpg" | "jpeg" | "bmp" => false,
        _ => {
            eprintln!("cannot write {}: use .png, .bmp, .tga or .jpg", path);
            return;
        }
    };

    let comp = if keep_alpha { 4 } else { 3 }; // with alpha, or without
    let mut bytes = Vec::with_capacity((w * h) as usize * comp);
    for px in rgba.chunks_exact(4) {
        let alpha = px[3].clamp(0.0, 1.0);
        for c in 0..3 {
            let straight = if alpha > 0.0 { px[c] / alpha } else { 0.0 };
            bytes.push(to8(if keep_alpha { straight } else { px[c] }));
        }
        if keep_alpha {
            bytes.push(to8(alpha));
        }
    }

    let result = if keep_alpha {
        let image = image::RgbaImage::from_raw(w, h, bytes).expect("w * h * 4 bytes");
        let format = if ext == "png" { image::ImageFormat::Png } else { image::ImageFormat::Tga };
        image.save_with_format(&path, format)
    } else {
        let image = image::RgbImage::from_raw(w, h, bytes).expect("w * h * 3 bytes");
        if ext == "bmp" {
            image.save_with_format(&path, image::ImageFormat::Bmp)
        } else {
            std::fs::File::create(&path).map_err(image::ImageError::IoError).and_then(|file| {
                let mut out = std::io::BufWriter::new(file);
                image::codecs::jpeg::JpegEncoder::new_with_quality(&mut out, 95).encode_image(&image)
            })
        }
    };
    match result {
        Ok(()) => println!("wrote {} ({} x {})", path, w, h),
        Err(e) => eprintln!("could not write {} ({}; run reyes from the kit root)", path, e),
    }
}

/// Write the depth image to renders/<RiDisplay name> in the handout's
/// format: "Rz01", int32 width and height, then one float per pixel, top
/// row first, all little-endian (the camera-space z of the nearest surface
/// at least half opaque, +infinity where there is none). Complete: call it
/// from RiWorldEnd when RiDisplay asked for depth. The scene that reads the
/// file back knows the camera it was rendered with.
pub fn write_depth(depth: &[f32]) {
    let (w, h, name) = {
        let o = OPTIONS.lock().unwrap();
        (o.xres, o.yres, o.display.clone())
    };
    let path = format!("assets/renders/{}", name);
    if depth.len() != (w as usize) * (h as usize) {
        eprintln!("write_depth: the buffer is not {} x {} floats", w, h);
        return;
    }
    let result = std::fs::File::create(&path).and_then(|file| {
        let mut out = std::io::BufWriter::new(file);
        out.write_all(b"Rz01")?;
        out.write_all(&w.to_le_bytes())?;
        out.write_all(&h.to_le_bytes())?;
        for d in depth {
            out.write_all(&d.to_le_bytes())?;
        }
        out.flush()
    });
    match result {
        Ok(()) => println!("wrote {} ({} x {} depth)", path, w, h),
        Err(e) => eprintln!("could not write {} ({}; run reyes from the kit root)", path, e),
    }
}
