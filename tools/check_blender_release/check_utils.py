#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later


import unittest


def sliceCommandLineArguments():
    """
    Slice command line arguments by -- argument.
    """

    import sys

    try:
        double_shasl_index = sys.argv.index("--")
    except ValueError:
        unittest_args = sys.argv[:]
        parser_args = []
    else:
        unittest_args = sys.argv[:double_shasl_index]
        parser_args = sys.argv[double_shasl_index + 1:]

    return unittest_args, parser_args


def parseArguments():
    import argparse

    # Construct argument parser.
    parser = argparse.ArgumentParser(description="Static binary checker")
    parser.add_argument('directory', help='Directories to check')
    # Parse arguments which are not handled by unit testing framework.
    unittest_args, parser_args = sliceCommandLineArguments()
    args = parser.parse_args(args=parser_args)
    # TODO(sergey): Run some checks here?
    return args


def runScriptInBlender(blender_directory, script):
    """
    Run given script inside Blender and check non-zero exit code
    """

    import os
    import subprocess

    blender = os.path.join(blender_directory, "blender")
    python = os.path.join(os.path.dirname(__file__), "scripts", script) + ".py"

    command = (blender,
               "-b",
               "--factory-startup",
               "--python-exit-code", "1",
               "--python", python)

    process = subprocess.Popen(command,
                               shell=False,
                               stdout=subprocess.PIPE,
                               stderr=subprocess.STDOUT)
    output, error = process.communicate()
    return process.returncode == 0


class SceiptUnitTesting(unittest.TestCase):
    def checkScript(self, script):
        # Parse arguments which are not handled by unit testing framework.
        args = parseArguments()
        # Perform actual test,
        self.assertTrue(runScriptInBlender(args.directory, script),
                        "Failed to run script {}" . format(script))
