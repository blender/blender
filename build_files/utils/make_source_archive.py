#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2021-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

__all__ = (
    "main",
)

import argparse
import make_utils
import os
import subprocess
import sys
from pathlib import Path

from typing import (
    TextIO,
    Any,
    Tuple,
    Union,
    # Proxies for `collections.abc`
    Iterable,
    List,
)

# This script can run from any location,
# output is created in the $CWD
#
# NOTE: while the Python part of this script is portable,
# it relies on external commands typically found on GNU/Linux.
# Support for other platforms could be added by moving GNU `tar` & `md5sum` use to Python.
# This also relies on having a Unix shell (sh) to run some git commands.

SKIP_NAMES: Tuple[str, ...] = (
    ".gitignore",
    ".gitmodules",
    ".gitattributes",
    ".git-blame-ignore-revs",
    ".arcconfig",
    ".svn",
)

# Tuple with folder names relative to the main repository root that are to be excluded.
SKIP_FOLDERS: Tuple[str, ...] = (
)

# Generated list of Paths to be ignored based on the SKIP_FOLDERS and some rum-time rules like
# the type of package that is being created.
SKIP_PATHS: List[Path] = []


def main() -> None:
    blender_srcdir = Path(__file__).absolute().parent.parent.parent

    cli_parser = argparse.ArgumentParser(
        description="Create a tarball of the Blender sources, optionally including sources of dependencies.",
        epilog="This script is intended to be run by `make source_archive_complete`.",
    )
    group = cli_parser.add_mutually_exclusive_group()
    group.add_argument(
        "-p",
        "--include-packages",
        type=Path,
        default=None,
        metavar="PACKAGE_PATH",
        help="Include all source files from the given package directory as well.",
    )
    group.add_argument(
        "-t",
        "--package-test-data",
        action='store_true',
        help="Package all test data into its own archive",
    )

    cli_args = cli_parser.parse_args()

    print(f"Source dir: {blender_srcdir}")

    curdir = blender_srcdir.parent
    os.chdir(curdir)
    blender_srcdir = blender_srcdir.relative_to(curdir)

    # Update our SKIP_FOLDERS blacklist with the source directory name
    global SKIP_PATHS
    SKIP_PATHS = [blender_srcdir / entry for entry in SKIP_FOLDERS]

    print(f"Output dir: {curdir}")

    version = make_utils.parse_blender_version()
    tarball = tarball_path(curdir, version, cli_args)
    manifest = manifest_path(tarball)
    packages_dir = packages_path(curdir, cli_args)

    if cli_args.package_test_data:
        print("Creating an archive of all test data.")
        create_manifest(version, manifest, blender_srcdir / "tests/files", packages_dir)
    else:
        SKIP_PATHS.append(blender_srcdir / "tests/files")
        create_manifest(version, manifest, blender_srcdir, packages_dir)

    create_tarball(version, tarball, manifest, blender_srcdir, packages_dir)
    create_checksum_file(tarball)
    cleanup(manifest)
    print("Done!")


def tarball_path(output_dir: Path, version: make_utils.BlenderVersion, cli_args: Any) -> Path:
    extra = ""
    if cli_args.include_packages:
        extra = "-with-libraries"
    elif cli_args.package_test_data:
        extra = "-test-data"

    return output_dir / f"blender{extra}-{version}.tar.xz"


def manifest_path(tarball: Path) -> Path:
    """Return the manifest path for the given tarball path.

    >>> from pathlib import Path
    >>> tarball = Path("/home/user/workspace/blender-git/blender-test.tar.gz")
    >>> manifest_path(tarball).as_posix()
    '/home/user/workspace/blender-git/blender-test-manifest.txt'
    """
    # Note that `.tar.gz` is seen as two suffixes.
    without_suffix = tarball.with_suffix("").with_suffix("")
    name = without_suffix.name
    return without_suffix.with_name(f"{name}-manifest.txt")


def packages_path(current_directory: Path, cli_args: Any) -> Union[Path, None]:
    if not cli_args.include_packages:
        return None

    abspath = cli_args.include_packages.absolute()

    # `os.path.relpath()` can return paths like "../../packages", where
    # `Path.relative_to()` will not go up directories (so its return value never
    # has "../" in there).
    relpath = os.path.relpath(abspath, current_directory)

    return Path(relpath)

# -----------------------------------------------------------------------------
# Manifest creation


def create_manifest(
    version: make_utils.BlenderVersion,
    outpath: Path,
    blender_srcdir: Path,
    packages_dir: Union[Path, None],
    exclude: List[Path] = []
) -> None:
    print(f'Building manifest of files:  "{outpath}"...', end="", flush=True)
    with outpath.open("w", encoding="utf-8") as outfile:
        main_files_to_manifest(blender_srcdir, outfile)

        if packages_dir:
            packages_to_manifest(outfile, packages_dir)
    print("OK")


def main_files_to_manifest(blender_srcdir: Path, outfile: TextIO) -> None:
    assert not blender_srcdir.is_absolute()
    for git_repo in git_gather_all_folders_to_package(blender_srcdir):
        for path in git_ls_files(git_repo):
            print(path, file=outfile)


def packages_to_manifest(outfile: TextIO, packages_dir: Path) -> None:
    for path in packages_dir.glob("*"):
        if not path.is_file():
            continue
        if path.name in SKIP_NAMES:
            continue
        print(path, file=outfile)


# -----------------------------------------------------------------------------
# Higher-level functions


def create_tarball(
    version: make_utils.BlenderVersion,
    tarball: Path,
    manifest: Path,
    blender_srcdir: Path,
    packages_dir: Union[Path, None],
) -> None:
    print(f'Creating archive:            "{tarball}" ...', end="", flush=True)

    # Requires GNU `tar`, since `--transform` is used.
    if sys.platform == "darwin":
        # Provided by `brew install gnu-tar`.
        command = ["gtar"]
    else:
        command = ["tar"]

    if packages_dir:
        command += ["--transform", f"s,{packages_dir}/,packages/,g"]

    command += [
        "--transform",
        f"s,^{blender_srcdir.name}/,blender-{version}/,g",
        "--use-compress-program=xz -1",
        "--create",
        f"--file={tarball}",
        f"--files-from={manifest}",
        # Without owner/group args, extracting the files as root will
        # use ownership from the tar archive:
        "--owner=0",
        "--group=0",
    ]

    subprocess.run(command, check=True, timeout=3600)
    print("OK")


def create_checksum_file(tarball: Path) -> None:
    md5_path = tarball.with_name(tarball.name + ".md5sum")
    print(f'Creating checksum:           "{md5_path}" ...', end="", flush=True)
    command = [
        "md5sum",
        # The name is enough, as the tarball resides in the same dir as the MD5
        # file, and that's the current working directory.
        tarball.name,
    ]
    md5_cmd = subprocess.run(
        command, stdout=subprocess.PIPE, check=True, text=True, timeout=300
    )
    with md5_path.open("w", encoding="utf-8") as outfile:
        outfile.write(md5_cmd.stdout)
    print("OK")


def cleanup(manifest: Path) -> None:
    print("Cleaning up ...", end="", flush=True)
    if manifest.exists():
        manifest.unlink()
    print("OK")


# -----------------------------------------------------------------------------
# Low-level commands


def git_gather_all_folders_to_package(directory: Path = Path(".")) -> Iterable[Path]:
    """Generator, yields lines which represents each directory to gather git files from.

    Each directory represents either the top level git repository or a submodule.
    All submodules that have the 'update = none' setting will be excluded from this list.

    The directory path given to this function will be included in the yielded paths
    """

    # For each submodule (recurse into submodules within submodules if they exist)
    git_main_command = "submodule --quiet foreach --recursive"
    # Return the path to the submodule and what the value is of their "update" setting
    # If the "update" setting doesn't exist, only the path to the submodule is returned
    git_command_args = "'echo $displaypath $(git config --file \"$toplevel/.gitmodules\" --get submodule.$name.update)'"

    # Yield the root directory as this is our top level git repo
    yield directory

    for line in git_command(f"-C '{directory}' {git_main_command} {git_command_args}"):
        # Check if we shouldn't include the directory on this line
        split_line = line.rsplit(maxsplit=1)
        if len(split_line) > 1 and split_line[-1] == "none":
            continue
        path = directory / split_line[0]
        yield path


def is_path_ignored(file: Path) -> bool:
    for skip_folder in SKIP_PATHS:
        if file.is_relative_to(skip_folder):
            return True
    return False


def git_ls_files(directory: Path = Path(".")) -> Iterable[Path]:
    """Generator, yields lines of output from 'git ls-files'.

    Only lines that are actually files (so no directories, sockets, etc.) are
    returned, and never one from SKIP_NAMES.
    """
    for line in git_command(f"-C '{directory}' ls-files -z", "\x00"):
        path = directory / line
        if not path.is_file() or path.name in SKIP_NAMES:
            continue
        if not is_path_ignored(path):
            yield path


def git_command(cli_args: str, split_char: str = "\n") -> Iterable[str]:
    """Generator, yields lines of output from a Git command."""
    command = "git " + cli_args

    # import shlex
    # print(">", " ".join(shlex.quote(arg) for arg in command))

    git = subprocess.run(
        command, stdout=subprocess.PIPE, shell=True, check=True, text=True, timeout=30
    )
    for line in git.stdout.split(split_char):
        if line:
            yield line


if __name__ == "__main__":
    import doctest

    if doctest.testmod().failed:
        raise SystemExit("ERROR: Self-test failed, refusing to run")

    main()
