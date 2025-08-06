# SPDX-FileCopyrightText: 2025 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# ./blender.bin --background --factory-startup --python tests/python/bl_pyapi_bpy_app_tempdir.py -- --verbose

# NOTE(ideasman42):
#
# - Creating a directory without write permissions across all supported platforms
#   is it not trivial, for the purpose of these tests simply create a file as a way of
#   pointing to a temporary path which can't have sub-directories created under it.
#
# - Internally, when no temporary path is set the hard coded path `/tmp/` is used.
#   For the purpose of the tests being complete - control over this path would be needed too.
#
# - When changing the temp directory & on exit
#   Blender's *session* temp directory is recursively removed.
#
#   These tests were added as part of a fix for a serious flaw (see #144042)
#   which would recursively delete the users `C:\` since the tests aren't sand-boxed
#   all of the following tests reference paths within a newly creating temporary directory
#   instead of referencing root paths to avoid risks for developers who run tests.
#
# - A related test exists `BLI_tempfile_test.cc` however this doesn't deal with
#   Blender's user preferences and fallbacks used in Blender.
#

__all__ = (
    "main",
)

import os
import unittest
import tempfile


is_win32 = os.name == "nt"
TEMP_ENV = "TEMP" if is_win32 else "TMPDIR"


def system_temp_set(path: str) -> None:
    os.environ[TEMP_ENV] = path


def prefs_temp_set(path: str) -> None:
    import bpy  # type: ignore
    bpy.context.preferences.filepaths.temporary_directory = path


def prefs_temp_get() -> str:
    import bpy
    result = bpy.context.preferences.filepaths.temporary_directory
    assert isinstance(result, str)
    return result


def blender_tempdir_session_get() -> str:
    import bpy
    result = bpy.app.tempdir
    assert isinstance(result, str)
    return result


def empty_file(path: str) -> None:
    with open(path, 'wb') as _fh:
        pass


def commonpath_safe(paths: list[str]) -> str:
    if is_win32:
        try:
            return os.path.commonpath(paths)
        except ValueError:
            # Workaround error on Windows.
            # ValueError: Paths don't have the same drive
            return ""

    return os.path.commonpath(paths)


class TestTempDir(unittest.TestCase):

    def setUp(self) -> None:
        print(prefs_temp_get())
        assert prefs_temp_get() == ""
        system_temp_set("")
        prefs_temp_set("")

    def tearDown(self) -> None:
        prefs_temp_set("")
        system_temp_set("")

    def test_fallback(self) -> None:
        # Set a file for preferences & system temp, ensure neither are used.
        with tempfile.TemporaryDirectory() as tempdir:

            temp_sys = os.path.join(tempdir, "a_sys")
            temp_bpy = os.path.join(tempdir, "b_bpy")

            empty_file(temp_sys)
            empty_file(temp_bpy)

            system_temp_set(temp_sys)
            prefs_temp_set(temp_bpy)

            temp_session = blender_tempdir_session_get()

            # Ensure neither are used.
            # This will try to use `/tmp/`.
            commonpath_test = commonpath_safe([temp_sys, temp_session])
            self.assertFalse(commonpath_test and os.path.samefile(temp_sys, commonpath_test))
            commonpath_test = commonpath_safe([temp_bpy, temp_session])
            self.assertFalse(commonpath_test and os.path.samefile(temp_bpy, commonpath_test))

    def test_system(self) -> None:
        # Set an file as the preferences temp directory, ensure the system path is used.
        with tempfile.TemporaryDirectory() as tempdir:

            temp_sys = os.path.join(tempdir, "a_sys")
            temp_bpy = os.path.join(tempdir, "b_bpy")

            os.mkdir(temp_sys)
            empty_file(temp_bpy)

            system_temp_set(temp_sys)
            prefs_temp_set(temp_bpy)

            temp_session = blender_tempdir_session_get()

            self.assertTrue(os.path.samefile(temp_sys, os.path.commonpath([temp_sys, temp_session])))

    def test_prefs(self) -> None:
        # Set an file as the system temp directory, ensure the preferences path is used.
        with tempfile.TemporaryDirectory() as tempdir:

            temp_sys = os.path.join(tempdir, "a_sys")
            temp_bpy = os.path.join(tempdir, "b_bpy")

            empty_file(temp_sys)
            os.mkdir(temp_bpy)

            system_temp_set(temp_sys)
            prefs_temp_set(temp_bpy)

            temp_session = blender_tempdir_session_get()

            self.assertTrue(os.path.samefile(temp_bpy, os.path.commonpath([temp_bpy, temp_session])))

    def test_system_to_prefs(self) -> None:
        with tempfile.TemporaryDirectory() as tempdir:
            temp_sys = os.path.join(tempdir, "a_sys")
            temp_bpy = os.path.join(tempdir, "b_bpy")

            os.mkdir(temp_sys)
            empty_file(temp_bpy)

            system_temp_set(temp_sys)
            prefs_temp_set(temp_bpy)

            temp_session = blender_tempdir_session_get()
            self.assertTrue(os.path.samefile(temp_sys, os.path.commonpath([temp_sys, temp_session])))

            # Now set the preferences and ensure the previous directory gets purged and the new one set.
            os.unlink(temp_bpy)
            os.mkdir(temp_bpy)

            # Ensure the preferences path is now used.
            prefs_temp_set(temp_bpy)

            self.assertFalse(os.path.exists(temp_session))
            temp_session = blender_tempdir_session_get()
            self.assertTrue(os.path.samefile(temp_bpy, os.path.commonpath([temp_bpy, temp_session])))

    def test_prefs_to_system(self) -> None:
        with tempfile.TemporaryDirectory() as tempdir:
            temp_sys = os.path.join(tempdir, "a_sys")
            temp_bpy = os.path.join(tempdir, "b_bpy")

            empty_file(temp_sys)
            os.mkdir(temp_bpy)

            system_temp_set(temp_sys)
            prefs_temp_set(temp_bpy)

            temp_session = blender_tempdir_session_get()
            self.assertTrue(os.path.samefile(temp_bpy, os.path.commonpath([temp_bpy, temp_session])))

            # Now set the preferences and ensure the previous directory gets purged and the new one set.
            os.unlink(temp_sys)
            os.mkdir(temp_sys)

            # Ensure the system path is now used.
            prefs_temp_set(temp_sys)

            self.assertFalse(os.path.exists(temp_session))
            temp_session = blender_tempdir_session_get()
            self.assertTrue(os.path.samefile(temp_sys, os.path.commonpath([temp_sys, temp_session])))


def main():
    import sys
    unittest.main(argv=[__file__] + (sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []))


if __name__ == "__main__":
    main()
