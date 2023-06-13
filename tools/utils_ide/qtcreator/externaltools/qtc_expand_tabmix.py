#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
import sys

# TODO, get from QtCreator
TABSIZE = 4

txt = sys.stdin.read()
data = txt.split("\n")

for i, l in enumerate(data):
    l_lstrip = l.lstrip("\t")
    l_lstrip_tot = (len(l) - len(l_lstrip))
    if l_lstrip_tot:
        l_pre_ws, l_post_ws = l[:l_lstrip_tot], l[l_lstrip_tot:]
    else:
        l_pre_ws, l_post_ws = "", l
    # expand tabs and remove trailing space
    data[i] = l_pre_ws + l_post_ws.expandtabs(TABSIZE).rstrip(" \t")


print("\n".join(data), end="")
