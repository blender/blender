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
from pathlib import Path
from make_utils import call, check_output
from urllib.parse import urljoin

from typing import (
    List,
    Iterable,
    Optional,
)


class Submodule:
    path: str
    branch: str
    branch_fallback: str

    def __init__(self, path: str, branch: str, branch_fallback: str) -> None:
        self.path = path
        self.branch = branch
        self.branch_fallback = branch_fallback


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
    parser.add_argument("--architecture", type=str, choices=("x86_64", "amd64", "arm64",))
    return parser.parse_args()


def get_blender_git_root() -> str:
    return check_output([args.git_command, "rev-parse", "--show-toplevel"])

# Setup for precompiled libraries and tests from svn.


def get_effective_architecture(args: argparse.Namespace) -> str:
    architecture = args.architecture
    if architecture:
        assert isinstance(architecture, str)
        return architecture

    # Check platform.version to detect arm64 with x86_64 python binary.
    if "ARM64" in platform.version():
        return "arm64"

    return platform.machine().lower()


def svn_update(args: argparse.Namespace, release_version: Optional[str]) -> None:
    svn_non_interactive = [args.svn_command, '--non-interactive']

    lib_dirpath = os.path.join(get_blender_git_root(), '..', 'lib')
    svn_url = make_utils.svn_libraries_base_url(release_version, args.svn_branch)

    # Checkout precompiled libraries
    architecture = get_effective_architecture(args)
    if sys.platform == 'darwin':
        if architecture == 'arm64':
            lib_platform = "darwin_arm64"
        elif architecture == 'x86_64':
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

    lib_assets = "assets"
    lib_assets_dirpath = os.path.join(lib_dirpath, lib_assets)

    if not os.path.exists(lib_assets_dirpath):
        print_stage("Checking out Assets")

        if make_utils.command_missing(args.svn_command):
            sys.stderr.write("svn not found, can't checkout assets\n")
            sys.exit(1)

        svn_url_assets = svn_url + lib_assets
        call(svn_non_interactive + ["checkout", svn_url_assets, lib_assets_dirpath])

    # Update precompiled libraries, assets and tests

    if not os.path.isdir(lib_dirpath):
        print("Library path: %r, not found, skipping" % lib_dirpath)
    else:
        paths_local_and_remote = []
        if os.path.exists(os.path.join(lib_dirpath, ".svn")):
            print_stage("Updating Precompiled Libraries, Assets and Tests (one repository)")
            paths_local_and_remote.append((lib_dirpath, svn_url))
        else:
            print_stage("Updating Precompiled Libraries, Assets and Tests (multiple repositories)")
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
    changes = check_output([args.git_command, 'status', '--porcelain', '--untracked-files=no', '--ignore-submodules'])
    if len(changes) != 0:
        return "you have unstaged changes"

    # Test if there is an upstream branch configured
    if check_remote_exists:
        branch = check_output([args.git_command, "rev-parse", "--abbrev-ref", "HEAD"])
        remote = check_output([args.git_command, "config", "branch." + branch + ".remote"], exit_on_error=False)
        if len(remote) == 0:
            return "no remote branch to pull from"

    return ""


def use_upstream_workflow(args: argparse.Namespace) -> bool:
    return make_utils.git_remote_exist(args.git_command, "upstream")


def work_tree_update_upstream_workflow(args: argparse.Namespace, use_fetch: bool = True) -> str:
    """
    Update the Blender repository using the Github style of fork organization

    Returns true if the current local branch has been updated to the upstream state.
    Otherwise false is returned.
    """

    branch_name = make_utils.git_branch(args.git_command)

    if use_fetch:
        call((args.git_command, "fetch", "upstream"))

    upstream_branch = f"upstream/{branch_name}"
    if not make_utils.git_branch_exists(args.git_command, upstream_branch):
        return "no_branch"

    retcode = call((args.git_command, "merge", "--ff-only", upstream_branch), exit_on_error=False)
    if retcode != 0:
        return "Unable to fast forward\n"

    return ""


def work_tree_update(args: argparse.Namespace, use_fetch: bool = True) -> str:
    """
    Update the Git working tree using the best strategy

    This function detects whether it is a github style of fork remote organization is used, or
    is it a repository which origin is an upstream.
    """

    if use_upstream_workflow(args):
        message = work_tree_update_upstream_workflow(args, use_fetch)
        if message != "no_branch":
            return message

        # If there is upstream configured but the local branch is not in the upstream, try to
        # update the branch from the fork.

    update_command = [args.git_command, "pull", "--rebase"]

    call(update_command)

    return ""


# Update blender repository.
def blender_update(args: argparse.Namespace) -> str:
    print_stage("Updating Blender Git Repository")

    return work_tree_update(args)


def resolve_external_url(blender_url: str, repo_name: str) -> str:
    return urljoin(blender_url + "/", "../" + repo_name)


def external_script_copy_old_submodule_over(args: argparse.Namespace, directory_name: str) -> None:
    blender_git_root = Path(get_blender_git_root())
    scripts_dir = blender_git_root / "scripts"
    external_dir = scripts_dir / directory_name

    old_submodule_relative_dir = Path("release") / "scripts" / directory_name
    print(f"Moving {old_submodule_relative_dir} to scripts/{directory_name} ...")

    old_submodule_dir = blender_git_root / old_submodule_relative_dir
    shutil.move(old_submodule_dir, external_dir)

    # Remove old ".git" which is a file with path to a submodule bare repo inside of main
    # repo .git/modules directory.
    (external_dir / ".git").unlink()

    bare_repo_relative_dir = Path(".git") / "modules" / "release" / "scripts" / directory_name
    print(f"Copying {bare_repo_relative_dir} to scripts/{directory_name}/.git ...")
    bare_repo_dir = blender_git_root / bare_repo_relative_dir
    shutil.copytree(bare_repo_dir, external_dir / ".git")

    git_config = external_dir / ".git" / "config"
    call((args.git_command, "config", "--file", str(git_config), "--unset", "core.worktree"))


def external_script_initialize_if_needed(args: argparse.Namespace,
                                         repo_name: str,
                                         directory_name: str) -> None:
    """Initialize checkout of an external repository scripts directory"""

    blender_git_root = Path(get_blender_git_root())
    blender_dot_git = blender_git_root / ".git"
    scripts_dir = blender_git_root / "scripts"
    external_dir = scripts_dir / directory_name

    if external_dir.exists():
        return

    print(f"Initializing scripts/{directory_name} ...")

    old_submodule_dot_git = blender_git_root / "release" / "scripts" / directory_name / ".git"
    if old_submodule_dot_git.exists() and blender_dot_git.is_dir():
        external_script_copy_old_submodule_over(args, directory_name)
        return

    origin_name = "upstream" if use_upstream_workflow(args) else "origin"
    blender_url = make_utils.git_get_remote_url(args.git_command, origin_name)
    external_url = resolve_external_url(blender_url, repo_name)

    # When running `make update` from a freshly cloned fork check whether the fork of the submodule is
    # available, If not, switch to the submodule relative to the main blender repository.
    if origin_name == "origin" and not make_utils.git_is_remote_repository(args.git_command, external_url):
        external_url = resolve_external_url("https://projects.blender.org/blender/blender", repo_name)

    call((args.git_command, "clone", "--origin", origin_name, external_url, str(external_dir)))


def external_script_add_origin_if_needed(args: argparse.Namespace,
                                         repo_name: str,
                                         directory_name: str) -> None:
    """
    Add remote called 'origin' if there is a fork of the external repository available

    This is only done when using Github style upstream workflow in the main repository.
    """

    if not use_upstream_workflow(args):
        return

    cwd = os.getcwd()

    blender_git_root = Path(get_blender_git_root())
    scripts_dir = blender_git_root / "scripts"
    external_dir = scripts_dir / directory_name

    origin_blender_url = make_utils.git_get_remote_url(args.git_command, "origin")
    origin_external_url = resolve_external_url(origin_blender_url, repo_name)

    try:
        os.chdir(external_dir)

        if (make_utils.git_remote_exist(args.git_command, "origin") or
                not make_utils.git_remote_exist(args.git_command, "upstream")):
            return

        if not make_utils.git_is_remote_repository(args.git_command, origin_external_url):
            return

        print(f"Adding origin remote to {directory_name} pointing to fork ...")

        # Non-obvious tricks to introduce the new remote called "origin" to the existing
        # submodule configuration.
        #
        # This is all within the content of creating a fork of a submodule after `make update`
        # has been run and possibly local branches tracking upstream were added.
        #
        # The idea here goes as following:
        #
        #  - Rename remote "upstream" to "origin", which takes care of changing the names of
        #    remotes the local branches are tracking.
        #
        #  - Change the URL to the "origin", which was still pointing to upstream.
        #
        #  - Re-introduce the "upstream" remote, with the same URL as it had prior to rename.

        upstream_url = make_utils.git_get_remote_url(args.git_command, "upstream")

        call((args.git_command, "remote", "rename", "upstream", "origin"))
        make_utils.git_set_config(args.git_command, f"remote.origin.url", origin_external_url)

        call((args.git_command, "remote", "add", "upstream", upstream_url))
    finally:
        os.chdir(cwd)

    return


def external_scripts_update(args: argparse.Namespace,
                            repo_name: str,
                            directory_name: str,
                            branch: Optional[str]) -> str:
    """Update a single external checkout with the given name in the scripts folder"""

    external_script_initialize_if_needed(args, repo_name, directory_name)
    external_script_add_origin_if_needed(args, repo_name, directory_name)

    print(f"Updating scripts/{directory_name} ...")

    cwd = os.getcwd()

    blender_git_root = Path(get_blender_git_root())
    scripts_dir = blender_git_root / "scripts"
    external_dir = scripts_dir / directory_name

    # Update externals to appropriate given branch, falling back to main if none is given and/or
    # found in a sub-repository.
    branch_fallback = "main"
    if not branch:
        branch = branch_fallback

    skip_msg = ""

    try:
        os.chdir(external_dir)
        msg = git_update_skip(args, check_remote_exists=False)
        if msg:
            skip_msg += directory_name + " skipped: " + msg + "\n"
        else:
            # Find a matching branch that exists.
            for remote in ("origin", "upstream"):
                if make_utils.git_remote_exist(args.git_command, remote):
                    call([args.git_command, "fetch", remote])

            submodule_branch = branch

            if make_utils.git_branch_exists(args.git_command, submodule_branch):
                pass
            elif make_utils.git_branch_exists(args.git_command, branch_fallback):
                submodule_branch = branch_fallback
            else:
                # Skip.
                submodule_branch = ""

            # Switch to branch and pull.
            if submodule_branch:
                if make_utils.git_branch(args.git_command) != submodule_branch:
                    # If the local branch exists just check out to it.
                    # If there is no local branch but only remote specify an explicit remote.
                    # Without this explicit specification Git attempts to set-up tracking
                    # automatically and fails when the branch is available in multiple remotes.
                    if make_utils.git_local_branch_exists(args.git_command, submodule_branch):
                        call([args.git_command, "checkout", submodule_branch])
                    else:
                        if make_utils.git_remote_branch_exists(args.git_command, "origin", submodule_branch):
                            call([args.git_command, "checkout", "-t", f"origin/{submodule_branch}"])
                        elif make_utils.git_remote_exist(args.git_command, "upstream"):
                            # For the Github style of upstream workflow create a local branch from
                            # the upstream, but do not track it, so that we stick to the paradigm
                            # that no local branches are tracking upstream, preventing possible
                            # accidental commit to upstream.
                            call([args.git_command, "checkout", "-b", submodule_branch,
                                 f"upstream/{submodule_branch}", "--no-track"])

                # Don't use extra fetch since all remotes of interest have been already fetched
                # some lines above.
                skip_msg += work_tree_update(args, use_fetch=False)
    finally:
        os.chdir(cwd)

    return skip_msg


def scripts_submodules_update(args: argparse.Namespace, branch: Optional[str]) -> str:
    """Update working trees of addons and addons_contrib within the scripts/ directory"""
    msg = ""

    msg += external_scripts_update(args, "blender-addons", "addons", branch)
    msg += external_scripts_update(args, "blender-addons-contrib", "addons_contrib", branch)

    return msg


def submodules_update(args: argparse.Namespace, branch: Optional[str]) -> str:
    """Update submodules or other externally tracked source trees"""
    msg = ""

    msg += scripts_submodules_update(args, branch)

    return msg


if __name__ == "__main__":
    args = parse_arguments()
    blender_skip_msg = ""
    submodules_skip_msg = ""

    blender_version = make_utils. parse_blender_version()
    if blender_version.cycle != 'alpha':
        major = blender_version.version // 100
        minor = blender_version.version % 100
        branch = f"blender-v{major}.{minor}-release"
        release_version: Optional[str] = f"{major}.{minor}"
    else:
        branch = 'main'
        release_version = None

    if not args.no_libraries:
        svn_update(args, release_version)
    if not args.no_blender:
        blender_skip_msg = git_update_skip(args)
        if not blender_skip_msg:
            blender_skip_msg = blender_update(args)
        if blender_skip_msg:
            blender_skip_msg = "Blender repository skipped: " + blender_skip_msg + "\n"
    if not args.no_submodules:
        submodules_skip_msg = submodules_update(args, branch)

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
