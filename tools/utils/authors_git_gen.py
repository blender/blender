#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
Example use to generate all authors:

   authors_git_gen.py

Example use a custom range:

   authors_git_gen.py --source=/src/blender --range=SHA1..HEAD
"""

# NOTE: this shares the basic structure with `credits_git_gen.py`,
# however details differ enough for them to be separate scripts.
# Improvements to this script may apply there too.

import argparse
import io
import multiprocessing
import os
import sys
import unicodedata

from typing import (
    Dict,
    Iterable,
    List,
)

from git_log import (
    GitCommitIter,
    GitCommit,
)

import git_data_canonical_authors
import git_data_sha1_override_authors

IS_ATTY = sys.stdout.isatty()

BASE_DIR = os.path.abspath(os.path.dirname(__file__))

SOURCE_DIR = os.path.normpath(os.path.join(BASE_DIR, "..", ".."))

# Exclude authors with this many lines modified.
AUTHOR_LINES_SKIP = 4

# Stop counting after this line limit.
AUTHOR_LINES_LIMIT = 100


# -----------------------------------------------------------------------------
# Lookup Table to clean up the authors
#
# This is a combination of unifying git logs as well as
# name change requested by the authors.

# Some projects prefer not to have their developers listed.
author_exclude_individuals = {
    "Jason Fielder <jason-fielder@noreply.localhost>",  # `@apple.com` developer.
    "Michael B Johnson <wave@noreply.localhost>",  # `@apple.com` developer.
}

author_exclude_glob = (
    "* <*@apple.com>",
)

author_table = git_data_canonical_authors.canonical_author_map()
author_override_table = git_data_sha1_override_authors.sha1_authors_map()

# -----------------------------------------------------------------------------
# Multi-Processing


def process_commits_for_map(commits: Iterable[GitCommit]) -> "Credits":
    result = Credits()
    for c in commits:
        result.process_commit(c)
    return result


# -----------------------------------------------------------------------------
# Progress Display

def value_as_percentage(value_partial: int, value_final: int) -> str:
    percent = 0.0 if (value_final == 0) else (value_partial / value_final)
    return "{:-6.2f}%".format(percent * 100)


if IS_ATTY:
    def progress_output(value_partial: int, value_final: int, info: str) -> None:
        sys.stdout.write("\r\033[K[{:s}]: {:s}".format(value_as_percentage(value_partial, value_final), info))
else:
    def progress_output(value_partial: int, value_final: int, info: str) -> None:
        sys.stdout.write("[{:s}]: {:s}\n".format(value_as_percentage(value_partial, value_final), info))


# -----------------------------------------------------------------------------
# Class for generating authors

class CreditUser:
    __slots__ = (
        "commit_total",
        "lines_change",
    )

    def __init__(self) -> None:
        self.commit_total = 0
        self.lines_change = -1


class Credits:
    __slots__ = (
        "users",
        # Use for progress, simply the number of times `process_commit` has been called.
        "process_commits_count",
    )

    def __init__(self) -> None:
        self.users: Dict[str, CreditUser] = {}
        self.process_commits_count = 0

    @classmethod
    def commit_authors_get(cls, c: GitCommit) -> List[str]:
        if (authors_overwrite := author_override_table.get(c.sha1, None)) is not None:
            # Ignore git commit info for these having an entry in `author_override_table`.
            return [author_table.get(author, author) for author in authors_overwrite]

        authors = [c.author] + c.co_authors
        # Normalize author string into canonical form, prevents duplicate credit users
        authors = [unicodedata.normalize('NFC', author) for author in authors]
        return [author_table.get(author, author) for author in authors]

    @classmethod
    def is_credit_commit_valid(cls, c: GitCommit) -> bool:
        ignore_dir = (
            b"blender/extern/",
            b"blender/intern/opennl/",
            # Will have own authors file.
            b"intern/cycles/",
            # Will have own authors file.
            b"intern/libmv/libmv/"
        )

        if not any(f for f in c.files if not f.startswith(ignore_dir)):
            return False

        return True

    def merge(self, other: "Credits") -> None:
        """
        Merge other Credits into this, clearing the other.
        """
        for user_key, user_other in other.users.items():
            user = self.users.get(user_key)
            if user is None:
                # Consume the user.
                self.users[user_key] = user_other
            else:
                user.commit_total += user_other.commit_total
                user.lines_change += user_other.lines_change
        other.users.clear()

    def process_commit(self, c: GitCommit) -> None:
        self.process_commits_count += 1

        if not self.is_credit_commit_valid(c):
            return

        lines_change = -1

        authors = self.commit_authors_get(c)
        for author in authors:
            cu = self.users.get(author)
            if cu is None:
                cu = self.users[author] = CreditUser()

            cu.commit_total += 1

            lines_change_for_author = cu.lines_change
            if lines_change_for_author < AUTHOR_LINES_LIMIT:
                if lines_change_for_author == -1:
                    cu.lines_change = lines_change_for_author = 0

                if lines_change == -1:
                    diff = c.diff
                    lines_change = diff.count("\n-") + diff.count("\n+")

                cu.lines_change += lines_change

    def _process_multiprocessing(self, commit_iter: Iterable[GitCommit], *, jobs: int) -> None:
        print("Collecting commits...")
        # NOTE(@ideasman42): that the chunk size doesn't have as much impact on
        # performance as you might expect, values between 16 and 1024 seem reasonable.
        # Although higher values tend to bottleneck as the process finishes.
        chunk_size = 256
        chunk_list = []
        chunk = []
        for i, c in enumerate(commit_iter):
            chunk.append(c)
            if len(chunk) >= chunk_size:
                chunk_list.append(chunk)
                chunk = []
        if chunk:
            chunk_list.append(chunk)

        commit_count_total = (max(len(chunk_list) - 1, 0) * chunk_size) + len(chunk)

        print("Found {:,d} commits, processing...".format(commit_count_total))
        commit_count_handled = 0
        with multiprocessing.Pool(processes=jobs) as pool:
            for result in pool.imap_unordered(process_commits_for_map, chunk_list):
                commit_count_handled += result.process_commits_count
                progress_output(
                    commit_count_handled,
                    commit_count_total,
                    "{:,d} / {:,d} commits".format(commit_count_handled, commit_count_total),
                )
                self.merge(result)

        if IS_ATTY:
            print("")  # Was printing on one-line, move to next.

    def process(self, commit_iter: Iterable[GitCommit], *, jobs: int) -> None:
        if jobs > 1:
            self._process_multiprocessing(commit_iter, jobs=jobs)
            return

        # Simple single process operation.
        for i, c in enumerate(commit_iter):
            self.process_commit(c)
            if not (i % 100):
                print(i)

    def write_object(
            self,
            fh: io.TextIOWrapper,
            *,
            use_metadata: bool = False,
    ) -> None:
        import fnmatch
        import re

        commit_word = "commit", "commits"
        metadata_right_margin = 79

        author_exclude_regex = tuple(
            (match_glob, re.compile(fnmatch.translate(match_glob)))
            for match_glob in author_exclude_glob
        )

        sorted_authors = dict(sorted(self.users.items()))
        for author_with_email, cu in sorted_authors.items():
            if author_with_email.endswith(" <>"):
                print("Skipping:", author_with_email, "(no email)")
                continue
            if cu.lines_change <= AUTHOR_LINES_SKIP:
                print("Skipping:", author_with_email, cu.lines_change, "line(s) changed.")
                continue
            if author_with_email in author_exclude_individuals:
                print("Skipping:", author_with_email, "explicit exclusion requested.")
                continue
            if author_with_email in author_exclude_individuals:
                print("Skipping:", author_with_email, "explicit exclusion requested.")
                continue
            if match_glob_found := next(iter([
                    match_glob for match_glob, match_regex in author_exclude_regex
                    if match_regex.match(author_with_email)
            ]), None):
                print("Skipping:", author_with_email, "glob exclusion \"{:s}\" requested.".format(match_glob_found))
                continue

            if use_metadata:
                fh.write("{:s} {:s}# lines={:,d} ({:s}), {:,d} {:s}\n".format(
                    author_with_email,
                    (" " * max(1, metadata_right_margin - len(author_with_email))),
                    min(cu.lines_change, AUTHOR_LINES_LIMIT),
                    "" if cu.lines_change >= AUTHOR_LINES_LIMIT else "<SKIP?>",
                    cu.commit_total,
                    commit_word[cu.commit_total > 1],
                ))
            else:
                fh.write("{:s}\n".format(author_with_email))

    def write(
            self,
            filepath: str,
            *,
            use_metadata: bool = False,
    ) -> None:
        with open(filepath, 'w', encoding="utf8", errors='xmlcharrefreplace') as fh:
            self.write_object(fh, use_metadata=use_metadata)


def argparse_create() -> argparse.ArgumentParser:

    # When --help or no args are given, print this help
    usage_text = "List authors."

    epilog = "This script is used to generate an AUTHORS file"

    parser = argparse.ArgumentParser(description=usage_text, epilog=epilog)

    parser.add_argument(
        "--source",
        dest="source_dir",
        metavar='PATH',
        required=False,
        default=SOURCE_DIR,
        help="Path to git repository",
    )
    parser.add_argument(
        "--output",
        dest="output_filepath",
        metavar='PATH',
        required=False,
        default="",
        help="File path to write the output to (when omitted, update the \"AUTHORS\" file)",
    )
    parser.add_argument(
        "--range",
        dest="range_sha1",
        metavar='SHA1_RANGE',
        required=False,
        default="HEAD",
        help="Range to use, eg: 169c95b8..HEAD or HEAD for all history.",
    )

    parser.add_argument(
        "--jobs",
        dest="jobs",
        type=int,
        default=0,
        help=(
            "The number of processes to use. "
            "Defaults to zero which detects the available cores, 1 is single threaded (useful for debugging)."
        ),
        required=False,
    )

    parser.add_argument(
        "--use-metadata",
        action='store_true',
        dest="use_metadata",
        default=False,
        help="Add commits & changed lines as metadata after the author."
    )

    return parser


def main() -> None:

    # ----------
    # Parse Args

    args = argparse_create().parse_args()

    credits = Credits()
    # commit_range = "HEAD~10..HEAD"
    # commit_range = "blender-v2.81-release..blender-v2.82-release"
    # commit_range = "blender-v2.82-release"
    commit_range = args.range_sha1
    jobs = args.jobs
    if jobs <= 0:
        # Clamp the value, higher values give errors with too many open files.
        # Allow users to manually pass very high values in as they might want to tweak system limits themselves.
        jobs = min(multiprocessing.cpu_count() * 2, 400)

    credits.process(GitCommitIter(args.source_dir, commit_range), jobs=jobs)

    if args.output_filepath:
        credits.write(args.output_filepath, use_metadata=args.use_metadata)
        print("Written:", args.output_filepath)
        return

    filepath_authors = os.path.join(args.source_dir, "AUTHORS")

    authors_text_io = io.StringIO()
    credits.write_object(authors_text_io, use_metadata=args.use_metadata)
    authors_text = authors_text_io.getvalue()
    del authors_text_io

    text_beg = "# BEGIN individuals section.\n"
    text_end = "# Please DO NOT APPEND here. See comments at the top of the file.\n"
    with open(filepath_authors, "r", encoding="utf-8") as fh:
        authors_contents = fh.read()

    text_beg_index = authors_contents.index(text_beg)
    text_end_index = authors_contents.index(text_end)
    if text_beg_index == -1:
        print("Text: {!r} not found in {!r}".format(text_beg, filepath_authors))
        sys.exit(1)
    if text_end_index == -1:
        print("Text: {!r} not found in {!r}".format(text_end, filepath_authors))
        sys.exit(1)

    text_beg_index += len(text_beg)

    with open(filepath_authors, "w", encoding="utf-8") as fh:
        fh.write(authors_contents[:text_beg_index])
        fh.write(authors_text)
        fh.write(authors_contents[text_end_index:])

    print("Updated:", filepath_authors)


if __name__ == "__main__":
    main()
