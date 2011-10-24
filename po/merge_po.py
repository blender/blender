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

import sys
import collections

from codecs import open


def read_messages(fname):
    def stripeol(s):
        return s.rstrip("\n\r")

    last_message = None

    if hasattr(collections, 'OrderedDict'):
        messages = collections.OrderedDict()
        commented_messages = collections.OrderedDict()
    else:
        messages = {}
        commented_messages = {}

    reading_message = False
    reading_translation = False
    commented = False
    message = ""
    translation = ""
    message_lines = []
    translation_lines = []
    comment_lines = []
    with open(fname, 'r', "utf-8") as handle:
        while True:
            line = handle.readline()

            if not line:
                break

            line = stripeol(line)
            if line.startswith("msgid") or line.startswith("#~ msgid"):
                if reading_translation:
                    last_message['translation'] = translation
                    translation_lines = []

                reading_message = True
                reading_translation = False

                if line.startswith('#~'):
                    message = line[10:-1]
                    commented = True
                else:
                    message = line[7:-1]
                    commented = False

                message_lines.append(message)
            elif line.startswith("msgstr") or line.startswith("#~ msgstr"):
                reading_message = False
                reading_translation = True
                last_message = {'comment_lines': comment_lines,
                                'message_lines': message_lines,
                                'translation_lines': translation_lines}

                if commented:
                    translation = line[11:-1]
                    commented_messages[message] = last_message
                else:
                    translation = line[8:-1]
                    messages[message] = last_message

                message_lines = []
                comment_lines = []
                translation_lines.append(translation)
            elif not line.startswith('"') and not line.startswith('#~ "'):
                if reading_translation:
                    last_message['translation'] = translation
                else:
                    comment_lines.append(line)

                reading_message = False
                reading_translation = False
                message_lines = []
                translation_lines = []
            elif reading_message:
                if line.startswith('#~ "'):
                    m = line[4:-1]
                else:
                    m = line[1:-1]

                message += m
                message_lines.append(m)
            elif reading_translation:
                if line.startswith('#~ "'):
                    t = line[4:-1]
                else:
                    t = line[1:-1]

                translation += t
                translation_lines.append(t)

    return (messages, commented_messages)


def main():
    if len(sys.argv) == 3:
        dst_messages, tmp = read_messages(sys.argv[1])
        from_messages, tmp = read_messages(sys.argv[2])

        for msgid in dst_messages:
            msg = dst_messages.get(msgid)
            from_msg = from_messages.get(msgid)

            if from_msg and from_msg['translation']:
                msg['translation'] = from_msg['translation']
                msg['translation_lines'] = from_msg['translation_lines']

        with open(sys.argv[1], 'w', 'utf-8') as handle:
            for msgid in dst_messages:
                item = dst_messages[msgid]

                for x in item['comment_lines']:
                    handle.write(x + "\n")

                first = True
                for x in item['message_lines']:
                    if first:
                        handle.write("msgid \"%s\"\n" % x)
                    else:
                        handle.write("\"%s\"\n" % x)
                    first = False

                first = True
                for x in item['translation_lines']:
                    if first:
                        handle.write("msgstr \"%s\"\n" % x)
                    else:
                        handle.write("\"%s\"\n" % x)
                    first = False

                handle.write("\n")
    else:
        print('Usage: %s <destination-po> <source-po>' % (sys.argv[0]))


if __name__ == "__main__":
    print("\n\n *** Running %r *** \n" % __file__)
    main()
