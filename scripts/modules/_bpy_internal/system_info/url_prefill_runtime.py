# SPDX-FileCopyrightText: 2019-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# Keep the information collected in this script synchronized with `startup.py`.

__all__ = (
    "url_from_blender",
)


def custom_gitea_report(repo, template_path, query_params):
    ''' Use this to open reports of experimental branches out of the main tracker '''
    import urllib.parse

    # A reconstruction of the expected data to go on the report
    body_string = "**System Information**\n"
    body_string += f"Operating System: {query_params['os']}\n"
    body_string += f"Graphics card: {query_params['gpu']}\n\n"
    body_string += "**Blender Version**\n"
    body_string += f"Broken: {query_params['broken_version']}\n"
    body_string += "Worked: (newest version of Blender that worked as expected)\n\n"
    body_string += "**Short description of error**\n"
    body_string += "[Please fill out a short description of the error here]\n\n"
    body_string += "**Exact steps for others to reproduce the error**\n"
    body_string += "[Please describe the exact steps needed to reproduce the issue]\n"
    body_string += "[Based on the default startup or an attached .blend file (as simple as possible)]\n"

    full_url = "https://projects.blender.org/"
    full_url += f"{repo}/issues/new?template="
    full_url += f"{urllib.parse.quote_plus(template_path)}&field:body="
    full_url += f"{urllib.parse.quote_plus(body_string)}"

    return full_url


def url_from_blender(*, addon_info=None):
    import bpy
    import gpu
    import struct
    import platform
    import urllib.parse

    query_params = {"type": "bug_report"}

    query_params["project"] = "blender-addons" if addon_info else "blender"

    query_params["os"] = "{:s} {:d} Bits".format(
        platform.platform(),
        struct.calcsize("P") * 8,
    )

    # Windowing Environment (include when dynamically selectable).
    # This lets us know if WAYLAND/X11 is in use.
    from _bpy import _ghost_backend
    ghost_backend = _ghost_backend()
    if ghost_backend not in {'NONE', 'DEFAULT'}:
        query_params["os"] += (", {:s} UI".format(ghost_backend))
    del _ghost_backend, ghost_backend

    query_params["gpu"] = "{:s} {:s} {:s}".format(
        gpu.platform.renderer_get(),
        gpu.platform.vendor_get(),
        gpu.platform.version_get(),
    )

    query_params["broken_version"] = "{:s}, branch: {:s}, commit date: {:s} {:s}, hash: `{:s}`".format(
        bpy.app.version_string,
        bpy.app.build_branch.decode('utf-8', 'replace'),
        bpy.app.build_commit_date.decode('utf-8', 'replace'),
        bpy.app.build_commit_time.decode('utf-8', 'replace'),
        bpy.app.build_hash.decode('ascii'),
    )

    if addon_info:
        addon_info_lines = addon_info.splitlines()
        query_params["addon_name"] = addon_info_lines[0].removeprefix("Name: ")
        query_params["addon_author"] = addon_info_lines[1].removeprefix("Author: ")

    if addon_info is None:
        # Report npr-prototype bugs on a separate tracker.
        return custom_gitea_report("pragma37/npr-tracker", ".gitea/issue_template/bug.yaml", query_params)

    query_str = urllib.parse.urlencode(query_params)
    return "https://redirect.blender.org/?" + query_str
