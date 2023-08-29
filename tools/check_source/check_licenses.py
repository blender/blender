#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2022-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
Check license headers follow the SPDX spec
https://spdx.org/licenses/

This can be activated by calling "make check_licenses" from Blenders root directory.
"""

import os
import argparse
import datetime
import re

from dataclasses import dataclass

from typing import (
    Callable,
    Dict,
    Generator,
    List,
    Tuple,
)

# -----------------------------------------------------------------------------
# Constants

# Add one, maybe someone runs this on new-years in another timezone or so.
YEAR_MAX = datetime.date.today().year + 1
# Lets not worry about software written before this time.
YEAR_MIN = 1950
YEAR_RANGE = range(YEAR_MIN, YEAR_MAX + 1)

# Faster bug makes exceptions and errors more difficult to troubleshoot.
USE_MULTIPROCESS = False

EXPECT_SPDX_IN_FIRST_CHARS = 1024

# Show unique headers after modifying them.
# Useful when reviewing changes as there may be many duplicates.
REPORT_UNIQUE_HEADER_MAPPING = False
mapping: Dict[str, List[str]] = {}

SOURCE_DIR = os.path.normpath(
    os.path.abspath(
        os.path.normpath(os.path.join(os.path.dirname(__file__), "..", ".."))
    )
)

SPDX_IDENTIFIER_FILE = os.path.join(
    SOURCE_DIR, "doc", "license", "SPDX-license-identifiers.txt"
)
SPDX_IDENTIFIER_UNKNOWN = "*Unknown License*"

with open(SPDX_IDENTIFIER_FILE, "r", encoding="utf-8") as fh:
    ACCEPTABLE_LICENSES = set(line.split()[0] for line in sorted(fh) if "https://spdx.org/licenses/" in line)
del fh


# -----------------------------------------------------------------------------
# Global Variables

# Count how many licenses are used.
SPDX_IDENTIFIER_STATS: Dict[str, int] = {SPDX_IDENTIFIER_UNKNOWN: 0}

# -----------------------------------------------------------------------------
# File Type Checks


# Use `/* .. */` style comments.
def filename_is_c_compat(filename: str) -> bool:
    return filename.endswith(
        (
            # C.
            ".c",
            ".h",
            # C++
            ".cc",
            ".cxx",
            ".cpp",
            ".hh",
            ".hxx",
            ".hpp",
            ".inl",
            # Objective-C/C++
            ".m",
            ".mm",
            # OpenGL Shading Language.
            ".glsl",
            # OPENCL.
            ".cl",
            # CUDA.
            ".cu",
            # Metal.
            ".metal",
            # Metal Shading Language.
            ".msl",
            # Open Shading Language.
            ".osl",
            # Cycles uses this extension.
            ".tables",
        )
    )


def filename_is_cmake(filename: str) -> bool:
    return filename.endswith(("CMakeLists.txt", ".cmake"))


# Use '#' style comments.
def filename_is_script_compat(filename: str) -> bool:
    return filename.endswith((".py", ".sh", "GNUmakefile"))


# -----------------------------------------------------------------------------
# Cursor Motion

def txt_next_line_while_fn(text: str, index: int, fn: Callable[[str], bool]) -> int:
    """
    Return the next line where ``fn`` fails.
    """
    while index < len(text):
        index_prev = index
        index = text.find("\n", index)
        if index == -1:
            index = len(text)
        if not fn(text[index_prev:index]):
            index = index_prev
            break
        # Step over the newline.
        index = index + 1
    return index


def txt_next_eol(text: str, pos: int, limit: int, step_over: bool) -> int:
    """
    Extend ``pos`` to just before the next EOL, otherwise EOF.
    As this is intended for use as a range, ``text[pos]``
    will either be ``\n`` or equal to out of range (equal to ``len(text)``).
    """
    if pos + 1 >= len(text):
        return pos
    # Already at the bounds.
    if text[pos] == "\n":
        return pos + (1 if step_over else 0)
    pos_next = text.find("\n", pos, limit)
    if pos_next == -1:
        return limit
    return pos_next + (1 if step_over else 0)


def txt_prev_bol(text: str, pos: int, limit: int) -> int:

    if pos == 0:
        return pos
    # Already at the bounds.
    if text[pos - 1] == "\n":
        return pos
    pos_next = text.rfind("\n", limit, pos)
    if pos_next == -1:
        return limit
    # We don't want to include the newline.
    return pos_next + 1


def txt_anonymous_years(text: str) -> str:
    """
    Replace year with text, since we don't want to consider them different when looking at unique headers.
    """

    # Replace year ranges with `2005-2009`: `####`.
    def key_replace_range(match: re.Match[str]) -> str:
        values = match.groups()
        if int(values[0]) in YEAR_RANGE and int(values[1]) in YEAR_RANGE:
            return '#' * len(values[0])
        return match.group()

    text = re.sub(r'([0-9]+)-([0-9]+)', key_replace_range, text)

    # Replace year ranges with `2005`: `####`.
    def key_replace(match: re.Match[str]) -> str:
        values = match.groups()
        if int(values[0]) in YEAR_RANGE:
            return '#' * len(values[0])
        return match.group()

    text = re.sub(r'([0-9]+)', key_replace, text)

    return text


def txt_find_next_indented_block(text: str, find: str, pos: int, limit: int) -> Tuple[int, int]:
    """
    Support for finding an indented block of text.
    Return the identifier index and the end of the block.

    Where searching for ``SPDX-FileCopyrightText: ``

    .. code-block::

       # SPDX-FileCopyrightText: 2020 Name
         ^ begin                          ^ end.

    With multiple lines supported:

    .. code-block::

       # SPDX-FileCopyrightText: 2020 Name
       #                         2021 Another Name
         ^ begin (one line up)                    ^ end.
    """
    pos_found = text.find(find, pos, limit)
    if pos_found == -1:
        return (-1, -1)

    pos_next = txt_next_eol(text, pos_found, limit - 1, False) + 1
    if pos_next != limit:
        pos_found_indent = pos_found - txt_prev_bol(text, pos_found, 0)
        while True:
            # Step over leading comment chars.
            pos_next_test = pos_next + pos_found_indent
            pos_next_step = pos_next_test + len(find)
            # The next lines text is indented.
            text_indent = text[pos_next_test:pos_next_step]
            if (len(text_indent) == pos_next_step - pos_next_test) and (not text[pos_next_test:pos_next_step].strip()):
                pos_next = txt_next_eol(text, pos_next_step, limit - 1, step_over=False) + 1
            else:
                break

    return (pos_found, pos_next)

# -----------------------------------------------------------------------------
# License Checker


def check_contents(filepath: str, text: str) -> None:
    """
    Check for license text, e.g: ``SPDX-License-Identifier: GPL-2.0-or-later``

    Intentionally be strict here... no extra spaces, no trailing space at the end of line etc.
    As there is no reason to be sloppy in this case.
    """
    text_header = text[:EXPECT_SPDX_IN_FIRST_CHARS]

    # Use the license to limit the copyright search,
    # so code-generation that includes copyright headers don't cause false alarms.
    license_id = " SPDX-License-Identifier: "
    license_id_beg = text_header.find(license_id)
    if license_id_beg == -1:
        # Allow completely empty files (sometimes `__init__.py`).
        if not text.rstrip():
            return
        # Empty file already accounted for.
        print("Missing {:s}{:s}".format(license_id, filepath))
        return

    # Check copyright text, reading multiple (potentially multi-line indented) blocks.
    copyright_id = " SPDX-FileCopyrightText: "

    copyright_id_step = 0
    copyright_id_beg = -1
    copyright_id_end = -1
    while ((copyright_id_item := txt_find_next_indented_block(
            text_header,
            copyright_id,
            copyright_id_step,
            license_id_beg,
    )) != (-1, -1)):
        if copyright_id_end == -1:
            # Set once.
            copyright_id_beg = copyright_id_item[0]
        else:
            lines = text_header[copyright_id_end:copyright_id_item[0]].count("\n")
            if lines != 0:
                print(
                    "Expected no blank lines, found {:d} between \"{:s}\": {:s}".format(
                        lines,
                        copyright_id,
                        filepath,
                    ))

        copyright_id_end = copyright_id_item[1]
        copyright_id_step = copyright_id_end
    del copyright_id_item, copyright_id_step

    if copyright_id_beg == -1:
        print("Missing {:s}{:s}".format(copyright_id, filepath))

        # Maintain statistics.
        SPDX_IDENTIFIER_STATS[SPDX_IDENTIFIER_UNKNOWN] += 1
        return

    # Check for blank lines:
    blank_lines = text[:copyright_id_beg].count("\n")
    if filename_is_script_compat(filepath):
        if blank_lines > 0 and text.startswith("#!/"):
            blank_lines -= 1
    if blank_lines > 0:
        print("SPDX \"{:s}\" not on first line: {:s}".format(copyright_id, filepath))

    # Leading char.
    leading_char = text_header[txt_prev_bol(text_header, license_id_beg, 0):license_id_beg].strip()
    text_blank_line = text_header[copyright_id_end:license_id_beg]
    if (text_blank_line.count("\n") != 1) or (text_blank_line.replace(leading_char, "").strip() != ""):
        print("Expected blank line between \"{:s}\" & \"{:s}\": {:s}".format(copyright_id, license_id, filepath))
    del text_blank_line, leading_char

    license_id_end = license_id_beg + len(license_id)
    line_end = txt_next_eol(text, license_id_end, len(text), step_over=False)
    license_text = text[license_id_end:line_end]
    # For C/C++ comments.
    license_text = license_text.rstrip("*/")
    for license_id in license_text.split():
        if license_id in {"AND", "OR"}:
            continue

        if license_id not in ACCEPTABLE_LICENSES:
            print(
                "Unexpected:",
                "{:s}:{:d}".format(filepath, text[:license_id_beg].count("\n") + 1),
                "contains license",
                repr(license_text),
                "not in",
                SPDX_IDENTIFIER_FILE,
            )

        try:
            SPDX_IDENTIFIER_STATS[license_id] += 1
        except KeyError:
            SPDX_IDENTIFIER_STATS[license_id] = 1

    if REPORT_UNIQUE_HEADER_MAPPING:
        if filename_is_c_compat(filepath):
            comment_beg = text.rfind("/*", 0, license_id_beg)
            if comment_beg == -1:
                print("Comment Block:", filepath, "failed to find comment start")
                return
            comment_end = text.find("*/", license_id_end, len(text))
            if comment_end == -1:
                print("Comment Block:", filepath, "failed to find comment end")
                return
            comment_end += 2
            comment_block = text[comment_beg + 2: comment_end - 2]
            comment_block = "\n".join(
                [line.removeprefix(" *") for line in comment_block.split("\n")]
            )
        elif filename_is_script_compat(filepath) or filename_is_cmake(filepath):
            comment_beg = txt_prev_bol(text, license_id_beg, 0)
            comment_end = txt_next_eol(text, license_id_beg, len(text), step_over=False)

            comment_beg = txt_next_line_while_fn(
                text,
                comment_beg,
                lambda line: line.startswith("#") and not line.startswith("#!/"),
            )
            comment_end = txt_next_line_while_fn(
                text,
                comment_end,
                lambda line: line.startswith("#"),
            )

            comment_block = text[comment_beg:comment_end].rstrip()
            comment_block = "\n".join(
                [line.removeprefix("# ") for line in comment_block.split("\n")]
            )
        else:
            raise Exception("Unknown file type: {:s}".format(filepath))

        mapping.setdefault(txt_anonymous_years(comment_block), []).append(filepath)


def report_statistics() -> None:
    """
    Report some final statistics of license usage.
    """
    print("")
    files_total = sum(SPDX_IDENTIFIER_STATS.values())
    files_unknown = SPDX_IDENTIFIER_STATS[SPDX_IDENTIFIER_UNKNOWN]
    files_percent = (1.0 - (files_unknown / files_total)) * 100.0
    title = "License Statistics in {:,d} Files, {:.2f}% Complete".format(files_total, files_percent)
    print("#" * len(title))
    print(title)
    print("#" * len(title))
    print("")
    max_length = max(len(k) for k in SPDX_IDENTIFIER_STATS.keys())
    print("  License:" + (" " * (max_length - 7)) + "Files:")
    print("")
    items = [(k, "{:,d}".format(v)) for k, v in sorted(SPDX_IDENTIFIER_STATS.items())]
    v_max = max([len(v) for _, v in items])
    for k, v in items:
        if v == "0":
            continue
        print("-", k + " " * (max_length - len(k)), (" " * (v_max - len(v))) + v)
    print("")


# -----------------------------------------------------------------------------
# Main Function & Source Listing

operation = check_contents


def source_files(
    path: str,
    paths_exclude: Tuple[str, ...],
    filename_test: Callable[[str], bool],
) -> Generator[str, None, None]:
    # Split paths into directories & files.
    dirs_exclude_list = []
    files_exclude_list = []
    for f in paths_exclude:
        if not os.path.exists(f):
            raise Exception("File {!r} doesn't exist!".format(f))
        if os.path.isdir(f):
            dirs_exclude_list.append(f)
        else:
            files_exclude_list.append(f)
    del paths_exclude

    dirs_exclude_set = set(p.rstrip("/") for p in dirs_exclude_list)
    dirs_exclude = tuple(p.rstrip("/") + "/" for p in dirs_exclude_list)

    files_exclude_set = set(p.rstrip("/") for p in files_exclude_list)
    del dirs_exclude_list, files_exclude_list

    for dirpath, dirnames, filenames in os.walk(path):
        dirnames[:] = [d for d in dirnames if not d.startswith(".")]
        if dirpath in dirs_exclude_set or dirpath.startswith(dirs_exclude):
            continue
        for filename in filenames:
            if filename.startswith("."):
                continue
            filepath = os.path.join(dirpath, filename)
            if filepath in files_exclude_set:
                files_exclude_set.remove(filepath)
                continue

            if filename_test(filename):
                yield filepath

    if files_exclude_set:
        raise Exception("Excluded paths not found: {!r}".format(repr(tuple(sorted(files_exclude_set)))))


def operation_wrap(filepath: str) -> None:
    with open(filepath, "r", encoding="utf-8") as f:
        try:
            text = f.read()
        except Exception as ex:
            print("Failed to read", filepath, "with", repr(ex))
            return

        operation(filepath, text)


def argparse_create() -> argparse.ArgumentParser:

    # When --help or no args are given, print this help
    description = __doc__
    parser = argparse.ArgumentParser(description=description)

    parser.add_argument(
        "--show-headers",
        dest="show_headers",
        type=bool,
        default=False,
        required=False,
        help="Show unique headers (useful for spotting irregularities).",
    )

    return parser


def main() -> None:
    global REPORT_UNIQUE_HEADER_MAPPING

    args = argparse_create().parse_args()

    REPORT_UNIQUE_HEADER_MAPPING = args.show_headers

    # Ensure paths are relative to the root, no matter where this script runs from.
    os.chdir(SOURCE_DIR)

    @dataclass
    class Pass:
        filename_test: Callable[[str], bool]
        source_paths_include: Tuple[str, ...]
        source_paths_exclude: Tuple[str, ...]

    passes = (
        Pass(
            filename_test=filename_is_c_compat,
            source_paths_include=(".",),
            source_paths_exclude=(
                # Directories:
                "./extern",
                "./scripts/addons_contrib",
                "./scripts/templates_osl",
                "./tools",
                # Needs manual handling as it mixes two licenses.
                "./intern/atomic",
                # Practically an "extern" within an "intern" module, leave as-is.
                "./intern/itasc/kdl",

                # TODO: Files in these directories should be handled but the files have valid licenses.
                "./intern/libmv",

                # Files:
                # This file is generated by a configure script (no point in manually setting the license).
                "./build_files/build_environment/patches/config_gmpxx.h",

                # A modified `Apache-2.0` license.
                "./intern/opensubdiv/internal/evaluator/shaders/glsl_compute_kernel.glsl",
            ),
        ),
        Pass(
            filename_test=filename_is_cmake,
            source_paths_include=(".",),
            source_paths_exclude=(
                # Directories:
                # This is an exception, it has its own CMake files we do not maintain.
                "./extern/audaspace",
                "./extern/quadriflow/3rd/lemon-1.3.1",
            ),
        ),
        Pass(
            filename_test=filename_is_script_compat,
            source_paths_include=(".",),
            source_paths_exclude=(
                # Directories:
                # This is an exception, it has its own CMake files we do not maintain.
                "./extern",
                "./scripts/addons_contrib",
                # Just data.
                "./doc/python_api/examples",
                "./scripts/addons/presets",
                "./scripts/presets",
                "./scripts/templates_py",
            ),
        ),
    )

    for pass_data in passes:
        if USE_MULTIPROCESS:
            filepath_args = [
                filepath
                for dirpath in pass_data.source_paths_include
                for filepath in source_files(
                    dirpath,
                    pass_data.source_paths_exclude,
                    pass_data.filename_test,
                )
            ]
            import multiprocessing

            job_total = multiprocessing.cpu_count()
            pool = multiprocessing.Pool(processes=job_total * 2)
            pool.map(operation_wrap, filepath_args)
        else:
            for filepath in [
                filepath
                for dirpath in pass_data.source_paths_include
                for filepath in source_files(
                    dirpath,
                    pass_data.source_paths_exclude,
                    pass_data.filename_test,
                )
            ]:
                operation_wrap(filepath)

    if REPORT_UNIQUE_HEADER_MAPPING:
        print("#####################")
        print("Unique Header Listing")
        print("#####################")
        print("")
        for k, v in sorted(mapping.items()):
            print("=" * 79)
            print(k)
            print("-" * 79)
            v.sort()
            for filepath in v:
                print("-", filepath)
            print("")

    report_statistics()


if __name__ == "__main__":
    main()
