// Tools for manipulating ELF files
// Copyright (C) 2015-present Alex Iadicicco <http://ajitek.net>

//! This crate contains a number of components for manipulating binary files
//! containing machine code.
//!
//! # Assembler toolkit
//! Common assembler bits are included here to assist assembler writers. This
//! way assembler authors can focus on the task of processing input and
//! encoding instructions instead of futzing about with the ELF format.
//!
//! # ELF library
//! For those that *do* wish to futz with the ELF format, a set of tools have
//! been provided for manipulating ELF files. There is a low level module,
//! `format`, with structures copied directly out of the ELF format manual,
//! and a less low level module `elf` with more Rust-like structures, as
//! well as code for loading/saving modules.
//!
//! # Linker
//! A linker is planned, that can link together several ELF object files into
//! a single ELF executable, performing the necessary relocations.

pub mod format;
pub mod reader;
pub mod elf;
