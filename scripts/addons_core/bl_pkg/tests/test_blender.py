# SPDX-FileCopyrightText: 2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
Test with command:
   make test
"""

# NOTE:
# Currently this sets up an environment and runs commands.

# High level tests, run operators which manage a repository, ensure they work as expected.
# This tests Blender's integration for all areas except for the interactive GUI... for now
# perhaps this is supported in the future.


# Start a web server, connect blender to it, then setup new repos and install extensions.


import json
import os
import subprocess
import sys
import tempfile

from typing import (
    Any,
    Sequence,
    Tuple,
)

BASE_DIR = os.path.abspath(os.path.dirname(__file__))

CMD = (
    sys.executable,
    os.path.normpath(os.path.join(BASE_DIR, "..", "cli", "blender_ext.py")),
)

# Simulate communicating with a web-server.
USE_HTTP = os.environ.get("USE_HTTP", "0") != "0"
HTTP_PORT = 8002

VERBOSE = os.environ.get("VERBOSE", "0") != "0"

sys.path.append(os.path.join(BASE_DIR, "modules"))
from http_server_context import HTTPServerContext  # noqa: E402


PKG_REPO_LIST_FILENAME = "index.json"

# Use an in-memory temp, when available.
TEMP_PREFIX = tempfile.gettempdir()
if os.path.exists("/ramcache/tmp"):
    TEMP_PREFIX = "/ramcache/tmp"

# Useful for debugging, when blank create dynamically.
TEMP_DIR_SOURCE = os.path.join(TEMP_PREFIX, "blender_app_ext_source")
TEMP_DIR_REMOTE = os.path.join(TEMP_PREFIX, "blender_app_ext_remote")
TEMP_DIR_LOCAL = os.path.join(TEMP_PREFIX, "blender_app_ext_local")

if TEMP_DIR_SOURCE and not os.path.isdir(TEMP_DIR_SOURCE):
    os.makedirs(TEMP_DIR_SOURCE)
if TEMP_DIR_LOCAL and not os.path.isdir(TEMP_DIR_LOCAL):
    os.makedirs(TEMP_DIR_LOCAL)
if TEMP_DIR_REMOTE and not os.path.isdir(TEMP_DIR_REMOTE):
    os.makedirs(TEMP_DIR_REMOTE)


# -----------------------------------------------------------------------------
# Generic Functions

def command_output_from_json_0(args: Sequence[str]) -> Sequence[Tuple[str, Any]]:
    result = []
    for json_bytes in subprocess.check_output(
        [*CMD, *args, "--output-type=JSON_0"],
    ).split(b'\0'):
        if not json_bytes:
            continue
        json_str = json_bytes.decode("utf-8")
        json_data = json.loads(json_str)
        assert len(json_data) == 2
        assert isinstance(json_data[0], str)
        result.append((json_data[0], json_data[1]))

    return result


def ensure_script_directory(script_directory_to_add: str) -> None:
    import bpy  # type: ignore
    script_directories = bpy.context.preferences.filepaths.script_directories
    script_dir_empty = None
    for script_dir in script_directories:
        dir_test = script_dir.directory
        if dir_test == script_directory_to_add:
            return
        if not dir_test:
            script_dir_empty = script_dir

    if not script_dir_empty:
        bpy.ops.preferences.script_directory_add()
        script_dir_empty = script_directories[-1]

    script_dir_empty.directory = script_directory_to_add

    if script_directory_to_add not in sys.path:
        sys.path.append(script_directory_to_add)


def blender_test_run(temp_dir_local: str) -> None:
    import bpy
    import addon_utils  # type: ignore

    preferences = bpy.context.preferences

    addon_dir = os.path.normpath(os.path.join(BASE_DIR, "..", "blender_addon"))

    ensure_script_directory(addon_dir)

    if VERBOSE:
        print("--- Begin ---")

    addon_utils.enable("bl_pkg")

    # NOTE: it's assumed the URL will expand to JSON, example:
    # http://extensions.local:8111/add-ons/?format=json
    # This is not supported by the test server so the file name needs to be added.
    remote_url = "http://localhost:{:d}/{:s}".format(HTTP_PORT, PKG_REPO_LIST_FILENAME)

    repo = preferences.extensions.repos.new(
        name="My Test",
        module="my_repo",
        custom_directory=temp_dir_local,
        remote_url=remote_url,
    )

    bpy.ops.extensions.dummy_progress()

    bpy.ops.extensions.repo_sync(
        repo_directory=temp_dir_local,
    )

    bpy.ops.extensions.package_install(
        repo_directory=temp_dir_local,
        pkg_id="blue",
    )

    bpy.ops.extensions.package_uninstall(
        repo_directory=temp_dir_local,
        pkg_id="blue",
    )

    preferences.extensions.repos.remove(repo)

    if VERBOSE:
        print("--- End ---")


def main() -> None:
    package_names = (
        "blue",
        "red",
        "green",
        "purple",
        "orange",
    )
    with tempfile.TemporaryDirectory(dir=TEMP_DIR_REMOTE) as temp_dir_remote:
        # Populate repository from source.
        for msg in command_output_from_json_0([
                "dummy-repo",
                "--repo-dir", temp_dir_remote,
                "--package-names", ",".join(package_names)
        ]):
            print(msg)

        with HTTPServerContext(
                directory=temp_dir_remote,
                port=HTTP_PORT,
                # Avoid error when running tests quickly,
                # sometimes the port isn't available yet.
                wait_tries=10,
                wait_delay=0.05,
        ):
            # Where we will put the files.
            with tempfile.TemporaryDirectory() as temp_dir_local:
                blender_test_run(temp_dir_local)

        if VERBOSE:
            with open(os.path.join(temp_dir_remote, PKG_REPO_LIST_FILENAME), 'r', encoding="utf-8") as fh:
                print(fh.read())

        # If we want to copy out these.
        # print(temp_dir_remote)
        # import time
        # time.sleep(540)


if __name__ == "__main__":
    main()
