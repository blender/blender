#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later

import argparse
import make_utils
import os
import re
import subprocess
from pathlib import Path
from typing import Iterable, TextIO, Optional, Any, Union

# This script can run from any location,
# output is created in the $CWD
#
# NOTE: while the Python part of this script is portable,
# it relies on external commands typically found on GNU/Linux.
# Support for other platforms could be added by moving GNU `tar` & `md5sum` use to Python.

SKIP_NAMES = {
    ".gitignore",
    ".gitmodules",
    ".gitattributes",
    ".git-blame-ignore-revs",
    ".arcconfig",
    ".svn",
}


def main() -> None:
    blender_srcdir = Path(__file__).absolute().parent.parent.parent

    cli_parser = argparse.ArgumentParser(
        description=f"Create a tarball of the Blender sources, optionally including sources of dependencies.",
        epilog="This script is intended to be run by `make source_archive_complete`.",
    )
    cli_parser.add_argument(
        "-p",
        "--include-packages",
        type=Path,
        default=None,
        metavar="PACKAGE_PATH",
        help="Include all source files from the given package directory as well.",
    )

    cli_args = cli_parser.parse_args()

    print(f"Source dir: {blender_srcdir}")

    curdir = blender_srcdir.parent
    os.chdir(curdir)
    blender_srcdir = blender_srcdir.relative_to(curdir)

    print(f"Output dir: {curdir}")

    version = make_utils.parse_blender_version()
    tarball = tarball_path(curdir, version, cli_args)
    manifest = manifest_path(tarball)
    packages_dir = packages_path(curdir, cli_args)

    create_manifest(version, manifest, blender_srcdir, packages_dir)
    create_tarball(version, tarball, manifest, blender_srcdir, packages_dir)
    create_checksum_file(tarball)
    cleanup(manifest)
    print("Done!")


def tarball_path(output_dir: Path, version: make_utils.BlenderVersion, cli_args: Any) -> Path:
    extra = ""
    if cli_args.include_packages:
        extra = "-with-libraries"

    return output_dir / f"blender{extra}-{version}.tar.xz"


def manifest_path(tarball: Path) -> Path:
    """Return the manifest path for the given tarball path.

    >>> from pathlib import Path
    >>> tarball = Path("/home/sybren/workspace/blender-git/blender-test.tar.gz")
    >>> manifest_path(tarball).as_posix()
    '/home/sybren/workspace/blender-git/blender-test-manifest.txt'
    """
    # ".tar.gz" is seen as two suffixes.
    without_suffix = tarball.with_suffix("").with_suffix("")
    name = without_suffix.name
    return without_suffix.with_name(f"{name}-manifest.txt")


def packages_path(current_directory: Path, cli_args: Any) -> Optional[Path]:
    if not cli_args.include_packages:
        return None

    abspath = cli_args.include_packages.absolute()

    # os.path.relpath() can return paths like "../../packages", where
    # Path.relative_to() will not go up directories (so its return value never
    # has "../" in there).
    relpath = os.path.relpath(abspath, current_directory)

    return Path(relpath)

# -----------------------------------------------------------------------------
# Manifest creation


def create_manifest(
    version: make_utils.BlenderVersion,
    outpath: Path,
    blender_srcdir: Path,
    packages_dir: Optional[Path],
) -> None:
    print(f'Building manifest of files:  "{outpath}"...', end="", flush=True)
    with outpath.open("w", encoding="utf-8") as outfile:
        main_files_to_manifest(blender_srcdir, outfile)
        assets_to_manifest(blender_srcdir, outfile)
        submodules_to_manifest(blender_srcdir, version, outfile)

        if packages_dir:
            packages_to_manifest(outfile, packages_dir)
    print("OK")


def main_files_to_manifest(blender_srcdir: Path, outfile: TextIO) -> None:
    assert not blender_srcdir.is_absolute()
    for path in git_ls_files(blender_srcdir):
        print(path, file=outfile)


def submodules_to_manifest(
    blender_srcdir: Path, version: make_utils.BlenderVersion, outfile: TextIO
) -> None:
    skip_addon_contrib = version.is_release()
    assert not blender_srcdir.is_absolute()

    for submodule in ("scripts/addons", "scripts/addons_contrib"):
        # Don't use native slashes as GIT for MS-Windows outputs forward slashes.
        if skip_addon_contrib and submodule == "scripts/addons_contrib":
            continue

        for path in git_ls_files(blender_srcdir / submodule):
            print(path, file=outfile)


def assets_to_manifest(blender_srcdir: Path, outfile: TextIO) -> None:
    assert not blender_srcdir.is_absolute()

    assets_dir = blender_srcdir.parent / "lib" / "assets"
    for path in assets_dir.glob("*"):
        if path.name == "working":
            continue
        if path.name in SKIP_NAMES:
            continue
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
    packages_dir: Optional[Path],
) -> None:
    print(f'Creating archive:            "{tarball}" ...', end="", flush=True)
    command = ["tar"]

    # Requires GNU `tar`, since `--transform` is used.
    if packages_dir:
        command += ["--transform", f"s,{packages_dir}/,packages/,g"]

    command += [
        "--transform",
        f"s,^{blender_srcdir.name}/,blender-{version}/,g",
        "--transform",
        f"s,^lib/assets/,blender-{version}/release/datafiles/assets/,g",
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


def git_ls_files(directory: Path = Path(".")) -> Iterable[Path]:
    """Generator, yields lines of output from 'git ls-files'.

    Only lines that are actually files (so no directories, sockets, etc.) are
    returned, and never one from SKIP_NAMES.
    """
    for line in git_command("-C", str(directory), "ls-files"):
        path = directory / line
        if not path.is_file() or path.name in SKIP_NAMES:
            continue
        yield path


def git_command(*cli_args: Union[bytes, str, Path]) -> Iterable[str]:
    """Generator, yields lines of output from a Git command."""
    command = ("git", *cli_args)

    # import shlex
    # print(">", " ".join(shlex.quote(arg) for arg in command))

    git = subprocess.run(
        command, stdout=subprocess.PIPE, check=True, text=True, timeout=30
    )
    for line in git.stdout.split("\n"):
        if line:
            yield line


if __name__ == "__main__":
    import doctest

    if doctest.testmod().failed:
        raise SystemExit("ERROR: Self-test failed, refusing to run")

    main()
