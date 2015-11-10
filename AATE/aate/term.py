"""
aterm.py -- Alex's ANSI Terminal Emulator

Copyright (c) 2010 Alex Iadicicco

This file is a part of AATE.

AATE is provided without warranty under the GNU General Public License. A copy
of this license can be found in the COPYING file in the project root directory,
or at <http://www.gnu.org/licenses/gpl.html>

This module provides a class which represents an ANSI terminal compliant with
X3.64 standards.
"""

import array
import string
import sys

class control:
    """
    Terminal control

    I used to keep all of this code in the terminal class, but it just got out
    of hand. Instead, every terminal creates an instance of this class, and
    delegates certain responsibilities to this class.

    This class's biggest responsibility is input interpretation.

    The terminal still maintains its own cursor.

    Control states (defined by 'state') are any of the following:
      0 (normal): waiting for ESC
      1 (escape): determining whether to accumulate or not
      2 (accum): acculumating, waiting for command
    """

    def __init__(self, term):
        self.term = term
        self.reset()

    def reset(self):
        """ Resets control settings """
        self.config()
        self.savecur()

    def config(self):
        """ All variables are initialized here """
        self.esc = "\x1b"
        self.state = 0 # normal
        self.accum = ""
        self.cursaved = [0, 0]

        self.special = {"\x0a": self.term.linefeed,
                        "\x0d": self.term.carriage,
                        "\x09": self.term.horiztab,
                        "\x08": self.term.backspace,
                        "\x07": self.term.bell}
        self.accumset = "["
        self.briefset = {"7": self.savecur,
                         "8": self.restorecur}
        self.ignoreset = "=>)("
        self.argset = "0123456789:;<=>?"
        self.argdelim = ";"
        self.editset = "PL"
        self.editcmd = {"P": self.term.delchr,
                        "L": self.term.insrow}
        self.moveset = "ABCDHf"
        self.mvrecurse = {"A": (-1, 0), "B": (1, 0),
                          "C": (0,  1), "D": (0, -1)}
        self.mvabs = "Hf"
        self.clrset = "JK"
        self.configset = "hlmr"
        self.reqset = "csu"

    # cursor position

    def savecur(self):
        """ Saves the cursor position """
        self.cursaved = self.term.cur[:]
        self.dispsaved = self.term.curdisp[:]

    def restorecur(self):
        """ Restores the last saved cursor position """
        self.term.cur = self.cursaved[:]
        self.term.curdisp = self.dispsaved[:]

    # control

    def process(self, c):
        """ Processes a single character. Returns False if the character should
        be echoed to the terminal, not processed. """
        if self.state == 0:
            if c in self.special:
                self.special[c]()
                return True
            elif c == self.esc:
                self.state = 1
                return True
            elif not c in string.printable:
                print "unhandled, unprintable character:", repr(c)
            return False
        if self.state == 1:
            if c in self.accumset:
                self.accum = ""
                self.state = 2
                return True
            if c in self.briefset:
                self.briefset[c]()
                self.state = 0
                return True
            if c in self.ignoreset:
                self.state = 0
                return True
            return False
        if self.state == 2:
            if not c in self.argset:
                self.docmd(c, self.accum)
                self.state = 0
            else:
                self.accum += c
            return True

    # high level command operation(s)

    def docmd(self, cmd, args):
        """ Performs a terminal operation, the given command and arguments """
        args = args.split(self.argdelim)
        if cmd in self.moveset:
            self.domove(cmd, args)
        elif cmd in self.clrset:
            self.doclr(cmd, args)
        elif cmd in self.configset:
            self.doconfig(cmd, args)
        elif cmd in self.reqset:
            self.doreq(cmd, args)
        elif cmd in self.editset:
            self.doedit(cmd, args)
        else:
            self.unhandled(cmd, args)

    def unhandled(self, cmd, args):
        """ Processes an unhandled command, usually by printing it to stdout """
        print "Unhandled Command: '%s', %s" % (cmd, args)

    # move methods

    def domove(self, cmd, args):
        """ Performs a move command """
        for i in range(len(args)):
            if len(args[i]) == 0:
                args[i] = "1"
        if cmd in self.mvrecurse:
            car = int(args[0])
            self.term.moveby(*self.mvrecurse[cmd])
            if car > 1:
                self.domove(cmd, [str(car-1)])
        if cmd in self.mvabs:
            if len(args) < 2:
                args = ["1", "1"]
            row = int(args[0]) - 1
            col = int(args[1]) - 1
            self.term.cur = [row, col]

    # clear methods

    def doclr(self, cmd, args):
        """ Performs a clear command, one of either K or J """
        row, col = self.term.cur
        if len(args[0]) == 0:
            args[0] = "0"
        if args[0] == "0" or args[0] == "2":
            self.clrrange(row, range(col, self.term.wide))
            if cmd == "J":
                for i in range(row, self.term.high):
                    self.clrrange(i, range(self.term.wide))
        if args[0] == "1" or args[0] == "2":
            self.clrrange(row, range(0, col+1))
            if cmd == "J":
                for i in range(0, row+1):
                    self.clrrange(i, range(self.term.wide))

    def clrrange(self, row, cols):
        """ Clears the characters in the specified row and list of
        columns. This function iterates through cols, sending each parameter to
        the terminal to clear """
        if row == -1:
                row = self.term.cur[0]
        for col in cols:
            self.term.setchr(self.term.blank, self.term.dispclr(), row, col)

    # config methods

    def doconfig(self, cmd, args):
        if cmd == "m":
            for i in args:
                if len(i) == 0:
                    i = "0"
                self.term.dispmode(int(i))
        elif cmd == "h":
            for i in args:
                if i in self.term.modes:
                    self.term.modes[i] = True
                else:
                    print "h: unknown mode", i
        elif cmd == "l":
            for i in args:
                if i in self.term.modes:
                    self.term.modes[i] = False
                else:
                    print "l: unknown mode", i
        elif cmd == "r":
            self.term.setscroll(int(args[0])-1, int(args[1])-1)
        else:
            self.unhandled(cmd, args)

    # request methods

    def doreq(self, cmd, args):
        if cmd == "s":
            self.savecur()
        elif cmd == "u":
            self.restorecur()
        else:
            self.unhandled(cmd, args)

    # edit methods

    def doedit(self, cmd, args):
        if len(args[0]) == 0:
            args[0] = "1"
        if cmd in self.editcmd:
            car = int(args[0])
            for i in range(car):
                self.editcmd[cmd]()

class terminal:
    """
    An instance of a terminal.

    Note that once a terminal has been created and its size has been decided
    on, it cannot be resized.

    This type implements a 'write' method which behaves just like a file
    object's write method. This allows this class to function as sys.stdout or
    sys.stderr. Output can also be written with the 'putchar' method which puts
    a single character at the character position and advances it.

    The method of input for this terminal is not like sys.stdin, however. If
    you were to call sys.stdin.readline(), the function would hang until a
    newline had been entered. This readline method will return 'None'
    immediately if a newline has not been entered yet, and will return and
    clear the entire input buffer up to and including the first newline

    The option to hook a callback on a newline is an option as well. To do
    this, use the "inputhook" method.

    The only way to send input to the terminal is character per character. To
    do this, use the "input" method.

    To differentiate between general terminal modes and display modes, "mode"
    is used to refer to terminal modes, and "disp" is used to refer to display
    modes. The display mode for a single character is a byte array of length
    6. The 6 items are:
      [ foreground, background, bright fg, bright bg, inverse, underline ]
    """

    def __init__(self, wide=80, high=24, echoing=False, blank=" "):
        """
        Initializes the terminal.

        wide -- width of the terminal in characters
        high -- height of the terminal in characters
        echoing -- if True, enables input echoing
        blank -- the clear character
        """
        self.wide = wide
        self.high = high
        self.echoing = echoing
        self.blank = blank

        self.inbuf = ""
        self.inputhook(None)

        self.modes = {"2":  True,  # Keyboard Lock
                      "4":  False, # Insert Mode
                      "20": False, # Linefeed/Newline
                      "?1": True,  # Cursor mode
                      }
        self.dispclrtemplate = array.array("B", [7, 0, 0, 0, 0, 0])

        self.reset()
        self.ctl = control(self)

    # clear methods

    def clrchr(self, len):
        """ Returns an array of clear characters. """
        return array.array("c", [self.blank for x in range(len)])

    def nodisp(self, len):
        """ Returns an array of 'no mode' display modes """
        return [self.dispclr() for x in range(len)]
    
    def dispclr(self):
        """ Returns a single empty display array """
        return array.array("B", self.dispclrtemplate)

    def reset(self):
        """ Resets the terminal. """
        self.data = self.clrchr(self.wide * self.high)
        self.disp = self.nodisp(self.wide * self.high)
        self.scrlow = 0
        self.scrhigh = self.high - 1
        self.cur = [0, 0] # [row, col]
        self.resetdisp()

    def resetdisp(self):
        """ Resets the display mode """
        self.curdisp = self.dispclr()

    def dispmode(self, n):
        """ Sets a display mode. n should be an ANSI display mode """
        if n == 0:
            self.resetdisp()
        elif n in range(30, 38):
            self.curdisp[0] = n - 30
        elif n in range(40, 48):
            self.curdisp[1] = n - 40
        elif n in [1, 2, 5, 6]:
            ## this code is a little obfuscated, so I'll comment it...
            # new will be 0 for even values, and 1 for odd values
            new = n % 2
            # n will be either 2 or 3
            n = n / 5 + 2
            self.curdisp[n] = new
        elif n == 7:
            self.curdisp[4] = True
        elif n == 4:
            self.curdisp[5] = True

    # access methods

    def getchr(self, row, col):
        """ Returns the character at col and row. Behavior for values outside
        of the width and height is undefined, so be careful. """
        return self.data[row * self.wide + col]

    def setchr(self, c, m, row, col):
        """ Sets the character at col and row to chr. This is an internal use
        function only. Behavior for values outside of the width and height is
        undefined. """
        self.data[row * self.wide + col] = c
        self.setdisp(m, row, col)

    def getdisp(self, row, col):
        return self.disp[row * self.wide + col]
    
    def setdisp(self, m, row, col):
        self.disp[row * self.wide + col] = m[:]

    def setscroll(self, low, high):
        self.scrlow = low
        self.scrhigh = high

    # cursor methods
    
    def moveto(self, row, col):
        """ Moves the cursor to the specified row and column. There is no error
        checking in this function. Behavior outside the range is undefined. """
        self.cur = [row, col]

    def moveby(self, drow, dcol):
        row, col = self.cur
        col += dcol
        row += drow
        if col < 0: col = 0
        if row < self.scrlow: row = 0
        if not col < self.wide: col = self.wide - 1
        if row > self.scrhigh: row = self.high - 1
        self.cur = [row, col]

    # high level cursor methods

    def scroll(self):
        """ Scrolls the data up by one line """
        for i in range(self.scrlow, self.scrhigh + 1):
            self.copyrow(i + 1, i, True)

    def copyrow(self, src, dest, clear=False):
        """ Copies both the characters and display properties of src to
        dest. src gets cleared if clear is True """
        sbeg = src * self.wide
        send = (src + 1) * self.wide
        dbeg = dest * self.wide
        dend = (dest + 1) * self.wide
        self.data[dbeg:dend] = self.data[sbeg:send]
        self.disp[dbeg:dend] = self.disp[sbeg:send]
        if clear:
            self.data[sbeg:send] = self.clrchr(send - sbeg)
            self.disp[sbeg:send] = self.nodisp(send - sbeg)

    def advance(self):
        """ Advances the cursor. """
        self.cur[1] += 1
        if not self.cur[1] < self.wide:
            self.carriage()

    def carriage(self):
        """ Performs a carriage return """
        self.cur[1] = 0

    def linefeed(self):
        """ Moves to the next line """
        self.cur[0] += 1
        while not self.cur[0] < self.scrhigh + 1:
            self.scroll()
            self.cur[0] -= 1
        if self.modes["20"]:
            self.carriage()

    def backspace(self):
        """ Moves the cursor back one """
        self.moveby(0, -1)

    def horiztab(self):
        """ Horizontal tab, multiple of 8 """
        row, col = self.cur
        ncol = int((col + 8) / 8) * 8
        for i in range(ncol - col):
            self.outchr(self.blank)

    def bell(self):
        """ Rings the bell """
        sys.stdout.write(chr(7))
        sys.stdout.flush()

    # high level editing methods

    def delchr(self):
        """ Deletes the character under the cursor on the line, moving
        everything to the right left"""
        row, col = self.cur
        for i in range(col, self.wide-1):
            n = row, i + 1
            c, m = self.getchr(*n), self.getdisp(*n)
            self.setchr(self.getchr(*n), self.getdisp(*n), row, i)
        self.setchr(self.blank, self.dispclr(), row, self.wide-1)

    def inschr(self):
        """ Moves everything to the right of the cursor right to make room for
        a new character. The new character defaults to the blank character. """
        row, col = self.cur
        for i in range(self.wide-1, col, -1):
            n = row, i - 1
            self.setchr(self.getchr(*n), self.getdisp(*n), row, i)
        self.setchr(self.blank, self.dispclr(), *self.cur)

    def insrow(self):
        """ Inserts a blank row at the cursor """
        row, col = self.cur
        beg = row * self.wide
        end = (row + 1) * self.wide
        for i in range(self.scrhigh, row, -1):
            self.copyrow(i - 1, i)
        self.data[beg:end] = self.clrchr(end - beg)
        self.disp[beg:end] = self.nodisp(end - beg)

    # output (incoming data) methods

    def outchr(self, c):
        """ Puts the character argument onto the terminal and advances the
        cursor. """
        if not self.ctl.process(c):
            if self.modes["4"]:
                self.inschr()
            self.setchr(c, self.curdisp, *self.cur)
            self.advance()

    def write(self, str):
        """ Writes a string to the terminal. """
        map(self.outchr, str)

    # input (outgoing data) methods

    def ret(self):
        """ Generates a newline character depending on the newline mode """
        if self.modes["20"]:
            return "\r\n"
        return "\r"

    def input(self, c):
        """ Puts the character argument onto the end of the buffer. """
        if c == "\r":
            c = self.ret()
        self.inbuf += c
        if self.echoing:
            self.write(c)
        if c == self.ret() and self.inputcb is not None:
            self.inputcb(self.read())

    def inputhook(self, method):
        """ Sets the LF callback method to 'method.' Call with method as
        'None' to remove the input callback."""
        self.inputcb = method

    def read(self):
        """ Returns and clears the input buffer. """
        ret = self.inbuf
        self.inbuf = ""
        return ret

    def readline(self):
        """ Returns and clears the input buffer up to and including the first
        newline. Returns None if the input buffer does not have a newline. """
        if self.ret() not in self.inbuf:
            return None
        index = self.inbuf.find(self.ret()) + 1
        ret = self.inbuf[:index]
        self.inbuf = self.inbuf[index:]
        return ret
