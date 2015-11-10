import logging as L

import os

from config import conf

import db

def initialize(name):
    os.chdir(str(conf.dir.run))

    root = L.getLogger()
    hdlr = L.FileHandler(str(conf['{}.log.path'.format(name)]))
    fmt = L.Formatter(fmt=str(conf.log.format), datefmt=str(conf.log.datefmt))

    root.setLevel(str(conf['{}.log.level'.format(name)]))
    root.addHandler(hdlr)
    hdlr.setFormatter(fmt)

    db.initialize()
