#!/usr/bin/env python

"""
echolog.py -- AATE logger console test

Copyright (C) 2010 Alex Iadicicco

This file is a part of AATE

This Python script will create a logcons instance, and do some really dumb
logging and stuff of Pygame events.
"""

import traceback
import pygame
from pygame.locals import *

from aate import *

# change these to suit your needs
FONT="font.png"
TWIDE=8
THIGH=16

TERMWIDE=80
TERMHIGH=24

pygame.init()
root = pygame.display.set_mode((TERMWIDE*TWIDE, TERMHIGH*THIGH))

def inputcb(line):
    global lc, running
    print "recieved input", line
    if line == "exit":
        running = False
        lc.log("exiting")
    else:
        lc.log("got input!!! %s" % line, "watermelon")

t = term.terminal(TERMWIDE, TERMHIGH)
view = sdl.colorview(FONT, TWIDE, THIGH, t)
lc = logcons.logcons("] ", t, inputcb)

lc.setlevel("watermelon", ">>> ", "1;34")
lc.setcolors("warning", "1;33")

running = True
lasttype = QUIT
try:
    while running:
        for event in pygame.event.get():
            if event.type == QUIT:
                running = False
            elif event.type == KEYDOWN:
                view.keyboard(event)
                lc.pull()
            elif event.type == MOUSEMOTION:
                if lasttype != MOUSEMOTION:
                    lc.log("moving mouse!")
            elif event.type == MOUSEBUTTONDOWN:
                if event.button == 1:
                    lc.log("clicked first mouse button at %s" % (event.pos,), "warning")
                else:
                    lc.log("mouse button %s down at %s" % (event.button, event.pos), "error")
            elif event.type == KEYUP:
                pass
            elif event.type == MOUSEBUTTONUP:
                pass
            else:
                lc.log("other event", "warning")
            lasttype = event.type
        view.draw()
        root.blit(view.display, (0, 0))
        pygame.display.flip()
except:
    traceback.print_exc()
finally:
    pygame.quit()
    raise SystemExit
