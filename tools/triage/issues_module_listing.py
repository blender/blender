#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2024 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
# This script prints the numbers of open issues per module.

Example usage:

    python ./issues_module_listing.py --severity High
"""

import argparse
import dataclasses
from datetime import date
from gitea_utils import gitea_json_issues_search


@dataclasses.dataclass
class ModuleInfo:
    name: str
    labelid: str
    buglist: list[str] = dataclasses.field(default_factory=list)
    buglist_full: list[str] = dataclasses.field(default_factory=list)


# Label names and IDs are taken from https://projects.blender.org/blender/blender/labels.
modules = {
    "Module/Animation & Rigging": ModuleInfo(name="Animation & Rigging", labelid="268"),
    "Module/Core": ModuleInfo(name="Core", labelid="269"),
    "Module/EEVEE & Viewport": ModuleInfo(name="EEVEE & Viewport", labelid="272"),
    "Module/Grease Pencil": ModuleInfo(name="Grease Pencil", labelid="273"),
    "Module/Modeling": ModuleInfo(name="Modeling", labelid="274"),
    "Module/Nodes & Physics": ModuleInfo(name="Nodes & Physics", labelid="275"),
    "Module/Pipeline, Assets & IO": ModuleInfo(name="Pipeline, Assets & I/O", labelid="276"),
    "Module/Platforms, Builds, Test & Devices": ModuleInfo(name="Platforms, Builds, Test & Devices", labelid="278"),
    "Module/Python API": ModuleInfo(name="Python API", labelid="279"),
    "Module/Render & Cycles": ModuleInfo(name="Render & Cycles", labelid="280"),
    "Module/Sculpt, Paint & Texture": ModuleInfo(name="Sculpt, Paint & Texture", labelid="281"),
    "Module/User Interface": ModuleInfo(name="User Interface", labelid="283"),
    "Module/VFX & Video": ModuleInfo(name="VFX & Video", labelid="284"),
}

base_url = (
    "https://projects.blender.org/blender/blender/"
    "issues?q=&type=all&sort=&state=open&labels="
)
total_url = (
    "https://projects.blender.org/blender/blender/"
    "issues?q=&type=all&sort=&state=open&labels=285%2c-297%2c-298%2c-299%2c-301"
)

severity_labelid = {
    "Low": "286",
    "Normal": "287",
    "High": "285",
    "Unbreak Now!": "288"
}


def compile_list(severity: str) -> None:

    label = f"Priority/{severity}"
    issues_json = gitea_json_issues_search(
        type="issues",
        state="open",
        labels=label,
        verbose=True,
    )

    uncategorized_reports = []

    issues_json_sorted = sorted(issues_json, key=lambda x: x["number"])

    for issue in issues_json_sorted:
        html_url = issue["html_url"]
        number = issue["number"]
        created_at = issue["created_at"].rsplit('T', 1)[0]
        title = issue["title"]

        # Check reports module assignment and fill in data.
        for label_iter in issue["labels"]:
            label = label_iter["name"]
            if label not in modules:
                continue

            modules[label].buglist.append(f"[#{number}]({html_url})")
            modules[label].buglist_full.append(f"* [{title}]({html_url}) - {created_at}\n")
            break
    else:
        uncategorized_reports.append(f"[#{number}]({html_url})")

    # Print statistics
    print(f"Open {severity} Priority bugs as of {date.today()}:\n")

    # Module overview with numbers
    total = 0
    for module in modules.values():
        buglist_str = (", ".join(module.buglist))
        buglist_len = len(module.buglist)
        total += buglist_len
        full_url = base_url + severity_labelid[severity] + "%2c" + module.labelid
        if not module.buglist or severity != "High":
            print(f"- [{module.name}]({full_url}): *{buglist_len}*")
        else:
            print(f"- [{module.name}]({full_url}): *{buglist_len}* _{buglist_str}_")

    print()
    print(f"[Total]({total_url}): {total}")
    print()
    print("Uncategorized:", ", ".join(uncategorized_reports))
    print()

    # Module overview with titles and creation date
    for module in modules.values():
        buglist_full_str = ("".join(module.buglist_full))
        buglist_full_len = len(module.buglist_full)
        if buglist_full_len != 0:
            print(f"{module.name}:")
            print(f"{buglist_full_str}")


def main() -> None:

    parser = argparse.ArgumentParser(
        description="Print statistics on open bug reports per module",
        epilog="This script is used to help module teams",
    )

    parser.add_argument(
        "--severity",
        dest="severity",
        default="High",
        type=str,
        required=False,
        choices=severity_labelid.keys(),
        help="Severity level of reports",
    )

    args = parser.parse_args()

    compile_list(args.severity)


if __name__ == "__main__":
    main()
