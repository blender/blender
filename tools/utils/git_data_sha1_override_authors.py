#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
The intent of this map is to be able to a single canonical author
for every ``Name <email>`` combination.
"""

__all__ = (
    "sha1_authors_map",
)

from typing import (
    Dict,
    Tuple,
)


def sha1_authors_map() -> Dict[bytes, Tuple[str, ...]]:
    """
    Return SHA1 to authors map.
    """
    # Mapping from a commit hash to additional authors.
    # Fully overwrite authors gathered from git commit info.
    # Intended usage: Correction of info stored in git commit itself.
    # Note that the names of the authors here are assumed fully valid and usable as-is.
    return {
        # Format: {full_git_hash: (tuple, of, authors),}.
        # Author was: `blender <blender@localhost.localdomain>`.
        b"ba3d49225c9ff3514fb87ae5d692baefe5edec30": ("Sergey Sharybin <sergey@blender.org>", ),
        # Author was: `Author Name <email@address.com>`.
        b"4b6a4b5bc25bce10367dffadf7718e373f81f299": ("Antonio Vazquez <blendergit@gmail.com>", ),
    }
