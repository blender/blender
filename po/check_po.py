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

import os
import sys
from codecs import open

CURRENT_DIR = os.path.abspath(os.path.dirname(__file__))

FILE_NAME_POT = os.path.join(CURRENT_DIR, "blender.pot")


def read_messages(fname):
    def stripeol(s):
        return s.rstrip("\n\r")

    messages = {}
    reading_message = False
    message = ""
    with open(fname, 'r', "utf-8") as handle:
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
                messages[message] = True
            elif reading_message:
                message += line[1:-1]
    return messages


def main():
    pot_messages = read_messages(FILE_NAME_POT)

    if len(sys.argv) > 1:
        for lang in sys.argv[1:]:
            po = os.path.join(CURRENT_DIR, lang + '.po')

            if os.path.exists(po):
                po_messages = read_messages(po)
                for msgid in po_messages:
                    if not pot_messages.get(msgid):
                        print('Unneeded message id \'%s\'' % (msgid))

                for msgid in pot_messages:
                    if not po_messages.get(msgid):
                        print('Missed message id \'%s\'' % (msgid))
    else:
        for po in os.listdir(CURRENT_DIR):
            if po.endswith('.po'):
                print('Processing %s...' % (po))
                po_messages = read_messages(po)
                for msgid in po_messages:
                    if not pot_messages.get(msgid):
                        print('    Unneeded message id \'%s\'' % (msgid))

                for msgid in pot_messages:
                    if not po_messages.get(msgid):
                        print('    Missed message id \'%s\'' % (msgid))


if __name__ == "__main__":
    print("\n\n *** Running %r *** \n" % __file__)
    main()
