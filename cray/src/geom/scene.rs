// scene.rs -- tracer-friendly scene data
// Copyright (C) 2015 Alex Iadicicco

#![allow(non_snake_case)]

//! Ray tracer scene data.

use geom::fast;
use geom::kd;
use load::obj;
use std::simd::f32x4;

/// A vertex
pub type Vertex = fast::Vec4;
/// A color .... hacky.
pub type Color = fast::Vec4;
/// A vertex or face normal
pub type Normal = fast::Vec4;

/// A `SceneBuilder` is the easiest way to build a `Scene`, especially if
/// loading OBJ data.
pub struct SceneBuilder {
    vp: Vec<Vertex>,
    vn_acc: Vec<Normal>,

    vc: Vec<Color>,

    vn: Vec<Normal>,

    f: Vec<Face>,
    fo: Vec<FaceOptions>,

    dropped_faces: usize,

    use_color: bool,
}

struct FaceOptions {
    smooth: bool,
    use_normal: [Result<usize, usize>; 3],
    use_color: [Option<usize>; 3],
}

/// A face!
#[derive(Debug)]
pub struct Face {
    /// The position of the 3 vertices of the face
    pub vp: [Vertex; 3],
    /// The colors of the vertices. This is a color representation of given
    /// texture coordinates, or white otherwise.
    pub vc: [Color; 3],
    /// The normals of the vertices. If the OBJ file specifies normals, those
    /// are used. Otherwise, these are computed by averaging adjacent face
    /// normals.
    pub vn: [Normal; 3],

    /// The face normal.
    pub n: Normal,

    /// The axis plane where projecting this face results in the largest area.
    /// Used for computing barycentric coordinates.
    pub plane: fast::Plane,

    /// A property of this face used for barycentric coordinate conversion. This
    /// is the inverse of T = ( r1 - r3, r2 - r3 ) where r1..3 are the vertices
    /// of the triangle. Multiplying this by ( x - r3 ) will result in a column
    /// vector containing the first 2 barycentric coords, from which the third
    /// can be derived by subtraction.
    pub Tinv: fast::Mat2x2,
}

pub struct Scene {
    pub f: Vec<Face>,
    pub kd: kd::KD,
}

impl SceneBuilder {
    /// Constructs a new `SceneBuilder`
    pub fn new() -> SceneBuilder {
        SceneBuilder {
            vp: Vec::new(),
            vn_acc: Vec::new(),

            vc: Vec::new(),

            vn: Vec::new(),

            f: Vec::new(),
            fo: Vec::new(),

            dropped_faces: 0,

            use_color: true,
        }
    }

    /// Consumes this `SceneBuilder`, creating a `Scene`. The BVH is built at
    /// this time.
    pub fn into_scene(self) -> Scene {
        if self.dropped_faces > 0 {
            println!("warning: dropped {} faces during load (probably \
                      collinear vertices)", self.dropped_faces);
        }

        println!("building k-d tree...");
        let face_names = (0..self.f.len()).collect();
        let kd = kd::KD::build(face_names, &self);

        Scene {
            f: self.f,
            kd: kd
        }
    }
}

impl obj::Importer for SceneBuilder {
    fn finalize(&mut self) {
        for vn in self.vn_acc.iter_mut() {
            *vn = fast::v_normalize(*vn);
        }

        for (i, f) in self.f.iter_mut().enumerate() {
            let opt = &self.fo[i];

            for i in 0..3 {
                f.vn[i] = match opt.use_normal[i] {
                    Ok(ni) => self.vn[ni],
                    Err(vi) => if opt.smooth {
                        self.vn_acc[vi]
                    } else {
                        f.n
                    }
                };

                if self.use_color {
                    f.vc[i] = match opt.use_color[i] {
                        Some(i) => self.vc[i],
                        None => f32x4(1.0, 1.0, 1.0, 0.0),
                    };
                }
            }
        }
    }

    fn add_vertex(&mut self, vp: obj::Vertex) {
        self.vp.push(f32x4(vp.x, vp.y, vp.z, 0.0));
        self.vn_acc.push(f32x4(0.0, 0.0, 0.0, 0.0));
    }

    fn add_texcoord(&mut self, vt: obj::TexCoord) {
        self.vc.push(f32x4(vt.x, vt.y, vt.z, 0.0));
    }

    fn add_normal(&mut self, vn: obj::Normal) {
        self.vn.push(fast::v_normalize(f32x4(vn.x, vn.y, vn.z, 0.0)));
    }

    fn add_face(&mut self, fin: [obj::FaceVertex; 3]) {
        if let Some(fout) = Face::new([
            self.vp[fin[0].p],
            self.vp[fin[1].p],
            self.vp[fin[2].p],
        ]) {
            self.vn_acc[fin[0].p] += fout.n;
            self.vn_acc[fin[1].p] += fout.n;
            self.vn_acc[fin[2].p] += fout.n;
            self.f.push(fout);
            self.fo.push(FaceOptions {
                smooth: true,
                use_normal: [
                    fin[0].n.ok_or(fin[0].p),
                    fin[1].n.ok_or(fin[1].p),
                    fin[2].n.ok_or(fin[2].p),
                ],
                use_color: [
                    fin[0].t,
                    fin[1].t,
                    fin[2].t
                ],
            });
        } else {
            self.dropped_faces += 1;
        }
    }
}

impl kd::Decoder for SceneBuilder {
    fn vertices(&self, i: usize) -> &[fast::Vec4] {
        &self.f[i].vp
    }
}

impl Face {
    fn new(vp: [Vertex; 3]) -> Option<Face> {
        // first we determine the plane where the area of the triangle is
        // largest. this improves accuracy in hit tests, and is necessary to
        // ensure we don't pick a plane perpendicular to the face, where its
        // area would be 0.

        let e02 = vp[0] - vp[2];
        let e12 = vp[1] - vp[2];

        let plane = {
            let aXY = fast::Plane::XY.area(e02, e12);
            let aYZ = fast::Plane::YZ.area(e02, e12);
            let aXZ = fast::Plane::XZ.area(e02, e12);

            if aXY >= aYZ && aXY >= aXZ {
                fast::Plane::XY
            } else if aYZ >= aXZ {
                fast::Plane::YZ
            } else {
                fast::Plane::XZ
            }
        };

        // then we can compute T and its inverse, used in computing barycentric
        // coordinates of the triangle when projected in the plane chosen above

        let Tinv = {
            let c0 = plane.proj(vp[0]);
            let c1 = plane.proj(vp[1]);
            let c2 = plane.proj(vp[2]);

            let f32x4(a, c, _, _) = c0 - c2;
            let f32x4(b, d, _, _) = c1 - c2;

            let det = a * d - b * c;

            if det == 0.0 {
                return None;
            }

            f32x4(d/det, -b/det, -c/det, a/det)
        };

        // then we compute the face normal and can return success

        let c = f32x4(1.0, 1.0, 1.0, 0.0);
        let n = fast::v_normalize(fast::v_cross_v(e02, e12));

        Some(Face {
            vp: vp,
            vc: [c, c, c],
            vn: [n, n, n],

            n: n,

            plane: plane,
            Tinv: Tinv,
        })
    }
}
