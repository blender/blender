#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""
This script runs clang-format on multiple files/directories.

While it can be called directly, you may prefer to run this from Blender's root directory with the command:

   make format

"""

import argparse
import multiprocessing
import os
import sys
import subprocess

from typing import (
    List,
    Optional,
    Sequence,
    Tuple,
)

VERSION_MIN = (8, 0, 0)
VERSION_MAX_RECOMMENDED = (12, 0, 0)
CLANG_FORMAT_CMD = "clang-format"

BASE_DIR = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
os.chdir(BASE_DIR)


extensions = (
    ".c", ".cc", ".cpp", ".cxx",
    ".h", ".hh", ".hpp", ".hxx",
    ".m", ".mm",
    ".osl", ".glsl",
)

extensions_only_retab = (
    ".cmake",
    "CMakeLists.txt",
    ".sh",
)

ignore_files = {
    "intern/cycles/render/sobol.cpp",  # Too heavy for clang-format
}


def compute_paths(paths: List[str], use_default_paths: bool) -> List[str]:
    # Optionally pass in files to operate on.
    if use_default_paths:
        paths = [
            "intern/atomic",
            "intern/audaspace",
            "intern/clog",
            "intern/cycles",
            "intern/dualcon",
            "intern/eigen",
            "intern/ffmpeg",
            "intern/ghost",
            "intern/glew-mx",
            "intern/guardedalloc",
            "intern/iksolver",
            "intern/libmv",
            "intern/locale",
            "intern/memutil",
            "intern/mikktspace",
            "intern/opencolorio",
            "intern/opensubdiv",
            "intern/openvdb",
            "intern/rigidbody",
            "intern/utfconv",
            "source",
            "tests/gtests",
        ]
    else:
        # Filter out files, this is only done so this utility wont print that it's
        # "Operating" on files that will be filtered out later on.
        paths = [
            f for f in paths
            if os.path.isdir(f) or (os.path.isfile(f) and f.endswith(extensions))
        ]

    if os.sep != "/":
        paths = [f.replace("/", os.sep) for f in paths]
    return paths


def source_files_from_git(paths: Sequence[str], changed_only: bool) -> List[str]:
    if changed_only:
        cmd = ("git", "diff", "HEAD", "--name-only", "-z", "--", *paths)
    else:
        cmd = ("git", "ls-tree", "-r", "HEAD", *paths, "--name-only", "-z")
    files = subprocess.check_output(cmd).split(b'\0')
    return [f.decode('ascii') for f in files]


def convert_tabs_to_spaces(files: Sequence[str]) -> None:
    for f in files:
        print("TabExpand", f)
        with open(f, 'r', encoding="utf-8") as fh:
            data = fh.read()
            if False:
                # Simple 4 space
                data = data.expandtabs(4)
            else:
                # Complex 2 space
                # because some comments have tabs for alignment.
                def handle(l: str) -> str:
                    ls = l.lstrip("\t")
                    d = len(l) - len(ls)
                    if d != 0:
                        return ("  " * d) + ls.expandtabs(4)
                    else:
                        return l.expandtabs(4)

                lines = data.splitlines(keepends=True)
                lines = [handle(l) for l in lines]
                data = "".join(lines)
        with open(f, 'w', encoding="utf-8") as fh:
            fh.write(data)


def clang_format_ensure_version() -> Optional[Tuple[int, int, int]]:
    global CLANG_FORMAT_CMD
    clang_format_cmd = None
    version_output = ""
    for i in range(2, -1, -1):
        clang_format_cmd = (
            "clang-format-" + (".".join(["%d"] * i) % VERSION_MIN[:i])
            if i > 0 else
            "clang-format"
        )
        try:
            version_output = subprocess.check_output((clang_format_cmd, "-version")).decode('utf-8')
        except FileNotFoundError:
            continue
        CLANG_FORMAT_CMD = clang_format_cmd
        break
    version: Optional[str] = next(iter(v for v in version_output.split() if v[0].isdigit()), None)
    if version is None:
        return None

    version = version.split("-")[0]
    # Ensure exactly 3 numbers.
    version_num: Tuple[int, int, int] = (tuple(int(n) for n in version.split(".")) + (0, 0, 0))[:3]  # type: ignore
    print("Using %s (%d.%d.%d)..." % (CLANG_FORMAT_CMD, version_num[0], version_num[1], version_num[2]))
    return version_num


def clang_format_file(files: List[str]) -> bytes:
    cmd = [
        CLANG_FORMAT_CMD,
        # Update the files in-place.
        "-i",
        # Shows the list of processed files.
        "-verbose",
    ] + files
    return subprocess.check_output(cmd, stderr=subprocess.STDOUT)


def clang_print_output(output: bytes) -> None:
    print(output.decode('utf8', errors='ignore').strip())


def clang_format(files: List[str]) -> None:
    pool = multiprocessing.Pool()

    # Process in chunks to reduce overhead of starting processes.
    cpu_count = multiprocessing.cpu_count()
    chunk_size = min(max(len(files) // cpu_count // 2, 1), 32)
    for i in range(0, len(files), chunk_size):
        files_chunk = files[i:i + chunk_size]
        pool.apply_async(clang_format_file, args=[files_chunk], callback=clang_print_output)

    pool.close()
    pool.join()


def argparse_create() -> argparse.ArgumentParser:

    parser = argparse.ArgumentParser(
        description="Format C/C++/GLSL & Objective-C source code.",
        epilog=__doc__,
        # Don't re-wrap text, keep newlines & indentation.
        formatter_class=argparse.RawTextHelpFormatter,

    )
    parser.add_argument(
        "--expand-tabs",
        dest="expand_tabs",
        default=False,
        action='store_true',
        help="Run a pre-pass that expands tabs "
        "(default=False)",
        required=False,
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
        "paths",
        nargs=argparse.REMAINDER,
        help="All trailing arguments are treated as paths.",
    )

    return parser


def main() -> None:
    version = clang_format_ensure_version()
    if version is None:
        print("Unable to detect 'clang-format -version'")
        sys.exit(1)
    if version < VERSION_MIN:
        print("Version of clang-format is too old:", version, "<", VERSION_MIN)
        sys.exit(1)
    if version > VERSION_MAX_RECOMMENDED:
        print(
            "WARNING: Version of clang-format is too recent:",
            version, ">", VERSION_MAX_RECOMMENDED,
        )
        print(
            "You may want to install clang-format-%d.%d, "
            "or use the precompiled libs repository." %
            (VERSION_MAX_RECOMMENDED[0], VERSION_MAX_RECOMMENDED[1]),
        )

    args = argparse_create().parse_args()

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

    # Always operate on all CMAKE files (when expanding tabs and no paths given).
    files_retab = [
        f for f in source_files_from_git((".",) if use_default_paths else paths, args.changed_only)
        if f.endswith(extensions_only_retab)
        if f not in ignore_files
    ]

    if args.expand_tabs:
        convert_tabs_to_spaces(files + files_retab)
    clang_format(files)


if __name__ == "__main__":
    main()
