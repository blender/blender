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

"""Autocomplete with the standard library"""

import rlcompleter

TEMP = '__tEmP__'  # only \w characters are allowed!
TEMP_N = len(TEMP)


def complete(word, namespace, private=True):
    """Complete word within a namespace with the standard rlcompleter
    module. Also supports index or key access [].

    :param word: word to be completed
    :type word: str
    :param namespace: namespace
    :type namespace: dict
    :param private: whether private attribute/methods should be returned
    :type private: bool

    >>> complete('fo', {'foo': 'bar'})
    ['foo']
    """
    completer = rlcompleter.Completer(namespace)

    # brackets are normally not allowed -> work around (only in this case)
    if '[' in word:
        obj, attr = word.rsplit('.', 1)
        try:
            # do not run the obj expression in the console
            namespace[TEMP] = eval(obj, namespace)
        except Exception:
            return []
        _word = TEMP + '.' + attr
    else:
        _word = word

    # find matches with stdlibrary (don't try to implement this yourself)
    completer.complete(_word, 0)
    matches = completer.matches

    # brackets are normally not allowed -> clean up
    if '[' in word:
        matches = [obj + match[TEMP_N:] for match in matches]
        del namespace[TEMP]

    # separate public from private
    public_matches = [match for match in matches if not('._' in match)]
    if private:
        private_matches = [match for match in matches if '._' in match]
        return public_matches + private_matches
    else:
        return public_matches
