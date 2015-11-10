// view.rs -- ray generation
// Copyright (C) 2015 Alex Iadicicco

use cgmath::*;
use std::f32::consts;
use render::trace;
use rand;
use toml;

/// A trait representing some conversion of 2 dimensional screen space
/// coordinates to 3 dimensional Rays.
pub trait View: Send + Sync {
    /// uv should be in the range [-1,1]^2. The user may "crop" this range to
    /// scale it to the output, but points outside the range should not be
    /// provided.
    fn get_ray(&self, tracer: &trace::Tracer, uv: Vector2<f32>) -> trace::Ray;
}

/// A rectilinear projection. This is the projection most people are familiar
/// with. The view is modeled as a frustum converging toward a position in
/// space. It is initially facing in the -Z direction, but Euler angles may be
/// provided to rotate the view toward a particular orientation. The field of
/// view parameter determines the angle inside the frustum along its longest
/// axis. An aperture radius and focal point may be provided to simulate
/// depth-of-field blurring. The focal plane is parallel to the image plane, and
/// contains the focal point.
pub struct Rectilinear {
    pos: Vector3<f32>,
    look: Matrix3<f32>,

    f: f32,

    aper: f32,
    foc: Vector3<f32>,
}

impl Rectilinear {
    /// Creates a new rectilinear view with the given parameters.
    pub fn new(
        px: f32, py: f32, pz: f32,
        rx: f32, ry: f32, rz: f32,
        fov: f32, aper: f32,
        fx: f32, fy: f32, fz: f32,
    ) -> Rectilinear {
        Rectilinear {
            pos: Vector3::new(px, py, pz),
            look: Matrix3::from_euler(
                Rad { s: rx.to_radians() },
                Rad { s: ry.to_radians() },
                Rad { s: rz.to_radians() },
            ),

            f: (fov / 2.0).to_radians().tan(),

            aper: aper,
            foc: Vector3::new(fx, fy, fz),
        }
    }

    /// Creates a rectilinear view from the given TOML value.
    pub fn from_toml(conf: &toml::Value) -> Rectilinear {
        Rectilinear::new(
            conf.lookup("position.0").unwrap().as_float().unwrap() as f32,
            conf.lookup("position.1").unwrap().as_float().unwrap() as f32,
            conf.lookup("position.2").unwrap().as_float().unwrap() as f32,

            conf.lookup("rotation.0").unwrap().as_float().unwrap() as f32,
            conf.lookup("rotation.1").unwrap().as_float().unwrap() as f32,
            conf.lookup("rotation.2").unwrap().as_float().unwrap() as f32,

            conf.lookup("fov").unwrap().as_float().unwrap() as f32,
            conf.lookup("aperture").unwrap().as_float().unwrap_or(0.0) as f32,

            conf.lookup("focal.0").unwrap().as_float().unwrap_or(0.0) as f32,
            conf.lookup("focal.1").unwrap().as_float().unwrap_or(0.0) as f32,
            conf.lookup("focal.2").unwrap().as_float().unwrap_or(0.0) as f32,
        )
    }
}

impl View for Rectilinear {
    fn get_ray(&self, tracer: &trace::Tracer, uv: Vector2<f32>) -> trace::Ray {
        let p0 = self.pos;
        let v = self.look.mul_v(&Vector3::new(
            uv.x * self.f,
            uv.y * self.f,
            -1.0
        ).normalize());

        // determine where this initial ray intersects the focal plane. chances
        // are there's a much easier way to compute this, but I'm doing the way
        // I know works
        let t = {
            let n = self.look.mul_v(&Vector3::new(0.0, 0.0, 1.0));
            (self.foc - p0).dot(&n) / v.dot(&n)
        };

        // now we pick a random spot on our bokeh, which is just a plain circle
        // at the moment, and orient it to be parallel to the lens. this
        // randomness is specifically evenly weighted across the area of a
        // circle.
        let bokeh = {
            let r = rand::random::<f32>().sqrt();
            let th = 2.0 * consts::PI * rand::random::<f32>();

            self.look.mul_v(&Vector3::new(
                r * th.cos() * self.aper,
                r * th.sin() * self.aper,
                0.0
            ))
        };

        // then we displace the ray origin by the bokeh amount while keeping the
        // ray vector pointed at the right spot on the focal plane. lucky for
        // us, the simplified result is cheap!
        trace::Ray::import(
            tracer,
            p0 + bokeh,
            (v.mul_s(t) - bokeh).normalize()
        )
    }
}

/// An equirectangular panoramic projection centered at a point. UV parameters
/// are mapped to 360 degrees in both directions. In other words, (-1, 0) would
/// be 180 degrees left, 0 degrees up, and (1, -1) would be 180 degrees right,
/// 180 degrees down. Clearly, for a canonical equirectangular projection,
/// parameters should only be given in the range (-1, -.5) to (1, .5).
pub struct Equirectangular {
    pos: Vector3<f32>
}

impl Equirectangular {
    /// Creates a new equirectangular view, centered at the given point.
    pub fn new(px: f32, py: f32, pz: f32) -> Equirectangular {
        Equirectangular {
            pos: Vector3::new(px, py, pz),
        }
    }

    /// Loads the given TOML value into an equirectangular view.
    pub fn from_toml(conf: &toml::Value) -> Equirectangular {
        Equirectangular::new(
            conf.lookup("position.0").unwrap().as_float().unwrap() as f32,
            conf.lookup("position.1").unwrap().as_float().unwrap() as f32,
            conf.lookup("position.2").unwrap().as_float().unwrap() as f32,
        )
    }
}

impl View for Equirectangular {
    fn get_ray(&self, tracer: &trace::Tracer, uv: Vector2<f32>) -> trace::Ray {
        let az = uv.x * consts::PI;
        let el = uv.y * consts::PI;

        let p0 = self.pos;
        let v = Vector3::new(
            el.cos() * az.sin(),
            el.sin(),
            el.cos() * -az.cos(),
        );

        trace::Ray::import(tracer, p0, v)
    }
}
