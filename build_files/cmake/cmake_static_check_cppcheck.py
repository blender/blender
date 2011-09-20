#!/usr/bin/env python

# $Id$
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

import project_source_info
import subprocess
import sys
import os

CHECKER_IGNORE_PREFIX = [
    "extern",
    "intern/moto",
    ]

CHECKER_BIN = "cppcheck"

CHECKER_ARGS = [
    # not sure why this is needed, but it is.
    "-I" + os.path.join(project_source_info.SOURCE_DIR, "extern/glew/include"),

    #  "--check-config", # when includes are missing
    #  "--enable=all",  # if you want sixty hundred pedantic suggestions
    ]


def main():
    source_info = project_source_info.build_info(ignore_prefix_list=CHECKER_IGNORE_PREFIX)

    check_commands = []
    for c, inc_dirs, defs in source_info:
        cmd = ([CHECKER_BIN] +
                CHECKER_ARGS +
               [c] +
               [("-I%s" % i) for i in inc_dirs] +
               [("-D%s" % d) for d in defs]
              )

        check_commands.append((c, cmd))

    for i, (c, cmd) in enumerate(check_commands):
        percent = 100.0 * (i / (len(check_commands) - 1))
        percent_str = "[" + ("%.2f]" % percent).rjust(7) + " %:"

        # if percent < 27.9:
        #    continue

        # let cppcheck finish the line off...
        sys.stdout.write("%s " % percent_str)

        sys.stdout.flush()
        process = subprocess.Popen(cmd)
        process.wait()


if __name__ == "__main__":
    main()
