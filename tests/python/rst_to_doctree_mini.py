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

# <pep8 compliant>

# Module with function to extract a doctree from an reStructuredText file.
# Named 'Mini' because we only parse the minimum data needed to check
# Python classes, methods and attributes match up to those in existing modules.
# (To test for documentation completeness)

# note: literalinclude's are not followed.
# could be nice to add but not really needed either right now.

import collections

Directive = collections.namedtuple('Directive',
                                   ("type",
                                    "value",
                                    "value_strip",
                                    "line",
                                    "indent",
                                    "members"))


def parse_rst_py(filepath):
    import re

    # Get the prefix assuming the line is lstrip()'d
    # ..foo:: bar
    # -->
    # ("foo", "bar")
    re_prefix = re.compile(r"^\.\.\s([a-zA-Z09\-]+)::\s*(.*)\s*$")

    tree = collections.defaultdict(list)
    indent_map = {}
    indent_prev = 0
    f = open(filepath, encoding="utf-8")
    for i, line in enumerate(f):
        line_strip = line.lstrip()
        # ^\.\.\s[a-zA-Z09\-]+::.*$
        #if line.startswith(".. "):
        march = re_prefix.match(line_strip)

        if march:
            directive, value = march.group(1, 2)
            indent = len(line) - len(line_strip)
            value_strip = value.replace("(", " ").split()
            value_strip = value_strip[0] if value_strip else ""

            item = Directive(type=directive,
                             value=value,
                             value_strip=value_strip,
                             line=i,
                             indent=indent,
                             members=[])

            tree[indent].append(item)
            if indent_prev < indent:
                indent_map[indent] = indent_prev
            if indent > 0:
                tree[indent_map[indent]][-1].members.append(item)
            indent_prev = indent
    f.close()

    return tree[0]


if __name__ == "__main__":
    # not intended use, but may as well print rst files passed as a test.
    import sys
    for arg in sys.argv:
        if arg.lower().endswith((".txt", ".rst")):
            items = parse_rst_py(arg)
            for i in items:
                print(i)
