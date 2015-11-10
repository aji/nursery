// Tools for manipulating ELF files
// Copyright (C) 2015-present Alex Iadicicco <http://ajitek.net>

//! Rust-like structures and definitions for ELF files, from a logical
//! standpoint, as well as code to load and save them. The code in this file
//! does not give the user much control over the final layout of the bytes
//! in the file, so user be warned.

use std::collections::HashMap;
use std::io;
use std::num::FromPrimitive;

use format;
use reader::ElfReader;

#[stable] impl Copy for FileType { }
#[stable] impl Copy for DataFormat { }
#[stable] impl Copy for Machine { }
#[stable] impl Copy for Relocation { }

/// Top level structure for an ELF file. Main output or input of the load and
/// save routines.
pub struct File<'a> {
    /// The type of file represented
    pub typ:           FileType,

    /// Data format (endianness)
    pub datafmt:       DataFormat,

    /// The machine the file targets
    pub mach:          Machine,

    /// The entry point address, if one is provided
    pub entry:         Option<u32>,

    /// Map of sections in the file, by name.
    pub sects:         HashMap<&'a str, Box<Section<'a>>>,
}

/// Different types an ELF file can be.
#[unstable="enumeration members not fixed"]
#[derive(Show)]
pub enum FileType {
    #[stable] #[doc="The file has no type"]        NoType,
    #[stable] #[doc="A relocatable file"]          Relocatable,
    #[stable] #[doc="An executable file"]          Executable,
    #[stable] #[doc="A shared object file"]        SharedObject,
    #[stable] #[doc="A core dump"]                 Core,

    #[doc="Unknown file type; value copied literally"]
    Unknown(u16)
}

/// Data format (endianness)
#[stable]
#[derive(Show)]
pub enum DataFormat {
    #[stable] LittleEndian,
    #[stable] BigEndian,
}

/// The architecture for a file
#[unstable="enumeration members not fixed"]
#[derive(Show)]
pub enum Machine {
    #[stable] #[doc="No machine type specified"]   NoMachine,
    #[stable] #[doc="AT&T WE 32100"]               WE32100,
    #[stable] #[doc="SPARC, duh"]                  SPARC,
    #[stable] #[doc="Intel 80386"]                 I80386,
    #[stable] #[doc="Motorola 68000"]              M68000,
    #[stable] #[doc="Motorala 88000"]              M88000,
    #[stable] #[doc="Intel 80860"]                 I80860,
    #[stable] #[doc="MIPS RS3000"]                 RS3000,

    #[doc="Unknown machine type; value copied literally"]
    Unknown(u16)
}

/// Section header. There's one for every section in the file.
///
/// Header fields not represented here are either calculated based on the
/// content of other fields, or are arguments of the `typ` file.
pub struct Section<'a> {
    /// The name of a section, if known. Is a reference to a string in the
    /// string table.
    pub name:          Option<&'a String>,

    /// The type of the section. Includes a type-specific representation of
    /// the data contained in the section and section header.
    pub typ:           SectionType<'a>,

    /// The address of the section, if it is to appear in memory. Otherwise,
    /// None.
    pub addr:          Option<u32>,
}

/// Identifies the type of section, and contains a type-specific representation
/// of the data contained within it.
#[unstable="enumeration members not fixed"]
pub enum SectionType<'a> {
    /// Holds arbitrary information whose meaning is determined solely by
    /// the program.
    #[stable] ProgBits(Vec<u8>),

    /// A section of this type has no data, but otherwise resembles `ProgBits`.
    #[stable] NoBits,

    /// Holds a string table. An object file may have multiple s tring tables.
    #[stable] StrTab(Vec<String>),

    /// Holds a symbol table. Typically provides symbols for link editing.
    #[stable] SymTab(Vec<Symbol<'a>>),

    /// Holds a minimal set of dynamic linking symbols, to save space.
    #[stable] DynSym(Vec<Symbol<'a>>),

    /// Holds relocation entries with explicit addends. An object file may
    /// have multiple relocation sections.
    #[stable] Rela(Vec<Relocation>),

    /// Holds relocation entries without explicit addends.
    #[stable] Rel(Vec<Relocation>),

    /// Unknown section type; value copied literally
    Unknown(u16)
}

/// A reference to a section. Used in symbol definitions to determine the
/// section a symbol resides in. Some enumeration variants have special
/// meanings, and do not necessarily refer to a specific symbol.
#[unstable="enumeration members not fixed"]
pub enum SectionRef<'a> {
    /// Marks an undefined, irrelevant, or otherwise meaningless section.
    #[stable] Undefined,

    /// Refers to a section
    #[stable] Section(&'a Section<'a>),

    /// Specfiies an absolute value for the corresponding reference
    #[stable] Absolute,

    /// Symbols defined relative to `Common` are common symbols
    #[stable] Common,

    /// Invalid section reference; value copied literally
    Unknown(u16)
}

pub struct Symbol<'a> {
    pub name:          Option<&'a String>,
    pub value:         u32,
    pub size:          u32,
    pub info:          u8,
    pub sec:           SectionRef<'a>,
}

pub struct Relocation {
    pub offset:        u32,
    pub info:          u32,
    pub addend:        Option<i32>,
}

impl<'a> File<'a> {
    pub fn load<R: Reader + Seek>(r: &mut ElfReader<R>) -> io::Result<File<'a>> {
        let ehdr = try!(r.ehdr());

        let mut file = File {
            typ: match FromPrimitive::from_u16(ehdr.e_type) {
                Some(format::ET_NONE)      => FileType::NoType,
                Some(format::ET_REL)       => FileType::Relocatable,
                Some(format::ET_EXEC)      => FileType::Executable,
                Some(format::ET_DYN)       => FileType::SharedObject,
                Some(format::ET_CORE)      => FileType::Core,
                None => FileType::Unknown(ehdr.e_type),
            },

            datafmt: match FromPrimitive::from_u8(ehdr.e_ident[5]) {
                Some(format::ELFDATA2LSB)  => DataFormat::LittleEndian,
                Some(format::ELFDATA2MSB)  => DataFormat::BigEndian,
                _ => return Err(IoError {
                    kind:     IoErrorKind::InvalidInput,
                    desc:     "EI_DATA field has unknown value",
                    detail:   None,
                }),
            },

            mach: match FromPrimitive::from_u16(ehdr.e_machine) {
                Some(format::EM_NONE)      => Machine::NoMachine,
                Some(format::EM_M32)       => Machine::WE32100,
                Some(format::EM_SPARC)     => Machine::SPARC,
                Some(format::EM_386)       => Machine::I80386,
                Some(format::EM_68K)       => Machine::M68000,
                Some(format::EM_88K)       => Machine::M88000,
                Some(format::EM_860)       => Machine::I80860,
                Some(format::EM_MIPS)      => Machine::RS3000,
                None => Machine::Unknown(ehdr.e_machine),
            },

            entry: match ehdr.e_entry {
                0 => None,
                x => Some(x),
            },

            sects: HashMap::new(),
        };

        Ok(file)
    }
}
