#!/usr/bin/python3

# ***** BEGIN GPL LICENSE BLOCK *****
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ***** END GPL LICENSE BLOCK *****

# <pep8-80 compliant>

# Write out messages.txt from Blender.

import os
import sys
import subprocess

import settings


BLENDER_ARGS = [
    settings.BLENDER_EXEC,
    "--background",
    "--factory-startup",
    "--python",
    os.path.join(settings.TOOLS_DIR, "bl_process_msg.py"),
    "--",
    "-m",
]


def main():
    import argparse
    parser = argparse.ArgumentParser(description="Write out messages.txt " \
                                                 "from Blender.")
    parser.add_argument('-c', '--no_checks', default=True,
                        action="store_false",
                        help="No checks over UI messages.")
    parser.add_argument('-b', '--blender', help="Blender executable path.")
    parser.add_argument('-o', '--output', help="Output messages file path.")
    args = parser.parse_args()
    if args.blender:
        BLENDER_ARGS[0] = args.blender
    if not args.no_checks:
        BLENDER_ARGS.append("-c")
    if args.output:
        BLENDER_ARGS.append("-o")
        BLENDER_ARGS.append(args.output)
    ret = subprocess.call(BLENDER_ARGS)

    return ret


if __name__ == "__main__":
    print("\n\n *** Running {} *** \n".format(__file__))
    ret = main()
    if ret:
        raise(Exception(ret))
