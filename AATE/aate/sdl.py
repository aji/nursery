"""
sdl.py -- AATE interface through SDL using Pygame

Copyright (c) 2010 Alex Iadicicco

This file is a part of AATE.

AATE is provided without warranty under the GNU General Public License. A copy
of this license can be found in the COPYING file in the project root directory,
or at <http://www.gnu.org/licenses/gpl.html>

This module provides an interface to an AATE terminal through SDL via
pygame. To use this module you must have pygame installed. If you do not have
pygame, an import error is raised.

As of v0.2, this module only supports tile-based character sets.
"""

try:
    import pygame
    from pygame.locals import *
except ImportError:
    raise ImportError("You do not have Pygame installed!")

import term

def load_surface(filename, colorkey):
    """ Loads a surface and gives it the specified color key. """
    try:
        ret = pygame.image.load(filename)
        ret.set_colorkey(colorkey)
    except pygame.error, message:
        print "Couldn't load image:", filename
        raise SystemExit, message
    return ret

# default color key, magenta
_defckey=(255,0,255)

class termview:
    """
    Maintains a single Pygame surface reflecting terminal data. Graphics
    are tile based. More information is given in the __init__
    documentation.

    To access the display surface, simply accesss the "display" property.
    """

    def __init__(self, font, fwide, fhigh, terminal,\
                     colorkey=_defckey, cursor=True,\
                     ccolor=(255,0,0), cphase=400):
        """
        Initalizes the terminal view. Parameters are as follows:

        font -- a path to an image representing a font tileset
        fwide -- width of a single glyph
        fhigh -- height of a single glyph
        terminal -- a term.terminal instance
        colorkey -- the colorkey to use on the font.
        cursor -- if True, draws a cursor
        ccolor -- the cursor color. white by default. color is subtracted
        cphase -- number of milliseconds per cursor blink. 0 for no blink
        """
        self.terminal = terminal

        self.font = load_surface(font, colorkey)
        self.fwide = fwide
        self.fhigh = fhigh
        self.xmod = self.font.get_width() / self.fwide
        self.clip = pygame.Rect(0, 0, self.fwide, self.fhigh)

        self.cursor = True
        self.cshowing = True
        self.ccolor = ccolor
        self.cphase = cphase
        self.cticks = pygame.time.get_ticks()

        # linux framebuffer colors
        self.palette = [[[0,   0,   0  ],
                         [170, 0,   0  ],
                         [0,   170, 0  ],
                         [170, 85,  0  ],
                         [0,   0,   170],
                         [170, 0,   170],
                         [0,   170, 170],
                         [170, 170, 170]],
                        [[85,  85,  85 ],
                         [255, 85,  85 ],
                         [85,  255, 85 ],
                         [255, 255, 85 ],
                         [85,  85,  255],
                         [255, 85,  255],
                         [85,  255, 255],
                         [255, 255, 255]]]

        self.display = pygame.Surface((self.fwide * self.terminal.wide,\
                                       self.fhigh * self.terminal.high))

    def clear(self, color=(0,0,0)):
        """ Clears the display to color, defaults to black. """
        self.display.fill(color)
        
    def writeglyph(self, c, m, col, row):
        """
        Draws the glyph specified by c to the display at row, col. row and col
        are multiplied by fwide and fhigh to get the actual screen
        coordinates. c should be an integer.
        """
        self.clip.left = c % self.xmod * self.fwide
        self.clip.top = int(c / self.xmod) * self.fhigh
        fg, bg = self.getcolors(m)
        self.drawglyph(col * self.fwide, row * self.fhigh, fg, bg)

    def getcolors(self, m):
        """
        Returns a 2-tuple of RGB tuples for the given display mode. See the documentation
        for term for more information about what this argument means
        """
        conv = ["normal", "bright"]
        fg, bg, brfg, brbg, inv, ul = m
        retfg = self.palette[brfg][fg]
        retbg = self.palette[brbg][bg]
        if inv == 1:
            retfg, retbg = retbg, retfg
        return retfg, retbg

    def drawglyph(self, x, y, fg, bg):
        self.display.blit(self.font, (x, y), self.clip)

    def dispul(self, row, col):
        """ Renders an underline at the specified row and column """
        x = col * self.fwide
        y = (row + 1) * self.fhigh - 1
        self.display.fill(_deffg, (x, y, self.fwide, 1))

    def draw(self):
        """
        Draws the entire terminal to the display. Automatically clears it
        """
        self.clear()
        for row in range(self.terminal.high):
            for col in range(self.terminal.wide):
                c = self.terminal.getchr(row, col)
                m = self.terminal.getdisp(row, col)
                self.writeglyph(ord(c), m, col, row)
        if self.cursor:
            self.drawcursor()

    def drawcursor(self):
        """ Draws the cursor to the screen """
        now = pygame.time.get_ticks()
        if now - self.cticks > self.cphase and self.cphase != 0:
            self.cshowing = not self.cshowing
            self.cticks = now
        if self.cshowing:
            self.display.fill(self.ccolor, (self.terminal.cur[1] * self.fwide,\
                                            self.terminal.cur[0] * self.fhigh,\
                                            self.fwide, self.fhigh))

    def keyboard(self, event):
        """
        Processes a Pygame keyboard event, turning it into acceptable input for
        the terminal.
        """
        if len(event.unicode) == 1:
            s = str(event.unicode)
            self.terminal.input(s)
            self.cticks = pygame.time.get_ticks()
            self.cshowing = True

class colorview(termview):
    """
    This subclass of termview overrides a couple methods from the regular
    termview class enabling color rendering. This class goes a little bit
    slower than the termview class.
    """

    def __init__(self, font, fwide, fhigh, terminal,\
                     colorkey=_defckey, cursor=True,\
                     ccolor=(255,0,0), cphase=400):
        termview.__init__(self, font, fwide, fhigh, terminal,\
                              colorkey, cursor, ccolor, cphase)
        self.display = self.display.convert(8)
        self.font = self.font.convert(8)
        self.font.set_colorkey(None)
        self.fgbgcolors((255,255,255), colorkey)

    def fgbgcolors(self, fg, bg):
        """ Sets the foreground and background colors from the source image """
        self.fgidx = self.font.map_rgb(fg)
        self.bgidx = self.font.map_rgb(bg)

        self.lastfg = fg
        self.lastbg = bg
        
        self.broken = False
        if self.font.get_bitsize() > 8:
            self.broken = True
            print "broken!"

    def drawglyph(self, x, y, fg, bg):
        if self.lastfg != fg:
            self.font.set_palette_at(self.fgidx, fg)
            self.lastfg = fg
        if self.lastbg != bg:
            self.font.set_palette_at(self.bgidx, bg)
            self.lastbg = bg
        self.display.blit(self.font, (x, y), self.clip)
