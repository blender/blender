#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

r"""
Validates sizes in C/C++ sources written as: ``type name[/*MAX_NAME*/ 64]``
where ``MAX_NAME`` is expected to be a define equal to 64, otherwise a warning is reported.
"""
__all__ = (
    "main",
)

import os
import sys
import re

THIS_DIR = os.path.dirname(__file__)
BASE_DIR = os.path.normpath(os.path.abspath(os.path.normpath(os.path.join(THIS_DIR, "..", ".."))))
sys.path.append(os.path.join(THIS_DIR, "..", "utils_maintenance", "modules"))

from batch_edit_text import run
import line_number_utils


# -----------------------------------------------------------------------------
# Utilities


# -----------------------------------------------------------------------------
# Local Settings

# TODO, move to config file
SOURCE_DIRS = (
    "source",
)

SOURCE_EXT = (
    # C/C++
    ".c", ".h", ".cpp", ".hpp", ".cc", ".hh", ".cxx", ".hxx", ".inl",
    # Objective C
    ".m", ".mm",
    # GLSL
    ".glsl",
)

# Mainly useful for development to check extraction & validation are working.
SHOW_SUCCESS = True


# -----------------------------------------------------------------------------
# Globals


# Map defines to a list of (filename-split, value) pairs.
global_defines: dict[
    # The define ID.
    str,
    # Value(s), in case it's defined in multiple files.
    list[
        tuple[
            # The `BASE_DIR` relative path (split by `os.sep`).
            tuple[str, ...],
            # The value of the define,
            # a literal string with comments stripped out.
            str,
        ],
    ],
] = {}


REGEX_ID_LITERAL = "[A-Za-z_][A-Za-z_0-9]*"

# Detect:
#   `[/*ID*/ 64]`.
#   `[/*ID - 2*/ 62]`.
REGEX_SIZE_COMMENT_IN_ARRAY = re.compile("\\[\\/\\*([^\\]]+)\\*\\/\\s*(\\d+)\\]")
# Detect: `#define ID 64`
REGEX_DEFINE_C_LIKE = re.compile("^\\s*#\\s*define\\s+(" + REGEX_ID_LITERAL + ")[ \t]+([^\n]+)", re.MULTILINE)
# Detect:
#   `ID = 64,`
#   `ID = 64`
REGEX_ENUM_C_LIKE = re.compile("^\\s*(" + REGEX_ID_LITERAL + ")\\s=\\s([^,\n]+)", re.MULTILINE)
# Detect ID's.
REGEX_ID_OR_NUMBER_C_LIKE = re.compile("[A-Za-z0-9_]+")


def extract_defines(filepath: str, data_src: str) -> None:
    filepath_rel = os.path.relpath(filepath, BASE_DIR)
    for regex_matcher in (REGEX_DEFINE_C_LIKE, REGEX_ENUM_C_LIKE):
        for m in regex_matcher.finditer(data_src):
            value_id = m.group(1)
            value_literal = m.group(2)

            # Weak comment stripping.
            # This is (arguably) acceptable since the intent is to extract numbers,
            # if developers feel the need to write lines such as:
            # `#define VALUE_MAX /* Lets make some trouble! */ 64`
            # Then they can consider if that's actually needed (sigh!)...
            # Otherwise, we could replace this with a full parser such as CLANG,
            # however this is a bit of a hassle to setup.
            if "//" in value_literal:
                value_literal = value_literal.split("//", 1)[0]
            if "/*" in value_literal:
                value_literal = value_literal.split("/*", 1)[0]

            try:
                global_defines[value_id].append((tuple(filepath_rel.split(os.sep)), value_literal))
            except KeyError:
                global_defines[value_id] = [(tuple(filepath_rel.split(os.sep)), value_literal)]

    # Returning None indicates the file is not edited.


def path_score_distance(a: tuple[str, ...], b: tuple[str, ...]) -> tuple[int, int]:
    """
    Compare two paths, to find which paths are "closer" to each-other.
    This is used as a tie breaker when defines are found in multiple headers.
    """
    count_shared = 0
    range_min = min(len(a), len(b))
    range_max = max(len(a), len(b))
    for i in range(range_min):
        if a[i] != b[i]:
            break
        count_shared += 1

    count_nested = range_max - count_shared
    # Negate shared so smaller is better.
    # Less path nesting also gets priority.
    return (-count_shared, count_nested)


def eval_define(
        value_literal: str,
        *,
        default: str,
        filepath_ref_split: tuple[str, ...],
) -> tuple[str, list[str]]:
    failed: list[str] = []

    def re_replace_fn(match: re.Match[str]) -> str:
        value = match.group()
        if value.isdigit():
            return value

        other_values = global_defines.get(value)
        if other_values is None:
            failed.append(value)
            return value

        if len(other_values) == 1:
            other_filepath_split, other_literal = other_values[0]
        else:
            # Find the "closest" on the file system.
            # In practice favor paths which are co-located works fairly well,
            # needed as it's now known which headers ID's in a head *could* reference.
            other_literal_best = ""
            other_score_best = (0, 0)
            other_filepath_split_best: tuple[str, ...] = ("",)

            for other_filepath_split_test, other_literal_test in other_values:
                other_score_test = path_score_distance(filepath_ref_split, other_filepath_split_test)
                if (
                    # First time.
                    (not other_literal_best) or
                    # A lower score has been found (smaller is better).
                    (other_score_test < other_score_best)
                ):
                    other_literal_best = other_literal_test
                    other_score_best = other_score_test
                    other_filepath_split_best = other_filepath_split_test
                del other_score_test
            other_literal = other_literal_best
            other_filepath_split = other_filepath_split_best
            del other_literal_best, other_score_best, other_filepath_split_best

        other_literal_eval, other_failed = eval_define(
            other_literal,
            default="",
            filepath_ref_split=other_filepath_split,
        )
        if other_literal_eval:
            return other_literal_eval

        # `failed.append(value)` is also valid, report the gestured failure as its more likely to give insights
        # into what went wrong.
        failed.extend(other_failed)
        return value

    # Use integer division.
    value_literal = value_literal.replace(r"/", r"//")

    # Populates `failed`.
    value_literal_eval = REGEX_ID_OR_NUMBER_C_LIKE.sub(re_replace_fn, value_literal)

    if failed:
        # One or more ID could not be found.
        return default, failed

    # This could use exception handling, don't unless it's needed though.
    # pylint: disable-next=eval-used
    return str(eval(value_literal_eval)), failed


def validate_sizes(filepath: str, data_src: str) -> None:
    # Nicer for printing.
    filepath_rel = os.path.relpath(filepath, BASE_DIR)
    filepath_rel_split = tuple(filepath_rel.split(os.sep))

    for m, line, (beg, end) in line_number_utils.finditer_with_line_numbers_and_bounds(
            REGEX_SIZE_COMMENT_IN_ARRAY,
            data_src,
    ):
        del end
        value_id = m.group(1)
        value_literal = m.group(2)

        value_eval, lookups_failed = eval_define(
            value_id,
            default="",
            filepath_ref_split=filepath_rel_split,
        )

        data_line_column = "{:s}:{:d}:{:d}:".format(
            filepath_rel,
            line + 1,
            # Place the cursor after the `[`.
            (m.start(0) + 1) - beg,
        )

        if len(value_id.strip()) != len(value_id):
            print("WARN:", data_line_column, "comment includes white-space")
            continue

        if lookups_failed:
            print("WARN:", data_line_column, "[{:s}]".format(", ".join(lookups_failed)), "unknown")
            continue

        if value_literal != value_eval:
            print("WARN:", data_line_column, value_id, "mismatch", "({:s} != {:s})".format(value_literal, value_eval))
            continue

        if SHOW_SUCCESS:
            print("OK:  ", data_line_column, "{:s}={:s},".format(value_id, value_literal))

    # Returning None indicates the file is not edited.


def main() -> int:

    # Extract defines.
    run(
        directories=[os.path.join(BASE_DIR, d) for d in SOURCE_DIRS],
        is_text=lambda filepath: filepath.endswith(SOURCE_EXT),
        text_operation=extract_defines,
        # Can't be used if we want to accumulate in a global variable.
        use_multiprocess=False,
    )

    # For predictable lookups on tie breakers.
    # In practice it should almost never matter.
    for values in global_defines.values():
        if len(values) > 1:
            values.sort()

    # Validate sizes.
    run(
        directories=[os.path.join(BASE_DIR, d) for d in SOURCE_DIRS],
        is_text=lambda filepath: filepath.endswith(SOURCE_EXT),
        text_operation=validate_sizes,
        # Can't be used if we want to accumulate in a global variable.
        use_multiprocess=False,
    )

    return 0


if __name__ == "__main__":
    sys.exit(main())
