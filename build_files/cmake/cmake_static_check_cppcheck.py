#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2011-2022 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import project_source_info
import subprocess
import sys
import os
import tempfile

from typing import (
    Any,
    List,
    Tuple,
)

USE_VERBOSE = (os.environ.get("VERBOSE", None) is not None)
# Could make configurable.
USE_VERBOSE_PROGRESS = True

CHECKER_BIN = "cppcheck"

CHECKER_IGNORE_PREFIX = [
    "extern",
]

# To add files use a relative path.
CHECKER_EXCLUDE_SOURCE_FILES = set(os.path.join(*f.split("/")) for f in (
    # "source/blender/example.c",
))

CHECKER_ARGS = [
    # Speed up execution.
    # As Blender has many defines, the total number of configurations is large making execution unreasonably slow.
    # This could be increased but do so with care.
    "--max-configs=1",

    # Enable this when includes are missing.
    #  "--check-config",

    # Shows many pedantic issues, some are quite useful.
    "--enable=all",

    # Also shows useful messages, even if some are false-positives.
    "--inconclusive",

    # Quiet output, otherwise all defines/includes are printed (overly verbose).
    # Only enable this for troubleshooting (if defines are not set as expected for example).
    *(() if USE_VERBOSE else ("--quiet",))

    # NOTE: `--cppcheck-build-dir=<dir>` is added later as a temporary directory.
]


def source_info_filter(
        source_info: List[Tuple[str, List[str], List[str]]],
) -> List[Tuple[str, List[str], List[str]]]:
    source_dir = project_source_info.SOURCE_DIR
    if not source_dir.endswith(os.sep):
        source_dir += os.sep
    source_info_result = []
    for i, item in enumerate(source_info):
        c = item[0]
        if c.startswith(source_dir):
            c_relative = c[len(source_dir):]
            if c_relative in CHECKER_EXCLUDE_SOURCE_FILES:
                CHECKER_EXCLUDE_SOURCE_FILES.remove(c_relative)
                continue
        source_info_result.append(item)
    if CHECKER_EXCLUDE_SOURCE_FILES:
        sys.stderr.write("Error: exclude file(s) are missing: %r\n" % list(sorted(CHECKER_EXCLUDE_SOURCE_FILES)))
        sys.exit(1)
    return source_info_result


def cppcheck() -> None:
    source_info = project_source_info.build_info(ignore_prefix_list=CHECKER_IGNORE_PREFIX)
    source_defines = project_source_info.build_defines_as_args()

    # Apply exclusion.
    source_info = source_info_filter(source_info)

    check_commands = []
    for c, inc_dirs, defs in source_info:
        cmd = (
            [CHECKER_BIN] +
            CHECKER_ARGS +
            [c] +
            [("-I%s" % i) for i in inc_dirs] +
            [("-D%s" % d) for d in defs] +
            source_defines
        )

        check_commands.append((c, cmd))

    process_functions = []

    def my_process(i: int, c: str, cmd: List[str]) -> subprocess.Popen[Any]:
        if USE_VERBOSE_PROGRESS:
            percent = 100.0 * (i / len(check_commands))
            percent_str = "[" + ("%.2f]" % percent).rjust(7) + " %:"

            sys.stdout.flush()
            sys.stdout.write("%s %s\n" % (
                percent_str,
                os.path.relpath(c, project_source_info.SOURCE_DIR)
            ))

        return subprocess.Popen(cmd)

    for i, (c, cmd) in enumerate(check_commands):
        process_functions.append((my_process, (i, c, cmd)))

    project_source_info.queue_processes(process_functions)

    print("Finished!")


def main() -> None:
    with tempfile.TemporaryDirectory() as temp_dir:
        CHECKER_ARGS.append("--cppcheck-build-dir=" + temp_dir)
        cppcheck()


if __name__ == "__main__":
    main()
