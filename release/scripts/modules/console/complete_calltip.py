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

import inspect
import re


# regular expression constants
DEF_DOC = '%s\s*(\(.*?\))'
DEF_SOURCE = 'def\s+%s\s*(\(.*?\)):'
RE_EMPTY_LINE = re.compile('^\s*\n')
RE_FLAG = re.MULTILINE | re.DOTALL
RE_NEWLINE = re.compile('\n+')
RE_SPACE = re.compile('\s+')
RE_DEF_COMPLETE = re.compile(
    # don't start with a quote
    '''(?:^|[^"'a-zA-Z0-9_])'''
    # start with a \w = [a-zA-Z0-9_]
    '''((\w+'''
    # allow also dots and closed bracket pairs []
    '''(?:\w|[.]|\[.+?\])*'''
    # allow empty string
    '''|)'''
    # allow opening bracket(s)
    '''(?:\(|\s)*)$''')


def reduce_newlines(text):
    """Reduces multiple newlines to a single newline.

    :param text: text with multiple newlines
    :type text: str
    :returns: text with single newlines
    :rtype: str

    >>> reduce_newlines('hello\\n\\nworld')
    'hello\\nworld'
    """
    return RE_NEWLINE.sub('\n', text)


def reduce_spaces(text):
    """Reduces multiple whitespaces to a single space.

    :param text: text with multiple spaces
    :type text: str
    :returns: text with single spaces
    :rtype: str

    >>> reduce_spaces('hello    \\nworld')
    'hello world'
    """
    return RE_SPACE.sub(' ', text)


def get_doc(obj):
    """Get the doc string or comments for an object.

    :param object: object
    :returns: doc string
    :rtype: str

    >>> get_doc(abs)
    'abs(number) -> number\\n\\nReturn the absolute value of the argument.'
    """
    result = inspect.getdoc(obj) or inspect.getcomments(obj)
    return result and RE_EMPTY_LINE.sub('', result.rstrip()) or ''


def get_argspec(func, strip_self=True, doc=None, source=None):
    """Get argument specifications.

    :param strip_self: strip `self` from argspec
    :type strip_self: bool
    :param doc: doc string of func (optional)
    :type doc: str
    :param source: source code of func (optional)
    :type source: str
    :returns: argument specification
    :rtype: str

    >>> get_argspec(inspect.getclasstree)
    '(classes, unique=0)'
    >>> get_argspec(abs)
    '(number)'
    """
    # get the function object of the class
    try:
        func = func.__func__
    except AttributeError:
        try:
            # py 2.X
            func = func.im_func
        except AttributeError:
            pass
    # is callable?
    if not hasattr(func, '__call__'):
        return ''
    # func should have a name
    try:
        func_name = func.__name__
    except AttributeError:
        return ''
    # from docstring
    if doc is None:
        doc = get_doc(func)
    match = re.search(DEF_DOC % func_name, doc, RE_FLAG)
    # from source code
    if not match:
        if source is None:
            try:
                source = inspect.getsource(func)
            except (TypeError, IOError):
                source = ''
        if source:
            match = re.search(DEF_SOURCE % func_name, source, RE_FLAG)
    if match:
        argspec = reduce_spaces(match.group(1))
    else:
        # try with the inspect.getarg* functions
        try:
            argspec = inspect.formatargspec(*inspect.getfullargspec(func))
        except:
            try:
                # py 2.X
                argspec = inspect.formatargspec(*inspect.getargspec(func))
            except:
                try:
                    argspec = inspect.formatargvalues(
                        *inspect.getargvalues(func))
                except:
                    argspec = ''
        if strip_self:
            argspec = argspec.replace('self, ', '')
    return argspec


def complete(line, cursor, namespace):
    """Complete callable with calltip.

    :param line: incomplete text line
    :type line: str
    :param cursor: current character position
    :type cursor: int
    :param namespace: namespace
    :type namespace: dict
    :returns: (matches, world, scrollback)
    :rtype: (list of str, str, str)

    >>> import os
    >>> complete('os.path.isdir(', 14, {'os': os})[-1]
    'isdir(s)\\nReturn true if the pathname refers to an existing directory.'
    >>> complete('abs(', 4, {})[-1]
    'abs(number) -> number\\nReturn the absolute value of the argument.'
    """
    matches = []
    word = ''
    scrollback = ''
    match = RE_DEF_COMPLETE.search(line[:cursor])

    if match:
        word = match.group(1)
        func_word = match.group(2)
        try:
            func = eval(func_word, namespace)
        except Exception:
            func = None

        if func:
            doc = get_doc(func)
            argspec = get_argspec(func, doc=doc)
            scrollback = func_word.split('.')[-1] + (argspec or '()')
            if doc.startswith(scrollback):
                scrollback = doc
            elif doc:
                scrollback += '\n' + doc
            scrollback = reduce_newlines(scrollback)

    return matches, word, scrollback
