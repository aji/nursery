"""
logcons.py -- Logger console functionality

Copyright (c) 2010 Alex Iadicicco

This file is a part of AATE.

AATE is provided without warranty under the GNU General Public License. A copy
of this license can be found in the COPYING file in the project root directory,
or at <http://www.gnu.org/licenses/gpl.html>

This module provides a 'logcons' class which gives logger and console
functionality similar to the consoles found in games like Quake
"""

import string

class readline:
    """
    This class provides a similar functionality to the GNU readline
    library. The mechanics of this class are very different, however. This
    class works more like a toolkit than a framework. A user initalizes the
    class with an input callback and sends keyboard input to this class through
    the "input" method.

    To clear the readline prompt for printing text to the console, use the
    "interrupt" function. When finished, use the "resume" function.
    """

    def __init__(self, prompt, term, callback):
        """
        prompt is the prompt to display.

        term should be an aate.terminal instance.

        When the user enters a return character, the callback function is
        called with the command line string as the only argument.
        """
        self.prompt = prompt
        self.term = term
        self.inputcb = callback
        self.yank = ""
        self.history = []
        self.histpos = 0
        self.special = {"\b": self.backspace,
                        "\x04": self.delete,
                        "\r\n": self.entered,
                        "\r": lambda: self.specdup("\r\n"),
                        "\x01": lambda: self.moveto(self.homecol()),
                        "\x05": lambda: self.moveto(self.endcol()),
                        "\x02": lambda: self.moveby(-1),
                        "\x06": lambda: self.moveby(1),
                        "\x0b": self.clrtoeol,
                        "\x19": self.pasteyank,
                        "\x10": self.prevcmd}
        self.putprompt()

    def putprompt(self):
        """ Clears the current line and prints the prompt. This is this class's
        reset function. """
        self.clearline()
        self.inputmode()
        self.nlmode()
        self.term.write(self.prompt)
        self.col = len(self.prompt)
        self.line = ""
        self.active = True

    # abstractions 

    def clearline(self):
        """ Abstraction function for this commonly used control sequence """
        self.term.write("\x1b[2K\r")

    def inputmode(self):
        """ Abstraction function for this commonly used control sequence """
        self.term.write("\x1b[4h")

    def nlmode(self):
        """ Abstraction function for this commonly used control sequence """
        self.term.write("\x1b[20h")

    def homecol(self):
        return len(self.prompt)

    def endcol(self):
        return len(self.prompt + self.line)

    # prompt activity control

    def interrupt(self):
        """ Clears the line the prompt is on and stops accepting input """
        if self.active:
            self.clearline()
    
    def resume(self):
        """ Reprints the prompt and input and begins accepting input """
        if self.active:
            self.clearline()
            self.inputmode()
            self.nlmode()
            self.term.write(self.prompt)
            self.term.write(self.line)
            self.moveto()

    # control methods

    def moveto(self, c=-1):
        """ Moves the cursor to c """
        if c == -1:
            c = self.col
        self.term.write("\r\x1b[%sC" % c)
        self.col = c

    def moveby(self, diff):
        """ Moves the cursor by the specified amount. Limits the user to the
        input length. """
        new = self.col + diff
        if new < self.homecol(): new = self.homecol()
        if new > self.endcol(): new = self.endcol()
        self.moveto(new)

    def backspace(self):
        """ Deletes a character backward """
        pos = self.col - len(self.prompt)
        if pos > 0:
            self.line = self.line[:pos-1] + self.line[pos:]
            self.term.write("\b\x1b[P")
            self.moveby(-1)
        else:
            self.term.write("\x07")

    def delete(self):
        """ Deletes a character """
        pos = self.col - len(self.prompt)
        if pos < len(self.line):
            self.line = self.line[:pos] + self.line[pos+1:]
            self.term.write("\x1b[P")
        else:
            self.term.write("\x07")

    def clrtoeol(self):
        """ Clears to the end of the line, putting everything into the yank
        buffer. The yank buffer can be pasted with C-y """
        pos = self.col - len(self.prompt)
        self.yank = self.line[pos:]
        self.line = self.line[:pos]
        self.term.write("\x1b[K")

    def pasteyank(self):
        """ Pastes the yank buffer """
        map(self.input, self.yank)

    def prevcmd(self):
        """ Inserts the previously entered command. Repeated usage moves
        backward through the command history. """
        self.moveto(self.homecol())
        self.term.write("\x1b[K")
        if self.histpos < len(self.history):
            self.line = self.history[self.histpos]
            self.histpos += 1
        else:
            self.term.write("\x07")
        self.term.write(self.line)
        self.moveto(self.endcol())

    def specdup(self, c):
        """ Acts as if a special character c was recieved as input """
        if c in self.special:
            self.special[c]()

    # input methods

    def write(self, c):
        """ Writes a single character to input """
        pos = self.col - len(self.prompt)
        self.line = self.line[:pos] + c + self.line[pos:]
        self.term.write(c)
        self.col += 1
    
    def entered(self):
        """ Called when RETURN is passed to the terminal. Clears the input line
        and prints a new prompt """
        self.term.write("\n")
        self.active = False
        self.histpos = 0
        self.history = [self.line] + self.history
        self.inputcb(self.line)
        self.putprompt()

    def input(self, c):
        """ Argument 'c' should be a VT102 input sequence """
        if c in self.special:
            self.special[c]()
        elif c in string.printable:
            self.write(c)

class logcons:
    """
    A simple logger and console class which, when attatched to an AATE
    terminal, can print log strings and perform basic input.

    This logger supports prefixes and colors for varying log levels.

    Prefixes are stored in the self.prefixes dictionary, and are just
    strings. They can be set with the setprefix method.

    Colors are stored in the self.colors dictionary. These are strings
    containing a semicolon-separated list of VT102 display attributes.
    """

    def __init__(self, prompt, term, callback):
        """
        prompt, term, and callback are passed right throught to readline
        """
        self.term = term
        self.prompt = readline(prompt, term, callback)

        self.prefixes = {"none": "",
                         "warning": "!! ",
                         "error": "EE "}
        self.colors = {"none": "0",
                       "warning": "0;33",
                       "error": "1;31"}

    def setprefix(self, mode, prefix):
        """ Prefix should be string """
        self.prefixes[mode] = prefix

    def setcolors(self, mode, colors):
        """ Colors should be a string which contains a set of arguments to go
        between \x1b[ and m """
        self.colors[mode] = colors

    def setlevel(self, mode, prefix, colors):
        """ Sets both the prefix and the color for a mode. This is the
        suggested way to create a mode """
        self.setprefix(mode, prefix)
        self.setcolors(mode, colors)

    def log(self, line, level="none"):
        """
        Interrutps the readline prompt to print the line. This method prints a
        newline as well. A log prefix and color are printed for the
        message. level should be a string referring to the key for these
        attributes in the self.prefixes and self.colors dictionaries.
        """
        self.prompt.interrupt()
        # this looks like line noise, but it's a formatting string which
        # contains escape sequences
        self.term.write("\x1b[%sm%s%s\x1b[0m\n" %
                        (self.colors[level],
                         self.prefixes[level],
                         line))
        self.prompt.resume()

    def pull(self):
        """
        Pulls input from the terminal, sending it to readline if readline is
        active. If readline is not active, it is simply ignored.
        """
        input = self.term.read()
        if self.prompt.active:
            self.prompt.input(input)
