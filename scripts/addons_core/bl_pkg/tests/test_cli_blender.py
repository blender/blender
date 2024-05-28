# SPDX-FileCopyrightText: 2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
This test emulates running packaging commands with Blender via the command line.

This also happens to test packages with ``*.whl``.

Command to run this test:
   make test_cli_blender BLENDER_BIN=$PWD/../../../blender.bin
"""

import os
import shlex
import shutil
import subprocess
import sys
import tempfile
import time
import unittest

from typing import (
    Dict,
    Sequence,
    Tuple,
)


PKG_MANIFEST_FILENAME_TOML = "blender_manifest.toml"

VERBOSE_CMD = False


BLENDER_BIN = os.environ.get("BLENDER_BIN")
if BLENDER_BIN is None:
    raise Exception("BLENDER_BIN: environment variable not defined")


# Arguments to ensure extensions are enabled (currently it's an experimental feature).
BLENDER_ENABLE_EXTENSION_ARGS = [
    "--online-mode",
    "--python-exit-code", "1",
]

BASE_DIR = os.path.abspath(os.path.dirname(__file__))
sys.path.append(os.path.join(BASE_DIR, "modules"))
import python_wheel_generate  # noqa: E402


# Write the command to a script, use so it's possible to manually run commands outside of the test environment.
TEMP_COMMAND_OUTPUT = ""  # os.path.join(tempfile.gettempdir(), "blender_test.sh")

# Handy when developing test so the paths can be manually inspected.
USE_PAUSE_BEFORE_EXIT = False


# -----------------------------------------------------------------------------
# Utility Functions

def path_to_url(path: str) -> str:
    from urllib.parse import urljoin
    from urllib.request import pathname2url
    return urljoin('file:', pathname2url(path))


def pause_until_keyboard_interrupt() -> None:
    print("Waiting for keyboard interrupt...")
    try:
        time.sleep(10_000)
    except KeyboardInterrupt:
        pass
    print("Exiting!")


def contents_to_filesystem(
        contents: Dict[str, bytes],
        directory: str,
) -> None:
    swap_slash = os.sep == "\\"
    for key, value in contents.items():
        path = key.replace("/", "\\") if swap_slash else key
        path_full = os.path.join(directory, path)
        path_base = os.path.dirname(path_full)
        os.makedirs(path_base, exist_ok=True)

        with (
                open(path_full, "wb") if isinstance(value, bytes) else
                open(path_full, "w", encoding="utf-8")
        ) as fh:
            fh.write(value)


def create_package(
        pkg_src_dir: str,
        pkg_idname: str,
        wheel_module_name: str,
        wheel_module_version: str,
) -> None:
    pkg_name = pkg_idname.replace("_", " ").title()

    wheel_filename, wheel_filedata = python_wheel_generate.generate_from_source(
        module_name=wheel_module_name,
        version=wheel_module_version,
        source=(
            "__version__ = {!r}\n"
            "print(\"The wheel has been found\")\n"
        ).format(wheel_module_version),
    )

    wheel_dir = os.path.join(pkg_src_dir, "wheels")
    os.makedirs(wheel_dir, exist_ok=True)
    path = os.path.join(wheel_dir, wheel_filename)
    with open(path, "wb") as fh:
        fh.write(wheel_filedata)

    with open(os.path.join(pkg_src_dir, PKG_MANIFEST_FILENAME_TOML), "w", encoding="utf-8") as fh:
        fh.write('''# Example\n''')
        fh.write('''schema_version = "1.0.0"\n''')
        fh.write('''id = "{:s}"\n'''.format(pkg_idname))
        fh.write('''name = "{:s}"\n'''.format(pkg_name))
        fh.write('''type = "add-on"\n''')
        fh.write('''tags = ["UV"]\n''')
        fh.write('''maintainer = "Maintainer Name <username@addr.com>"\n''')
        fh.write('''license = ["SPDX:GPL-2.0-or-later"]\n''')
        fh.write('''version = "1.0.0"\n''')
        fh.write('''tagline = "This is a tagline"\n''')
        fh.write('''blender_version_min = "0.0.0"\n''')
        fh.write('''\n''')
        fh.write('''wheels = ["./wheels/{:s}"]\n'''.format(wheel_filename))

    with open(os.path.join(pkg_src_dir, "__init__.py"), "w", encoding="utf-8") as fh:
        fh.write((
            '''import {:s}\n'''
            '''def register():\n'''
            '''    print("Register success:", __name__)\n'''
            '''\n'''
            '''def unregister():\n'''
            '''    print("Unregister success:", __name__)\n'''
        ).format(wheel_module_name))


def run_blender(
        args: Sequence[str],
        force_script_and_pause: bool = False,
) -> Tuple[int, str, str]:
    """
    :arg force_script_and_pause:
       When true, write out a shell script and wait,
       this lets the developer run the command manually which is useful as the temporary directories
       are removed once the test finished.
    """
    assert BLENDER_BIN is not None
    cmd: Tuple[str, ...] = (
        BLENDER_BIN,
        # Needed while extensions is experimental.
        *BLENDER_ENABLE_EXTENSION_ARGS,
        *args,
    )
    cwd = TEMP_DIR_LOCAL

    if VERBOSE_CMD:
        print(shlex.join(cmd))

    env_overlay = {
        "TMPDIR": TEMP_DIR_TMPDIR,
        "BLENDER_USER_RESOURCES": TEMP_DIR_BLENDER_USER,
        # Needed for ASAN builds.
        "ASAN_OPTIONS": "log_path={:s}:exitcode=0:{:s}".format(
            # Needed so the `stdout` & `stderr` aren't mixed in with ASAN messages.
            os.path.join(TEMP_DIR_TMPDIR, "blender_asan.txt"),
            # Support using existing configuration (if set).
            os.environ.get("ASAN_OPTIONS", ""),
        ),
    }

    if force_script_and_pause:
        temp_command_output = os.path.join(tempfile.gettempdir(), "blender_test.sh")
    else:
        temp_command_output = TEMP_COMMAND_OUTPUT

    if temp_command_output:
        with open(temp_command_output, "w", encoding="utf-8") as fh:
            fh.write("#!/usr/bin/env bash\n")
            for k, v in env_overlay.items():
                fh.write("export {:s}={:s}\n".format(k, shlex.quote(v)))
            fh.write("\n")

            fh.write("cd {:s}\n\n".format(shlex.quote(cwd)))

            for i, v in enumerate(cmd):
                if i != 0:
                    fh.write("  ")
                fh.write(shlex.quote(v))
                if i + 1 != len(cmd):
                    fh.write(" \\\n")
            fh.write("\n\n")

        if force_script_and_pause:
            print("Written:", temp_command_output)
            time.sleep(10_000)

    output = subprocess.run(
        cmd,
        cwd=cwd,
        env={
            **os.environ,
            **env_overlay,
        },
        stderr=subprocess.PIPE,
        stdout=subprocess.PIPE,
    )
    stdout = output.stdout.decode("utf-8")
    stderr = output.stderr.decode("utf-8")

    if VERBOSE_CMD:
        print(stdout)
        print(stderr)

    return (
        output.returncode,
        stdout,
        stderr,
    )


def run_blender_no_errors(
        args: Sequence[str],
        force_script_and_pause: bool = False,
) -> str:
    returncode, stdout, stderr = run_blender(args, force_script_and_pause=force_script_and_pause)
    if returncode != 0:
        if stdout:
            sys.stdout.write("STDOUT:\n")
            sys.stdout.write(stdout + "\n")
        if stderr:
            sys.stdout.write("STDERR:\n")
            sys.stdout.write(stderr + "\n")
        raise Exception("Expected zero returncode, got {:d}".format(returncode))
    if stderr:
        raise Exception("Expected empty stderr, got {:s}".format(stderr))
    return stdout


def run_blender_extensions(
        args: Sequence[str],
        force_script_and_pause: bool = False,
) -> Tuple[int, str, str]:
    return run_blender(("--command", "extension", *args,), force_script_and_pause=force_script_and_pause)


def run_blender_extensions_no_errors(
        args: Sequence[str],
        force_script_and_pause: bool = False,
) -> str:
    return run_blender_no_errors(("--command", "extension", *args,), force_script_and_pause=force_script_and_pause)


# Initialized from `main()`.
TEMP_DIR_BLENDER_USER = ""
TEMP_DIR_REMOTE = ""
TEMP_DIR_REMOTE_AS_URL = ""
TEMP_DIR_LOCAL = ""
# Don't leave temporary files in TMP: `/tmp` (since it's only cleared on restart).
# Instead, have a test-local temporary directly which is removed when the test finishes.
TEMP_DIR_TMPDIR = ""

user_dirs: Tuple[str, ...] = (
    "config",
    "datafiles",
    "extensions",
    "scripts",
)


class TestWithTempBlenderUser_MixIn(unittest.TestCase):

    @classmethod
    def setUpClass(cls) -> None:
        for dirname in user_dirs:
            os.makedirs(os.path.join(TEMP_DIR_BLENDER_USER, dirname), exist_ok=True)

    @classmethod
    def tearDownClass(cls) -> None:
        for dirname in user_dirs:
            shutil.rmtree(os.path.join(TEMP_DIR_BLENDER_USER, dirname))


class TestSimple(TestWithTempBlenderUser_MixIn, unittest.TestCase):

    # Internal utilities.
    def _build_package(
            self,
            *,
            pkg_idname: str,
            wheel_module_name: str,
            wheel_module_version: str,
    ) -> None:
        pkg_output_filepath = os.path.join(TEMP_DIR_REMOTE, pkg_idname + ".zip")
        with tempfile.TemporaryDirectory() as package_build_dir:
            create_package(
                package_build_dir,
                pkg_idname=pkg_idname,
                wheel_module_name=wheel_module_name,
                wheel_module_version=wheel_module_version,
            )
            stdout = run_blender_extensions_no_errors((
                "build",
                "--source-dir", package_build_dir,
                "--output-filepath", pkg_output_filepath,
            ))
            self.assertEqual(
                stdout,
                (
                    "Building {:s}.zip\n"
                    "complete\n"
                    "created \"{:s}\", {:d}\n"
                ).format(pkg_idname, pkg_output_filepath, os.path.getsize(pkg_output_filepath)),
            )

    def test_simple_package(self) -> None:
        """
        Create a simple package and install it.
        """

        repo_id = "test_repo_module_name"

        stdout = run_blender_extensions_no_errors((
            "repo-add",
            "--name", "MyTestRepo",
            "--directory", TEMP_DIR_LOCAL,
            "--url", TEMP_DIR_REMOTE_AS_URL,
            # A bit odd, this argument avoids running so many commands to setup a test.
            "--clear-all",
            repo_id,
        ))
        self.assertEqual(stdout, "Info: Preferences saved\n")

        wheel_module_name = "my_custom_wheel"

        # Create a package contents.
        pkg_idname = "my_test_pkg"
        self._build_package(
            pkg_idname=pkg_idname,
            wheel_module_name=wheel_module_name,
            wheel_module_version="1.0.1",
        )

        # Generate the repository.
        stdout = run_blender_extensions_no_errors((
            "server-generate",
            "--repo-dir", TEMP_DIR_REMOTE,
        ))
        self.assertEqual(stdout, "found 1 packages.\n")

        stdout = run_blender_extensions_no_errors((
            "sync",
        ))
        self.assertEqual(
            stdout.rstrip("\n").split("\n")[-1],
            "STATUS Sync complete: {:s}".format(TEMP_DIR_REMOTE_AS_URL),
        )

        # Install the package into Blender.

        stdout = run_blender_extensions_no_errors(("repo-list",))
        self.assertEqual(
            stdout,
            (
                '''test_repo_module_name:\n'''
                '''    name: "MyTestRepo"\n'''
                '''    directory: "{:s}"\n'''
                '''    url: "{:s}"\n'''
            ).format(TEMP_DIR_LOCAL, TEMP_DIR_REMOTE_AS_URL))

        stdout = run_blender_extensions_no_errors(("list",))
        self.assertEqual(
            stdout,
            (
                '''Repository: "MyTestRepo" (id=test_repo_module_name)\n'''
                '''  my_test_pkg: "My Test Pkg", This is a tagline\n'''
            )
        )

        stdout = run_blender_extensions_no_errors(("install", pkg_idname, "--enable"))
        self.assertEqual(
            [line for line in stdout.split("\n") if line.startswith("STATUS ")][0],
            "STATUS Installed \"my_test_pkg\""
        )

        # TODO: validate the installation works - that the package does something non-trivial when Blender starts.

        stdout = run_blender_extensions_no_errors(("remove", pkg_idname))
        self.assertEqual(
            [line for line in stdout.split("\n") if line.startswith("STATUS ")][0],
            "STATUS Removed \"my_test_pkg\""
        )

        returncode, _, _ = run_blender((
            "-b",
            "--python-expr",
            # Return an `exitcode` of 64 if the module exists.
            # The module should not exist (and return a zero error code).
            (
                '''import sys\n'''
                '''try:\n'''
                '''    import {:s}\n'''
                '''    code = 32\n'''
                '''except ModuleNotFoundError:\n'''
                '''    code = 64\n'''
                '''sys.exit(code)\n'''
            ).format(wheel_module_name)
        ))
        self.assertEqual(returncode, 64)

        # Ensure packages that including conflicting dependencies use the newest wheel.
        packages_to_install = ["my_test_pkg"]
        # This is the maximum wheel version.
        packages_wheel_version_max = "4.0.1"
        # Create a package contents (with a different wheel version).
        for pkg_idname, wheel_module_version in (
                ("my_test_pkg_a", "2.0.1"),
                ("my_test_pkg_b", packages_wheel_version_max),
                ("my_test_pkg_c", "3.0.1"),
        ):
            packages_to_install.append(pkg_idname)
            self._build_package(
                pkg_idname=pkg_idname,
                wheel_module_name=wheel_module_name,
                wheel_module_version=wheel_module_version,
            )

        # Generate the repository.
        stdout = run_blender_extensions_no_errors((
            "server-generate",
            "--repo-dir", TEMP_DIR_REMOTE,
        ))
        self.assertEqual(stdout, "found 4 packages.\n")

        stdout = run_blender_extensions_no_errors((
            "sync",
        ))
        self.assertEqual(
            stdout.rstrip("\n").split("\n")[-1],
            "STATUS Sync complete: {:s}".format(TEMP_DIR_REMOTE_AS_URL),
        )

        # Install.

        stdout = run_blender_extensions_no_errors(("install", ",".join(packages_to_install), "--enable"))
        self.assertEqual(
            tuple([line for line in stdout.split("\n") if line.startswith("STATUS ")]),
            (
                '''STATUS Installed "my_test_pkg"''',
                '''STATUS Installed "my_test_pkg_a"''',
                '''STATUS Installed "my_test_pkg_b"''',
                '''STATUS Installed "my_test_pkg_c"''',
            )
        )

        returncode, stdout, stderr = run_blender((
            "-b",
            "--python-expr",
            # Return an `exitcode` of 64 if the module exists.
            # The module should not exist (and return a zero error code).
            (
                '''import sys\n'''
                '''try:\n'''
                '''    import {:s}\n'''
                '''    found = True\n'''
                '''except ModuleNotFoundError:\n'''
                '''    found = False\n'''
                '''if found:\n'''
                '''    if {:s}.__version__ == "{:s}":\n'''
                '''        sys.exit(64)  # Success!\n'''
                '''    else:\n'''
                '''        sys.exit(32)\n'''
                '''else:\n'''
                '''    sys.exit(16)\n'''
            ).format(wheel_module_name, wheel_module_name, packages_wheel_version_max),
        ))

        self.assertEqual(returncode, 64)

        if USE_PAUSE_BEFORE_EXIT:
            print(TEMP_DIR_REMOTE)
            print(TEMP_DIR_BLENDER_USER)
            pause_until_keyboard_interrupt()


def main() -> None:
    global TEMP_DIR_BLENDER_USER
    global TEMP_DIR_REMOTE
    global TEMP_DIR_LOCAL
    global TEMP_DIR_TMPDIR
    global TEMP_DIR_REMOTE_AS_URL

    with tempfile.TemporaryDirectory() as temp_prefix:
        TEMP_DIR_BLENDER_USER = os.path.join(temp_prefix, "bl_ext_blender")
        TEMP_DIR_REMOTE = os.path.join(temp_prefix, "bl_ext_remote")
        TEMP_DIR_LOCAL = os.path.join(temp_prefix, "bl_ext_local")
        TEMP_DIR_TMPDIR = os.path.join(temp_prefix, "tmp")

        for directory in (
                TEMP_DIR_BLENDER_USER,
                TEMP_DIR_REMOTE,
                TEMP_DIR_LOCAL,
                TEMP_DIR_TMPDIR,
        ):
            os.makedirs(directory, exist_ok=True)

        for dirname in user_dirs:
            os.makedirs(os.path.join(TEMP_DIR_BLENDER_USER, dirname), exist_ok=True)

        TEMP_DIR_REMOTE_AS_URL = path_to_url(TEMP_DIR_REMOTE)

        unittest.main()


if __name__ == "__main__":
    main()
