#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# Usage: ./check_release.py -- ../path/to/release/folder


import os
import sys
import unittest

import check_module_enabled
import check_module_numpy
import check_module_requests
import check_static_binaries
from check_utils import sliceCommandLineArguments


def load_tests(loader, standard_tests, pattern):
    standard_tests.addTests(loader.loadTestsFromTestCase(
        check_module_enabled.UnitTesting))
    standard_tests.addTests(loader.loadTestsFromTestCase(
        check_module_numpy.UnitTesting))
    standard_tests.addTests(loader.loadTestsFromTestCase(
        check_module_requests.UnitTesting))
    standard_tests.addTests(loader.loadTestsFromTestCase(
        check_static_binaries.UnitTesting))
    return standard_tests


def main():
    # Slice command line arguments by '--'
    unittest_args, _parser_args = sliceCommandLineArguments()
    # Construct and run unit tests.
    unittest.main(argv=unittest_args)


if __name__ == "__main__":
    main()
