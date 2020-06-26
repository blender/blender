# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8-80 compliant>


def url_prefill_from_blender(addon_info=None):
    import bpy
    import bgl
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
            bgl.glGetString(bgl.GL_RENDERER),
            bgl.glGetString(bgl.GL_VENDOR),
            bgl.glGetString(bgl.GL_VERSION),
        )
    )
    fh.write(
        "\n"
        "**Blender Version**\n"
    )
    fh.write(
        "Broken: version: %s, branch: %s, commit date: %s %s, hash: `rB%s`\n" % (
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

    fh.seek(0)

    form_number = 2 if addon_info else 1
    return (
        "https://developer.blender.org/maniphest/task/edit/form/%i?description=" % form_number +
        urllib.parse.quote(fh.read())
    )
