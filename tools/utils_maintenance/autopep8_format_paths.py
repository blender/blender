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

__all__ = (
    "main",
)

import os
import sys

import subprocess
import argparse

VERSION_MIN = (2, 3, 1)
VERSION_MAX_RECOMMENDED = (2, 3, 1)
AUTOPEP8_FORMAT_CMD = "autopep8"
AUTOPEP8_FORMAT_DEFAULT_ARGS = (
    # Operate on all directories recursively.
    "--recursive",
    # Update the files in-place.
    "--in-place",
    # Auto-detect the number of jobs to use.
    "--jobs=0",
)

BASE_DIR = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", ".."))
os.chdir(BASE_DIR)


extensions = (
    ".py",
)

ignore_files = {
    "scripts/modules/_rna_manual_reference.py",  # Large generated file, don't format.
    "tools/svn_rev_map/rev_to_sha1.py",
    "tools/svn_rev_map/sha1_to_rev.py",
}


def compute_paths(paths: list[str], use_default_paths: bool) -> list[str]:
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


def source_files_from_git(paths: list[str], changed_only: bool) -> list[str]:
    if changed_only:
        cmd = ("git", "diff", "HEAD", "--name-only", "-z", "--", *paths)
    else:
        cmd = ("git", "ls-tree", "-r", "HEAD", *paths, "--name-only", "-z")
    files = subprocess.check_output(cmd).split(b'\0')
    return [f.decode('utf-8') for f in files]


def autopep8_parse_version(version: str) -> tuple[int, int, int]:
    # Ensure exactly 3 numbers.
    major, minor, patch = (tuple(int(n) for n in version.split("-")[0].split(".")) + (0, 0, 0))[0:3]
    return major, minor, patch


def version_str_from_tuple(version: tuple[int, ...]) -> str:
    return ".".join(str(x) for x in version)


def autopep8_ensure_version_from_command(
        autopep8_format_cmd_argument: str,
) -> tuple[str, tuple[int, int, int]] | None:

    # The version to parse.
    version_str: str | None = None

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
        assert isinstance(autopep8_format_cmd, str)
        major, minor, patch = autopep8_parse_version(version_str)
        return autopep8_format_cmd, (major, minor, patch)

    return None


def autopep8_ensure_version_from_module() -> tuple[str, tuple[int, int, int]] | None:

    # The version to parse.
    version_str: str | None = None

    # Extract the version from the module.
    try:
        # pylint: disable-next=import-outside-toplevel
        import autopep8  # type: ignore
    except ModuleNotFoundError as ex:
        if ex.name != "autopep8":
            raise ex
        return None

    version_str = autopep8.__version__
    if version_str is not None:
        major, minor, patch = autopep8_parse_version(version_str)
        return autopep8.__file__, (major, minor, patch)

    return None


def autopep8_format(files: list[str]) -> bytes:
    cmd = [
        AUTOPEP8_FORMAT_CMD,
        *AUTOPEP8_FORMAT_DEFAULT_ARGS,
        *files
    ]

    # Support executing from the module directory because Blender does not distribute the command.
    if cmd[0].endswith(".py"):
        cmd = [sys.executable, *cmd]

    return subprocess.check_output(cmd, stderr=subprocess.STDOUT)


def autopep8_format_no_subprocess(files: list[str]) -> None:
    cmd = [
        *AUTOPEP8_FORMAT_DEFAULT_ARGS,
        *files
    ]
    # NOTE: this import will have already succeeded, see: `autopep8_ensure_version_from_module`.
    # pylint: disable-next=import-outside-toplevel
    from autopep8 import main as autopep8_main
    autopep8_main(argv=cmd)


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
        "--no-subprocess",
        dest="no_subprocess",
        default=False,
        action='store_true',
        help=(
            "Don't use a sub-process, load autopep8 into this instance of Python. "
            "Works around 8191 argument length limit on WIN32."
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


def main() -> int:
    args = argparse_create().parse_args()

    if args.no_subprocess:
        source_and_version = autopep8_ensure_version_from_module()
    else:
        source_and_version = autopep8_ensure_version_from_command(args.autopep8_command)

    if source_and_version is None:
        if args.no_subprocess:
            sys.stderr.write("ERROR: unable to import \"autopep8\" from Python at \"{:s}\".\n".format(sys.executable))
        else:
            sys.stderr.write("ERROR: unable to detect \"autopep8 --version\".\n")

        sys.stderr.write("You may want to install autopep8-{:s}, or use the pre-compiled libs repository.\n".format(
            version_str_from_tuple(VERSION_MAX_RECOMMENDED[0:2]),
        ))
        return 1

    autopep8_source, version = source_and_version

    if version < VERSION_MIN:
        sys.stderr.write("Using \"{:s}\"\nERROR: the autopep8 version is too old: {:s} < {:s}.\n".format(
            autopep8_source,
            version_str_from_tuple(version),
            version_str_from_tuple(VERSION_MIN),
        ))
        return 1
    if version > VERSION_MAX_RECOMMENDED:
        sys.stderr.write("Using \"{:s}\"\nWARNING: the autopep8 version is too recent: {:s} > {:s}.\n".format(
            autopep8_source,
            version_str_from_tuple(version),
            version_str_from_tuple(VERSION_MAX_RECOMMENDED),
        ))
        sys.stderr.write("You may want to install autopep8-{:s}, or use the pre-compiled libs repository.\n".format(
            version_str_from_tuple(VERSION_MAX_RECOMMENDED[0:2]),
        ))
    else:
        print("Using \"{:s}\", ({:s})...".format(autopep8_source, version_str_from_tuple(version)))

    use_default_paths = not (bool(args.paths) or bool(args.changed_only))
    paths = compute_paths(args.paths, use_default_paths)
    # Check if user-defined paths exclude all Python sources.
    if args.paths and not paths:
        print("Skip autopep8: no target to format")
        return 0

    print("Operating on:" + (" ({:d} changed paths)".format(len(paths)) if args.changed_only else ""))
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
        return 0

    if args.no_subprocess:
        autopep8_format_no_subprocess(files)
    else:
        autopep8_format(files)

    return 0


if __name__ == "__main__":
    sys.exit(main())
