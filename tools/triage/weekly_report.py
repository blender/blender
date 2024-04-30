#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
# Generates the weekly report containing information on:
# - Pull Requests created,
# - Pull Requests revised,
# - Issues closed,
# - Issues confirmed,
# - Commits,

Example usage:

    python ./weekly_report.py --username mano-wii
"""

import argparse
import datetime
import json
import re
import shutil
import sys

from gitea_utils import (
    gitea_json_activities_get,
    gitea_json_issue_events_filter,
    gitea_json_issue_get,
    gitea_user_get, git_username_detect,
)

from typing import (
    Any,
    Dict,
    List,
    Set,
    Iterable,
)

# Support piping the output to a file or process.
IS_ATTY = sys.stdout.isatty()


if IS_ATTY:
    def print_progress(text: str) -> None:
        # The trailing space clears the previous output.
        term_width = shutil.get_terminal_size(fallback=(80, 20))[0]

        if (space := term_width - len(text)) > 0:
            text = text + (" " * space)
        print(text, end="\r", flush=True)
else:
    def print_progress(text: str) -> None:
        pass


def argparse_create() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Generate Weekly Report",
        epilog="This script is typically used to help write weekly reports",
    )

    parser.add_argument(
        "--username",
        dest="username",
        metavar='USERNAME',
        type=str,
        required=False,
        help="",
    )

    parser.add_argument(
        "--weeks-ago",
        dest="weeks_ago",
        type=int,
        default=1,
        help=(
            "Determine which week the report should be generated for. 0 means the current week. "
            "The default is 1, to create a report for the previous week."
        ),
    )

    parser.add_argument(
        "-v",
        "--verbose",
        action="store_true",
        help="increase output verbosity",
    )

    return parser


def report_personal_weekly_get(username: str, start: datetime.datetime, verbose: bool = True) -> None:

    data_cache: Dict[str, Dict[str, Any]] = {}

    def gitea_json_issue_get_cached(issue_fullname: str) -> Dict[str, Any]:
        if issue_fullname not in data_cache:
            issue = gitea_json_issue_get(issue_fullname)
            data_cache[issue_fullname] = issue

        return data_cache[issue_fullname]

    pulls_closed: Set[str] = set()
    pulls_commented: Set[str] = set()
    pulls_created: Set[str] = set()

    issues_closed: Set[str] = set()
    issues_commented: Set[str] = set()
    issues_created: Set[str] = set()

    pulls_reviewed: List[str] = []

    issues_confirmed: List[str] = []
    issues_needing_user_info: List[str] = []
    issues_needing_developer_info: List[str] = []
    issues_fixed: List[str] = []
    issues_duplicated: List[str] = []
    issues_archived: List[str] = []

    commits_main: List[str] = []

    user_data: Dict[str, Any] = gitea_user_get(username)

    for i in range(7):
        date_curr = start + datetime.timedelta(days=i)
        date_curr_str = date_curr.strftime("%Y-%m-%d")
        print_progress(f"Requesting activity of {date_curr_str}")
        for activity in gitea_json_activities_get(username, date_curr_str):
            op_type = activity["op_type"]
            if op_type == "close_issue":
                fullname = activity["repo"]["full_name"] + "/issues/" + activity["content"].split('|')[0]
                issues_closed.add(fullname)
            elif op_type == "comment_issue":
                fullname = activity["repo"]["full_name"] + "/issues/" + activity["content"].split('|')[0]
                issues_commented.add(fullname)
            elif op_type == "create_issue":
                fullname = activity["repo"]["full_name"] + "/issues/" + activity["content"].split('|')[0]
                issues_created.add(fullname)
            elif op_type == "merge_pull_request":
                fullname = activity["repo"]["full_name"] + "/pulls/" + activity["content"].split('|')[0]
                pulls_closed.add(fullname)
            elif op_type == "comment_pull":
                fullname = activity["repo"]["full_name"] + "/pulls/" + activity["content"].split('|')[0]
                pulls_commented.add(fullname)
            elif op_type == "create_pull_request":
                fullname = activity["repo"]["full_name"] + "/pulls/" + activity["content"].split('|')[0]
                pulls_created.add(fullname)
            elif op_type in {"approve_pull_request", "reject_pull_request"}:
                fullname = activity["repo"]["full_name"] + "/pulls/" + activity["content"].split('|')[0]
                pulls_reviewed.append(fullname)
            elif op_type == "commit_repo":
                if (
                        activity["ref_name"] == "refs/heads/main" and
                        activity["content"] and
                        activity["repo"]["name"] != ".profile"
                ):
                    content_json = json.loads(activity["content"])
                    assert isinstance(content_json, dict)
                    repo_fullname = activity["repo"]["full_name"]
                    content_json_commits: List[Dict[str, Any]] = content_json["Commits"]
                    for commits in content_json_commits:
                        # Skip commits that were not made by this user. Using email doesn't seem to
                        # be possible unfortunately.
                        if commits["AuthorName"] != user_data["full_name"]:
                            continue

                        title = commits["Message"].split('\n', 1)[0]

                        if title.startswith("Merge branch "):
                            continue

                        # Substitute occurrences of "#\d+" with "repo#\d+"
                        title = re.sub(r"#(\d+)", rf"{repo_fullname}#\1", title)

                        hash_value = commits["Sha1"][:10]
                        commits_main.append(f"{title} ({repo_fullname}@{hash_value})")

    date_end = date_curr
    len_total = len(issues_closed) + len(issues_commented) + len(pulls_commented)
    process = 0
    for issue in issues_commented:
        print_progress("[{:d}%] Checking issue {:s}".format(int(100 * (process / len_total)), issue))
        process += 1

        issue_events = gitea_json_issue_events_filter(
            issue,
            date_start=start,
            date_end=date_end,
            username=username,
            labels={
                "Status/Confirmed",
                "Status/Needs Information from User",
                "Status/Needs Info from Developers"
            }
        )

        for event in issue_events:
            label_name = event["label"]["name"]
            if label_name == "Status/Confirmed":
                issues_confirmed.append(issue)
            elif label_name == "Status/Needs Information from User":
                issues_needing_user_info.append(issue)
            elif label_name == "Status/Needs Info from Developers":
                issues_needing_developer_info.append(issue)

    for issue in issues_closed:
        print_progress("[{:d}%] Checking issue {:s}".format(int(100 * (process / len_total)), issue))
        process += 1

        issue_events = gitea_json_issue_events_filter(
            issue,
            date_start=start,
            date_end=date_end,
            username=username,
            event_type={"close", "commit_ref"},
            labels={"Status/Duplicate"},
        )

        for event in issue_events:
            event_type = event["type"]
            if event_type == "commit_ref":
                issues_fixed.append(issue)
            elif event_type == "label":
                issues_duplicated.append(issue)
            else:
                issues_archived.append(issue)

    for pull in pulls_commented:
        print_progress("[{:d}%] Checking pull {:s}".format(int(100 * (process / len_total)), pull))
        process += 1

        pull_events = gitea_json_issue_events_filter(
            pull.replace("pulls", "issues"),
            date_start=start,
            date_end=date_end,
            username=username,
            event_type={"comment"},
        )

        if pull_events:
            pull_data = gitea_json_issue_get_cached(pull)
            if pull_data["user"]["login"] != username:
                pulls_reviewed.append(pull)

    # Print triaging stats

    issues_involved = issues_closed | issues_commented | issues_created

    # Clear any progress.
    print_progress("")

    print("**Involved in {:d} reports:**".format(len(issues_involved)))
    print("* Confirmed: {:d}".format(len(issues_confirmed)))
    print("* Closed as Resolved: {:d}".format(len(issues_fixed)))
    print("* Closed as Archived: {:d}".format(len(issues_archived)))
    print("* Closed as Duplicate: {:d}".format(len(issues_duplicated)))
    print("* Needs Info from User: {:d}".format(len(issues_needing_user_info)))
    print("* Needs Info from Developers: {:d}".format(len(issues_needing_developer_info)))
    print("* Actions total: {:d}".format(len(issues_closed) + len(issues_commented) + len(issues_created)))
    print()

    # Print review stats
    def print_pulls(pulls: Iterable[str]) -> None:
        for pull in pulls:
            pull_data = gitea_json_issue_get_cached(pull)
            title = pull_data["title"]
            owner, repo, _, number = pull.split('/')
            print(f"* {title} ({owner}/{repo}!{number})")

    print("**Review: {:d}**".format(len(pulls_reviewed)))
    print_pulls(pulls_reviewed)
    print()

    # Print created diffs
    print("**Created Pull Requests: {:d}**".format(len(pulls_created)))
    print_pulls(pulls_created)
    print()

    # Print commits
    print("**Commits:**")
    for commit in commits_main:
        print("*", commit)
    print()

    if verbose:
        # Debug

        def print_links(issues: Iterable[str]) -> None:
            for fullname in issues:
                print(f"https://projects.blender.org/{fullname}")

        print("Debug:")
        print(f"Activities from {start.isoformat()} to {date_end.isoformat()}:")
        print()
        print("Pull Requests Created:")
        print_links(pulls_created)
        print("Pull Requests Reviewed:")
        print_links(pulls_reviewed)
        print("Issues Confirmed:")
        print_links(issues_confirmed)
        print("Issues Closed as Resolved:")
        print_links(issues_fixed)
        print("Issues Closed as Archived:")
        print_links(issues_closed)
        print("Issues Closed as Duplicate:")
        print_links(issues_duplicated)
        print("Issues Needing Info from User:")
        print_links(issues_needing_user_info)
        print("Issues Needing Info from Developers:")
        print_links(issues_needing_developer_info)


def main() -> None:
    # ----------
    # Parse Args
    args = argparse_create().parse_args()
    username = args.username
    if not username:
        username = git_username_detect()
        if not username:
            return

    # end_date = datetime.datetime(2020, 3, 14)
    end_date = datetime.datetime.now() - datetime.timedelta(weeks=(args.weeks_ago - 1))
    weekday = end_date.weekday()

    # Assuming I am lazy and making this at last moment or even later in worst case
    if weekday < 2:
        time_delta = 7 + weekday
        start_date = end_date - datetime.timedelta(days=time_delta, hours=end_date.hour)
        end_date -= datetime.timedelta(days=weekday, hours=end_date.hour)
    else:
        time_delta = weekday
        start_date = end_date - datetime.timedelta(days=time_delta, hours=end_date.hour)

    sunday = start_date + datetime.timedelta(days=6)
    # week = start_date.isocalendar()[1]
    start_date_str = start_date.strftime('%B ') + str(start_date.day)
    end_date_str = str(sunday.day) if start_date.month == sunday.month else sunday.strftime('%B ') + str(sunday.day)

    print(f"## {start_date_str} - {end_date_str}\n")
    report_personal_weekly_get(username, start_date, verbose=args.verbose)


if __name__ == "__main__":
    main()

    # Wait for input to close window.
    if IS_ATTY:
        input()
