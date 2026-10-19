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
use std::ptr;
use std::sync::{Arc, Mutex};

use glam::{Mat4, Vec2, Vec3, Vec4Swizzles};
use ndarray::Array2;

/* ------------------------------------------------------------------ types */

pub type RtVoid = ();
pub type RtFloat = f32;
pub type RtInt = i32;
pub type RtToken = *const c_char;
pub type RtPointer = *mut c_void;
pub type RtColor = Vec3;
pub type RtPoint = Vec3;
pub type RtVector = Vec3;
pub type RtNormal = Vec3;
pub type RtMatrix = Mat4;

pub const RI_PERSPECTIVE: &str = "perspective";
pub const RI_ORTHOGRAPHIC: &str = "orthographic";

enum Projection {
    Orthographic,
    Perspective {fov: f32}
}

#[derive(Clone)]
struct Vertex {
    p: Vec3,
    c: RtColor,
    o: RtColor,
    u: f32,
    v: f32,
}

impl Vertex {
    fn new(p: Vec3, c: RtColor, o: RtColor, u: f32, v: f32) -> Self {
        Vertex {
            p,
            c,
            o,
            u,
            v,
        }
    }
}

struct Triangle<'a> {
    v1: &'a Vertex,
    v2: &'a Vertex,
    v3: &'a Vertex,
}

impl<'a> Triangle<'a> {
    fn new(v1: &'a Vertex, v2: &'a Vertex, v3: &'a Vertex) -> Self {
        Triangle {
            v1,
            v2,
            v3,
        }
    }

    fn rasterize(&self, object_to_screen : RtMatrix, s: &mut Array2<Vec<Sample>>, w: f32, h: f32, xsamples: usize, ysamples: usize) {
        // Transform object coordinates to NDC coordinates
        let (p1, p2, p3) = (
            map_to_space(object_to_screen, self.v1.p),
            map_to_space(object_to_screen, self.v2.p),
            map_to_space(object_to_screen, self.v3.p),
        );
        
        let (r1, r2, r3) = (
            Vec2::new((p1.x + 1.) * 0.5 * w, (1. - p1.y) * 0.5 * h),
            Vec2::new((p2.x + 1.) * 0.5 * w, (1. - p2.y) * 0.5 * h),
            Vec2::new((p3.x + 1.) * 0.5 * w, (1. - p3.y) * 0.5 * h),
        );

        // check if in bounds
        let (x_min, x_max, y_min, y_max) = (
            r1.x.min(r2.x.min(r3.x)),
            r1.x.max(r2.x.max(r3.x)),
            r1.y.min(r2.y.min(r3.y)),
            r1.y.max(r2.y.max(r3.y)),
        );

        // Don't render tris outside of the screen.
        if x_max < 0. || x_min > w || y_max < 0. || y_min > h { return; }

        let (x_min, x_max, y_min, y_max) = (
            (x_min.floor() - 1.).max(0.0) as usize,
            (x_max.ceil() as usize).min(w as usize),
            (y_min.floor() - 1.).max(0.0) as usize,
            (y_max.ceil() as usize).min(h as usize),
        );

        for i in x_min..x_max {
            for j in y_min..y_max {
                let pixel_base = Vec2::new(i as f32, j as f32);

                for s_i in 0..xsamples {
                    for s_j in 0..ysamples {
                        let sample = s.get_mut((i * xsamples + s_i, j * ysamples + s_j));

                        // the sample doesn't exist therefore the pixel doesn't either
                        let Some(sample) = sample else { unreachable!(); };

                        // Generate the position of the pixel (0.5 jitter to start)
                        let r = pixel_base + Vec2::new(
                            (s_i as f32 + 0.5) / (xsamples as f32),
                            (s_j as f32 + 0.5) / (ysamples as f32),
                        );

                        let tri_area = (r1-r3).perp_dot(r2-r3);

                        if tri_area.abs() < 1e-8 { continue; }
        
                        let (w1, w2, w3) = (
                            (r-r3).perp_dot(r2-r3) / tri_area,
                            (r-r3).perp_dot(r3-r1) / tri_area,
                            (r-r1).perp_dot(r1-r2) / tri_area,
                        );
        
                        if 0. <= w1 && w1 <= 1.
                        && 0. <= w2 && w2 <= 1.
                        && 0. <= w3 && w3 <= 1. {
                            let hit_info = Sample {
                                depth: barycentric(w1, w2, p1.z, p2.z, p3.z),
                                color: barycentric_vec(w1, w2, self.v1.c, self.v2.c, self.v3.c),
                                opacity: barycentric_vec(w1, w2, self.v1.o, self.v2.o, self.v3.o),
                            };

                            let will_truncate = hit_info.opacity.abs_diff_eq(Vec3::ONE, 1e-6);

                            let mut insert_index = Some(sample.len());
                            for i in 0..sample.len() {
                                if sample[i].depth > hit_info.depth {
                                    insert_index = Some(i);

                                    if will_truncate {
                                        sample.resize(i, Sample::default()); // truncate depth hits to
                                    }

                                    break;
                                }

                                // If we hit an opaque poly closer than our poly, then we can't insert.
                                if sample[i].opacity.abs_diff_eq(Vec3::ONE, 1e-6) {
                                    insert_index = None;
                                    break;
                                }
                            }

                            if let Some(insert_index) = insert_index {
                                sample.insert(insert_index, hit_info);
                            }
                        }
                    }
                }
            }
        }
    }
}

fn barycentric(alpha: f32, beta: f32, v1: f32, v2: f32, v3: f32) -> f32 {
    v1 + alpha * (v2 - v1) + beta * (v3 - v1)
}

fn barycentric_vec(alpha: f32, beta: f32, v1: Vec3, v2: Vec3, v3: Vec3) -> Vec3 {
    v1 + alpha * (v2 - v1) + beta * (v3 - v1)
}

struct MicroGrid {
    vertices: Vec<Vertex>,
    w: usize,
    h: usize
}

impl MicroGrid {
    fn new(w: usize, h: usize) -> Self {
        MicroGrid {
            vertices: vec![Vertex::new(Vec3::ZERO, Vec3::ZERO, Vec3::ZERO, 0., 0.); (w + 1) * (h + 1)],
            w,
            h,
        }
    }

    fn get_vertex(&self, x: usize, y: usize) -> &Vertex {
        &self.vertices[x + y * (self.w + 1)]
    }

    fn get_vertex_mut(&mut self, x: usize, y: usize) -> &mut Vertex {
        &mut self.vertices[x + y * (self.w + 1)]
    }

    fn shade(&mut self, g: &GraphicsState, o: &Options) {
        let object_to_screen = g.object_to_screen(&o);
        let object_to_world = g.world_to_view() * g.transformation_curr;

        for u in 0..=self.w {
            for v in 0..=self.h {

                let n = {
                    let p1_w = map_to_space(object_to_world, self.get_vertex(u, v).p);
                    let p2_w = map_to_space(object_to_world, self.get_vertex(if u == self.w {u - 1} else {u + 1}, v).p);
                    let p3_w = map_to_space(object_to_world, self.get_vertex(u, if v == self.h {v - 1} else {v + 1}).p);
                    (p1_w - p2_w).cross(p1_w - p3_w).normalize()
                };

                let vertex = self.get_vertex_mut(u, v);
                
                if let Some(shader_fn) = g.shader_fn {
                    let p_screen = map_to_space(object_to_screen, vertex.p);
                    
                    let params: *const c_void = if let Some(shader_params) = &g.shader_params {
                        shader_params.as_ptr() as *const c_void
                    } else {
                        ptr::null()
                    };

                    let mut shader_context = RtShaderContext {
                        P: p_screen,
                        Po: vertex.p, /* same point in object space */
                        N: n, /* unit normal, recomputed after displacement */
                        u: vertex.u, /* surface parameters in [0,1], or a mesh's st */
                        v: vertex.v,
                        Cs: g.color, /* current RiColor and RiOpacity */
                        Os: g.opacity,
                        params: params,
                        Ci: Vec3::ZERO, /* written by surface shaders */
                        Oi: Vec3::ZERO,
                    };
    
                    unsafe {
                        shader_fn(&mut shader_context);
                    }

                    vertex.c = shader_context.Ci;
                    vertex.o = shader_context.Oi;
                }
            }
        }
    }

    fn sample(&mut self, g: &GraphicsState, o: &Options) {
        let s = &mut SAMPLE_BUF.lock().unwrap();
        let s = s.as_mut().unwrap();
        let object_to_screen = g.object_to_screen(&o);
        let (w, h, xs, ys) = (o.xres as f32, o.yres as f32, o.xsamples, o.ysamples);

        for u in 0..self.w {
            for v in 0..self.h {
                let v_tl: &Vertex = self.get_vertex(u, v);
                let v_tr: &Vertex = self.get_vertex(u + 1, v);
                let v_bl: &Vertex = self.get_vertex(u, v + 1);
                let v_br: &Vertex = self.get_vertex(u + 1, v + 1);

                let (tri1, tri2) = (
                    Triangle::new(v_tl, v_tr, v_bl),
                    Triangle::new(v_bl, v_tr, v_br),
                );

                tri1.rasterize(object_to_screen, s, w, h, xs, ys);
                tri2.rasterize(object_to_screen, s, w, h, xs, ys);
            }
        }
    } 
}

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

/* --------------------------------------------------------- type helpers */

unsafe fn RtMatrix_from_ptr(transform: *const [[RtFloat; 4]; 4]) -> RtMatrix {
    RtMatrix::from_cols_array_2d(transform.as_ref_unchecked())
}

fn map_to_space(transform: RtMatrix, p: Vec3) -> Vec3 {
    let p_transformed = transform.mul_vec4(p.extend(1.));
    p_transformed.xyz() / p_transformed.w
}

/* --------------------------------------------------------- renderer state */

/// What the scene has asked for so far. The finished renderer keeps much
/// more here: the current transformation, its stack, the color, opacity and
/// bound shaders, the projection, the sample buffer ... (handout, Section 2).
struct Options {
    xres: i32,
    yres: i32,
    xsamples: usize,
    ysamples: usize,
    display: String,
    depth_display: bool, /* RiDisplay(name, "zfile", "z") */
    projection: Projection
}

impl Options {
    const fn default() -> Self {
        Options {
            xres: 640,
            yres: 480,
            xsamples: 2,
            ysamples: 2,
            display: String::new(),
            depth_display: false,
            projection: Projection::Orthographic
        }
    }
}

static OPTIONS: Mutex<Options> = Mutex::new(Options::default());

struct GraphicsState {
    color: RtColor,
    opacity: RtColor,
    transformation_curr: RtMatrix,
    transformation_stack: Vec<RtMatrix>,
    camera_transform: RtMatrix,
    shader_fn: RtShaderFn,
    shader_params: Option<Arc<[u8]>>,
}

impl GraphicsState {
    const fn default() -> Self {
        GraphicsState {
            color: Vec3::ONE,
            opacity: Vec3::ONE,
            transformation_curr: RtMatrix::IDENTITY,
            transformation_stack: Vec::new(),
            camera_transform: RtMatrix::IDENTITY,
            shader_fn: None,
            shader_params: None,
        }
    }

    fn object_to_screen(&self, o: &Options) -> RtMatrix {
        self.view_to_screen(&o) * self.world_to_view() * self.transformation_curr
    }

    fn world_to_view(&self) -> RtMatrix {
        self.camera_transform.inverse()
    }

    fn view_to_screen(&self, o: &Options) -> RtMatrix {
        let (n, f) = (0.01, 10000.);
        let aspect = o.xres as f32 / o.yres as f32;

        match o.projection {
            Projection::Orthographic => {
                let (l, r) = if aspect >= 1. { (-aspect, aspect) } else { (-1.,1.) };
                let (t, b) = if aspect >= 1. { (-1., 1.) } else { (-1. / aspect, 1. / aspect) };

                RtMatrix::from_cols_array_2d(&[
                    [2. / (r - l),0.,0.,0.], 
                    [0.,2. / (t - b),0.,0.], 
                    [0.,0.,2. / (f - n),0.], 
                    [-((r + l) / (r - l)),-((t + b) / (t - b)),-((f + n) / (f - n)),1.],
                ])
            }

            Projection::Perspective { fov } => {
                let cot = 1. / (fov * 0.5).tan();

                let (sx, sy) = if aspect >= 1. { (cot / aspect, cot) } else { (cot, cot * aspect) };

                RtMatrix::from_cols_array_2d(&[
                    [sx,0.,0.,0.],
                    [0.,sy,0.,0.],
                    [0.,0.,f / (f - n),1.],
                    [0.,0.,(- f * n / (f - n)),0.],
                ])
            }
        }
    }
}

static G_STATE: Mutex<GraphicsState> = Mutex::new(GraphicsState::default());

/// A C string argument as a Rust &str ("" for a null pointer).
unsafe fn token(s: *const c_char) -> &'static str {
    if s.is_null() {
        ""
    } else {
        CStr::from_ptr(s).to_str().unwrap_or("")
    }
}

#[derive(Clone)]
struct Sample {
    depth: RtFloat,
    color: RtColor,
    opacity: RtColor,
}

impl Sample {
    fn default() -> Self {
        Sample { 
            depth: f32::INFINITY, 
            color: Vec3::ZERO,
            opacity: Vec3::ZERO,
        }
    }
}

static SAMPLE_BUF: Mutex<Option<Array2<Vec<Sample>>>> = Mutex::new(None);

#[inline]
fn get_samples_index(x: usize, y: usize, w: usize) -> usize {
    x + y * w
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
    *o = Options::default();

    let mut o = G_STATE.lock().unwrap();
    *o = GraphicsState::default();
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
    o.xsamples = xsamples.max(1) as usize;
    o.ysamples = ysamples.max(1) as usize;
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
pub extern "C" fn RiWorldBegin() {
    let mut g = G_STATE.lock().unwrap();
    g.camera_transform = g.transformation_curr.clone();
    g.transformation_curr = RtMatrix::IDENTITY;
    g.transformation_stack.clear();

    let (w, h) = {
        let o = OPTIONS.lock().unwrap();
        (o.xres as usize, o.yres as usize)
    };

    let o = OPTIONS.lock().unwrap();

    *SAMPLE_BUF.lock().unwrap() = Some(Array2::from_elem((w * o.xsamples, h * o.ysamples), Vec::new()));
}

#[no_mangle]
pub extern "C" fn RiWorldEnd() {
    // TODO: your pixels. For now, a black, fully transparent image (or an
    // empty depth image, +infinity everywhere).
    let (w, h, depth, xsamples, ysamples) = {
        let o = OPTIONS.lock().unwrap();
        (o.xres as usize, o.yres as usize, o.depth_display, o.xsamples, o.ysamples)
    };

    let sample_count = xsamples * ysamples;

    let s = &SAMPLE_BUF.lock().unwrap();
    let samples = s.as_ref().unwrap();
    if depth {
        let mut depth_buf: Vec<RtFloat> = vec![0.; w * h];

        let pixels = samples.windows_with_stride((sample_count,sample_count), (xsamples, ysamples));
        for (pos, pixel) in pixels.into_iter().enumerate() {
            let mut depth = f32::INFINITY;
            for sample in pixel {
                if let Some(closest) = sample.get(0) {
                    if depth > closest.depth {
                        depth = closest.depth;
                    }
                }
            }

            depth_buf[pos] = depth;
        }

        write_depth(&depth_buf);
    } else {
        let mut screen_buf: Vec<RtFloat> = vec![0.; 4 * w * h];

        let pixels = samples.windows_with_stride((sample_count, sample_count), (xsamples, ysamples));
        // for (pos, pixel) in pixels.into_iter().enumerate() {
        for x in 0..w {
            for y in 0..h {
                let pos = (x * xsamples, y * ysamples);

                let mut color = Vec3::ZERO;
                let mut opacity = Vec3::ZERO;
                for i in 0..xsamples {
                    for j in 0..ysamples {
                        let sample = samples.get((pos.0 + i, pos.1 + j));
                        let Some(sample) = sample else { unreachable!(); };

                        let mut sample_color = Vec3::ZERO;
                        let mut sample_opacity = Vec3::ZERO;
                        for hit in sample {
                            if sample_opacity.abs_diff_eq(Vec3::ONE, 1e-6) {
                                break;
                            }
        
                            sample_color.x += (1. - sample_opacity.x) * hit.color.x * hit.opacity.x;
                            sample_color.y += (1. - sample_opacity.y) * hit.color.y * hit.opacity.y;
                            sample_color.z += (1. - sample_opacity.z) * hit.color.z * hit.opacity.z;
                            sample_opacity.x += (1. - sample_opacity.x) * hit.opacity.x;
                            sample_opacity.y += (1. - sample_opacity.y) * hit.opacity.y;
                            sample_opacity.z += (1. - sample_opacity.z) * hit.opacity.z;
                        }
        
                        color += sample_color;
                        opacity += sample_opacity;
                    }
                }
                color /= (xsamples * ysamples) as f32;
                opacity /= (xsamples * ysamples) as f32;
                
                let pos = x + y * w;

                screen_buf[4 * pos] = color.x;
                screen_buf[4 * pos + 1] = color.y;
                screen_buf[4 * pos + 2] = color.z;
                screen_buf[4 * pos + 3] = opacity.max_element();
            }
        }

        write_image(&screen_buf);
    }
}

/// RiProjection(RI_PERSPECTIVE, "fov", &fov), fov in degrees over the
/// shorter side of the image, or RiProjection(RI_ORTHOGRAPHIC, NULL, NULL):
/// the C++ scenes omit the last two arguments, which default to NULL. It
/// also resets the current transformation to the identity: the camera
/// transform is built after it (Section 2). value points at an RtFloat.
#[no_mangle]
pub unsafe extern "C" fn RiProjection(p_name: RtToken, p_token: RtToken, p_value: RtPointer) {
    let mut o = OPTIONS.lock().unwrap();
    match token(p_name) {
        RI_PERSPECTIVE => {
            o.projection = Projection::Perspective { fov: (*(p_value as *const f32)).to_radians() }
        }
        RI_ORTHOGRAPHIC => { o.projection = Projection::Orthographic; }
        err => panic!("Invalid Projection: {err}")
    };
    
    let mut g = G_STATE.lock().unwrap();
    g.transformation_curr = RtMatrix::IDENTITY;
    g.transformation_stack.clear();
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
    let mut o = G_STATE.lock().unwrap();
    let transformation_curr = o.transformation_curr.clone();
    o.transformation_stack.push(transformation_curr);
}

#[no_mangle]
pub extern "C" fn RiTransformEnd() {
    let mut o = G_STATE.lock().unwrap();
    let transformation = o.transformation_stack.pop().unwrap();
    o.transformation_curr = transformation;
}

#[no_mangle]
pub unsafe extern "C" fn RiConcatTransform(transform: *const [[RtFloat; 4]; 4]) {
    let mut o = G_STATE.lock().unwrap();
    let concat_transform = RtMatrix_from_ptr(transform);
    o.transformation_curr *= concat_transform
}

#[no_mangle]
pub extern "C" fn RiTranslate(dx: RtFloat, dy: RtFloat, dz: RtFloat) {
    let mut o = G_STATE.lock().unwrap();
    let translate_transform = RtMatrix::from_translation(Vec3::new(dx, dy, dz));
    o.transformation_curr *= translate_transform
}

#[no_mangle]
pub extern "C" fn RiRotate(theta: RtFloat, dx: RtFloat, dy: RtFloat, dz: RtFloat) {
    let mut o = G_STATE.lock().unwrap();
    let rotate_transform = RtMatrix::from_axis_angle(Vec3::new(dx, dy, dz).normalize(), theta.to_radians());
    o.transformation_curr *= rotate_transform
}

#[no_mangle]
pub extern "C" fn RiScale(sx: RtFloat, sy: RtFloat, sz: RtFloat) {
    let mut o = G_STATE.lock().unwrap();
    let scale_transform = RtMatrix::from_scale(Vec3::new(sx, sy, sz));
    o.transformation_curr *= scale_transform
}

pub fn RiApplyTransform(px: RtFloat, py: RtFloat, pz: RtFloat) -> RtVector {
    let o = G_STATE.lock().unwrap();
    o.transformation_curr.transform_point3(Vec3::new(px, py, pz))
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
    let g = G_STATE.lock().unwrap();
    let o = OPTIONS.lock().unwrap();

    let (phi_min, phi_max, theta_max) = (
        if zmin > -radius {f32::asin(zmin.clamp(-radius, radius) / radius)} else {f32::to_radians(-90.0)},
        if zmax < radius {f32::asin(zmax.clamp(-radius, radius) / radius)} else {f32::to_radians(90.0)},
        thetamax.to_radians()
    );

    let (umax, vmax) = (20, 30);

    let mut grid: MicroGrid = MicroGrid::new(umax, vmax);

    for _u in 0..=umax {
        for _v in 0..=vmax {
            let (u, v) = (_u as f32 / umax as f32, _v as f32 / vmax as f32);

            let (phi, theta) = (
                phi_min + v * (phi_max - phi_min),
                u * theta_max
            );
        
            let (x, y, z) = (
                radius * theta.cos() * phi.cos(),
                radius * theta.sin() * phi.cos(),
                radius * phi.sin(),
            );

            let vert = grid.get_vertex_mut(_u, _v);
            vert.p = Vec3::new(x, y, z);
            vert.c = g.color;
            vert.o = g.opacity;
            vert.u = u;
            vert.v = v;
        }
    }

    grid.shade(&g, &o);
    grid.sample(&g, &o);
}

#[no_mangle]
pub extern "C" fn RiCylinder(radius: RtFloat, zmin: RtFloat, zmax: RtFloat, theta_max: RtFloat) {
    let g = G_STATE.lock().unwrap();
    let o = OPTIONS.lock().unwrap();

    let (umax, vmax) = (200, 300);

    let mut grid: MicroGrid = MicroGrid::new(umax, vmax);

    for _u in 0..=umax {
        for _v in 0..=vmax {
            let (u, v) = (_u as f32 / umax as f32, _v as f32 / vmax as f32);

            let theta = u * theta_max.to_radians();
        
            let (x, y, z) = (
                radius * theta.cos(),
                radius * theta.sin(),
                zmin + v * (zmax - zmin),
            );

            let vert = grid.get_vertex_mut(_u, _v);
            vert.p = Vec3::new(x, y, z);
            vert.c = g.color;
            vert.o = g.opacity;
            vert.u = u;
            vert.v = v;
        }
    }

    grid.shade(&g, &o);
    grid.sample(&g, &o);
}

#[no_mangle]
pub extern "C" fn RiCone(height: RtFloat, radius: RtFloat, theta_max: RtFloat) {
    let g = G_STATE.lock().unwrap();
    let o = OPTIONS.lock().unwrap();

    let (umax, vmax) = (20, 30);

    let mut grid: MicroGrid = MicroGrid::new(umax, vmax);

    for _u in 0..=umax {
        for _v in 0..=vmax {
            let (u, v) = (_u as f32 / umax as f32, _v as f32 / vmax as f32);

            let theta = u * theta_max.to_radians();
        
            let (x, y, z) = (
                radius * theta.cos() * (1. - v),
                radius * theta.sin() * (1. - v),
                v * height,
            );

            let vert = grid.get_vertex_mut(_u, _v);
            vert.p = Vec3::new(x, y, z);
            vert.c = g.color;
            vert.o = g.opacity;
            vert.u = u;
            vert.v = v;
        }
    }

    grid.shade(&g, &o);
    grid.sample(&g, &o);
}

#[no_mangle]
pub extern "C" fn RiTorus(majorradius: RtFloat, minorradius: RtFloat, phimin: RtFloat, phimax: RtFloat, theta_max: RtFloat) {
    let g = G_STATE.lock().unwrap();
    let o = OPTIONS.lock().unwrap();

    let (umax, vmax) = (100, 200);

    let mut grid: MicroGrid = MicroGrid::new(umax, vmax);

    for _u in 0..=umax {
        for _v in 0..=vmax {
            let (u, v) = (_u as f32 / umax as f32, _v as f32 / vmax as f32);

            let (theta, phi) = (
                (u * theta_max).to_radians(),
                (phimin + v * (phimax - phimin)).to_radians(),
            );

            let r = minorradius * phi.cos();
        
            let (x, y, z) = (
                (majorradius + r) * theta.sin(),
                (majorradius + r) * theta.cos(),
                minorradius * phi.sin(),
            );

            let vert = grid.get_vertex_mut(_u, _v);
            vert.p = Vec3::new(x, y, z);
            vert.c = g.color;
            vert.o = g.opacity;
            vert.u = u;
            vert.v = v;
        }
    }

    grid.shade(&g, &o);
    grid.sample(&g, &o);
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
pub unsafe extern "C" fn RiColor(color: *const [RtFloat; 3]) {
    let mut g = G_STATE.lock().unwrap();
    g.color = Vec3::new((*color)[0], (*color)[1], (*color)[2]);
}

#[no_mangle]
pub unsafe extern "C" fn RiOpacity(opacity: *const [RtFloat; 3]) {
    let mut g = G_STATE.lock().unwrap();
    g.opacity = Vec3::new((*opacity)[0], (*opacity)[1], (*opacity)[2]);;
}

/// Bind a shader to the primitives that follow. Copy nbytes of params now
/// (std::slice::from_raw_parts(params as *const u8, nbytes).to_vec()): the
/// scene's struct may be gone by the time the shader runs. A None shader
/// means STANDARD for RiSurface and no displacement for RiDisplacement
/// (Section 5).
#[no_mangle]
pub unsafe extern "C" fn RiSurface(fn_: RtShaderFn, params: *const c_void, nbytes: usize) {
    let mut g = G_STATE.lock().unwrap();

    match fn_ {
        Some(_) => { g.shader_fn = fn_ }
        None => {
            g.shader_fn = Some(crate::shaders::STANDARD)
        }
    };

    if !params.is_null() {
        let params = std::slice::from_raw_parts(params as *const u8, nbytes);
        g.shader_params = Some(params.into());
    }
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
