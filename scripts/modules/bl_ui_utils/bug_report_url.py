# SPDX-License-Identifier: GPL-2.0-or-later


def url_prefill_from_blender(*, addon_info=None):
    import bpy
    import gpu
    import struct
    import platform
    import urllib.parse
    import io

    fh = io.StringIO()

    fh.write("**System Information**\n")
    fh.write(
        "Operating system: %s %d Bits\n" % (
            platform.platform(),
            struct.calcsize("P") * 8,
        )
    )
    fh.write(
        "Graphics card: %s %s %s\n" % (
            gpu.platform.renderer_get(),
            gpu.platform.vendor_get(),
            gpu.platform.version_get(),
        )
    )
    fh.write(
        "\n"
        "**Blender Version**\n"
    )
    fh.write(
        "Broken: version: %s, branch: %s, commit date: %s %s, hash: `%s`\n" % (
            bpy.app.version_string,
            bpy.app.build_branch.decode('utf-8', 'replace'),
            bpy.app.build_commit_date.decode('utf-8', 'replace'),
            bpy.app.build_commit_time.decode('utf-8', 'replace'),
            bpy.app.build_hash.decode('ascii'),
        )
    )
    fh.write(
        "Worked: (newest version of Blender that worked as expected)\n"
    )
    if addon_info:
        fh.write(
            "\n"
            "**Addon Information**\n"
        )
        fh.write(addon_info)

    fh.write(
        "\n"
        "**Short description of error**\n"
        "[Please fill out a short description of the error here]\n"
        "\n"
        "**Exact steps for others to reproduce the error**\n"
        "[Please describe the exact steps needed to reproduce the issue]\n"
        "[Based on the default startup or an attached .blend file (as simple as possible)]\n"
        "\n"
    )

    form_number = 2 if addon_info else 1
    return (
        "https://developer.blender.org/maniphest/task/edit/form/%i?description=" % form_number +
        urllib.parse.quote(fh.getvalue())
    )
