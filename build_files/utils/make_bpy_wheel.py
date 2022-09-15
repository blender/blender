#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later

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
    Tuple,
    List,
    Optional,
    Sequence,
)


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
        print("Unable to find %r exiting!" % var)
        sys.exit(1)
    return value


# ------------------------------------------------------------------------------
# Main Function

def main() -> None:

    # Parse arguments.
    parser = argparse.ArgumentParser(description="Make Python wheel package")
    parser.add_argument("install_dir")
    parser.add_argument("--build-dir", default=None)
    parser.add_argument("--output-dir", default=None)
    args = parser.parse_args()

    install_dir = os.path.abspath(args.install_dir)
    build_dir = os.path.abspath(args.build_dir) if args.build_dir else install_dir
    output_dir = os.path.abspath(args.output_dir) if args.output_dir else install_dir

    filepath_cmake_cache = find_dominating_file(build_dir, ("CMakeCache.txt",))
    if not filepath_cmake_cache:
        # Should never fail.
        print("Unable to find CMakeCache.txt in or above %r" % (build_dir))
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
        glibc = os.confstr("CS_GNU_LIBC_VERSION").split()[1].split(".")
        platform_tag = "manylinux_%s_%s_%s" % (glibc[0], glibc[1], platform.machine().lower())
    else:
        print("Unsupported platform %s" % (sys.platform))
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
        def has_ext_modules(foo):
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
            # No apparent way to override this ABI version with setuptools, so rename.
            sys_py = "cp%d%d" % (sys.version_info.major, sys.version_info.minor)
            blender_py = "cp%d%d" % (python_version_number[0], python_version_number[1])
            renamed_f = f.replace(sys_py, blender_py)

            os.rename(os.path.join(dist_dir, f), os.path.join(output_dir, renamed_f))


if __name__ == "__main__":
    main()
