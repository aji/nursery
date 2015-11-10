// 68000-family assembler library
// Copyright (C) 2015-present Alex Iadicicco <http://ajitek.net>

//! Assembly file "parser". More correctly, this is a tokenizer, but the term
//! "lexer" is a bit overloaded in my opinion. An assembly file parse tree would
//! not be very meaningful anyway, and would almost certainly look like a `Vec`
//! of tokens.

#![experimental]

use std::io::IoResult;

/// Single input "token". An input file is parsed into a list of these.
#[derive(Show)]
pub enum Input {
    /// A single input directive, with arguments. The `Vec` parameter is a list
    /// of comma-delimited arguments to the initial input directive. As an
    /// example, the input
    ///
    ///     add   1, d2
    ///     dc.b  65, 74, 73, 13, 10
    ///
    /// would be parsed as
    ///
    ///     Directive("add", ["1", "d2"])
    ///     Directive("dc.b", ["65", "74", "73", "13", "10"])
    ///
    /// (ignoring the fact that Rust `Vec` literals do not look like that). Note
    /// that no distinction is made between assembler directives (like `dc.b`,
    /// `.text`, etc.) and proper instructions. It's not the lexer's job to know
    /// the names of directives and instructions.
    Directive          (String, Vec<String>),

    /// A label. The colon is not included in the returned `String`.
    Label              (String),
}

/// A parser instance, given a source iterator. At present, the only useful
/// implementation is for `IoResult<char>` iterators, which many file-like
/// objects can provide.
pub struct Parser<It> {
    /// Input stream, probably a character stream.
    input:             It,
    /// Internal lookahead buffer.
    ungot:             Vec<char>,
    /// Line number, starting at 1
    line_nr:           usize,
}

macro_rules! try_char {
    ($inp:expr) => (
        match $inp {
            Some(x) => x,
            None => return Err(false),
        }
    );
}

impl<It: Iterator<Item = IoResult<char>>> Parser<It> {
    /// Creates a new parser that reads from the given input stream. As an
    /// example, creating one of these parsers from standard input would look
    /// like this:
    ///
    ///     let p = Parser::new(stdin().lock().chars());
    ///
    pub fn new(input: It) -> Parser<It> {
        Parser {
            input:     input,
            ungot:     Vec::with_capacity(3),
            line_nr:   1,
        }
    }

    /// Un-fetches the given input character, to be returned on the next call to
    /// `getc`. Characters are `getc`'d in the opposite order they are
    /// `ungetc`'d. In other words, the `ungot` buffer acts as a stack.
    fn ungetc(&mut self, c: char) {
        if c == '\n' {
            self.line_nr -= 1;
        }

        self.ungot.push(c);
    }

    /// Fetches the next input character, considering the `ungot` buffer. Should
    /// not be used directly, since `getc` performs important structure updates
    /// depending on the character.
    ///
    /// TODO: handle errors better. At present, errors are silently discarded
    /// and treated as end-of-stream.
    fn getc_raw(&mut self) -> Option<char> {
        match self.ungot.pop() {
            Some(c) => Some(c),
            None => match self.input.next() {
                Some(Ok(c)) => Some(c),
                _ => None,
            }
        }
    }

    /// Fetches the next input character, performing correct internal structure
    /// updates, such as incrementing the line counter.
    fn getc(&mut self) -> Option<char> {
        let c = self.getc_raw();

        match c {
            Some('\n') => self.line_nr += 1,
            _ => { },
        }

        c
    }

    /// Fetches characters while the predicate returns true. Returns the list
    /// of characters read as a string, or `None` if an error occurs.
    fn getc_while<T: Fn(char) -> bool>(&mut self, f: T) -> Option<String> {
        let mut s = String::with_capacity(5);

        loop {
            match self.getc() {
                Some(c) =>
                    if f(c) {
                        s.push(c);
                    } else {
                        self.ungetc(c);
                        return Some(s);
                    },

                None =>
                    return None,
            }
        }
    }

    /// If the input stream is at a comment, skips over the comment.
    fn skip_comment(&mut self) -> bool {
        match self.getc() {
            Some(';') => { /* continue eating comment */ },
            Some(x) => { self.ungetc(x); return false },
            None => { return false },
        }

        loop {
            match self.getc() {
                Some('\n') => { return true },
                Some(_) => { /* continue eating comment */ },
                None => { return true },
            }
        }
    }

    fn skip_whitespace(&mut self) -> Option<String> {
        self.getc_while(|c| c.is_whitespace())
    }

    /// Attempts to read the next input token. Returns...
    ///
    /// - ... `Ok(Input)` if an input token was read successfully
    /// - ... `Err(true)` if another trip around the parser is needed.
    /// - ... `Err(false)` if end of input has occurred
    fn try_input(&mut self) -> Result<Input, bool> {
        // eat whitespace and to-eol comments right off the bat.
        loop {
            try_char!(self.skip_whitespace());

            if !self.skip_comment() {
                break;
            }
        }

        // now we're pointing at the first non-comment, non-whitespace
        // character. this is either the start of a label, or the start of a
        // directive. the only way to tell is to grab everything up until a
        // disambiguating character, which will be either whitespace, start of
        // comment, or a colon. if whitespace, we'll have to go past the
        // whitespace to see if it's a colon or not. it's kind of crazy but
        // these are the things we do for assembler.

        let s = try_char!(self.getc_while(
            |c| !c.is_whitespace() && c != ':' && c != ';'
        ));
        try_char!(self.skip_whitespace());

        // now we examine the character we stopped at to determine how to
        // proceed.

        match self.getc() {
            Some(':') =>
                return Ok(Input::Label(s)),

            Some(';') => {
                self.ungetc(';');
                return Ok(Input::Directive(s, Vec::new()));
            },

            Some(x) =>
                self.ungetc(x), /* continue to with-args directive parse */

            None =>
                return Err(false),
        }

        // TODO: this part
        Ok(Input::Label(format!("{}", try_char!(self.getc()))))
    }
}

impl<It: Iterator<Item = IoResult<char>>> Iterator for Parser<It> {
    type Item = Input;

    fn next(&mut self) -> Option<Input> {
        loop {
            match self.try_input() {
                Ok(x) => return Some(x),
                Err(true) => { },
                Err(false) => return None,
            }
        }
    }
}
