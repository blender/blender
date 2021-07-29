# -*- coding: utf-8 -*-
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

import sys
import email

class SvDocstring(object):
    """
    A class that incapsulates parsing of Sverchok's nodes docstrings.
    As a standard, RFC822-style syntax is to be used. The docstring should
    start with headers:

            Triggers: This should be very short (two or three words, not much more) to be used in Ctrl-Space search menu.
            Tooltip: Longer description to be present as a tooltip in UI.

            More detailed description with technical information or historical notes goes after empty line.
            This is not shown anywhere in the UI.

    Other headers can possibly be introduced later. Unknown headers are just ignored.
    For compatibility reasons, the old docstring syntax is also supported:

            Triggers description /// Longer description

    If we can't parse Triggers and Tooltip from docstring, then:
    * The whole docstring will be used as tooltip
    * The node will not have shorthand for search.
    """

    def __init__(self, docstring):
        self.docstring = docstring
        if docstring:
            self.message = email.message_from_string(SvDocstring.trim(docstring))
        else:
            self.message = {}

    @staticmethod
    def trim(docstring):
        """
        Trim docstring indentation and extra spaces.
        This is just copy-pasted from PEP-0257.
        """

        if not docstring:
            return ''
        # Convert tabs to spaces (following the normal Python rules)
        # and split into a list of lines:
        lines = docstring.expandtabs().splitlines()
        # Determine minimum indentation (first line doesn't count):
        indent = sys.maxsize
        for line in lines[1:]:
            stripped = line.lstrip()
            if stripped:
                indent = min(indent, len(line) - len(stripped))
        # Remove indentation (first line is special):
        trimmed = [lines[0].strip()]
        if indent < sys.maxsize:
            for line in lines[1:]:
                trimmed.append(line[indent:].rstrip())
        # Strip off trailing and leading blank lines:
        while trimmed and not trimmed[-1]:
            trimmed.pop()
        while trimmed and not trimmed[0]:
            trimmed.pop(0)
        # Return a single string:
        return '\n'.join(trimmed)

    def get(self, header, default=None):
        """Obtain any header from docstring."""
        return self.message.get(header, default)

    def __getitem__(self, header):
        return self.message[header]

    def get_shorthand(self, fallback=True):
        """
        Get shorthand to be used in search menu.
        If fallback == True, then whole docstring
        will be returned for case when we can't
        find valid shorthand specification.
        """

        if 'Triggers' in self.message:
            return self.message['Triggers']
        elif not self.docstring:
            return ""
        elif '///' in self.docstring:
            return self.docstring.strip().split('///')[0]
        elif fallback:
            return self.docstring
        else:
            return None

    def has_shorthand(self):
        return self.get_shorthand() is not None

    def get_tooltip(self):
        """Get tooltip"""

        if 'Tooltip' in self.message:
            return self.message['Tooltip'].strip()
        elif not self.docstring:
            return ""
        elif '///' in self.docstring:
            return self.docstring.strip().split('///')[1].strip()
        else:
            return self.docstring.strip()

