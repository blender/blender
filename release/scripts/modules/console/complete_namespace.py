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

"""Autocomplete with the standard library"""

import re
import rlcompleter


RE_INCOMPLETE_INDEX = re.compile('(.*?)\[[^\]]+$')

TEMP = '__tEmP__'  # only \w characters are allowed!
TEMP_N = len(TEMP)


def is_dict(obj):
    """Returns whether obj is a dictionary"""
    return hasattr(obj, 'keys') and hasattr(getattr(obj, 'keys'), '__call__')


def is_struct_seq(obj):
    """Returns whether obj is a structured sequence subclass: sys.float_info"""
    return isinstance(obj, tuple) and hasattr(obj, 'n_fields')


def complete_names(word, namespace):
    """Complete variable names or attributes

    :param word: word to be completed
    :type word: str
    :param namespace: namespace
    :type namespace: dict
    :returns: completion matches
    :rtype: list of str

    >>> complete_names('fo', {'foo': 'bar'})
    ['foo', 'for', 'format(']
    """
    # start completer
    completer = rlcompleter.Completer(namespace)
    # find matches with std library (don't try to implement this yourself)
    completer.complete(word, 0)
    return sorted(set(completer.matches))


def complete_indices(word, namespace, obj=None, base=None):
    """Complete a list or dictionary with its indices:

    * integer numbers for list
    * any keys for dictionary

    :param word: word to be completed
    :type word: str
    :param namespace: namespace
    :type namespace: dict
    :param obj: object evaluated from base
    :param base: substring which can be evaluated into an object
    :type base: str
    :returns: completion matches
    :rtype: list of str

    >>> complete_indices('foo', {'foo': range(5)})
    ['foo[0]', 'foo[1]', 'foo[2]', 'foo[3]', 'foo[4]']
    >>> complete_indices('foo', {'foo': {'bar':0, 1:2}})
    ['foo[1]', "foo['bar']"]
    >>> complete_indices("foo['b", {'foo': {'bar':0, 1:2}}, base='foo')
    ["foo['bar']"]
    """
    #FIXME: 'foo["b'
    if base is None:
        base = word
    if obj is None:
        try:
            obj = eval(base, namespace)
        except Exception:
            return []
    if not hasattr(obj, '__getitem__'):
        # obj is not a list or dictionary
        return []

    obj_is_dict = is_dict(obj)

    # rare objects have a __getitem__ but no __len__ (eg. BMEdge)
    if not obj_is_dict:
        try:
            obj_len = len(obj)
        except TypeError:
            return []

    if obj_is_dict:
        # dictionary type
        matches = ['%s[%r]' % (base, key) for key in sorted(obj.keys())]
    else:
        # list type
        matches = ['%s[%d]' % (base, idx) for idx in range(obj_len)]
    if word != base:
        matches = [match for match in matches if match.startswith(word)]
    return matches


def complete(word, namespace, private=True):
    """Complete word within a namespace with the standard rlcompleter
    module. Also supports index or key access [].

    :param word: word to be completed
    :type word: str
    :param namespace: namespace
    :type namespace: dict
    :param private: whether private attribute/methods should be returned
    :type private: bool
    :returns: completion matches
    :rtype: list of str

    >>> complete('foo[1', {'foo': range(14)})
    ['foo[1]', 'foo[10]', 'foo[11]', 'foo[12]', 'foo[13]']
    >>> complete('foo[0]', {'foo': [range(5)]})
    ['foo[0][0]', 'foo[0][1]', 'foo[0][2]', 'foo[0][3]', 'foo[0][4]']
    >>> complete('foo[0].i', {'foo': [range(5)]})
    ['foo[0].index(', 'foo[0].insert(']
    >>> complete('rlcompleter', {'rlcompleter': rlcompleter})
    ['rlcompleter.']
    """
    #
    # if word is empty -> nothing to complete
    if not word:
        return []

    re_incomplete_index = RE_INCOMPLETE_INDEX.search(word)
    if re_incomplete_index:
        # ignore incomplete index at the end, e.g 'a[1' -> 'a'
        matches = complete_indices(word, namespace,
                    base=re_incomplete_index.group(1))

    elif not('[' in word):
        matches = complete_names(word, namespace)

    elif word[-1] == ']':
        matches = [word]

    elif '.' in word:
        # brackets are normally not allowed -> work around

        # remove brackets by using a temp var without brackets
        obj, attr = word.rsplit('.', 1)
        try:
            # do not run the obj expression in the console
            namespace[TEMP] = eval(obj, namespace)
        except Exception:
            return []
        matches = complete_names(TEMP + '.' + attr, namespace)
        matches = [obj + match[TEMP_N:] for match in matches]
        del namespace[TEMP]

    else:
        # safety net, but when would this occur?
        return []

    if not matches:
        return []

    # add '.', '('  or '[' if no match has been found
    elif len(matches) == 1 and matches[0] == word:

        # try to retrieve the object
        try:
            obj = eval(word, namespace)
        except Exception:
            return []
        # ignore basic types
        if type(obj) in (bool, float, int, str):
            return []
        # an extra char '[', '(' or '.' will be added
        if hasattr(obj, '__getitem__') and not is_struct_seq(obj):
            # list or dictionary
            matches = complete_indices(word, namespace, obj)
        elif hasattr(obj, '__call__'):
            # callables
            matches = [word + '(']
        else:
            # any other type
            matches = [word + '.']

    # separate public from private
    public_matches = [match for match in matches if not('._' in match)]
    if private:
        private_matches = [match for match in matches if '._' in match]
        return public_matches + private_matches
    else:
        return public_matches
