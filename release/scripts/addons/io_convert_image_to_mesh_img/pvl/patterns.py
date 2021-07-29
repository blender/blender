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

"""PVL Regular Expression Patterns"""

import re

# End of PVL File
END = re.compile(
    r'\s* \bEND\b \s*', (re.VERBOSE + re.IGNORECASE)
)

# PVL Comment
COMMENT = re.compile(
    r'/\* .*? \*/', (re.DOTALL + re.VERBOSE)
)

# PVL Statement
STATEMENT = re.compile(
    r"""
    \s* (?P<key>\w+) # Match a PVL key
    \s* = \s* # Who knows how many spaces we encounter
    (?P<val> # Match a PVL value
        ([+-]?\d+\.?\d*) # We could match a number
        | (['"]?((\w+ \s*?)+)['"]?) # Or a string
    )
    (\s* <(?P<units>.*?) >)? # The value may have an associated unit
    """, re.VERBOSE
)

# Integer Number
INTEGER = re.compile(
    r"""
    [+-]?(?<!\.)\b[0-9]+\b(?!\.[0-9])
    """, re.VERBOSE
)

# Floating Point Number
FLOATING = re.compile(
    r"""
    [+-]?\b[0-9]*\.?[0-9]+
    """, re.VERBOSE
)
