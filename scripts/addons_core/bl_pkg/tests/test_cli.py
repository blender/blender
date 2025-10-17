# SPDX-FileCopyrightText: 2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
Test with command:
   make test_blender BLENDER_BIN=$PWD/../../../blender.bin
"""

import json
import os
import shutil
import subprocess
import sys
import tempfile
import tomllib
import unittest
import unittest.util
import zipfile

from typing import (
    Any,
    NamedTuple,
)
from collections.abc import (
    Sequence,
)

# A tree of files.
FileTree = dict[str, "FileTree | bytes"]

JSON_OutputElem = tuple[str, Any]

# For more useful output that isn't clipped.
# pylint: disable-next=protected-access
unittest.util._MAX_LENGTH = 10_000  # type: ignore

IS_WIN32 = sys.platform == "win32"

# See the variable with the same name in `blender_ext.py`.
REMOTE_REPO_HAS_JSON_IMPLIED = True

PKG_EXT = ".zip"

# PKG_REPO_LIST_FILENAME = "index.json"

PKG_MANIFEST_FILENAME_TOML = "blender_manifest.toml"

# Use an in-memory temp, when available.
TEMP_PREFIX = tempfile.gettempdir()
if os.path.exists("/ramcache/tmp"):
    TEMP_PREFIX = "/ramcache/tmp"

TEMP_DIR_REMOTE = os.path.join(TEMP_PREFIX, "bl_ext_remote")
TEMP_DIR_LOCAL = os.path.join(TEMP_PREFIX, "bl_ext_local")

if TEMP_DIR_LOCAL and not os.path.isdir(TEMP_DIR_LOCAL):
    os.makedirs(TEMP_DIR_LOCAL)
if TEMP_DIR_REMOTE and not os.path.isdir(TEMP_DIR_REMOTE):
    os.makedirs(TEMP_DIR_REMOTE)


BASE_DIR = os.path.abspath(os.path.dirname(__file__))
# PYTHON_CMD = sys.executable

CMD = (
    sys.executable,
    os.path.normpath(os.path.join(BASE_DIR, "..", "cli", "blender_ext.py")),
)

# Simulate communicating with a web-server.
USE_HTTP = os.environ.get("USE_HTTP", "0") != "0"
HTTP_PORT = 8001

VERBOSE = os.environ.get("VERBOSE", "0") != "0"

sys.path.append(os.path.join(BASE_DIR, "modules"))
from http_server_context import HTTPServerContext  # noqa: E402

import python_wheel_generate  # noqa: E402


STATUS_NON_ERROR = {'STATUS', 'PROGRESS'}


# -----------------------------------------------------------------------------
# Generic Utilities
#

def remote_url_params_strip(url: str) -> str:
    import urllib
    # Parse the URL to get its scheme, domain, and query parameters.
    parsed_url = urllib.parse.urlparse(url)

    # Combine the scheme, netloc, path without any other parameters, stripping the URL.
    new_url = urllib.parse.urlunparse((
        parsed_url.scheme,
        parsed_url.netloc,
        parsed_url.path,
        None,  # `parsed_url.params,`
        None,  # `parsed_url.query,`
        None,  # `parsed_url.fragment,`
    ))

    return new_url


def path_to_url(path: str) -> str:
    from urllib.parse import urljoin
    from urllib.request import pathname2url
    return urljoin('file:', pathname2url(path))


def rmdir_contents(directory: str) -> None:
    """
    Remove all directory contents without removing the directory.
    """
    for entry in os.scandir(directory):
        filepath = os.path.join(directory, entry.name)
        if entry.is_dir():
            shutil.rmtree(filepath)
        else:
            os.unlink(filepath)


def manifest_dict_from_archive(filepath: str) -> dict[str, Any]:
    with zipfile.ZipFile(filepath, mode="r") as zip_fh:
        manifest_data = zip_fh.read(PKG_MANIFEST_FILENAME_TOML)
        manifest_dict = tomllib.loads(manifest_data.decode("utf-8"))
        return manifest_dict


# -----------------------------------------------------------------------------
# Generate Repository
#

class WheelModuleParams(NamedTuple):
    module_name: str
    module_version: str

    filename: str | None = None


def files_create_in_dir(basedir: str, files: FileTree) -> None:
    if not os.path.isdir(basedir):
        os.makedirs(basedir)
    for filename_iter, data in files.items():
        path = os.path.join(basedir, filename_iter)
        if isinstance(data, bytes):
            with open(path, "wb") as fh:
                fh.write(data)
        elif isinstance(data, dict):
            files_create_in_dir(path, data)
        else:
            assert False, "Unreachable"


def my_create_package(
        dirpath: str,
        filename: str,
        *,
        metadata: dict[str, Any],
        files: FileTree,
        build_args_extra: tuple[str, ...],
        wheel_params: Sequence[WheelModuleParams] = (),
        expected_returncode: int = 0,
) -> Sequence[JSON_OutputElem]:
    """
    Create a package using the command line interface.
    """
    assert filename.endswith(PKG_EXT)
    outfile = os.path.join(dirpath, filename)

    # NOTE: use the command line packaging utility to ensure 1:1 behavior with actual packages.
    metadata_copy = metadata.copy()

    with tempfile.TemporaryDirectory() as temp_dir_pkg:

        wheel_filenames = []
        if wheel_params:
            for w in wheel_params:
                wheel_filename, wheel_filedata = python_wheel_generate.generate_from_source(
                    module_name=w.module_name,
                    version=w.module_version,
                    source=(
                        "__version__ = {!r}\n"
                        "print(\"The wheel has been found\")\n"
                    ).format(w.module_version),
                )
                if w.filename is not None:
                    wheel_filename = w.filename

                wheel_dir = os.path.join(temp_dir_pkg, "wheels")
                os.makedirs(wheel_dir, exist_ok=True)

                wheel_path = os.path.join(wheel_dir, wheel_filename)
                with open(wheel_path, "wb") as fh:
                    fh.write(wheel_filedata)

                wheel_filenames.append(wheel_filename)

        temp_dir_pkg_manifest_toml = os.path.join(temp_dir_pkg, PKG_MANIFEST_FILENAME_TOML)
        with open(temp_dir_pkg_manifest_toml, "wb") as fh:
            # NOTE: escaping is not supported, this is primitive TOML writing for tests.
            data_list = [
                """# Example\n""",
                """schema_version = "{:s}"\n""".format(metadata_copy.pop("schema_version")),
                """id = "{:s}"\n""".format(metadata_copy.pop("id")),
                """name = "{:s}"\n""".format(metadata_copy.pop("name")),
                """tagline = "{:s}"\n""".format(metadata_copy.pop("tagline")),
                """version = "{:s}"\n""".format(metadata_copy.pop("version")),
                """type = "{:s}"\n""".format(metadata_copy.pop("type")),
                """tags = [{:s}]\n""".format(", ".join("\"{:s}\"".format(v) for v in metadata_copy.pop("tags"))),
                """blender_version_min = "{:s}"\n""".format(metadata_copy.pop("blender_version_min")),
                """maintainer = "{:s}"\n""".format(metadata_copy.pop("maintainer")),
                """license = [{:s}]\n""".format(", ".join("\"{:s}\"".format(v) for v in metadata_copy.pop("license"))),
            ]

            if (value := metadata_copy.pop("platforms", None)) is not None:
                data_list.append("""platforms = [{:s}]\n""".format(", ".join("\"{:s}\"".format(v) for v in value)))

            has_wheels = False
            wheels_all = []
            if (value := metadata_copy.pop("wheels", None)) is not None:
                wheels_all.extend(value)
                has_wheels = True
            if wheel_filenames:
                wheels_all.extend(["./wheels/" + wheel_filename for wheel_filename in wheel_filenames])
                has_wheels = True

            if has_wheels:
                data_list.append("""wheels = [{:s}]\n""".format(", ".join("\"{:s}\"".format(v) for v in wheels_all)))
            del has_wheels, wheels_all

            if (value := metadata_copy.pop("build", None)) is not None:
                value_copy = value.copy()

                data_list.append(
                    """\n"""
                    """[build]\n"""
                )

                if (value := value_copy.pop("paths", None)) is not None:
                    data_list.append("""paths = [{:s}]\n""".format(", ".join("\"{:s}\"".format(v) for v in value)))

                if value_copy:
                    raise Exception("Unexpected metadata [build]: {!r}".format(value_copy))
                del value_copy

            fh.write("".join(data_list).encode('utf-8'))

        if metadata_copy:
            raise Exception("Unexpected metadata: {!r}".format(metadata_copy))

        files_create_in_dir(temp_dir_pkg, files)

        output_json = command_output_from_json_0(
            [
                "build",
                "--source-dir", ".",
                "--output-filepath", outfile,
                *build_args_extra,
            ],
            exclude_types={"PROGRESS"},
            expected_returncode=expected_returncode,
            # NOTE: using the CWD makes error message output more predictable
            # in this case it means `temp_dir_pkg` isn't part of the output
            # which is important as the value is not known to the caller.
            cwd=temp_dir_pkg,
        )

        output_json_error = command_output_filter_exclude(
            output_json,
            exclude_types=STATUS_NON_ERROR,
        )

        if expected_returncode == 0:
            if output_json_error:
                raise Exception("Creating a package produced some error output: {!r}".format(output_json_error))

        return output_json


class PkgTemplate(NamedTuple):
    """Data need to create a package for testing."""
    idname: str
    name: str
    version: str


def my_generate_repo(
        dirpath: str,
        *,
        templates: Sequence[PkgTemplate],
) -> None:
    for template in templates:
        my_create_package(
            dirpath, template.idname + PKG_EXT,
            metadata={
                "schema_version": "1.0.0",
                "id": template.idname,
                "name": template.name,
                "tagline": """This package has a tagline""",
                "version": template.version,
                "type": "add-on",
                "tags": ["UV", "Modeling"],
                "blender_version_min": "0.0.0",
                "maintainer": "Some Developer",
                "license": ["SPDX:GPL-2.0-or-later"],
            },
            files={
                "__init__.py": b"# This is a script\n",
            },
            build_args_extra=(),
        )


def command_output_filter_include(
        output_json: Sequence[JSON_OutputElem],
        include_types: set[str],
) -> Sequence[JSON_OutputElem]:
    return [(a, b) for a, b in output_json if a in include_types]


def command_output_filter_exclude(
        output_json: Sequence[JSON_OutputElem],
        exclude_types: set[str],
) -> Sequence[JSON_OutputElem]:
    return [(a, b) for a, b in output_json if a not in exclude_types]


def command_output(
        args: Sequence[str],
        expected_returncode: int = 0,
) -> str:
    proc = subprocess.run(
        [*CMD, *args],
        stdout=subprocess.PIPE,
        check=expected_returncode == 0,
    )
    if proc.returncode != expected_returncode:
        raise subprocess.CalledProcessError(proc.returncode, proc.args, output=proc.stdout, stderr=proc.stderr)
    result = proc.stdout.decode("utf-8")
    if IS_WIN32:
        result = result.replace("\r\n", "\n")
    return result


def command_output_from_json_0(
        args: Sequence[str],
        *,
        exclude_types: set[str] | None = None,
        expected_returncode: int = 0,
        cwd: str | None = None,
) -> Sequence[JSON_OutputElem]:
    result = []

    proc = subprocess.run(
        [*CMD, *args, "--output-type=JSON_0"],
        stdout=subprocess.PIPE,
        check=expected_returncode == 0,
        cwd=cwd,
    )
    if proc.returncode != expected_returncode:
        raise subprocess.CalledProcessError(proc.returncode, proc.args, output=proc.stdout, stderr=proc.stderr)
    for json_bytes in proc.stdout.split(b'\0'):
        if not json_bytes:
            continue
        json_str = json_bytes.decode("utf-8")
        json_data = json.loads(json_str)
        assert len(json_data) == 2
        assert isinstance(json_data[0], str)
        if (exclude_types is not None) and (json_data[0] in exclude_types):
            continue
        result.append((json_data[0], json_data[1]))

    return result


class TestCLI(unittest.TestCase):

    def test_version(self) -> None:
        self.assertEqual(command_output(["--version"]), "0.1\n")

    def test_progress(self) -> None:
        self.assertEqual(
            command_output(["dummy-progress", "--time-delay=0.0", "--steps-limit=4"]),
            'PROGRESS: Demo, BYTE, 0, 4\n'
            'PROGRESS: Demo, BYTE, 1, 4\n'
            'PROGRESS: Demo, BYTE, 2, 4\n'
            'PROGRESS: Demo, BYTE, 3, 4\n'
            'PROGRESS: Demo, BYTE, 4, 4\n'
            'DONE: \n',
        )


class TestCLI_Build(unittest.TestCase):

    dirpath = ""

    @classmethod
    def setUpClass(cls) -> None:
        cls.dirpath = TEMP_DIR_LOCAL
        if os.path.isdir(cls.dirpath):
            rmdir_contents(TEMP_DIR_LOCAL)
        else:
            os.makedirs(TEMP_DIR_LOCAL)

    @classmethod
    def tearDownClass(cls) -> None:
        if os.path.isdir(cls.dirpath):
            rmdir_contents(TEMP_DIR_LOCAL)

    def test_build_multi_platform(self) -> None:
        platforms = [
            "linux-arm64",
            "linux-x64",
            "macos-arm64",
            "macos-x64",
            "windows-arm64",
            "windows-x64",
        ]
        wheels = [
            # Must be included in all packages.
            "my_portable_package-3.0.1-py3-none-any.whl",

            # Each package must include only one.
            # Include two versions (B may be a "universal" package) for platforms that support it.
            "my_platform_package_A-10.3.0-cp311-cp311-macosx_11_0_arm64.whl",
            "my_platform_package_A-10.3.0-cp311-cp311-macosx_11_0_x86_64.whl",
            "my_platform_package_B-10.3.0-cp311-cp311-macosx_11_0_universal2.whl",

            "my_platform_package_A-10.3.0-cp311-cp311-manylinux_2_28_aarch64.whl",
            "my_platform_package_B-10.3.0-cp311-cp311-manylinux_2_28_aarch64.whl",
            "my_platform_package_A-10.3.0-cp311-cp311-manylinux_2_28_x86_64.whl",
            "my_platform_package_B-10.3.0-cp311-cp311-manylinux_2_28_x86_64.whl",

            "my_platform_package_A-10.3.0-cp311-cp311-win_amd64.whl",
            "my_platform_package_B-10.3.0-cp311-cp311-win_amd64.whl",
            "my_platform_package_A-10.3.0-cp311-cp311-win_arm64.whl",
            "my_platform_package_B-10.3.0-cp311-cp311-win_arm64.whl",
        ]

        pkg_idname = "my_test"
        output_json = my_create_package(
            self.dirpath,
            pkg_idname + PKG_EXT,
            metadata={
                "schema_version": "1.0.0",
                "id": "multi_platform_test",
                "name": "Multi Platform Test",
                "tagline": """This package has a tagline""",
                "version": "1.0.0",
                "type": "add-on",
                "tags": ["UV", "Modeling"],
                "blender_version_min": "0.0.0",
                "maintainer": "Some Developer",
                "license": ["SPDX:GPL-2.0-or-later"],
                "platforms": platforms,
                "wheels": ["./wheels/" + filename for filename in wheels]
            },
            files={
                "__init__.py": b"# This is a script\n",
                "wheels": {filename: b"" for filename in wheels},
            },
            build_args_extra=(
                # Include `add: {...}` so the file list can be scanned.
                "--verbose",
                "--split-platforms",
            ),
        )

        output_json = command_output_filter_include(
            output_json,
            include_types={'STATUS'},
        )

        packages: list[tuple[str, list[JSON_OutputElem]]] = [("", [])]
        for _, message in output_json:
            if message.startswith("building: "):
                assert not packages[-1][0]
                assert not packages[-1][1]
                packages[-1] = (message.removeprefix("building: "), [])
            elif message.startswith("add: "):
                packages[-1][1].append(message.removeprefix("add: "))
            elif message.startswith("created: "):
                pass
            elif message == "complete":
                packages.append(("", []))
            else:
                raise Exception("Unexpected status: {:s}".format(message))

        packages_dict = dict(packages)
        for platform in platforms:
            filename = "{:s}-{:s}{:s}".format(pkg_idname, platform.replace("-", "_"), PKG_EXT)
            value = packages_dict.get(filename)
            assert isinstance(value, list)
            # A check here that gives a better error would be nice, for now, check there are always 5 files.
            self.assertEqual(len(value), 5)

            manifest_dict = manifest_dict_from_archive(os.path.join(self.dirpath, filename))

            # Ensure the generated data is included:
            # `[build.generated]`
            # `platforms = [{platform}]`
            build_value = manifest_dict.get("build")
            assert build_value is not None
            build_generated_value = build_value.get("generated")
            assert build_generated_value is not None
            build_generated_platforms_value = build_generated_value.get("platforms")
            assert build_generated_platforms_value is not None
            self.assertEqual(build_generated_platforms_value, [platform])

    def _test_build_path_literal_impl(
            self,
            build_paths: list[str],
            *,
            wheel_params: Sequence[WheelModuleParams] = (),
            expected_returncode: int = 0,
    ) -> tuple[str, Sequence[JSON_OutputElem]]:
        pkg_idname = "my_test_paths"
        output_json = my_create_package(
            self.dirpath,
            pkg_idname + PKG_EXT,
            metadata={
                "schema_version": "1.0.0",
                "id": "multi_platform_test",
                "name": "Multi Platform Test",
                "tagline": """This package has a tagline""",
                "version": "1.0.0",
                "type": "add-on",
                "tags": ["UV", "Modeling"],
                "blender_version_min": "0.0.0",
                "maintainer": "Some Developer",
                "license": ["SPDX:GPL-2.0-or-later"],
                # The main difference with other tests.
                "build": {
                    "paths": build_paths,
                },
            },
            files={
                "__init__.py": b"# This is a script\n",
            },
            build_args_extra=(
                # Include `add: {...}` so the file list can be scanned.
                "--verbose",
            ),
            wheel_params=wheel_params,
            expected_returncode=expected_returncode,
        )

        output_path = os.path.join(TEMP_DIR_LOCAL, pkg_idname + ".zip")
        return output_path, output_json

    def test_build_path_literal_success(self) -> None:
        output_path, output_json = self._test_build_path_literal_impl(
            build_paths=["__init__.py"],
            expected_returncode=0,

        )

        output_size = os.path.getsize(output_path)
        self.assertEqual(
            output_json, [
                ("STATUS", "building: my_test_paths.zip"),
                ("STATUS", "add: blender_manifest.toml"),
                ("STATUS", "add: __init__.py"),
                ("STATUS", "complete"),
                ("STATUS", "created: \"{:s}\", {:d}".format(output_path, output_size)),
            ]
        )

    def test_build_path_literal_with_wheel_success(self) -> None:
        output_path, output_json = self._test_build_path_literal_impl(
            build_paths=["__init__.py"],
            wheel_params=[
                WheelModuleParams(module_name="my_wheel", module_version="2.3"),
            ],
            expected_returncode=0,

        )

        output_size = os.path.getsize(output_path)
        self.assertEqual(
            output_json, [
                ("STATUS", "building: my_test_paths.zip"),
                ("STATUS", "add: blender_manifest.toml"),
                ("STATUS", "add: __init__.py"),
                ('STATUS', 'add: wheels/my_wheel-2.3-py3-none-any.whl'),
                ("STATUS", "complete"),
                ("STATUS", "created: \"{:s}\", {:d}".format(output_path, output_size)),
            ]
        )

    def test_build_path_literal_error_duplicate(self) -> None:
        _output_path, output_json = self._test_build_path_literal_impl(
            build_paths=[
                "__init__.py",
                "__init__.py",
            ],
            expected_returncode=1,

        )

        self.assertEqual(
            output_json, [
                ("FATAL_ERROR", (
                    "Error parsing TOML \"{:s}\" Expected \"paths\" to contain unique paths, "
                    "duplicate found: \"__init__.py\""
                ).format(os.path.join(".", PKG_MANIFEST_FILENAME_TOML)))
            ]
        )

    def test_build_path_literal_error_non_relative(self) -> None:
        path_abs = "/test" if os.sep == "/" else "C:\\"
        _output_path, output_json = self._test_build_path_literal_impl(
            build_paths=[
                "__init__.py",
                path_abs,
            ],
            expected_returncode=1,

        )

        self.assertEqual(
            output_json, [
                ("FATAL_ERROR", (
                    "Error parsing TOML \"{:s}\" Expected \"paths\" to be relative, "
                    "found: \"{:s}\""
                ).format(os.path.join(".", PKG_MANIFEST_FILENAME_TOML), path_abs))
            ]
        )

    def test_build_path_literal_error_missing_file(self) -> None:
        _output_path, output_json = self._test_build_path_literal_impl(
            build_paths=[
                "__init__.py",
                "MISSING_FILE",
            ],
            expected_returncode=1,

        )

        self.assertEqual(
            output_json, [
                ("STATUS", "building: my_test_paths.zip"),
                ("STATUS", "add: blender_manifest.toml"),
                ("STATUS", "add: __init__.py"),
                ("FATAL_ERROR", "Error adding to archive, file not found: \"MISSING_FILE\""),
            ]
        )

    def test_build_path_literal_error_has_manifest(self) -> None:
        _output_path, output_json = self._test_build_path_literal_impl(
            build_paths=[
                "__init__.py",
                PKG_MANIFEST_FILENAME_TOML,
            ],
            expected_returncode=1,

        )

        self.assertEqual(
            output_json, [
                (
                    "FATAL_ERROR",
                    (
                        "Error parsing TOML \"{:s}\" Expected \"paths\" not to "
                        "contain the manifest, found: \"blender_manifest.toml\""
                    ).format(os.path.join(".", PKG_MANIFEST_FILENAME_TOML)),
                ),
            ]
        )


class TestCLI_WithRepo(unittest.TestCase):
    dirpath = ""
    dirpath_url = ""

    @classmethod
    def setUpClass(cls) -> None:
        if TEMP_DIR_REMOTE:
            cls.dirpath = TEMP_DIR_REMOTE
            if os.path.isdir(cls.dirpath):
                # pylint: disable-next=using-constant-test
                if False:
                    shutil.rmtree(cls.dirpath)
                    os.makedirs(TEMP_DIR_REMOTE)
                else:
                    # Empty the path without removing it,
                    # handy so a developer can remain in the directory.
                    rmdir_contents(TEMP_DIR_REMOTE)
            else:
                os.makedirs(TEMP_DIR_REMOTE)
        else:
            cls.dirpath = tempfile.mkdtemp(prefix="bl_ext_")

        my_generate_repo(
            cls.dirpath,
            templates=(
                PkgTemplate(idname="foo_bar", name="Foo Bar", version="1.0.5"),
                PkgTemplate(idname="another_package", name="Another Package", version="1.5.2"),
                PkgTemplate(idname="test_package", name="Test Package", version="1.5.2"),
            ),
        )

        if USE_HTTP:
            if REMOTE_REPO_HAS_JSON_IMPLIED:
                cls.dirpath_url = "http://localhost:{:d}/index.json".format(HTTP_PORT)
            else:
                cls.dirpath_url = "http://localhost:{:d}".format(HTTP_PORT)
        else:
            # Even local paths must URL syntax: `file://`.
            cls.dirpath_url = path_to_url(cls.dirpath)

    @classmethod
    def tearDownClass(cls) -> None:
        if not TEMP_DIR_REMOTE:
            shutil.rmtree(cls.dirpath)
        del cls.dirpath
        del cls.dirpath_url

    def test_version(self) -> None:
        self.assertEqual(command_output(["--version"]), "0.1\n")

    def test_server_generate(self) -> None:
        output = command_output(["server-generate", "--repo-dir", self.dirpath])
        self.assertEqual(output, "found 3 packages.\n")

    def test_client_list(self) -> None:
        # TODO: only run once.
        self.test_server_generate()

        output = command_output(["list", "--remote-url", self.dirpath_url, "--local-dir", ""])
        self.assertEqual(
            output, (
                "another_package(1.5.2): Another Package\n"
                "foo_bar(1.0.5): Foo Bar\n"
                "test_package(1.5.2): Test Package\n"
            )
        )
        del output

        # TODO, figure out how to split JSON & TEXT output tests, this test just checks JSON is working at all.
        output_json = command_output_from_json_0(
            ["list", "--remote-url", self.dirpath_url, "--local-dir", ""],
            exclude_types={"PROGRESS"},
        )
        self.assertEqual(
            output_json, [
                ("STATUS", "another_package(1.5.2): Another Package"),
                ("STATUS", "foo_bar(1.0.5): Foo Bar"),
                ("STATUS", "test_package(1.5.2): Test Package"),
            ]
        )

    def test_client_install_and_uninstall(self) -> None:
        stripped_url = remote_url_params_strip(self.dirpath_url)
        with tempfile.TemporaryDirectory(dir=TEMP_DIR_LOCAL) as temp_dir_local:
            # TODO: only run once.
            self.test_server_generate()

            output_json = command_output_from_json_0([
                "sync",
                "--remote-url", self.dirpath_url,
                "--local-dir", temp_dir_local,
            ], exclude_types={"PROGRESS"})
            self.assertEqual(
                output_json, [
                    ('STATUS', "Checking repository \"{:s}\" for updates...".format(stripped_url)),
                    ('STATUS', "Refreshing extensions list for \"{:s}\"...".format(stripped_url)),
                    ('STATUS', "Extensions list for \"{:s}\" updated".format(stripped_url)),
                ]
            )

            # Install.
            output_json = command_output_from_json_0(
                [
                    "install", "another_package",
                    "--remote-url", self.dirpath_url,
                    "--local-dir", temp_dir_local,
                ],
                exclude_types={"PROGRESS"},
            )
            self.assertEqual(
                output_json, [
                    ("STATUS", "Installed \"another_package\"")
                ]
            )
            self.assertTrue(os.path.isdir(os.path.join(temp_dir_local, "another_package")))

            # Re-Install.
            output_json = command_output_from_json_0(
                [
                    "install", "another_package",
                    "--remote-url", self.dirpath_url,
                    "--local-dir", temp_dir_local,
                ],
                exclude_types={"PROGRESS"},
            )
            self.assertEqual(
                output_json, [
                    ("STATUS", "Reinstalled \"another_package\"")
                ]
            )
            self.assertTrue(os.path.isdir(os.path.join(temp_dir_local, "another_package")))

            # Uninstall (not found).
            output_json = command_output_from_json_0(
                [
                    "uninstall", "another_package_",
                    "--local-dir", temp_dir_local,
                ],
                expected_returncode=1,
            )
            self.assertEqual(
                output_json, [
                    ("FATAL_ERROR", "Package not found \"another_package_\"")
                ]
            )

            # Uninstall.
            output_json = command_output_from_json_0([
                "uninstall", "another_package",
                "--local-dir", temp_dir_local,
            ])
            self.assertEqual(
                output_json, [
                    ("STATUS", "Removed \"another_package\"")
                ]
            )
            self.assertFalse(os.path.isdir(os.path.join(temp_dir_local, "another_package")))


if __name__ == "__main__":
    if USE_HTTP:
        # This doesn't take advantage of a HTTP client/server.
        del TestCLI_Build

        with HTTPServerContext(directory=TEMP_DIR_REMOTE, port=HTTP_PORT):
            unittest.main()
    else:
        unittest.main()
