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
        lib_platform = "win64_vc14"
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
        if dirname == ".svn":
            continue

        dirpath = os.path.join(lib_dirpath, dirname)
        svn_dirpath = os.path.join(dirpath, ".svn")
        svn_root_dirpath = os.path.join(lib_dirpath, ".svn")

        if os.path.isdir(dirpath) and \
           (os.path.exists(svn_dirpath) or os.path.exists(svn_root_dirpath)):
            if make_utils.command_missing(args.svn_command):
                sys.stderr.write("svn not found, can't update libraries\n")
                sys.exit(1)

            call(svn_non_interactive + ["cleanup", dirpath])
            call(svn_non_interactive + ["switch", svn_url + dirname, dirpath])
            call(svn_non_interactive + ["update", dirpath])


# Update blender repository.
def blender_update_skip(args):
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
    branch = check_output([args.git_command, "rev-parse", "--abbrev-ref", "HEAD"])
    remote = check_output([args.git_command, "config", "branch." + branch + ".remote"], exit_on_error=False)
    if len(remote) == 0:
        return "no remote branch to pull from"

    return None

def blender_update(args):
    print_stage("Updating Blender Git Repository")
    call([args.git_command, "pull", "--rebase"])


# Update submodules.
def submodules_update(args, release_version):
    print_stage("Updating Submodules")
    if make_utils.command_missing(args.git_command):
        sys.stderr.write("git not found, can't update code\n")
        sys.exit(1)

    call([args.git_command, "submodule", "update", "--init", "--recursive"])
    if not release_version:
        # Update submodules to latest master if not building a specific release.
        # In that case submodules are set to a specific revision, which is checked
        # out by running "git submodule update".
        call([args.git_command, "submodule", "foreach", "git", "checkout", "master"])
        call([args.git_command, "submodule", "foreach", "git", "pull", "--rebase", "origin", "master"])


if __name__ == "__main__":
    args = parse_arguments()
    blender_skipped = None

    # Test if we are building a specific release version.
    release_version = make_utils.git_branch_release_version(args.git_command)

    if not args.no_libraries:
        svn_update(args, release_version)
    if not args.no_blender:
        blender_skipped = blender_update_skip(args)
        if not blender_skipped:
            blender_update(args)
    if not args.no_submodules:
        submodules_update(args, release_version)

    if blender_skipped:
        print_stage("Blender repository skipped: " + blender_skipped)
