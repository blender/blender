#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later

# Checks for defines which aren't used anywhere.

import os
import sys

PWD = os.path.dirname(__file__)
sys.path.append(os.path.join(PWD, "..", "utils_maintenance", "modules"))

from batch_edit_text import run

SOURCE_DIR = os.path.normpath(os.path.abspath(os.path.normpath(os.path.join(PWD, "..", "..", ".."))))

# TODO, move to config file
SOURCE_DIRS = (
    "source",
)

SOURCE_EXT = (
    # C/C++
    ".c", ".h", ".cpp", ".hpp", ".cc", ".hh", ".cxx", ".hxx", ".inl",
    # Objective C
    ".m", ".mm",
    # GLSL
    ".glsl",
)

words = set()
words_multi = set()
defines = {}

import re
re_words = re.compile("[A-Za-z_][A-Za-z_0-9]*")
re_defines = re.compile("^\\s*#define\\s+([A-Za-z_][A-Za-z_0-9]*)", re.MULTILINE)

# From
# https://stackoverflow.com/a/18381470/432509


def remove_comments(string):
    pattern = r"(\".*?\"|\'.*?\')|(/\*.*?\*/|//[^\r\n]*$)"
    # first group captures quoted strings (double or single)
    # second group captures comments (//single-line or /* multi-line */)
    regex = re.compile(pattern, re.MULTILINE | re.DOTALL)

    def _replacer(match):
        # if the 2nd group (capturing comments) is not None,
        # it means we have captured a non-quoted (real) comment string.
        if match.group(2) is not None:
            return ""  # so we will return empty to remove the comment
        else:  # otherwise, we will return the 1st group
            return match.group(1)  # capture
    return regex.sub(_replacer, string)


def extract_terms(fn, data_src):
    data_src_nocomments = remove_comments(data_src)
    for m in re_words.finditer(data_src_nocomments):
        words_len = len(words)
        m_text = m.group()
        words.add(m_text)
        if words_len == len(words):
            words_multi.add(m_text)

    for m in re_defines.finditer(data_src_nocomments):
        defines[m.group(1)] = fn

    # Don't edit the file.
    return None


run(
    directories=[os.path.join(SOURCE_DIR, d) for d in SOURCE_DIRS],
    is_text=lambda fn: fn.endswith(SOURCE_EXT),
    text_operation=extract_terms,
    # Can't be used if we want to accumulate in a global variable.
    use_multiprocess=False,
)

print("Found", len(defines), "defines, searching", len(words_multi), "terms...")
for fn, define in sorted([(fn, define) for define, fn in defines.items()]):
    if define not in words_multi:
        print(define, "->", fn)
