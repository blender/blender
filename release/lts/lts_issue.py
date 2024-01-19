#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import requests


class ReleaseLogLine:
    """
    Class containing the information of a single line of the release log

    Instance attributes:

    * line: (str) the original line used to create this log line
    * issue_id: (int or None) the extracted issue id associated with this log
               line. Can be None if the log line isn't associated with a issue.
    * commit_id: (str or None) the extracted commit id associated with this log
               line. Only filled when no `issue_id` could be found.
    * ref: (str) `issue_id` or `commit_id` of this line, including `T` for issues
            or `D` for diffs.
    * title: (str) title of this log line. When constructed this attribute is
            an empty string. The called needs to retrieve the title from the
            backend.
    * url: (str) url of the ticket issue or commit.
    """

    def __init__(self, line: str):
        self.line = line
        items = line.split("|")
        self.issue_id = None
        self.issue_repo = None
        self.commit_id = None
        self.commit_repo = None
        base_url = "https://projects.blender.org"
        try:
            issue_tokens = items[1].strip().split("#")
            if len(issue_tokens[0]) > 0:
                self.issue_repo = issue_tokens[0]
                self.issue_id = issue_tokens[1]
            else:
                self.issue_repo = "blender/blender"
                self.issue_id = issue_tokens[1]

            self.ref = f"#{self.issue_id}"
            self.url = f"{base_url}/{self.issue_repo}/issues/{self.issue_id}"
        except IndexError:
            # no issue
            commit_string = items[3].strip()
            commit_string = commit_string.split(",")[0]
            commit_string = commit_string.split("]")[0]
            commit_string = commit_string.replace("[", "")

            commit_tokens = commit_string.split("@")
            if len(commit_tokens) > 1:
                self.commit_repo = commit_tokens[0]
                self.commit_id = commit_tokens[1]
            else:
                self.commit_repo = "blender/blender"
                self.commit_id = commit_tokens[0]

            self.ref = f"{self.commit_id}"
            self.url = f"{base_url}/{self.commit_repo}/commit/{self.commit_id}"

        self.title = ""

    def __format_as_html(self) -> str:
        return f"  <li>{self.title} [<a href=\"{self.url}\">{self.ref}</a>]</li>"

    def __format_as_text(self) -> str:
        return f"* {self.title} [{self.ref}]"

    def __format_as_steam(self) -> str:
        return f"* {self.title} ([url={self.url}]{self.ref}[/url])"

    def __format_as_markdown(self) -> str:
        if self.issue_id:
            return f"* {self.title} ({self.issue_repo}#{self.issue_id})"
        else:
            return f"* {self.title} ({self.commit_repo}@{self.commit_id})"

    def format(self, format: str) -> str:
        """
        Format this line

        :attr format: the desired format. Possible values are 'text', 'steam' or 'html'
        :type string:
        """
        if format == 'html':
            return self.__format_as_html()
        elif format == 'steam':
            return self.__format_as_steam()
        elif format == 'markdown':
            return self.__format_as_markdown()
        else:
            return self.__format_as_text()


def format_title(title: str) -> str:
    title = title.strip()
    if not title.endswith("."):
        title = title + "."
    return title


def extract_release_notes(version: str, issue: str):
    """
    Extract all release notes logs

    # Process

    1. Retrieval of description of the given `issue_id`.
    2. Find rows for the given `version` and convert to `ReleaseLogLine`.
    3. based on the associated issue or commit retrieves the title of the log
       line.
    """
    base_url = "https://projects.blender.org/api/v1/repos"
    issues_url = base_url + "/blender/blender/issues/"
    headers = {'accept': 'application/json'}

    response = requests.get(issues_url + issue[1:], headers=headers)
    description = response.json()["body"]

    lines = description.split("\n")
    start_index = lines.index(f"## Blender {version}")
    lines = lines[start_index + 1:]
    for line in lines:
        if not line.strip():
            continue
        if line.startswith("| **Report**"):
            continue
        if line.startswith("## Blender"):
            break
        if line.find("| -- |") != -1:
            continue

        log_line = ReleaseLogLine(line)
        if log_line.issue_id:
            issue_url = f"{base_url}/{log_line.issue_repo}/issues/{log_line.issue_id}"
            response = requests.get(issue_url, headers=headers)
            if response.status_code != 200:
                raise ValueError("Issue not found: " + str(log_line.issue_id))

            log_line.title = format_title(response.json()["title"])
            yield log_line
        elif log_line.commit_id:
            commit_url = f"{base_url}/{log_line.commit_repo}/git/commits/{log_line.commit_id}"
            response = requests.get(commit_url, headers=headers)
            if response.status_code != 200:
                raise ValueError("Commit not found: " + log_line.commit_id)

            commit_message = response.json()['commit']['message']
            commit_title = commit_message.split("\n")[0]
            log_line.title = format_title(commit_title)
            yield log_line


def print_notes(version: str, format: str, issue: str):
    """
        Generate and print the release notes to the console.
    """
    if format == 'html':
        print("<ul>")
    if format == 'steam':
        print("[ul]")
    for log_item in extract_release_notes(version=version, issue=issue):
        print(log_item.format(format=format))
    if format == 'html':
        print("</ul>")
    if format == 'steam':
        print("[/ul]")
