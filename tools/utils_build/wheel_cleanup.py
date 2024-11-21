#!/usr/bin/env python3

# Script which cleans up libraries for the bpy module.
#
# It scans actual dependencies of the bpy module and its dependencies and removes libraries that
# are not needed. The libraries that are needed are ensured to be regular files (not a symbolic
# link).
#
# The goal is to prepare the bpy install directory for the use by the wheel packaging tool: since
# wheels don't support symbolic links leaving them in the install folder will result in big
# resulting file sizes.
#
# Doing cleanup as a dedicated step allows to run all sort of regression tests before the wheel is
# packed.
#
# Usage:
#   wheel_cleanup.py <path/to/installed/bpy/folder>

import argparse
import re
import shutil
import subprocess
import sys
from pathlib import Path
from string import digits
from typing import Generator


# Regex matchers for libraries that might be seen in the libs folder, not directly referenced, but
# yet are still required for the proper operation of the bpy module.
KEEP_MATCHERS = (
    # libOpenImageDenoise.so loads core, device_cuda, etc libraries at runtime.
    re.compile("libOpenImageDenoise_.*"),
)


def print_banner(text: str) -> None:
    print("")
    print(text)
    print("=" * len(text))
    print("")


def print_stage(text: str) -> None:
    print("")
    print(text)
    print("-" * len(text))
    print("")


def get_direct_elf_dependencies(elf: Path) -> set[str]:
    """
    Get direct dependencies of the given library or executable in ELF format

    Uses readelf command and parses its output.
    """
    output = subprocess.check_output(("readelf", "-d", elf))
    deps = set()
    for line_bytes in output.splitlines():
        # Example of a line from the readelf command:
        # b' 0x0000000000000001 (NEEDED)             Shared library: [libgcc_s.so.1]'
        line = line_bytes.decode()
        if "(NEEDED)" not in line:
            continue
        if "Shared library:" not in line:
            continue
        lib_quoted = line.split("Shared library: ")[1]
        lib = lib_quoted.removeprefix("[").removesuffix("]")
        deps.add(lib)
    return deps


def name_strip_abi_suffix(name: str) -> str:
    """
    Strip any ABI suffix from the given file name

    For example: libfoo.so.1.2-3 -> linfoo.so
    """

    while name[-1] in digits:
        new_name = name.rstrip(digits)
        if new_name[-1] in (".", "-"):
            name = new_name[:-1]
        else:
            break
    return name


def name_is_so(name: str) -> bool:
    """
    Return true if the given name is an .so library

    Ignores any possible ABI specification.
    This is purely lexicographical operation.
    """
    clean_name = name_strip_abi_suffix(name)
    return clean_name.endswith(".so")


def iter_so_in_dir(parent_dir: Path) -> Generator[Path, None, None]:
    """
    Iterate .so files (with ABI variants) in the given directory

    The file are yielded from this generator.
    """
    for lib_filepath in parent_dir.iterdir():
        if not lib_filepath.is_file():
            continue
        if not name_is_so(lib_filepath.name):
            continue
        yield lib_filepath


def resolve_symlink(filepath: Path) -> Path:
    """
    Resolve symbolic link

    Recursively follows the resolution.

    NOTE: Does not support cyclic symbolic links
    """

    if not filepath.is_symlink():
        return filepath

    link = Path(filepath.readlink())
    return resolve_symlink(filepath.parent / link)


def make_real(filepath: Path) -> None:
    """
    Make the given file real by resolving symbolic link
    """

    print(f"Making {filepath} real")
    if not filepath.is_symlink():
        print(f"{filepath} is not a link")
        return

    resolved = resolve_symlink(filepath)

    print(f"Resolved to {resolved}")

    filepath.unlink()
    shutil.copy2(resolved, filepath)


def library_matches_keep_pattern(lib_filepath: Path) -> bool:
    """
    Returns true if the library matches any pattern that requires it to be kept
    """
    for matcher in KEEP_MATCHERS:
        if matcher.match(lib_filepath.name):
            return True
    return False


def cleanup_linux(bpy_dir: Path) -> None:
    print_stage("Gathering dependencies")
    deps = get_direct_elf_dependencies(bpy_dir / "__init__.so")
    print(f"- __init__.so depends on {deps}")
    # TODO(sergey): Can do something smarter like actual recursive dependency tracing.
    for lib_filepath in iter_so_in_dir(bpy_dir / "lib"):
        lib_deps = deps.union(get_direct_elf_dependencies(lib_filepath))
        print(f"- {lib_filepath} depends on {lib_deps}")
        deps = deps.union(lib_deps)

    print_stage("Cleaning up")
    deps_to_remove = []
    for lib_filepath in iter_so_in_dir(bpy_dir / "lib"):
        if lib_filepath.name in deps:
            print(f"Keeping dependency {lib_filepath}")
            make_real(lib_filepath)
            continue

        if library_matches_keep_pattern(lib_filepath):
            print(f"Keeping dependency {lib_filepath} as per static rules")
            continue

        deps_to_remove.append(lib_filepath)
        print(f"Will remove unused dependency {lib_filepath}")

    print_stage("Removing unused dependencies")
    for dep_to_remove in deps_to_remove:
        print(f"Removing unused dependency {dep_to_remove}")
        dep_to_remove.unlink()


def main() -> None:
    print_banner("BPY module libraries cleaner")

    parser = argparse.ArgumentParser()
    parser.add_argument("bpy_dir", type=Path, help="Path to the bpy directory to cleanup")
    args = parser.parse_args()

    bpy_dir = Path(sys.argv[1])

    if sys.platform == "linux":
        cleanup_linux(bpy_dir)
        return

    # Windows and macOS do not use symlinks for libraries, so no need to figure out which copies of
    # the same library with different ABI can be removed.
    print("The wheel cleanup script is only intended to be used on Linux")


if __name__ == "__main__":
    main()
