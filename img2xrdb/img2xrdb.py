#!/usr/bin/env python2
#
# you might have to change the shebang to just 'python', but the
# important thing is that you run the version with the Python Imaging
# Library installed

from PIL import Image
import sys, os

# This is how the pixels are to be arranged in the image:
#  8  9 10 11 12 13 14 15
#  0  1  2  3  4  5  6  7

# convert a pixel tuple to a hex string (leading # not included)
def hexify(pixel):
    return "{0[0]:02x}{0[1]:02x}{0[2]:02x}".format(pixel)

# convert a color number (12 for example) to coordinates
def coord(colornum):
    c = colornum % 16  # bounds checking
    x = c % 8
    y = (c >> 3) ^ 1
    return (x, y)

# get an xrdb string for the colors
def xrdb(colornum, hexstr):
    return "*color{0}: #{1}\n".format(colornum, hexstr)

# iterate 0 <= x < 16 and write the strings with wfn()
def colors(img, wfn):
    for colornum in range(16):
	pixel = img.getpixel(coord(colornum))
	wfn(xrdb(colornum, hexify(pixel)))

# use first argument as image filename
if __name__ == "__main__":
    if len(sys.argv) < 2:
	sys.stderr.write("usage: {0} IMAGE [PREAMBLE]\n".format(sys.argv[0]))
	exit(1)

    if len(sys.argv) > 2:
	try:
	    f = open(sys.argv[2], "r")
	    for line in f:
		sys.stdout.write(line)
	except IOError:
	    sys.stderr.write("Couldn't open preamble; ignoring...")

    try:
	img = Image.open(sys.argv[1])
    except IOError:
	sys.stderr.write("Couldn't open {0}\n".format(sys.argv[1]))
	exit(1)

    colors(img, sys.stdout.write)

