# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# blender -b -P tests/python/bl_pyapi_bpy_app.py -- --verbose

__all__ = (
    "main",
)

import os.path
import sys
import unittest

import bpy


class AppCachedirTest(unittest.TestCase):
    def test_app_cachedir(self) -> None:
        match sys.platform:
            case 'darwin':
                expect = '$HOME/Library/Caches/Blender/'
            case 'win32':
                expect = '%USERPROFILE%\\AppData\\Local\\Blender Foundation\\Blender\\Cache\\'
            case _:  # Linux or other POSIX-ish system.
                expect = '$HOME/.cache/blender/'
        expect = os.path.expandvars(expect)

        self.assertEqual(expect, bpy.app.cachedir)


def main():
    import sys
    sys.argv = [__file__] + (sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])
    unittest.main()


if __name__ == '__main__':
    main()
