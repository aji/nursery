This crate contains a number of components for manipulating ELF files
containing machine code, as well as a few driver programs for working
with these pieces as standalone units.

# Library features
First and foremost is the library. A summary of library features follows.

## `format`: Format manual implementation
Many of the structures and constants defined in the ELF format manual
have been copied down as Rust structures and enumerations in the `format`
module. Relevant text from the manual has been duplicated as rustdoc
comments. These are useful for working directly with data in the on-disk
ELF format, but not for much else.

## `elf`: ELF tree
The structures in the format manual (and `format` module) describe only
the on-disk format. Additionally, being C inspired, they are rather
cumbersome when used from Rust. Relationships between parts of the file
are completely unresolved, and as such they are not directly useful
for traversing or manipulating ELF data. The `elf` module contains code
for loading and saving ELF files from a more meaningful perspective, and
includes a collection of structures for describing ELF data semantically;
references are resolved, tables are parsed, and identifying fields take
on more descriptive values.

## `asm`: Assembler toolkit (*planned*)
Constructing relocatable ELF files from scratch by assembling machine
instructions is a relatively architecture-independent task. A module is
planned for handling common assembler tasks.

## `link`: Linker toolkit (*planned*)
Handling relocations is also a rather architecture-independent task. A
module is planned for performing these relocations.

# Driver programs
In addition to the included library features are a handful of driver
programs, described below.

## `elfdump`: ELF dumper
Similar to the `objdump` utility from GNU, an `elfdump` utility is
provided for both dumping ELF files and exercising the ELF loader code. At
writing, its usage is simple:

    $ elfdump FILE
