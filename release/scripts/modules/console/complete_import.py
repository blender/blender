# Copyright (c) 2009 Fernando Perez, www.stani.be (GPL license)

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

# Original copyright (see docstring):
#*****************************************************************************
#       Copyright (C) 2001-2006 Fernando Perez <fperez@colorado.edu>
#
#  Distributed under the terms of the BSD License.  The full license is in
#  the file COPYING, distributed as part of this software.
#*****************************************************************************

# <pep8 compliant>

"""Completer for import statements

Original code was from IPython/Extensions/ipy_completers.py. The following
changes have been made:
- ported to python3
- pep8 polishing
- limit list of modules to prefix in case of "from w"
- sorted modules
- added sphinx documentation
- complete() returns a blank list of the module isnt found
"""


import os
import sys

TIMEOUT_STORAGE = 3  # Time in secs after which the root-modules will be stored
TIMEOUT_GIVEUP = 20  # Time in secs after which we give up

ROOT_MODULES = None


def get_root_modules():
    """
    Returns a list containing the names of all the modules available in the
    folders of the python-path.

    :returns: modules
    :rtype: list
    """
    global ROOT_MODULES
    modules = []
    if not(ROOT_MODULES is None):
        return ROOT_MODULES
    from time import time
    t = time()
    store = False
    for path in sys.path:
        modules += module_list(path)
        if time() - t >= TIMEOUT_STORAGE and not store:
            # Caching the list of root modules, please wait!
            store = True
        if time() - t > TIMEOUT_GIVEUP:
            # This is taking too long, we give up.
            ROOT_MODULES = []
            return []

    modules += sys.builtin_module_names

    # needed for modules defined in C
    modules += sys.modules.keys()

    modules = list(set(modules))
    if '__init__' in modules:
        modules.remove('__init__')
    modules = sorted(modules)
    if store:
        ROOT_MODULES = modules
    return modules


def module_list(path):
    """
    Return the list containing the names of the modules available in
    the given folder.

    :param path: folder path
    :type path: str
    :returns: modules
    :rtype: list
    """

    if os.path.isdir(path):
        folder_list = os.listdir(path)
    elif path.endswith('.egg'):
        from zipimport import zipimporter
        try:
            folder_list = [f for f in zipimporter(path)._files]
        except:
            folder_list = []
    else:
        folder_list = []
    #folder_list = glob.glob(os.path.join(path,'*'))
    folder_list = [p for p in folder_list  \
       if os.path.exists(os.path.join(path, p, '__init__.py'))\
           or p[-3:] in ('.py', '.so')\
           or p[-4:] in ('.pyc', '.pyo', '.pyd')]

    folder_list = [os.path.basename(p).split('.')[0] for p in folder_list]
    return folder_list


def complete(line):
    """
    Returns a list containing the completion possibilities for an import line.

    :param line:

        incomplete line which contains an import statement::

            import xml.d
            from xml.dom import

    :type line: str
    :returns: list of completion possibilities
    :rtype: list

    >>> complete('import weak')
    ['weakref']
    >>> complete('from weakref import C')
    ['CallableProxyType']
    """
    import inspect

    def try_import(mod, only_modules=False):

        def is_importable(module, attr):
            if only_modules:
                return inspect.ismodule(getattr(module, attr))
            else:
                return not(attr[:2] == '__' and attr[-2:] == '__')

        try:
            m = __import__(mod)
        except:
            return []
        mods = mod.split('.')
        for module in mods[1:]:
            m = getattr(m, module)
        if (not hasattr(m, '__file__')) or (not only_modules) or\
           (hasattr(m, '__file__') and '__init__' in m.__file__):
            completion_list = [attr for attr in dir(m)
                if is_importable(m, attr)]
        else:
            completion_list = []
        completion_list.extend(getattr(m, '__all__', []))
        if hasattr(m, '__file__') and '__init__' in m.__file__:
            completion_list.extend(module_list(os.path.dirname(m.__file__)))
        completion_list = list(set(completion_list))
        if '__init__' in completion_list:
            completion_list.remove('__init__')
        return completion_list

    def filter_prefix(names, prefix):
        return [name for name in names if name.startswith(prefix)]

    words = line.split(' ')
    if len(words) == 3 and words[0] == 'from':
        return ['import ']
    if len(words) < 3 and (words[0] in ['import', 'from']):
        if len(words) == 1:
            return get_root_modules()
        mod = words[1].split('.')
        if len(mod) < 2:
            return filter_prefix(get_root_modules(), words[-1])
        completion_list = try_import('.'.join(mod[:-1]), True)
        completion_list = ['.'.join(mod[:-1] + [el]) for el in completion_list]
        return filter_prefix(completion_list, words[-1])
    if len(words) >= 3 and words[0] == 'from':
        mod = words[1]
        return filter_prefix(try_import(mod), words[-1])

    # get here if the import is not found
    # import invalidmodule
    #                      ^, in this case return nothing
    return []
