# SPDX-FileCopyrightText: 2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
This module generates a Python wheel (*.whl) for the purpose of running tests.
"""
__all__ = (
    "generate_from_file_data",
    "generate_from_source",
)

import os
import subprocess
import sys
import tempfile

from typing import (
    Callable,
    Dict,
    List,
    Tuple,
)


def _contents_to_filesystem(
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


def search_impl(directory: str, fn: Callable[[os.DirEntry[str]], bool], result: List[str]) -> None:
    for entry in os.scandir(directory):
        if entry.is_dir():
            search_impl(entry.path, fn, result)
        if fn(entry):
            result.append(entry.path)


def search(directory: str, fn: Callable[[os.DirEntry[str]], bool]) -> List[str]:
    result: List[str] = []
    search_impl(directory, fn, result)
    return result


def generate_from_file_data(
        *,
        module_name: str,
        version: str,
        package_contents: Dict[str, bytes],
) -> Tuple[str, bytes]:
    """
    :arg package_contents:
       The package contents.
       - The key is a path.
       - The value is file contents.

    Return filename & data.
    """

    setup_contents: Dict[str, bytes] = {
        "setup.py": """
from setuptools import setup

setup()
""".encode("utf-8"),
        "pyproject.toml": """
[build-system]
requires = ["setuptools >= 61.0"]
build-backend = "setuptools.build_meta"

[project]
name = "{:s}"
version = "{:s}"
dependencies = []

requires-python = ">=3.11"
authors = [
  {{name = "Developer Name", email = "name@example.com"}},
]
maintainers = [
  {{name = "Developer Name", email = "name@example.com"}}
]
description = "Dummy description."
keywords = ["egg", "bacon", "sausage", "tomatoes", "Lobster Thermidor"]
classifiers = [
  "Development Status :: 4 - Beta",
  "Programming Language :: Python"
]
""".format(module_name, version).encode("utf-8"),
    }

    with tempfile.TemporaryDirectory() as temp_dir:
        _contents_to_filesystem(package_contents, temp_dir)
        _contents_to_filesystem(setup_contents, temp_dir)

        output = subprocess.run(
            [sys.executable, "setup.py", "bdist_wheel"],
            cwd=temp_dir,
            stderr=subprocess.PIPE,
            stdout=subprocess.PIPE,
        )

        result = search(temp_dir, lambda entry: entry.name.endswith(".whl"))
        if len(result) != 1:
            print(output)
            raise Exception("failed to create wheel!")

        with open(result[0], 'rb') as fh:
            data = fh.read()

        filename = os.path.basename(result[0])

        return filename, data


def generate_from_source(
        *,
        module_name: str,
        version: str,
        source: str,
) -> Tuple[str, bytes]:
    """
    Return filename & data.
    """
    return generate_from_file_data(
        module_name=module_name,
        version=version,
        package_contents={
            "{:s}/__init__.py".format(module_name): source.encode("utf-8"),
        },
    )


if __name__ == "__main__":
    filename, data = generate_from_source(
        module_name="blender_example_module",
        version="0.0.1",
        source="print(\"Hello World\")"
    )
    print(filename, len(data))
