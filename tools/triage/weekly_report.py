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
__all__ = (
    "main",
)


import argparse
import datetime
import json
import re
import shutil
import sys

from dataclasses import dataclass, field

from gitea_utils import (
    gitea_json_activities_get,
    gitea_json_pull_request_by_base_and_head_get,
    gitea_json_issue_events_filter,
    gitea_json_issue_get,
    gitea_user_get, git_username_detect,
)

from typing import (
    Any,
)
from collections.abc import (
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
        del text


def argparse_create() -> argparse.ArgumentParser:

    def str_as_isodate(value: str) -> datetime.datetime:
        try:
            value_as_date = datetime.datetime.fromisoformat(value)
        except Exception as ex:
            raise argparse.ArgumentTypeError("Must be a valid ISO date (YYYY-MM-DD), failed: {!s}".format(ex))
        return value_as_date

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
        "--date",
        dest="date",
        type=str_as_isodate,
        default=None,
        help="Show only for this day (YYYY-MM-DD), and not for an entire week."
    )

    parser.add_argument(
        "--hash-length",
        dest="hash_length",
        type=int,
        default=10,
        help="Number of characters to abbreviate the hash to (0 to disable).",
    )

    parser.add_argument(
        "-v",
        "--verbose",
        action="store_true",
        help="increase output verbosity",
    )

    return parser


def report_personal_weekly_get(
        username: str,
        start: datetime.datetime,
        num_days: int,
        *,
        hash_length: int,
        verbose: bool = True,
) -> None:

    data_cache: dict[str, dict[str, Any]] = {}

    def gitea_json_issue_get_cached(issue_fullname: str) -> dict[str, Any]:
        if issue_fullname not in data_cache:
            issue = gitea_json_issue_get(issue_fullname)
            data_cache[issue_fullname] = issue

        return data_cache[issue_fullname]

    pulls_closed: set[str] = set()
    pulls_commented: set[str] = set()
    pulls_created: set[str] = set()

    issues_closed: set[str] = set()
    issues_commented: set[str] = set()
    issues_created: set[str] = set()

    pulls_reviewed: list[str] = []

    issues_confirmed: list[str] = []
    issues_needing_user_info: list[str] = []
    issues_needing_developer_info: list[str] = []
    issues_fixed: list[str] = []
    issues_duplicated: list[str] = []
    issues_archived: list[str] = []

    @dataclass
    class Branch:
        # Name of the repository owning the branch (which can differ from the repository targeted by this branch!)
        repository_full_name: str
        commits: list[str]

    @dataclass
    class PullRequest:
        title_str: str

    @dataclass
    class Repository:
        name: str
        # Branches targeting this repository. Branch name is key.
        branches: dict[str, Branch] = field(default_factory=dict)
        # Pull requests targeting this repository. Key is repository of the branch and the branch name.
        prs: dict[tuple[str, str], PullRequest] = field(default_factory=dict)

    # Repositories containing any commit activity, identified by full name (e.g. "blender/blender").
    repositories: dict[str, Repository] = {}

    user_data: dict[str, Any] = gitea_user_get(username)

    for i in range(num_days):
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
                        activity["content"] and
                        activity["repo"]["name"] != ".profile"
                ):
                    content_json = json.loads(activity["content"])
                    assert isinstance(content_json, dict)
                    repo = activity["repo"]
                    repo_fullname = repo["full_name"]
                    content_json_commits: list[dict[str, Any]] = content_json["Commits"]
                    for commit_json in content_json_commits:
                        # Skip commits that were not made by this user. Using email doesn't seem to
                        # be possible unfortunately.
                        if commit_json["AuthorName"] != user_data["full_name"]:
                            continue

                        title = commit_json["Message"].split('\n', 1)[0]

                        if title.startswith("Merge branch "):
                            continue

                        hash_value = commit_json["Sha1"]
                        if hash_length > 0:
                            hash_value = hash_value[:hash_length]

                        branch_name = activity["ref_name"].removeprefix("refs/heads/")
                        is_release_branch = re.match(r"^blender-v(?:\d+\.\d+)(?:\.\d+)?-release$", branch_name)

                        pr = None

                        # The PR workflow means branches and PRs are owned by a user's repository instead of the
                        # repository they are made for. For weekly reports it makes more sense to keep all branches and
                        # PRs related to a single repository together, regardless of who happens to own them.
                        #
                        # So the following adds branches and PRs to a "target" repository, not the owning one.
                        target_repo_json = repo.get("parent", repo)
                        target_repo_fullname = target_repo_json["full_name"] if target_repo_json else repo_fullname

                        # Substitute occurrences of "#\d+" with "repo#\d+"
                        title = re.sub(r"#(\d+)", rf"{target_repo_fullname}#\1", title)

                        if target_repo_fullname not in repositories:
                            repositories[target_repo_fullname] = Repository(target_repo_fullname)
                        target_repo = repositories[target_repo_fullname]

                        if branch_name not in target_repo.branches:
                            target_repo.branches[branch_name] = Branch(repo_fullname, [])
                            # If we see this branch for the first time, try to find a PR for it. Only catches PRs made
                            # against the default branch of the target repository.
                            if not is_release_branch and target_repo_json:
                                pr = gitea_json_pull_request_by_base_and_head_get(
                                    target_repo_fullname,
                                    target_repo_json["default_branch"],
                                    f"{repo_fullname}:{branch_name}",
                                )
                        branch = target_repo.branches[branch_name]

                        if pr:
                            pr_title = pr["title"]
                            pr_id = pr["number"]
                            target_repo.prs[(repo_fullname, branch_name)
                                            ] = PullRequest(f"{pr_title} ({target_repo_fullname}!{pr_id})")

                        branch.commits.append(f"{title} ({repo_fullname}@{hash_value})")

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

    nice_repo_names = {
        "blender/blender-developer-docs": "Developer Documentation",
        "blender/blender-manual": "Blender Manual",
    }

    def print_repo(repo: Repository, indent_level: int = 0) -> None:
        # Print main branch commits immediately, no need to add extra section.
        main_branch = repo.branches.get("main")
        if main_branch:
            for commit in main_branch.commits:
                print("{:s}* {:s}".format("  " * indent_level, commit))

        for branch_name, branch in repo.branches.items():
            # Main branch already printed above.
            if branch_name == "main":
                continue

            pr = repo.prs.get((branch.repository_full_name, branch_name))
            if pr:
                print("{:s}* {:s}".format("  " * indent_level, pr.title_str))
            else:
                print("{:s}* {:s}:{:s}".format("  " * indent_level, branch.repository_full_name, branch_name))

            for commit in branch.commits:
                print("  {:s}* {:s}".format("  " * indent_level, commit))

    # Print commits
    print("**Commits:**")
    # Print main branch commits from blender/blender first.
    blender_repo = repositories.get("blender/blender")
    if blender_repo:
        print_repo(blender_repo)

    for repo in repositories.values():
        # Blender repo already handled above.
        if repo.name == "blender/blender":
            continue

        # For some repositories we know a nicer name to display (e.g. "blender/blender-manual" -> "Blender Manual")
        nice_repo_name = nice_repo_names.get(repo.name, repo.name)
        print(f"* {nice_repo_name}:")
        print_repo(repo, indent_level=1)

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

    if args.date:
        num_days = 1  # Show only one day.
        start_date = args.date
        start_date_str = start_date.strftime('%B ') + str(start_date.day)

        print(f"## {start_date_str}\n")
    else:
        num_days = 7  # Show an entire week.
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

    report_personal_weekly_get(
        username,
        start_date,
        num_days,
        hash_length=args.hash_length,
        verbose=args.verbose,
    )


if __name__ == "__main__":
    main()

    # Wait for input to close window.
    if IS_ATTY:
        input()
