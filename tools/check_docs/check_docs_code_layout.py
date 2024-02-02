#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
This script is to validate the markdown page that documents Blender's file-structure, see:

    https://developer.blender.org/docs/features/code_layout/

It can run without any arguments, where it will download the markdown to Blender's source root:

You may pass the markdown text as an argument, e.g.

check_docs_code_layout.py --markdown=markdown.txt
"""

import os
import re
import argparse

from typing import (
    List,
    Optional,
    Tuple,
)


# -----------------------------------------------------------------------------
# Constants

CURRENT_DIR = os.path.abspath(os.path.dirname(__file__))
SOURCE_DIR = os.path.normpath(os.path.join(CURRENT_DIR, "..", ".."))

MARKDOWN_URL = "https://projects.blender.org/blender/blender-developer-docs/raw/branch/main/docs/features/code_layout.md"


# -----------------------------------------------------------------------------
# HTML Utilities

def text_with_title_underline(text: str, underline: str = "=") -> str:
    return "\n{:s}\n{:s}\n".format(text, len(text) * underline)


def html_extract_markdown_from_url(url: str) -> Optional[str]:
    """
    Download
    """
    import urllib.request

    req = urllib.request.Request(url=url)
    with urllib.request.urlopen(req) as fh:
        data = fh.read().decode('utf-8')

    return data


# -----------------------------------------------------------------------------
# markdown Text Parsing

def markdown_to_paths(markdown: str) -> Tuple[List[str], List[str]]:
    file_paths = []
    markdown = markdown.replace("<p>", "")
    markdown = markdown.replace("</p>", "")
    markdown = markdown.replace("<strong>", "")
    markdown = markdown.replace("</strong>", "")
    markdown = markdown.replace("</td>", "")

    path_prefix = "<td markdown>/"

    for line in markdown.splitlines():
        line = line.strip()
        if line.startswith(path_prefix):
            file_path = line[len(path_prefix):]
            file_path = file_path.rstrip("/")
            file_paths.append(file_path)

    return file_paths


# -----------------------------------------------------------------------------
# Reporting

def report_known_markdown_paths(file_paths: List[str]) -> None:
    heading = "Paths Found in markdown Table"
    print(text_with_title_underline(heading))
    for p in file_paths:
        print("-", p)


def report_missing_source(file_paths: List[str]) -> int:
    heading = "Missing in Source Dir"

    test = [p for p in file_paths if not os.path.exists(os.path.join(SOURCE_DIR, p))]

    amount = str(len(test)) if test else "none found"
    print(text_with_title_underline("{:s} ({:s})".format(heading, amount)))
    if not test:
        return 0

    print("The following paths were found in the markdown\n"
          "but were not found in Blender's source directory:\n")
    for p in test:
        print("-", p)

    return len(test)


def report_incomplete(file_paths: List[str]) -> int:
    heading = "Missing Documentation"

    test = []
    basedirs = {os.path.dirname(p) for p in file_paths}
    for base in sorted(basedirs):
        base_abs = os.path.join(SOURCE_DIR, base)
        if(os.path.exists(base_abs)):
            for p in os.listdir(base_abs):
                if not p.startswith("."):
                    p_abs = os.path.join(base_abs, p)
                    if os.path.isdir(p_abs):
                        p_rel = os.path.join(base, p)
                        if p_rel not in file_paths:
                            test.append(p_rel)

    amount = str(len(test)) if test else "none found"
    print(text_with_title_underline("{:s} ({:s})".format(heading, amount)))
    if not test:
        return 0

    print("The following paths were found in Blender's source directory\n"
          "but are missing from the markdown:\n")
    for p in sorted(test):
        print("-", p)

    return len(test)


def report_alphabetical_order(file_paths: List[str]) -> int:
    heading = "Non-Alphabetically Ordered"
    test = []

    p_prev = ""
    p_prev_dir = ""
    for p in file_paths:
        p_dir = os.path.dirname(p)
        if p_prev:
            if p_dir == p_prev_dir:
                if p < p_prev:
                    test.append((p_prev, p))
        p_prev_dir = p_dir
        p_prev = p

    amount = str(len(test)) if test else "none found"
    print(text_with_title_underline("{:s} ({:s})".format(heading, amount)))
    if not test:
        return 0

    for p_prev, p in test:
        print("-", p, "(should be before)\n ", p_prev)

    return len(test)


# -----------------------------------------------------------------------------
# Argument Parser

def create_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)

    parser.add_argument(
        "-m",
        "--markdown",
        dest="markdown",
        metavar='PATH',
        default=os.path.join(SOURCE_DIR, "markdown_file_structure.txt"),
        help="markdown text file path, NOTE: this will be downloaded if not found!",
    )
    return parser


# -----------------------------------------------------------------------------
# Main Function

def main() -> None:
    parser = create_parser()

    args = parser.parse_args()

    if os.path.exists(args.markdown):
        print("Using existing markdown text:", args.markdown)
    else:
        data = html_extract_markdown_from_url(MARKDOWN_URL)
        if data is not None:
            with open(args.markdown, 'w', encoding='utf-8') as fh:
                fh.write(data)
            print("Downloaded markdown text to:", args.markdown)
            print("Update and save to:", MARKDOWN_URL)
        else:
            print("Failed to downloaded or extract markdown text, aborting!")
            return

    with open(args.markdown, 'r', encoding='utf-8') as fh:
        file_paths = markdown_to_paths(fh.read())

    # Disable, mostly useful when debugging why paths might not be found.
    # report_known_markdown_paths()
    issues = 0
    issues += report_missing_source(file_paths)
    issues += report_incomplete(file_paths)
    issues += report_alphabetical_order(file_paths)

    if issues:
        print("Warning, found {:d} issues!\n".format(issues))
    else:
        print("Success! The markdown text is up to date with Blender's source tree!\n")


if __name__ == "__main__":
    main()
