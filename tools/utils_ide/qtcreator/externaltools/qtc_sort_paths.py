#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
import sys

txt = sys.stdin.read()
data = txt.split("\n")


class PathCMP:

    def __init__(self, path):
        path = path.strip()

        self.path = path
        if path.startswith("."):
            path = path[1:]

        if path.startswith("/"):
            path = path[1:]
        if path.endswith("/"):
            path = path[:-1]

        self.level = self.path.count("..")
        if self.level == 0:
            self.level = (self.path.count("/") - 10000)

    def __eq__(self, other):
        return self.path == other.path

    def __lt__(self, other):
        return self.path < other.path if self.level == other.level else self.level < other.level

    def __gt__(self, other):
        return self.path > other.path if self.level == other.level else self.level > other.level


data.sort(key=lambda a: PathCMP(a))

print("\n".join(data), end="")
