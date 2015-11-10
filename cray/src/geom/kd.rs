// kd.rs -- k-d tree
// Copyright (C) 2015 Alex Iadicicco

//! k-d tree.

use geom::fast;
use std;
use std::cell::Cell;
use std::simd::f32x4;
use render::trace::Ray;

#[derive(Copy, Clone, Debug)]
pub enum Partition { X(f32), Y(f32), Z(f32) }

impl Partition {
    fn next(self) -> Partition {
        match self {
            Partition::X(_) => Partition::Y(0.0),
            Partition::Y(_) => Partition::Z(0.0),
            Partition::Z(_) => Partition::X(0.0),
        }
    }

    fn set(&self, v: f32) -> Partition {
        match *self {
            Partition::X(_) => Partition::X(v),
            Partition::Y(_) => Partition::Y(v),
            Partition::Z(_) => Partition::Z(v),
        }
    }

    fn project(&self, f32x4(x, y, z, _): fast::Vec4) -> f32 {
        match *self {
            Partition::X(_) => x,
            Partition::Y(_) => y,
            Partition::Z(_) => z,
        }
    }

    fn hit(&self, p0: fast::Vec4, vdiv: fast::Vec4) -> f32 {
        match *self {
            Partition::X(x) => (x - p0.0) * vdiv.0,
            Partition::Y(y) => (y - p0.1) * vdiv.1,
            Partition::Z(z) => (z - p0.2) * vdiv.2,
        }
    }

    fn on_small_side(&self, f32x4(vx, vy, vz, _): fast::Vec4) -> bool {
        match *self {
            Partition::X(x) => vx < x,
            Partition::Y(y) => vy < y,
            Partition::Z(z) => vz < z,
        }
    }
}

pub trait Decoder {
    fn vertices(&self, i: usize) -> &[fast::Vec4];
}

pub enum KD {
    Inner(Partition, Box<KD>, Box<KD>),
    Leaf(Vec<usize>),
}

impl KD {
    fn build_real<D: Decoder>(faces: Vec<usize>, d: &D, ax: Partition) -> KD {
        // edge case, small number of faces, make it one leaf
        if faces.len() <= 4 {
            return KD::Leaf(faces);
        }

        /*let ax = {
            let mut x0 = 0.0f32;
            let mut x1 = 0.0f32;
            let mut y0 = 0.0f32;
            let mut y1 = 0.0f32;
            let mut z0 = 0.0f32;
            let mut z1 = 0.0f32;
            for i in faces.iter() {
                let vp = d.vertices(*i);
                x0 = x0.min(vp[0].x).min(vp[1].x).min(vp[2].x);
                x1 = x1.max(vp[0].x).max(vp[1].x).max(vp[2].x);
                y0 = y0.min(vp[0].y).min(vp[1].y).min(vp[2].y);
                y1 = y1.max(vp[0].y).max(vp[1].y).max(vp[2].y);
                z0 = z0.min(vp[0].z).min(vp[1].z).min(vp[2].z);
                z1 = z1.max(vp[0].z).max(vp[1].z).max(vp[2].z);
            }
            let dx = x1 - x0;
            let dy = y1 - y0;
            let dz = z1 - z0;
            if dx > dy && dx > dz {
                Partition::X(0.0)
            } else if dy > dz {
                Partition::Y(0.0)
            } else {
                Partition::Z(0.0)
            }
        };*/

        let p = ax.set({
            let mut sum = 0.0;
            let mut count = 0;
            for i in faces.iter() {
                let vp = d.vertices(*i);
                sum += ax.project(vp[0]);
                sum += ax.project(vp[1]);
                sum += ax.project(vp[2]);
                count += 3;
            }
            sum / (count as f32)
        });

        let mut less = Vec::new();
        let mut more = Vec::new();

        let mut on_bound = 0;

        for i in faces.iter() {
            let vp = d.vertices(*i);
            let s0 = p.on_small_side(vp[0]);
            let s1 = p.on_small_side(vp[1]);
            let s2 = p.on_small_side(vp[2]);
            if s0 || s1 || s2 {
                less.push(*i);
            }
            if !s0 || !s1 || !s2 {
                more.push(*i);
                if s0 || s1 || s2 {
                    on_bound += 1;
                }
            }
        }

        if false //2 * on_bound >= faces.len()
                || less.len() == faces.len()
                || more.len() == faces.len() {
            KD::Leaf(faces)
        } else {
            drop(faces);
            KD::Inner(
                p,
                Box::new(KD::build_real(less, d, ax.next())),
                Box::new(KD::build_real(more, d, ax.next()))
            )
        }
    }

    pub fn build<D: Decoder>(faces: Vec<usize>, d: &D) -> KD {
        KD::build_real(faces, d, Partition::X(0.0))
    }

    fn query_real<F>(
        &self,
        p0: fast::Vec4,
        v: fast::Vec4,
        vdiv: fast::Vec4,
        id: u64,
        ids: &Vec<Cell<u64>>,
        cb: &mut F,
        tmax: f32
    ) -> bool
    where F: FnMut(usize) -> bool {
        match *self {
            KD::Leaf(ref v) => {
                for idx in v.iter() {
                    if ids[*idx].get() < id {
                        ids[*idx].set(id);
                        if cb(*idx) {
                            return true;
                        }
                    }
                }

                false
            }

            KD::Inner(p, ref left, ref right) => {
                let t = p.hit(p0, v);

                if t > 0.0 {
                    // ray segment crosses boundary
                    let p1 = p0 + v * fast::scalar(t);
                    if p.on_small_side(p0) {
                        left.query_real(p0, v, vdiv, id, ids, cb, t) ||
                        right.query_real(p1, v, vdiv, id, ids, cb, tmax - t)
                    } else {
                        left.query_real(p1, v, vdiv, id, ids, cb, tmax - t) ||
                        right.query_real(p0, v, vdiv, id, ids, cb, t)
                    }
                } else {
                    if p.on_small_side(p0) {
                        left.query_real(p0, v, vdiv, id, ids, cb, t)
                    } else {
                        right.query_real(p0, v, vdiv, id, ids, cb, t)
                    }
                }
            }
        }
    }

    pub fn query<F>(&self, r: &Ray, ids: &Vec<Cell<u64>>, cb: &mut F) -> bool
    where F: FnMut(usize) -> bool {
        self.query_real(r.p0, r.v, r.vdiv, r.id, ids, cb, std::f32::INFINITY)
    }
}
