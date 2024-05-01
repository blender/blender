#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2019-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
"make test" for all platforms, running automated tests.
"""

import argparse
import os
import sys

import make_utils
from make_utils import call
from pathlib import Path

# Parse arguments.


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--ctest-command", default="ctest")
    parser.add_argument("--cmake-command", default="cmake")
    parser.add_argument("--git-command", default="git")
    parser.add_argument("--config", default="")
    parser.add_argument("build_directory")
    return parser.parse_args()


args = parse_arguments()
git_command = args.git_command
ctest_command = args.ctest_command
cmake_command = args.cmake_command
config = args.config
build_dir = args.build_directory

if make_utils.command_missing(ctest_command):
    sys.stderr.write("ctest not found, can't run tests\n")
    sys.exit(1)

if make_utils.command_missing(git_command):
    sys.stderr.write("git not found, can't run tests\n")
    sys.exit(1)

# Test if we are building a specific release version.
lib_tests_dirpath = Path("tests") / "data"

if not (lib_tests_dirpath / ".git").exists():
    print("Tests files not found, downloading...")

    if make_utils.command_missing(cmake_command):
        sys.stderr.write("cmake not found, can't checkout test files\n")
        sys.exit(1)

    # Ensure the test data files sub-module is configured and present.
    make_utils.git_enable_submodule(git_command, Path("tests") / "data")
    make_utils.git_update_submodule(args.git_command, lib_tests_dirpath)

    # Run cmake again to detect tests files.
    os.chdir(build_dir)
    call([cmake_command, "."])

# Run tests
tests_dir = os.path.join(build_dir, "tests")
os.makedirs(tests_dir, exist_ok=True)

os.chdir(build_dir)
command = [ctest_command, ".", "--output-on-failure"]
if len(config):
    command += ["-C", config]
    tests_log = "log_" + config + ".txt"
else:
    tests_log = "log.txt"
command += ["-O", os.path.join(tests_dir, tests_log)]
call(command)
