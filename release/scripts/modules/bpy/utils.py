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

"""
This module contains utility functions specific to blender but
not assosiated with blenders internal data.
"""

__all__ = (
    "blend_paths",
    "keyconfig_set",
    "load_scripts",
    "modules_from_path",
    "preset_find",
    "preset_paths",
    "refresh_script_paths",
    "register_class",
    "register_module",
    "resource_path",
    "script_paths",
    "smpte_from_frame",
    "smpte_from_seconds",
    "unregister_class",
    "unregister_module",
    "user_resource",
    "user_script_path",
    )

from _bpy import register_class, unregister_class, blend_paths, resource_path
from _bpy import script_paths as _bpy_script_paths
from _bpy import user_resource as _user_resource

import bpy as _bpy
import os as _os
import sys as _sys

import addon_utils as _addon_utils

_script_module_dirs = "startup", "modules"


def _test_import(module_name, loaded_modules):
    use_time = _bpy.app.debug_python

    if module_name in loaded_modules:
        return None
    if "." in module_name:
        print("Ignoring '%s', can't import files containing "
              "multiple periods" % module_name)
        return None

    if use_time:
        import time
        t = time.time()

    try:
        mod = __import__(module_name)
    except:
        import traceback
        traceback.print_exc()
        return None

    if use_time:
        print("time %s %.4f" % (module_name, time.time() - t))

    loaded_modules.add(mod.__name__)  # should match mod.__name__ too
    return mod


def _sys_path_ensure(path):
    if path not in _sys.path:  # reloading would add twice
        _sys.path.insert(0, path)


def modules_from_path(path, loaded_modules):
    """
    Load all modules in a path and return them as a list.

    :arg path: this path is scanned for scripts and packages.
    :type path: string
    :arg loaded_modules: already loaded module names, files matching these
       names will be ignored.
    :type loaded_modules: set
    :return: all loaded modules.
    :rtype: list
    """
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

    :arg reload_scripts: Causes all scripts to have their unregister method
       called before loading.
    :type reload_scripts: bool
    :arg refresh_scripts: only load scripts which are not already loaded
       as modules.
    :type refresh_scripts: bool
    """
    use_time = _bpy.app.debug_python

    prefs = _bpy.context.user_preferences

    if use_time:
        import time
        t_main = time.time()

    loaded_modules = set()

    if refresh_scripts:
        original_modules = _sys.modules.values()

    if reload_scripts:
        _bpy_types.TypeMap.clear()

        # just unload, don't change user defaults, this means we can sync
        # to reload. note that they will only actually reload of the
        # modification time changes. This `won't` work for packages so...
        # its not perfect.
        for module_name in [ext.module for ext in prefs.addons]:
            _addon_utils.disable(module_name, default_set=False)

    def register_module_call(mod):
        register = getattr(mod, "register", None)
        if register:
            try:
                register()
            except:
                import traceback
                traceback.print_exc()
        else:
            print("\nWarning! '%s' has no register function, "
                  "this is now a requirement for registerable scripts" %
                  mod.__file__)

    def unregister_module_call(mod):
        unregister = getattr(mod, "unregister", None)
        if unregister:
            try:
                unregister()
            except:
                import traceback
                traceback.print_exc()

    def test_reload(mod):
        import imp
        # reloading this causes internal errors
        # because the classes from this module are stored internally
        # possibly to refresh internal references too but for now, best not to.
        if mod == _bpy_types:
            return mod

        try:
            return imp.reload(mod)
        except:
            import traceback
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
        _global_loaded_modules[:] = [_sys.modules[mod_name]
                                     for mod_name in _global_loaded_modules]

        # loop over and unload all scripts
        _global_loaded_modules.reverse()
        for mod in _global_loaded_modules:
            unregister_module_call(mod)

        for mod in _global_loaded_modules:
            test_reload(mod)

        _global_loaded_modules[:] = []

    for base_path in script_paths():
        for path_subdir in _script_module_dirs:
            path = _os.path.join(base_path, path_subdir)
            if _os.path.isdir(path):
                _sys_path_ensure(path)

                # only add this to sys.modules, don't run
                if path_subdir == "modules":
                    continue

                for mod in modules_from_path(path, loaded_modules):
                    test_register(mod)

    # deal with addons separately
    _addon_utils.reset_all(reload_scripts)

    # run the active integration preset
    filepath = preset_find(prefs.inputs.active_keyconfig, "keyconfig")

    if filepath:
        keyconfig_set(filepath)

    if reload_scripts:
        import gc
        print("gc.collect() -> %d" % gc.collect())

    if use_time:
        print("Python Script Load Time %.4f" % (time.time() - t_main))


# base scripts
_scripts = _os.path.join(_os.path.dirname(__file__),
                         _os.path.pardir,
                         _os.path.pardir,
                         )
_scripts = (_os.path.normpath(_scripts), )


def user_script_path():
    prefs = _bpy.context.user_preferences
    path = prefs.filepaths.script_directory

    if path:
        path = _os.path.normpath(path)
        return path
    else:
        return None


def script_paths(subdir=None, user_pref=True, check_all=False):
    """
    Returns a list of valid script paths.

    :arg subdir: Optional subdir.
    :type subdir: string
    :arg user_pref: Include the user preference script path.
    :type user_pref: bool
    :arg check_all: Include local, user and system paths rather just the paths
       blender uses.
    :type check_all: bool
    :return: script paths.
    :rtype: list
    """
    scripts = list(_scripts)
    prefs = _bpy.context.user_preferences

    # add user scripts dir
    user_script = prefs.filepaths.script_directory if user_pref else None

    if check_all:
        # all possible paths
        base_paths = tuple(_os.path.join(resource_path(res), "scripts")
                           for res in ('LOCAL', 'USER', 'SYSTEM'))
    else:
        # only paths blender uses
        base_paths = _bpy_script_paths()

    for path in base_paths + (user_script, ):
        if path:
            path = _os.path.normpath(path)
            if path not in scripts and _os.path.isdir(path):
                scripts.append(path)

    if subdir is None:
        return scripts

    scripts_subdir = []
    for path in scripts:
        path_subdir = _os.path.join(path, subdir)
        if _os.path.isdir(path_subdir):
            scripts_subdir.append(path_subdir)

    return scripts_subdir


def refresh_script_paths():
    """
    Run this after creating new script paths to update sys.path
    """

    for base_path in script_paths():
        for path_subdir in _script_module_dirs:
            path = _os.path.join(base_path, path_subdir)
            if _os.path.isdir(path):
                _sys_path_ensure(path)

    for path in _addon_utils.paths():
        _sys_path_ensure(path)
        path = _os.path.join(path, "modules")
        if _os.path.isdir(path):
            _sys_path_ensure(path)


def preset_paths(subdir):
    """
    Returns a list of paths for a specific preset.

    :arg subdir: preset subdirectory (must not be an absolute path).
    :type subdir: string
    :return: script paths.
    :rtype: list
    """
    dirs = []
    for path in script_paths("presets", check_all=True):
        directory = _os.path.join(path, subdir)
        if not directory.startswith(path):
            raise Exception("invalid subdir given %r" % subdir)
        elif _os.path.isdir(directory):
            dirs.append(directory)

    # Find addons preset paths
    import addon_utils
    for path in addon_utils.paths():
        directory = _os.path.join(path, "presets", subdir)
        if _os.path.isdir(directory):
            dirs.append(directory)

    return dirs


def smpte_from_seconds(time, fps=None):
    """
    Returns an SMPTE formatted string from the time in seconds: "HH:MM:SS:FF".

    If the *fps* is not given the current scene is used.
    """
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
    if time >= 60.0:  # minutes
        minutes = int(time / 60.0)
        time = time % 60.0

    seconds = int(time)
    frames = int(round(math.floor(((time - seconds) * fps))))

    return "%s%02d:%02d:%02d:%02d" % (neg, hours, minutes, seconds, frames)


def smpte_from_frame(frame, fps=None, fps_base=None):
    """
    Returns an SMPTE formatted string from the frame: "HH:MM:SS:FF".

    If *fps* and *fps_base* are not given the current scene is used.

    :arg time: time in seconds.
    :type time: number or timedelta object
    :return: the frame.
    :rtype: float
    """

    if fps is None:
        fps = _bpy.context.scene.render.fps

    if fps_base is None:
        fps_base = _bpy.context.scene.render.fps_base

    return smpte_from_seconds((frame * fps_base) / fps, fps)


def time_from_frame(frame, fps=None, fps_base=None):
    """
    Returns the time from a frame number .

    If *fps* and *fps_base* are not given the current scene is used.

    :arg frame: number.
    :type frame: the frame number
    :return: the time in seconds.
    :rtype: timedate.timedelta
    """

    if fps is None:
        fps = _bpy.context.scene.render.fps

    if fps_base is None:
        fps_base = _bpy.context.scene.render.fps_base

    from datetime import timedelta

    return timedelta((frame * fps_base) / fps)


def time_to_frame(time, fps=None, fps_base=None):
    """
    Returns a float frame number from a time given in seconds or
    as a timedate.timedelta object.

    If *fps* and *fps_base* are not given the current scene is used.

    :arg time: time in seconds.
    :type time: number or a timedate.timedelta object
    :return: the frame.
    :rtype: float
    """

    if fps is None:
        fps = _bpy.context.scene.render.fps

    if fps_base is None:
        fps_base = _bpy.context.scene.render.fps_base

    from datetime import timedelta

    if isinstance(time, timedelta):
        time = time.total_seconds()

    return (time / fps_base) * fps


def preset_find(name, preset_path, display_name=False, ext=".py"):
    if not name:
        return None

    for directory in preset_paths(preset_path):

        if display_name:
            filename = ""
            for fn in _os.listdir(directory):
                if fn.endswith(ext) and name == _bpy.path.display_name(fn):
                    filename = fn
                    break
        else:
            filename = name + ext

        if filename:
            filepath = _os.path.join(directory, filename)
            if _os.path.exists(filepath):
                return filepath


def keyconfig_set(filepath):
    from os.path import basename, splitext

    if _bpy.app.debug_python:
        print("loading preset:", filepath)

    keyconfigs = _bpy.context.window_manager.keyconfigs

    keyconfigs_old = keyconfigs[:]

    try:
        keyfile = open(filepath)
        exec(compile(keyfile.read(), filepath, 'exec'), {"__file__": filepath})
        keyfile.close()
    except:
        import traceback
        traceback.print_exc()

    kc_new = [kc for kc in keyconfigs if kc not in keyconfigs_old][0]

    kc_new.name = ""

    # remove duplicates
    name = splitext(basename(filepath))[0]
    while True:
        kc_dupe = keyconfigs.get(name)
        if kc_dupe:
            keyconfigs.remove(kc_dupe)
        else:
            break

    kc_new.name = name
    keyconfigs.active = kc_new


def user_resource(resource_type, path="", create=False):
    """
    Return a user resource path (normally from the users home directory).

    :arg type: Resource type in ['DATAFILES', 'CONFIG', 'SCRIPTS', 'AUTOSAVE'].
    :type type: string
    :arg subdir: Optional subdirectory.
    :type subdir: string
    :arg create: Treat the path as a directory and create
       it if its not existing.
    :type create: boolean
    :return: a path.
    :rtype: string
    """

    target_path = _user_resource(resource_type, path)

    if create:
        # should always be true.
        if target_path:
            # create path if not existing.
            if not _os.path.exists(target_path):
                try:
                    _os.makedirs(target_path)
                except:
                    import traceback
                    traceback.print_exc()
                    target_path = ""
            elif not _os.path.isdir(target_path):
                print("Path %r found but isn't a directory!" % target_path)
                target_path = ""

    return target_path


def _bpy_module_classes(module, is_registered=False):
    typemap_list = _bpy_types.TypeMap.get(module, ())
    i = 0
    while i < len(typemap_list):
        cls_weakref = typemap_list[i]
        cls = cls_weakref()

        if cls is None:
            del typemap_list[i]
        else:
            if is_registered == cls.is_registered:
                yield cls
            i += 1


def register_module(module, verbose=False):
    if verbose:
        print("bpy.utils.register_module(%r): ..." % module)
    cls = None
    for cls in _bpy_module_classes(module, is_registered=False):
        if verbose:
            print("    %r" % cls)
        try:
            register_class(cls)
        except:
            print("bpy.utils.register_module(): "
                  "failed to registering class %r" % cls)
            import traceback
            traceback.print_exc()
    if verbose:
        print("done.\n")
    if cls is None:
        raise Exception("register_module(%r): defines no classes" % module)


def unregister_module(module, verbose=False):
    if verbose:
        print("bpy.utils.unregister_module(%r): ..." % module)
    for cls in _bpy_module_classes(module, is_registered=True):
        if verbose:
            print("    %r" % cls)
        try:
            unregister_class(cls)
        except:
            print("bpy.utils.unregister_module(): "
                  "failed to unregistering class %r" % cls)
            import traceback
            traceback.print_exc()
    if verbose:
        print("done.\n")
