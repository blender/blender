#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later

"""
Utility functions for make update and make tests.
"""

import re
import shutil
import subprocess
import sys


def call(cmd, exit_on_error=True, silent=False):
    if not silent:
        print(" ".join(cmd))

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


def check_output(cmd, exit_on_error=True):
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


def git_branch_exists(git_command, branch):
    return (
        call([git_command, "rev-parse", "--verify", branch], exit_on_error=False, silent=True) == 0 or
        call([git_command, "rev-parse", "--verify", "remotes/origin/" + branch], exit_on_error=False, silent=True) == 0
    )


def git_branch(git_command):
    # Get current branch name.
    try:
        branch = subprocess.check_output([git_command, "rev-parse", "--abbrev-ref", "HEAD"])
    except subprocess.CalledProcessError as e:
        sys.stderr.write("Failed to get Blender git branch\n")
        sys.exit(1)

    return branch.strip().decode('utf8')


def git_tag(git_command):
    # Get current tag name.
    try:
        tag = subprocess.check_output([git_command, "describe", "--exact-match"], stderr=subprocess.STDOUT)
    except subprocess.CalledProcessError as e:
        return None

    return tag.strip().decode('utf8')


def git_branch_release_version(branch, tag):
    release_version = re.search("^blender-v(.*)-release$", branch)
    if release_version:
        release_version = release_version.group(1)
    elif tag:
        release_version = re.search(r"^v([0-9]*\.[0-9]*).*", tag)
        if release_version:
            release_version = release_version.group(1)
    return release_version


def svn_libraries_base_url(release_version, branch=None):
    if release_version:
        svn_branch = "tags/blender-" + release_version + "-release"
    elif branch:
        svn_branch = "branches/" + branch
    else:
        svn_branch = "trunk"
    return "https://svn.blender.org/svnroot/bf-blender/" + svn_branch + "/lib/"


def command_missing(command):
    # Support running with Python 2 for macOS
    if sys.version_info >= (3, 0):
        return shutil.which(command) is None
    else:
        return False
