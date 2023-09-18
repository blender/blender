#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
Extract line & function information from addresses (found in back-traces) using addr2line.

Example:

   addr2line_backtrace.py --exe=/path/to/blender error.log

Piping from the standard-input is also supported:

   cat error.log | addr2line_backtrace.py --exe=blender.bin

The text is printed to the standard output.
"""

import argparse
import multiprocessing
import os
import re
import subprocess
import sys
import time

from typing import (
    Any,
    List,
    Optional,
    Sequence,
    Tuple,
)

RE_ADDR = re.compile("\\[(0x[A-Fa-f0-9]+)\\]")
IS_ATTY = sys.stdout.isatty()


def value_as_percentage(value_partial: int, value_final: int) -> str:
    percent = 0.0 if (value_final == 0) else (value_partial / value_final)
    return "{:-6.2f}%".format(percent * 100)


if IS_ATTY:
    def progress_output(value_partial: int, value_final: int, info: str) -> None:
        sys.stdout.write("\r\033[K[{:s}]: {:s}".format(value_as_percentage(value_partial, value_final), info))
else:
    def progress_output(value_partial: int, value_final: int, info: str) -> None:
        sys.stdout.write("[{:s}]: {:s}\n".format(value_as_percentage(value_partial, value_final), info))


def find_gitroot(filepath_reference: str) -> Optional[str]:
    path = filepath_reference
    path_prev = ""
    found = False
    while not (found := os.path.exists(os.path.join(path, ".git"))) and path != path_prev:
        path_prev = path
        path = os.path.dirname(path)
    if found:
        return path
    return None


def addr2line_fn(arg_pair: Tuple[Tuple[str, str, bool], Sequence[str]]) -> Sequence[Tuple[str, str]]:
    shared_args, addr_list = arg_pair
    (exe, base_path, time_command) = shared_args
    cmd = (
        "addr2line",
        *addr_list,
        "--functions",
        "--demangle",
        "--exe=" + exe,
    )
    if time_command:
        time_beg = time.time()

    output = subprocess.check_output(cmd).rstrip().decode("utf-8", errors="surrogateescape")
    output_lines = output.split("\n")

    result: List[Tuple[str, str]] = []

    while output_lines:
        # Swap (function, line), to (line, function).
        output_lines_for_addr = output_lines[:2]
        assert len(output_lines_for_addr) == 2
        del output_lines[:2]
        line_list = []
        for line in output_lines_for_addr:
            if line.startswith(base_path):
                line = "." + os.sep + line[len(base_path):]
            line_list.append(line)
        output = ": ".join(reversed(line_list))

        if time_command:
            time_end = time.time()
            output = "{:s} ({:.2f})".format(output, time_end - time_beg)
        result.append((addr_list[len(result)], output))

    return result


def argparse_create() -> argparse.ArgumentParser:
    import argparse

    # When `--help` or no arguments are given, print this help.
    epilog = "This is typically used from the output of a stack-trace on Linux/Unix."

    parser = argparse.ArgumentParser(
        formatter_class=argparse.RawTextHelpFormatter,
        description=__doc__,
        epilog=epilog,
    )

    parser.add_argument(
        "--exe",
        dest="exe",
        metavar='EXECUTABLE',
        required=True,
        help="Path to the binary.",
    )
    parser.add_argument(
        "--base",
        dest="base",
        metavar='BASE_PATH',
        default="",
        required=False,
        help="Base path.",
    )
    parser.add_argument(
        "--time",
        dest="time_command",
        action='store_true',
        required=False,
        help="Time addr2line (useful for checking on especially slow lookup).",
    )
    parser.add_argument(
        "--jobs",
        dest="jobs",
        type=int,
        default=4,
        help=(
            "The number of processes to use. "
            "Defaults to 4 to prevent using too much memory, 1 is single threaded (useful for debugging)."
        ),
        required=False,
    )
    parser.add_argument(
        "backtraces",
        nargs="*",
        help="Back-trace files to scan for addresses.",
    )

    return parser


def addr2line_for_filedata(
        exe: str,
        base_path: str,
        time_command: bool,
        jobs: int,
        backtrace_data: str,
) -> None:
    addr_set = set()
    for match in RE_ADDR.finditer(backtrace_data):
        addr = match.group(1)
        addr_set.add(addr)

    shared_args = exe, base_path, time_command
    if jobs >= len(addr_set):
        addr2line_args = [(shared_args, [addr]) for addr in addr_set]
    else:
        addr2line_args = [(shared_args, []) for _ in range(jobs)]
        # Avoid using consecutive addresses in chunks since slower lookups are likely to be groups.
        for i, addr in enumerate(addr_set):
            addr2line_args[i % jobs][1].append(addr)

    addr_map = {}
    addr_done = 0
    addr_len = len(addr_set)

    with multiprocessing.Pool(jobs) as pool:
        for i, result_list in enumerate(pool.imap_unordered(addr2line_fn, addr2line_args), 1):
            for (addr, result) in result_list:
                progress_output(addr_done, addr_len, "{:d} of {:d}".format(addr_done, addr_len))
                addr_map[addr] = result
                addr_done += 1

    if IS_ATTY:
        print()

    def re_replace_fn(match: re.Match[str]) -> str:
        addr = match.group(1)
        return "{:s} ({:s})".format(addr_map[addr], addr)

    backtrace_data_updated = RE_ADDR.sub(re_replace_fn, backtrace_data)

    sys.stdout.write(backtrace_data_updated)
    sys.stdout.write("\n")


def main() -> None:

    args = argparse_create().parse_args()

    jobs = args.jobs
    if jobs <= 0:
        jobs = multiprocessing.cpu_count() * 2

    base_path = args.base
    if not base_path:
        base_test = find_gitroot(os.getcwd())
        if base_test is not None:
            base_path = base_test
    if base_path:
        base_path = base_path.rstrip(os.sep) + os.sep

    if args.backtraces:
        for backtrace_filepath in args.backtraces:
            try:
                with open(backtrace_filepath, 'r', encoding="utf-8", errors="surrogateescape") as fh:
                    bactrace_data = fh.read()
            except BaseException as ex:
                print("Filed to open {!r}, {:s}".format(backtrace_filepath, str(ex)))
                continue

            addr2line_for_filedata(args.exe, base_path, args.time_command, jobs, bactrace_data)
    else:
        bactrace_data = sys.stdin.read()
        addr2line_for_filedata(args.exe, base_path, args.time_command, jobs, bactrace_data)


if __name__ == "__main__":
    main()
