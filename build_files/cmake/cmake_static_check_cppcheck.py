#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later

import project_source_info
import subprocess
import sys
import os
import tempfile

from typing import (
    Any,
    List,
)


USE_QUIET = (os.environ.get("QUIET", None) is not None)

CHECKER_IGNORE_PREFIX = [
    "extern",
]

CHECKER_BIN = "cppcheck"

CHECKER_ARGS = [
    # not sure why this is needed, but it is.
    "-I" + os.path.join(project_source_info.SOURCE_DIR, "extern", "glew", "include"),
    "--suppress=*:%s/extern/glew/include/GL/glew.h:241" % project_source_info.SOURCE_DIR,
    "--max-configs=1",  # speeds up execution
    #  "--check-config", # when includes are missing
    "--enable=all",  # if you want sixty hundred pedantic suggestions

    # Quiet output, otherwise all defines/includes are printed (overly verbose).
    # Only enable this for troubleshooting (if defines are not set as expected for example).
    "--quiet",

    # NOTE: `--cppcheck-build-dir=<dir>` is added later as a temporary directory.
]

if USE_QUIET:
    CHECKER_ARGS.append("--quiet")


def cppcheck() -> None:
    source_info = project_source_info.build_info(ignore_prefix_list=CHECKER_IGNORE_PREFIX)
    source_defines = project_source_info.build_defines_as_args()

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
        if not USE_QUIET:
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
