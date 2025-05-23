#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
Script which strips all libraries in the given library directory.
This is so we don't keep any debug data or symbols that contains
random hashes that are not reproducible between builds.

This will strip both static and shared libraries.

Usage:
  strip_libraries.py <path/to/library/directory>
"""
import argparse
import subprocess
import sys
from pathlib import Path


def print_strip_lib(strip_lib: Path, prev_print_len: int) -> int:
    print_str = f"Stripping: {strip_lib}"
    if prev_print_len > 0:
        print(f"\r{' ' * prev_print_len}\r", end="")
    print(print_str, end="", flush=True)
    return len(print_str)


def strip_libs(strip_dir: Path) -> None:
    print(f"Stripping libraries in: {strip_dir}")
    prev_print_len = 0
    for shared_lib in strip_dir.rglob("*.so*"):
        if shared_lib.suffix == ".py":
            # Work around badly named `sycl` scripts.
            continue

        if shared_lib.is_symlink():
            # Don't strip symbolic-links as we don't want to strip the same library multiple times.
            continue

        prev_print_len = print_strip_lib(shared_lib, prev_print_len)
        subprocess.check_call(["strip", "-s", "--enable-deterministic-archives", shared_lib])
    for static_lib in strip_dir.rglob("*.a"):
        if static_lib.is_symlink():
            # Don't strip symbolic-links as we don't want to strip the same library multiple times.
            continue

        prev_print_len = print_strip_lib(static_lib, prev_print_len)
        subprocess.check_call(["objcopy", "--enable-deterministic-archives", static_lib])

    print("\nDone stripping libraries!")


def main() -> None:
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawTextHelpFormatter,
    )
    parser.add_argument("directory", type=Path, help="Path to the library directory to strip")
    args = parser.parse_args()

    if sys.platform == "linux":
        strip_libs(args.directory)


if __name__ == "__main__":
    main()
