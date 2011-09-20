#!/usr/bin/python

# update the pot file according the POTFILES.in

import os

GETTEXT_XGETTEXT_EXECUTABLE="xgettext"
SOURCE_DIR=".."
DOMAIN="blender"

cmd = "%s --files-from=%s/po/POTFILES.in --keyword=_ --keyword=N_ --directory=%s --output=%s/po/%s.pot --from-code=utf-8" % (
    GETTEXT_XGETTEXT_EXECUTABLE, SOURCE_DIR, SOURCE_DIR, SOURCE_DIR, DOMAIN)

os.system( cmd )

def stripeol(s):
    if line.endswith("\n"):
        s = s[:-1]

    if line.endswith("\r"):
        s = s[:-1]

    return s

pot_messages = {}
reading_message = False
message = ""
with open("blender.pot", 'r') as handle:
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
with open("blender.pot", "a") as pot_handle:
    with open("messages.txt", 'r') as handle:
        while True:
            line = handle.readline()

            if not line:
                break

            line = stripeol(line)
            line = line.replace("\\", "\\\\")
            line = line.replace("\"", "\\\"")

            if not pot_messages.get(line):
                pot_handle.write("\n#: Automatically collected from RNA\n")
                pot_handle.write("msgid \"%s\"\n" % (line))
                pot_handle.write("msgstr \"\"\n")
