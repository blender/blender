#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later

import sys

# TODO, accept other characters as args

txt = sys.stdin.read()
print("(", end="")
print(txt, end="")
print(")", end="")
