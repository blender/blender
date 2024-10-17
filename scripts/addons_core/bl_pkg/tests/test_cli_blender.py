# SPDX-FileCopyrightText: 2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
This test emulates running packaging commands with Blender via the command line.

This also happens to test:
- Packages with ``*.whl``.
- Packages compatibility (mixing supported/unsupported platforms & versions).

Command to run this test:
   make test_cli_blender BLENDER_BIN=$PWD/../../../blender.bin

Command to run this test directly:
   env BLENDER_BIN=$PWD/blender.bin python ./scripts/addons_core/bl_pkg/tests/test_cli_blender.py

Command to run a single test:
   env BLENDER_BIN=$PWD/blender.bin python ./scripts/addons_core/bl_pkg/tests/test_cli_blender.py \
       TestModuleViolation.test_extension_sys_paths
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
    Any,
    NamedTuple,
    Optional,
    Sequence,
)


# For more useful output that isn't clipped.
# pylint: disable-next=protected-access
unittest.util._MAX_LENGTH = 10_000

PKG_EXT = ".zip"

PKG_MANIFEST_FILENAME_TOML = "blender_manifest.toml"

VERBOSE_CMD = False


BLENDER_BIN = os.environ.get("BLENDER_BIN")
if BLENDER_BIN is None:
    raise Exception("BLENDER_BIN: environment variable not defined")

BLENDER_VERSION_STR = subprocess.check_output([BLENDER_BIN, "--version"]).split()[1].decode('ascii')
BLENDER_VERSION: tuple[int, int, int] = tuple(int(x) for x in BLENDER_VERSION_STR.split("."))  # type: ignore
assert len(BLENDER_VERSION) == 3


# Arguments to ensure extensions are enabled (currently it's an experimental feature).
BLENDER_ENABLE_EXTENSION_ARGS = [
    "--online-mode",
    "--python-exit-code", "1",
]

BASE_DIR = os.path.abspath(os.path.dirname(__file__))
sys.path.append(os.path.join(BASE_DIR, "modules"))
import python_wheel_generate  # noqa: E402


# Don't import as module, instead load the class.
def execfile(filepath: str, *, name: str = "__main__") -> dict[str, Any]:
    global_namespace = {"__file__": filepath, "__name__": name}
    with open(filepath, encoding="utf-8") as fh:
        # pylint: disable-next=exec-used
        exec(compile(fh.read(), filepath, 'exec'), global_namespace)
    return global_namespace


_blender_ext = execfile(
    os.path.join(
        BASE_DIR,
        "..",
        "cli",
        "blender_ext.py",
    ),
    name="blender_ext",
)
platform_from_this_system = _blender_ext["platform_from_this_system"]
assert callable(platform_from_this_system)


# Write the command to a script, use so it's possible to manually run commands outside of the test environment.
TEMP_COMMAND_OUTPUT = ""  # os.path.join(tempfile.gettempdir(), "blender_test.sh")

# Handy when developing test so the paths can be manually inspected.
USE_PAUSE_BEFORE_EXIT = False


# -----------------------------------------------------------------------------
# Utility Functions


# Generate different version numbers as strings, used for automatically creating versions
# which are known to be compatible or incompatible with the current version.
def blender_version_relative(version_offset: tuple[int, int, int]) -> str:
    version_new = (
        BLENDER_VERSION[0] + version_offset[0],
        BLENDER_VERSION[1] + version_offset[1],
        BLENDER_VERSION[2] + version_offset[2],
    )
    assert min(*version_new) >= 0
    return "{:d}.{:d}.{:d}".format(*version_new)


def python_script_generate_for_addon(text: str) -> str:
    return (
        '''def register():\n'''
        '''    print("Register success{sep:s}{text:s}:", __name__)\n'''
        '''\n'''
        '''def unregister():\n'''
        '''    print("Unregister success{sep:s}{text:s}:", __name__)\n'''
    ).format(
        sep=" " if text else "",
        text=text,
    )


class WheelModuleParams(NamedTuple):
    module_name: str
    module_version: str


def path_to_url(path: str) -> str:
    from urllib.parse import urljoin
    from urllib.request import pathname2url
    return urljoin('file:', pathname2url(path))


def pause_until_keyboard_interrupt() -> None:
    print("Waiting for keyboard interrupt...")
    try:
        time.sleep(100_000)
    except KeyboardInterrupt:
        pass
    print("Exiting!")


def contents_to_filesystem(
        contents: dict[str, bytes],
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
        *,
        pkg_idname: str,

        # Optional.
        wheel_params: Optional[WheelModuleParams] = None,
        platforms: Optional[tuple[str, ...]] = None,
        blender_version_min: Optional[str] = None,
        blender_version_max: Optional[str] = None,
        python_script: Optional[str] = None,
        file_contents: Optional[dict[str, bytes]] = None,
) -> None:
    pkg_name = pkg_idname.replace("_", " ").title()

    if wheel_params is not None:
        wheel_filename, wheel_filedata = python_wheel_generate.generate_from_source(
            module_name=wheel_params.module_name,
            version=wheel_params.module_version,
            source=(
                "__version__ = {!r}\n"
                "print(\"The wheel has been found\")\n"
            ).format(wheel_params.module_version),
        )

        wheel_dir = os.path.join(pkg_src_dir, "wheels")
        os.makedirs(wheel_dir, exist_ok=True)

        wheel_path = os.path.join(wheel_dir, wheel_filename)
        with open(wheel_path, "wb") as fh:
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
        fh.write('''blender_version_min = "{:s}"\n'''.format(blender_version_min or "0.0.0"))
        if blender_version_min is not None:
            fh.write('''blender_version_max = "{:s}"\n'''.format(blender_version_max))
        fh.write('''\n''')

        if wheel_params is not None:
            fh.write('''wheels = ["./wheels/{:s}"]\n'''.format(wheel_filename))

        if platforms is not None:
            fh.write('''platforms = [{:s}]\n'''.format(", ".join(["\"{:s}\"".format(x) for x in platforms])))

    with open(os.path.join(pkg_src_dir, "__init__.py"), "w", encoding="utf-8") as fh:
        if wheel_params is not None:
            fh.write("import {:s}\n".format(wheel_params.module_name))

        if python_script is not None:
            fh.write(python_script)
        else:
            fh.write(python_script_generate_for_addon(text=""))

    if file_contents is not None:
        contents_to_filesystem(file_contents, pkg_src_dir)


def run_blender(
        args: Sequence[str],
        force_script_and_pause: bool = False,
) -> tuple[int, str, str]:
    """
    :arg force_script_and_pause:
       When true, write out a shell script and wait,
       this lets the developer run the command manually which is useful as the temporary directories
       are removed once the test finished.
    """
    assert BLENDER_BIN is not None
    cmd: tuple[str, ...] = (
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
        # Allow the caller to read a non-zero return-code.
        check=False,
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
) -> tuple[int, str, str]:
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

user_dirs: tuple[str, ...] = (
    "config",
    "datafiles",
    "extensions",
    "scripts",
)


class TestWithTempBlenderUser_MixIn(unittest.TestCase):

    @staticmethod
    def _repo_dirs_create() -> None:
        for dirname in user_dirs:
            os.makedirs(os.path.join(TEMP_DIR_BLENDER_USER, dirname), exist_ok=True)
        os.makedirs(os.path.join(TEMP_DIR_BLENDER_USER, dirname), exist_ok=True)
        os.makedirs(TEMP_DIR_REMOTE, exist_ok=True)
        os.makedirs(TEMP_DIR_LOCAL, exist_ok=True)

    @staticmethod
    def _repo_dirs_destroy() -> None:
        for dirname in user_dirs:
            shutil.rmtree(os.path.join(TEMP_DIR_BLENDER_USER, dirname))
        if os.path.exists(TEMP_DIR_REMOTE):
            shutil.rmtree(TEMP_DIR_REMOTE)
        if os.path.exists(TEMP_DIR_LOCAL):
            shutil.rmtree(TEMP_DIR_LOCAL)

    def setUp(self) -> None:
        self._repo_dirs_create()

    def tearDown(self) -> None:
        self._repo_dirs_destroy()

    def repo_add(self, *, repo_id: str, repo_name: str) -> None:
        stdout = run_blender_extensions_no_errors((
            "repo-add",
            "--name", repo_name,
            "--directory", TEMP_DIR_LOCAL,
            "--url", TEMP_DIR_REMOTE_AS_URL,
            # A bit odd, this argument avoids running so many commands to setup a test.
            "--clear-all",
            repo_id,
        ))
        self.assertEqual(stdout, "")

    def build_package(
            self,
            *,
            pkg_idname: str,
            wheel_params: Optional[WheelModuleParams] = None,

            # Optional.
            pkg_filename: Optional[str] = None,
            platforms: Optional[tuple[str, ...]] = None,
            blender_version_min: Optional[str] = None,
            blender_version_max: Optional[str] = None,
            python_script: Optional[str] = None,
            file_contents: Optional[dict[str, bytes]] = None,
    ) -> None:
        if pkg_filename is None:
            pkg_filename = pkg_idname
        pkg_output_filepath = os.path.join(TEMP_DIR_REMOTE, pkg_filename + PKG_EXT)
        with tempfile.TemporaryDirectory() as package_build_dir:
            create_package(
                package_build_dir,
                pkg_idname=pkg_idname,

                # Optional.
                wheel_params=wheel_params,
                platforms=platforms,
                blender_version_min=blender_version_min,
                blender_version_max=blender_version_max,
                python_script=python_script,
                file_contents=file_contents,
            )
            stdout = run_blender_extensions_no_errors((
                "build",
                "--source-dir", package_build_dir,
                "--output-filepath", pkg_output_filepath,
            ))
            self.assertEqual(
                stdout,
                (
                    "building: {:s}{:s}\n"
                    "complete\n"
                    "created: \"{:s}\", {:d}\n"
                ).format(pkg_filename, PKG_EXT, pkg_output_filepath, os.path.getsize(pkg_output_filepath)),
            )


class TestSimple(TestWithTempBlenderUser_MixIn, unittest.TestCase):

    def test_simple_package(self) -> None:
        """
        Create a simple package and install it.
        """

        repo_id = "test_repo_module_name"
        repo_name = "MyTestRepo"

        self.repo_add(repo_id=repo_id, repo_name=repo_name)

        wheel_module_name = "my_custom_wheel"

        # Create a package contents.
        pkg_idname = "my_test_pkg"
        self.build_package(
            pkg_idname=pkg_idname,
            wheel_params=WheelModuleParams(
                module_name=wheel_module_name,
                module_version="1.0.1",
            ),
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
            "STATUS Extensions list for \"MyTestRepo\" updated",
        )

        # Install the package into Blender.

        stdout = run_blender_extensions_no_errors(("repo-list",))
        self.assertEqual(
            stdout,
            (
                '''{:s}:\n'''
                '''    name: "MyTestRepo"\n'''
                '''    directory: "{:s}"\n'''
                '''    url: "{:s}"\n'''
                '''    access_token: None\n'''
            ).format(repo_id, TEMP_DIR_LOCAL, TEMP_DIR_REMOTE_AS_URL))

        stdout = run_blender_extensions_no_errors(("list",))
        self.assertEqual(
            stdout,
            (
                '''Repository: "MyTestRepo" (id={:s})\n'''
                '''  my_test_pkg: "My Test Pkg", This is a tagline\n'''
            ).format(repo_id)
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
            self.build_package(
                pkg_idname=pkg_idname,
                wheel_params=WheelModuleParams(
                    module_name=wheel_module_name,
                    module_version=wheel_module_version,
                ),
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
            "STATUS Extensions list for \"MyTestRepo\" updated",
        )

        # Install.

        stdout = run_blender_extensions_no_errors(("install", ",".join(packages_to_install), "--enable"))
        # Sort output because the order doesn't matter and may change depending on how jobs are split up.
        self.assertEqual(
            tuple(line for line in sorted(stdout.split("\n")) if line.startswith("STATUS ")),
            (
                '''STATUS Installed "my_test_pkg"''',
                '''STATUS Installed "my_test_pkg_a"''',
                '''STATUS Installed "my_test_pkg_b"''',
                '''STATUS Installed "my_test_pkg_c"''',
            )
        )

        returncode, stdout, _stderr = run_blender((
            "-b",
            "--python-expr",
            # Return an `exitcode` of 64 if the module exists.
            # The module should not exist (and return a zero error code).
            (
                '''import sys\n'''
                '''try:\n'''
                '''    import {wheel_module_name:s}\n'''
                '''    found = True\n'''
                '''except ModuleNotFoundError:\n'''
                '''    found = False\n'''
                '''if found:\n'''
                '''    if {wheel_module_name:s}.__version__ == "{packages_wheel_version_max:s}":\n'''
                '''        sys.exit(64)  # Success!\n'''
                '''    else:\n'''
                '''        sys.exit(32)\n'''
                '''else:\n'''
                '''    sys.exit(16)\n'''
            ).format(
                wheel_module_name=wheel_module_name,
                packages_wheel_version_max=packages_wheel_version_max,
            ),
        ))

        self.assertEqual(returncode, 64)

        if USE_PAUSE_BEFORE_EXIT:
            print(TEMP_DIR_REMOTE)
            print(TEMP_DIR_BLENDER_USER)
            pause_until_keyboard_interrupt()


class TestPlatform(TestWithTempBlenderUser_MixIn, unittest.TestCase):
    def test_platform_filter(self) -> None:
        """
        Check that packages from different platforms are properly filtered.
        """
        platforms_other = ["linux-x64", "macos-arm64", "windows-x64"]

        platform_this = platform_from_this_system()

        if platform_this in platforms_other:
            platforms_other.remove(platform_this)
        else:  # For a predictable length.
            del platforms_other[-1]
        assert len(platforms_other) == 2

        # Create two packages with the same ID, and ensure the package seen by Blender is the one for our platform.

        repo_id = "test_repo_module_name"
        repo_name = "MyTestRepo"

        self.repo_add(repo_id=repo_id, repo_name=repo_name)

        # Create a range of versions, note that only minimum versions beginning
        # with `version_c_this` and higher can be installed with this Blender session.
        version_a = blender_version_relative((-2, 0, 0))
        version_b = blender_version_relative((-1, 0, 0))
        version_c_this = blender_version_relative((0, 0, 0))
        version_d = blender_version_relative((0, 1, 0))
        version_e = blender_version_relative((0, 2, 0))

        python_script_this = python_script_generate_for_addon("for this platform")
        python_script_old = python_script_generate_for_addon("old")
        python_script_new = python_script_generate_for_addon("new")
        python_script_other = python_script_generate_for_addon("other")
        python_script_conflict = python_script_generate_for_addon("conflict")

        # Create a package contents (with a different wheel version).
        pkg_idname = "my_platform_test"
        for platform in (platform_this, *platforms_other):
            if platform == platform_this:
                python_script = python_script_this
            else:
                python_script = python_script_other

            self.build_package(
                pkg_idname=pkg_idname,
                platforms=(platform,),
                # Needed to prevent duplicates.
                pkg_filename="{:s}-{:s}".format(pkg_idname, platform.replace("-", "_")),
                blender_version_min=version_c_this,
                blender_version_max=version_d,
                python_script=python_script,
            )

        # Generate the repository.
        stdout = run_blender_extensions_no_errors((
            "server-generate",
            "--repo-dir", TEMP_DIR_REMOTE,
        ))
        self.assertEqual(stdout, "found 3 packages.\n")

        for version_range, pkg_filename_suffix, python_script in (
                ((version_a, version_b), "_no_conflict_old", python_script_old),
                ((version_d, version_e), "_no_conflict_new", python_script_new),
        ):
            self.build_package(
                pkg_idname=pkg_idname,
                platforms=(platform_this,),
                pkg_filename="{:s}-{:s}{:s}".format(pkg_idname, platform_this.replace("-", "_"), pkg_filename_suffix),
                blender_version_min=version_range[0],
                blender_version_max=version_range[1],
                python_script=python_script + "\n" + "print(" + repr(version_range) + ")",
            )

        # Re-generate the repository (no conflicts).
        stdout = run_blender_extensions_no_errors((
            "server-generate",
            "--repo-dir", TEMP_DIR_REMOTE,
        ))
        self.assertEqual(stdout, "found 5 packages.\n")

        # Install the package and check it installs the correct package.
        stdout = run_blender_extensions_no_errors((
            "sync",
        ))
        self.assertEqual(
            stdout.rstrip("\n").split("\n")[-1],
            "STATUS Extensions list for \"MyTestRepo\" updated",
        )

        stdout = run_blender_extensions_no_errors(("list",))
        self.assertEqual(
            stdout,
            (
                '''Repository: "MyTestRepo" (id={:s})\n'''
                '''  {:s}: "My Platform Test", This is a tagline\n'''
            ).format(repo_id, pkg_idname)
        )

        stdout = run_blender_extensions_no_errors(("install", pkg_idname, "--enable"), force_script_and_pause=False)
        self.assertEqual(
            [line for line in stdout.split("\n") if line.startswith("STATUS ")][0],
            "STATUS Installed \"{:s}\"".format(pkg_idname)
        )

        # Ensure the correct package was installed, using the script text as an identifier.
        self.assertTrue("Register success for this platform: " in stdout)

        stdout = run_blender_extensions_no_errors(("remove", pkg_idname))
        self.assertEqual(
            [line for line in stdout.split("\n") if line.startswith("STATUS ")][0],
            "STATUS Removed \"{:s}\"".format(pkg_idname)
        )

        # Now add two conflicting packages, one with a version, one without any versions.
        for version_range, pkg_filename_suffix in (
                (("", ""), "_conflict_no_version"),
                ((version_a, version_e), "_conflict"),
        ):
            self.build_package(
                pkg_idname=pkg_idname,
                platforms=(platform_this,),
                pkg_filename="{:s}-{:s}{:s}".format(pkg_idname, platform_this.replace("-", "_"), pkg_filename_suffix),
                blender_version_min=version_range[0] or None,
                blender_version_max=version_range[1] or None,
                python_script=python_script_conflict,
            )

        stdout = run_blender_extensions_no_errors((
            "server-generate",
            "--repo-dir", TEMP_DIR_REMOTE,
        ))
        self.assertEqual(stdout, (
            '''WARN: archive found with duplicates for id {pkg_idname:s}: '''
            '''3 duplicate(s) found, conflicting blender versions \"{platform:s}\": '''
            '''([undefined] & [{version_a:s} -> {version_b:s}], '''
            '''[{version_a:s} -> {version_b:s}] & [{version_a:s} -> {version_e:s}], '''
            '''[{version_a:s} -> {version_e:s}] & [{version_c:s} -> {version_d:s}])\n'''
            '''found 7 packages.\n'''
        ).format(
            pkg_idname=pkg_idname,
            platform=platform_this,
            version_a=version_a,
            version_b=version_b,
            version_c=version_c_this,
            version_d=version_d,
            version_e=version_e,
        ))


class TestModuleViolation(TestWithTempBlenderUser_MixIn, unittest.TestCase):

    def test_extension(self) -> None:
        """
        Warn when:
        - extensions add themselves to the ``sys.path``.
        - extensions add top-level modules into ``sys.modules``.
        """
        repo_id = "test_repo_module_violation"
        repo_name = "MyTestRepoViolation"

        self.repo_add(repo_id=repo_id, repo_name=repo_name)

        # Create a package contents.
        pkg_idname = "my_test_pkg"
        self.build_package(
            pkg_idname=pkg_idname,
            python_script=(
                '''import sys\n'''
                '''import os\n'''
                '''\n'''
                '''sys.path.append(os.path.join(os.path.dirname(__file__), "sys_modules_violate"))\n'''
                '''\n'''
                '''import bpy_sys_modules_violate_test\n'''
                '''\n'''
                '''def register():\n'''
                '''    print("Register!")\n'''
                '''def unregister():\n'''
                '''    print("Unregister!")\n'''
            ),
            file_contents={
                os.path.join("sys_modules_violate", "bpy_sys_modules_violate_test.py"):
                b'''print("This violating module has been loaded!")\n'''
            },
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
            "STATUS Extensions list for \"MyTestRepoViolation\" updated",
        )

        # Install the package into Blender.

        stdout = run_blender_extensions_no_errors(("repo-list",))
        self.assertEqual(
            stdout,
            (
                '''{:s}:\n'''
                '''    name: "MyTestRepoViolation"\n'''
                '''    directory: "{:s}"\n'''
                '''    url: "{:s}"\n'''
                '''    access_token: None\n'''
            ).format(repo_id, TEMP_DIR_LOCAL, TEMP_DIR_REMOTE_AS_URL))

        stdout = run_blender_extensions_no_errors(("install", pkg_idname, "--enable"))
        self.assertEqual(
            [line for line in stdout.split("\n") if line.startswith("STATUS ")][0],
            "STATUS Installed \"my_test_pkg\""
        )

        # List extensions.
        stdout = run_blender_extensions_no_errors((
            "list",
        ))
        self.assertEqual(
            stdout,
            (
                '''This violating module has been loaded!\n'''
                '''Register!\n'''
                '''Repository: "MyTestRepoViolation" (id=test_repo_module_violation)\n'''
                '''  my_test_pkg [installed]: "My Test Pkg", This is a tagline\n'''
                '''    Policy violation with top level module: bpy_sys_modules_violate_test\n'''
                '''    Policy violation with sys.path: ./sys_modules_violate\n'''
                '''Unregister!\n'''
            )
        )


class TestBlockList(TestWithTempBlenderUser_MixIn, unittest.TestCase):

    def test_blocked(self) -> None:
        """
        Warn when:
        - extensions add themselves to the ``sys.path``.
        - extensions add top-level modules into ``sys.modules``.
        """
        repo_id = "test_repo_blocklist"
        repo_name = "MyTestRepoBlocked"

        self.repo_add(repo_id=repo_id, repo_name=repo_name)

        pkg_idnames = (
            "my_test_pkg_a",
            "my_test_pkg_b",
            "my_test_pkg_c",
        )

        # Create a package contents.
        for pkg_idname in pkg_idnames:
            self.build_package(pkg_idname=pkg_idname)

        repo_config_filepath = os.path.join(TEMP_DIR_REMOTE, "blender_repo.toml")
        with open(repo_config_filepath, "w", encoding="utf8") as fh:
            fh.write(
                '''schema_version = "1.0.0"\n'''
                '''[[blocklist]]\n'''
                '''id = "my_test_pkg_a"\n'''
                '''reason = "One example reason"\n'''
                '''[[blocklist]]\n'''
                '''id = "my_test_pkg_c"\n'''
                '''reason = "Another example reason"\n'''
            )

        # Generate the repository.
        stdout = run_blender_extensions_no_errors((
            "server-generate",
            "--repo-dir", TEMP_DIR_REMOTE,
            "--repo-config", repo_config_filepath,
        ))
        self.assertEqual(stdout, "found 3 packages.\n")

        stdout = run_blender_extensions_no_errors((
            "sync",
        ))
        self.assertEqual(
            stdout.rstrip("\n").split("\n")[-1],
            "STATUS Extensions list for \"{:s}\" updated".format(repo_name),
        )

        # List packages.
        stdout = run_blender_extensions_no_errors(("list",))
        self.assertEqual(
            stdout,
            (
                '''Repository: "{:s}" (id={:s})\n'''
                '''  my_test_pkg_a: "My Test Pkg A", This is a tagline\n'''
                '''    Blocked: One example reason\n'''
                '''  my_test_pkg_b: "My Test Pkg B", This is a tagline\n'''
                '''  my_test_pkg_c: "My Test Pkg C", This is a tagline\n'''
                '''    Blocked: Another example reason\n'''
            ).format(
                repo_name,
                repo_id,
            ))

        # Install the package into Blender.
        stdout = run_blender_extensions_no_errors(("install", pkg_idnames[1], "--enable"))
        self.assertEqual(
            [line for line in stdout.split("\n") if line.startswith("STATUS ")][0],
            "STATUS Installed \"{:s}\"".format(pkg_idnames[1])
        )

        # Ensure blocking works, fail to install the package into Blender.
        stdout = run_blender_extensions_no_errors(("install", pkg_idnames[0], "--enable"))
        self.assertEqual(
            [line for line in stdout.split("\n") if line.startswith("FATAL_ERROR ")][0],
            "FATAL_ERROR Package \"{:s}\", is blocked: One example reason".format(pkg_idnames[0])
        )

        # Install the package into Blender.


def main() -> None:
    # pylint: disable-next=global-statement
    global TEMP_DIR_BLENDER_USER, TEMP_DIR_REMOTE, TEMP_DIR_LOCAL, TEMP_DIR_TMPDIR, TEMP_DIR_REMOTE_AS_URL

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
