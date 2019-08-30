#!/usr/bin/env python3
#
# "make test" for all platforms, running automated tests.

import argparse
import os
import shutil
import sys

from make_utils import call

# Parse arguments

def parse_arguments():
    parser = argparse.ArgumentParser()
    parser.add_argument("--ctest-command", default="ctest")
    parser.add_argument("build_directory")
    return parser.parse_args()

args = parse_arguments()
ctest_command = args.ctest_command
build_dir = args.build_directory

if shutil.which(ctest_command) is None:
    sys.stderr.write("ctest not found, can't run tests\n")
    sys.exit(1)

# Run tests
os.chdir(build_dir)
call([ctest_command, ".", "--output-on-failure"])
