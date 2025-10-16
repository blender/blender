# SPDX-FileCopyrightText: 2009-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
This module contains utility functions specific to blender but
not associated with blenders internal data.
"""

__all__ = (
    "blend_paths",
    "escape_identifier",
    "flip_name",
    "unescape_identifier",
    "keyconfig_init",
    "keyconfig_set",
    "load_scripts",
    "modules_from_path",
    "preset_find",
    "preset_paths",
    "refresh_script_paths",
    "app_template_paths",
    "register_class",
    "register_cli_command",
    "time_from_frame",
    "unregister_cli_command",
    "register_manual_map",
    "unregister_manual_map",
    "register_preset_path",
    "unregister_preset_path",
    "register_classes_factory",
    "register_submodule_factory",
    "register_tool",
    "make_rna_paths",
    "manual_map",
    "manual_language_code",
    "previews",
    "resource_path",
    "script_path_user",
    "extension_path_user",
    "script_paths",
    "smpte_from_frame",
    "smpte_from_seconds",
    "units",
    "unregister_class",
    "unregister_tool",
    "user_resource",
    "execfile",
    "expose_bundled_modules",
)

from _bpy import (
    _utils_units as units,
    blend_paths,
    escape_identifier,
    flip_name,
    unescape_identifier,
    register_class,
    register_cli_command,
    resource_path,
    script_paths as _bpy_script_paths,
    unregister_class,
    unregister_cli_command,
    user_resource as _user_resource,
    system_resource,
)

import bpy as _bpy
import os as _os
import sys as _sys

import addon_utils as _addon_utils

_preferences = _bpy.context.preferences
_is_factory_startup = _bpy.app.factory_startup

# Directories added to the start of `sys.path` for all of Blender's "scripts" directories.
_script_module_dirs = "startup", "modules"

# Base scripts, this points to the directory containing: "modules" & "startup" (see `_script_module_dirs`).
# In Blender's code-base this is `./scripts`.
#
# NOTE: in virtually all cases this should match `BLENDER_SYSTEM_SCRIPTS` as this script is itself a system script,
# it must be in the `BLENDER_SYSTEM_SCRIPTS` by definition and there is no need for a look-up from `_bpy_script_paths`.
_script_base_dir = _os.path.dirname(_os.path.dirname(_os.path.dirname(_os.path.dirname(__file__))))

# Paths managed by `register_preset_path` & `unregister_preset_path`.
_preset_path_registry = set()


def execfile(filepath, *, mod=None):
    """
    Execute a file path as a Python script.

    :arg filepath: Path of the script to execute.
    :type filepath: str
    :arg mod: Optional cached module, the result of a previous execution.
    :type mod: ModuleType | None
    :return: The module which can be passed back in as ``mod``.
    :rtype: ModuleType
    """

    import importlib.util
    mod_name = "__main__"
    mod_spec = importlib.util.spec_from_file_location(mod_name, filepath)
    if mod is None:
        mod = importlib.util.module_from_spec(mod_spec)

    # While the module name is not added to `sys.modules`, it's important to temporarily
    # include this so statements such as `sys.modules[cls.__module__].__dict__` behave as expected.
    # See: https://bugs.python.org/issue9499 for details.
    modules = _sys.modules
    mod_orig = modules.get(mod_name, None)
    modules[mod_name] = mod

    # No error suppression, just ensure `sys.modules[mod_name]` is properly restored in the case of an error.
    try:
        mod_spec.loader.exec_module(mod)
    finally:
        if mod_orig is None:
            modules.pop(mod_name, None)
        else:
            modules[mod_name] = mod_orig

    return mod


def _test_import(module_name, loaded_modules):
    use_time = _bpy.app.debug_python

    if module_name in loaded_modules:
        return None
    if "." in module_name:
        print("Ignoring '{:s}', cannot import files containing multiple periods".format(module_name))
        return None

    if use_time:
        import time
        t = time.time()

    try:
        mod = __import__(module_name)
    except Exception:
        import traceback
        traceback.print_exc()
        return None

    if use_time:
        print("time {:s} {:.4f}".format(module_name, time.time() - t))

    loaded_modules.add(mod.__name__)  # should match mod.__name__ too
    return mod


# Check before adding paths as reloading would add twice.

# Storing and restoring the full `sys.path` is risky as this may be intentionally modified
# by technical users/developers.
#
# Instead, track which paths have been added, clearing them before refreshing.
# This supports the case of loading a new preferences file which may reset scripts path.
_sys_path_ensure_paths = set()


def _sys_path_ensure_prepend(path):
    if path not in _sys.path:
        _sys.path.insert(0, path)
        _sys_path_ensure_paths.add(path)


def _sys_path_ensure_append(path):
    if path not in _sys.path:
        _sys.path.append(path)
        _sys_path_ensure_paths.add(path)


def modules_from_path(path, loaded_modules):
    """
    Load all modules in a path and return them as a list.

    :arg path: this path is scanned for scripts and packages.
    :type path: str
    :arg loaded_modules: already loaded module names, files matching these
       names will be ignored.
    :type loaded_modules: set[ModuleType]
    :return: all loaded modules.
    :rtype: list[ModuleType]
    """
    modules = []

    for mod_name, _mod_path in _bpy.path.module_names(path):
        mod = _test_import(mod_name, loaded_modules)
        if mod:
            modules.append(mod)

    return modules


# Store registered module names for reloading.
# Currently used for "startup" modules.
_registered_module_names = []
# Keep for comparisons, never ever reload this.
import _bpy_types


def _register_module_call(mod):
    register = getattr(mod, "register", None)
    if register:
        try:
            register()
        except Exception:
            import traceback
            traceback.print_exc()
    else:
        print(
            "\nWarning! {!r} has no register function, "
            "this is now a requirement for registerable scripts".format(mod.__file__)
        )


def _unregister_module_call(mod):
    unregister = getattr(mod, "unregister", None)
    if unregister:
        try:
            unregister()
        except Exception:
            import traceback
            traceback.print_exc()


def load_scripts(*, reload_scripts=False, refresh_scripts=False, extensions=True):
    """
    Load scripts and run each modules register function.

    :arg reload_scripts: Causes all scripts to have their unregister method
       called before loading.
    :type reload_scripts: bool
    :arg refresh_scripts: only load scripts which are not already loaded
       as modules.
    :type refresh_scripts: bool
    :arg extensions: Loads additional scripts (add-ons & app-templates).
    :type extensions: bool
    """
    use_time = use_class_register_check = _bpy.app.debug_python
    use_user = not _is_factory_startup

    if use_time:
        import time
        t_main = time.time()

    loaded_modules = set()

    if refresh_scripts:
        original_modules = _sys.modules.values()

    if reload_scripts:
        # just unload, don't change user defaults, this means we can sync
        # to reload. note that they will only actually reload of the
        # modification time changes. This `won't` work for packages so...
        # its not perfect.
        for addon_module_name in [ext.module for ext in _preferences.addons]:
            _addon_utils.disable(addon_module_name)

    def test_reload(mod):
        import importlib
        # reloading this causes internal errors
        # because the classes from this module are stored internally
        # possibly to refresh internal references too but for now, best not to.
        if mod == _bpy_types:
            return mod

        try:
            return importlib.reload(mod)
        except Exception:
            import traceback
            traceback.print_exc()

    def test_register(mod):

        if refresh_scripts and mod in original_modules:
            return

        if reload_scripts and mod:
            print("Reloading:", mod)
            mod = test_reload(mod)

        if mod:
            _register_module_call(mod)
            _registered_module_names.append(mod.__name__)

    if reload_scripts:
        # Module names -> modules.
        #
        # Reverse the modules so they are unregistered in the reverse order they're registered.
        # While this isn't essential for the most part, it ensures any inter-dependencies can be handled properly.
        registered_modules = [
            mod for mod in map(_sys.modules.get, reversed(_registered_module_names))
            if mod is not None
        ]

        # This should never happen, only report this to notify developers that something unexpected happened.
        if len(registered_modules) != len(_registered_module_names):
            print(
                "Warning: globally loaded modules not found in sys.modules:",
                [mod_name for mod_name in _registered_module_names if mod_name not in _sys.modules]
            )
        _registered_module_names.clear()

        # Loop over and unload all scripts.
        for mod in registered_modules:
            _unregister_module_call(mod)

        for mod in registered_modules:
            test_reload(mod)
        del registered_modules

        # Update key-maps to account for operators no longer existing.
        # Typically unloading operators would refresh the event system (such as disabling an add-on)
        # however reloading scripts re-enable all add-ons immediately (which may inspect key-maps).
        # For this reason it's important to update key-maps which will have been tagged to update.
        # Without this, add-on register functions accessing key-map properties can crash, see: #111702.
        _bpy.context.window_manager.keyconfigs.update(keep_properties=True)

    from _bpy_restrict_state import RestrictBlend

    with RestrictBlend():
        for base_path in script_paths(use_user=use_user):
            for path_subdir in _script_module_dirs:
                path = _os.path.join(base_path, path_subdir)
                if _os.path.isdir(path):
                    _sys_path_ensure_prepend(path)

                    # Only add to `sys.modules` unless this is 'startup'.
                    if path_subdir == "startup":
                        for mod in modules_from_path(path, loaded_modules):
                            test_register(mod)

    if reload_scripts:
        # Update key-maps for key-map items referencing operators defined in "startup".
        # Without this, key-map items wont be set properly, see: #113309.
        _bpy.context.window_manager.keyconfigs.update()

    if extensions:
        load_scripts_extensions(reload_scripts=reload_scripts)

    if reload_scripts:
        _bpy.context.window_manager.tag_script_reload()

        import gc
        print("gc.collect() -> {:d}".format(gc.collect()))

    if use_time:
        print("Python Script Load Time {:.4f}".format(time.time() - t_main))

    if use_class_register_check:
        for cls in _bpy.types.bpy_struct.__subclasses__():
            if getattr(cls, "is_registered", False):
                for subcls in cls.__subclasses__():
                    if not subcls.is_registered:
                        print("Warning, unregistered class: {:s}({:s})".format(subcls.__name__, cls.__name__))


# Internal only, called on exit by `WM_exit_ex`.
def _on_exit():
    # Disable all add-ons.
    _addon_utils.disable_all()

    # Call `unregister` function on internal startup module.
    # Must only be used as part of Blender 'exit' process.
    from _bpy_restrict_state import RestrictBlend
    with RestrictBlend():
        for mod_name in reversed(_registered_module_names):
            if (mod := _sys.modules.get(mod_name)) is None:
                print("Warning: module", repr(mod_name), "not found in sys.modules")
                continue
            _unregister_module_call(mod)


def load_scripts_extensions(*, reload_scripts=False):
    """
    Load extensions scripts (add-ons and app-templates)

    :arg reload_scripts: Causes all scripts to have their unregister method
       called before loading.
    :type reload_scripts: bool
    """
    # load template (if set)
    if any(_bpy.utils.app_template_paths()):
        import bl_app_template_utils
        bl_app_template_utils.reset(reload_scripts=reload_scripts)
        del bl_app_template_utils

    # Deal with add-ons separately.
    _initialize_once = getattr(_addon_utils, "_initialize_once", None)
    if _initialize_once is not None:
        # first time, use fast-path
        _initialize_once()
        del _addon_utils._initialize_once
    else:
        _addon_utils.reset_all(reload_scripts=reload_scripts)
    del _initialize_once


def script_path_user():
    """returns the env var and falls back to home dir or None"""
    path = _user_resource('SCRIPTS')
    return _os.path.normpath(path) if path else None


def script_paths_pref():
    """Returns a list of user preference script directories."""
    paths = []
    for script_directory in _preferences.filepaths.script_directories:
        directory = script_directory.directory
        if directory:
            paths.append(_os.path.normpath(directory))
    return paths


def script_paths_system_environment():
    """Returns a list of system script directories from environment variables."""
    if env_system_paths := _os.environ.get("BLENDER_SYSTEM_SCRIPTS"):
        return [_os.path.normpath(p) for p in env_system_paths.split(_os.pathsep) if p]
    return []


def script_paths(*, subdir=None, user_pref=True, check_all=False, use_user=True, use_system_environment=True):
    """
    Returns a list of valid script paths.

    :arg subdir: Optional subdir.
    :type subdir: str
    :arg user_pref: Include the user preference script paths.
    :type user_pref: bool
    :arg check_all: Include local, user and system paths rather just the paths Blender uses.
    :type check_all: bool
    :arg use_user: Include user paths
    :type use_user: bool
    :arg use_system_environment: Include BLENDER_SYSTEM_SCRIPTS variable path
    :type use_system_environment: bool
    :return: script paths.
    :rtype: list[str]
    """

    if check_all or use_user:
        path_system, path_user = _bpy_script_paths()

    base_paths = []

    if check_all:
        # Order: 'LOCAL', 'USER', 'SYSTEM' (where user is optional).
        if path_local := resource_path('LOCAL'):
            base_paths.append(_os.path.join(path_local, "scripts"))
        if use_user:
            base_paths.append(path_user)
        base_paths.append(path_system)  # Same as: `system_resource('SCRIPTS')`.

    # Note that `_script_base_dir` may be either:
    # - `os.path.join(bpy.utils.resource_path('LOCAL'), "scripts")`
    # - `bpy.utils.system_resource('SCRIPTS')`.
    # When `check_all` is enabled duplicate paths will be added however
    # paths are de-duplicated so it wont cause problems.
    base_paths.append(_script_base_dir)

    if not check_all:
        if use_user:
            base_paths.append(path_user)

    if user_pref:
        base_paths.extend(script_paths_pref())

    if use_system_environment:
        base_paths.extend(script_paths_system_environment())

    scripts = []
    for path in base_paths:
        if not path:
            continue

        path = _os.path.normpath(path)
        if subdir is not None:
            path = _os.path.join(path, subdir)

        if path in scripts:
            continue
        if not _os.path.isdir(path):
            continue
        scripts.append(path)

    return scripts


def refresh_script_paths():
    """
    Run this after creating new script paths to update sys.path
    """

    for path in _sys_path_ensure_paths:
        try:
            _sys.path.remove(path)
        except ValueError:
            pass
    _sys_path_ensure_paths.clear()

    for base_path in script_paths():
        for path_subdir in _script_module_dirs:
            path = _os.path.join(base_path, path_subdir)
            if _os.path.isdir(path):
                _sys_path_ensure_prepend(path)

    for path in _addon_utils.paths():
        _sys_path_ensure_append(path)
        path = _os.path.join(path, "modules")
        if _os.path.isdir(path):
            _sys_path_ensure_append(path)


def app_template_paths(*, path=None):
    """
    Returns valid application template paths.

    :arg path: Optional subdir.
    :type path: str
    :return: App template paths.
    :rtype: Iterator[str]
    """
    subdir_args = (path,) if path is not None else ()
    # Note: keep in sync with: Blender's 'BKE_appdir_app_template_any'.
    # Uses BLENDER_USER_SCRIPTS
    path_test = _user_resource('SCRIPTS', path=_os.path.join("startup", "bl_app_templates_user", *subdir_args))
    if path_test and _os.path.isdir(path_test):
        yield path_test

    # Uses BLENDER_SYSTTEM_SCRIPTS
    for path in script_paths_system_environment():
        path_test = _os.path.join(path, "startup", "bl_app_templates_system", *subdir_args)
        if _os.path.isdir(path_test):
            yield path_test

    # Uses default local or system location.
    path_test = system_resource('SCRIPTS', path=_os.path.join("startup", "bl_app_templates_system", *subdir_args))
    if path_test and _os.path.isdir(path_test):
        yield path_test


def preset_paths(subdir):
    """
    Returns a list of paths for a specific preset.

    :arg subdir: preset subdirectory (must not be an absolute path).
    :type subdir: str
    :return: Script paths.
    :rtype: list[str]
    """
    dirs = []
    for path in script_paths(subdir="presets"):
        directory = _os.path.join(path, subdir)
        if not directory.startswith(path):
            raise Exception("invalid subdir given {!r}".format(subdir))
        elif _os.path.isdir(directory):
            dirs.append(directory)

    # Find addons preset paths
    for path in _addon_utils.paths():
        directory = _os.path.join(path, "presets", subdir)
        if _os.path.isdir(directory):
            dirs.append(directory)

    for path in _preset_path_registry:
        directory = _os.path.join(path, "presets", subdir)
        if _os.path.isdir(directory):
            dirs.append(directory)

    return dirs


def register_preset_path(path):
    """
    Register a preset search path.

    :arg path: preset directory (must be an absolute path).

       This path must contain a "presets" subdirectory which will typically contain presets for add-ons.

       You may call ``bpy.utils.register_preset_path(os.path.dirname(__file__))`` from an add-ons ``__init__.py`` file.
       When the ``__init__.py`` is in the same location as a ``presets`` directory.
       For example an operators preset would be located under: ``presets/operator/{operator.id}/``
       where ``operator.id`` is the ``bl_idname`` of the operator.
    :type path: str
    :return: success
    :rtype: bool
    """
    set_len = len(_preset_path_registry)
    _preset_path_registry.add(path)
    if set_len == len(_preset_path_registry):
        print("Warning: preset path already registered", path)
        return False
    return True


def unregister_preset_path(path):
    """
    Unregister a preset search path.

    :arg path: preset directory (must be an absolute path).

       This must match the registered path exactly.
    :type path: str
    :return: success
    :rtype: bool
    """
    set_len = len(_preset_path_registry)
    _preset_path_registry.discard(path)
    if set_len == len(_preset_path_registry):
        print("Warning: preset path not registered", path)
        return False
    return True


def _is_path_parent_of(parent_path, path):
    try:
        if _os.path.samefile(
                _os.path.commonpath([parent_path]),
                _os.path.commonpath([parent_path, path])
        ):
            return True

    # NOTE: skipping in the case files can't be found isn't ideal because
    # `/a/b` is logically *inside* `/a/` irrespective of the permissions or existence of either paths.
    # Nevertheless, skip them as it's impractical to operate on paths that can't be accessed.
    # In all likelihood the caller is also unable to properly handle the result.
    except FileNotFoundError:
        # The path we tried to look up doesn't exist.
        pass
    except ValueError:
        # Happens on Windows when paths don't have the same drive.
        pass
    except PermissionError:
        # When either of the paths don't have permissions to access.
        pass

    return False


def is_path_builtin(path):
    """
    Returns True if the path is one of the built-in paths used by Blender.

    :arg path: Path you want to check if it is in the built-in settings directory
    :type path: str
    :rtype: bool
    """
    # Note that this function isn't optimized for speed,
    # it's intended to be used to check if it's OK to remove presets.
    #
    # If this is used in a draw-loop for example, we could cache some of the values.
    user_path = resource_path('USER')

    for res in ('SYSTEM', 'LOCAL'):
        parent_path = resource_path(res)
        if not parent_path or parent_path == user_path:
            # Make sure that the current path is not empty string and that it is
            # not the same as the user config path. IE "~/.config/blender" on Linux
            # This can happen on portable installs.
            continue

        if _is_path_parent_of(parent_path, path):
            return True

    return False


def is_path_extension(path):
    """
    Returns True if the path is from an extensions repository.

    :arg path: Path to check if it is within an extension repository.
    :type path: str
    :rtype: bool
    """
    for repo in _preferences.extensions.repos:
        if not repo.enabled:
            continue
        # NOTE: since these paths are user defined, they can be anything.
        # Empty or malformed paths will be skipped.
        if _is_path_parent_of(repo.directory, path):
            return True

    return False


def smpte_from_seconds(time, *, fps=None, fps_base=None):
    """
    Returns an SMPTE formatted string from the *time*:
    ``HH:MM:SS:FF``.

    If *fps* and *fps_base* are not given the current scene is used.

    :arg time: time in seconds.
    :type time: int | float | datetime.timedelta
    :return: the frame string.
    :rtype: str
    """

    return smpte_from_frame(
        time_to_frame(time, fps=fps, fps_base=fps_base),
        fps=fps,
        fps_base=fps_base,
    )


def smpte_from_frame(frame, *, fps=None, fps_base=None):
    """
    Returns an SMPTE formatted string from the *frame*:
    ``HH:MM:SS:FF``.

    If *fps* and *fps_base* are not given the current scene is used.

    :arg frame: frame number.
    :type frame: int | float
    :return: the frame string.
    :rtype: str
    """

    if fps is None:
        fps = _bpy.context.scene.render.fps

    if fps_base is None:
        fps_base = _bpy.context.scene.render.fps_base

    fps = fps / fps_base
    sign = "-" if frame < 0 else ""
    frame = abs(frame)

    return (
        "{:s}{:02d}:{:02d}:{:02d}:{:02d}".format(
            sign,
            int(frame / (3600 * fps)),          # HH
            int((frame / (60 * fps)) % 60),     # MM
            int((frame / fps) % 60),            # SS
            int(frame % fps),                   # FF
        )
    )


def time_from_frame(frame, *, fps=None, fps_base=None):
    """
    Returns the time from a frame number .

    If *fps* and *fps_base* are not given the current scene is used.

    :arg frame: number.
    :type frame: int | float
    :return: the time in seconds.
    :rtype: datetime.timedelta
    """

    if fps is None:
        fps = _bpy.context.scene.render.fps

    if fps_base is None:
        fps_base = _bpy.context.scene.render.fps_base

    fps = fps / fps_base

    from datetime import timedelta

    return timedelta(0, frame / fps)


def time_to_frame(time, *, fps=None, fps_base=None):
    """
    Returns a float frame number from a time given in seconds or
    as a datetime.timedelta object.

    If *fps* and *fps_base* are not given the current scene is used.

    :arg time: time in seconds.
    :type time: float | int | datetime.timedelta
    :return: The frame.
    :rtype: float | int | datetime.timedelta
    """

    if fps is None:
        fps = _bpy.context.scene.render.fps

    if fps_base is None:
        fps_base = _bpy.context.scene.render.fps_base

    fps = fps / fps_base

    from datetime import timedelta

    if isinstance(time, timedelta):
        time = time.total_seconds()

    return time * fps


def preset_find(name, preset_path, *, display_name=False, ext=".py"):
    if not name:
        return None

    for directory in preset_paths(preset_path):

        if display_name:
            filename = ""
            for fn in _os.listdir(directory):
                if fn.endswith(ext) and name == _bpy.path.display_name(fn, title_case=False):
                    filename = fn
                    break
        else:
            filename = name + ext

        if filename:
            filepath = _os.path.join(directory, filename)
            if _os.path.exists(filepath):
                return filepath


def keyconfig_init():
    # Key configuration initialization and refresh, called from the Blender
    # window manager on startup and refresh.
    default_config = "Blender"
    active_config = _preferences.keymap.active_keyconfig

    # Load the default key configuration.
    filepath = preset_find(default_config, "keyconfig")
    keyconfig_set(filepath)

    # Set the active key configuration if different.
    if default_config != active_config:
        filepath = preset_find(active_config, "keyconfig")
        if filepath:
            keyconfig_set(filepath)


def keyconfig_set(filepath, *, report=None):
    from os.path import basename, splitext

    if _bpy.app.debug_python:
        print("loading preset:", filepath)

    keyconfigs = _bpy.context.window_manager.keyconfigs
    name = splitext(basename(filepath))[0]

    # Store the old key-configuration case of error, to know if it should be removed or not on failure.
    kc_old = keyconfigs.get(name)

    try:
        error_msg = ""
        execfile(filepath)
    except Exception:
        import traceback
        error_msg = traceback.format_exc()

    kc_new = keyconfigs.get(name)

    if error_msg:
        if report is not None:
            report({'ERROR'}, error_msg)
        print(error_msg)
        if (kc_new is not None) and (kc_new != kc_old):
            keyconfigs.remove(kc_new)
        return False

    # Get name, exception for default keymap to keep backwards compatibility.
    if kc_new is None:
        if report is not None:
            report({'ERROR'}, "Failed to load keymap {!r}".format(filepath))
        return False
    else:
        keyconfigs.active = kc_new
        return True


def user_resource(resource_type, *, path="", create=False):
    """
    Return a user resource path (normally from the users home directory).

    :arg resource_type: Resource type in ['DATAFILES', 'CONFIG', 'SCRIPTS', 'EXTENSIONS'].
    :type resource_type: str
    :arg path: Optional subdirectory.
    :type path: str
    :arg create: Treat the path as a directory and create it if its not existing.
    :type create: bool
    :return: a path.
    :rtype: str
    """

    target_path = _user_resource(resource_type, path=path)

    if create:
        # should always be true.
        if target_path:
            # create path if not existing.
            if not _os.path.exists(target_path):
                try:
                    _os.makedirs(target_path)
                except Exception:
                    import traceback
                    traceback.print_exc()
                    target_path = ""
            elif not _os.path.isdir(target_path):
                print("Path {!r} found but isn't a directory!".format(target_path))
                target_path = ""

    return target_path


def extension_path_user(package, *, path="", create=False):
    """
    Return a user writable directory associated with an extension.

    .. note::

       This allows each extension to have its own user directory to store files.

       The location of the extension it self is not a suitable place to store files
       because it is cleared each upgrade and the users may not have write permissions
       to the repository (typically "System" repositories).

    :arg package: The ``__package__`` of the extension.
    :type package: str
    :arg path: Optional subdirectory.
    :type path: str
    :arg create: Treat the path as a directory and create it if its not existing.
    :type create: bool
    :return: a path.
    :rtype: str
    """
    from addon_utils import _extension_module_name_decompose

    # Handles own errors.
    repo_module, pkg_idname = _extension_module_name_decompose(package)

    target_path = _user_resource('EXTENSIONS')
    # Should always be true.
    if target_path:
        if path:
            target_path = _os.path.join(target_path, ".user", repo_module, pkg_idname, path)
        else:
            target_path = _os.path.join(target_path, ".user", repo_module, pkg_idname)
        if create:
            # create path if not existing.
            if not _os.path.exists(target_path):
                try:
                    _os.makedirs(target_path)
                except Exception:
                    import traceback
                    traceback.print_exc()
                    target_path = ""
            elif not _os.path.isdir(target_path):
                print("Path {!r} found but isn't a directory!".format(target_path))
                target_path = ""

    return target_path


def register_classes_factory(classes):
    """
    Utility function to create register and unregister functions
    which simply registers and unregisters a sequence of classes.
    """
    def register():
        for cls in classes:
            register_class(cls)

    def unregister():
        for cls in reversed(classes):
            unregister_class(cls)

    return register, unregister


def register_submodule_factory(module_name, submodule_names):
    """
    Utility function to create register and unregister functions
    which simply load submodules,
    calling their register & unregister functions.

    .. note::

       Modules are registered in the order given,
       unregistered in reverse order.

    :arg module_name: The module name, typically ``__name__``.
    :type module_name: str
    :arg submodule_names: List of submodule names to load and unload.
    :type submodule_names: list[str]
    :return: register and unregister functions.
    :rtype: tuple[Callable[[], None], Callable[[], None]]
    """

    module = None
    submodules = []

    def register():
        nonlocal module
        module = __import__(name=module_name, fromlist=submodule_names)
        submodules[:] = [(getattr(module, mod_name), mod_name) for mod_name in submodule_names]
        for mod, _mod_name in submodules:
            mod.register()

    def unregister():
        from sys import modules
        for mod, mod_name in reversed(submodules):
            mod.unregister()
            delattr(module, mod_name)
            del modules[mod.__name__]
        submodules.clear()

    return register, unregister


# -----------------------------------------------------------------------------
# Tool Registration


def register_tool(tool_cls, *, after=None, separator=False, group=False):
    """
    Register a tool in the toolbar.

    :arg tool_cls: A tool subclass.
    :type tool_cls: type[:class:`bpy.types.WorkSpaceTool`]
    :arg after: Optional identifiers this tool will be added after.
    :type after: Sequence[str] | set[str] | None
    :arg separator: When true, add a separator before this tool.
    :type separator: bool
    :arg group: When true, add a new nested group of tools.
    :type group: bool
    """
    space_type = tool_cls.bl_space_type
    context_mode = tool_cls.bl_context_mode

    from bl_ui.space_toolsystem_common import (
        ToolSelectPanelHelper,
        ToolDef,
    )

    cls = ToolSelectPanelHelper._tool_class_from_space_type(space_type)
    if cls is None:
        raise Exception("Space type {!r} has no toolbar".format(space_type))
    tools = cls._tools[context_mode]

    # First sanity check
    from bpy.types import WorkSpaceTool
    tools_id = {
        item.idname for item in ToolSelectPanelHelper._tools_flatten(tools)
        if item is not None
    }
    if not issubclass(tool_cls, WorkSpaceTool):
        raise Exception("Expected WorkSpaceTool subclass, not {!r}".format(type(tool_cls)))
    if tool_cls.bl_idname in tools_id:
        raise Exception("Tool {!r} already exists!".format(tool_cls.bl_idname))
    del tools_id, WorkSpaceTool

    # Convert the class into a ToolDef.
    def tool_from_class(tool_cls):
        # Convert class to tuple, store in the class for removal.
        tool_def = ToolDef.from_dict({
            "idname": tool_cls.bl_idname,
            "label": tool_cls.bl_label,
            "description": getattr(tool_cls, "bl_description", tool_cls.__doc__),
            "icon": getattr(tool_cls, "bl_icon", None),
            "cursor": getattr(tool_cls, "bl_cursor", None),
            "options": getattr(tool_cls, "bl_options", None),
            "widget": getattr(tool_cls, "bl_widget", None),
            "widget_properties": getattr(tool_cls, "bl_widget_properties", None),
            "keymap": getattr(tool_cls, "bl_keymap", None),
            "data_block": getattr(tool_cls, "bl_data_block", None),
            "operator": getattr(tool_cls, "bl_operator", None),
            "draw_settings": getattr(tool_cls, "draw_settings", None),
            "draw_cursor": getattr(tool_cls, "draw_cursor", None),
        })
        tool_cls._bl_tool = tool_def

        keymap_data = tool_def.keymap
        if keymap_data is not None and callable(keymap_data[0]):
            from bpy import context
            wm = context.window_manager
            keyconfigs = wm.keyconfigs
            kc_default = keyconfigs.default
            # Note that Blender's default tools use the default key-config for both.
            # We need to use the add-ons for 3rd party tools so reloading the key-map doesn't clear them.
            kc = keyconfigs.addon
            if kc is not None:
                if context_mode is None:
                    context_descr = "All"
                else:
                    context_descr = context_mode.replace("_", " ").title()
                cls._km_action_simple(kc_default, kc, context_descr, tool_def.label, keymap_data)
        return tool_def

    tool_converted = tool_from_class(tool_cls)

    if group:
        # Create a new group
        tool_converted = (tool_converted,)

    tool_def_insert = (
        (None, tool_converted) if separator else
        (tool_converted,)
    )

    def skip_to_end_of_group(seq, i):
        i_prev = i
        while i < len(seq) and seq[i] is not None:
            i_prev = i
            i += 1
        return i_prev

    changed = False
    if after is not None:
        for i, item in enumerate(tools):
            if item is None:
                pass
            elif isinstance(item, ToolDef):
                if item.idname in after:
                    i = skip_to_end_of_group(item, i)
                    tools[i + 1:i + 1] = tool_def_insert
                    changed = True
                    break
            elif isinstance(item, tuple):
                for j, sub_item in enumerate(item, 1):
                    if isinstance(sub_item, ToolDef):
                        if sub_item.idname in after:
                            if group:
                                # Can't add a group within a group,
                                # add a new group after this group.
                                i = skip_to_end_of_group(tools, i)
                                tools[i + 1:i + 1] = tool_def_insert
                            else:
                                j = skip_to_end_of_group(item, j)
                                item = item[:j + 1] + tool_def_insert + item[j + 1:]
                                tools[i] = item
                            changed = True
                            break
                if changed:
                    break

        if not changed:
            print("bpy.utils.register_tool: could not find 'after'", after)
    if not changed:
        tools.extend(tool_def_insert)


def unregister_tool(tool_cls):
    space_type = tool_cls.bl_space_type
    context_mode = tool_cls.bl_context_mode

    from bl_ui.space_toolsystem_common import ToolSelectPanelHelper
    cls = ToolSelectPanelHelper._tool_class_from_space_type(space_type)
    if cls is None:
        raise Exception("Space type {!r} has no toolbar".format(space_type))
    tools = cls._tools[context_mode]

    tool_def = tool_cls._bl_tool
    try:
        i = tools.index(tool_def)
    except ValueError:
        i = -1

    def tool_list_clean(tool_list):
        # Trim separators.
        while tool_list and tool_list[-1] is None:
            del tool_list[-1]
        while tool_list and tool_list[0] is None:
            del tool_list[0]
        # Remove duplicate separators.
        for i in range(len(tool_list) - 1, -1, -1):
            is_none = tool_list[i] is None
            if is_none and prev_is_none:
                del tool_list[i]
            prev_is_none = is_none

    changed = False
    if i != -1:
        del tools[i]
        tool_list_clean(tools)
        changed = True

    if not changed:
        for i, item in enumerate(tools):
            if isinstance(item, tuple):
                try:
                    j = item.index(tool_def)
                except ValueError:
                    j = -1

                if j != -1:
                    item_clean = list(item)
                    del item_clean[j]
                    tool_list_clean(item_clean)
                    if item_clean:
                        tools[i] = tuple(item_clean)
                    else:
                        del tools[i]
                        tool_list_clean(tools)
                    del item_clean

                    # tuple(sub_item for sub_item in items if sub_item is not tool_def)
                    changed = True
                    break

    if not changed:
        raise Exception("Unable to remove {!r}".format(tool_cls))
    del tool_cls._bl_tool

    keymap_data = tool_def.keymap
    if keymap_data is not None:
        from bpy import context
        wm = context.window_manager
        keyconfigs = wm.keyconfigs
        for kc in (keyconfigs.default, keyconfigs.addon):
            if kc is None:
                continue
            km = kc.keymaps.get(keymap_data[0])
            if km is None:
                print("Warning keymap {!r} not found in {!r}!".format(keymap_data[0], kc.name))
            else:
                kc.keymaps.remove(km)


# -----------------------------------------------------------------------------
# Manual lookups, each function has to return a basepath and a sequence
# of...

# we start with the built-in default mapping
def _blender_default_map():
    # NOTE(@ideasman42): Avoid importing this as there is no need to keep the lookup table in memory.
    # As this runs when the user accesses the "Online Manual", the overhead loading the file is acceptable.
    # In my tests it's under 1/100th of a second loading from a `pyc`.
    ref_mod = execfile(_os.path.join(_script_base_dir, "modules", "_rna_manual_reference.py"))
    return (ref_mod.url_manual_prefix, ref_mod.url_manual_mapping)


# hooks for doc lookups
_manual_map = [_blender_default_map]


def register_manual_map(manual_hook):
    _manual_map.append(manual_hook)


def unregister_manual_map(manual_hook):
    _manual_map.remove(manual_hook)


def manual_map():
    # reverse so default is called last
    for cb in reversed(_manual_map):
        try:
            prefix, url_manual_mapping = cb()
        except Exception:
            print("Error calling {!r}".format(cb))
            import traceback
            traceback.print_exc()
            continue

        yield prefix, url_manual_mapping


# Languages which are supported by the user manual (commented when there is no translation).
_manual_language_codes = {
    # "ab": "ab",  #Abkhaz
    # "am_ET": "am",  # Amharic
    "ar_EG": "ar",  # Arabic
    # "be": "be",  # Belarusian
    # "bg_BG": "bg",  # Bulgarian
    "ca_AD": "ca",  # Catalan
    # "cs_CZ": "cz",  # Czech
    # "da": "da",  # Danish
    "de_DE": "de",  # German
    "el_GR": "el",  # Greek
    # "eo": "eo",  # Esperanto
    "es": "es",  # Spanish
    # "et_EE": "et",  # Estonian
    # "eu_EU": "eu",  # Basque
    # "fa_IR": "fa",  # Persian
    "fi_FI": "fi",  # Finnish
    "fr_FR": "fr",  # French
    # "ha": "ha",  # Hausa
    # "he_IL": "he",  # Hebrew
    # "hi_IN": "hi",  # Hindi
    # "hr_HR": "hr",  # Croatian
    # "hu_HU": "hu",  # Hungarian
    "id_ID": "id",  # Indonesian
    "it_IT": "it",  # Italian
    "ja_JP": "ja",  # Japanese
    # "ka": "ka",  # Georgian
    # "kk_KZ": "kk",  # kazakh
    # "km": "km",  # Khmer
    "ko_KR": "ko",  # Korean
    # "ky_KG": "ky",  # Kyrgyz
    # "nb": "nb",  # Norwegian
    # "ne_NP": "ne",  # Nepali
    "nl_NL": "nl",  # Dutch
    # "pl_PL": "pl",  # Polish
    "pt_PT": "pt",  # Portuguese
    # Portuguese - Brazil, for until we have a pt_BR version.
    "pt_BR": "pt",
    # "ro_RO": "ro",  # Romanian
    "ru_RU": "ru",  # Russian
    "sk_SK": "sk",  # Slovak
    # "sl": "sl",  # Slovenian
    "sr_RS": "sr",  # Serbian
    # "sv_SE": "sv",  # Swedish
    # "sw": "sw",  # Swahili
    # "ta": "ta",  # Tamil
    "th_TH": "th",  # Thai
    # "tr_TR": "tr",  # Turkish
    "uk_UA": "uk",  # Ukrainian
    # "uz_UZ": "uz",  # Uzbek
    "vi_VN": "vi",  # Vietnamese
    "zh_HANS": "zh-hans",  # Simplified Chinese
    "zh_HANT": "zh-hant",  # Traditional Chinese
}


def manual_language_code(default="en"):
    """
    :return:
       The language code used for user manual URL component based on the current language user-preference,
       falling back to the ``default`` when unavailable.
    :rtype: str
    """
    language = _bpy.context.preferences.view.language
    if language == 'DEFAULT':
        language = _os.getenv("LANG", "").split(".")[0]
    return _manual_language_codes.get(language, default)


# Build an RNA path from struct/property/enum names.
def make_rna_paths(struct_name, prop_name, enum_name):
    """
    Create RNA "paths" from given names.

    :arg struct_name: Name of a RNA struct (like e.g. "Scene").
    :type struct_name: str
    :arg prop_name: Name of a RNA struct's property.
    :type prop_name: str
    :arg enum_name: Name of a RNA enum identifier.
    :type enum_name: str
    :return: A triple of three "RNA paths"
       (most_complete_path, "struct.prop", "struct.prop:'enum'").
       If no enum_name is given, the third element will always be void.
    :rtype: tuple[str, str, str]
    """
    src = src_rna = src_enum = ""
    if struct_name:
        if prop_name:
            src = src_rna = ".".join((struct_name, prop_name))
            if enum_name:
                src = src_enum = "{:s}:'{:s}'".format(src_rna, enum_name)
        else:
            src = src_rna = struct_name
    return src, src_rna, src_enum


def expose_bundled_modules():
    """
    For Blender as a Python module, add bundled VFX library python bindings
    to ``sys.path``. These may be used instead of dedicated packages, to ensure
    the libraries are compatible with Blender.
    """
    # For Blender executable there is nothing to do, already exposed.
    if not _bpy.app.module:
        return
    # System installations do not bundle additional modules,
    # these are expected to be installed on the system too.
    if not _bpy.app.portable:
        return

    version_dir = _os.path.normpath(_os.path.join(_bpy.__file__, "..", "..", "..", ".."))
    packages_dir = _os.path.join(version_dir, "python", "lib")
    if _sys.platform != "win32":
        packages_dir = _os.path.join(packages_dir, "python{:d}.{:d}".format(*_sys.version_info[:2]))
    packages_dir = _os.path.join(packages_dir, "site-packages")

    if packages_dir not in _sys.path:
        _sys.path.insert(0, packages_dir)
