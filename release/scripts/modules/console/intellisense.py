# Copyright (c) 2009 www.stani.be (GPL license)

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
RE_MODULE = re.compile('^import(\s|$)|from.+')

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


def complete(line, cursor, namespace, private):
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
            from . import complete_import
            matches = complete_import.complete(line)
            if not private:
                matches[:] = [m for m in matches if m[:1] != "_"]
            matches.sort()
        else:
            from . import complete_namespace
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
        from . import complete_calltip
        matches, word, scrollback = complete_calltip.complete(line,
            cursor, namespace)
        prefix = os.path.commonprefix(matches)[len(word):]
        no_calltip = False
    else:
        matches, word = complete(line, cursor, namespace, private)
        prefix = os.path.commonprefix(matches)[len(word):]
        if len(matches) == 1:
            scrollback = ''
        else:
            # causes blender bug [#27495] since string keys may contain '.'
            # scrollback = '  '.join([m.split('.')[-1] for m in matches])

            # add white space to align with the cursor
            white_space = "    " + (" " * (cursor + len(prefix)))
            word_prefix = word + prefix
            scrollback = '\n'.join(
                    [white_space + m[len(word_prefix):]
                     if (word_prefix and m.startswith(word_prefix))
                     else
                     white_space + m.split('.')[-1]
                     for m in matches])

        no_calltip = True

    if prefix:
        line = line[:cursor] + prefix + line[cursor:]
        cursor += len(prefix.encode('utf-8'))
        if no_calltip and prefix.endswith('('):
            return expand(line, cursor, namespace, private)
    return line, cursor, scrollback
