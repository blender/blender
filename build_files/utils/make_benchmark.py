#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
`make benchmark` helper

* Creates relevant directory if none exists and populates with BUILD_DIR binary
* Runs the default profile
"""

__all__ = {
    "main"
}

import argparse
import sys

from pathlib import Path

from make_utils import call
from make_update import floating_checkout_update


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("build_directory")
    parser.add_argument(
        "--git-command",
        default="git",
        help="Path to the git binary. (Only useful if it is not in your PATH)")
    return parser.parse_args()


def main() -> int:
    args = parse_arguments()

    msg = floating_checkout_update(
        args,
        "blender-benchmarks",
        Path("tests") / "benchmarks",
        "main",
    )

    if msg:
        sys.stderr.write("Unable to initialize / update 'benchmark' repository: {}".format(msg))
        return 1

    benchmark_dir = Path(__file__).absolute().parent.joinpath("benchmark")
    if not benchmark_dir.exists():
        build_dir = Path(args.build_directory)
        blender_bin = build_dir.joinpath("bin").absolute()

        if not blender_bin.exists():
            sys.stderr.write("blender `bin` directory not found, can't initialize benchmarks")
            return 1

        create_dir_command = ["./tests/performance/benchmark.py", "init", "--blender", blender_bin]
        exitcode = call(create_dir_command)
        if exitcode != 0:
            return exitcode

    run_command = ["./tests/performance/benchmark.py", "run", "default"]
    return call(run_command)


if __name__ == "__main__":
    sys.exit(main())
