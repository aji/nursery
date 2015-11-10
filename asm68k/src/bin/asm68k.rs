// 68000-family assembler driver program
// Copyright (C) 2015-present Alex Iadicicco <http://ajitek.net>

extern crate asm68k;

use std::io::BufferedReader;
use std::io::File;
use std::os;

use asm68k::parser::Parser;

fn main() {
    let args = os::args();

    if args.len() < 2 {
        panic!("missing input file argument");
    }

    let f = File::open(&Path::new(&args[1])).unwrap();
    let mut b = BufferedReader::new(f);

    for input in Parser::new(b.chars()) {
        println!("{:?}", input);
    }
}
