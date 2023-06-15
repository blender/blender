#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import sys

txt = sys.stdin.read()
data = txt.split("\n")

# Check if we're if0
is_comment = False
for l in data:
    l_strip = l.strip()
    if l_strip:
        if l_strip.startswith("#if 0"):
            is_comment = True
        else:
            is_comment = False
        break

if is_comment:
    pop_a = None
    pop_b = None
    for i, l in enumerate(data):
        l_strip = l.strip()

        if pop_a is None:
            if l_strip.startswith("#if 0"):
                pop_a = i

        if l_strip.startswith("#endif"):
            pop_b = i

    if pop_a is not None and pop_b is not None:
        del data[pop_b]
        del data[pop_a]
else:
    while data and not data[-1].strip():
        data.pop()
    data = ["#if 0"] + data + ["#endif\n"]


print("\n".join(data), end="")
