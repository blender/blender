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
This module contains utility functions specific to blender but
not assosiated with blenders internal data.
"""

import bpy as _bpy
import os as _os
import sys as _sys

from _bpy import blend_paths
from _bpy import script_paths as _bpy_script_paths
from _bpy import LoadModule, UnloadModule


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
    :arg loaded_modules: already loaded module names, files matching these names will be ignored.
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

    # must be set back to True on exits
    _bpy_types._register_immediate = False

    t_main = time.time()

    loaded_modules = set()

    if refresh_scripts:
        original_modules = _sys.modules.values()

    def unload_module(mod):
        UnloadModule(mod.__name__)
        unregister = getattr(mod, "unregister", None)
        if unregister:
            try:
                unregister()
            except:
                traceback.print_exc()
                
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
            LoadModule(mod.__name__, reload_scripts)
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

        # TODO, this is broken but should work, needs looking into
        '''
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
        '''

        # loop over and unload all scripts
        _loaded.reverse()
        for mod in _loaded:
            unload_module(mod)

        for mod in _loaded:
            reload(mod)

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
        print("Python Script Load Time %.4f" % (time.time() - t_main))
    
    _bpy_types._register_immediate = True


def expandpath(path):
    """
    Returns the absolute path relative to the current blend file using the "//" prefix.
    """
    if path.startswith("//"):
        return _os.path.join(_os.path.dirname(_bpy.data.filepath), path[2:])

    return path


def relpath(path, start=None):
    """
    Returns the path relative to the current blend file using the "//" prefix.

    :arg start: Relative to this path, when not set the current filename is used.
    :type start: string
    """
    if not path.startswith("//"):
        if start is None:
            start = _os.path.dirname(_bpy.data.filepath)
        return "//" + _os.path.relpath(path, start)

    return path


def clean_name(name, replace="_"):
    """
    Returns a name with characters replaced that may cause problems under various circumstances, such as writing to a file.
    All characters besides A-Z/a-z, 0-9 are replaced with "_"
    or the replace argument if defined.
    """

    unclean_chars = \
                 "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\
                  \x0f\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\
                  \x1e\x1f\x20\x21\x22\x23\x24\x25\x26\x27\x28\x29\x2a\x2b\x2c\
                  \x2e\x2f\x3a\x3b\x3c\x3d\x3e\x3f\x40\x5b\x5c\x5d\x5e\x60\x7b\
                  \x7c\x7d\x7e\x7f\x80\x81\x82\x83\x84\x85\x86\x87\x88\x89\x8a\
                  \x8b\x8c\x8d\x8e\x8f\x90\x91\x92\x93\x94\x95\x96\x97\x98\x99\
                  \x9a\x9b\x9c\x9d\x9e\x9f\xa0\xa1\xa2\xa3\xa4\xa5\xa6\xa7\xa8\
                  \xa9\xaa\xab\xac\xad\xae\xaf\xb0\xb1\xb2\xb3\xb4\xb5\xb6\xb7\
                  \xb8\xb9\xba\xbb\xbc\xbd\xbe\xbf\xc0\xc1\xc2\xc3\xc4\xc5\xc6\
                  \xc7\xc8\xc9\xca\xcb\xcc\xcd\xce\xcf\xd0\xd1\xd2\xd3\xd4\xd5\
                  \xd6\xd7\xd8\xd9\xda\xdb\xdc\xdd\xde\xdf\xe0\xe1\xe2\xe3\xe4\
                  \xe5\xe6\xe7\xe8\xe9\xea\xeb\xec\xed\xee\xef\xf0\xf1\xf2\xf3\
                  \xf4\xf5\xf6\xf7\xf8\xf9\xfa\xfb\xfc\xfd\xfe"

    for ch in unclean_chars:
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

    for path in _bpy_script_paths() + (user_script_path, ):
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
    Returns a list of paths for a specific preset.
    '''

    return (_os.path.join(_presets, subdir), )


def smpte_from_seconds(time, fps=None):
    '''
    Returns an SMPTE formatted string from the time in seconds: "HH:MM:SS:FF".

    If the fps is not given the current scene is used.
    '''
    import math

    if fps is None:
        fps = _bpy.context.scene.render.fps

    hours = minutes = seconds = frames = 0

    if time < 0:
        time = - time
        neg = "-"
    else:
        neg = ""

    if time >= 3600.0: # hours
        hours = int(time / 3600.0)
        time = time % 3600.0
    if time >= 60.0: # mins
        minutes = int(time / 60.0)
        time = time % 60.0

    seconds = int(time)
    frames = int(round(math.floor(((time - seconds) * fps))))

    return "%s%02d:%02d:%02d:%02d" % (neg, hours, minutes, seconds, frames)


def smpte_from_frame(frame, fps=None, fps_base=None):
    '''
    Returns an SMPTE formatted string from the frame: "HH:MM:SS:FF".

    If the fps and fps_base are not given the current scene is used.
    '''

    if fps is None:
        fps = _bpy.context.scene.render.fps

    if fps_base is None:
        fps_base = _bpy.context.scene.render.fps_base

    return smpte_from_seconds((frame * fps_base) / fps, fps)
