#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
When a source file declares a struct which isn't used anywhere else in the file.
Remove it.

There may be times this is needed, however they can typically be removed
and any errors caused can be added to the headers which require the forward declarations.
"""

import os
import sys
import re

from typing import (
    Dict,
    Optional,
)

PWD = os.path.dirname(__file__)
sys.path.append(os.path.join(PWD, "modules"))

from batch_edit_text import run

SOURCE_DIR = os.path.normpath(os.path.abspath(os.path.normpath(os.path.join(PWD, "..", ".."))))

# TODO: move to configuration file.
SOURCE_DIRS = (
    "source",
    os.path.join("intern", "ghost"),
)

SOURCE_EXT = (
    # C/C++
    ".c", ".h", ".cpp", ".hpp", ".cc", ".hh", ".cxx", ".hxx", ".inl",
    # Objective C
    ".m", ".mm",
)

re_words = re.compile("[A-Za-z_][A-Za-z_0-9]*")
re_match_struct = re.compile(r"struct\s+([A-Za-z_][A-Za-z_0-9]*)\s*;")


def clean_structs(fn: str, data_src: str) -> Optional[str]:
    from pygments.token import Token
    from pygments import lexers

    word_occurance: Dict[str, int] = {}

    lex = lexers.get_lexer_by_name("c++")
    lex.get_tokens(data_src)

    ty_exact = (Token.Comment.Preproc, Token.Comment.PreprocFile)

    for ty, _text in lex.get_tokens(data_src):
        if ty not in ty_exact:
            if ty in Token.String:  # type: ignore
                continue
            if ty in Token.Comment:  # type: ignore
                continue

        for w_match in re_words.finditer(data_src):
            w = w_match.group(0)
            try:
                word_occurance[w] += 1
            except KeyError:
                word_occurance[w] = 1

    lines = data_src.splitlines(keepends=True)

    i = 0
    while i < len(lines):
        m = re_match_struct.match(lines[i])
        if m is not None:
            struct_name = m.group(1)
            if word_occurance[struct_name] == 1:
                print(struct_name, fn)
                del lines[i]
                i -= 1

        i += 1

    data_dst = "".join(lines)
    if data_src != data_dst:
        return data_dst
    return None


run(
    directories=[os.path.join(SOURCE_DIR, d) for d in SOURCE_DIRS],
    is_text=lambda fn: fn.endswith(SOURCE_EXT),
    text_operation=clean_structs,
    use_multiprocess=False,
)
