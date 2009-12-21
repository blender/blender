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

# <pep8-80 compliant>

"""This module provides intellisense features such as:

* autocompletion
* calltips

It unifies all completion plugins and only loads them on demand.
"""

# TODO: file complete if startswith quotes
import os
import re

# regular expressions to find out which completer we need

# line which starts with an import statement
RE_MODULE = re.compile('^import|from.+')

# The following regular expression means an 'unquoted' word
RE_UNQUOTED_WORD = re.compile(
    # don't start with a quote
    '''(?:^|[^"'a-zA-Z0-9_])'''
    # start with a \w = [a-zA-Z0-9_]
    '''((?:\w+'''
    # allow also dots and closed bracket pairs []
    '''(?:\w|[.]|\[.+?\])*'''
    # allow empty string
    '''|)'''
    # allow an unfinished index at the end (including quotes)
    '''(?:\[[^\]]*$)?)$''',
    # allow unicode as theoretically this is possible
    re.UNICODE)


def complete(line, cursor, namespace, private=True):
    """Returns a list of possible completions:

    * name completion
    * attribute completion (obj.attr)
    * index completion for lists and dictionaries
    * module completion (from/import)

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

    >>> complete('re.sr', 5, {'re': re})
    (['re.sre_compile', 're.sre_parse'], 're.sr')
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

    >>> expand('os.path.isdir(', 14, {'os': os})[-1]
    'isdir(s)\\nReturn true if the pathname refers to an existing directory.'
    >>> expand('abs(', 4, {})[-1]
    'abs(number) -> number\\nReturn the absolute value of the argument.'
    """
    if line[:cursor].strip().endswith('('):
        import complete_calltip
        matches, word, scrollback = complete_calltip.complete(line,
            cursor, namespace)
        no_calltip = False
    else:
        matches, word = complete(line, cursor, namespace, private)
        if len(matches) == 1:
            scrollback = ''
        else:
            scrollback = '  '.join([m.split('.')[-1] for m in matches])
        no_calltip = True
    prefix = os.path.commonprefix(matches)[len(word):]
    if prefix:
        line = line[:cursor] + prefix + line[cursor:]
        cursor += len(prefix)
        if no_calltip and prefix.endswith('('):
            return expand(line, cursor, namespace, private)
    return line, cursor, scrollback
