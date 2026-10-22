use std::{ffi::c_void, ptr};

use glam::{Vec2, Vec3};
use rayon::prelude::*;

use crate::ri::{GraphicsState, Options, RtShaderContext, Triangle, Vertex, SAMPLE_BUF};

// Data structure for the micropolygon grid. It handles vertex displacement, shading, and creating triangles to be sampled.
pub struct MicroGrid {
    vertices: Vec<Vertex>,
    w: usize,
    h: usize,
}

impl MicroGrid {
    pub fn new(w: usize, h: usize) -> Self {
        MicroGrid {
            vertices: vec![
                Vertex::new(Vec3::ZERO, Vec3::ZERO, Vec3::ZERO, Vec3::ZERO, 0., 0.);
                (w + 1) * (h + 1)
            ],
            w,
            h,
        }
    }

    pub fn get_vertex(&self, x: usize, y: usize) -> &Vertex {
        &self.vertices[x + y * (self.w + 1)]
    }

    pub fn get_vertex_mut(&mut self, x: usize, y: usize) -> &mut Vertex {
        &mut self.vertices[x + y * (self.w + 1)]
    }

    pub fn displace(&mut self, g: &GraphicsState) {
        let Some(displace_fn) = g.displace_fn else {
            return;
        };

        self.vertices.par_iter_mut().for_each(|vertex| {
            let params: *const c_void = if let Some(displace_params) = &g.displace_params {
                displace_params.as_ptr() as *const c_void
            } else {
                ptr::null()
            };

            let mut shader_context = RtShaderContext {
                P: vertex.p,   /* position of grid vertex in camera/eye space */
                Po: vertex.po, /* same point in object space */
                N: vertex.ng,  /* unit normal, recomputed after displacement */
                u: vertex.u,   /* surface parameters in [0,1], or a mesh's st */
                v: vertex.v,
                Cs: g.color, /* current RiColor and RiOpacity */
                Os: g.opacity,
                params: params,
                Ci: Vec3::ZERO, /* written by surface shaders */
                Oi: Vec3::ZERO,
            };

            unsafe {
                displace_fn(&mut shader_context);
            }

            vertex.p = shader_context.P;
        });

        self.renormalize();
    }

    fn renormalize(&mut self) {
        for u in 0..=self.w {
            for v in 0..=self.h {
                let dPdu = if u == self.w {
                    self.get_vertex(u, v).p - self.get_vertex(u - 1, v).p
                } else {
                    self.get_vertex(u + 1, v).p - self.get_vertex(u, v).p
                };

                let dPdv = if v == self.h {
                    self.get_vertex(u, v).p - self.get_vertex(u, v - 1).p
                } else {
                    self.get_vertex(u, v + 1).p - self.get_vertex(u, v).p
                };

                let vertex = self.get_vertex_mut(u, v);
                let cross = dPdu.cross(dPdv);
                if !(dPdu.abs_diff_eq(Vec3::ZERO, f32::EPSILON) || dPdv.abs_diff_eq(Vec3::ZERO, f32::EPSILON)) {
                    vertex.ng = cross.normalize();
                }
            }
        }
    }

    fn flip_normals(&mut self) {
        self.vertices.par_iter_mut().for_each(|vertex| {
            if vertex.ng.dot(vertex.p) > 0.0 {
                vertex.ng = -vertex.ng;
            }
        });
    }

    pub fn shade(&mut self, g: &GraphicsState) {
        self.flip_normals();

        self.vertices.par_iter_mut().for_each(|vertex| {
            let params = if let Some(shader_params) = &g.shader_params {
                shader_params.as_ptr() as *const c_void
            } else {
                ptr::null()
            };

            let mut shader_context = RtShaderContext {
                P: vertex.p,   /* position of grid vertex in camera/eye space */
                Po: vertex.po, /* same point in object space */
                N: vertex.ng,  /* unit normal, recomputed after displacement */
                u: vertex.u,   /* surface parameters in [0,1], or a mesh's st */
                v: vertex.v,
                Cs: g.color, /* current RiColor and RiOpacity */
                Os: g.opacity,
                params: params,
                Ci: Vec3::ZERO, /* written by surface shaders */
                Oi: Vec3::ZERO,
            };

            if let Some(shader_fn) = g.shader_fn {
                unsafe {
                    shader_fn(&mut shader_context);
                }
            } else {
                unsafe {
                    crate::shaders::STANDARD(&mut shader_context);
                }
            }

            // vertex.c = glam::Vec3Swizzles::zzz(vertex.p) / 30.0;
            vertex.c = shader_context.Ci;
            vertex.o = shader_context.Oi;
            // vertex.c = (shader_context.N + Vec3::ONE) * 0.5;
            // vertex.o = Vec3::ONE;
        });
    }

    pub fn splat(&self, g: &GraphicsState, o: &Options) {
        let s = &SAMPLE_BUF.lock().unwrap();
        let s = s.as_ref().unwrap();
        let view_to_screen = g.view_to_screen(&o);
        let (w, h, xs, ys) = (o.xres as f32, o.yres as f32, o.xsamples, o.ysamples);

        self.vertices.par_iter().for_each(|vertex| {
            // Transform object coordinates to NDC coordinates
            let p1 = super::map_to_space(&view_to_screen, vertex.p);
            let r1 = Vec2::new((p1.x + 1.) * 0.5 * w, (1. - p1.y) * 0.5 * h);

            // Don't render tris outside of the screen.
            if r1.x < 0. || r1.x > w || r1.y < 0. || r1.y > h {
                return;
            }

            let sample = s.get((xs * r1.x.floor() as usize, ys * r1.y.floor() as usize));
            let Some(sample) = sample else {
                return;
            };

            let hit_info = super::Sample {
                depth: vertex.p.z,
                color: vertex.c,
                opacity: vertex.o * ((xs * ys) as f32),
            };
            sample.depths.lock().unwrap().push(hit_info);
        });
    }

    pub fn sample(&mut self, g: &GraphicsState, o: &Options) {
        let s = &SAMPLE_BUF.lock().unwrap();
        let s = s.as_ref().unwrap();
        let view_to_screen = g.view_to_screen(&o);
        let (w, h, xs, ys) = (o.xres as f32, o.yres as f32, o.xsamples, o.ysamples);

        (0..(self.w * self.h)).into_par_iter().for_each(|i| {
            let u = i % self.w;
            let v = (i - u) / self.w;

            let v_tl: &Vertex = self.get_vertex(u, v);
            let v_tr: &Vertex = self.get_vertex(u + 1, v);
            let v_bl: &Vertex = self.get_vertex(u, v + 1);
            let v_br: &Vertex = self.get_vertex(u + 1, v + 1);

            let (tri1, tri2) = (
                Triangle::new(v_tl, v_tr, v_bl),
                Triangle::new(v_bl, v_tr, v_br),
            );

            let z_tl = v_tl.p.z;
            let z_tr = v_tr.p.z;
            let z_bl = v_bl.p.z;
            let z_br = v_br.p.z;

            if z_tl.min(z_tr.min(z_bl)) > 0.5 {
                tri1.rasterize(&view_to_screen, &s, w, h, xs, ys);
            }

            if z_bl.min(z_tl.min(z_br)) > 0.5 {
                tri2.rasterize(&view_to_screen, &s, w, h, xs, ys);
            }
        });
    }
}
