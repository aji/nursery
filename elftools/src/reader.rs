// Tools for manipulating ELF files
// Copyright (C) 2015-present Alex Iadicicco <http://ajitek.net>

//! Functions for loading `format` structures from different sources.
//! At present, only types with `Reader+Seek` traits are supported as a source.

use std::io;
use std::mem;
use std::num::FromPrimitive;

use format::*;

// Static sizes for the different headers. This is kind of bad, but at present
// I don't trust the compiler to allocate the format::* sizes correctly!
#[allow(dead_code)] static ELF32_EHDR_SIZE: usize = 52;
#[allow(dead_code)] static ELF32_SHDR_SIZE: usize = 40;

enum EndianReader<R> { BE(R), LE(R) }

#[allow(dead_code)]
impl<R: Reader + Seek> EndianReader<R> {
    fn back(&mut self) -> &mut R {
        match *self {
            EndianReader::BE(ref mut r) => r,
            EndianReader::LE(ref mut r) => r,
        }
    }

    fn read_u32(&mut self) -> io::Result<u32> {
        match *self {
            EndianReader::BE(ref mut r) => r.read_be_u32(),
            EndianReader::LE(ref mut r) => r.read_le_u32(),
        }
    }

    fn read_u16(&mut self) -> io::Result<u16> {
        match *self {
            EndianReader::BE(ref mut r) => r.read_be_u16(),
            EndianReader::LE(ref mut r) => r.read_le_u16(),
        }
    }

    fn read_i32(&mut self) -> io::Result<i32> {
        match *self {
            EndianReader::BE(ref mut r) => r.read_be_i32(),
            EndianReader::LE(ref mut r) => r.read_le_i32(),
        }
    }

    fn read_i16(&mut self) -> io::Result<i16> {
        match *self {
            EndianReader::BE(ref mut r) => r.read_be_i16(),
            EndianReader::LE(ref mut r) => r.read_le_i16(),
        }
    }

    fn read_u8 (&mut self) -> io::Result<u8> { self.back().read_u8() }
    fn read_i8 (&mut self) -> io::Result<i8> { self.back().read_i8() }

    fn seek(&mut self, pos: i64, style: SeekStyle) -> io::Result<()> {
        self.back().seek(pos, style)
    }
}

pub struct ElfReader<R> {
    back: EndianReader<R>,
}

impl<R: Reader + Seek> ElfReader<R> {
    fn from(back: EndianReader<R>) -> ElfReader<R> {
        ElfReader {
            back: back
        }
    }

    /// Creates a new ElfReader from the given backing input. Also performs
    /// basic input header checks. This is necessary, as the reader needs
    /// to know the endianness of the input file.
    pub fn new(mut back: R) -> io::Result<ElfReader<R>> {
        try!(back.seek(0, SeekStyle::SeekSet));

        let ident = try!(back.read_exact(EI_NIDENT as usize));

        if  ELFMAG[0] != ident[0] ||
            ELFMAG[1] != ident[1] ||
            ELFMAG[2] != ident[2] ||
            ELFMAG[3] != ident[3] {
            return Err(IoError {
                kind:    IoErrorKind::InvalidInput,
                desc:    "missing magic number",
                detail:  None,
            });
        }

        if ident[EI_CLASS as usize] != ELFCLASS32 as u8 {
            return Err(IoError {
                kind:    IoErrorKind::InvalidInput,
                desc:    "can only read 32-bit ELF input currently",
                detail:  None,
            });
        }

        if ident[EI_VERSION as usize] != EV_CURRENT as u8 {
            return Err(IoError {
                kind:    IoErrorKind::InvalidInput,
                desc:    "version header does not match",
                detail:  None,
            });
        }

        match ELFDATA::from_u8(ident[EI_DATA as usize]) {
            Some(ELFDATA2LSB) => Ok(ElfReader::from(EndianReader::LE(back))),
            Some(ELFDATA2MSB) => Ok(ElfReader::from(EndianReader::BE(back))),
            _ => Err(IoError {
                kind:    IoErrorKind::InvalidInput,
                desc:    "EI_DATA field has unknown value",
                detail:  None,
            }),
        }
    }

    /// Reads the ELF file header. No position option is given because the
    /// ELF header is always at the beginning of the file.
    pub fn ehdr(&mut self) -> io::Result<Elf32_Ehdr> {
        try!(self.back.seek(0, SeekStyle::SeekSet));

        let mut h: Elf32_Ehdr = unsafe { mem::uninitialized() };

        for i in range(0, EI_NIDENT as usize) {
            h.e_ident[i] = try!(self.back.read_u8());
        }

        h.e_type        = try!(self.back.read_u16());
        h.e_machine     = try!(self.back.read_u16());
        h.e_version     = try!(self.back.read_u32());
        h.e_entry       = try!(self.back.read_u32());
        h.e_phoff       = try!(self.back.read_u32());
        h.e_shoff       = try!(self.back.read_u32());
        h.e_flags       = try!(self.back.read_u32());
        h.e_ehsize      = try!(self.back.read_u16());
        h.e_phentsize   = try!(self.back.read_u16());
        h.e_phnum       = try!(self.back.read_u16());
        h.e_shentsize   = try!(self.back.read_u16());
        h.e_shnum       = try!(self.back.read_u16());
        h.e_shstrndx    = try!(self.back.read_u16());

        return Ok(h);
    }

    /// Reads an ELF section header from the given location in the input.
    pub fn shdr(&mut self, at: i64) -> io::Result<Elf32_Shdr> {
        try!(self.back.seek(at, SeekStyle::SeekSet));

        let mut h: Elf32_Shdr = unsafe { mem::uninitialized() };

        h.sh_name       = try!(self.back.read_u32());
        h.sh_type       = try!(self.back.read_u32());
        h.sh_flags      = try!(self.back.read_u32());
        h.sh_addr       = try!(self.back.read_u32());
        h.sh_offset     = try!(self.back.read_u32());
        h.sh_size       = try!(self.back.read_u32());
        h.sh_link       = try!(self.back.read_u32());
        h.sh_info       = try!(self.back.read_u32());
        h.sh_addralign  = try!(self.back.read_u32());
        h.sh_entsize    = try!(self.back.read_u32());

        return Ok(h);
    }

    /// Reads a symbol table entry from the given location in the input.
    pub fn sym(&mut self, at: i64) -> io::Result<Elf32_Sym> {
        try!(self.back.seek(at, SeekStyle::SeekSet));

        let mut s: Elf32_Sym = unsafe { mem::uninitialized() };

        s.st_name       = try!(self.back.read_u32());
        s.st_value      = try!(self.back.read_u32());
        s.st_size       = try!(self.back.read_u32());
        s.st_info       = try!(self.back.read_u8());
        s.st_other      = try!(self.back.read_u8());
        s.st_shndx      = try!(self.back.read_u16());

        return Ok(s);
    }

    /// Reads an implicit addend relocation entry from the given location
    /// in the input
    pub fn rel(&mut self, at: i64) -> io::Result<Elf32_Rel> {
        try!(self.back.seek(at, SeekStyle::SeekSet));

        let mut r: Elf32_Rel = unsafe { mem::uninitialized() };

        r.r_offset      = try!(self.back.read_u32());
        r.r_info        = try!(self.back.read_u32());

        return Ok(r);
    }

    /// Reads an explicit addend relocation entry from the given location
    /// in the input
    pub fn rela(&mut self, at: i64) -> io::Result<Elf32_Rela> {
        try!(self.back.seek(at, SeekStyle::SeekSet));

        let mut r: Elf32_Rela = unsafe { mem::uninitialized() };

        r.r_offset      = try!(self.back.read_u32());
        r.r_info        = try!(self.back.read_u32());
        r.r_addend      = try!(self.back.read_i32());

        return Ok(r);
    }

    /// Reads a program header table entry from the given location in 
    /// the input
    pub fn phdr(&mut self, at: i64) -> io::Result<Elf32_Phdr> {
        try!(self.back.seek(at, SeekStyle::SeekSet));

        let mut p: Elf32_Phdr = unsafe { mem::uninitialized() };

        p.p_type        = try!(self.back.read_u32());
        p.p_offset      = try!(self.back.read_u32());
        p.p_vaddr       = try!(self.back.read_u32());
        p.p_paddr       = try!(self.back.read_u32());
        p.p_filesz      = try!(self.back.read_u32());
        p.p_memsz       = try!(self.back.read_u32());
        p.p_flags       = try!(self.back.read_u32());
        p.p_align       = try!(self.back.read_u32());

        return Ok(p);
    }
}
