#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2024 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# Keep the information collected in this script synchronized with `runtime.py`.

def prefill_bug_report_info() -> int:
    import re
    import struct
    import platform
    import subprocess
    import webbrowser
    import urllib.parse
    from pathlib import Path

    print("Collecting system information...")

    query_params = {"type": "bug_report", "project": "blender"}

    query_params["os"] = "{:s} {:d} Bits".format(
        platform.platform(),
        struct.calcsize("P") * 8,
    )

    # There doesn't appear to be a easy way to collect GPU information in Python
    # if Blender isn't opening and we can't import the GPU module.
    # So just tell users to follow a written guide.
    query_params["gpu"] = (
        "Follow our guide to collect this information:\n"
        "https://developer.blender.org/docs/handbook/bug_reports/making_good_bug_reports/collect_system_information/"
    )

    os_type = platform.system()
    script_directory = Path(__file__).parent.resolve()
    if os_type == "Darwin":  # macOS appears as Darwin.
        blender_dir = script_directory.joinpath("../../../../../../MacOS/Blender")
    elif os_type == "Windows":
        blender_dir = script_directory.joinpath("../../../../../Blender.exe")
    else:  # Linux and other Unix systems.
        blender_dir = script_directory.joinpath("../../../../../blender")

    try:
        blender_output = subprocess.run(
            [blender_dir, "--version"],
            stdout=subprocess.PIPE,
            encoding="utf-8",
            errors="surrogateescape",
        )
    except Exception as ex:
        sys.stderr.write("{:s}\n".format(str(ex)))
        return 1

    text = blender_output.stdout

    # Gather Blender version information.
    version_match = re.search(r"^Blender (.*)", text, flags=re.MULTILINE)
    branch_match = re.search(r"^\s+build branch: (.*)", text, flags=re.MULTILINE)
    commit_date_match = re.search(r"^\s+build commit date: (.*)", text, flags=re.MULTILINE)
    commit_time_match = re.search(r"^\s+build commit time: (.*)", text, flags=re.MULTILINE)
    build_hash_match = re.search(r"^\s+build hash: (.*)", text, flags=re.MULTILINE)

    if not (version_match or branch_match or commit_date_match or commit_time_match or build_hash_match):
        # No valid Blender info could be found.
        print("Blender did not provide any build information. Blender may be corrupt or blocked from running.")
        print("Please try reinstalling Blender and double check your anti-virus isn't blocking it from running.")
        return 1

    missing_string = "<unknown>"

    query_params["broken_version"] = "{:s}, branch: {:s}, commit date: {:s} {:s}, hash `{:s}`".format(
        version_match.group(1) if version_match else missing_string,
        branch_match.group(1) if branch_match else missing_string,
        commit_date_match.group(1) if commit_date_match else missing_string,
        commit_time_match.group(1) if commit_time_match else missing_string,
        build_hash_match.group(1) if build_hash_match else missing_string,
    )

    query_str = urllib.parse.urlencode(query_params)
    webbrowser.open("https://redirect.blender.org/?{:s}".format(query_str))

    return 0


if __name__ == "__main__":
    import sys
    sys.exit(prefill_bug_report_info())
