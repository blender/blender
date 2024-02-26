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
import sys

from pathlib import Path

import make_utils
from make_utils import call, check_output


def print_stage(text):
    print("")
    print(text)
    print("=" * len(text))
    print("")

# Parse arguments


def parse_arguments():
    parser = argparse.ArgumentParser()
    parser.add_argument("--no-libraries", action="store_true")
    parser.add_argument("--no-blender", action="store_true")
    parser.add_argument("--no-submodules", action="store_true")
    parser.add_argument("--use-tests", action="store_true")
    parser.add_argument("--svn-command", default="svn")
    parser.add_argument("--svn-branch", default=None)
    parser.add_argument("--git-command", default="git")

    # NOTE: Both old and new style command line flags, so that the Buildbot can use the new style.
    # It is not possible to know from the Buildbot which style to use when building patches.
    #
    # Acts as an alias: `use_centos_libraries or use_linux_libraries`.
    parser.add_argument("--use-centos-libraries", action="store_true")
    parser.add_argument("--use-linux-libraries", action="store_true")

    parser.add_argument("--architecture", type=str, choices=("x86_64", "amd64", "arm64",))

    return parser.parse_args()


def get_blender_git_root():
    return Path(check_output([args.git_command, "rev-parse", "--show-toplevel"]))

# Setup for precompiled libraries and tests from svn.


def get_effective_platform(args) -> str:
    """
    Get platform of the host.

    The result string is normalized to the name used by Blender releases and
    library repository name prefixes: linux, macos, windows.
    """

    if sys.platform == "darwin":
        platform = "macos"
    elif sys.platform == "win32":
        platform = "windows"
    else:
        platform = sys.platform

    assert (platform in ("linux", "macos", "windows"))

    return platform


def get_effective_architecture(args) -> str:
    """
    Get architecture of the host.

    The result string is normalized to the architecture name used by the Blender
    releases and library repository name suffixes: x64, arm64.

    NOTE: When cross-compiling the architecture is coming from the command line
    argument.
    """
    architecture = args.architecture
    if architecture:
        assert isinstance(architecture, str)
    elif "ARM64" in platform.version():
        # Check platform.version to detect arm64 with x86_64 python binary.
        architecture = "arm64"
    else:
        architecture = platform.machine().lower()

    # Normalize the architecture name.
    if architecture in ("x86_64", "amd64"):
        architecture = "x64"

    assert (architecture in ("x64", "arm64"))

    return architecture


def get_submodule_directories(args):
    """
    Get list of all configured submodule directories.
    """

    blender_git_root = get_blender_git_root()
    dot_modules = blender_git_root / ".gitmodules"

    if not dot_modules.exists():
        return ()

    submodule_directories_output = check_output(
        [args.git_command, "config", "--file", dot_modules, "--get-regexp", "path"])
    return (Path(line.split(' ', 1)[1]) for line in submodule_directories_output.strip().splitlines())


def ensure_git_lfs(args) -> None:
    # Use `--skip-repo` to avoid creating git hooks.
    # This is called from the `blender.git` checkout, so we don't need to install hooks there.
    call((args.git_command, "lfs", "install", "--skip-repo"), exit_on_error=True)


def update_precompiled_libraries(args):
    """
    Configure and update submodule for precompiled libraries

    This function detects the current host architecture and enables
    corresponding submodule, and updates the submodule.

    NOTE: When cross-compiling the architecture is coming from the command line
    argument.
    """

    print_stage("Configuring Precompiled Libraries")

    platform = get_effective_platform(args)
    arch = get_effective_architecture(args)

    print(f"Detected platform     : {platform}")
    print(f"Detected architecture : {arch}")
    print()

    if sys.platform == "linux" and not args.use_linux_libraries:
        print("Skipping Linux libraries configuration")
        return ""

    submodule_dir = f"lib/{platform}_{arch}"

    submodule_directories = get_submodule_directories(args)

    if Path(submodule_dir) not in submodule_directories:
        return "Skipping libraries update: no configured submodule\n"

    make_utils.git_enable_submodule(args.git_command, submodule_dir)

    if not make_utils.git_update_submodule(args.git_command, submodule_dir):
        return "Error updating precompiled libraries\n"

    return ""


def update_tests_data_files(args):
    """
    Configure and update submodule with files used by regression tests
    """

    print_stage("Configuring Tests Data Files")

    submodule_dir = "tests/data"

    make_utils.git_enable_submodule(args.git_command, submodule_dir)

    if not make_utils.git_update_submodule(args.git_command, submodule_dir):
        return "Error updating test data\n"

    return ""


# Test if git repo can be updated.
def git_update_skip(args, check_remote_exists=True):
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
def blender_update(args):
    print_stage("Updating Blender Git Repository")
    call([args.git_command, "pull", "--rebase"])


def submodules_update_non_tracking(args, release_version, branch):
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
        print(f"- Updating {submodule_path}")
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
                    submodule_branch = None

                # Switch to branch and pull.
                if submodule_branch:
                    if make_utils.git_branch(args.git_command) != submodule_branch:
                        call([args.git_command, "checkout", submodule_branch])
                    call([args.git_command, "pull", "--rebase", "origin", submodule_branch])
        finally:
            os.chdir(cwd)

    return skip_msg


def submodules_update_tracking(args, release_version, branch):
    msg = ""

    submodule_directories = get_submodule_directories(args)
    for submodule_path in submodule_directories:
        if submodule_path.parts[0] == "lib" and args.no_libraries:
            print(f"Skipping library submodule {submodule_path}")
            continue

        if submodule_path.parts[0] == "tests" and not args.use_tests:
            print(f"Skipping tests submodule {submodule_path}")
            continue

        print(f"- Updating {submodule_path}")

        if not make_utils.git_update_submodule(args.git_command, submodule_path):
            msg += f"Error updating Git submodule {submodule_path}\n"

    return msg


def submodules_update(args, release_version, branch):
    print_stage("Updating Submodules")

    if make_utils.command_missing(args.git_command):
        sys.stderr.write("git not found, can't update code\n")
        sys.exit(1)

    msg = ""

    msg += submodules_update_non_tracking(args, release_version, branch)
    msg += submodules_update_tracking(args, release_version, branch)

    return msg


if __name__ == "__main__":
    args = parse_arguments()
    blender_skip_msg = ""
    libraries_skip_msg = ""
    submodules_skip_msg = ""

    blender_version = make_utils. parse_blender_version()
    if blender_version.cycle != 'alpha':
        major = blender_version.version // 100
        minor = blender_version.version % 100
        branch = f"blender-v{major}.{minor}-release"
        release_version = f"{major}.{minor}"
    else:
        branch = 'main'
        release_version = None

    # Submodules and precompiled libraries require Git LFS.
    ensure_git_lfs(args)

    if not args.no_blender:
        blender_skip_msg = git_update_skip(args)
        if blender_skip_msg:
            blender_skip_msg = "Blender repository skipped: " + blender_skip_msg + "\n"
        else:
            blender_update(args)

    if not args.no_libraries:
        libraries_skip_msg += update_precompiled_libraries(args)
        if args.use_tests:
            libraries_skip_msg += update_tests_data_files(args)

    if not args.no_submodules:
        submodules_skip_msg = submodules_update(args, release_version, branch)

    # Report any skipped repositories at the end, so it's not as easy to miss.
    skip_msg = blender_skip_msg + libraries_skip_msg + submodules_skip_msg
    if skip_msg:
        print_stage("Update finished with the following messages")
        print(skip_msg.strip())

    # For failed submodule update we throw an error, since not having correct
    # submodules can make Blender throw errors.
    # For Blender itself we don't and consider "make update" to be a command
    # you can use while working on uncommitted code.
    if submodules_skip_msg:
        sys.exit(1)
