#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2011-2022 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
Run CPPCHECK on Blender's source files,
writing results to a log as well as a summary of all checks.

Existing logs are renamed to ``.old.log`` so they can be compared.
"""

__all__ = (
    "main",
)

import argparse
import project_source_info
import subprocess
import sys
import os
import re
import tempfile
import time

from typing import (
    Any,
    IO,
)

USE_VERBOSE = (os.environ.get("VERBOSE", None) is not None)
# Could make configurable.
USE_VERBOSE_PROGRESS = True

CHECKER_BIN = "cppcheck"

CHECKER_IGNORE_PREFIX = [
    "extern",
]

# Optionally use a separate build dir for each source code directory.
# According to CPPCHECK docs using one directory is a way to take advantage of "whole program" checks,
# although it looks as if there might be name-space issues - overwriting files with similar names across
# different parts of the source.
CHECKER_ISOLATE_BUILD_DIR = False

CHECKER_EXCLUDE_SOURCE_FILES_EXT = (
    # Exclude generated shaders, harmless but also not very useful and are quite slow.
    ".glsl.c",
)

# To add files use a relative path.
CHECKER_EXCLUDE_SOURCE_FILES = set(os.path.join(*f.split("/")) for f in (
    "source/blender/draw/engines/eevee/eevee_lut.cc",
    # Hangs for hours CPPCHECK-2.14.0.
    "intern/cycles/blender/output_driver.cpp",
))


CHECKER_EXCLUDE_SOURCE_DIRECTORIES_BUILD = set(os.path.join(*f.split("/")) + os.sep for f in (
    # Exclude data-files, especially `datatoc` as the files can be large & are slow to scan.
    "release/datafiles",
    # Exclude generated RNA, harmless but also not very useful and are quite slow.
    "source/blender/makesrna/intern",
    # Exclude generated WAYLAND protocols.
    "intern/ghost/libwayland"
))

CHECKER_ARGS = (
    # Speed up execution.
    # As Blender has many defines, the total number of configurations is large making execution unreasonably slow.
    # This could be increased but do so with care.
    "--max-configs=1",

    # Enable this when includes are missing.
    #  `"--check-config",`

    # May be interesting to check on increasing this for better results:
    # `"--max-ctu-depth=2",`

    # This is slower, for a comprehensive output it is needed.
    "--check-level=exhaustive",

    # Shows many pedantic issues, some are quite useful.
    "--enable=all",

    # Tends to give many false positives, could investigate if there are any ways to resolve, for now it's noisy.
    "--disable=unusedFunction",

    # Also shows useful messages, even if some are false-positives.
    "--inconclusive",

    # Generates many warnings, CPPCHECK known about system includes without resolving them.
    # To get a list of these use:
    # `cppcheck --errorlist | pcregrep --only-matching "error id=\"[a-zA-Z_0-9]+\""`
    *("--suppress={:s}".format(s) for s in (
        # Noisy, and we can't always avoid this.
        "missingIncludeSystem",
        # Typically these can't be made `const`.
        "constParameterCallback",

        # Overly noisy, we could consider resolving all of these at some point.
        "cstyleCast",

        # Calling `memset` of float may technically be a bug but works in practice.
        "memsetClassFloat",
        # There are various classes which don't have copy or equal constructors (GHOST windows for example)
        "noCopyConstructor",
        # Also noisy, looks like these are not issues to "solve".
        "unusedFunction",
        # There seems to be many false positives here.
        "unusedPrivateFunction",
        # May be interesting to handle but very noisy currently.
        "variableScope",
        # TODO: consider enabling this, more of a preference,
        # not using STL algorithm's doesn't often hint at actual errors.
        "useStlAlgorithm",
        # TODO: consider enabling this, currently noisy and we are not likely to resolve them short term.
        "functionStatic",

        # These could be added back, currently there are so many warnings and they don't seem especially error-prone.
        "missingMemberCopy",
        "missingOverride",
        "noExplicitConstructor",
        "uninitDerivedMemberVar",
        "uninitDerivedMemberVarPrivate",
        "uninitMemberVar",
        "useInitializationList",
    )),

    # Quiet output, otherwise all defines/includes are printed (overly verbose).
    # Only enable this for troubleshooting (if defines are not set as expected for example).
    *(() if USE_VERBOSE else ("--quiet",))

    # NOTE: `--cppcheck-build-dir=<dir>` is added later as a temporary directory.
)

CHECKER_ARGS_C = (
    "--std=c11",
)

CHECKER_ARGS_CXX = (
    "--std=c++17",
)

# NOTE: it seems we can't exclude these from CPPCHECK directly (from what I can see)
# so exclude them from the summary.
CHECKER_EXCLUDE_FROM_SUMMARY = {
    # Not considered an error.
    "allocaCalled",
    # Similar for `noCopyConstructor`.
    "nonoOperatorEq",
}


def source_info_filter(
        source_info: list[tuple[str, list[str], list[str]]],
        source_dir: str,
        cmake_dir: str,
) -> list[tuple[str, list[str], list[str]]]:
    source_dir = source_dir.rstrip(os.sep) + os.sep
    cmake_dir = cmake_dir.rstrip(os.sep) + os.sep

    cmake_dir_prefix_tuple = tuple(CHECKER_EXCLUDE_SOURCE_DIRECTORIES_BUILD)

    source_info_result = []
    for i, item in enumerate(source_info):
        c = item[0]

        if c.endswith(*CHECKER_EXCLUDE_SOURCE_FILES_EXT):
            continue

        if c.startswith(source_dir):
            c_relative = c[len(source_dir):]
            if c_relative in CHECKER_EXCLUDE_SOURCE_FILES:
                CHECKER_EXCLUDE_SOURCE_FILES.remove(c_relative)
                continue
        elif c.startswith(cmake_dir):
            c_relative = c[len(cmake_dir):]
            if c_relative.startswith(cmake_dir_prefix_tuple):
                continue

        # TODO: support filtering on filepath.
        # if "/editors/mask" not in c:
        #     continue
        source_info_result.append(item)
    if CHECKER_EXCLUDE_SOURCE_FILES:
        sys.stderr.write(
            "Error: exclude file(s) are missing: {!r}\n".format(list(sorted(CHECKER_EXCLUDE_SOURCE_FILES)))
        )
        sys.exit(1)
    return source_info_result


def cppcheck(cppcheck_dir: str, temp_dir: str, log_fh: IO[bytes]) -> None:
    temp_source_dir = os.path.join(temp_dir, "source")
    os.mkdir(temp_source_dir)
    del temp_dir

    source_dir = os.path.normpath(os.path.abspath(project_source_info.SOURCE_DIR))
    cmake_dir = os.path.normpath(os.path.abspath(project_source_info.CMAKE_DIR))

    cppcheck_build_dir = os.path.join(cppcheck_dir, "build")
    os.makedirs(cppcheck_build_dir, exist_ok=True)

    source_info = project_source_info.build_info(ignore_prefix_list=CHECKER_IGNORE_PREFIX)
    cppcheck_compiler_h = os.path.join(temp_source_dir, "cppcheck_compiler.h")
    with open(cppcheck_compiler_h, "w", encoding="utf-8") as fh:
        fh.write(project_source_info.build_defines_as_source())

        # Add additional defines.
        fh.write("\n")
        # Python's `pyport.h` errors without this.
        fh.write("#define UCHAR_MAX 255\n")
        # `intern/atomic/intern/atomic_ops_utils.h` errors with `Cannot find int size` without this.
        fh.write("#define UINT_MAX 0xFFFFFFFF\n")

    # Apply exclusion.
    source_info = source_info_filter(source_info, source_dir, cmake_dir)

    check_commands = []
    for c, inc_dirs, defs in source_info:
        if c.endswith(".c"):
            checker_args_extra = CHECKER_ARGS_C
        else:
            checker_args_extra = CHECKER_ARGS_CXX

        if CHECKER_ISOLATE_BUILD_DIR:
            build_dir_for_source = os.path.relpath(os.path.dirname(os.path.normpath(os.path.abspath(c))), source_dir)
            build_dir_for_source = os.sep + build_dir_for_source + os.sep
            build_dir_for_source = build_dir_for_source.replace(
                os.sep + ".." + os.sep,
                os.sep + "__" + os.sep,
            ).strip(os.sep)

            build_dir_for_source = os.path.join(cppcheck_build_dir, build_dir_for_source)

            os.makedirs(build_dir_for_source, exist_ok=True)
        else:
            build_dir_for_source = cppcheck_build_dir

        cmd = (
            CHECKER_BIN,
            *CHECKER_ARGS,
            *checker_args_extra,
            "--cppcheck-build-dir=" + build_dir_for_source,
            "--include=" + cppcheck_compiler_h,
            # NOTE: for some reason failing to include this crease a large number of syntax errors
            # from `intern/guardedalloc/MEM_guardedalloc.h`. Include directly to resolve.
            "--include={:s}".format(os.path.join(source_dir, "source", "blender", "blenlib", "BLI_compiler_attrs.h")),
            c,
            *[("-I{:s}".format(i)) for i in inc_dirs],
            *[("-D{:s}".format(d)) for d in defs],
        )

        check_commands.append((c, cmd))

    process_functions = []

    def my_process(i: int, c: str, cmd: list[str]) -> subprocess.Popen[Any]:
        del c
        proc = subprocess.Popen(
            cmd,
            stderr=subprocess.PIPE,
            stdout=subprocess.PIPE,
        )

        # A bit dirty, but simplifies logic to read these back later.
        proc.my_index = i  # type: ignore
        proc.my_time = time.time()  # type: ignore

        return proc

    for i, (c, cmd) in enumerate(check_commands):
        process_functions.append((my_process, (i, c, cmd)))

    index_current = 0
    index_count = 0
    proc_results_by_index: dict[int, tuple[bytes, bytes]] = {}

    def process_finalize(
            proc: subprocess.Popen[Any],
            stdout: bytes,
            stderr: bytes,
    ) -> None:
        nonlocal index_current, index_count
        index_count += 1

        assert hasattr(proc, "my_index")
        index = proc.my_index
        assert hasattr(proc, "my_time")
        time_orig = proc.my_time

        c = check_commands[index][0]

        time_delta = time.time() - time_orig
        if USE_VERBOSE_PROGRESS:
            percent = 100.0 * (index_count / len(check_commands))
            sys.stdout.flush()
            sys.stdout.write("[{:s}] %: {:s} ({:.2f})\n".format(
                ("{:.2f}".format(percent)).rjust(6),
                os.path.relpath(c, source_dir),
                time_delta,
            ))

        while index == index_current:
            log_fh.write(stderr)
            log_fh.write(b"\n")
            log_fh.write(stdout)
            log_fh.write(b"\n")

            index_current += 1
            test_data = proc_results_by_index.pop(index_current, None)
            if test_data is not None:
                stdout, stderr = test_data
                index += 1
        else:
            proc_results_by_index[index] = stdout, stderr

    project_source_info.queue_processes(
        process_functions,
        process_finalize=process_finalize,
        # job_total=4,
    )

    print("Finished!")


def cppcheck_generate_summary(
        log_fh: IO[str],
        log_summary_fh: IO[str],
) -> None:
    source_dir = project_source_info.SOURCE_DIR
    source_dir_source = os.path.join(source_dir, "source") + os.sep
    source_dir_intern = os.path.join(source_dir, "intern") + os.sep

    filter_line_prefix = (source_dir_source, source_dir_intern)

    source_dir_prefix_len = len(source_dir.rstrip(os.sep))

    # Avoids many duplicate lines generated by headers.
    lines_unique = set()

    category: dict[str, list[str]] = {}
    re_match = re.compile(".* \\[([a-zA-Z_]+)\\]$")
    for line in log_fh:
        if not line.startswith(filter_line_prefix):
            continue
        # Print a relative directory from `SOURCE_DIR`,
        # less visual noise and makes it possible to compare reports from different systems.
        line = "." + line[source_dir_prefix_len:]
        if (m := re_match.match(line)) is None:
            continue
        g = m.group(1)
        if g in CHECKER_EXCLUDE_FROM_SUMMARY:
            continue

        if line in lines_unique:
            continue
        lines_unique.add(line)

        try:
            category_list = category[g]
        except KeyError:
            category_list = category[g] = []
        category_list.append(line)

    for key, value in sorted(category.items()):
        log_summary_fh.write("\n\n{:s}\n".format(key))
        for line in value:
            log_summary_fh.write(line)


def argparse_create() -> argparse.ArgumentParser:

    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawTextHelpFormatter,
    )

    parser.add_argument(
        "--build-dir",
        dest="build_dir",
        metavar='BUILD_DIR',
        type=str,
        help=(
            "The build directory (containing CMakeCache.txt).\n"
            "\n"
            "Defaults to the \".\"."
        ),
        default=".",
        required=False,
    )

    parser.add_argument(
        "--output-dir",
        dest="output_dir",
        metavar='OUTPUT_DIR',
        type=str,
        help=(
            "Specify the directory where CPPCHECK logs will be written to.\n"
            "Using this may be preferred so the build directory can be cleared\n"
            "without loosing the result of previous checks.\n"
            "\n"
            "Defaults to {BUILD_DIR}/cppcheck/"
        ),
        default="",
        required=False,
    )

    return parser


def main() -> None:
    args = argparse_create().parse_args()

    project_source_info.cmake_dir_set(args.build_dir)

    cppcheck_dir = args.output_dir

    if cppcheck_dir:
        cppcheck_dir = os.path.normpath(os.path.abspath(cppcheck_dir))
    else:
        cppcheck_dir = os.path.join(os.path.normpath(os.path.abspath(project_source_info.CMAKE_DIR)), "cppcheck")

    del args

    filepath_output_log = os.path.join(cppcheck_dir, "cppcheck.part.log")
    filepath_output_summary_log = os.path.join(cppcheck_dir, "cppcheck_summary.part.log")

    try:
        os.makedirs(cppcheck_dir, exist_ok=True)

        files_old = {}

        # Comparing logs is useful, keep the old ones (renamed).
        with tempfile.TemporaryDirectory() as temp_dir:
            with open(filepath_output_log, "wb") as log_fh:
                cppcheck(cppcheck_dir, temp_dir, log_fh)

        with (
                open(filepath_output_log, "r", encoding="utf-8") as log_fh,
                open(filepath_output_summary_log, "w", encoding="utf-8") as log_summary_fh,
        ):
            cppcheck_generate_summary(log_fh, log_summary_fh)

    except KeyboardInterrupt:
        print("\nCanceling...")
        for filepath_part in (
                filepath_output_log,
                filepath_output_summary_log,
        ):
            if os.path.exists(filepath_part):
                os.remove(filepath_part)
        return

    # The partial files have been written.
    # - Move previous files -> `.old.log`.
    # - Move `.log.part` -> `.log`
    #
    # Do this last so it's possible to cancel execution without breaking the old/new log comparison
    # which is especially useful when comparing the old/new summary.

    for filepath_part in (
            filepath_output_log,
            filepath_output_summary_log,
    ):
        filepath = filepath_part.removesuffix(".part.log") + ".log"
        if not os.path.exists(filepath):
            os.rename(filepath_part, filepath)
            continue

        filepath_old = filepath.removesuffix(".log") + ".old.log"
        if os.path.exists(filepath_old):
            os.remove(filepath_old)
        os.rename(filepath, filepath_old)
        os.rename(filepath_part, filepath)
        files_old[filepath] = filepath_old

    print("Written:")
    for filepath_part in (
            filepath_output_log,
            filepath_output_summary_log,
    ):
        filepath = filepath_part.removesuffix(".part.log") + ".log"
        print(" ", filepath, "<->", files_old.get(filepath, "<none>"))


if __name__ == "__main__":
    main()
