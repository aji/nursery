// obj.rs -- Wavefront OBJ reader
// Copyright (C) 2015 Alex Iadicicco

//! Wavefront OBJ reader.
//!
//! This module functions as little more than a parser. The parsed data is
//! passed to an `Importer` which performs actual storage.

use std::io;
use std::str::FromStr;
use cgmath;

/// A vertex record
pub type Vertex = cgmath::Vector4<f32>;
/// A texture coordinate record
pub type TexCoord = cgmath::Vector3<f32>;
/// A vertex normal record
pub type Normal = cgmath::Vector3<f32>;

/// A single vertex in a face record
#[derive(Copy, Clone)]
pub struct FaceVertex {
    /// The index of the corresponding vertex position record
    pub p: usize,
    /// The index of the corresponding texture coordinate record, if one was
    /// provided
    pub t: Option<usize>,
    /// The index of the corresponding vertex normal record, if one was
    /// provided.
    pub n: Option<usize>,
}

/// The `obj` module is not capable of building vertices in memory by itself.
/// The data parsed out of the OBJ file is handed to the `Importer` provided
/// to the call to `read`
pub trait Importer {
    /// Called when `read` is finished to do things like calculate vertex
    /// normals.
    fn finalize(&mut self);

    /// Indicates a vertex position record
    fn add_vertex(&mut self, vp: Vertex);
    /// Indicates a texture coordinate record
    fn add_texcoord(&mut self, vt: TexCoord);
    /// Indicates a vertex normal record
    fn add_normal(&mut self, vn: Normal);

    /// Indicates a face record, with the given face vertices. 4 vertex faces
    /// are broken up into two 3 vertex faces during load. This limitation is
    /// not considered high priority.
    fn add_face(&mut self, f: [FaceVertex; 3]);
}

struct ReaderContext {
    nvert: usize,
    ntexc: usize,
    nnorm: usize,

    ntri: usize,
}

/// Reads the given `BufRead` as a Wavefront OBJ file, sending directives to the
/// given `Importer`.
pub fn read<B, I>(input: B, tgt: &mut I) -> io::Result<()>
where B: io::BufRead, I: Importer {
    let mut rctx = ReaderContext {
        nvert: 0,
        ntexc: 0,
        nnorm: 0,

        ntri: 0
    };

    println!("reading obj...");

    for ln_ in input.lines() {
        let ln = try!(ln_);
        let fields: Vec<&str> = ln
            .split(|c| c == '#' || c == '\r')
            .nth(0)
            .unwrap_or(&"")
            .split(' ')
            .filter(|s| s.len() > 0)
            .collect();

        if fields.len() == 0 {
            continue;
        }

        match fields[0] {
            "v" => {
                rctx.nvert += 1;
                tgt.add_vertex(parse_vertex(&fields[1..]));
            },

            "vt" => {
                rctx.ntexc += 1;
                tgt.add_texcoord(parse_texcoord(&fields[1..]));
            },

            "vn" => {
                rctx.nnorm += 1;
                tgt.add_normal(parse_normal(&fields[1..]));
            },

            "f" => {
                match fields.len() - 1 {
                    3 => {
                        let v1 = parse_face_vertex(&rctx, fields[1]);
                        let v2 = parse_face_vertex(&rctx, fields[2]);
                        let v3 = parse_face_vertex(&rctx, fields[3]);
                        tgt.add_face([v1, v2, v3]);
                        rctx.ntri += 1;
                    },

                    4 => {
                        let v1 = parse_face_vertex(&rctx, fields[1]);
                        let v2 = parse_face_vertex(&rctx, fields[2]);
                        let v3 = parse_face_vertex(&rctx, fields[3]);
                        let v4 = parse_face_vertex(&rctx, fields[4]);
                        tgt.add_face([v1, v2, v3]);
                        tgt.add_face([v3, v4, v1]);
                        rctx.ntri += 2;
                    },

                    _ => { println!("skipping invalid face"); }
                }

            },

            "g" | "o" | "s" | "mtllib" | "usemtl" => { },

            _ => {
                println!("unknown .obj directive: {}", ln);
            }
        }
    }

    println!("obj reader statistics:");
    println!("  {} vertices", rctx.nvert);
    println!("  {} normals", rctx.nnorm);
    println!("  {} triangles", rctx.ntri);

    tgt.finalize();

    Ok(())
}

fn parse_vertex(s: &[&str]) -> Vertex {
    cgmath::Vector4::new(
        FromStr::from_str(s.get(0).unwrap_or(&"0")).unwrap_or(0.0),
        FromStr::from_str(s.get(1).unwrap_or(&"0")).unwrap_or(0.0),
        FromStr::from_str(s.get(2).unwrap_or(&"0")).unwrap_or(0.0),
        FromStr::from_str(s.get(3).unwrap_or(&"1")).unwrap_or(1.0),
    )
}

fn parse_texcoord(s: &[&str]) -> TexCoord {
    cgmath::Vector3::new(
        FromStr::from_str(s.get(0).unwrap_or(&"0")).unwrap_or(0.0),
        FromStr::from_str(s.get(1).unwrap_or(&"0")).unwrap_or(0.0),
        FromStr::from_str(s.get(2).unwrap_or(&"0")).unwrap_or(0.0),
    )
}

fn parse_normal(s: &[&str]) -> Normal {
    cgmath::Vector3::new(
        FromStr::from_str(s.get(0).unwrap_or(&"0")).unwrap_or(0.0),
        FromStr::from_str(s.get(1).unwrap_or(&"0")).unwrap_or(0.0),
        FromStr::from_str(s.get(2).unwrap_or(&"0")).unwrap_or(0.0),
    )
}

fn decode_index(idx: isize, n: usize) -> usize {
    (if idx < 0 {
        idx + n as isize
    } else {
        idx - 1
    }) as usize
}

fn parse_face_vertex(rctx: &ReaderContext, s: &str) -> FaceVertex {
    let mut f = FaceVertex {
        p: 0,
        t: None,
        n: None,
    };

    for (i, s) in s.split('/').enumerate() {
        if s.len() == 0 {
            continue;
        }

        let v = match FromStr::from_str(s) {
            Ok(v) => v,
            Err(e) => panic!("parse error on {:?}: {}", s, e),
        };

        match i {
            0 => { f.p = decode_index(v, rctx.nvert); }
            1 => { f.t = Some(decode_index(v, rctx.ntexc)); }
            2 => { f.n = Some(decode_index(v, rctx.nnorm)); }

            _ => { },
        }
    }

    f
}
