# SPDX-FileCopyrightText: 2011-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

r"""
Example usage:

   blender --factory-startup --python ./tests/utils/batch_load_blendfiles.py

Arguments may be passed in:

   blender --factory-startup --python ./tests/utils/batch_load_blendfiles.py -- --sort-by=SIZE --range=0:10 --wait=0.1
"""
import os
import sys
import argparse

from typing import (
    Generator,
    List,
    Optional,
    Tuple,
)

SOURCE_DIR = os.path.abspath(os.path.normpath(os.path.join(os.path.dirname(__file__), "..", "..")))
LIB_DIR = os.path.abspath(os.path.normpath(os.path.join(SOURCE_DIR, "..", "lib")))

SORT_BY_FN = {
    "PATH": lambda path: path,
    "SIZE": lambda path: os.path.getsize(path),
}


def blend_list(path: str) -> Generator[str, None, None]:
    for dirpath, dirnames, filenames in os.walk(path):
        # skip '.git'
        dirnames[:] = [d for d in dirnames if not d.startswith(".")]
        for filename in filenames:
            if filename.lower().endswith(".blend"):
                filepath = os.path.join(dirpath, filename)
                yield filepath


def print_load_message(filepath: str, index: int) -> None:
    msg = "({:d}): {:s}".format(index, filepath)
    print("=" * len(msg))
    print(msg)
    print("=" * len(msg))


def load_blend_file(filepath: str) -> None:
    import bpy  # type: ignore
    bpy.ops.wm.open_mainfile(filepath=filepath)


def load_files_immediately(blend_files: List[str], blend_file_index_offset: int) -> None:
    index = blend_file_index_offset
    for filepath in blend_files:
        print_load_message(filepath, index)
        index += 1
        load_blend_file(filepath)


def load_files_with_wait(blend_files: List[str], blend_file_index_offset: int, wait: float) -> None:
    index = 0

    def load_on_timer() -> Optional[float]:
        nonlocal index
        if index >= len(blend_files):
            sys.exit(0)

        filepath = blend_files[index]
        print_load_message(filepath, index + blend_file_index_offset)
        index += 1

        load_blend_file(filepath)
        return wait

    import bpy
    bpy.app.timers.register(load_on_timer, persistent=True)


def argparse_handle_int_range(value: str) -> Tuple[int, int]:
    range_beg, sep, range_end = value.partition(":")
    if not sep:
        raise argparse.ArgumentTypeError("Expected a \":\" separator!")
    try:
        result = int(range_beg), int(range_end)
    except Exception as ex:
        raise argparse.ArgumentTypeError("Expected two integers: {!s}".format(ex))
    return result


def argparse_create() -> argparse.ArgumentParser:
    import argparse
    sort_by_choices = tuple(sorted(SORT_BY_FN.keys()))

    # When `--help` or no arguments are given, print this help.
    epilog = "Use to automate loading many blend files in a single Blender instance."

    parser = argparse.ArgumentParser(
        formatter_class=argparse.RawTextHelpFormatter,
        description=__doc__,
        epilog=epilog,
    )

    parser.add_argument(
        "--blend-dir",
        dest="blend_dir",
        metavar='BLEND_DIR',
        default=LIB_DIR,
        required=False,
        help="Path to recursively search blend files.",
    )

    parser.add_argument(
        '--sort-by',
        dest='files_sort_by',
        choices=sort_by_choices,
        default="PATH",
        required=False,
        metavar='SORT_METHOD',
        help='Order to load files {:s}.'.format(repr(sort_by_choices)),
    )

    parser.add_argument(
        '--range',
        dest='files_range',
        type=argparse_handle_int_range,
        required=False,
        default=(0, sys.maxsize),
        metavar='RANGE',
        help=(
            "The beginning and end range separated by a \":\", e.g."
            "useful for loading a range of files known to cause problems."
        ),
    )

    parser.add_argument(
        "--wait",
        dest="wait",
        type=float,
        default=-1.0,
        required=False,
        help=(
            "Time to wait between loading files, "
            "implies redrawing and even allows user interaction (-1.0 to disable)."
        ),
    )

    return parser


def main() -> Optional[int]:
    try:
        argv_sep = sys.argv.index("--")
    except ValueError:
        argv_sep = -1

    argv = [] if argv_sep == -1 else sys.argv[argv_sep + 1:]
    args = argparse_create().parse_args(argv)
    del argv

    if not os.path.exists(args.blend_dir):
        sys.stderr.write("Path {!r} not found!\n".format(args.blend_dir))
        return 1
    blend_files = list(blend_list(args.blend_dir))
    if not blend_files:
        sys.stderr.write("No blend files in {!r}!\n".format(args.blend_dir))
        return 1

    blend_files.sort(key=SORT_BY_FN[args.files_sort_by])

    range_beg, range_end = args.files_range

    blend_files_total = len(blend_files)

    blend_files = blend_files[range_beg:range_end]

    print("Found {:,d} files within {!r}".format(blend_files_total, args.blend_dir))
    if len(blend_files) != blend_files_total:
        print("Using a sub-range of {:,d}".format(len(blend_files)))

    if args.wait == -1.0:
        load_files_immediately(blend_files, range_beg)
    else:
        load_files_with_wait(blend_files, range_beg, args.wait)
        return None

    return 0


if __name__ == "__main__":
    result = main()
    if result is not None:
        sys.exit(result)
