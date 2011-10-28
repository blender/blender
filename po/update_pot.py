#!/usr/bin/env python

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

# <pep8 compliant>

# update the pot file according the POTFILES.in

import subprocess
import os
from codecs import open

GETTEXT_XGETTEXT_EXECUTABLE = "xgettext"
CURRENT_DIR = os.path.abspath(os.path.dirname(__file__))
SOURCE_DIR = os.path.normpath(os.path.abspath(os.path.join(CURRENT_DIR, "..")))
DOMAIN = "blender"
COMMENT_PREFIX = "#~ "  # from update_msg.py

FILE_NAME_POT = os.path.join(CURRENT_DIR, "blender.pot")
FILE_NAME_MESSAGES = os.path.join(CURRENT_DIR, "messages.txt")


def main():
    cmd = (GETTEXT_XGETTEXT_EXECUTABLE,
           "--files-from=%s" % os.path.join(SOURCE_DIR, "po", "POTFILES.in"),
           "--keyword=_",
           "--keyword=N_",
           "--directory=%s" % SOURCE_DIR,
           "--output=%s" % os.path.join(SOURCE_DIR, "po", "%s.pot" % DOMAIN),
           "--from-code=utf-8",
           )

    print(" ".join(cmd))
    process = subprocess.Popen(cmd)
    process.wait()

    def stripeol(s):
        return s.rstrip("\n\r")

    pot_messages = {}
    reading_message = False
    message = ""
    with open(FILE_NAME_POT, 'r', "utf-8") as handle:
        while True:
            line = handle.readline()

            if not line:
                break

            line = stripeol(line)
            if line.startswith("msgid"):
                reading_message = True
                message = line[7:-1]
            elif line.startswith("msgstr"):
                reading_message = False
                pot_messages[message] = True
            elif reading_message:
                message += line[1:-1]

    # add messages collected automatically from RNA
    with open(FILE_NAME_POT, "a", "utf-8") as pot_handle:
        with open(FILE_NAME_MESSAGES, 'r', "utf-8") as handle:
            msgsrc_ls = []
            while True:
                line = handle.readline()

                if not line:
                    break

                line = stripeol(line)

                # COMMENT_PREFIX
                if line.startswith(COMMENT_PREFIX):
                    msgsrc_ls.append(line[len(COMMENT_PREFIX):].strip())
                else:
                    line = line.replace("\\", "\\\\")
                    line = line.replace("\"", "\\\"")
                    line = line.replace("\t", "\\t")

                    if not pot_messages.get(line):
                        for msgsrc in msgsrc_ls:
                            pot_handle.write("#: %s\n" % msgsrc)
                        pot_handle.write("msgid \"%s\"\n" % line)
                        pot_handle.write("msgstr \"\"\n\n")
                    msgsrc_ls[:] = []


if __name__ == "__main__":
    print("\n\n *** Running %r *** \n" % __file__)
    main()
