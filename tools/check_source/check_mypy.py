#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import os
from os.path import join, splitext

from check_mypy_config import PATHS, PATHS_EXCLUDE

from typing import (
    Any,
    Callable,
    Generator,
    Optional,
    Tuple,
    Dict,
)

FileAndArgs = Tuple[str, Tuple[Any, ...], Dict[str, str]]

# print(PATHS)
SOURCE_EXT = (
    # Python
    ".py",
)


def is_source(filename: str) -> bool:
    return filename.endswith(SOURCE_EXT)


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


def path_expand_with_args(
        paths_and_args: Tuple[FileAndArgs, ...],
        filename_check: Optional[Callable[[str], bool]] = None,
) -> Generator[FileAndArgs, None, None]:
    for f_and_args in paths_and_args:
        f, f_args = f_and_args[0], f_and_args[1:]
        if not os.path.exists(f):
            print("Missing:", f)
        elif os.path.isdir(f):
            for f_iter in path_iter(f, filename_check):
                yield (f_iter, *f_args)
        else:
            yield (f, *f_args)


def main() -> None:
    import sys
    import subprocess
    import shlex

    # Fixed location, so change the current working directory doesn't create cache everywhere.
    cache_dir = os.path.join(os.getcwd(), ".mypy_cache")

    if os.path.samefile(sys.argv[-1], __file__):
        paths = path_expand_with_args(PATHS, is_source)
    else:
        paths = path_expand_with_args(
            tuple((p, (), {}) for p in sys.argv[1:]),
            is_source,
        )

    for f, extra_args, extra_env in paths:
        if f in PATHS_EXCLUDE:
            continue

        if not extra_args:
            extra_args = ()
        if not extra_env:
            extra_env = {}

        print(f)
        cmd = (
            "mypy",
            "--strict",
            "--cache-dir=" + cache_dir,
            "--color-output",
            f,
            *extra_args,
        )
        # p = subprocess.Popen(cmd, env=extra_env, stdout=sys.stdout, stderr=sys.stderr)

        if extra_env:
            for k, v in extra_env.items():
                os.environ[k] = v

        os.chdir(os.path.dirname(f))

        os.system(" ".join([shlex.quote(arg) for arg in cmd]))

        if extra_env:
            for k in extra_env.keys():
                del os.environ[k]


if __name__ == "__main__":
    main()
