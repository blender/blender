#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
Tool to inspect and automatically fix clang-tidy warnings and hints.

Run complete checks:
./tools/utils_maintenance/clang_tidy.py check -- source

Perform safe auto fixes:
./tools/utils_maintenance/clang_tidy.py fix --config=safe -- source

Run checks without auto fixing:
./tools/utils_maintenance/clang_tidy.py check --config=safe -- source
"""

__all__ = (
    "main",
)

import argparse
import contextlib
import multiprocessing
import pathlib
import sys
import subprocess
from collections.abc import (
    Sequence,
)
from concurrent.futures import (
    ProcessPoolExecutor,
)

base_dir = pathlib.Path(__file__).parent.parent.parent.resolve()

extensions = (
    ".c", ".cc", ".cpp", ".cxx",
    ".h", ".hh", ".hpp", ".hxx",
    ".m", ".mm",
)

# A subset of checks that are safe and useful to fix.
config_safe = """
Checks: >
  -*,
  modernize-min-max-use-initializer-list,
  modernize-redundant-void-arg,
  modernize-use-bool-literals,
  modernize-use-nullptr,
  modernize-use-override,
  modernize-use-starts-ends-with,
  readability-braces-around-statements,
  readability-container-contains,
  readability-duplicate-include,
  readability-qualified-auto,
  readability-redundant-inline-specifier
"""

# These sometimes seem to make mistakes, so need manual verification.
config_unsafe = """
Checks: >
  -*,
  modernize-deprecated-headers,
  modernize-use-equals-default,
  modernize-use-ranges,
  modernize-use-using,
  readability-redundant-casting,
  readability-use-std-min-max
"""

# Config to fix automatically add const. This requires some manual fixes due to wrong
# changes with functions pointers. And to avoid changing the existing const qualifier
# order it can be used as follows:
#
#   echo "QualifierAlignment: Left" >> .clang-format
#   make format
#   git commit source
#
#   ./tools/utils_maintenance/clang_tidy.py fix --config=const -- source
#   make format
#   git commit source
#
#   git checkout .clang-format
#   git revert HEAD~1
#
config_const = """
Checks: >
  -*,
  misc-const-correctness,
  readability-non-const-parameter,

CheckOptions:
  - key: misc-const-correctness.WarnPointersAsPointers
    value: 1
  - key: misc-const-correctness.TransformPointersAsPointers
    value: 1
"""

# Config to remove unused includes.
# This can easily break with different platforms and build options, and also
# requires LLVM version 23 or newer to support disabling MissingIncludes.
config_includes = """
Checks: >
  -*,
  misc-include-cleaner

CheckOptions:
  - key: misc-include-cleaner.MissingIncludes
    value: 0
  - key: misc-include-cleaner.UnusedIncludes
    value: 1
"""

configs = {
    "complete": None,
    "safe": config_safe,
    "unsafe": config_unsafe,
    "const": config_const,
    "includes": config_includes,
}


def source_files_from_git(paths: Sequence[str]) -> list[str]:
    cmd = ("git", "ls-tree", "-r", "HEAD", *paths, "--name-only", "-z")
    try:
        files_bytes = subprocess.check_output(cmd, cwd=base_dir).split(b"\0")
    except subprocess.CalledProcessError:
        return []
    files = [f.decode("utf-8") for f in files_bytes if f]
    return [f for f in files if f.endswith(extensions)]


def process_file(
    file_path: str,
    tidy_config: str | None,
    fix: bool,
    compile_commands_dir: str,
    done: multiprocessing.managers.ValueProxy[int],
    lock: contextlib.AbstractContextManager[bool],
    total: int,
) -> None:
    # Progress display.
    with lock:
        done.value += 1
        progress_text = f"[{done.value}/{total}] {file_path}"
        if sys.stdout.isatty():
            sys.stdout.write(f"\r{progress_text}\033[K")
        else:
            sys.stdout.write(f"{progress_text}\n")
        sys.stdout.flush()

    cmd = ["clang-tidy", "-p", compile_commands_dir]
    if tidy_config:
        cmd.append(f"--config={tidy_config}")
    if fix:
        cmd.append("--fix")
    cmd.append("--quiet")
    cmd.append(file_path)

    # Ignore errors as clang-tidy returns non-zero for irrelevant errors.
    if fix:
        subprocess.run(
            cmd,
            check=False,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            cwd=base_dir,
        )
    else:
        subprocess.run(cmd, check=False, cwd=base_dir)


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Run clang-tidy on files.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    # Show full help instead of short usage on argument errors.
    parser.print_usage = parser.print_help  # type: ignore[method-assign]
    parser.add_argument(
        "command",
        choices=["check", "fix"],
        help="Check or auto-fix",
    )
    parser.add_argument(
        "--config",
        choices=configs.keys(),
        default="complete",
        help="Config to use, see the description for details",
    )
    parser.add_argument(
        "--compile-commands-dir",
        default=str(base_dir),
        help="Directory containing compile_commands.json (default: project root)",
    )
    parser.add_argument(
        "paths",
        nargs="+",
        help="Files or directories to process.",
    )

    args = parser.parse_args()

    compile_commands = pathlib.Path(args.compile_commands_dir) / "compile_commands.json"
    if not compile_commands.exists():
        sys.stderr.write(f"Error: {compile_commands} not found\n")
        sys.stderr.write("Enable CMAKE_EXPORT_COMPILE_COMMANDS or use --compile-commands-dir=/build/dir/\n")
        sys.exit(1)

    if args.command == "fix" and args.config == "complete":
        parser.error("The 'complete' configuration cannot be used with the 'fix' command.")

    files = source_files_from_git(args.paths)
    if not files:
        print("No files found to process.")
        return

    jobs = multiprocessing.cpu_count()
    total_files = len(files)

    with multiprocessing.Manager() as mpm:
        done = mpm.Value("i", 0)
        lock = mpm.Lock()

        with ProcessPoolExecutor(max_workers=jobs) as executor:
            futures = [
                executor.submit(
                    process_file,
                    f,
                    configs[args.config],
                    args.command == "fix",
                    args.compile_commands_dir,
                    done,
                    lock,
                    total_files,
                )
                for f in files
            ]
            for future in futures:
                future.result()

    print("\nDone.")


if __name__ == "__main__":
    main()
