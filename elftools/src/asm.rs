// Tools for manipulating ELF files
// Copyright (C) 2015-present Alex Iadicicco <http://ajitek.net>

//! This is the assembler toolkit. It provides a trait for an instruction
//! encoder, a trait for an instruction parser, a trait for output, an
//! implementation of the output trait that builds an ELF file, and a driver to
//! connect it all together.

pub type AsmResult<T> = Result<T, AsmError>;

pub struct AsmError {
    pub kind:      AsmErrorKind,
    pub desc:      &'static str,
    pub detail:    Option<String>,
}

pub enum AsmErrorKind {
    UnknownMnemonic,
    UnknownDirective,
    UnknownOperand,
    InvalidOperand,
}

// ENCODERS
// =============================================================================

pub trait Encoder<T, U> {
    /// Encode the given input into the given output buffer. Returns the number
    /// of output elements used, or 
    fn encode(&T, &mut[U]) -> AsmResult<usize>;
}

pub type AsmByteEncoder<'a> = Encoder<AsmStatement<'a>, u8>;
pub type AsmWordEncoder<'a> = Encoder<AsmStatement<'a>, u16>;

pub struct AsmStatement<'a> {
    pub kind:      AsmStatementKind,
    pub mnem:      &'a str,
    pub args:      &'a[&'a str],
}

pub enum AsmStatementKind {
    Directive,
    Instruction,
}

// PARSERS
// =============================================================================

