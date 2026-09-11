#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
Test the "Trusted Source" state when opening blend-files from the command line.

This test runs outside of Blender. Blender is run to create the blend-files &
preferences, then run again to open them with different command line arguments,
checking ``bpy.app.autoexec`` after each file is open.

Command to run this test directly:
   python3 tests/python/bl_blend_autoexec_cli.py --blender ./blender.bin

Command to run a single test:
   python3 tests/python/bl_blend_autoexec_cli.py --blender ./blender.bin \
       TestAutoExecCLI.test_pref_enabled_untrusted
"""

import argparse
import json
import os
import shlex
import subprocess
import sys
import tempfile
import unittest

BASE_TRUSTED = "trusted_dir"
BASE_UNTRUSTED = "untrusted_dir"
BASE_UNTRUSTED_SUB = "sub_dir"

BLEND_TRUSTED = "trusted_file.blend"
BLEND_TRUSTED_OTHER = "trusted_other_file.blend"
BLEND_UNTRUSTED = "untrusted_file.blend"
BLEND_UNTRUSTED_SUB = "untrusted_sub_file.blend"

# A registered text block, run when the blend-file is loaded with scripts trusted.
TEXT_SCRIPT = "script_maybe_executes.py"
SCENE_SCRIPT_RAN = "Scene [modified by script]"
TEXT_SCRIPT_BODY = (
    "import bpy\n"
    "bpy.data.scenes[0].name = {!r}\n"
).format(SCENE_SCRIPT_RAN)

# The state is printed on a single line with this prefix so it can be found in Blender's output.
STATE_PREFIX = "AUTOEXEC_STATE:"

# Preference configurations, each saved by a setup run
# into its own `BLENDER_USER_RESOURCES` directory.
CONFIG_AUTOEXEC_ON = "autoexec_on"
CONFIG_AUTOEXEC_OFF = "autoexec_off"
CONFIG_AUTOEXEC_ON_GLOB = "autoexec_on_glob"

# The options each configuration is created with: (use_autoexec, use_glob).
CONFIGS = {
    CONFIG_AUTOEXEC_ON: (True, False),
    CONFIG_AUTOEXEC_OFF: (False, False),
    CONFIG_AUTOEXEC_ON_GLOB: (True, True),
}

# Blender runs this file as a script & imports it, see `main_blender` & `STATE_EXPR`.
FILEPATH_SELF = os.path.abspath(__file__)

# The Blender binary to test, set from `--blender`.
BLENDER_BIN = ""

# NOTE: reporting after each of several files needs an expression, not "--python",
# since the "--" arguments a script reads would consume the files which follow it.
STATE_EXPR = (
    "import sys; "
    "sys.path.append({:s}); "
    "from {:s} import blender_state_print; "
    "blender_state_print()"
).format(
    repr(os.path.dirname(FILEPATH_SELF)),
    os.path.splitext(os.path.basename(FILEPATH_SELF))[0],
)


# -----------------------------------------------------------------------------
# Utilities

def output_as_text(output):
    return output.decode("utf-8", errors="replace")


def cmd_as_text(cmd):
    """
    Return the command as text, so a failing run can be repeated by hand.
    """
    return " ".join(shlex.quote(arg) for arg in cmd)


def blend_filepaths(dirpath_temp):
    """
    Return the blend-files in ``dirpath_temp``.
    """
    return {
        BLEND_TRUSTED: os.path.join(dirpath_temp, BASE_TRUSTED, BLEND_TRUSTED),
        BLEND_TRUSTED_OTHER: os.path.join(dirpath_temp, BASE_TRUSTED, BLEND_TRUSTED_OTHER),
        BLEND_UNTRUSTED: os.path.join(dirpath_temp, BASE_UNTRUSTED, BLEND_UNTRUSTED),
        BLEND_UNTRUSTED_SUB: os.path.join(
            dirpath_temp, BASE_UNTRUSTED, BASE_UNTRUSTED_SUB, BLEND_UNTRUSTED_SUB),
    }


def dirpath_resources(dirpath_temp, config):
    """
    Return the ``BLENDER_USER_RESOURCES`` directory for ``config``.
    """
    return os.path.join(dirpath_temp, "resources_" + config)


# -----------------------------------------------------------------------------
# Run Inside Blender
#
# NOTE: these run inside Blender. `blender_setup` is invoked as `--python <this file>`,
# `blender_state_print` that way too & by importing this module from `--python-expr`,
# see `STATE_EXPR` (so renaming this file changes the import it builds).
# Splitting them into their own script means duplicating the constants & state
# they share with the tests or moving them into a third module. The paths, the
# scene name & the output prefix must agree exactly for the tests to prove
# anything, so keep them here while there are only two modes.

def blender_setup(dirpath_temp, config):
    """
    Save the blend-files in ``dirpath_temp`` & the preferences for ``config``.
    """
    import bpy

    use_autoexec, use_glob = CONFIGS[config]

    # Saving writes the preferences, an empty directory proves they land in the
    # temporary `BLENDER_USER_RESOURCES`, never the user's own configuration.
    dirpath = bpy.utils.user_resource('CONFIG', create=True)
    if os.listdir(dirpath):
        raise Exception("Expected an empty user configuration in {!r}".format(dirpath))

    prefs = bpy.context.preferences
    prefs.use_preferences_save = False
    prefs.filepaths.use_scripts_auto_execute = use_autoexec
    prefs.filepaths.file_preview_type = 'NONE'

    # The "+" button in the preferences creates an empty entry, it must never match.
    prefs.autoexec_paths.new()

    path_cmp = prefs.autoexec_paths.new()
    if use_glob:
        path_cmp.use_glob = True
        # No path separator, so the same pattern matches on all platforms.
        path_cmp.path = "*" + BASE_UNTRUSTED + "*"
    else:
        path_cmp.path = os.path.join(dirpath_temp, BASE_UNTRUSTED) + os.sep

    # Saved into every file, running it renames the scene so it can be detected.
    text = bpy.data.texts.new(TEXT_SCRIPT)
    text.write(TEXT_SCRIPT_BODY)
    text.use_module = True

    for filepath in blend_filepaths(dirpath_temp).values():
        os.makedirs(os.path.dirname(filepath), exist_ok=True)
        bpy.ops.wm.save_as_mainfile(filepath=filepath)

    bpy.ops.wm.save_userpref()

    # The tests are meaningless when the preferences weren't written.
    if not os.listdir(dirpath):
        raise Exception("Preferences were not written into {!r}".format(dirpath))


def blender_state_print():
    """
    Print the state of the blend-file which was opened.
    """
    import bpy

    filepath = bpy.data.filepath
    sys.stdout.write("{:s}{:s}\n".format(STATE_PREFIX, json.dumps({
        "filepath": filepath,
        "autoexec": bpy.app.autoexec,
        "autoexec_fail": bpy.app.autoexec_fail,
        "autoexec_fail_message": bpy.app.autoexec_fail_message,
        "scene_name": bpy.data.scenes[0].name,
    })))
    # Flush so the line isn't held in the buffer while Blender's own output is written.
    sys.stdout.flush()


def main_blender(argv):
    mode, *args = argv
    if mode == "setup":
        blender_setup(*args)
    elif mode == "state":
        blender_state_print()
    else:
        raise Exception("Unknown mode {!r}".format(mode))


# -----------------------------------------------------------------------------
# Tests

class TestAutoExecCLI(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        cls.dirpath_temp_context = tempfile.TemporaryDirectory(prefix="bl_blend_autoexec_cli_")
        # Clean up even if the setup below raises, `tearDownClass` isn't called then.
        cls.addClassCleanup(cls.dirpath_temp_context.cleanup)
        # Resolve symbolic links, Blender canonicalizes paths lexically while the OS
        # resolves the working directory, see `test_pref_enabled_untrusted_relative`.
        cls.dirpath_temp = os.path.realpath(cls.dirpath_temp_context.name)

        for config in CONFIGS:
            os.makedirs(dirpath_resources(cls.dirpath_temp, config))
            proc = cls.blender_run(
                [
                    "--factory-startup",
                    "--python", FILEPATH_SELF,
                    "--",
                    "setup", cls.dirpath_temp, config,
                ],
                config=config,
            )
            if proc.returncode != 0:
                raise Exception("Setup failed: {:s}\n{:s}".format(cmd_as_text(proc.args), output_as_text(proc.stdout)))

    @classmethod
    def blender_run(cls, args, *, config, cwd=None):
        """
        Run Blender with the preferences for ``config``.

        The user configuration is isolated, the users own preferences are never
        read or written.
        """
        # NOTE: "--factory-startup" is deliberately not one of the shared arguments,
        # the preferences the setup run saved must load. Only setup runs pass it.
        cmd = [
            BLENDER_BIN,
            "--background",
            "--console-crash-handler",
            "--debug-memory",
            "--debug-exit-on-error",
            "--python-exit-code", "1",
            *args,
        ]
        env = {
            **os.environ,
            "BLENDER_USER_RESOURCES": dirpath_resources(cls.dirpath_temp, config),
        }
        # These take precedence over `BLENDER_USER_RESOURCES` so the user's own
        # configuration would be used, see `BKE_appdir_folder_id_ex`.
        for varname in (
                "BLENDER_USER_DATAFILES",
                "BLENDER_USER_CONFIG",
                "BLENDER_USER_SCRIPTS",
                "BLENDER_USER_EXTENSIONS",
        ):
            env.pop(varname, None)
        return subprocess.run(
            cmd,
            env=env,
            cwd=cwd,
            stdin=subprocess.DEVNULL,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
        )

    def blend_filepath(self, filename):
        return blend_filepaths(self.dirpath_temp)[filename]

    def blend_file_states(self, args, *, config, cwd=None):
        """
        Run Blender, returning each state reported in its output
        & a message naming the command, reported by every failure so it can be repeated by hand.

        Callers must pair a reporting argument with each file they open.
        """
        proc = self.blender_run(args, config=config, cwd=cwd)
        output = output_as_text(proc.stdout)
        message = "Command: {:s}".format(cmd_as_text(proc.args))

        self.assertEqual(proc.returncode, 0, "{:s}\n{:s}".format(message, output))

        states = [
            json.loads(line[len(STATE_PREFIX):])
            for line in output.splitlines()
            if line.startswith(STATE_PREFIX)
        ]
        if not states:
            self.fail("Auto-execution state not found in: {:s}\n{:s}".format(message, output))
        return states, message

    def assert_blend_file_state(self, state, filename, *, is_trusted, message):
        """
        Check ``filename`` opened with the expected trust
        & that its registered text block ran (or didn't).
        """
        self.assertEqual(os.path.basename(state["filepath"]), filename, message)
        self.assertEqual(state["autoexec"], is_trusted, message)
        # The file's registered text block only runs when the file is trusted.
        self.assertEqual(state["scene_name"] == SCENE_SCRIPT_RAN, is_trusted, message)
        self.assertEqual(state["autoexec_fail"], not is_trusted, message)
        if is_trusted:
            self.assertEqual(state["autoexec_fail_message"], "", message)
        else:
            # The message is translated, only the quoted text block name is stable.
            self.assertIn("'{:s}'".format(TEXT_SCRIPT), state["autoexec_fail_message"], message)

    def assert_blend_file_open(self, args, filename, *, config, is_trusted, cwd=None):
        """
        Open a blend-file from the command line, checking ``filename`` opened with the
        expected trust & that its registered text block ran (or didn't).

        The arguments which print the state come last, so they run after the file is opened.
        """
        states, message = self.blend_file_states(
            [*args, "--python", FILEPATH_SELF, "--", "state"],
            config=config,
            cwd=cwd,
        )
        self.assertEqual(len(states), 1, message)
        self.assert_blend_file_state(states[0], filename, is_trusted=is_trusted, message=message)

    # -------------------------------------------------------------------------
    # Tests with Auto-Execution Preference "Enabled"

    def test_pref_enabled_trusted(self):
        """
        Open a trusted file, scripts must run.

        Proves a path which isn't excluded is trusted without being asked.
        """
        self.assert_blend_file_open(
            [self.blend_filepath(BLEND_TRUSTED)],
            BLEND_TRUSTED,
            config=CONFIG_AUTOEXEC_ON,
            is_trusted=True,
        )

    def test_pref_enabled_untrusted(self):
        """
        Open an excluded file, scripts must not run.

        Proves the excluded paths apply when opening from the command line.
        """
        self.assert_blend_file_open(
            [self.blend_filepath(BLEND_UNTRUSTED)],
            BLEND_UNTRUSTED,
            config=CONFIG_AUTOEXEC_ON,
            is_trusted=False,
        )

    def test_pref_enabled_untrusted_sub_dir(self):
        """
        Open a file below an excluded directory, scripts must not run.

        Proves excluding a directory excludes everything under it.
        """
        self.assert_blend_file_open(
            [self.blend_filepath(BLEND_UNTRUSTED_SUB)],
            BLEND_UNTRUSTED_SUB,
            config=CONFIG_AUTOEXEC_ON,
            is_trusted=False,
        )

    def test_pref_enabled_untrusted_relative(self):
        """
        Open an excluded file by a relative path, scripts must not run.

        Proves the path is made absolute before the excluded paths are checked.
        """
        self.assert_blend_file_open(
            [BLEND_UNTRUSTED],
            BLEND_UNTRUSTED,
            config=CONFIG_AUTOEXEC_ON,
            is_trusted=False,
            cwd=os.path.dirname(self.blend_filepath(BLEND_UNTRUSTED)),
        )

    def test_pref_enabled_untrusted_path_escape(self):
        """
        Open an excluded file by a path which steps into it, scripts must not run.

        Proves the path is normalized before matching, an excluded path can't be spelled around.
        """
        filepath = os.path.join(self.dirpath_temp, BASE_TRUSTED, "..", BASE_UNTRUSTED, BLEND_UNTRUSTED)
        self.assert_blend_file_open(
            [filepath],
            BLEND_UNTRUSTED,
            config=CONFIG_AUTOEXEC_ON,
            is_trusted=False,
        )

    def test_pref_enabled_untrusted_glob(self):
        """
        Open a file excluded by a glob, scripts must not run.

        Proves excluded paths with wild-cards are matched.
        """
        self.assert_blend_file_open(
            [self.blend_filepath(BLEND_UNTRUSTED)],
            BLEND_UNTRUSTED,
            config=CONFIG_AUTOEXEC_ON_GLOB,
            is_trusted=False,
        )

    def test_pref_enabled_trusted_glob(self):
        """
        Open a trusted file while a glob excludes another directory, scripts must run.

        Proves the glob only matches the directory it's meant to.
        """
        self.assert_blend_file_open(
            [self.blend_filepath(BLEND_TRUSTED)],
            BLEND_TRUSTED,
            config=CONFIG_AUTOEXEC_ON_GLOB,
            is_trusted=True,
        )

    def test_pref_enabled_untrusted_enable_autoexec(self):
        """
        Open an excluded file with "--enable-autoexec", scripts must run.

        Proves the command line overrides the excluded paths.
        """
        self.assert_blend_file_open(
            ["--enable-autoexec", self.blend_filepath(BLEND_UNTRUSTED)],
            BLEND_UNTRUSTED,
            config=CONFIG_AUTOEXEC_ON,
            is_trusted=True,
        )

    def test_pref_enabled_untrusted_enable_autoexec_after_filepath(self):
        """
        Open an excluded file with "--enable-autoexec" after the file path, scripts must run.

        Proves the override applies wherever it is on the command line.
        """
        self.assert_blend_file_open(
            [self.blend_filepath(BLEND_UNTRUSTED), "--enable-autoexec"],
            BLEND_UNTRUSTED,
            config=CONFIG_AUTOEXEC_ON,
            is_trusted=True,
        )

    def test_pref_enabled_trusted_disable_autoexec(self):
        """
        Open a trusted file with "--disable-autoexec", scripts must not run.

        Proves the command line overrides the preference.
        """
        self.assert_blend_file_open(
            ["--disable-autoexec", self.blend_filepath(BLEND_TRUSTED)],
            BLEND_TRUSTED,
            config=CONFIG_AUTOEXEC_ON,
            is_trusted=False,
        )

    # -------------------------------------------------------------------------
    # Tests with Auto-Execution Preference "Disabled"

    def test_pref_disabled_trusted(self):
        """
        Open a trusted file, scripts must not run.

        Proves the preference alone stops them, a trusted path rules out the excluded paths.
        """
        self.assert_blend_file_open(
            [self.blend_filepath(BLEND_TRUSTED)],
            BLEND_TRUSTED,
            config=CONFIG_AUTOEXEC_OFF,
            is_trusted=False,
        )

    def test_pref_disabled_trusted_enable_autoexec(self):
        """
        Open a trusted file with "--enable-autoexec", scripts must run.

        Proves the command line overrides the preference.
        """
        self.assert_blend_file_open(
            ["--enable-autoexec", self.blend_filepath(BLEND_TRUSTED)],
            BLEND_TRUSTED,
            config=CONFIG_AUTOEXEC_OFF,
            is_trusted=True,
        )

    def test_pref_disabled_untrusted_enable_autoexec(self):
        """
        Open an excluded file with "--enable-autoexec", auto-exec preference disabled, scripts must run.

        Proves the excluded paths only apply when the preference is enabled.
        """
        self.assert_blend_file_open(
            ["--enable-autoexec", self.blend_filepath(BLEND_UNTRUSTED)],
            BLEND_UNTRUSTED,
            config=CONFIG_AUTOEXEC_OFF,
            is_trusted=True,
        )

    # -------------------------------------------------------------------------
    # Tests Opening Multiple Files

    def test_multiple_files_alternating_trust(self):
        """
        Open four files in one Blender invocation, alternating between trusted & excluded paths,
        reporting the trust after each, so every file is checked as it loads.

        Proves opening an excluded file disables auto-execution for every file opened after it,
        even when their paths are trusted.
        """
        blend_files_with_trust = (
            (BLEND_TRUSTED, True),
            (BLEND_UNTRUSTED, False),
            # NOTE(@ideasman42): Auto-execution remaining disabled for this trusted file could be considered an error.
            # for now test the current behavior.
            (BLEND_TRUSTED_OTHER, False),
            (BLEND_UNTRUSTED_SUB, False),
        )

        args = []
        for filename, _ in blend_files_with_trust:
            args.extend((self.blend_filepath(filename), "--python-expr", STATE_EXPR))

        states, message = self.blend_file_states(args, config=CONFIG_AUTOEXEC_ON)

        for state, (filename, is_trusted) in zip(states, blend_files_with_trust, strict=True):
            self.assert_blend_file_state(state, filename, is_trusted=is_trusted, message=message)


# -----------------------------------------------------------------------------
# Entry Point

def main():
    global BLENDER_BIN

    parser = argparse.ArgumentParser()
    parser.add_argument("--blender", required=True)
    args, remaining = parser.parse_known_args()

    # Make absolute, tests which set the working directory would fail on a relative path.
    BLENDER_BIN = os.path.abspath(args.blender)

    unittest.main(argv=sys.argv[0:1] + remaining)


if __name__ == '__main__':
    if "bpy" in sys.modules:
        # Running inside Blender, its own arguments come before "--".
        main_blender(sys.argv[sys.argv.index("--") + 1:])
    else:
        main()
