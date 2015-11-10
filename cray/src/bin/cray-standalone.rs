// main.rs -- entry point
// Copyright (C) 2015 Alex Iadicicco

//! deprecated, to be deleted.

#![feature(step_by)]
#![feature(simd)]
#![feature(asm)]

extern crate cgmath;
extern crate rand;
extern crate time;
extern crate toml;

use std::fs;
use std::io;
use std::thread;
use std::path::Path;
use std::sync::{Arc, Mutex};
use std::sync::mpsc;

extern crate cray;

use cray::geom::scene;
use cray::load::obj;
use cray::render::trace;
use cray::render::view;
use cray::util::color;

use cray::render::view::View;

struct Config {
    sc: scene::Scene,

    threads: usize,
    cols: usize,
    rows: usize,
    samples: usize,
    max_depth: usize,
    branch: usize,
    gamma: f32,

    v: Box<View>
}

impl Config {
    fn load(root: &Path, conf: toml::Value) -> Result<Config, &'static str> {
        let v = {
            let view_toml = match conf.lookup("view") {
                Some(v@&toml::Value::Table(_)) => v,
                Some(_) => return Err("'view' should be a table"),
                _ => return Err("'view' missing"),
            };

            let typ = match view_toml.lookup("type") {
                Some(&toml::Value::String(ref s)) => s,
                Some(_) => return Err("'view.type' should be a string"),
                _ => return Err("'view.type' missing"),
            };

            match &typ[..] {
                "rectilinear" =>
                    Box::new(view::Rectilinear::from_toml(view_toml))
                            as Box<View>,
                "equirectangular" =>
                    Box::new(view::Equirectangular::from_toml(view_toml))
                            as Box<View>,
                _ => return Err("unknown view type")
            }
        };

        let threads = match conf.lookup("threads") {
            Some(&toml::Value::Integer(i)) => i,
            Some(_) => return Err("'threads' should be an integer"),
            _ => {
                println!("warning: 'threads' defaulting to 1");
                1
            },
        } as usize;

        let cols = match conf.lookup("cols") {
            Some(&toml::Value::Integer(i)) => i,
            Some(_) => return Err("'cols' should be an integer"),
            _ => return Err("'cols' missing"),
        } as usize;

        let rows = match conf.lookup("rows") {
            Some(&toml::Value::Integer(i)) => i,
            Some(_) => return Err("'rows' should be an integer"),
            _ => return Err("'rows' missing"),
        } as usize;

        let samples = match conf.lookup("samples") {
            Some(&toml::Value::Integer(i)) => i,
            Some(_) => return Err("'samples' should be an integer"),
            _ => {
                println!("warning: 'samples' defaulting to 20");
                20
            },
        } as usize;

        let max_depth = match conf.lookup("max_depth") {
            Some(&toml::Value::Integer(i)) => i,
            Some(_) => return Err("'max_depth' should be an integer"),
            _ => {
                println!("warning: 'max_depth' defaulting to 4");
                4
            },
        } as usize;

        let branch = match conf.lookup("branch") {
            Some(&toml::Value::Integer(i)) => i,
            Some(_) => return Err("'branch' should be an integer"),
            _ => {
                println!("warning: 'branch' defaulting to 1");
                1
            },
        } as usize;

        let gamma = match conf.lookup("gamma") {
            Some(&toml::Value::Float(f)) => f,
            Some(_) => return Err("'gamma' should be a floating point value"),
            _ => {
                println!("note: 'gamma' defaulting to 1.0");
                1.0
            },
        } as f32;

        let sc = {
            let path = match conf.lookup("obj") {
                Some(&toml::Value::String(ref s)) => s,
                Some(_) => return Err("'obj' should be a string"),
                _ => return Err("'obj' missing"),
            };

            let f = match fs::File::open(root.join(path)) {
                Ok(f) => f,
                Err(_) => return Err("file open failed"),
            };

            let br = io::BufReader::new(f);
            let mut sb = scene::SceneBuilder::new();

            if let Err(_) = obj::read(br, &mut sb) {
                return Err(".obj load failed");
            }

            sb.into_scene()
        };

        Ok(Config {
            sc: sc,

            threads: threads,
            cols: cols,
            rows: rows,
            samples: samples,
            max_depth: max_depth,
            branch: branch,
            gamma: 1.0 / gamma,

            v: v
        })
    }
}

fn main() {
    let args: Vec<String> = std::env::args().collect();

    let conf_path = match args.get(1) {
        Some(path) => Path::new(path),
        None => {
            println!("usage: {} <scene.cray>", args[0]);
            panic!("missing required argument");
        }
    };

    let conf_str = match fs::File::open(&conf_path) {
        Ok(mut f) => {
            use std::io::Read;
            let mut conf = String::new();
            f.read_to_string(&mut conf).unwrap();
            conf
        },
        Err(e) => {
            println!("error opening file: {}", e);
            panic!("config read failed");
        }
    };

    let conf_toml = {
        let mut parser = toml::Parser::new(&conf_str[..]);
        match parser.parse() {
            Some(v) => v,
            None => {
                for e in parser.errors {
                    println!("toml: {:#?}", e);
                }
                panic!("config parse failed");
            }
        }
    };

    let conf_root = conf_path.parent().unwrap();
    let conf_toml = toml::Value::Table(conf_toml);
    let conf = Arc::new(match Config::load(conf_root, conf_toml) {
        Ok(c) => c,
        Err(e) => {
            println!("conf: {}", e);
            panic!("config load error");
        }
    });

    let out: Arc<Mutex<Vec<Vec<color::Color>>>>
        = Arc::new(Mutex::new((0..conf.rows).map(|_| { Vec::new() }).collect()));
    let (tx, rx) = mpsc::channel();

    for i in 0..conf.threads {
        let out = out.clone();
        let tx = tx.clone();

        let conf = conf.clone();
        let tracer = trace::Tracer::new(
            conf.branch,
            conf.max_depth,
            conf.sc.f.len(),
        );

        thread::spawn(move || {
            let rs = conf.rows as isize;
            let cs = conf.cols as isize;
            let scale = (rs as f32).max(cs as f32);

            for row in (i..conf.rows).step_by(conf.threads) {
                let ri = row as isize;
                let mut pixels = Vec::with_capacity(conf.cols);

                for col in 0..conf.cols {
                    let ci = col as isize;
                    let uv = cgmath::Vector2::new(
                        ((2 * ci - cs) as f32) / scale,
                        ((rs - 2 * ri) as f32) / scale,
                    );

                    let mut rtot: f32 = 0.0;
                    let mut gtot: f32 = 0.0;
                    let mut btot: f32 = 0.0;

                    for _ in 0..(conf.samples) {
                        let r = conf.v.get_ray(
                            &tracer,
                            uv + cgmath::Vector2::new(
                                2.0 * rand::random::<f32>() / scale,
                                2.0 * rand::random::<f32>() / scale,
                            )
                        );
                        let c = tracer.sample(&conf.sc, &r);

                        rtot += c.r;
                        gtot += c.g;
                        btot += c.b;
                    }

                    pixels.push(color::Color::new(
                        (rtot / (conf.samples as f32)).powf(conf.gamma),
                        (gtot / (conf.samples as f32)).powf(conf.gamma),
                        (btot / (conf.samples as f32)).powf(conf.gamma)
                    ))
                }
                out.lock().unwrap()[row] = pixels;
                let _ = tx.send(());
            }
        });
    }

    let tm_start = time::precise_time_ns();
    for i in 0..(conf.rows+1) {
        let elapsed = time::precise_time_ns() - tm_start;
        let ratio = (i as f32) / (conf.rows as f32);
        let left = (elapsed as f32) * (1.0 - ratio) / ratio;
        println!(
            " {:6.2}% {:5}/{} rows, {:8.1}s remaining",
            100.0 * ratio,
            i, conf.rows,
            left / 1e9
        );
        if i < conf.rows {
            let _ = rx.recv();
        }
    }

    println!("writing ppm output...");

    if let Ok(out) = out.lock() {
        if let Ok(fraw) = fs::File::create("cray.ppm") {
            use std::io::Write;

            let mut f = io::BufWriter::new(fraw);
            let _ = write!(f, "P3 {} {} 255\n", conf.cols, conf.rows);
            for row in out.iter() {
                for col in row.iter() {
                    let _ = write!(f, "{} ", col.r8());
                    let _ = write!(f, "{} ", col.g8());
                    let _ = write!(f, "{} ", col.b8());
                }
                let _ = write!(f, "\n");
            }
        } else {
            panic!("failed to open output file");
        }
    } else {
        panic!("failed to obtain lock on output");
    }

    drop(out);
}
