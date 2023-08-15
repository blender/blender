#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
Example use:

   credits_git_gen.py --source=/src/blender --range=SHA1..HEAD
"""

import argparse
import multiprocessing
import re
import unicodedata

from git_log import (
    GitCommitIter,
    GitCommit,
)

from typing import (
    Dict,
    Tuple,
    Iterable,
    List,
)


# -----------------------------------------------------------------------------
# Lookup Table to clean up the credits
#
# This is a combination of unifying git logs as well as
# name change requested by the authors.

author_table = {
    "Aaron": "Aaron Carlisle",
    "Your Name": "Aaron Carlisle",
    "Alan": "Alan Troth",
    "andreas atteneder": "Andreas Atteneder",
    "Ankit": "Ankit Meel",
    "Antonioya": "Antonio Vazquez",
    "Antonio  Vazquez": "Antonio Vazquez",
    "Antony Ryakiotakis": "Antony Riakiotakis",
    "Amélie Fondevilla": "Amelie Fondevilla",
    "bastien": "Bastien Montagne",
    "mont29": "Bastien Montagne",
    "bjornmose": "Bjorn Mose",
    "meta-androcto": "Brendon Murphy",
    "Brecht van Lommel": "Brecht Van Lommel",
    "Brecht Van Lömmel": "Brecht Van Lommel",
    "recht Van Lommel": "Brecht Van Lommel",
    "ClÃ©ment Foucault": "Clément Foucault",
    "Clément": "Clément Foucault",
    "fclem": "Clément Foucault",
    "Clment Foucault": "Clément Foucault",
    "christian brinkmann": "Christian Brinkmann",
    "ZanQdo": "Daniel Salazar",
    "unclezeiv": "Davide Vercelli",
    "dilithjay": "Dilith Jayakody",
    "gaiaclary": "Gaia Clary",
    "DESKTOP-ON14TH5\\Sonny Campbell": "Sonny Campbell",
    "demeterdzadik@gmail.com": "Demeter Dzadik",
    "Diego Hernan Borghetti": "Diego Borghetti",
    "Dotsnov Valentin": "Dontsov Valentin",
    "Eitan": "Eitan Traurig",
    "EitanSomething": "Eitan Traurig",
    "Erik": "Erik Abrahamsson",
    "Erick Abrahammson": "Erik Abrahamsson",
    "Eric Abrahamsson": "Erik Abrahamsson",
    "Ethan-Hall": "Ethan Hall",
    "filedescriptor": "Falk David",
    "Germano": "Germano Cavalcante",
    "Germano Cavalcantemano-wii": "Germano Cavalcante",
    "mano-wii": "Germano Cavalcante",
    "gsr": "Guillermo S. Romero",
    "Henrik Dick (weasel)": "Henrik Dick",
    "howardt": "Howard Trickey",
    "Iliay Katueshenock": "Iliya Katueshenock",
    "MOD": "Iliya Katueshenock",
    "Inês Almeida": "Ines Almeida",
    "brita": "Ines Almeida",
    "Ivan": "Ivan Perevala",
    "jensverwiebe": "Jens Verwiebe",
    "Jesse Y": "Jesse Yurkovich",
    "Joe Eagar": "Joseph Eagar",
    "Johnny Matthews (guitargeek)": "Johnny Matthews",
    "guitargeek": "Johnny Matthews",
    "jon denning": "Jon Denning",
    "julianeisel": "Julian Eisel",
    "Severin": "Julian Eisel",
    "Alex Strand": "Kenzie Strand",
    "Kevin Dietrich": "Kévin Dietrich",
    "Leon Leno": "Leon Schittek",
    "Lukas Toenne": "Lukas Tönne",
    "Mikhail": "Mikhail Matrosov",
    "OmarSquircleArt": "Omar Emara",
    "lazydodo": "Ray Molenkamp",
    "Ray molenkamp": "Ray Molenkamp",
    "Author Name": "Robert Guetzkow",
    "Sybren A. StÃÂ¼vel": "Sybren A. Stüvel",
    "Simon": "Simon G",
    "Stephan": "Stephan Seitz",
    "Sebastian Herhoz": "Sebastian Herholz",
    "blender": "Sergey Sharybin",
    "Vuk GardaÅ¡eviÄ": "Vuk Gardašević",
    "ianwill": "Willian Padovani Germano",
    "Yiming Wu": "YimingWu",
}

# Mapping from a comit hash to additional authors.
# Fully overwrite authors gathered from git commit info.
# Intended usage: Correction of info stored in git commit itself.
# Note that the names of the authors here are assumed fully valid and usable as-is.
commit_authors_overwrite: Dict[bytes, Tuple[str, str]] = {
    # Format: {full_git_hash: (tuple, of, authors),}.
    # Example:
    # b"a60c1e5bb814078411ce105b7cf347afac6f2afd": ("Blender Authors",  "Suzanne", "Ton"),
}


# -----------------------------------------------------------------------------
# Multi-Processing

def process_commits_for_map(commits: Iterable[GitCommit]) -> "Credits":
    result = Credits()
    for c in commits:
        result.process_commit(c)
    return result


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
    )

    # Expected to cover the following formats (the e-mail address is not captured if present):
    #    `Co-authored-by: Blender Authors`
    #    `Co-authored-by: Blender Authors <foundation@blender.org>`
    #    `Co-authored-by: Blender Authors <Suzanne>`
    GIT_COMMIT_COAUTHORS_RE = re.compile(r"^Co-authored-by:[ \t]*(?P<author>[ \w\t]*\w)(?:$|[ \t]*<)", re.MULTILINE)

    def __init__(self) -> None:
        self.users: Dict[str, CreditUser] = {}

    @classmethod
    def commit_authors_get(cls, c: GitCommit) -> List[str]:
        if (authors_overwrite := commit_authors_overwrite.get(c.sha1, None)) is not None:
            # Ignore git commit info for these having an entry in commit_authors_overwrite.
            return [author_table.get(author, author) for author in authors_overwrite]

        authors = [c.author] + cls.GIT_COMMIT_COAUTHORS_RE.findall(c.body)
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

        total_commits = (max(len(chunk_list) - 1, 0) * chunk_size) + len(chunk)

        print("Found {:d} commits, processing...".format(total_commits))
        with multiprocessing.Pool(processes=jobs) as pool:
            for i, result in enumerate(pool.imap_unordered(process_commits_for_map, chunk_list)):
                print("{:d} of {:d}".format(i, len(chunk_list)))
                self.merge(result)

    def process(self, commit_iter: Iterable[GitCommit], *, jobs: int) -> None:
        if jobs > 1:
            self._process_multiprocessing(commit_iter, jobs=jobs)
            return

        # Simple single process operation.
        for i, c in enumerate(commit_iter):
            self.process_commit(c)
            if not (i % 100):
                print(i)

    def write(
            self,
            filepath: str,
            is_main_credits: bool = True,
            contrib_companies: Tuple[str, ...] = (),
            sort: str = "name",
    ) -> None:

        # patch_word = "patch", "patches"
        commit_word = "commit", "commits"

        sorted_authors = {}
        if sort == "commit":
            sorted_authors = dict(sorted(self.users.items(), key=lambda item: item[1].commit_total))
        else:
            sorted_authors = dict(sorted(self.users.items()))

        with open(filepath, 'w', encoding="ascii", errors='xmlcharrefreplace') as file:
            file.write("<h3>Individual Contributors</h3>\n\n")
            for author, cu in sorted_authors.items():
                file.write("{:s}, {:,d} {:s} {:s}<br />\n".format(
                    author,
                    cu.commit_total,
                    commit_word[cu.commit_total > 1],
                    ("" if not is_main_credits else
                     ("- {:d}".format(cu.year_min) if cu.year_min == cu.year_max else
                      ("({:d} - {:d})".format(cu.year_min, cu.year_max))))))

            # -------------------------------------------------------------------------
            # Companies, hard coded
            if is_main_credits:
                file.write("<h3>Contributions from Companies & Organizations</h3>\n")
                file.write("<p>\n")
                for line in contrib_companies:
                    file.write("{:s}<br />\n".format(line))
                file.write("</p>\n")

                import datetime
                now = datetime.datetime.now()
                fn = __file__.split("\\")[-1].split("/")[-1]
                file.write(
                    "<p><center><i>Generated by '{:s}' {:d}/{:d}/{:d}</i></center></p>\n".format(
                        fn, now.year, now.month, now.day
                    ))


def argparse_create() -> argparse.ArgumentParser:

    # When --help or no args are given, print this help
    usage_text = "Review revisions."

    epilog = "This script is used to generate credits"

    parser = argparse.ArgumentParser(description=usage_text, epilog=epilog)

    parser.add_argument(
        "--source", dest="source_dir",
        metavar='PATH',
        required=True,
        help="Path to git repository",
    )
    parser.add_argument(
        "--range",
        dest="range_sha1",
        metavar='SHA1_RANGE',
        required=True,
        help="Range to use, eg: 169c95b8..HEAD",
    )

    parser.add_argument(
        "--sort", dest="sort",
        metavar='METHOD',
        required=False,
        help="Sort credits by 'name' (default) or 'commit'",
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
        "<b>BioSkill GmbH</b> - H3D compatibility for X3D Exporter, "
        "OBJ Nurbs Import/Export",
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

    credits.write("credits.html",
                  is_main_credits=True,
                  contrib_companies=contrib_companies,
                  sort=sort)
    print("Written: credits.html")


if __name__ == "__main__":
    main()
