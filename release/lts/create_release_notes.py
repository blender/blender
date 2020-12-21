#!/usr/bin/env python3

import argparse
import phabricator


DESCRIPTION = ("This python script is used to generate the release notes "
               "which we can copy-paste directly into the CMS of "
               "www.blender.org and stores.")
USAGE = "./create_release_notes.py --task=T77348 --version=2.83.7"


class ReleaseLogLine:
    """
    Class containing the information of a single line of the release log

    Instance attributes:

    * line: (str) the original line used to create this log line
    * task_id: (int or None) the extracted task id associated with this log
               line. Can be None if the log line isn't associated with a task.
    * commit_id: (str or None) the extracted commit id associated with this log
               line. Only filled when no `task_id` could be found.
    * ref: (str) `task_id` or `commit_id` of this line, including `T` for tasks
            or `D` for diffs.
    * title: (str) title of this log line. When constructed this attribute is
            an empty string. The called needs to retrieve the title from the
            backend.
    * url: (str) url of the ticket task or commit.
    """
    def __init__(self, line: str):
        self.line=line
        items = line.split("|")
        self.task_id = None
        self.commit_id = None
        try:
            task_id = int(items[1].strip()[1:])
            self.task_id = task_id
            self.ref = f"T{self.task_id}"
        except ValueError:
            # no task
            commit_string = items[3].strip()
            commits = commit_string.split(",")
            commit_id = commits[0]
            commit_id = commit_id.replace("{", "").replace("}", "")
            if not commit_id.startswith("rB"):
                commit_id = f"rB{commit_id}"
            self.commit_id = commit_id

            self.ref = f"{self.commit_id}"

        self.title = ""
        self.url = f"https://developer.blender.org/{self.ref}"
    
    def __format_as_html(self)-> str:
        return f"  <li>{self.title} [<a href=\"{self.url}\">{self.ref}</a>]</li>"

    def __format_as_text(self) ->str:
        return f"* {self.title} [{self.ref}]"

    def __format_as_steam(self) -> str:
        return f"* {self.title} ([url={self.url}]{self.ref}[/url])"

    def __format_as_wiki(self) -> str:
        if self.task_id:
            return f"* {self.title} [{{{{BugReport|{self.task_id}}}}}]"
        else:
            return f"* {self.title} [{{{{GitCommit|{self.commit_id[2:]}}}}}]"

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
        elif format == 'wiki':
            return self.__format_as_wiki()
        else:
            return self.__format_as_text()


def format_title(title: str) -> str:
    title = title.strip()
    if not title.endswith("."):
        title = title + "."
    return title


def extract_release_notes(version: str, task_id: int):
    """
    Extract all release notes logs

    # Process

    1. Retrieval of description of the gived `task_id`.
    2. Find rows for the given `version` and convert to `ReleaseLogLine`.
    3. based on the associated task or commit retrieves the title of the log
       line.
    """
    phab = phabricator.Phabricator()
    phab.update_interfaces()
    task = phab.maniphest.info(task_id=task_id)
    description = task["description"]
    lines = description.split("\n")
    start_index = lines.index(f"## Blender {version} ##")
    lines = lines[start_index+1:]
    for line in lines:
        if not line.strip():
            continue
        if line.startswith("| **Report**"):
            continue
        if line.startswith("## Blender"):
            break

        log_line = ReleaseLogLine(line)
        if log_line.task_id:
            issue_task = phab.maniphest.info(task_id=log_line.task_id)
            log_line.title = format_title(issue_task.title)
            yield log_line
        elif log_line.commit_id:
            commits = phab.diffusion.commit.search(constraints={"identifiers":[log_line.commit_id]})
            commit = commits.data[0]
            commit_message = commit['fields']['message']
            commit_title = commit_message.split("\n")[0]
            log_line.title = format_title(commit_title)
            yield log_line


def print_release_notes(version: str, format: str, task_id: int):
    """
        Generate and print the release notes to the console.
    """
    if format == 'html':
        print("<ul>")
    if format == 'steam':
        print("[ul]")
    for log_item in extract_release_notes(version=version, task_id=task_id):
        print(log_item.format(format=format))
    if format == 'html':
        print("</ul>")
    if format == 'steam':
        print("[/ul]")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=DESCRIPTION, usage=USAGE)
    parser.add_argument(
        "--version",
        required=True,
        help="Version string in the form of {major}.{minor}.{build} (e.g. 2.83.7)")
    parser.add_argument(
        "--task",
        required=True,
        help="Phabricator ticket that is contains the release notes information (e.g. T77348)")
    parser.add_argument(
        "--format",
        help="Format the result in `text`, `steam`, `wiki` or `html`",
        default="text")
    args = parser.parse_args()

    print_release_notes(version=args.version, format=args.format, task_id=int(args.task[1:]))
