#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
Example use to generate all credits:

   credits_git_gen.py

Example use a custom range:

   credits_git_gen.py --source=/src/blender --range=SHA1..HEAD
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
    Tuple,
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
# Class for generating credits

class CreditUser:
    __slots__ = (
        "commit_total",
        "year_min",
        "year_max",
    )

    def __init__(self) -> None:
        self.commit_total = 0
        self.year_min = 0
        self.year_max = 0


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
                user.year_min = min(user.year_min, user_other.year_min)
                user.year_max = max(user.year_max, user_other.year_max)
        other.users.clear()

    def process_commit(self, c: GitCommit) -> None:
        self.process_commits_count += 1

        if not self.is_credit_commit_valid(c):
            return

        authors = self.commit_authors_get(c)
        year = c.date.year
        for author in authors:
            cu = self.users.get(author)
            if cu is None:
                cu = self.users[author] = CreditUser()
                cu.year_min = year
                cu.year_max = year

            cu.commit_total += 1
            cu.year_min = min(cu.year_min, year)
            cu.year_max = max(cu.year_max, year)

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
            is_main_credits: bool = True,
            contrib_companies: Tuple[str, ...] = (),
            sort: str = "name",
            use_email: bool = False,
    ) -> None:
        commit_word = "commit", "commits"

        sorted_authors = {}
        if sort == "commit":
            sorted_authors = dict(sorted(self.users.items(), key=lambda item: item[1].commit_total))
        else:
            sorted_authors = dict(sorted(self.users.items()))

        fh.write("<h3>Individual Contributors</h3>\n\n")
        for author, cu in sorted_authors.items():
            fh.write("{:s}, {:,d} {:s} {:s}<br />\n".format(
                author if use_email else author.split("<", 1)[0].rstrip(),
                cu.commit_total,
                commit_word[cu.commit_total > 1],
                (
                    "" if not is_main_credits else (
                        "- {:d}".format(cu.year_min) if cu.year_min == cu.year_max else
                        "({:d} - {:d})".format(cu.year_min, cu.year_max)
                    )
                ),
            ))

        # -------------------------------------------------------------------------
        # Companies, hard coded
        if is_main_credits:
            fh.write("<h3>Contributions from Companies & Organizations</h3>\n")
            fh.write("<p>\n")
            for line in contrib_companies:
                fh.write("{:s}<br />\n".format(line))
            fh.write("</p>\n")

            import datetime
            now = datetime.datetime.now()
            fn = os.path.basename(__file__)
            fh.write(
                "<p><center><i>Generated by '{:s}' {:d}/{:d}/{:d}</i></center></p>\n".format(
                    fn, now.year, now.month, now.day,
                ))

    def write(
            self,
            filepath: str,
            *,
            is_main_credits: bool = True,
            contrib_companies: Tuple[str, ...] = (),
            sort: str = "name",
            use_email: bool = False,
    ) -> None:
        with open(filepath, 'w', encoding="ascii", errors='xmlcharrefreplace') as fh:
            self.write_object(
                fh,
                is_main_credits=is_main_credits,
                contrib_companies=contrib_companies,
                sort=sort,
                use_email=use_email,
            )


def argparse_create() -> argparse.ArgumentParser:

    # When `--help` or no arguments are given, print this help.
    usage_text = "Review revisions."

    epilog = "This script is used to generate credits"

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
        "--range",
        dest="range_sha1",
        metavar='SHA1_RANGE',
        required=False,
        default="HEAD",
        help="Range to use, eg: 169c95b8..HEAD or HEAD for all history.",
    )

    parser.add_argument(
        "--sort",
        dest="sort",
        metavar='METHOD',
        required=False,
        help="Sort credits by 'name' (default) or 'commit'",
    )
    # Don't include email addresses, they're useful for identifying developers but better not include in credits.
    # Even though it's publicly available, developers may not want this so easily accessible on the credits page.
    parser.add_argument(
        "--use-email",
        dest="use_email",
        required=False,
        action='store_true',
        help="Include the email address in the credits (useful for debugging/investigating issues)",
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

    return parser


def main() -> None:

    # ----------
    # Parse Args

    args = argparse_create().parse_args()

    # TODO, there are for sure more companies then are currently listed.
    # 1 liners for in html syntax
    contrib_companies = (
        "<b>Unity Technologies</b> - FBX Exporter",
        "<b>BioSkill GmbH</b> - H3D compatibility for X3D Exporter, OBJ Nurbs Import/Export",
        "<b>AutoCRC</b> - Improvements to fluid particles, vertex color baking",
        "<b>Adidas</b> - Principled BSDF shader in Cycles",
        "<b>AMD</b> - Cycles HIP GPU rendering, CPU optimizations",
        "<b>Intel</b> - Cycles oneAPI GPU rendering, CPU optimizations",
        "<b>NVIDIA</b> - Cycles OptiX GPU rendering, USD importer",
        "<b>Facebook</b> - Cycles subsurface scattering improvements",
        "<b>Apple</b> - Cycles Metal GPU backend",
    )

    credits = Credits()
    # commit_range = "HEAD~10..HEAD"
    # commit_range = "blender-v2.81-release..blender-v2.82-release"
    # commit_range = "blender-v2.82-release"
    commit_range = args.range_sha1
    sort = args.sort
    jobs = args.jobs
    if jobs <= 0:
        # Clamp the value, higher values give errors with too many open files.
        # Allow users to manually pass very high values in as they might want to tweak system limits themselves.
        jobs = min(multiprocessing.cpu_count() * 2, 400)

    credits.process(GitCommitIter(args.source_dir, commit_range), jobs=jobs)

    credits.write(
        "credits.html",
        is_main_credits=True,
        contrib_companies=contrib_companies,
        sort=sort,
        use_email=args.use_email,
    )
    print("Written: credits.html")


if __name__ == "__main__":
    main()
