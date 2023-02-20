#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later

"""
Make Python wheel package (`*.whl`) file from Blender built with 'WITH_PYTHON_MODULE' enabled.

Example
=======

If the "bpy" module was build on Linux using the command:

   make bpy lite

The command to package it as a wheel is:

   ./build_files/utils/make_bpy_wheel.py ../build_linux_bpy_lite/bin --output-dir=./

This will create a `*.whl` file in the current directory.
"""

import argparse
import make_utils
import os
import re
import platform
import string
import setuptools
import sys

from typing import (
    Generator,
    List,
    Optional,
    Sequence,
    Tuple,
)

# ------------------------------------------------------------------------------
# Long Description

long_description = """# Blender

[Blender](https://www.blender.org) is the free and open source 3D creation suite. It supports the entirety of the 3D pipeline—modeling, rigging, animation, simulation, rendering, compositing and motion tracking, even video editing.

This package provides Blender as a Python module for use in studio pipelines, web services, scientific research, and more.

## Documentation

* [Blender Python API](https://docs.blender.org/api/current/)
* [Blender as a Python Module](https://docs.blender.org/api/current/info_advanced_blender_as_bpy.html)

## Requirements

[System requirements](https://www.blender.org/download/requirements/) are the same as Blender.

Each Blender release supports one Python version, and the package is only compatible with that version.

## Source Code

* [Releases](https://download.blender.org/source/)
* Repository: [projects.blender.org/blender/blender.git](https://projects.blender.org/blender/blender)

## Credits

Created by the [Blender developer community](https://www.blender.org/about/credits/).

Thanks to Tyler Alden Gubala for maintaining the original version of this package."""

# ------------------------------------------------------------------------------
# Generic Functions


def find_dominating_file(
    path: str,
    search: Sequence[str],
) -> str:
    while True:
        for d in search:
            if os.path.exists(os.path.join(path, d)):
                return os.path.join(path, d)
        path_next = os.path.normpath(os.path.join(path, ".."))
        if path == path_next:
            break
        path = path_next
    return ""


# ------------------------------------------------------------------------------
# CMake Cache Access

def cmake_cache_var_iter(filepath_cmake_cache: str) -> Generator[Tuple[str, str, str], None, None]:
    import re
    re_cache = re.compile(r"([A-Za-z0-9_\-]+)?:?([A-Za-z0-9_\-]+)?=(.*)$")
    with open(filepath_cmake_cache, "r", encoding="utf-8") as cache_file:
        for l in cache_file:
            match = re_cache.match(l.strip())
            if match is not None:
                var, type_, val = match.groups()
                yield (var, type_ or "", val)


def cmake_cache_var(filepath_cmake_cache: str, var: str) -> Optional[str]:
    for var_iter, type_iter, value_iter in cmake_cache_var_iter(filepath_cmake_cache):
        if var == var_iter:
            return value_iter
    return None


def cmake_cache_var_or_exit(filepath_cmake_cache: str, var: str) -> str:
    value = cmake_cache_var(filepath_cmake_cache, var)
    if value is None:
        sys.stderr.write("Unable to find %r in %r, abort!\n" % (var, filepath_cmake_cache))
        sys.exit(1)
    return value


# ------------------------------------------------------------------------------
# Argument Parser

def argparse_create() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawTextHelpFormatter)
    parser.add_argument(
        "install_dir",
        metavar='INSTALL_DIR',
        type=str,
        help="The installation directory containing the \"bpy\" package.",
    )
    parser.add_argument(
        "--build-dir",
        metavar='BUILD_DIR',
        default=None,
        help="The build directory containing 'CMakeCache.txt' (search parent directories of INSTALL_DIR when omitted).",
        required=False,
    )
    parser.add_argument(
        "--output-dir",
        metavar='OUTPUT_DIR',
        default=None,
        help="The destination directory for the '*.whl' file (use INSTALL_DIR when omitted).",
        required=False,
    )

    return parser


# ------------------------------------------------------------------------------
# Main Function

def main() -> None:

    # Parse arguments.
    args = argparse_create().parse_args()

    install_dir = os.path.abspath(args.install_dir)
    output_dir = os.path.abspath(args.output_dir) if args.output_dir else install_dir

    if args.build_dir:
        build_dir = os.path.abspath(args.build_dir)
        filepath_cmake_cache = os.path.join(build_dir, "CMakeCache.txt")
        del build_dir
        if not os.path.exists(filepath_cmake_cache):
            sys.stderr.write("File not found %r, abort!\n" % filepath_cmake_cache)
            sys.exit(1)
    else:
        filepath_cmake_cache = find_dominating_file(install_dir, ("CMakeCache.txt",))
        if not filepath_cmake_cache:
            # Should never fail.
            sys.stderr.write("Unable to find CMakeCache.txt in or above %r, abort!\n" % install_dir)
            sys.exit(1)

    # Get the major and minor Python version.
    python_version = cmake_cache_var_or_exit(filepath_cmake_cache, "PYTHON_VERSION")
    python_version_number = (
        tuple(int("".join(c for c in digit if c in string.digits)) for digit in python_version.split(".")) +
        # Support version without a minor version "3" (add zero).
        tuple((0, 0, 0))
    )
    python_version_str = "%d.%d" % python_version_number[:2]

    # Get Blender version.
    blender_version_str = str(make_utils.parse_blender_version())

    # Set platform tag following conventions.
    if sys.platform == "darwin":
        target = cmake_cache_var_or_exit(filepath_cmake_cache, "CMAKE_OSX_DEPLOYMENT_TARGET").split(".")
        machine = cmake_cache_var_or_exit(filepath_cmake_cache, "CMAKE_OSX_ARCHITECTURES")
        platform_tag = "macosx_%d_%d_%s" % (int(target[0]), int(target[1]), machine)
    elif sys.platform == "win32":
        platform_tag = "win_%s" % (platform.machine().lower())
    elif sys.platform == "linux":
        glibc = os.confstr("CS_GNU_LIBC_VERSION")
        if glibc is None:
            sys.stderr.write("Unable to find \"CS_GNU_LIBC_VERSION\", abort!\n")
            sys.exit(1)
        glibc = "%s_%s" % tuple(glibc.split()[1].split(".")[:2])
        platform_tag = "manylinux_%s_%s" % (glibc, platform.machine().lower())
    else:
        sys.stderr.write("Unsupported platform: %s, abort!\n" % (sys.platform))
        sys.exit(1)

    os.chdir(install_dir)

    # Include all files recursively.
    def package_files(root_dir: str) -> List[str]:
        paths = []
        for path, dirs, files in os.walk(root_dir):
            paths += [os.path.join("..", path, f) for f in files]
        return paths

    # Ensure this wheel is marked platform specific.
    class BinaryDistribution(setuptools.dist.Distribution):
        def has_ext_modules(self) -> bool:
            return True

    # Build wheel.
    sys.argv = [sys.argv[0], "bdist_wheel"]

    setuptools.setup(
        name="bpy",
        version=blender_version_str,
        install_requires=["cython", "numpy", "requests", "zstandard"],
        python_requires="==%d.%d.*" % (python_version_number[0], python_version_number[1]),
        packages=["bpy"],
        package_data={"": package_files("bpy")},
        distclass=BinaryDistribution,
        options={"bdist_wheel": {"plat_name": platform_tag}},

        description="Blender as a Python module",
        long_description=long_description,
        long_description_content_type='text/markdown',
        license="GPL-3.0",
        author="Blender Foundation",
        author_email="bf-committers@blender.org",
        url="https://www.blender.org"
    )

    if not os.path.exists(output_dir):
        os.makedirs(output_dir)

    # Move wheel to output directory.
    dist_dir = os.path.join(install_dir, "dist")
    for f in os.listdir(dist_dir):
        if f.endswith(".whl"):
            blender_py = "cp%d%d" % (python_version_number[0], python_version_number[1])

            # No apparent way to override this ABI version with setuptools, so rename.
            sys_py = "cp%d%d" % (sys.version_info.major, sys.version_info.minor)
            if hasattr(sys, "abiflags"):
                sys_py_abi = sys_py + sys.abiflags
                renamed_f = f.replace(sys_py_abi, blender_py).replace(sys_py, blender_py)
            else:
                renamed_f = f.replace(sys_py, blender_py)

            os.rename(os.path.join(dist_dir, f), os.path.join(output_dir, renamed_f))


if __name__ == "__main__":
    main()
