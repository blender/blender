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

    loaded_modules.add(mod.__name__)  # should match mod.__name__ too
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

    for mod_name, mod_path in _bpy.path.module_names(path):
        mod = _test_import(mod_name, loaded_modules)
        if mod:
            modules.append(mod)

    return modules


_global_loaded_modules = []  # store loaded module names for reloading.
import bpy_types as _bpy_types  # keep for comparisons, never ever reload this.


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

    if reload_scripts:
        _bpy_types.TypeMap.clear()
        _bpy_types.PropertiesMap.clear()

    def register_module_call(mod):
        _bpy_types._register_module(mod.__name__)
        register = getattr(mod, "register", None)
        if register:
            try:
                register()
            except:
                traceback.print_exc()
        else:
            print("\nWarning! '%s' has no register function, this is now a requirement for registerable scripts." % mod.__file__)

    def unregister_module_call(mod):
        _bpy_types._unregister_module(mod.__name__)
        unregister = getattr(mod, "unregister", None)
        if unregister:
            try:
                unregister()
            except:
                traceback.print_exc()

    def sys_path_ensure(path):
        if path not in _sys.path:  # reloading would add twice
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
            register_module_call(mod)
            _global_loaded_modules.append(mod.__name__)

    if reload_scripts:

        # module names -> modules
        _global_loaded_modules[:] = [_sys.modules[mod_name] for mod_name in _global_loaded_modules]

        # loop over and unload all scripts
        _global_loaded_modules.reverse()
        for mod in _global_loaded_modules:
            unregister_module_call(mod)

        for mod in _global_loaded_modules:
            test_reload(mod)

        _global_loaded_modules[:] = []

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
                    continue  # avoid loading 2.4x scripts

                for mod in modules_from_path(path, loaded_modules):
                    test_register(mod)

    # load addons
    used_ext = {ext.module for ext in _bpy.context.user_preferences.addons}
    paths = script_paths("addons") + script_paths("addons_contrib")
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


# base scripts
_scripts = _os.path.join(_os.path.dirname(__file__), _os.path.pardir, _os.path.pardir)
_scripts = (_os.path.normpath(_scripts), )


def user_script_path():
    path = _bpy.context.user_preferences.filepaths.script_directory

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
        user_script_path = _bpy.context.user_preferences.filepaths.script_directory
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


_presets = _os.path.join(_scripts[0], "presets")  # FIXME - multiple paths


def preset_paths(subdir):
    '''
    Returns a list of paths for a specific preset.
    '''

    return (_os.path.join(_presets, subdir), )


def smpte_from_seconds(time, fps=None):
    '''
    Returns an SMPTE formatted string from the time in seconds: "HH:MM:SS:FF".

    If the *fps* is not given the current scene is used.
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

    if time >= 3600.0:  # hours
        hours = int(time / 3600.0)
        time = time % 3600.0
    if time >= 60.0:  # mins
        minutes = int(time / 60.0)
        time = time % 60.0

    seconds = int(time)
    frames = int(round(math.floor(((time - seconds) * fps))))

    return "%s%02d:%02d:%02d:%02d" % (neg, hours, minutes, seconds, frames)


def smpte_from_frame(frame, fps=None, fps_base=None):
    '''
    Returns an SMPTE formatted string from the frame: "HH:MM:SS:FF".

    If *fps* and *fps_base* are not given the current scene is used.
    '''

    if fps is None:
        fps = _bpy.context.scene.render.fps

    if fps_base is None:
        fps_base = _bpy.context.scene.render.fps_base

    return smpte_from_seconds((frame * fps_base) / fps, fps)
