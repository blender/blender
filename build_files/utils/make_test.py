#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2019-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
"make test" for all platforms, running automated tests.
"""

__all__ = (
    "main",
)

import argparse
import os
import sys

import make_utils
from make_utils import call


# Parse arguments.
def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--ctest-command", default="ctest")
    parser.add_argument("--git-command", default="git")
    parser.add_argument("--config", default="")
    parser.add_argument("build_directory")
    return parser.parse_args()


def main() -> int:
    args = parse_arguments()
    git_command = args.git_command
    ctest_command = args.ctest_command
    config = args.config
    build_dir = args.build_directory

    if make_utils.command_missing(ctest_command):
        sys.stderr.write("ctest not found, can't run tests\n")
        return 1

    if make_utils.command_missing(git_command):
        sys.stderr.write("git not found, can't run tests\n")
        return 1

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
    return 0


if __name__ == "__main__":
    sys.exit(main())
