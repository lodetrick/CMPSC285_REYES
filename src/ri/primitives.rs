use core::slice;

use crate::ri::{
    map_to_space, micro_grid::MicroGrid, RtFloat, RtInt, RtMatrix, RtNormal, RtPoint, G_STATE,
    OPTIONS,
};
use glam::{Vec2, Vec3, Vec4Swizzles};

/* ---------------------------------------------------------------- primitives */

const DICE_MAX: usize = 1024;

macro_rules! add_bounding_points {
    ($dicer:expr, $x_max:expr, $x_min:expr, $y_max:expr, $y_min:expr, $z_max:expr, $z_min:expr) => {
        $dicer.add_point(Vec3::new($x_max, $y_max, $z_max));
        $dicer.add_point(Vec3::new($x_max, $y_max, $z_min));
        $dicer.add_point(Vec3::new($x_max, $y_min, $z_max));
        $dicer.add_point(Vec3::new($x_max, $y_min, $z_min));
        $dicer.add_point(Vec3::new($x_min, $y_max, $z_max));
        $dicer.add_point(Vec3::new($x_min, $y_max, $z_min));
        $dicer.add_point(Vec3::new($x_min, $y_min, $z_max));
        $dicer.add_point(Vec3::new($x_min, $y_min, $z_min));
    };
}

struct Dicer {
    x_min: f32,
    x_max: f32,
    y_min: f32,
    y_max: f32,
    obj_to_screen: RtMatrix,
    w: f32,
    h: f32,
}

impl Dicer {
    fn new(obj_to_screen: RtMatrix, w: f32, h: f32) -> Self {
        Dicer {
            x_min: f32::INFINITY,
            x_max: 0.0,
            y_min: f32::INFINITY,
            y_max: 0.0,
            obj_to_screen,
            w,
            h,
        }
    }

    fn add_point(&mut self, p_: Vec3) {
        let p = map_to_space(&self.obj_to_screen, p_);
        let p = Vec2::new((p.x + 1.) * 0.5 * self.w, (1. - p.y) * 0.5 * self.h);

        self.x_min = self.x_min.min(p.x);
        self.x_max = self.x_max.max(p.x);
        self.y_min = self.y_min.min(p.y);
        self.y_max = self.y_max.max(p.y);
    }

	fn clip_primitive(&self) -> bool {
		self.x_max < 0.0 || self.x_min > self.w || self.y_max < 0.0 || self.y_min > self.h
	}

    fn get_dice_rate(&self, clamp_min: usize, clamp_max: usize) -> (usize, usize) {
        let diag = Vec2::new(self.x_max - self.x_min, self.y_max - self.y_min).length();
        let n = ((2.0 * diag).round() as usize).clamp(clamp_min, clamp_max);
        println!("Dice rate: {n} ({diag})");
        (n, n)
    }
}

fn uv_identity(u: f32, v: f32) -> Vec2 {
    Vec2::new(u, v)
}

fn render_primitive<F1, F2, F3>((umax, vmax): (usize, usize), pos_fn: F1, norm_fn: F2, st_fn: F3)
where
    F1: Fn(f32, f32) -> Vec3,
    F2: Fn(f32, f32) -> Vec3,
    F3: Fn(f32, f32) -> Vec2,
{
    // Grab the locks that we need.
    let g = G_STATE.lock().unwrap();
    let o = OPTIONS.lock().unwrap();
    let object_to_view = g.object_to_view();

    let mut grid: MicroGrid = MicroGrid::new(umax, vmax);

    for _v in 0..=vmax {
        for _u in 0..=umax {
            let (u, v) = (_u as f32 / umax as f32, _v as f32 / vmax as f32);

            let vert = grid.get_vertex_mut(_u, _v);
            let po = pos_fn(u, v);

            let n = object_to_view.try_inverse().unwrap().transpose() * norm_fn(u, v).extend(0.0);
            let ng = n.w.signum() * n.xyz().normalize();

            let uv = st_fn(u, v);

            vert.p = map_to_space(&object_to_view, po);
            vert.po = po;
            vert.ng = ng;
            vert.c = g.color;
            vert.o = g.opacity;
            vert.u = uv.x;
            vert.v = uv.y;
        }
    }

    grid.displace(&g);
    grid.shade(&g);
    grid.sample(&g, &o);
}

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
    println!("making a sphere");
    let (phi_min, phi_max, theta_max) = (
        if zmin > -radius {
            f32::asin(zmin.clamp(-radius, radius) / radius)
        } else {
            f32::to_radians(-90.0)
        },
        if zmax < radius {
            f32::asin(zmax.clamp(-radius, radius) / radius)
        } else {
            f32::to_radians(90.0)
        },
        thetamax.to_radians(),
    );

    let sphere_pos = |u: f32, v: f32| -> Vec3 {
        let (phi, theta) = (phi_min + v * (phi_max - phi_min), u * theta_max);

        Vec3::new(theta.cos() * phi.cos(), theta.sin() * phi.cos(), phi.sin())
    };

    let dice_amount = {
        let g = G_STATE.lock().unwrap();
        let o = OPTIONS.lock().unwrap();

        let mut dicer = Dicer::new(g.object_to_screen(&o), o.xres as f32, o.yres as f32);
        add_bounding_points!(dicer, radius, -radius, radius, -radius, zmax, zmin);

		if dicer.clip_primitive() {
			return; // Drop out-of-bounds primitive
		}

        dicer.get_dice_rate(4, DICE_MAX)
    };

    render_primitive(
        dice_amount,
        |u, v| -> Vec3 { radius * sphere_pos(u, v) },
        sphere_pos,
        uv_identity,
    );
}

#[no_mangle]
pub extern "C" fn RiCylinder(radius: RtFloat, zmin: RtFloat, zmax: RtFloat, theta_max: RtFloat) {
    println!("making a cylinder");
    let dice_amount = {
        let g = G_STATE.lock().unwrap();
        let o = OPTIONS.lock().unwrap();

        let mut dicer = Dicer::new(g.object_to_screen(&o), o.xres as f32, o.yres as f32);
        add_bounding_points!(dicer, radius, -radius, radius, -radius, zmax, zmin);

		if dicer.clip_primitive() {
			return; // Drop out-of-bounds primitive
		}

        dicer.get_dice_rate(4, DICE_MAX)
    };

    render_primitive(
        dice_amount,
        |u, v| -> Vec3 {
            let theta = u * theta_max.to_radians();

            Vec3::new(
                radius * theta.cos(),
                radius * theta.sin(),
                zmin + v * (zmax - zmin),
            )
        },
        |u, _v| -> Vec3 {
            let theta = u * theta_max.to_radians();

            Vec3::new(theta.cos(), theta.sin(), 0.0)
        },
        uv_identity,
    );
}

#[no_mangle]
pub extern "C" fn RiCone(height: RtFloat, radius: RtFloat, theta_max: RtFloat) {
    println!("making a cone");
    let dice_amount = {
        let g = G_STATE.lock().unwrap();
        let o = OPTIONS.lock().unwrap();

        let mut dicer = Dicer::new(g.object_to_screen(&o), o.xres as f32, o.yres as f32);
        add_bounding_points!(dicer, radius, -radius, radius, -radius, height, 0.0);

		if dicer.clip_primitive() {
			return; // Drop out-of-bounds primitive
		}

        dicer.get_dice_rate(4, DICE_MAX)
    };

    render_primitive(
        dice_amount,
        |u, v| -> Vec3 {
            let theta = u * theta_max.to_radians();

            Vec3::new(
                radius * theta.cos() * (1. - v),
                radius * theta.sin() * (1. - v),
                v * height,
            )
        },
        |u, _v| -> Vec3 {
            let theta = u * theta_max.to_radians();

            Vec3::new(height * theta.cos(), height * theta.sin(), radius).normalize()
        },
        uv_identity,
    );
}

#[no_mangle]
pub extern "C" fn RiTorus(
    majorradius: RtFloat,
    minorradius: RtFloat,
    phimin: RtFloat,
    phimax: RtFloat,
    theta_max: RtFloat,
) {
    println!("making a torus");
    let dice_amount = {
        let g = G_STATE.lock().unwrap();
        let o = OPTIONS.lock().unwrap();

        let mut dicer = Dicer::new(g.object_to_screen(&o), o.xres as f32, o.yres as f32);
        add_bounding_points!(
            dicer,
            majorradius + minorradius,
            -(majorradius + minorradius),
            majorradius + minorradius,
            -(majorradius + minorradius),
            minorradius,
            -minorradius
        );

		if dicer.clip_primitive() {
			return; // Drop out-of-bounds primitive
		}

        dicer.get_dice_rate(4, DICE_MAX)
    };

    render_primitive(
        dice_amount,
        |u, v| -> Vec3 {
            let (theta, phi) = (
                (u * theta_max).to_radians(),
                (phimin + v * (phimax - phimin)).to_radians(),
            );

            let r = minorradius * phi.cos();

            Vec3::new(
                (majorradius + r) * theta.cos(),
                (majorradius + r) * theta.sin(),
                minorradius * phi.sin(),
            )
        },
        |u, v| -> Vec3 {
            let (theta, phi) = (
                (u * theta_max).to_radians(),
                (phimin + v * (phimax - phimin)).to_radians(),
            );

            Vec3::new(phi.cos() * theta.cos(), phi.cos() * theta.sin(), phi.sin())
        },
        uv_identity,
    );
}

fn render_tri(
    dice_amount: (usize, usize),
    verts: &[RtInt],
    P: &[RtPoint],
    N: &Option<&[RtPoint]>,
    st: &Option<&[RtFloat]>,
) {
    let (p0, p1, p2) = (
        P[verts[0] as usize],
        P[verts[1] as usize],
        P[verts[2] as usize],
    );

    // let Some(N) = N else {
    //     return;
    // };
    // let (n0, n1, n2) = (
    //     N[verts[0] as usize],
    //     N[verts[1] as usize],
    //     N[verts[2] as usize],
    // );

    // let st_fn: dyn Fn(f32, f32) -> Vec2 = if let Some(st) = st {
    //     let (st0, st1, st2) = (
    //         Vec2::new(st[verts[0] as usize], st[verts[0] as usize + 1]),
    //         Vec2::new(st[verts[1] as usize], st[verts[1] as usize + 1]),
    //         Vec2::new(st[verts[2] as usize], st[verts[2] as usize + 1]),
    //     );
    //     |u, v| -> Vec2 { (1.0 - v) * ((1.0 - u) * st0 + u * st1) + v * st2 }
    // } else {
    //     uv_identity
    // };

    // let Some(st) = st else {
    //     return;
    // };

    // let (st0, st1, st2) = (
    //     Vec2::new(st[2 * verts[0] as usize], st[2 * verts[0] as usize + 1]),
    //     Vec2::new(st[2 * verts[1] as usize], st[2 * verts[1] as usize + 1]),
    //     Vec2::new(st[2 * verts[2] as usize], st[2 * verts[2] as usize + 1]),
    // );

    render_primitive(
        dice_amount,
        |u, v| -> Vec3 { (1.0 - v) * ((1.0 - u) * p0 + u * p1) + v * p2 },
        |u, v| -> Vec3 { Vec3::new(0., 0., -1.) },
        // |u, v| -> Vec3 { (1.0 - v) * ((1.0 - u) * n0 + u * n1) + v * n2 },
        uv_identity,
        // |u, v| -> Vec2 { (1.0 - v) * ((1.0 - u) * st0 + u * st1) + v * st2 },
    );
}

fn render_quad(
    dice_amount: (usize, usize),
    verts: &[RtInt],
    P: &[RtPoint],
    N: &Option<&[RtPoint]>,
    st: &Option<&[RtFloat]>,
) {
    let (p0, p1, p2, p3) = (
        P[verts[0] as usize],
        P[verts[1] as usize],
        P[verts[2] as usize],
        P[verts[3] as usize],
    );

    // let Some(st) = st else {
    //     return;
    // };

    // let (st0, st1, st2, st3) = (
    //     Vec2::new(st[2 * verts[0] as usize], st[2 * verts[0] as usize + 1]),
    //     Vec2::new(st[2 * verts[1] as usize], st[2 * verts[1] as usize + 1]),
    //     Vec2::new(st[2 * verts[2] as usize], st[2 * verts[2] as usize + 1]),
    //     Vec2::new(st[2 * verts[3] as usize], st[2 * verts[3] as usize + 1]),
    // );

    render_primitive(
        dice_amount,
        |u, v| -> Vec3 {
            (1.0 - u) * (1.0 - v) * p0 + u * (1.0 - v) * p1 + (1.0 - u) * v * p3 + u * v * p2
        },
        |u, v| -> Vec3 { Vec3::new(0., 0., -1.) },
        // |u, v| -> Vec3 { (1.0 - v) * ((1.0 - u) * n0 + u * n1) + v * n2 },
        uv_identity,
        // |u, v| -> Vec2 {
        //     (1.0 - u) * (1.0 - v) * st0 + u * (1.0 - v) * st1 + (1.0 - u) * v * st3 + u * v * st2
        // },
    );
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
    let nverts = slice::from_raw_parts(nverts, npolys as usize);
    let total_vertices = nverts.iter().sum::<i32>() as usize;
    let verts = slice::from_raw_parts(verts, total_vertices);
    let P = slice::from_raw_parts(P, total_vertices);
    let N = if N.is_null() {
        None
    } else {
        Some(slice::from_raw_parts(N, total_vertices))
    };
    let st = if st.is_null() {
        None
    } else {
        Some(slice::from_raw_parts(st, 2 * total_vertices))
    };

    let (object_to_screen, w, h) = {
        let g = G_STATE.lock().unwrap();
        let o = OPTIONS.lock().unwrap();
        (&g.object_to_screen(&o), o.xres as f32, o.yres as f32)
    };

    let mut tri_begin: usize = 0;

	let mut start = Instant::now();
    for (i, nverts_) in nverts.iter().enumerate() {
        println!("Rendering Polygon [{} / {}]", i + 1, nverts.len());
		if i % 100 == 0 {
			let end = Instant::now() - start;
			start = Instant::now();
			println!("100 polygons took {:#?}", end);
		}
        let nverts = *nverts_ as usize;

        if nverts == 3 {
            let dice_amount = {
                let mut dicer = Dicer::new(*object_to_screen, w, h);
                for vert in &verts[tri_begin..(tri_begin + nverts)] {
                    dicer.add_point(P[*vert as usize]);
                }

				if dicer.clip_primitive() {
					continue; // Drop out-of-bounds primitive
				}

                dicer.get_dice_rate(2, DICE_MAX)
            };

            render_tri(
                dice_amount,
                &verts[tri_begin..(tri_begin + nverts)],
                P,
                &N,
                &st,
            );
        } else if nverts == 4 {
            let dice_amount = {
                let mut dicer = Dicer::new(*object_to_screen, w, h);
                for vert in &verts[tri_begin..(tri_begin + nverts)] {
                    dicer.add_point(P[*vert as usize]);
                }

				if dicer.clip_primitive() {
					continue; // Drop out-of-bounds primitive
				}

                dicer.get_dice_rate(2, DICE_MAX)
            };

            render_quad(
                dice_amount,
                &verts[tri_begin..(tri_begin + nverts)],
                P,
                &N,
                &st,
            );
        }

        tri_begin += nverts;
    }
}
