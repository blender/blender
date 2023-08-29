#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import unittest

from check_utils import (sliceCommandLineArguments,
                         SceiptUnitTesting)


class UnitTesting(SceiptUnitTesting):
    def test_requestsImports(self):
        self.checkScript("requests_import")

    def test_requestsBasicAccess(self):
        self.checkScript("requests_basic_access")


def main():
    # Slice command line arguments by '--'
    unittest_args, _parser_args = sliceCommandLineArguments()
    # Construct and run unit tests.
    unittest.main(argv=unittest_args)


if __name__ == "__main__":
    main()
