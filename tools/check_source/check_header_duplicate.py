#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
Run this script to check if headers are included multiple times.

    python3 check_header_duplicate.py

Now build the code to find duplicate errors, resolve them manually.

Then restore the headers to their original state:

    python3 check_header_duplicate.py --restore
"""

import os
import sys
import argparse

from typing import (
    Callable,
    Generator,
    Optional,
)

# Use GCC's `__INCLUDE_LEVEL__` to find direct duplicate includes.

BASEDIR = os.path.normpath(os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..")))

# TODO: make this an argument.
dirs_include = [
    os.path.join(BASEDIR, "intern"),
    os.path.join(BASEDIR, "source"),
]

files_exclude = {
    os.path.join(BASEDIR, "extern", "curve_fit_nd", "intern", "generic_alloc_impl.h"),
    os.path.join(BASEDIR, "source", "blender", "blenlib", "intern", "list_sort_impl.h"),
    os.path.join(BASEDIR, "source", "blender", "makesdna", "intern", "dna_rename_defs.h"),
    os.path.join(BASEDIR, "source", "blender", "makesrna", "RNA_enum_items.hh"),
}


HEADER_FMT = """\
#if __INCLUDE_LEVEL__ == 1
#  ifdef _DOUBLEHEADERGUARD_{0:d}
#    error "duplicate header!"
#  endif
#endif
#if __INCLUDE_LEVEL__ == 1
#  define _DOUBLEHEADERGUARD_{0:d}
#endif /* END! */
"""

HEADER_END = "#endif /* END! */\n"

UUID = 0


def source_filepath_guard_add(filepath: str) -> None:
    global UUID

    header = HEADER_FMT.format(UUID)
    UUID += 1

    with open(filepath, 'r', encoding='utf-8') as f:
        data = f.read()

    with open(filepath, 'w', encoding='utf-8') as f:
        f.write(header)
        f.write(data)


def source_filepath_guard_restore(filepath: str) -> None:
    with open(filepath, 'r', encoding='utf-8') as f:
        data = f.read()

    index = data.index(HEADER_END)
    if index == -1:
        return

    with open(filepath, 'w', encoding='utf-8') as f:
        f.write(data[index + len(HEADER_END):])


def scan_source_recursive(dirpath: str, is_restore: bool) -> None:
    from os.path import splitext

    def source_list(
            path: str,
            filename_check: Optional[Callable[[str], bool]] = None,
    ) -> Generator[str, None, None]:
        for dirpath, dirnames, filenames in os.walk(path):
            # skip '.git'
            dirnames[:] = [d for d in dirnames if not d.startswith(".")]

            for filename in filenames:
                filepath = os.path.join(dirpath, filename)
                if filename_check is None or filename_check(filepath):
                    yield filepath

    def is_header_source(filename: str) -> bool:
        ext = splitext(filename)[1]
        return (ext in {".hpp", ".hxx", ".h", ".hh"})

    for filepath in sorted(source_list(dirpath, is_header_source)):
        if filepath in files_exclude:
            continue

        print("file:", filepath)

        if is_restore:
            source_filepath_guard_restore(filepath)
        else:
            source_filepath_guard_add(filepath)


def argparse_create() -> argparse.ArgumentParser:

    parser = argparse.ArgumentParser(
        description="Detect duplicate headers",
        epilog=__doc__,
        # Don't re-wrap text, keep newlines & indentation.
        formatter_class=argparse.RawTextHelpFormatter,

    )
    parser.add_argument(
        "--restore",
        dest="restore",
        default=False,
        action='store_true',
        help=(
            "Restore the files to their original state"
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


def main() -> int:
    ok = True

    args = argparse_create().parse_args()
    if args.paths:
        paths = [os.path.normpath(os.path.abspath(p)) for p in args.paths]
    else:
        paths = dirs_include

    for p in paths:
        if not p.startswith(BASEDIR + os.sep):
            sys.stderr.write("Path \"{:s}\" outside \"{:s}\", aborting!\n".format(p, BASEDIR))
            ok = False
        if not os.path.exists(p):
            sys.stderr.write("Path \"{:s}\" does not exist, aborting!\n".format(p))
            ok = False

    for p in files_exclude:
        if not os.path.exists(p):
            sys.stderr.write("Excluded path \"{:s}\" does not exist, aborting!\n".format(p))
            ok = False

    if not ok:
        return 1

    for dirpath in paths:
        scan_source_recursive(dirpath, args.restore)
    return 0


if __name__ == "__main__":
    sys.exit(main())
