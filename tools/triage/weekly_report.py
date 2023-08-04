#!/usr/bin/env python3
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
from gitea_utils import gitea_json_activities_get, gitea_json_issue_get, gitea_json_issue_events_filter, git_username_detect


def argparse_create():
    parser = argparse.ArgumentParser(
        description="Generate Weekly Report",
        epilog="This script is typically used to help write weekly reports")

    parser.add_argument(
        "--username",
        dest="username",
        metavar='USERNAME',
        type=str,
        required=False,
        help="")

    parser.add_argument(
        "-v",
        "--verbose",
        action="store_true",
        help="increase output verbosity")

    return parser


def report_personal_weekly_get(username, start, verbose=True):

    data_cache = {}

    def gitea_json_issue_get_cached(issue_fullname):
        if issue_fullname not in data_cache:
            data_cache[issue_fullname] = gitea_json_issue_get(issue_fullname)

        return data_cache[issue_fullname]

    pulls_closed = set()
    pulls_commented = set()
    pulls_created = set()

    issues_closed = set()
    issues_commented = set()
    issues_created = set()

    pulls_reviewed = []

    issues_confirmed = []
    issues_needing_user_info = []
    issues_needing_developer_info = []
    issues_fixed = []
    issues_duplicated = []
    issues_archived = []

    commits_main = []

    for i in range(7):
        date_curr = start + datetime.timedelta(days=i)
        date_curr_str = date_curr.strftime("%Y-%m-%d")
        print(f"Requesting activity of {date_curr_str}", end="\r", flush=True)
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
            elif op_type == "commit_repo":
                if activity["ref_name"] == "refs/heads/main":
                    content_json = json.loads(activity["content"])
                    repo_name = activity["repo"]["name"]
                    for commits in content_json["Commits"]:
                        title = commits["Message"].split('\n', 1)[0]

                        # Substitute occurrences of "#\d+" with "{{Issue|\d+|repo}}"
                        title = re.sub(r"#(\d+)", rf"{{{{Issue|\1|{repo_name}}}}}", title)

                        hash_value = commits["Sha1"][:10]
                        commits_main.append(f"{title} ({{{{GitCommit|{hash_value}|{repo_name}}}}})")

    date_end = date_curr
    len_total = len(issues_closed) + len(issues_commented) + len(pulls_commented)
    process = 0
    for issue in issues_commented:
        print(f"[{int(100 * (process / len_total))}%] Checking issue {issue}       ", end="\r", flush=True)
        process += 1

        issue_events = gitea_json_issue_events_filter(issue,
                                                      date_start=start,
                                                      date_end=date_end,
                                                      username=username,
                                                      labels={
                                                          "Status/Confirmed",
                                                          "Status/Needs Information from User",
                                                          "Status/Needs Info from Developers"})

        for event in issue_events:
            label_name = event["label"]["name"]
            if label_name == "Status/Confirmed":
                issues_confirmed.append(issue)
            elif label_name == "Status/Needs Information from User":
                issues_needing_user_info.append(issue)
            elif label_name == "Status/Needs Info from Developers":
                issues_needing_developer_info.append(issue)

    for issue in issues_closed:
        print(f"[{int(100 * (process / len_total))}%] Checking issue {issue}       ", end="\r", flush=True)
        process += 1

        issue_events = gitea_json_issue_events_filter(issue,
                                                      date_start=start,
                                                      date_end=date_end,
                                                      username=username,
                                                      event_type={"close", "commit_ref"},
                                                      labels={"Status/Duplicate"})

        for event in issue_events:
            event_type = event["type"]
            if event_type == "commit_ref":
                issues_fixed.append(issue)
            elif event_type == "label":
                issues_duplicated.append(issue)
            else:
                issues_archived.append(issue)

    for pull in pulls_commented:
        print(f"[{int(100 * (process / len_total))}%] Checking pull {pull}         ", end="\r", flush=True)
        process += 1

        pull_events = gitea_json_issue_events_filter(pull.replace("pulls", "issues"),
                                                     date_start=start,
                                                     date_end=date_end,
                                                     username=username,
                                                     event_type={"comment"})

        if pull_events:
            pull_data = gitea_json_issue_get_cached(pull)
            if pull_data["user"]["login"] != username:
                pulls_reviewed.append(pull)

    # Print triaging stats

    issues_involved = issues_closed | issues_commented | issues_created

    print("\'\'\'Involved in %s reports:\'\'\'                                     " % len(issues_involved))
    print("* Confirmed: %s" % len(issues_confirmed))
    print("* Closed as Resolved: %s" % len(issues_fixed))
    print("* Closed as Archived: %s" % len(issues_archived))
    print("* Closed as Duplicate: %s" % len(issues_duplicated))
    print("* Needs Info from User: %s" % len(issues_needing_user_info))
    print("* Needs Info from Developers: %s" % len(issues_needing_developer_info))
    print("* Actions total: %s" % (len(issues_closed) + len(issues_commented) + len(issues_created)))
    print()

    # Print review stats
    def print_pulls(pulls):
        for pull in pulls:
            pull_data = gitea_json_issue_get_cached(pull)
            title = pull_data["title"]
            _, repo, _, number = pull.split('/')
            print(f"* {{{{PullRequest|{number}|{repo}}}}}: {title}")

    print("'''Review: %s'''" % len(pulls_reviewed))
    print_pulls(pulls_reviewed)
    print()

    # Print created diffs
    print("'''Created pulls: %s'''" % len(pulls_created))
    print_pulls(pulls_created)
    print()

    # Print commits
    print("'''Commits:'''")
    for commit in commits_main:
        print("*", commit)
    print()

    if verbose:
        # Debug

        def print_links(issues):
            for fullname in issues:
                print(f"https://projects.blender.org/{fullname}")

        print("Debug:")
        print(f"Activities from {start.isoformat()} to {date_end.isoformat()}:")
        print()
        print("Pulls Created:")
        print_links(pulls_created)
        print("Pulls Reviewed:")
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

    #end_date = datetime.datetime(2020, 3, 14)
    end_date = datetime.datetime.now()
    weekday = end_date.weekday()

    # Assuming I am lazy and making this at last moment or even later in worst case
    if weekday < 2:
        time_delta = 7 + weekday
        start_date = end_date - datetime.timedelta(days=time_delta, hours=end_date.hour)
        end_date -= datetime.timedelta(days=weekday, hours=end_date.hour)
    else:
        time_delta = weekday
        start_date = end_date - datetime.timedelta(days=time_delta, hours=end_date.hour)

    # Ensure friday :)
    friday = start_date + datetime.timedelta(days=4)
    week = start_date.isocalendar()[1]
    start_date_str = start_date.strftime('%b %d')
    end_date_str = friday.strftime('%b %d')

    print("== Week %d (%s - %s) ==\n\n" % (week, start_date_str, end_date_str))
    report_personal_weekly_get(username, start_date, verbose=args.verbose)


if __name__ == "__main__":
    main()

    # wait for input to close window
    input()
