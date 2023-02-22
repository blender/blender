#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright 2009 Blender Foundation

import sys

argv = sys.argv[:]

strip_byte = False
if "--strip-byte" in argv:
    argv.remove("--strip-byte")
    strip_byte = True

if len(argv) < 2:
    sys.stdout.write("Usage: ctodata <c_file> [--strip-byte]\n")
    sys.exit(1)

filename = argv[1]

try:
    fpin = open(filename, "r")
except:
    sys.stdout.write("Unable to open input %s\n" % argv[1])
    sys.exit(1)

data = fpin.read().rsplit("{")[-1].split("}")[0]
data = data.replace(",", " ")
data = data.split()
data = [int(v) for v in data]

if strip_byte:
    # String data gets trailing byte.
    last = data.pop()
    assert last == 0

data = bytes(data)

dname = filename + ".ctodata"

sys.stdout.write("Making DATA file <%s>\n" % dname)

try:
    fpout = open(dname, "wb")
except:
    sys.stdout.write("Unable to open output %s\n" % dname)
    sys.exit(1)

size = fpout.write(data)

sys.stdout.write("%d\n" % size)
