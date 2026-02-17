#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
Convert C++ Style Comments:

    // hello
    // world

To This:

    /* hello
     * world
     */
"""

import sys

txt = sys.stdin.read()
data = txt.split("\n")
# TODO. block comments


# first detect blocks
def block_data(data, i_start):
    i_begin = -1
    i_index = -1
    i_end = -1
    i = i_start
    while i < len(data):
        l = data[i]
        if "//" in l:
            i_begin = i
            i_index = l.index("//")
            break
        i += 1
    if i_begin != -1:
        i_end = i_begin
        for i in range(i_begin + 1, len(data)):
            l = data[i]
            if "//" in l and l.lstrip().startswith("//") and l.index("//") == i_index:
                i_end = i
            else:
                break

        if i_begin != i_end:
            # do a block comment replacement
            data[i_begin] = data[i_begin].replace("//", "/*", 1)
            for i in range(i_begin + 1, i_end + 1):
                data[i] = data[i].replace("//", " *", 1)
            data[i_end] = "{:s} */".format(data[i_end].rstrip())
    # done with block comment, still go onto do regular replace
    return max(i_end, i_start + 1)


i = 0
while i < len(data):
    i = block_data(data, i)

i = 0
while "//" not in data[i] and i > len(data):
    i += 1


for i, l in enumerate(data):
    if "//" in l:  # should check if it's in a string.

        text, comment = l.split("//", 1)

        l = "{:s}/* {:s} */".format(text, comment.strip())

        data[i] = l


print("\n".join(data), end="")
