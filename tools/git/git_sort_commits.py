#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later

"""
Sort commits by date (oldest first)
(useful for collecting commits to cherry pick)

Example:

    git_sort_commits.py < commits.txt
"""

import sys
import os

MODULE_DIR = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", "utils"))
SOURCE_DIR = os.path.normpath(os.path.join(MODULE_DIR, "..", "..", ".git"))

sys.path.append(MODULE_DIR)

import git_log


def main():

    import re
    re_sha1_prefix = re.compile("^r[A-Z]+")

    def strip_sha1(sha1):
        # strip rB, rBA ... etc
        sha1 = re.sub(re_sha1_prefix, "", sha1)
        return sha1

    commits = [git_log.GitCommit(strip_sha1(l), SOURCE_DIR)
               for l in sys.stdin.read().split()]

    commits.sort(key=lambda c: c.date)

    for c in commits:
        print(c.sha1)


if __name__ == "__main__":
    main()
