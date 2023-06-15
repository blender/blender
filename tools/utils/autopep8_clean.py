#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import subprocess
import os
from os.path import join

from autopep8_clean_config import PATHS, PATHS_EXCLUDE

from typing import (
    Callable,
    Generator,
    Optional,
    Sequence,
)

# Useful to disable when debugging warnings.
USE_MULTIPROCESS = True

print(PATHS)
SOURCE_EXT = (
    # Python
    ".py",
)


def is_source_and_included(filename: str) -> bool:
    return (
        filename.endswith(SOURCE_EXT) and
        filename not in PATHS_EXCLUDE
    )


def path_iter(
        path: str,
        filename_check: Optional[Callable[[str], bool]] = None,
) -> Generator[str, None, None]:
    for dirpath, dirnames, filenames in os.walk(path):
        # skip ".git"
        dirnames[:] = [d for d in dirnames if not d.startswith(".")]

        for filename in filenames:
            if filename.startswith("."):
                continue
            filepath = join(dirpath, filename)
            if filename_check is None or filename_check(filepath):
                yield filepath


def path_expand(
        paths: Sequence[str],
        filename_check: Optional[Callable[[str], bool]] = None,
) -> Generator[str, None, None]:
    for f in paths:
        if not os.path.exists(f):
            print("Missing:", f)
        elif os.path.isdir(f):
            yield from path_iter(f, filename_check)
        else:
            yield f


def autopep8_format_file(f: str) -> None:
    print(f)
    subprocess.call((
        "autopep8",
        "--ignore",
        ",".join((
            # Info: Use `isinstance()` instead of comparing types directly.
            # Why disable?: Changes code logic, in rare cases we want to compare exact types.
            "E721",
            # Info: Fix bare except.
            # Why disable?: Disruptive, leave our exceptions alone.
            "E722",
            # Info: Fix module level import not at top of file.
            # Why disable?: re-ordering imports is disruptive and breaks some scripts
            # that need to check if a module has already been loaded in the case of reloading.
            "E402",
            # Info: Try to make lines fit within --max-line-length characters.
            # Why disable? Causes lines to be wrapped, where long lines have the
            # trailing bracket moved to the end of the line.
            # If trailing commas were respected as they are by clang-format this might be acceptable.
            # Note that this doesn't disable all line wrapping.
            "E501",
            # Info: Fix various deprecated code (via lib2to3)
            # Why disable?: causes imports to be added/re-arranged.
            "W690",
        )),
        "--aggressive",
        "--in-place",
        "--max-line-length", "120",
        f,
    ))


def main() -> None:
    import sys

    if os.path.samefile(sys.argv[-1], __file__):
        paths = path_expand(PATHS, is_source_and_included)
    else:
        paths = path_expand(sys.argv[1:], is_source_and_included)

    if USE_MULTIPROCESS:
        import multiprocessing
        job_total = multiprocessing.cpu_count()
        pool = multiprocessing.Pool(processes=job_total * 2)
        pool.map(autopep8_format_file, paths)
    else:
        for f in paths:
            autopep8_format_file(f)


if __name__ == "__main__":
    main()
