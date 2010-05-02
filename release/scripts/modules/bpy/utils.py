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

# <pep8 compliant>

"""
This module contains utility functions spesific to blender but
not assosiated with blenders internal data.
"""

import bpy as _bpy
import os as _os
import sys as _sys

from _bpy import home_paths


def _test_import(module_name, loaded_modules):
    import traceback
    import time
    if module_name in loaded_modules:
        return None
    if "." in module_name:
        print("Ignoring '%s', can't import files containing multiple periods." % module_name)
        return None

    t = time.time()
    try:
        mod = __import__(module_name)
    except:
        traceback.print_exc()
        return None

    if _bpy.app.debug:
        print("time %s %.4f" % (module_name, time.time() - t))

    loaded_modules.add(mod.__name__) # should match mod.__name__ too
    return mod


def modules_from_path(path, loaded_modules):
    """
    Load all modules in a path and return them as a list.

    :arg path: this path is scanned for scripts and packages.
    :type path: string
    :arg loaded_modules: alredy loaded module names, files matching these names will be ignored.
    :type loaded_modules: set
    :return: all loaded modules.
    :rtype: list
    """
    import traceback
    import time

    modules = []

    for f in sorted(_os.listdir(path)):
        if f.endswith(".py"):
            # python module
            mod = _test_import(f[0:-3], loaded_modules)
        elif ("." not in f) and (_os.path.isfile(_os.path.join(path, f, "__init__.py"))):
            # python package
            mod = _test_import(f, loaded_modules)
        else:
            mod = None

        if mod:
            modules.append(mod)

    return modules

_loaded = [] # store loaded modules for reloading.
_bpy_types = __import__("bpy_types") # keep for comparisons, never ever reload this.


def load_scripts(reload_scripts=False, refresh_scripts=False):
    """
    Load scripts and run each modules register function.

    :arg reload_scripts: Causes all scripts to have their unregister method called before loading.
    :type reload_scripts: bool
    :arg refresh_scripts: only load scripts which are not already loaded as modules.
    :type refresh_scripts: bool
    """
    import traceback
    import time

    t_main = time.time()

    loaded_modules = set()

    if refresh_scripts:
        original_modules = _sys.modules.values()

    def sys_path_ensure(path):
        if path not in _sys.path: # reloading would add twice
            _sys.path.insert(0, path)

    def test_reload(mod):
        # reloading this causes internal errors
        # because the classes from this module are stored internally
        # possibly to refresh internal references too but for now, best not to.
        if mod == _bpy_types:
            return mod

        try:
            return reload(mod)
        except:
            traceback.print_exc()

    def test_register(mod):

        if refresh_scripts and mod in original_modules:
            return

        if reload_scripts and mod:
            print("Reloading:", mod)
            mod = test_reload(mod)

        if mod:
            register = getattr(mod, "register", None)
            if register:
                try:
                    register()
                except:
                    traceback.print_exc()
            else:
                print("\nWarning! '%s' has no register function, this is now a requirement for registerable scripts." % mod.__file__)
            _loaded.append(mod)

    if reload_scripts:
        # reload modules that may not be directly included
        for type_class_name in dir(_bpy.types):
            type_class = getattr(_bpy.types, type_class_name)
            module_name = getattr(type_class, "__module__", "")

            if module_name and module_name != "bpy.types": # hard coded for C types
                loaded_modules.add(module_name)

        # sorting isnt needed but rather it be pradictable
        for module_name in sorted(loaded_modules):
            print("Reloading:", module_name)
            test_reload(_sys.modules[module_name])

        # loop over and unload all scripts
        _loaded.reverse()
        for mod in _loaded:
            unregister = getattr(mod, "unregister", None)
            if unregister:
                try:
                    unregister()
                except:
                    traceback.print_exc()
        _loaded[:] = []

    user_path = user_script_path()

    for base_path in script_paths():
        for path_subdir in ("", "ui", "op", "io", "cfg", "keyingsets", "modules"):
            path = _os.path.join(base_path, path_subdir)
            if _os.path.isdir(path):
                sys_path_ensure(path)

                # only add this to sys.modules, dont run
                if path_subdir == "modules":
                    continue

                if user_path != base_path and path_subdir == "":
                    continue # avoid loading 2.4x scripts

                for mod in modules_from_path(path, loaded_modules):
                    test_register(mod)

    # load addons
    used_ext = {ext.module for ext in _bpy.context.user_preferences.addons}
    paths = script_paths("addons")
    for path in paths:
        sys_path_ensure(path)

    for module_name in sorted(used_ext):
        mod = _test_import(module_name, loaded_modules)
        test_register(mod)

    if reload_scripts:
        import gc
        print("gc.collect() -> %d" % gc.collect())

    if _bpy.app.debug:
        print("Time %.4f" % (time.time() - t_main))


def expandpath(path):
    """
    Returns the absolute path relative to the current blend file using the "//" prefix.
    """
    if path.startswith("//"):
        return _os.path.join(_os.path.dirname(_bpy.data.filename), path[2:])

    return path


def relpath(path, start=None):
    """
    Returns the path relative to the current blend file using the "//" prefix.

    :arg start: Relative to this path, when not set the current filename is used.
    :type start: string
    """
    if not path.startswith("//"):
        if start is None:
            start = _os.path.dirname(_bpy.data.filename)
        return "//" + _os.path.relpath(path, start)

    return path


_unclean_chars = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, \
    17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, \
    35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 46, 47, 58, 59, 60, 61, 62, 63, \
    64, 91, 92, 93, 94, 96, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, \
    133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, \
    147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, \
    161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, \
    175, 176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, \
    189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, \
    203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214, 215, 216, \
    217, 218, 219, 220, 221, 222, 223, 224, 225, 226, 227, 228, 229, 230, \
    231, 232, 233, 234, 235, 236, 237, 238, 239, 240, 241, 242, 243, 244, \
    245, 246, 247, 248, 249, 250, 251, 252, 253, 254]

_unclean_chars = ''.join([chr(i) for i in _unclean_chars])


def clean_name(name, replace="_"):
    """
    Returns a name with characters replaced that may cause problems under various circumstances, such as writing to a file.
    All characters besides A-Z/a-z, 0-9 are replaced with "_"
    or the replace argument if defined.
    """
    for ch in _unclean_chars:
        name = name.replace(ch, replace)
    return name


def display_name(name):
    """
    Creates a display string from name to be used menus and the user interface.
    Capitalize the first letter in all lowercase names, mixed case names are kept as is.
    Intended for use with filenames and module names.
    """
    name_base = _os.path.splitext(name)[0]

    # string replacements
    name_base = name_base.replace("_colon_", ":")

    name_base = name_base.replace("_", " ")

    if name_base.islower():
        return name_base.capitalize()
    else:
        return name_base


# base scripts
_scripts = _os.path.join(_os.path.dirname(__file__), _os.path.pardir, _os.path.pardir)
_scripts = (_os.path.normpath(_scripts), )


def user_script_path():
    path = _bpy.context.user_preferences.filepaths.python_scripts_directory

    if path:
        path = _os.path.normpath(path)
        return path
    else:
        return None


def script_paths(subdir=None, user=True):
    """
    Returns a list of valid script paths from the home directory and user preferences.

    Accepts any number of string arguments which are joined to make a path.
    """
    scripts = list(_scripts)

    # add user scripts dir
    if user:
        user_script_path = _bpy.context.user_preferences.filepaths.python_scripts_directory
    else:
        user_script_path = None

    for path in home_paths("scripts") + (user_script_path, ):
        if path:
            path = _os.path.normpath(path)
            if path not in scripts and _os.path.isdir(path):
                scripts.append(path)

    if not subdir:
        return scripts

    script_paths = []
    for path in scripts:
        path_subdir = _os.path.join(path, subdir)
        if _os.path.isdir(path_subdir):
            script_paths.append(path_subdir)

    return script_paths


_presets = _os.path.join(_scripts[0], "presets") # FIXME - multiple paths


def preset_paths(subdir):
    '''
    Returns a list of paths for a spesific preset.
    '''

    return (_os.path.join(_presets, subdir), )
