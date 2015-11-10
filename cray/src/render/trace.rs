// trace.rs -- raytracer core
// Copyright (C) 2015 Alex Iadicicco

//! Ray tracer core

use cgmath::{Vector3, Matrix};
use util::color::Color;
use geom::fast;
use geom::scene;
use rand;
use std;
use std::cell::Cell;
use std::simd::f32x4;

const M_2_PI: f32 = 6.283185307179586;

/// A ray in 3 dimensional space
pub struct Ray {
    /// The origin of the ray
    pub p0: fast::Vec4,
    /// A unit vector in the direction of the ray
    pub v: fast::Vec4,
    /// The component-wise reciprocal of v, useful for performing division by a
    /// component of v using only multiplication instructions.
    pub vdiv: fast::Vec4,

    /// The ID of this ray, in this thread
    pub id: u64,
}

/// A collision between a ray and some face in the scene.
#[derive(Copy, Clone)]
pub struct Collision {
    /// The distance from the ray origin to the collision point. This can be
    /// used to find the exact collision point.
    pub dist: f32,
    /// The name of the face the ray collided with.
    pub face: usize,
    /// The first two barycentric coordinates of the collision point in the
    /// face.
    pub bary: [f32x4; 2],
}

impl Ray {
    /// Creates a new `Ray` with the given origin and direction
    pub fn new(t: &Tracer, p0: fast::Vec4, v: fast::Vec4) -> Ray {
        let f32x4(vx, vy, vz, _) = fast::v_normalize(v);
        Ray {
            p0: p0,
            v: f32x4(vx, vy, vz, 0.0),
            vdiv: f32x4(1.0/vx, 1.0/vy, 1.0/vz, 0.0),
            id: t.get_next_id(),
        }
    }

    pub fn import(t: &Tracer, p0: Vector3<f32>, v: Vector3<f32>) -> Ray {
        Ray::new(
            t,
            f32x4(p0.x, p0.y, p0.z, 0.0),
            f32x4(v.x, v.y, v.z, 0.0)
        )
    }

    pub fn trace_any(&self, t: &Tracer, sc: &scene::Scene) -> bool {
        let mut coll = Collision {
            dist: std::f32::INFINITY,
            face: 0,
            bary: [
                fast::zero(),
                fast::zero(),
            ]
        };

        sc.kd.query(self, &t.last_ids, &mut |idx| {
            self.hit(sc, idx, &mut coll)
        })
    }

    /// Traces the ray through the scene, finding the closest collision point,
    /// if any.
    pub fn trace(&self, t: &Tracer, sc: &scene::Scene) -> Option<Collision> {
        let mut coll = Collision {
            dist: std::f32::INFINITY,
            face: 0,
            bary: [
                fast::zero(),
                fast::zero(),
            ]
        };

        sc.kd.query(self, &t.last_ids, &mut |idx| {
            self.hit(sc, idx, &mut coll);
            false
        });

        if coll.dist < std::f32::INFINITY {
            Some(coll)
        } else {
            None
        }
    }

    /// Decides whether the ray collides with the given face in some distance
    /// less than `max`. If not, the return value is `None`. Otherwise, a `Some`
    /// containing a `Collision` is returned.
    pub fn hit(
        &self,
        sc: &scene::Scene,
        fi: usize,
        coll: &mut Collision
    ) -> bool {
        let f = &sc.f[fi];

        let denom = fast::v_dot_v(self.v, f.n);

        if denom == 0.0 {
            return false;
        }

        let t = fast::v_dot_v(f.vp[2] - self.p0, f.n) / denom;

        if t < 0.0 || t > coll.dist {
            return false;
        }

        let p = f.plane.proj(self.p0 + self.v * fast::scalar(t) - f.vp[2]);
        let f32x4(q0, q1, _, _) = fast::m_mul_v(f.Tinv, p);

        if 0.0<=q0 && 0.0<=q1 && q0<=1.0 && q1<=1.0 && (q0+q1)<=1.0 {
            coll.dist = t;
            coll.face = fi;
            coll.bary = [
                fast::scalar(q0),
                fast::scalar(q1),
            ];
            true
        } else {
            false
        }
    }
}

/// A tracer configuration
pub struct Tracer {
    horizon: Color,
    max_depth: usize,
    sun_basis: [fast::Vec4; 3],
    next_id: Cell<u64>,
    last_ids: Vec<Cell<u64>>,
}

impl Tracer {
    /// Creates a new tracer with the given options.
    pub fn new(_: usize, max_depth: usize, nfaces: usize) -> Tracer {
        Tracer {
            horizon: Color::new(0.31, 0.41, 0.72),
            max_depth: max_depth,
            sun_basis: {
                //let sun_dir = fast::v_normalize(f32x4(-1.0, -0.7, -0.6, 0.0));
                let sun_dir = fast::v_normalize(f32x4(0.6, -2.0, -0.4, 0.0));
                let (b0, b1, b2) = basis_from_b1(sun_dir);
                [b0, b1, b2]
            },
            next_id: Cell::new(0),
            last_ids: (0..nfaces).map(|_| Cell::new(0)).collect(),
        }
    }

    pub fn get_next_id(&self) -> u64 {
        let id = self.next_id.get();
        self.next_id.set(id + 1);
        id
    }

    /// Computes the sum of the direct light hitting the surface at the given
    /// point and surface normal in space.
    pub fn direct_light(
        &self, sc: &scene::Scene,
        p: fast::Vec4, sn: fast::Vec4,
    ) -> Color {
        // constant sun atm
        let sun_radius = 0.5f32.to_radians();
        let sun_dir = {
            // only correct at small radiuses i think
            let r = rand::random::<f32>().sqrt();
            let th = (360.0 * rand::random::<f32>()).to_radians();
            let az = fast::scalar(sun_radius * r * th.cos());
            let el = fast::scalar(sun_radius * r * th.sin());
            fast::v_normalize(
                self.sun_basis[0] * az +
                self.sun_basis[1] +
                self.sun_basis[2] * el
            ) * fast::scalar(-1.0)
        };
        let sun_color = Color::new(0.75, 0.62, 0.41);
        let ambient = Color::new(0.0, 0.0, 0.0);

        // determine if the sun is hidden from p
        let to_sun = Ray::new(
            &self,
            p + sun_dir * fast::scalar(0.00001),
            sun_dir,
        );
        if to_sun.trace_any(&self, sc) {
            return ambient;
        }

        // otherwise, return the sun color, scaled by the dot product of the
        // surface normal and sun vector
        return ambient.mix(&sun_color, fast::v_dot_v(sn, sun_dir));
    }

    fn sample_real(&self, sc: &scene::Scene, r: &Ray, depth: usize) -> Sample {
        if depth > self.max_depth {
            return Sample::MaxDepth;
        }

        let c = match r.trace(&self, sc) {
            Some(c) => c,
            None => return Sample::Horizon,
        };

        let p = r.p0 + r.v * fast::scalar(c.dist);
        let f = &sc.f[c.face];
        let sn = {
            f.vn[0] * c.bary[0] +
            f.vn[1] * c.bary[1] +
            f.vn[2] * (fast::one() - c.bary[0] - c.bary[1])
        };
        let absorb = {
            f.vc[0] * c.bary[0] +
            f.vc[1] * c.bary[1] +
            f.vc[2] * (fast::one() - c.bary[0] - c.bary[1])
        };

        let indirect = {
            let iv = hemi_rand(sn);
            let fac = fast::v_dot_v(sn, iv) as f32;
            let r = Ray::new(&self, p + iv * fast::scalar(0.00001), iv);
            let c = self.sample_real(sc, &r, depth + 1).get_color(&self);
            Color::new(
                c.r * fac,
                c.g * fac,
                c.b * fac,
            )
        };

        let direct = self.direct_light(sc, p, sn);

        Sample::Hit(c, Color::new(
            absorb.0 * (direct.r + indirect.r),
            absorb.1 * (direct.g + indirect.g),
            absorb.2 * (direct.b + indirect.b),
        ))
    }

    /// Samples the scene, starting with the given ray, determining the color.
    pub fn sample(&self, sc: &scene::Scene, r: &Ray) -> Color {
        self.sample_real(sc, r, 1).get_color(&self)
    }
}

#[derive(Copy, Clone)]
enum Sample {
    Horizon,
    MaxDepth,
    Hit(Collision, Color)
}

impl Sample {
    fn get_color(&self, tr: &Tracer) -> Color {
        match *self {
            Sample::Horizon => tr.horizon,
            Sample::MaxDepth => Color::new(0.0, 0.0, 0.0),
            Sample::Hit(_, c) => c,
        }
    }
}

#[allow(dead_code)]
fn normal_color(n: &Vector3<f32>) -> Color {
    Color::new_f32(
        (n.x + 1.0) * 0.5,
        (n.y + 1.0) * 0.5,
        (n.z + 1.0) * 0.5
    )
}

fn basis_from_b1(b1: fast::Vec4) -> (fast::Vec4, fast::Vec4, fast::Vec4) {
    let b0 = fast::v_normalize(
        if b1.0 != 0.0 || b1.1 != 0.0 {
            f32x4(b1.1, -b1.0, 0.0, 0.0)
        } else {
            f32x4(0.0, -b1.2, b1.1, 0.0)
        }
    );

    let b2 = fast::v_cross_v(b0, b1);

    (b0, b1, b2)
}

fn hemi_rand(b1: fast::Vec4) -> fast::Vec4 {
    let (b0, b1, b2) = basis_from_b1(b1);

    let az = M_2_PI * rand::random::<f32>();
    let sin_el = rand::random::<f32>() * 0.999;
    let el = sin_el.asin();

    let cos_el = el.cos();

    b0 * fast::scalar(cos_el * az.cos()) +
    b1 * fast::scalar(sin_el) +
    b2 * fast::scalar(cos_el * az.sin())
}
