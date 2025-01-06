#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2024 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

__all__ = (
    "main",
)

import argparse
import os
import shutil
import sys
import textwrap
import webbrowser

from coverage_report import parse, report_as_html
from coverage_report.util import print_updateable_line
from pathlib import Path

usage = textwrap.dedent(
    """\
    coverage.py <command> [<args>]

    Commands:
        report                              Analyse coverage data and generate html report.
          [--build-directory]                 Blender build directory. This will be scanned for .gcda files.
          [--no-browser]                      Don't open the browser in the end.
        reset [--build-directory]           Delete .gcda files.
        help                                Show this help.
    """
)


def main():
    parser = argparse.ArgumentParser(description="Blender test coverage", usage=usage)

    parser.add_argument("command", nargs="?", default="help")
    args = parser.parse_args(sys.argv[1:2])
    command = args.command

    argv = sys.argv[2:]

    if command == "report":
        run_report(argv)
    elif command == "reset":
        run_reset(argv)
    elif command == "help":
        print(usage)
    else:
        print("Unknown command: {}".format(command))
        sys.exit(1)


def run_report(argv):
    parser = argparse.ArgumentParser(usage=usage)
    parser.add_argument("--build-directory", type=str, default=".")
    parser.add_argument("--no-browser", action="store_true", default=False)
    args = parser.parse_args(argv)

    build_dir = Path(args.build_directory).absolute()
    if not is_blender_build_directory(build_dir):
        print("Directory does not seem to be a Blender build directory.")
        sys.exit(1)

    coverage_dir = build_dir / "coverage"
    analysis_dir = coverage_dir / "analysis"
    reference_dir = coverage_dir / "reference"
    report_dir = coverage_dir / "report"

    parse(build_dir, analysis_dir)
    report_as_html(analysis_dir, report_dir, reference_dir=reference_dir)

    if not args.no_browser:
        webbrowser.open("file://" + str(report_dir / "index.html"))


def run_reset(argv):
    parser = argparse.ArgumentParser(usage=usage)
    parser.add_argument("--build-directory", type=str, default=".")
    args = parser.parse_args(argv)

    build_dir = Path(args.build_directory).absolute()
    if not is_blender_build_directory(build_dir):
        print("Directory does not seem to be a Blender build directory.")
        sys.exit(1)

    print("Remove .gcda files...")
    gcda_files = list(build_dir.glob("**/*.gcda"))
    for i, path in enumerate(gcda_files):
        print_updateable_line("[{}/{}] Remove: {}".format(i + 1, len(gcda_files), path))
        os.remove(path)
    print()


def is_blender_build_directory(build_dir):
    return (Path(build_dir) / "CMakeCache.txt").exists()


if __name__ == '__main__':
    main()
