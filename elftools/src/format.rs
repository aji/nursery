// Tools for manipulating ELF files
// Copyright (C) 2015-present Alex Iadicicco <http://ajitek.net>

//! Structures and definitions for ELF files, copied directly from the format
//! manual. Ideally these will be laid out in memory exactly as laid out in the
//! file, so that simple byte copies from disk will be enough to load files.
//!
//! Users are encouraged to use the `elf` module for loading ELF files instead.

#![stable]
#![allow(non_camel_case_types)]

pub use self::ET::*;
pub use self::EM::*;
pub use self::EV::*;
pub use self::EI::*;
pub use self::ELFCLASS::*;
pub use self::ELFDATA::*;
pub use self::SHN::*;
pub use self::SHT::*;
pub use self::SHF::*;
pub use self::STB::*;
pub use self::STT::*;
pub use self::PT::*;

#[stable] impl Copy for Elf32_Ehdr { }
#[stable] impl Copy for ET { }
#[stable] impl Copy for EM { }
#[stable] impl Copy for EV { }
#[stable] impl Copy for EI { }
#[stable] impl Copy for ELFCLASS { }
#[stable] impl Copy for ELFDATA { }
#[stable] impl Copy for SHN { }
#[stable] impl Copy for Elf32_Shdr { }
#[stable] impl Copy for SHT { }
#[stable] impl Copy for SHF { }
#[stable] impl Copy for Elf32_Sym { }
#[stable] impl Copy for STB { }
#[stable] impl Copy for STT { }
#[stable] impl Copy for Elf32_Rel { }
#[stable] impl Copy for Elf32_Rela { }
#[stable] impl Copy for Elf32_Phdr { }
#[stable] impl Copy for PT { }

#[stable] pub type Elf32_Addr   = u32;
#[stable] pub type Elf32_Half   = u16;
#[stable] pub type Elf32_Off    = u32;
#[stable] pub type Elf32_Sword  = i32;
#[stable] pub type Elf32_Word   = u32;

/// ELF file header, as defined in the format manual. The format manual has
/// the following to say about the ELF header:
///
/// Some object file control structures can grow, because the ELF header
/// contains their actual sizes. If the object file for mat changes, a
/// program may encounter control structures that are larger or smaller
/// than expected. Programs might therefore ignore "extra" information. The
/// treatment of "missing" information depends on context and will be
/// specified when and if extensions are defined.
#[derive(Show)]
#[stable] pub struct Elf32_Ehdr {
    /// The initial bytes mark the file as an object file and provide
    /// machine-independent data with which to decode and interpret the
    /// file's contents.
    #[stable] pub e_ident:      [u8; 16],

    /// Identifies the object file type. Should be one of the `ET_*` values
    #[stable] pub e_type:       Elf32_Half,

    /// Identifies the architecture for a file.
    #[stable] pub e_machine:    Elf32_Half,

    /// This member identifies the object file version.
    #[stable] pub e_version:    Elf32_Word,

    /// This member gives the virtual address to which the system first
    /// transfers control, thus starting the process. If the file has no
    /// associated entry point, this member holds zero.
    #[stable] pub e_entry:      Elf32_Addr,

    /// This member holds the program header table's file offset in bytes. If
    /// the file has no program header table, this member holds zero.
    #[stable] pub e_phoff:      Elf32_Off,

    /// This member holds the section header table's file offset in bytes. If
    /// the file has no section header table, this member holds zero.
    #[stable] pub e_shoff:      Elf32_Off,

    /// This member holds processor-specific flags associated with the
    /// file. Flags names take the form `EF_machine_flag`.
    #[stable] pub e_flags:      Elf32_Word,

    /// This member holds the ELF header's size in bytes.
    #[stable] pub e_ehsize:     Elf32_Half,

    /// This member holds the size in bytes of one entry in the file's
    /// program header table; all entries are the same size.
    #[stable] pub e_phentsize:  Elf32_Half,

    /// This member holds the number of entries in the program header
    /// table. Thus the product of `e_phentsize` and `e_phnum` give the
    /// table's size in bytes. If a file has no program header table,
    /// `e_phnum` holds the value zero.
    #[stable] pub e_phnum:      Elf32_Half,

    /// This member holds a section header's size in bytes. A section header is
    /// one entry in the section header table; all entries are the same size.
    #[stable] pub e_shentsize:  Elf32_Half,

    /// This member holds the number of entries in the section header
    /// table. thus the product of `e_shentsize` and `e_shnum` gives the
    /// section header table's size in bytes. IF a file has no section header
    /// table, `e_shnum` holds the value zero.
    #[stable] pub e_shnum:      Elf32_Half,

    /// This member holds the section header table index of the entry
    /// associated with the section name string table. If the file has no
    /// section name string table, this member holds the value `SHN_UNDEF`.
    #[stable] pub e_shstrndx:   Elf32_Half,
}

#[unstable="enumeration name may change"]
#[repr(u16)]
#[derive(FromPrimitive)]
pub enum ET {
    #[stable] #[doc="No file type"]       ET_NONE     = 0,
    #[stable] #[doc="Relocatable file"]   ET_REL      = 1,
    #[stable] #[doc="Executeable file"]   ET_EXEC     = 2,
    #[stable] #[doc="Shared object file"] ET_DYN      = 3,
    #[stable] #[doc="Core file"]          ET_CORE     = 4,
}

#[unstable="enumeration name may change"]
#[repr(u16)]
#[derive(FromPrimitive)]
pub enum EM {
    #[stable] #[doc="No machine"]         EM_NONE     = 0,
    #[stable] #[doc="AT&T WE 32100"]      EM_M32      = 1,
    #[stable] #[doc="SPARC"]              EM_SPARC    = 2,
    #[stable] #[doc="Intel 80386"]        EM_386      = 3,
    #[stable] #[doc="Motorola 68000"]     EM_68K      = 4,
    #[stable] #[doc="Motorala 88000"]     EM_88K      = 5,
    #[stable] #[doc="Intel 80860"]        EM_860      = 7,
    #[stable] #[doc="Mips RS3000"]        EM_MIPS     = 8,
}

#[unstable="enumeration name may change"]
#[repr(u32)]
#[derive(FromPrimitive)]
pub enum EV {
    #[stable] #[doc="Invalid version"]    EV_NONE     = 0,
    #[stable] #[doc="Current version"]    EV_CURRENT  = 1,
}

#[unstable="enumeration name may change"]
#[repr(u8)]
#[derive(FromPrimitive)]
pub enum EI {
    #[stable] #[doc="File identification"]     EI_MAG0      =  0,
    #[stable] #[doc="File identification"]     EI_MAG1      =  1,
    #[stable] #[doc="File identification"]     EI_MAG2      =  2,
    #[stable] #[doc="File identification"]     EI_MAG3      =  3,
    #[stable] #[doc="File class"]              EI_CLASS     =  4,
    #[stable] #[doc="Data encoding"]           EI_DATA      =  5,
    #[stable] #[doc="File version"]            EI_VERSION   =  6,
    #[stable] #[doc="Start of padding bytes"]  EI_PAD       =  7,
    #[stable] #[doc="Size of `e_ident[]`"]     EI_NIDENT    = 16,
}

/// Magic number
#[stable] pub static ELFMAG: &'static [u8] = b"\x7fELF";

#[unstable="enumeration name may change"]
#[repr(u8)]
#[derive(FromPrimitive)]
pub enum ELFCLASS {
    #[stable] #[doc="Invalid class"]           ELFCLASSNONE   =  0,
    #[stable] #[doc="32-bit objects"]          ELFCLASS32     =  1,
    #[stable] #[doc="64-bit objects"]          ELFCLASS64     =  2,
}

#[unstable="enumeration name may change"]
#[repr(u8)]
#[derive(FromPrimitive)]
pub enum ELFDATA {
    #[stable] #[doc="Invalid data encoding"]   ELFDATANONE    =  0,
    #[stable] #[doc="Little-endian"]           ELFDATA2LSB    =  1,
    #[stable] #[doc="Big-endian"]              ELFDATA2MSB    =  2,
}

/// Section references.
#[unstable="enumeration name may change"]
#[repr(u16)]
#[derive(FromPrimitive)]
pub enum SHN {
    /// This value marks an undefined, missing, irrelevant, or otherwise
    /// meaningless section reference. For example, a symbol "defined"
    /// relative to section number `SHN_UNDEF` is an undefined symbol.
    #[stable] SHN_UNDEF       =       0,

    /// This value specifies the lower bound of the range of reserved indices.
    #[stable] SHN_LORESERVE   =  0xff00,

    /// Values in this inclusive range (`SHN_LORESERVE` to `SHN_HIPROC`) are
    /// reserved for processor-specific semantics
    #[stable] SHN_HIPROC      =  0xff1f,

    /// This value specifies absolute values for the corresponding
    /// reference. For example, symbols defined relative to section number
    /// `SHN_ABS` have absolute values and are not affected by relocation.
    #[stable] SHN_ABS         =  0xfff1,

    /// Symbols defined relative to this section are common symbols, such
    /// as FORTRAN `COMMON` or unallocated C external variables.
    #[stable] SHN_COMMON      =  0xfff2,

    /// This value specifies the upper bound of the range of reserved
    /// indexes. The system reserves indexes between `SHN_LORESERVE` and
    /// `SHN_HIRESERVE`, inclusive; the values do not reference the section
    /// header table. That is, the section header table does *not* contain
    /// entries for the reserved indexes.
    #[stable] SHN_HIRESERVE  =  0xffff,
}

/// ELF section header, as defined in the format manual. The ELF format
/// manual says the following about sections:
///
/// - Every section in an object file has exactly one section header describing
///   it. Section headers may exist that do not have a section.
/// - Each section occupies one contiguous (possibly empty) sequence of
///   bytes within a file.
/// - Sections in a file may not overlap. No byte in a file resides in more
///   than one section.
/// - An object file may have inactive space. The various headers and sections
///   might not "cover" every byte in an object file. The contents of the
///   inactive data are unspecified.
#[derive(Show)]
#[stable] pub struct Elf32_Shdr {
    /// This member specifies the name of the section. Its value is an index
    /// into the section header string table section, giving the location
    /// of a null-terminated string.
    #[stable] pub sh_name:       Elf32_Word,

    /// This member categorizes the section's and semantics. One of the
    /// `SHT_*` values.
    #[stable] pub sh_type:       Elf32_Word,

    /// Sections support 1-bit flags that describe miscellaneous
    /// attributes. Flag definitions are `SHF_*`
    #[stable] pub sh_flags:      Elf32_Word,

    /// If the section will appear in the memory image of a process, this
    /// member gives the address at which the section's first byte should
    /// reside. Otherwise, the member contains 0.
    #[stable] pub sh_addr:       Elf32_Addr,

    /// This member's value gives the byte offset from the beginning of the
    /// file to the first byte in the section. One section type, `SHT_NOBITS`
    /// described below, occupies no space in the file, and its `sh_offset`
    /// member locates the conceptual placement in the file. *Ed. note:
    /// I don't know what that means, but it's what the manual says.*
    #[stable] pub sh_offset:     Elf32_Off,

    /// This member gives the section's size in bytes. Unless the section
    /// type is `SHT_NOBITS`, the section occupies `sh_size` bytes in the
    /// file. A section of type `SHT_NOBITS` may have a non-zero size,
    /// but it occupies no space in the file.
    #[stable] pub sh_size:       Elf32_Word,

    /// This member holds a section header table index link, whose interpretation
    /// depends on the section type:
    ///
    /// - `SHT_DYNAMIC` -- The section header index of the string table used
    ///   by entries in the section.
    /// - `SHT_HASH` -- The section header index of the symbol table to
    ///   which the hash table applies.
    /// - `SHT_REL`, `SHT_RELA` -- The section header index of the associated
    ///   symbol table
    /// - `SHT_SYMTAB`, `SHT_DYNSYM` -- The section header index of the
    ///   associated string table.
    /// - other -- `SHN_UNDEF`
    #[stable] pub sh_link:       Elf32_Word,

    /// This member holds extra information, whose interpretation depends
    /// on the section type:
    ///
    /// - `SHT_DYNAMIC` -- 0
    /// - `SHT_HASH` -- 0
    /// - `SHT_REL`, `SHT_RELA` -- The section header index of the section
    ///   to which the relocation applies.
    /// - `SHT_SYMTAB`, `SHT_DYNSYM` -- One greater than the symbol table
    ///   index of the last local symbol (binding `STB_LOCAL`).
    /// - other -- 0
    #[stable] pub sh_info:       Elf32_Word,

    /// Some sections have address alignment constraints. For example,
    /// if a section holds a doubleword, the system must ensure doubleword
    /// alignment for the entire section. That is, the value of `sh_addr` must
    /// be congruent to 0, modulo the value of `sh_addralign`. Currently,
    /// only 0 and positive integral powers of tw oare allowed. Values 0
    /// and 1 mean the section has no alignment constraints.
    #[stable] pub sh_addralign:  Elf32_Word,

    /// Some sections hold a table of fixed-size entries, such as a symbol
    /// table. For such a section, this member gives the size in bytes of
    /// each entry. This member contains 0 if the section does not hold a
    /// table of fixed-size entries.
    #[stable] pub sh_entsize:    Elf32_Word,
}

/// A section header's `sh_type` member specifies the section's semantics.
/// It can have one of the following values
#[unstable="enumeration name may change"]
#[repr(u32)]
#[derive(FromPrimitive)]
pub enum SHT {
    /// This value markes the section header as inactive; it does not have an
    /// associated section. Other members of the section header have undefined
    /// values.
    #[stable] SHT_NULL       =           0,

    /// This section holds information defined by the program, whose format
    /// and meaning are determined solely by the program
    #[stable] SHT_PROGBITS   =           1,

    /// These sections hold a symbol table. Currently, an object file may
    /// have only one section of each type, but this restriction may be
    /// relaxed in the future. Typically, `SHT_SYMTAB` provides symbols for
    /// link editing, though it may also be used fur dynamic linking. As a
    /// complete symbol table, it may contain many symbols unnecessary for
    /// dynamic linking. Consequently, an object file may also contain a
    /// `SHT_DYNSYM` section, which holds a minimal set of dynamic linking
    /// symbols, to save space.
    ///
    #[stable] SHT_SYMTAB     =           2,

    /// The section holds a string table. An object file may have multiple
    /// string table sections.
    ///
    /// String table sections hold null-terminated character-sequences,
    /// commonly called strings. The object file uses these strings to
    /// represent symbol and section names. One references a string as an
    /// index into the string table section. The first byte, which is index
    /// zero, is defined to hold a null character. Likewise, a string's last
    /// byte is defined to hold a null character, ensuring null termination
    /// for all strings. A string whose index is zero specifies either no
    /// name or a null name, depending on context. An empty string table
    /// section is permitted; its section header's `sh_size` member would
    /// contain zero. Non-zero indexes are invalid for an empty string table.
    #[stable] SHT_STRTAB     =           3,

    /// The section holds relocation entries with explicit addends, such as
    /// type `Elf32_Rela` for the 32-bit class of object files. An object
    /// file may have multiple relocation sections.
    #[stable] SHT_RELA       =           4,

    /// The section holds a symbol hash table. All objects participating
    /// in dynamic linking must contain a symbol hash table. Currently,
    /// an object file may only have one hash table, but this restriction
    /// may be relaxed in the future.
    #[stable] SHT_HASH       =           5,

    /// The section holds information for dynamic linking. Currently, an
    /// object file may have only one dynamic section, but this restriction
    /// may be relaxed in the future.
    #[stable] SHT_DYNAMIC    =           6,

    /// The section holds information that marks the file in some way.
    #[stable] SHT_NOTE       =           7,

    /// A section of this type occupies no space in the file but otherwise
    /// resembles `SHT_PROGBITS`. Although this section contains no bytes,
    /// the `sh_offset` member contains the conceptual file offset. *Ed. note:
    /// I still have no idea what that means.*
    #[stable] SHT_NOBITS     =           8,

    /// The section holds relocation entries without explicit addends, such
    /// as type `Elf32_Rel` for the 32-bit class of object files. An object
    /// file may have multiple relocation sections.
    #[stable] SHT_REL        =           9,

    /// This section type is reserved but has unspecified semantics. Programs
    /// that contain a section of this type do not conform to the ABI.
    #[stable] SHT_SHLIB      =          10,

    /// See: `SHT_SYMTAB`
    #[stable] SHT_DYNSYM     =          11,

    /// Values in this inclusive range are reserved for processor-specific
    /// semantics.
    #[stable] SHT_LOPROC     =  0x70000000,

    /// See: `SHT_LOPROC`
    #[stable] SHT_HIPROC     =  0x7fffffff,

    /// This value specifies the lower bound of the range of indexes reserved
    /// for application programs.
    #[stable] SHT_LOUSER     =  0x80000000,

    /// This value specifies the upper bound of the range of indexes reserved
    /// for application programs. Section types between `SHT_LOUSER` and
    /// `SHT_HIUSER` may be used by the application, without conflicting
    /// with current or future system-defined section types.
    #[stable] SHT_HIUSER     =  0x8fffffff,
}

/// A section header's `sh_flags` member holds 1-bit flags that describe
/// the section's attributes. Defined values appear below; other values
/// are reserved.
///
/// If a flag bit is set in `sh_flags` the attribute is "on" for the
/// section. Otherwise, the attribute is "off" or does not apply. Undefined
/// attributes are set to zero.
#[unstable="enumeration name may change"]
#[repr(u32)]
#[derive(FromPrimitive)]
pub enum SHF {
    /// The section contains data that should be writable during process
    /// execution.
    #[stable] SHF_WRITE      =         0x1,

    /// The section occupies memory during process execution. Some control
    /// sections do not reside in the memory image of an object file; this
    /// attribute is off for those sections.
    #[stable] SHF_ALLOC      =         0x2,

    /// The section contains executable machine instructions.
    #[stable] SHF_EXECINSTR  =         0x4,

    /// All bits included in this mask are reserved for processor-specific
    /// semantics.
    #[stable] SHF_MASKPROC   =  0xf0000000,
}

/// A symbol table entry has the given format.
///
/// An object file's symbol table holds information needed to locate and
/// relocate a program's symbolic definitions and references. A symbol table
/// index is a subscript into this array. Index 0 both designates the first
/// entry in the table and serves as the undefined symbol index. The contents
/// of the initial entry are specified later in this section.
#[derive(Show)]
#[stable] pub struct Elf32_Sym {
    /// This member holds an index into the object file's symbol string table,
    /// which holds the character representations of the symbol names. If
    /// the value is non-zero, it represents a string table index that gives
    /// the symbol name. Otherwise, the symbol table entry has no name.
    #[stable] pub st_name:        Elf32_Word,

    /// This member gives the value of the associated symbol. Depending on
    /// the context, this may be an absolute value, an address, etc.
    ///
    /// Symbol table entries for different object file types have slightly
    /// different interpretations for this member:
    ///
    /// - In relocatable files, `st_value` holds alignment constraints for
    ///   a symbol whose section index is `SHN_COMMON`
    /// - In relocatable files, `st_value` holds a section offset for a
    ///   defined symbol. That is, `st_value` is an offset from the beginning
    ///   of the section that `st_shndx` identifies.
    /// - In executable and shared object files, `st_value` holds a virtual
    ///   address. To make these files' symbols more useful for the dynamic
    ///   linker, the section offset (file interpretation) gives way to
    ///   a virtual address (memory interpretation) for which the section
    ///   number is irrelevant.
    #[stable] pub st_value:       Elf32_Addr,

    /// Many symbols have associated sizes. For example, a data object's
    /// size is the number of bytes contained in the object. This member
    /// holds 0 if the symbol has no size or an unknown size.
    #[stable] pub st_size:        Elf32_Word,

    /// This member specifies the symbol's type and binding attributes. A
    /// list of the values and meanings appears belows. The following code
    /// shows how to manipulate the values.
    #[stable] pub st_info:        u8,

    /// This member currently holds 0 and has no defined meaning.
    #[stable] pub st_other:       u8,

    /// Every symbol table entry is "defined" in relation to some section;
    /// this member holds the relevant section header table index. As related
    /// text describes, some section indexes indicate special meanings.
    #[stable] pub st_shndx:       Elf32_Half,
}

/// A symbol's binding determines the linkage visibility and behavior.
///
/// Global and weak symbols differ in two major ways:
///
/// - When the link editor combines several relocatable object files,
///   it does not allow multiple definitions of `STB_GLOBAL` symbols with
///   the same name. On the other hand, if a defined global symbol exists,
///   the appearance of a weak symbol with the same name will not cause an
///   error. The link editor honors the global definition and ignores the
///   weak ones. Similarly, if a common symbol exists (i.e., a symbol whose
///   `st_shndx` field holds `SHN_COMMON`), the appearance of a weak symbol
///   with the same name will not cause an error. The link editor honors
///   the common definition and ignores the weak ones.
///
/// - When the link editor searches archive libraries, it extracts archive
///   members that contain definitions of undefined global symbols. The
///   member's definition may be either a global or a weak symbol. The link
///   editor does not extract archive members to resolve undefined weak
///   symbols. Unresolved weak symbols have a zero value.
///
/// In each symbol table, all symbol tables with `STB_LOCAL` binding precede
/// the weak and global symbols. A symbol table section's `sh_info` section
/// header member holds the symbol table index for the first non-local symbol.
#[unstable="enumeration name may change"]
#[derive(FromPrimitive)]
pub enum STB {
    /// Local symbols are not visible outside the object file containing
    /// their definition. Local symbols of the same name may exist in multiple
    /// files without interfering with each other.
    #[stable] STB_LOCAL          =   0,

    /// Global symbols are visible to all object files being combined. One
    /// file's definition of a global symbol will satisfy another file's
    /// undefined reference to the same global symbol.
    #[stable] STB_GLOBAL         =   1,

    /// Weak symbols resemble global symbols, but their definitions have
    /// lower precedence.
    #[stable] STB_WEAK           =   2,

    /// Values in this inclusive range are reserved for processor-specific
    /// semantics.
    #[stable] STB_LOPROC         =  13,
    /// See: `STB_LOPROC`
    #[stable] STB_HIPROC         =  15,
}

/// A symbol's type provides a general classification for the associated
/// entity.
///
/// Function symbols (those with type `STT_FUNC`) in shared object files
/// have special significance. When another object file references a function
/// from a shared object, the link editor automatically creates a procedure
/// linkage table entry for the referenced symbol. Shared object symbols
/// with types other than `STT_FUNC` will not be referenced automatically
/// through the procedure linkage table.
///
/// If a symbol's value refers to a specific location within a section,
/// its section index member, `st_shndx`, holds an index into the section
/// header table. As the section moves during relocation, the symbol's
/// value changes as well, and references to the symbol continue to "point"
/// to the same location in the program. Some special section index values
/// give other semantics:
///
/// - `SHN_ABS` -- The symbol has an absolute value that will not change
///   because of relocation.
/// - `SHN_COMMON` -- The symbol lables a common block that has not yet been
///   allocated. The symbol's value gives alignment constrraints, similar
///   to a section's `sh_addralign` member. That is, the link editor will
///   allocate the storage for the symbol at an address that is a multiple of
///   `st_value`. The symbol's size tells how many bytes are required.
/// - `SHN_UNDEF` -- This section table index means the symbol is
///   undefined. When the link editor combines this object file with another
///   that defines the indicated symbol, this file's references to the symbol
///   will be linked to the actual definition.
///
/// The symbol table entry for index 0 (`STN_UNDEF`) is reserved. It holds
/// all zero values.
#[unstable="enumeration name may change"]
#[derive(FromPrimitive)]
pub enum STT {
    /// The symbol's type is not specified
    #[stable] STT_NOTYPE         =   0,

    /// The symbol is associated with a data object, such as a variable,
    /// an array, etc.
    #[stable] STT_OBJECT         =   1,

    /// The symbol is associated with a function or other executable code.
    #[stable] STT_FUNC           =   2,

    /// The symbol is associtade with a section. Symbol table entries of this
    /// type exist primarily for relocation and normally have `STB_LOCAL`
    /// binding.
    #[stable] STT_SECTION        =   3,

    /// Conventionally, the symbol's name gives the name of the source
    /// file associated with the object file. A file symbol has `STB_LOCAL`
    /// binding, its section index is `SHN_ABS`, and it precedes the other
    /// `STB_LOCAL` symbols for the file, if it is present.
    #[stable] STT_FILE           =   4,

    /// Values in this inclusive range are reserved for processor-specific
    /// semantics.
    #[stable] STT_LOPROC         =  13,
    /// See: `STT_LOPROC`
    #[stable] STT_HIPROC         =  15,
}

/// See: `Elf32_Rela`
#[derive(Show)]
#[stable] pub struct Elf32_Rel {
    /// See: `Elf32_Rela`
    #[stable] pub r_offset:        Elf32_Addr,
    /// See: `Elf32_Rela`
    #[stable] pub r_info:          Elf32_Word,
}

/// Only `Elf32_Rela` entries contain an explicit addend. Entries of
/// type `Elf32_Rel` store an implicit addend in the location to be
/// modified. Depending on the processor architecture, one form or the other
/// might be necessary or more convenient. Consequently, an implementation
/// for a particular machine may use one form exclusively or either form
/// depending on context.
///
/// Refer to the ELF format manual for more information about relocation
/// entries.
#[derive(Show)]
#[stable] pub struct Elf32_Rela {
    /// This member gives the location at which to apply the relocation
    /// action. For a relocatable file, the value is the byte offset from
    /// the beginning of the section to the storage unit affected by the
    /// relocation. For an executable file or a shared object, the value is
    /// the virtual address of the storage unit affected by the relocation.
    ///
    /// A relocation section references two other sections: a symbol
    /// table and a section to modify. The section header's `sh_info` and
    /// `sh_link` members, described in "Sections" above, specify these
    /// relationships. Relocation entries for different object files have
    /// slightly different interpretations for the `r_offset` member.
    ///
    /// - In relocatable files, `r_offset` holds a section offset. That is,
    ///   the relocation section itself describes how to modify another
    ///   section in the file; relocation offsets designate a storage unit
    ///   within the second section.
    /// - In executable and shared object files, `r_offset` holds a virtual
    ///   address. To make these files' relocation entries more useful
    ///   for the dynamic linker, the section offset (file interpretation)
    ///   gives way to a virtual address (memory interpretation).
    ///
    /// Although the interpretation of `r_offset` changes for different
    /// object files to allow efficient access by the relevant programs,
    /// the relocation types' meanings stay the same.
    #[stable] pub r_offset:        Elf32_Addr,

    /// This member gives both the symbol table index with respect to
    /// which the relocation must be made, and the type of relocation to
    /// apply. For example, a call instruction's relocation entry would hold
    /// the symbol table index of the function being called. If the index is
    /// `STN_UNDEF`, the undefined symbol index, the relocation uses 0 as the
    /// "symbol value." Relocation types are processor-specific. When the text
    /// refers to a relocation entry's relocation type or symbol table index,
    /// it means the result of applying `ELF32_R_TYPE` or `ELF32_R_SYM`,
    /// respectively, to the entry's `r_info` member.
    ///
    /// ```c
    /// #define ELF32_R_SYM(i)     ((i)>>8)
    /// #define ELF32_R_TYPE(i)    ((unsigned char)(i))
    /// #define ELF32_R_INFO(s,t)  (((s)<<8)+(unsigned char)(t))
    /// ```
    #[stable] pub r_info:          Elf32_Word,

    /// This member specifies a constant addend used to compute the value
    /// to be stored into the relocatable field.
    #[stable] pub r_addend:        Elf32_Sword,
}

/// An executable or shared object file's program hedaer table is an array
/// of structures, each describing a segment or other information the system
/// needs to prepare the program for execution. An object file *segment*
/// contains one or moer *sections*. Program headers are meaningful only
/// for executable and shared object files. A file specifies its own program
/// header size with the ELF header's `e_phentsize` and `e_phnum` members.
#[derive(Show)]
#[stable] pub struct Elf32_Phdr {
    /// This member tells what kind of segment this array element describes
    /// or how to interpret the array element's information.
    #[stable] pub p_type:          Elf32_Word,

    /// This member gives the offset from the beginning of the file at which
    /// the first byte of the segment resides.
    #[stable] pub p_offset:        Elf32_Off,

    /// This member gives the virtual address at which the firstr byte of
    /// the segment resides in memory.
    #[stable] pub p_vaddr:         Elf32_Addr,

    /// On systems for which physical addressing is relevant, this member
    /// is reserved for the segment's physical address.
    #[stable] pub p_paddr:         Elf32_Addr,

    /// This member gives the number of bytes in the file image of the segment;
    /// it may be zero.
    #[stable] pub p_filesz:        Elf32_Word,

    /// This member gives the number of bytes in the memory image of the
    /// segment; it may be zero
    #[stable] pub p_memsz:         Elf32_Word,

    /// This member gives flags relevant to the segment.
    #[stable] pub p_flags:         Elf32_Word,

    /// Loadable process segments must have congruent values for `p_vaddr`
    /// and `p_offset`, modulo the page size. This member gives the value to
    /// which the segments are aligned in memory and in the file. Values 0
    /// and 1 mean no alignment is required. Otherwise, `p_align` should be
    /// positive, integral power of 2, and `p_vaddr` should equal `p_offset`,
    /// modulo p_align
    #[stable] pub p_align:         Elf32_Word,
}

/// Values for program header type, `p_type`
#[unstable="enumeration name may change"]
#[repr(u32)]
#[derive(FromPrimitive)]
pub enum PT {
    /// The array element is unused; other members' values are undefined.
    /// This type lets the program header table have ignored entries.
    #[stable] PT_NULL          =            0,

    /// The array element specifies a loadable segment, described by
    /// `p_filesz` and `p_memsz`. The bytes from the file are mapped to
    /// the beginning of the memory segment. If the segment's memory size,
    /// (`p_memsz`) is larger than the file size, (`p_filesz`), the "extra"
    /// bytes are defined to hold the value 0 and to follow the segment's
    /// initialized area. The file size may not be larger than the memory
    /// size. Loadable segment entries in the program header table appear
    /// in ascending order, sorted on the `p_vaddr` member.
    #[stable] PT_LOAD          =            1,

    /// The array element specifies dynamic linking information.
    #[stable] PT_DYNAMIC       =            2,

    /// The array element specifies the location and size of a null-terminated
    /// path name to invoke as an interpreter. This segment type is meaningful
    /// only for executable files, (though it may occur for shared objects);
    /// it may not occur more than once in a file. If it is present, it must
    /// precede any loadable segment entry.
    #[stable] PT_INTERP        =            3,

    /// The array element specifies the location and size of auxiliary
    /// information.
    #[stable] PT_NOTE          =            4,

    /// This segment type is reserved but has unspecified semantics. Programs
    /// that contain an array element of this type do not conform to the ABI.
    #[stable] PT_SHLIB         =            5,

    /// The array element, if present, specifies the location and size of
    /// the program header table itself, both in the file and in the memory
    /// image of the program. This segment type may not occur more than
    /// once in a file. Moreover, it may occur only if the program header
    /// table is part of the memory image of the program. If it is present,
    /// it must precede any loadable segment entry.
    #[stable] PT_PHDR          =            6,

    /// Values in this inclusive range (`PT_LOPROC` through `PT_HIPROC`)
    /// are reserved for processor-specific semantics.
    #[stable] PT_LOPROC        =   0x70000000,

    /// See: `PT_LOPROC`
    #[stable] PT_HIPROC        =   0x7fffffff,
}
