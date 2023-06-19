#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import sys

# TODO, get from QtCreator
TABSIZE = 4

txt = sys.stdin.read()
data = txt.split("\n")

maxlen = 0
# tabs -> spaces
for i, l in enumerate(data):
    l = l.replace("\t", " " * TABSIZE)
    l = l.rstrip()
    maxlen = max(maxlen, len(l))
    data[i] = l

for i, l in enumerate(data):
    ws = l.rsplit(" ", 1)
    if len(l.strip().split()) == 1 or len(ws) == 1:
        pass
    else:
        j = 1
        while len(l) < maxlen:
            l = (" " * j).join(ws)
            j += 1
    data[i] = l

# add tabs back in
for i, l in enumerate(data):
    ls = l.lstrip()
    d = len(l) - len(ls)
    indent = ""
    while d >= TABSIZE:
        d -= TABSIZE
        indent += "\t"
    if d:
        indent += (" " * d)
    data[i] = indent + ls


print("\n".join(data), end="")
