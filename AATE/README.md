[license]: http://www.gnu.org/licenses/gpl.html "GNU General Public License"
[project]: http://github.com/aji/AATE "aji's AATE on GitHub"

# Alex's ANSI Terminal Emulator (AATE) v0.4

AATE (pronounced like 'ate' or 'eight') aims to be a sufficient emulator for a
VT102 terminal.

AATE is written entirely in Python.

A special `sdl` module is provided which allows easy integration with Pygame.

# Features

This package comse with a lot more than just an ANSI terminal emulator. Here
are some of AATE's features:

  * Support for most VT102 escape sequences.
  * Easy Pygame integration with the `sdl` module.
  * Color support.
  * A `logcons` module which provides a basic console with logging, line
    editing, and command history

# Test Scripts

Included is a "tests" directory which contains a few test scripts to
demonstrate some functionality of AATE.

After installing AATE you can run these by entering the "tests" directory
and running them like a normal python script, e.g. `python shell.py`

### shell.py

This script opens a shell and connects it to an AATE terminal. A couple ncurses
applications, like emacs and nano, have trouble, but vi, less, and nethack seem
to do fine.

NOTE: This only works on systems which provide the 'pty' module. I know this
*is* Linux and is *not* Windows, but I don't know about Mac OS X.

### echolog.py

This script creates an instance of an AATE logcons and connects it to an AATE
terminal. Entering a command at the prompt simply results in it being echoed to
both the terminal and stdout. Doing things to the display, like clicking it and
moving it, will result in output being printed to the display.

This should work on every system. Try using some of the readline line editing
and command history sequences.

# Installation

After obtaining a copy of the source code, AATE can be installed using the
distutils setup.py script. To install AATE, use the following command:

     python setup.py install

You will need to run this as root.

# Documentation

Docstrings are strewn throughout the code. Using a documentation generator tool
such as pydoc should be a sufficient solution.

# License and Copying

AATE is Copyright &copy; 2010 Alex Iadicicco.

AATE is provided without warranty under the GNU General Public License. A copy
of this license can be found in the COPYING file in the project root directory,
or [on GNU's website][license].
