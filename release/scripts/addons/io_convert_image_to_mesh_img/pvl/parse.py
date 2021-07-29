# This file is a part of the HiRISE DTM Importer for Blender
#
# Copyright (C) 2017 Arizona Board of Regents on behalf of the Planetary Image
# Research Laboratory, Lunar and Planetary Laboratory at the University of
# Arizona.
#
# This program is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the Free
# Software Foundation, either version 3 of the License, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program.  If not, see <http://www.gnu.org/licenses/>.

"""PVL Label Parsing"""

import collections
import re

from . import patterns
from .label import Label

Quantity = collections.namedtuple('Quantity', ['value', 'units'])


class PVLParseError(Exception):
    """Error parsing PVL file"""
    def __init__(self, message):
        super(PVLParseError, self).__init__(message)


class LabelParser:
    """A PVL Parser"""
    @staticmethod
    def load(path):
        """
        Load a dict-like representation of a PVL label header

        Parameters
        ----------
        path : str
            Path to a file containing a PVL header

        Returns
        ----------
        label : pvl.Label

        """
        raw = LabelParser._read(path)
        return Label(**LabelParser._parse(raw))

    @staticmethod
    def _read(path):
        """
        Get the PVL header from a file as a string

        Parameters
        ----------
        path : str
            Path to a file containing a PVL header

        Returns
        ----------
        raw : str

        Notes
        ---------
        * This function assumes that the file begins with a PVL header
          and it will read lines from the file until it encounters
          a PVL end statement.

        To-Do
        ---------
        * This could be more robust. What happens if there is no label
          in the file?

        """
        with open(path, 'rb') as f:
            raw = ''
            while True:
                try:
                    line = f.readline().decode()
                    raw += line
                    if re.match(patterns.END, line):
                        break
                except UnicodeDecodeError:
                    raise PVLParseError("Error parsing PVL label from "
                                        "file: {}".format(path))
        return raw

    @staticmethod
    def _remove_comments(raw):
        return re.sub(patterns.COMMENT, '', raw)

    @staticmethod
    def _parse(raw):
        raw = LabelParser._remove_comments(raw)
        label_iter = re.finditer(patterns.STATEMENT, raw)
        return LabelParser._parse_iter(label_iter)

    @staticmethod
    def _parse_iter(label_iter):
        """Recursively parse a PVL label iterator"""
        obj = {}
        while True:
            try:
                # Try to fetch the next match from the iter
                match = next(label_iter)
                val = match.group('val')
                key = match.group('key')
                # Handle nested object groups
                if key == 'OBJECT':
                    obj.update({
                        val: LabelParser._parse_iter(label_iter)
                    })
                elif key == 'END_OBJECT':
                    return obj
                # Add key/value pair to dict
                else:
                    # Should this value be a numeric type?
                    try:
                        val = LabelParser._convert_to_numeric(val)
                    except ValueError:
                        pass
                    # Should this value have units?
                    if match.group('units'):
                        val = Quantity(val, match.group('units'))
                    # Add it to the dict
                    obj.update({key: val})
            except StopIteration:
                break
        return obj

    @staticmethod
    def _convert_to_numeric(s):
        """Convert a string to its appropriate numeric type"""
        if re.match(patterns.INTEGER, s):
            return int(s)
        elif re.match(patterns.FLOATING, s):
            return float(s)
        else:
            raise ValueError
