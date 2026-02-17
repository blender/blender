#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
# This script prints the URLs of all opened issues labeled
# "Status/Needs Information from User" by the specified user
# and last updated more than 7 days ago.

Example usage:

    python ./issues_needing_info.py --username mano-wii
"""
__all__ = (
    "main",
)

import argparse
import datetime
from gitea_utils import (
    git_username_detect,
    gitea_json_issue_events_filter,
    gitea_json_issues_search,
)


def print_needing_info_urls(username: str, before: str) -> None:

    print(f"Needs information from user before {before}:")

    label = "Status/Needs Information from User"
    issues_json = gitea_json_issues_search(
        type="issues",
        state="open",
        before=before,
        labels=label,
        verbose=True,
    )

    for issue in issues_json:
        fullname = issue["repository"]["full_name"]
        number = issue["number"]
        issue_events = gitea_json_issue_events_filter(
            f"{fullname}/issues/{number}",
            username=username,
            labels={label})

        if issue_events:
            print(issue["html_url"])

    print("concluded")


def main() -> None:

    parser = argparse.ArgumentParser(
        description="Print URL of Issues Needing Info",
        epilog="This script is typically used to help triaging")

    parser.add_argument(
        "--username",
        dest="username",
        type=str,
        required=False,
        help="Username registered in Gitea")

    args = parser.parse_args()
    username = args.username
    if not username:
        username = git_username_detect()
        if not username:
            return

    before_date = datetime.datetime.now() - datetime.timedelta(7)
    print_needing_info_urls(username, f"{before_date.isoformat()}Z")


if __name__ == "__main__":
    main()

    # wait for input to close window
    input()
