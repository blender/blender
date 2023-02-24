#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later

"""
Utility functions for make update and make tests.
"""

import re
import shutil
import subprocess
import sys
import os
from pathlib import Path
from urllib.parse import urljoin

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
            sys.stderr.write(" ".join(cmd))
            sys.stderr.write(e.output + "\n")
            sys.exit(e.returncode)
        output = ""

    return output.strip()


def git_branch_exists(git_command: str, branch: str) -> bool:
    return (
        call([git_command, "rev-parse", "--verify", branch], exit_on_error=False, silent=True) == 0 or
        call([git_command, "rev-parse", "--verify", "remotes/upstream/" + branch], exit_on_error=False, silent=True) == 0 or
        call([git_command, "rev-parse", "--verify", "remotes/origin/" + branch], exit_on_error=False, silent=True) == 0
    )


def git_get_remote_url(git_command: str, remote_name: str) -> bool:
    return check_output((git_command, "ls-remote", "--get-url", remote_name))


def git_remote_exist(git_command: str, remote_name: str) -> bool:
    """Check whether there is a remote with the given name"""
    # `git ls-remote --get-url upstream` will print an URL if there is such remote configured, and
    # otherwise will print "upstream".
    remote_url = check_output((git_command, "ls-remote", "--get-url", remote_name))
    return remote_url != remote_name


def git_get_resolved_submodule_url(git_command: str, blender_url: str, submodule_path: str) -> str:
    git_root = check_output([git_command, "rev-parse", "--show-toplevel"])
    dot_gitmodules = os.path.join(git_root, ".gitmodules")

    submodule_key_prefix = f"submodule.{submodule_path}"
    submodule_key_url = f"{submodule_key_prefix}.url"

    gitmodule_url = git_get_config(
        git_command, submodule_key_url, file=dot_gitmodules)

    # A bit of a trickery to construct final URL.
    # Only works for the relative submodule URLs.
    #
    # Note that unless the LHS URL ends up with a slash urljoin treats the last component as a
    # file.
    assert gitmodule_url.startswith('..')
    return urljoin(blender_url + "/", gitmodule_url)


def git_is_remote_repository(git_command: str, repo: str) -> bool:
    """Returns true if the given repository is a valid/clonable git repo"""
    exit_code = call((git_command, "ls-remote", repo, "HEAD"), exit_on_error=False, silent=True)
    return exit_code == 0


def git_branch(git_command: str) -> str:
    # Get current branch name.
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


def git_tag(git_command: str) -> Optional[str]:
    # Get current tag name.
    try:
        tag = subprocess.check_output([git_command, "describe", "--exact-match"], stderr=subprocess.STDOUT)
    except subprocess.CalledProcessError as e:
        return None

    return tag.strip().decode('utf8')


def git_branch_release_version(branch: str, tag: Optional[str]) -> Optional[str]:
    re_match = re.search("^blender-v(.*)-release$", branch)
    release_version = None
    if re_match:
        release_version = re_match.group(1)
    elif tag:
        re_match = re.search(r"^v([0-9]*\.[0-9]*).*", tag)
        if re_match:
            release_version = re_match.group(1)
    return release_version


def svn_libraries_base_url(release_version: Optional[str], branch: Optional[str] = None) -> str:
    if release_version:
        svn_branch = "tags/blender-" + release_version + "-release"
    elif branch:
        svn_branch = "branches/" + branch
    else:
        svn_branch = "trunk"
    return "https://svn.blender.org/svnroot/bf-blender/" + svn_branch + "/lib/"


def command_missing(command: str) -> bool:
    # Support running with Python 2 for macOS
    if sys.version_info >= (3, 0):
        return shutil.which(command) is None
    else:
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
