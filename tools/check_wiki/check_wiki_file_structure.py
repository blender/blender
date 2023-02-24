#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later

"""
This script is to validate the WIKI page that documents Blender's file-structure, see:

    https://wiki.blender.org/wiki/Source/File_Structure

It can run without any arguments, where it will download the WIKI to Blender's source root:

You may pass the wiki text as an argument, e.g.

check_wiki_file_structure.py --wiki=wiki.txt
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
SOURCE_DIR = os.path.normpath(os.path.join(CURRENT_DIR, "..", "..", ".."))
WIKI_URL = "https://wiki.blender.org/wiki/Source/File_Structure"
WIKI_URL_EDIT = "https://wiki.blender.org/w/index.php?title=Source/File_Structure&action=edit"


# -----------------------------------------------------------------------------
# HTML Utilities

def text_with_title_underline(text: str, underline: str = "=") -> str:
    return "\n{:s}\n{:s}\n".format(text, len(text) * underline)


def html_extract_first_textarea(data: str) -> Optional[str]:
    """
    Extract and escape text within the first
    ``<textarea ...> ... </textarea>`` found in the HTML text.
    """
    beg = data.find("<textarea")
    if beg == -1:
        print("Failed to extract <textarea ...> start")
        return None

    beg = data.find(">", beg)
    if beg == -1:
        print("Failed to extract <textarea ...> end")
        return None
    beg += 1

    end = data.find("</textarea>", beg)
    if end == -1:
        print("Failed to extract </textarea>")
        return None

    data = data[beg:end]
    for (src, dst) in (
            ("&lt;", "<"),
            ("&gt;", ">"),
            ("&amp;", "&"),
            ("&quot;", "\""),
    ):
        data = data.replace(src, dst)
    return data


def html_extract_first_textarea_from_url(url: str) -> Optional[str]:
    """
    Download
    """
    import urllib.request

    req = urllib.request.Request(url=url)
    with urllib.request.urlopen(req) as fh:
        data = fh.read().decode('utf-8')

    return html_extract_first_textarea(data)


# -----------------------------------------------------------------------------
# WIKI Text Parsing

def wiki_to_paths_and_docstrings(wiki_text: str) -> Tuple[List[str], List[str]]:
    file_paths = []
    file_paths_docstring = []
    lines = wiki_text.split("\n")
    i = 0
    while i < len(lines):
        if lines[i].startswith("| /"):
            # Convert:
            # `| /source/'''blender/'''` -> `/source/blender`.
            p = lines[i][3:].replace("'''", "").split(" ", 1)[0].rstrip("/")
            file_paths.append(p)

            body = []
            i += 1
            while lines[i].strip() not in {"|-", "|}"}:
                body.append(lines[i].lstrip("| "))
                i += 1
            i -= 1
            file_paths_docstring.append("\n".join(body))

        i += 1

    return file_paths, file_paths_docstring


# -----------------------------------------------------------------------------
# Reporting

def report_known_wiki_paths(file_paths: List[str]) -> None:
    heading = "Paths Found in WIKI Table"
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

    print("The following paths were found in the WIKI text\n"
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
          "but are missing from the WIKI text:\n")
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


def report_todo_in_docstrings(file_paths: List[str], file_paths_docstring: List[str]) -> int:
    heading = "Marked as TODO"
    test = []

    re_todo = re.compile(r"\bTODO\b")
    for p, docstring in zip(file_paths, file_paths_docstring):
        if re_todo.match(docstring):
            test.append(p)

    amount = str(len(test)) if test else "none found"
    print(text_with_title_underline("{:s} ({:s})".format(heading, amount)))
    if not test:
        return 0

    for p in test:
        print("-", p)

    return len(test)


# -----------------------------------------------------------------------------
# Argument Parser

def create_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)

    parser.add_argument(
        "-w",
        "--wiki",
        dest="wiki_text",
        metavar='PATH',
        default=os.path.join(SOURCE_DIR, "wiki_file_structure.txt"),
        help="WIKI text file path, NOTE: this will be downloaded if not found!",
    )
    return parser


# -----------------------------------------------------------------------------
# Main Function

def main() -> None:
    parser = create_parser()

    args = parser.parse_args()

    if os.path.exists(args.wiki_text):
        print("Using existing WIKI text:", args.wiki_text)
    else:
        data = html_extract_first_textarea_from_url(WIKI_URL_EDIT)
        if data is not None:
            with open(args.wiki_text, 'w', encoding='utf-8') as fh:
                fh.write(data)
            print("Downloaded WIKI text to:", args.wiki_text)
            print("Update and save to:", WIKI_URL)
        else:
            print("Failed to downloaded or extract WIKI text, aborting!")
            return

    with open(args.wiki_text, 'r', encoding='utf-8') as fh:
        file_paths, file_paths_docstring = wiki_to_paths_and_docstrings(fh.read())

    # Disable, mostly useful when debugging why paths might not be found.
    # report_known_wiki_paths()
    issues = 0
    issues += report_missing_source(file_paths)
    issues += report_incomplete(file_paths)
    issues += report_alphabetical_order(file_paths)
    issues += report_todo_in_docstrings(file_paths, file_paths_docstring)

    if issues:
        print("Warning, found {:d} issues!\n".format(issues))
    else:
        print("Success! The WIKI text is up to date with Blender's source tree!\n")


if __name__ == "__main__":
    main()
