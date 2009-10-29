# Copyright (c) 2009 www.stani.be (GPL license)

# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

"""This module provides intellisense features such as:

* autocompletion
* calltips (not yet implemented)

It unifies all completion plugins and only loads them on demand.
"""
# TODO: file complete if startswith quotes
import os
import re

# regular expressions to find out which completer we need

# line which starts with an import statement
RE_MODULE = re.compile('^import|from.+')

# The following regular expression means a word which:
# - doesn't start with a quote (quoted words are not py objects)
# - starts with a [a-zA-Z0-9_]
# - afterwards dots are allowed as well
# - square bracket pairs [] are allowed (should be closed)
RE_UNQUOTED_WORD = re.compile(
    '''(?:^|[^"'])((?:\w+(?:\w|[.]|\[.+?\])*|))$''', re.UNICODE)


def complete(line, cursor, namespace, private=True):
    """Returns a list of possible completions.

    :param line: incomplete text line
    :type line: str
    :param cursor: current character position
    :type cursor: int
    :param namespace: namespace
    :type namespace: dict
    :param private: whether private variables should be listed
    :type private: bool
    :returns: list of completions, word
    :rtype: list, str
    """
    re_unquoted_word = RE_UNQUOTED_WORD.search(line[:cursor])
    if re_unquoted_word:
        # unquoted word -> module or attribute completion
        word = re_unquoted_word.group(1)
        if RE_MODULE.match(line):
            import complete_import
            matches = complete_import.complete(line)
        else:
            import complete_namespace
            matches = complete_namespace.complete(word, namespace, private)
    else:
        # for now we don't have completers for strings
        # TODO: add file auto completer for strings
        word = ''
        matches = []
    return matches, word


def expand(line, cursor, namespace, private=True):
    """This method is invoked when the user asks autocompletion,
    e.g. when Ctrl+Space is clicked.

    :param line: incomplete text line
    :type line: str
    :param cursor: current character position
    :type cursor: int
    :param namespace: namespace
    :type namespace: dict
    :param private: whether private variables should be listed
    :type private: bool
    :returns:

        current expanded line, updated cursor position and scrollback

    :rtype: str, int, str
    """
    matches, word = complete(line, cursor, namespace, private)
    prefix = os.path.commonprefix(matches)[len(word):]
    if prefix:
        line = line[:cursor] + prefix + line[cursor:]
        cursor += len(prefix)
    if len(matches) == 1:
        scrollback = ''
    else:
        scrollback = '  '.join([m.split('.')[-1] for m in matches])
    return line, cursor, scrollback
