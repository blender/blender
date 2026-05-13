#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
Type-check Blender Python examples and templates against generated stubs.
Each file is checked in a separate MYPY process, running in parallel.

NOTE(@ideasman42): we are nowhere near close to having Blender scripts type check without any type warnings.
This is mainly as a way to check:

- The stubs are valid can be loaded into MYPY.
- The stubs are working as expected,
  since errors in the stubs *do* point to errors in the RST documentation.

However, it is not as a way to ensure we have zero typing errors,
as there are too many false positives.
"""

import argparse
import multiprocessing
import os
import subprocess
import sys
from pathlib import Path

# Project root derived from this file's location (doc/python_api/).
SOURCE_DIR = Path(__file__).resolve().parents[2]

STUB_DIR = SOURCE_DIR / "doc" / "python_api" / "stubs"

SKIP = {
    "doc/python_api/examples/aud.0.py",
    "doc/python_api/examples/bpy.types.HydraRenderEngine.py",
    "scripts/templates_py/ui_list_generic.py",
}


def check_file(filepath: str) -> tuple[str, str]:
    """Run mypy on a single file, return (filepath, error_output)."""
    env = os.environ.copy()
    env["MYPYPATH"] = str(STUB_DIR)
    result = subprocess.run(
        [
            sys.executable, "-m", "mypy", filepath,
            "--no-error-summary",
            "--explicit-package-bases",
        ],
        capture_output=True,
        text=True,
        env=env,
    )
    prefix = filepath + ":"
    lines = [
        line for line in result.stdout.splitlines()
        if line.startswith(prefix)
        # Mix-in classes that narrow `bl_*` Literal attributes cause diamond
        # inheritance conflicts - a `mypy` limitation, not a stub bug.
        and "incompatible with definition in base class" not in line
    ]
    return filepath, "\n".join(lines)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "-j", "--jobs", type=int, default=0,
        help=(
            "Parallel jobs (default 0 uses CPU count; 1 runs synchronously, "
            "streaming each mypy invocation's output directly to the terminal)."
        ),
    )
    args = parser.parse_args()

    os.chdir(SOURCE_DIR)

    # `Path.as_posix()` normalizes backslashes for WIN32 so SKIP paths match.
    all_files = [
        path.as_posix() for pattern in (
            "doc/python_api/examples/*.py",
            "scripts/templates_py/*.py",
            "tests/python/*.py",
            "scripts/modules/**/*.py",
            "scripts/startup/**/*.py",
            "scripts/addons_core/**/*.py",
        ) for path in Path().glob(pattern)
    ]
    files = sorted(f for f in all_files if f not in SKIP)

    jobs = args.jobs
    if jobs <= 0:
        jobs = multiprocessing.cpu_count()

    errors = 0

    if jobs == 1:
        # Synchronous: print each file's result as soon as it's ready.
        for filepath in files:
            _, output = check_file(filepath)
            if output:
                errors += 1
                print(output)
                print()
    else:
        results: dict[str, str] = {}
        with multiprocessing.Pool(jobs) as pool:
            for filepath, output in pool.imap_unordered(check_file, files):
                results[filepath] = output

        # Print results in file order.
        for filepath in files:
            output = results[filepath]
            if output:
                errors += 1
                print(output)
                print()

    print("Checked {:d} files, {:d} with errors.".format(len(files), errors))


if __name__ == "__main__":
    main()
