#!/usr/bin/env python3
#
# "make update" for all platforms, updating svn libraries and tests and Blender
# git repository and submodules.
#
# For release branches, this will check out the appropriate branches of
# submodules and libraries.

import argparse
import os
import shutil
import sys

import make_utils
from make_utils import call, check_output

def print_stage(text):
    print("")
    print(text)
    print("")

# Parse arguments
def parse_arguments():
    parser = argparse.ArgumentParser()
    parser.add_argument("--no-libraries", action="store_true")
    parser.add_argument("--no-blender", action="store_true")
    parser.add_argument("--no-submodules", action="store_true")
    parser.add_argument("--use-tests", action="store_true")
    parser.add_argument("--svn-command", default="svn")
    parser.add_argument("--git-command", default="git")
    parser.add_argument("--use-centos-libraries", action="store_true")
    return parser.parse_args()

def get_blender_git_root():
    return check_output([args.git_command, "rev-parse", "--show-toplevel"])

# Setup for precompiled libraries and tests from svn.
def svn_update(args, release_version):
    svn_non_interactive = [args.svn_command, '--non-interactive']

    lib_dirpath = os.path.join(get_blender_git_root(), '..', 'lib')
    svn_url = make_utils.svn_libraries_base_url(release_version)

    # Checkout precompiled libraries
    if sys.platform == 'darwin':
        lib_platform = "darwin"
    elif sys.platform == 'win32':
        # Windows checkout is usually handled by bat scripts since python3 to run
        # this script is bundled as part of the precompiled libraries. However it
        # is used by the buildbot.
        lib_platform = "win64_vc15"
    elif args.use_centos_libraries:
        lib_platform = "linux_centos7_x86_64"
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
    print_stage("Updating Precompiled Libraries and Tests")

    if os.path.isdir(lib_dirpath):
      for dirname in os.listdir(lib_dirpath):
        dirpath = os.path.join(lib_dirpath, dirname)

        if dirname == ".svn":
            # Cleanup must be run from svn root directory if it exists.
            if not make_utils.command_missing(args.svn_command):
                call(svn_non_interactive + ["cleanup", lib_dirpath])
            continue

        svn_dirpath = os.path.join(dirpath, ".svn")
        svn_root_dirpath = os.path.join(lib_dirpath, ".svn")

        if os.path.isdir(dirpath) and \
           (os.path.exists(svn_dirpath) or os.path.exists(svn_root_dirpath)):
            if make_utils.command_missing(args.svn_command):
                sys.stderr.write("svn not found, can't update libraries\n")
                sys.exit(1)

            # Cleanup to continue with interrupted downloads.
            if os.path.exists(svn_dirpath):
                call(svn_non_interactive + ["cleanup", dirpath])
            # Switch to appropriate branch and update.
            call(svn_non_interactive + ["switch", svn_url + dirname, dirpath], exit_on_error=False)
            call(svn_non_interactive + ["update", dirpath])

# Test if git repo can be updated.
def git_update_skip(args, check_remote_exists=True):
    if make_utils.command_missing(args.git_command):
        sys.stderr.write("git not found, can't update code\n")
        sys.exit(1)

    # Abort if a rebase is still progress.
    rebase_merge = check_output([args.git_command, 'rev-parse', '--git-path', 'rebase-merge'], exit_on_error=False)
    rebase_apply = check_output([args.git_command, 'rev-parse', '--git-path', 'rebase-apply'], exit_on_error=False)
    merge_head = check_output([args.git_command, 'rev-parse', '--git-path', 'MERGE_HEAD'], exit_on_error=False)
    if os.path.exists(rebase_merge) or \
       os.path.exists(rebase_apply) or \
       os.path.exists(merge_head):
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


# Update submodules.
def submodules_update(args, release_version, branch):
    print_stage("Updating Submodules")
    if make_utils.command_missing(args.git_command):
        sys.stderr.write("git not found, can't update code\n")
        sys.exit(1)

    # Update submodules to latest master or appropriate release branch.
    if not release_version:
        branch = "master"

    submodules = [
        ("release/scripts/addons", branch),
        ("release/scripts/addons_contrib", branch),
        ("release/datafiles/locale", branch),
        ("source/tools", branch),
    ]

    # Initialize submodules only if needed.
    for submodule_path, submodule_branch in submodules:
        if not os.path.exists(os.path.join(submodule_path, ".git")):
            call([args.git_command, "submodule", "update", "--init", "--recursive"])
            break

    # Checkout appropriate branch and pull changes.
    skip_msg = ""
    for submodule_path, submodule_branch in submodules:
        cwd = os.getcwd()
        try:
            os.chdir(submodule_path)
            msg = git_update_skip(args, check_remote_exists=False)
            if msg:
                skip_msg += submodule_path + " skipped: "  + msg + "\n"
            else:
                if make_utils.git_branch(args.git_command) != submodule_branch:
                    call([args.git_command, "fetch", "origin"])
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
