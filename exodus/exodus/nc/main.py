import curses
import time
import traceback
import sys

import exodus.nc.view

class Timer(object):
    @classmethod
    def sync(cls):
        cls.now = time.time()

    def __init__(self, cb, delay, repeat=False):
        self.cb = cb
        self.delay = delay
        self.repeat = repeat
        self.alive = True

        Timer.sync()
        self.deadline = Timer.now + self.delay

    def get_delay(self):
        if Timer.now > self.deadline:
            return 0
        return self.deadline - Timer.now

    def try_fire(self):
        if self.get_delay() > 0:
            return False

        self.cb()

        if self.repeat:
            self.deadline += self.delay
        else:
            self.alive = False

        return True

class Main(object):
    def __init__(self):
        self.timers = []
        self.v = None

    def main(self):
        self.running = True

        self.v = exodus.nc.view.MainView(self, self.scr)

        while self.running:
            self.v.draw()
            self.scr.refresh()

            Timer.sync()
            if len(self.timers) == 0:
                delay = -1
            else:
                delay = int(min(t.get_delay() for t in self.timers) * 1000)
            self.scr.timeout(delay)

            ch = self.scr.getch()
            if ch == -1:
                Timer.sync()
                for t in self.timers:
                    t.try_fire()
                self.timers = [t for t in self.timers if t.alive]
            else:
                self.on_input(ch)

    def on_input(self, ch):
        self.v.on_input(ch)

    def add_timer(self, t):
        Timer.sync()
        self.timers.append(t)

    def run(self):
        exbuf = ''

        self.scr = curses.initscr()

        curses.noecho()
        curses.cbreak()
        self.scr.keypad(True)

        try:
            self.main()
        except:
            exbuf = traceback.format_exc()

        curses.nocbreak()
        self.scr.keypad(False)
        curses.echo()

        curses.endwin()

        sys.stderr.write(exbuf)

    def stop(self):
        self.running = False
