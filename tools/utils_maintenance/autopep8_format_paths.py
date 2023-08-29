#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
This script runs autopep8 on multiple files/directories.

While it can be called directly, you may prefer to run this from Blender's root directory with the command:

   make format

Otherwise you may call this script directly, for example:

   ./tools/utils_maintenance/autopep8_format_paths.py --changed-only tests/python
"""

import os
import sys

import subprocess
import argparse

from typing import (
    List,
    Tuple,
    Optional,
)

VERSION_MIN = (1, 6, 0)
VERSION_MAX_RECOMMENDED = (1, 6, 0)
AUTOPEP8_FORMAT_CMD = "autopep8"

BASE_DIR = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", ".."))
os.chdir(BASE_DIR)


extensions = (
    ".py",
)

ignore_files = {
    "scripts/modules/rna_manual_reference.py",  # Large generated file, don't format.
    "tools/svn_rev_map/rev_to_sha1.py",
    "tools/svn_rev_map/sha1_to_rev.py",
}


def compute_paths(paths: List[str], use_default_paths: bool) -> List[str]:
    # Optionally pass in files to operate on.
    if use_default_paths:
        paths = [
            "build_files",
            "intern",
            "release",
            "scripts",
            "doc",
            "source",
            "tests",
            "tools",
        ]
    else:
        paths = [
            f for f in paths
            if os.path.isdir(f) or (os.path.isfile(f) and f.endswith(extensions))
        ]

    if os.sep != "/":
        paths = [f.replace("/", os.sep) for f in paths]
    return paths


def source_files_from_git(paths: List[str], changed_only: bool) -> List[str]:
    if changed_only:
        cmd = ("git", "diff", "HEAD", "--name-only", "-z", "--", *paths)
    else:
        cmd = ("git", "ls-tree", "-r", "HEAD", *paths, "--name-only", "-z")
    files = subprocess.check_output(cmd).split(b'\0')
    return [f.decode('ascii') for f in files]


def autopep8_ensure_version(autopep8_format_cmd_argument: str) -> Optional[Tuple[int, int, int]]:
    global AUTOPEP8_FORMAT_CMD
    autopep8_format_cmd = None
    version_output = None
    # Attempt to use `--autopep8-command` passed in from `make format`
    # so the autopep8 distributed with Blender will be used.
    for is_default in (True, False):
        if is_default:
            autopep8_format_cmd = autopep8_format_cmd_argument
            if autopep8_format_cmd and os.path.exists(autopep8_format_cmd):
                pass
            else:
                continue
        else:
            autopep8_format_cmd = "autopep8"

        cmd = [autopep8_format_cmd]
        if cmd[0].endswith(".py"):
            cmd = [sys.executable, *cmd]

        try:
            version_output = subprocess.check_output((*cmd, "--version")).decode('utf-8')
        except FileNotFoundError:
            continue
        AUTOPEP8_FORMAT_CMD = autopep8_format_cmd
        break
    if version_output is not None:
        version_str = next(iter(v for v in version_output.split() if v[0].isdigit()), None)
    if version_str is not None:
        # Ensure exactly 3 numbers.
        major, minor, patch = (tuple(int(n) for n in version_str.split("-")[0].split(".")) + (0, 0, 0))[0:3]
        print("Using %s (%d.%d.%d)..." % (AUTOPEP8_FORMAT_CMD, major, minor, patch))
        return major, minor, patch
    return None


def autopep8_format(files: List[str]) -> bytes:
    cmd = [
        AUTOPEP8_FORMAT_CMD,
        # Operate on all directories recursively.
        "--recursive",
        # Update the files in-place.
        "--in-place",
        # Auto-detect the number of jobs to use.
        "--jobs=0",
    ] + files

    # Support executing from the module directory because Blender does not distribute the command.
    if cmd[0].endswith(".py"):
        cmd = [sys.executable, *cmd]

    return subprocess.check_output(cmd, stderr=subprocess.STDOUT)


def argparse_create() -> argparse.ArgumentParser:

    parser = argparse.ArgumentParser(
        description="Format Python source code.",
        epilog=__doc__,
        # Don't re-wrap text, keep newlines & indentation.
        formatter_class=argparse.RawTextHelpFormatter,
    )
    parser.add_argument(
        "--changed-only",
        dest="changed_only",
        default=False,
        action='store_true',
        help=(
            "Format only edited files, including the staged ones. "
            "Using this with \"paths\" will pick the edited files lying on those paths. "
            "(default=False)"
        ),
        required=False,
    )
    parser.add_argument(
        "--autopep8-command",
        dest="autopep8_command",
        default=AUTOPEP8_FORMAT_CMD,
        help="The command to call autopep8.",
        required=False,
    )
    parser.add_argument(
        "paths",
        nargs=argparse.REMAINDER,
        help="All trailing arguments are treated as paths.",
    )

    return parser


def main() -> None:
    args = argparse_create().parse_args()

    version = autopep8_ensure_version(args.autopep8_command)
    if version is None:
        print("Unable to detect 'autopep8 --version'")
        sys.exit(1)
    if version < VERSION_MIN:
        print("Version of autopep8 is too old:", version, "<", VERSION_MIN)
        sys.exit(1)
    if version > VERSION_MAX_RECOMMENDED:
        print(
            "WARNING: Version of autopep8 is too recent:",
            version, ">", VERSION_MAX_RECOMMENDED,
        )
        print(
            "You may want to install autopep8-%d.%d, "
            "or use the precompiled libs repository." %
            (VERSION_MAX_RECOMMENDED[0], VERSION_MAX_RECOMMENDED[1]),
        )

    use_default_paths = not (bool(args.paths) or bool(args.changed_only))

    paths = compute_paths(args.paths, use_default_paths)
    print("Operating on:" + (" (%d changed paths)" % len(paths) if args.changed_only else ""))
    for p in paths:
        print(" ", p)

    files = [
        f for f in source_files_from_git(paths, args.changed_only)
        if f.endswith(extensions)
        if f not in ignore_files
    ]

    # Happens when users run "make format" passing in individual C/C++ files
    # (and no Python files).
    if not files:
        return

    autopep8_format(files)


if __name__ == "__main__":
    main()
