#!/usr/bin/env python3

import dataclasses
import os
import re
import subprocess
from pathlib import Path
from typing import Iterable, TextIO

# This script can run from any location,
# output is created in the $CWD
#
# NOTE: while the Python part of this script is portable,
# it relies on external commands typically found on GNU/Linux.
# Support for other platforms could be added by moving GNU `tar` & `md5sum` use to Python.

SKIP_NAMES = {
    ".gitignore",
    ".gitmodules",
    ".arcconfig",
}


def main() -> None:
    output_dir = Path(".").absolute()
    blender_srcdir = Path(__file__).absolute().parent.parent.parent
    print(f"Source dir: {blender_srcdir}")

    version = parse_blender_version(blender_srcdir)
    manifest = output_dir / f"blender-{version}-manifest.txt"
    tarball = output_dir / f"blender-{version}.tar.xz"

    os.chdir(blender_srcdir)
    create_manifest(version, manifest)
    create_tarball(version, tarball, manifest)
    create_checksum_file(tarball)
    cleanup(manifest)
    print("Done!")


@dataclasses.dataclass
class BlenderVersion:
    version: int  # 293 for 2.93.1
    patch: int  # 1 for 2.93.1
    cycle: str  # 'alpha', 'beta', 'release', maybe others.

    @property
    def is_release(self) -> bool:
        return self.cycle == "release"

    def __str__(self) -> str:
        """Convert to version string.

        >>> str(BlenderVersion(293, 1, "alpha"))
        '2.93.1-alpha'
        >>> str(BlenderVersion(327, 0, "release"))
        '3.27.0'
        """
        version_major = self.version // 100
        version_minor = self.version % 100
        as_string = f"{version_major}.{version_minor}.{self.patch}"
        if self.is_release:
            return as_string
        return f"{as_string}-{self.cycle}"


def parse_blender_version(blender_srcdir: Path) -> BlenderVersion:
    version_path = blender_srcdir / "source/blender/blenkernel/BKE_blender_version.h"

    version_info = {}
    line_re = re.compile(r"^#define (BLENDER_VERSION[A-Z_]*)\s+([0-9a-z]+)$")

    with version_path.open(encoding="utf-8") as version_file:
        for line in version_file:
            match = line_re.match(line.strip())
            if not match:
                continue
            version_info[match.group(1)] = match.group(2)

    return BlenderVersion(
        int(version_info["BLENDER_VERSION"]),
        int(version_info["BLENDER_VERSION_PATCH"]),
        version_info["BLENDER_VERSION_CYCLE"],
    )


### Manifest creation


def create_manifest(version: BlenderVersion, outpath: Path) -> None:
    print(f'Building manifest of files:  "{outpath}"...', end="", flush=True)
    with outpath.open("w", encoding="utf-8") as outfile:
        main_files_to_manifest(outfile)
        submodules_to_manifest(version, outfile)
    print("OK")


def main_files_to_manifest(outfile: TextIO) -> None:
    for path in git_ls_files():
        print(path, file=outfile)


def submodules_to_manifest(version: BlenderVersion, outfile: TextIO) -> None:
    skip_addon_contrib = version.is_release

    for line in git_command("submodule"):
        submodule = line.split()[1]

        # Don't use native slashes as GIT for MS-Windows outputs forward slashes.
        if skip_addon_contrib and submodule == "release/scripts/addons_contrib":
            continue

        for path in git_ls_files(Path(submodule)):
            print(path, file=outfile)


def create_tarball(version: BlenderVersion, tarball: Path, manifest: Path) -> None:
    print(f'Creating archive:            "{tarball}" ...', end="", flush=True)
    # Requires GNU `tar`, since `--transform` is used.
    command = [
        "tar",
        "--transform",
        f"s,^,blender-{version}/,g",
        "--use-compress-program=xz -9",
        "--create",
        f"--file={tarball}",
        f"--files-from={manifest}",
        # Without owner/group args, extracting the files as root will
        # use ownership from the tar archive:
        "--owner=0",
        "--group=0",
    ]
    subprocess.run(command, check=True, timeout=300)
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


## Low-level commands


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


def git_command(*cli_args) -> Iterable[str]:
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
