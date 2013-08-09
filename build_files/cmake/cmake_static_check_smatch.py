#!/usr/bin/env python3

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
# Contributor(s): Campbell Barton
#
# ***** END GPL LICENSE BLOCK *****

# <pep8 compliant>

CHECKER_IGNORE_PREFIX = [
    "extern",
    "intern/moto",
    "blender/intern/opennl",
    ]

CHECKER_BIN = "smatch"
CHECKER_ARGS = [
    "--full-path",
    "--two-passes",
    ]

import project_source_info
import subprocess
import sys
import os

USE_QUIET = (os.environ.get("QUIET", None) is not None)


def main():
    source_info = project_source_info.build_info(use_cxx=False, ignore_prefix_list=CHECKER_IGNORE_PREFIX)
    source_defines = project_source_info.build_defines_as_args()

    check_commands = []
    for c, inc_dirs, defs in source_info:

        cmd = ([CHECKER_BIN] +
               CHECKER_ARGS +
               [c] +
               [("-I%s" % i) for i in inc_dirs] +
               [("-D%s" % d) for d in defs] +
               source_defines
               )

        check_commands.append((c, cmd))

    def my_process(i, c, cmd):
        if not USE_QUIET:
            percent = 100.0 * (i / (len(check_commands) - 1))
            percent_str = "[" + ("%.2f]" % percent).rjust(7) + " %:"

            sys.stdout.flush()
            sys.stdout.write("%s %s\n" % (percent_str, c))

        return subprocess.Popen(cmd)

    process_functions = []
    for i, (c, cmd) in enumerate(check_commands):
        process_functions.append((my_process, (i, c, cmd)))

    project_source_info.queue_processes(process_functions)


if __name__ == "__main__":
    main()
