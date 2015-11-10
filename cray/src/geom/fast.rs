// fast.rs -- fast operations
// Copyright (C) 2015 Alex Iadicicco

//! Various fast operations, using SIMD things.

use std::simd::f32x4;

pub type Vec4 = f32x4;
pub type Vec2 = f32x4;
pub type Mat2x2 = f32x4; // row major

#[inline]
pub fn scalar(x: f32) -> Vec4 {
    f32x4(x, x, x, x)
}

#[inline]
pub fn zero() -> Vec4 {
    scalar(0.0)
}

#[inline]
pub fn one() -> Vec4 {
    scalar(1.0)
}

#[inline]
pub fn v_len(mut v: Vec4) -> f32 {
    v = v * v;
    (v.0 + v.1 + v.2 + v.3).sqrt()
}

#[inline]
pub fn v_normalize(v: Vec4) -> Vec4 {
    let n = v_len(v);
    v / f32x4(n, n, n, n)
}

#[inline]
pub fn v_cross_v(f32x4(u1, u2, u3, u4): Vec4, f32x4(v1, v2, v3, v4): Vec4) -> Vec4 {
    f32x4(u2, u3, u1, u4) *
    f32x4(v3, v1, v2, v4) -
    f32x4(u3, u1, u2, u4) *
    f32x4(v2, v3, v1, v4)
}

#[inline]
pub fn v_dot_v(mut u: Vec4, v: Vec4) -> f32 {
    u = u * v;
    u.0 + u.1 + u.2 + u.3
}

#[inline]
pub fn m_mul_v(m: Mat2x2, v: Vec2) -> Vec2 {
    let f32x4(a, b, c, d) = m * v;

    f32x4(a, c, a, c) +
    f32x4(b, d, b, d)
}

#[derive(Copy, Clone, Debug)]
pub enum Plane {
    XY,
    YZ,
    XZ,
}

impl Plane {
    pub fn proj(&self, f32x4(x, y, z, _): Vec4) -> Vec2 {
        match *self {
            Plane::XY => f32x4(x, y, x, y),
            Plane::YZ => f32x4(y, z, y, z),
            Plane::XZ => f32x4(x, z, x, z),
        }
    }

    pub fn area(&self, v1: Vec4, v2: Vec4) -> f32 {
        let f32x4(u1x, u1y, _, _) = self.proj(v1);
        let f32x4(u2x, u2y, _, _) = self.proj(v2);

        (u1x * u2y - u1y * u2x).abs()
    }
}
