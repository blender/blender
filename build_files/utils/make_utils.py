#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later

"""
Utility functions for make update and make tests.
"""

import re
import shutil
import subprocess
import sys
from pathlib import Path

from typing import (
    Sequence,
    Optional,
)


def call(cmd: Sequence[str], exit_on_error: bool = True, silent: bool = False) -> int:
    if not silent:
        print(" ".join([str(x) for x in cmd]))

    # Flush to ensure correct order output on Windows.
    sys.stdout.flush()
    sys.stderr.flush()

    if silent:
        retcode = subprocess.call(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    else:
        retcode = subprocess.call(cmd)

    if exit_on_error and retcode != 0:
        sys.exit(retcode)
    return retcode


def check_output(cmd: Sequence[str], exit_on_error: bool = True) -> str:
    # Flush to ensure correct order output on Windows.
    sys.stdout.flush()
    sys.stderr.flush()

    try:
        output = subprocess.check_output(cmd, stderr=subprocess.STDOUT, universal_newlines=True)
    except subprocess.CalledProcessError as e:
        if exit_on_error:
            sys.stderr.write(" ".join(cmd) + "\n")
            sys.stderr.write(e.output + "\n")
            sys.exit(e.returncode)
        output = ""

    return output.strip()


def git_local_branch_exists(git_command: str, branch: str) -> bool:
    return (
        call([git_command, "rev-parse", "--verify", branch], exit_on_error=False, silent=True) == 0
    )


def git_remote_branch_exists(git_command: str, remote: str, branch: str) -> bool:
    return call([git_command, "rev-parse", "--verify", f"remotes/{remote}/{branch}"],
                exit_on_error=False, silent=True) == 0


def git_branch_exists(git_command: str, branch: str) -> bool:
    return (
        git_local_branch_exists(git_command, branch) or
        git_remote_branch_exists(git_command, "upstream", branch) or
        git_remote_branch_exists(git_command, "origin", branch)
    )


def git_get_remote_url(git_command: str, remote_name: str) -> str:
    return check_output((git_command, "ls-remote", "--get-url", remote_name))


def git_remote_exist(git_command: str, remote_name: str) -> bool:
    """Check whether there is a remote with the given name"""
    # `git ls-remote --get-url upstream` will print an URL if there is such remote configured, and
    # otherwise will print "upstream".
    remote_url = check_output((git_command, "ls-remote", "--get-url", remote_name))
    return remote_url != remote_name


def git_is_remote_repository(git_command: str, repo: str) -> bool:
    """Returns true if the given repository is a valid/clonable git repo"""
    exit_code = call((git_command, "ls-remote", repo, "HEAD"), exit_on_error=False, silent=True)
    return exit_code == 0


def git_branch(git_command: str) -> str:
    """Get current branch name."""

    try:
        branch = subprocess.check_output([git_command, "rev-parse", "--abbrev-ref", "HEAD"])
    except subprocess.CalledProcessError as e:
        sys.stderr.write("Failed to get Blender git branch\n")
        sys.exit(1)

    return branch.strip().decode('utf8')


def git_get_config(git_command: str, key: str, file: Optional[str] = None) -> str:
    if file:
        return check_output([git_command, "config", "--file", file, "--get", key])

    return check_output([git_command, "config", "--get", key])


def git_set_config(git_command: str, key: str, value: str, file: Optional[str] = None) -> str:
    if file:
        return check_output([git_command, "config", "--file", file, key, value])

    return check_output([git_command, "config", key, value])


def git_enable_submodule(git_command: str, submodule_dir: str):
    """Enable submodule denoted by its directory within the repository"""

    command = (git_command,
               "config",
               "--local",
               f"submodule.{submodule_dir}.update", "checkout")
    call(command, exit_on_error=True, silent=False)


def git_update_submodule(git_command: str, submodule_dir: str):
    """
    Update the given submodule.

    The submodule is denoted by its path within the repository.
    This function will initialize the submodule if it has not been initialized.
    """

    call((git_command, "submodule", "update", "--init", submodule_dir))


def command_missing(command: str) -> bool:
    # Support running with Python 2 for macOS
    if sys.version_info >= (3, 0):
        return shutil.which(command) is None
    return False


class BlenderVersion:
    def __init__(self, version: int, patch: int, cycle: str):
        # 293 for 2.93.1
        self.version = version
        # 1 for 2.93.1
        self.patch = patch
        # 'alpha', 'beta', 'release', maybe others.
        self.cycle = cycle

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
        if self.is_release():
            return as_string
        return f"{as_string}-{self.cycle}"


def parse_blender_version() -> BlenderVersion:
    blender_srcdir = Path(__file__).absolute().parent.parent.parent
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
