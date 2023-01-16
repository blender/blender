#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later

"""
"make update" for all platforms, updating svn libraries and tests and Blender
git repository and sub-modules.

For release branches, this will check out the appropriate branches of
sub-modules and libraries.
"""

import argparse
import os
import platform
import shutil
import sys

import make_utils
from make_utils import call, check_output

from typing import (
    List,
    Optional,
)


def print_stage(text: str) -> None:
    print("")
    print(text)
    print("")

# Parse arguments


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--no-libraries", action="store_true")
    parser.add_argument("--no-blender", action="store_true")
    parser.add_argument("--no-submodules", action="store_true")
    parser.add_argument("--use-tests", action="store_true")
    parser.add_argument("--svn-command", default="svn")
    parser.add_argument("--svn-branch", default=None)
    parser.add_argument("--git-command", default="git")
    parser.add_argument("--use-linux-libraries", action="store_true")
    return parser.parse_args()


def get_blender_git_root() -> str:
    return check_output([args.git_command, "rev-parse", "--show-toplevel"])

# Setup for precompiled libraries and tests from svn.


def svn_update(args: argparse.Namespace, release_version: Optional[str]) -> None:
    svn_non_interactive = [args.svn_command, '--non-interactive']

    lib_dirpath = os.path.join(get_blender_git_root(), '..', 'lib')
    svn_url = make_utils.svn_libraries_base_url(release_version, args.svn_branch)

    # Checkout precompiled libraries
    if sys.platform == 'darwin':
        # Check platform.version to detect arm64 with x86_64 python binary.
        if platform.machine() == 'arm64' or ('ARM64' in platform.version()):
            lib_platform = "darwin_arm64"
        elif platform.machine() == 'x86_64':
            lib_platform = "darwin"
        else:
            lib_platform = None
    elif sys.platform == 'win32':
        # Windows checkout is usually handled by bat scripts since python3 to run
        # this script is bundled as part of the precompiled libraries. However it
        # is used by the buildbot.
        lib_platform = "win64_vc15"
    elif args.use_linux_libraries:
        lib_platform = "linux_x86_64_glibc_228"
    else:
        # No precompiled libraries for Linux.
        lib_platform = None

    if lib_platform:
        lib_platform_dirpath = os.path.join(lib_dirpath, lib_platform)

        if not os.path.exists(lib_platform_dirpath):
            print_stage("Checking out Precompiled Libraries")

            if make_utils.command_missing(args.svn_command):
                sys.stderr.write("svn not found, can't checkout libraries\n")
                sys.exit(1)

            svn_url_platform = svn_url + lib_platform
            call(svn_non_interactive + ["checkout", svn_url_platform, lib_platform_dirpath])

    if args.use_tests:
        lib_tests = "tests"
        lib_tests_dirpath = os.path.join(lib_dirpath, lib_tests)

        if not os.path.exists(lib_tests_dirpath):
            print_stage("Checking out Tests")

            if make_utils.command_missing(args.svn_command):
                sys.stderr.write("svn not found, can't checkout tests\n")
                sys.exit(1)

            svn_url_tests = svn_url + lib_tests
            call(svn_non_interactive + ["checkout", svn_url_tests, lib_tests_dirpath])

    # Update precompiled libraries and tests

    if not os.path.isdir(lib_dirpath):
        print("Library path: %r, not found, skipping" % lib_dirpath)
    else:
        paths_local_and_remote = []
        if os.path.exists(os.path.join(lib_dirpath, ".svn")):
            print_stage("Updating Precompiled Libraries and Tests (one repository)")
            paths_local_and_remote.append((lib_dirpath, svn_url))
        else:
            print_stage("Updating Precompiled Libraries and Tests (multiple repositories)")
            # Separate paths checked out.
            for dirname in os.listdir(lib_dirpath):
                if dirname.startswith("."):
                    # Temporary paths such as ".mypy_cache" will report a warning, skip hidden directories.
                    continue

                dirpath = os.path.join(lib_dirpath, dirname)
                if not (os.path.isdir(dirpath) and os.path.exists(os.path.join(dirpath, ".svn"))):
                    continue

                paths_local_and_remote.append((dirpath, svn_url + dirname))

        if paths_local_and_remote:
            if make_utils.command_missing(args.svn_command):
                sys.stderr.write("svn not found, can't update libraries\n")
                sys.exit(1)

            for dirpath, svn_url_full in paths_local_and_remote:
                call(svn_non_interactive + ["cleanup", dirpath])
                # Switch to appropriate branch and update.
                call(svn_non_interactive + ["switch", svn_url_full, dirpath], exit_on_error=False)
                call(svn_non_interactive + ["update", dirpath])


# Test if git repo can be updated.
def git_update_skip(args: argparse.Namespace, check_remote_exists: bool = True) -> str:
    if make_utils.command_missing(args.git_command):
        sys.stderr.write("git not found, can't update code\n")
        sys.exit(1)

    # Abort if a rebase is still progress.
    rebase_merge = check_output([args.git_command, 'rev-parse', '--git-path', 'rebase-merge'], exit_on_error=False)
    rebase_apply = check_output([args.git_command, 'rev-parse', '--git-path', 'rebase-apply'], exit_on_error=False)
    merge_head = check_output([args.git_command, 'rev-parse', '--git-path', 'MERGE_HEAD'], exit_on_error=False)
    if (
            os.path.exists(rebase_merge) or
            os.path.exists(rebase_apply) or
            os.path.exists(merge_head)
    ):
        return "rebase or merge in progress, complete it first"

    # Abort if uncommitted changes.
    changes = check_output([args.git_command, 'status', '--porcelain', '--untracked-files=no'])
    if len(changes) != 0:
        return "you have unstaged changes"

    # Test if there is an upstream branch configured
    if check_remote_exists:
        branch = check_output([args.git_command, "rev-parse", "--abbrev-ref", "HEAD"])
        remote = check_output([args.git_command, "config", "branch." + branch + ".remote"], exit_on_error=False)
        if len(remote) == 0:
            return "no remote branch to pull from"

    return ""


# Update blender repository.
def blender_update(args: argparse.Namespace) -> None:
    print_stage("Updating Blender Git Repository")
    call([args.git_command, "pull", "--rebase"])


# Update submodules.
def submodules_update(
        args: argparse.Namespace,
        release_version: Optional[str],
        branch: Optional[str],
) -> str:
    print_stage("Updating Submodules")
    if make_utils.command_missing(args.git_command):
        sys.stderr.write("git not found, can't update code\n")
        sys.exit(1)

    # Update submodules to appropriate given branch,
    # falling back to master if none is given and/or found in a sub-repository.
    branch_fallback = "master"
    if not branch:
        branch = branch_fallback

    submodules = [
        ("release/scripts/addons", branch, branch_fallback),
        ("release/scripts/addons_contrib", branch, branch_fallback),
        ("release/datafiles/locale", branch, branch_fallback),
        ("source/tools", branch, branch_fallback),
    ]

    # Initialize submodules only if needed.
    for submodule_path, submodule_branch, submodule_branch_fallback in submodules:
        if not os.path.exists(os.path.join(submodule_path, ".git")):
            call([args.git_command, "submodule", "update", "--init", "--recursive"])
            break

    # Checkout appropriate branch and pull changes.
    skip_msg = ""
    for submodule_path, submodule_branch, submodule_branch_fallback in submodules:
        cwd = os.getcwd()
        try:
            os.chdir(submodule_path)
            msg = git_update_skip(args, check_remote_exists=False)
            if msg:
                skip_msg += submodule_path + " skipped: " + msg + "\n"
            else:
                # Find a matching branch that exists.
                call([args.git_command, "fetch", "origin"])
                if make_utils.git_branch_exists(args.git_command, submodule_branch):
                    pass
                elif make_utils.git_branch_exists(args.git_command, submodule_branch_fallback):
                    submodule_branch = submodule_branch_fallback
                else:
                    # Skip.
                    submodule_branch = ""

                # Switch to branch and pull.
                if submodule_branch:
                    if make_utils.git_branch(args.git_command) != submodule_branch:
                        call([args.git_command, "checkout", submodule_branch])
                    call([args.git_command, "pull", "--rebase", "origin", submodule_branch])
        finally:
            os.chdir(cwd)

    return skip_msg


if __name__ == "__main__":
    args = parse_arguments()
    blender_skip_msg = ""
    submodules_skip_msg = ""

    # Test if we are building a specific release version.
    branch = make_utils.git_branch(args.git_command)
    if branch == 'HEAD':
        sys.stderr.write('Blender git repository is in detached HEAD state, must be in a branch\n')
        sys.exit(1)

    tag = make_utils.git_tag(args.git_command)
    release_version = make_utils.git_branch_release_version(branch, tag)

    if not args.no_libraries:
        svn_update(args, release_version)
    if not args.no_blender:
        blender_skip_msg = git_update_skip(args)
        if blender_skip_msg:
            blender_skip_msg = "Blender repository skipped: " + blender_skip_msg + "\n"
        else:
            blender_update(args)
    if not args.no_submodules:
        submodules_skip_msg = submodules_update(args, release_version, branch)

    # Report any skipped repositories at the end, so it's not as easy to miss.
    skip_msg = blender_skip_msg + submodules_skip_msg
    if skip_msg:
        print_stage(skip_msg.strip())

    # For failed submodule update we throw an error, since not having correct
    # submodules can make Blender throw errors.
    # For Blender itself we don't and consider "make update" to be a command
    # you can use while working on uncommitted code.
    if submodules_skip_msg:
        sys.exit(1)
