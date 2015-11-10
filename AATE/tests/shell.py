#!/usr/bin/env python

"""
shell.py -- AATE shell test

Copyright (C) 2010 Alex Iadicicco

This file is a part of AATE

This is a poorly written piece of Python code which creates an AATE terminal
and links it to a shell and a view. This is essentially an exclusive AATE
terminal application.

NOTE: this example will only work on installations which provide the pty
module. I know it works on Linux and not Windows, but I haven't tried it on a
Mac yet.
"""

import sys
import os
import pty
import threading
from threading import Thread
import traceback
import pygame
from pygame.locals import *

from aate import *

# change these to suit your needs
FONT="font.png"
TWIDE=8
THIGH=16

# changing these dimensions could lead to unexpected behavior for curses and
# termcap applications
TERMWIDE=80
TERMHIGH=24

pygame.init()
root = pygame.display.set_mode((TERMWIDE*TWIDE, TERMHIGH*THIGH))

t = term.terminal(TERMWIDE, TERMHIGH)
view = sdl.colorview(FONT, TWIDE, THIGH, t)

running = True

class termwrapin(Thread):
    def __init__(self, t, fd):
        Thread.__init__(self)
        self.t = t
        self.fd = fd
    def run(self):
        global running
        while running:
            self.t.write(os.read(self.fd, 2048))
class termwrapout(Thread):
    def __init__(self, t, fd):
        Thread.__init__(self)
        self.t = t
        self.fd = fd
    def run(self):
        global running
        while running:
            os.write(self.fd, self.t.read())

pid, fd = pty.fork()
if pid == 0:
    os.putenv("TERM", "vt102")
    pty.spawn("/bin/bash")
else:
    win = termwrapin(t, fd)
    wout = termwrapout(t, fd)
    win.start()
    wout.start()

    try:
        while running:
            for event in pygame.event.get():
                if event.type == QUIT:
                    running = False
                if event.type == KEYDOWN:
                    view.keyboard(event)
            view.draw()
            root.blit(view.display, (0, 0))
            pygame.display.flip()
    except:
        traceback.print_exc()
    finally:
        pygame.quit()
        raise SystemExit
