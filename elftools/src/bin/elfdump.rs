extern crate elftools;

use std::os;
use std::io::fs::File;

use elftools::elf;

trait Dumpable {
    fn dump(&self);
}

impl<'a> Dumpable for elf::File<'a> {
    fn dump(&self) {
        println!("ELF file");
        println!("  type: {:?}", self.typ);
        println!("  data format: {:?}", self.datafmt);
        println!("  machine: {:?}", self.mach);
        println!("  entry: {:?}", match self.entry {
            Some(x)  => format!("{}", x),
            None     => format!("unspecified")
        });
    }
}

fn main() {
    let args = os::args();

    if args.len() < 2 {
        panic!("missing input file argument");
    }

    let p = Path::new(&args[1]);
    let f = File::open(&p).unwrap();

    let mut r = elftools::reader::ElfReader::new(f).unwrap();
    let ef = elf::File::load(&mut r).unwrap();

    ef.dump();
}
