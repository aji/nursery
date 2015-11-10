// lib.rs -- cray root
// Copyright (C) 2015 Alex Iadicicco

#![feature(float_extras)]
#![feature(simd, core_simd)]
#![feature(asm)]

extern crate cgmath;
extern crate rand;
extern crate time;
extern crate toml;

pub mod distr;
pub mod geom;
pub mod load;
pub mod render;
pub mod util;
