# SPDX-FileCopyrightText: 2019-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# Keep the information collected in this script synchronized with `startup.py`.

__all__ = (
    "url_from_blender",
)


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

    gpu_backend = gpu.platform.backend_type_get()
    if gpu_backend not in {'NONE', 'UNKNOWN', 'METAL'}:
        query_params["gpu"] += (" {:s} Backend".format(gpu_backend.title()))

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

    query_str = urllib.parse.urlencode(query_params)
    return "https://redirect.blender.org/?" + query_str
