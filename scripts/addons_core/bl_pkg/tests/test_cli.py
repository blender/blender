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
import unittest

import unittest.util

from typing import (
    Any,
    Sequence,
    Dict,
    NamedTuple,
    Optional,
    Set,
    Tuple,
)

# For more useful output that isn't clipped.
unittest.util._MAX_LENGTH = 10_000

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


# -----------------------------------------------------------------------------
# HTTP Server (simulate remote access)
#

# -----------------------------------------------------------------------------
# Generate Repository
#


def my_create_package(dirpath: str, filename: str, *, metadata: Dict[str, Any], files: Dict[str, bytes]) -> None:
    """
    Create a package using the command line interface.
    """
    assert filename.endswith(PKG_EXT)
    outfile = os.path.join(dirpath, filename)

    # NOTE: use the command line packaging utility to ensure 1:1 behavior with actual packages.
    metadata_copy = metadata.copy()

    with tempfile.TemporaryDirectory() as temp_dir_pkg:
        temp_dir_pkg_manifest_toml = os.path.join(temp_dir_pkg, PKG_MANIFEST_FILENAME_TOML)
        with open(temp_dir_pkg_manifest_toml, "wb") as fh:
            # NOTE: escaping is not supported, this is primitive TOML writing for tests.
            data = "".join((
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
            )).encode('utf-8')
            fh.write(data)

        if metadata_copy:
            raise Exception("Unexpected mata-data: {!r}".format(metadata_copy))

        for filename_iter, data in files.items():
            with open(os.path.join(temp_dir_pkg, filename_iter), "wb") as fh:
                fh.write(data)

        output_json = command_output_from_json_0(
            [
                "build",
                "--source-dir", temp_dir_pkg,
                "--output-filepath", outfile,
            ],
            exclude_types={"PROGRESS"},
        )

        output_json_error = command_output_filter_exclude(
            output_json,
            exclude_types=STATUS_NON_ERROR,
        )

        if output_json_error:
            raise Exception("Creating a package produced some error output: {!r}".format(output_json_error))


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
        )


def command_output_filter_include(
        output_json: Sequence[Tuple[str, Any]],
        include_types: Set[str],
) -> Sequence[Tuple[str, Any]]:
    return [(a, b) for a, b in output_json if a in include_types]


def command_output_filter_exclude(
        output_json: Sequence[Tuple[str, Any]],
        exclude_types: Set[str],
) -> Sequence[Tuple[str, Any]]:
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
        exclude_types: Optional[Set[str]] = None,
        expected_returncode: int = 0,
) -> Sequence[Tuple[str, Any]]:
    result = []

    proc = subprocess.run(
        [*CMD, *args, "--output-type=JSON_0"],
        stdout=subprocess.PIPE,
        check=expected_returncode == 0,
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
                    ("STATUS", "Re-Installed \"another_package\"")
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
                    ("ERROR", "Package not found \"another_package_\"")
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
        with HTTPServerContext(directory=TEMP_DIR_REMOTE, port=HTTP_PORT):
            unittest.main()
    else:
        unittest.main()
