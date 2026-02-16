#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

r"""
### What this script does
This script looks for Fix #NUMBER commits in the current branch between
a start and end date. It then iterates through each commit figuring
out which issue it fixed and what module that issue belonged too.

Finally it prints the list of modules and the corresponding fix numbers
to terminal.

The steps to use it as as follows:
- Change the terminal to the Blender repository and make sure it's on
the branch you're interested in (E.g. main) and up to date.
- Run the script with `python bug_fixes_per_module.py -s YYYY-MM-DD -e YYYY-MM-DD`
  - `-s` and `-e` are the start and end dates you want to checkout.
- Wait for the script to finish.

Limitation:
Because the script is only looking at commits that contain `Fix #NUMBER`
in them, this will not gather a full list of fix commits. It will just
gather a list of commits that fixed reported issues.

Related to this, if the commit message contains the wrong issue number in
`Fix #NUMBER`, then the commit will be sorted improperly.
"""

__all__ = (
    "main",
)

import re
import sys
import argparse
import subprocess
import multiprocessing

from typing import Any
from time import time

from gitea_utils import url_json_get, BASE_API_URL


# -----------------------------------------------------------------------------
# Constant used throughout the script

UNKNOWN_MODULE = "UNKNOWN_MODULE"


# -----------------------------------------------------------------------------
# Commit Info Class


class CommitInfo():
    def __init__(self, commit_line: str) -> None:
        split_message = commit_line.split()

        # Commit line is in the format:
        # COMMIT_HASH Title of commit
        self.hash = split_message[0]

        self.fixed_reports: list[str] = []
        self.check_full_commit_message_for_fixed_reports()

        self.module = UNKNOWN_MODULE

    def check_full_commit_message_for_fixed_reports(self) -> None:
        command = ['git', 'show', '-s', '--format=%B', self.hash]
        command_output = subprocess.run(command, capture_output=True).stdout.decode('utf-8')

        if "revert" in command_output.lower():
            # If "revert" is the commit message, then it's probably a revert commit and didn't fix a issue.
            return

        # Find every instance of #NUMBER. These are the report that the commit claims to fix.
        issue_match = re.findall(r'#(\d+)\b', command_output)
        if issue_match:
            self.fixed_reports = issue_match

    def get_module(self, labels: list[dict[Any, Any]]) -> str:
        # Figures out what module the report that was fixed belongs too.
        for label in labels:
            if "module" in label['name'].lower():
                # Module labels are typically in the format Module/NAME.
                return " ".join(label['name'].split("/")[1:])

        return UNKNOWN_MODULE

    def classify(self) -> bool:
        commit_was_sorted = False
        for report_number in self.fixed_reports:
            report_information = url_json_get(f"{BASE_API_URL}/repos/blender/blender/issues/{report_number}")

            if report_information is None:
                # It might be `None` if bug report has been deleted.
                continue

            if isinstance(report_information, list):
                # List type is the wrong format.
                continue

            if "pull" in report_information['html_url']:
                # Pull requests aren't bug reports. So skip processing it.
                continue

            # The commit didn't exit early due to the criteria above, so it was correctly sorted.
            commit_was_sorted = True
            module = self.get_module(report_information['labels'])
            if module != UNKNOWN_MODULE:
                self.module = module
                break
        return commit_was_sorted

# -----------------------------------------------------------------------------
# Argument Parsing


def argparse_create() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawTextHelpFormatter)
    parser.add_argument(
        "-s",
        "--start",
        required=True,
        help=(
            "Date to start checking commits from. Must be in the format YYYY-MM-DD."
        ),
    )
    parser.add_argument(
        "-e",
        "--end",
        required=True,
        help=(
            "Date to stop checking commits. Must be in the format YYYY-MM-DD."
        ),
    )
    parser.add_argument(
        "-j",
        "--jobs",
        type=int,
        default=0,
        help=(
            "Number of threads to use when processing commit messages "
            "(Only really useful for debugging)."
        ),
    )

    return parser


def validate_arguments(args: argparse.Namespace) -> bool:
    def valid_date(date_string: str) -> bool:
        if len(date_string) == 0:
            print("Date is missing")
            print(date_string)
            return False

        split_date = date_string.split("-")
        if len(split_date) != 3:
            print("Date has too many or too few sections. It should be in the format YYYY-MM-DD")
            print(date_string)
            return False

        return True

    if not (valid_date(args.start) and valid_date(args.end)):
        return False

    return True

# -----------------------------------------------------------------------------


def setup_commit_info(commit: str) -> CommitInfo | None:
    commit_information = CommitInfo(commit)
    if len(commit_information.fixed_reports) > 0:
        return commit_information
    return None


def get_fix_commits(start_date: str, end_date: str, jobs: int) -> list[CommitInfo]:
    command = [
        'git',
        '--no-pager',
        'log',
        '--oneline',
        '--no-abbrev-commit',
        f'--since={start_date}',
        f'--until={end_date}',
        '-i',
        '-P',
        '--grep',
        r'Fix.*#+\d+',
    ]
    git_log_command_output = subprocess.run(command, capture_output=True).stdout.decode('utf-8')
    git_log_output = git_log_command_output.splitlines()

    # Gathering a list of commits is not compute intensive, it is time consuming due to hundreds of git log calls.
    # Multiprocessing can significantly reduce the time taken (E.g. 19s -> 4s on a 32 thread CPU).
    with multiprocessing.Pool(processes=jobs) as pool:
        list_of_commits = pool.map(setup_commit_info, git_log_output)

    return [commit for commit in list_of_commits if commit is not None]

# -----------------------------------------------------------------------------


def classify_commits(list_of_commits: list[CommitInfo]) -> list[CommitInfo]:
    number_of_commits = len(list_of_commits)

    print("Identifying which module the fix should be assigned too.")
    print("This requires querying information from Gitea which may take a while.\n")

    start_time = time()
    new_list_of_commits: list[CommitInfo] = []
    for i, commit in enumerate(list_of_commits, 1):
        # Progress bar.
        print(
            f"{i}/{number_of_commits} - Estimated time remaining:",
            f"{(((time() - start_time) / i) * (number_of_commits - i)) / 60:.1f} minutes",
            end="\r",
            flush=True
        )

        if commit.classify():
            # Only add commit to list if it was sorted.
            # If it wasn't sorted, then it probably means the commit "fixed" a pull request.
            new_list_of_commits.append(commit)

    # Print so we're away from the progress bar.
    print("\n\n\n")

    return new_list_of_commits

# -----------------------------------------------------------------------------


def print_info(list_of_commits: list[CommitInfo], start_date: str, end_date: str) -> None:
    print(f"Between {start_date} and {end_date}, there were a total of {len(list_of_commits)} Fix #NUMBER commits.")
    print("These are the numbers per module:\n")

    dict_of_modules_and_commits: dict[str, list[CommitInfo]] = {}

    for commit in list_of_commits:
        dict_of_modules_and_commits.setdefault(commit.module, []).append(commit)

    dict_of_modules_and_commits = dict(sorted(dict_of_modules_and_commits.items()))

    for module in dict_of_modules_and_commits:
        if module == UNKNOWN_MODULE:
            continue
        print(f"{module}: {len(dict_of_modules_and_commits[module])}")

    if UNKNOWN_MODULE in dict_of_modules_and_commits:
        unknown_commits = dict_of_modules_and_commits[UNKNOWN_MODULE]
        print(f"\nUnknown: {len(unknown_commits)}")
        print("Here is a list of the commits with unknown modules.")
        print("Go through each of the commit messages, find the bug reports they fixed, then update the module label.")
        for commit in unknown_commits:
            print(f"https://projects.blender.org/blender/blender/commit/{commit.hash}")


# -----------------------------------------------------------------------------


def main() -> int:
    args = argparse_create().parse_args()

    if not validate_arguments(args):
        return 0

    jobs = multiprocessing.cpu_count() if args.jobs < 1 else args.jobs

    list_of_commits = get_fix_commits(args.start, args.end, jobs)

    list_of_commits = classify_commits(list_of_commits)

    print_info(list_of_commits, args.start, args.end)

    return 0


if __name__ == "__main__":
    sys.exit(main())
