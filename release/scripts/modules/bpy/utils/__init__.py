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
not associated with blenders internal data.
"""

__all__ = (
    "blend_paths",
    "escape_identifier",
    "keyconfig_init",
    "keyconfig_set",
    "load_scripts",
    "modules_from_path",
    "preset_find",
    "preset_paths",
    "refresh_script_paths",
    "app_template_paths",
    "register_class",
    "register_manual_map",
    "unregister_manual_map",
    "register_classes_factory",
    "register_submodule_factory",
    "register_tool",
    "make_rna_paths",
    "manual_map",
    "previews",
    "resource_path",
    "script_path_user",
    "script_path_pref",
    "script_paths",
    "smpte_from_frame",
    "smpte_from_seconds",
    "units",
    "unregister_class",
    "unregister_tool",
    "user_resource",
    "execfile",
)

from _bpy import (
    _utils_units as units,
    blend_paths,
    escape_identifier,
    register_class,
    resource_path,
    script_paths as _bpy_script_paths,
    unregister_class,
    user_resource as _user_resource,
)

import bpy as _bpy
import os as _os
import sys as _sys

import addon_utils as _addon_utils

_preferences = _bpy.context.preferences
_script_module_dirs = "startup", "modules"
_is_factory_startup = _bpy.app.factory_startup


def execfile(filepath, mod=None):
    # module name isn't used or added to 'sys.modules'.
    # passing in 'mod' allows re-execution without having to reload.

    import importlib.util
    mod_spec = importlib.util.spec_from_file_location("__main__", filepath)
    if mod is None:
        mod = importlib.util.module_from_spec(mod_spec)
    mod_spec.loader.exec_module(mod)
    return mod


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

    for mod_name, _mod_path in _bpy.path.module_names(path):
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
        for module_name in [ext.module for ext in _preferences.addons]:
            _addon_utils.disable(module_name)

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
        import importlib
        # reloading this causes internal errors
        # because the classes from this module are stored internally
        # possibly to refresh internal references too but for now, best not to.
        if mod == _bpy_types:
            return mod

        try:
            return importlib.reload(mod)
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

        del _global_loaded_modules[:]

    from bpy_restrict_state import RestrictBlend

    with RestrictBlend():
        for base_path in script_paths(use_user=use_user):
            for path_subdir in _script_module_dirs:
                path = _os.path.join(base_path, path_subdir)
                if _os.path.isdir(path):
                    _sys_path_ensure(path)

                    # only add this to sys.modules, don't run
                    if path_subdir == "modules":
                        continue

                    for mod in modules_from_path(path, loaded_modules):
                        test_register(mod)

    # load template (if set)
    if any(_bpy.utils.app_template_paths()):
        import bl_app_template_utils
        bl_app_template_utils.reset(reload_scripts=reload_scripts)
        del bl_app_template_utils

    # deal with addons separately
    _initialize = getattr(_addon_utils, "_initialize", None)
    if _initialize is not None:
        # first time, use fast-path
        _initialize()
        del _addon_utils._initialize
    else:
        _addon_utils.reset_all(reload_scripts=reload_scripts)
    del _initialize

    if reload_scripts:
        import gc
        print("gc.collect() -> %d" % gc.collect())

    if use_time:
        print("Python Script Load Time %.4f" % (time.time() - t_main))

    if use_class_register_check:
        for cls in _bpy.types.bpy_struct.__subclasses__():
            if getattr(cls, "is_registered", False):
                for subcls in cls.__subclasses__():
                    if not subcls.is_registered:
                        print(
                            "Warning, unregistered class: %s(%s)" %
                            (subcls.__name__, cls.__name__)
                        )


# base scripts
_scripts = (
    _os.path.dirname(_os.path.dirname(_os.path.dirname(__file__))),
)


def script_path_user():
    """returns the env var and falls back to home dir or None"""
    path = _user_resource('SCRIPTS')
    return _os.path.normpath(path) if path else None


def script_path_pref():
    """returns the user preference or None"""
    path = _preferences.filepaths.script_directory
    return _os.path.normpath(path) if path else None


def script_paths(subdir=None, user_pref=True, check_all=False, use_user=True):
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

    # Only paths Blender uses.
    #
    # Needed this is needed even when 'check_all' is enabled,
    # so the 'BLENDER_SYSTEM_SCRIPTS' environment variable will be used.
    base_paths = _bpy_script_paths()

    # Defined to be (system, user) so we can skip the second if needed.
    if not use_user:
        base_paths = base_paths[:1]

    if check_all:
        # All possible paths, no duplicates, keep order.
        if use_user:
            test_paths = ('LOCAL', 'USER', 'SYSTEM')
        else:
            test_paths = ('LOCAL', 'SYSTEM')

        base_paths = (
            *(path for path in (
                _os.path.join(resource_path(res), "scripts")
                for res in test_paths) if path not in base_paths),
            *base_paths,
        )

    test_paths = (
        *base_paths,
        *((script_path_user(),) if use_user else ()),
        *((script_path_pref(),) if user_pref else ()),
    )

    for path in test_paths:
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


def app_template_paths(subdir=None):
    """
    Returns valid application template paths.

    :arg subdir: Optional subdir.
    :type subdir: string
    :return: app template paths.
    :rtype: generator
    """
    # Note: keep in sync with: Blender's BKE_appdir_app_template_any

    subdir_tuple = (subdir,) if subdir is not None else ()

    # Avoid adding 'bl_app_templates_system' twice.
    # Either we have a portable build or an installed system build.
    for resource_type, module_name in (
            ('USER', "bl_app_templates_user"),
            ('LOCAL', "bl_app_templates_system"),
            ('SYSTEM', "bl_app_templates_system"),
    ):
        path = resource_path(resource_type)
        if path:
            path = _os.path.join(
                *(path, "scripts", "startup", module_name, *subdir_tuple))
            if _os.path.isdir(path):
                yield path
                # Only load LOCAL or SYSTEM (never both).
                if resource_type == 'LOCAL':
                    break


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
    for path in _addon_utils.paths():
        directory = _os.path.join(path, "presets", subdir)
        if _os.path.isdir(directory):
            dirs.append(directory)

    return dirs


def smpte_from_seconds(time, fps=None):
    """
    Returns an SMPTE formatted string from the *time*:
    ``HH:MM:SS:FF``.

    If the *fps* is not given the current scene is used.

    :arg time: time in seconds.
    :type time: int, float or ``datetime.timedelta``.
    :return: the frame string.
    :rtype: string
    """

    return smpte_from_frame(time_to_frame(time, fps=fps), fps)


def smpte_from_frame(frame, fps=None, fps_base=None):
    """
    Returns an SMPTE formatted string from the *frame*:
    ``HH:MM:SS:FF``.

    If *fps* and *fps_base* are not given the current scene is used.

    :arg frame: frame number.
    :type frame: int or float.
    :return: the frame string.
    :rtype: string
    """

    if fps is None:
        fps = _bpy.context.scene.render.fps

    if fps_base is None:
        fps_base = _bpy.context.scene.render.fps_base

    sign = "-" if frame < 0 else ""
    frame = abs(frame * fps_base)

    return (
        "%s%02d:%02d:%02d:%02d" % (
            sign,
            int(frame / (3600 * fps)),          # HH
            int((frame / (60 * fps)) % 60),     # MM
            int((frame / fps) % 60),            # SS
            int(frame % fps),                   # FF
        ))


def time_from_frame(frame, fps=None, fps_base=None):
    """
    Returns the time from a frame number .

    If *fps* and *fps_base* are not given the current scene is used.

    :arg frame: number.
    :type frame: int or float.
    :return: the time in seconds.
    :rtype: datetime.timedelta
    """

    if fps is None:
        fps = _bpy.context.scene.render.fps

    if fps_base is None:
        fps_base = _bpy.context.scene.render.fps_base

    from datetime import timedelta

    return timedelta(0, (frame * fps_base) / fps)


def time_to_frame(time, fps=None, fps_base=None):
    """
    Returns a float frame number from a time given in seconds or
    as a datetime.timedelta object.

    If *fps* and *fps_base* are not given the current scene is used.

    :arg time: time in seconds.
    :type time: number or a ``datetime.timedelta`` object
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


def keyconfig_init():
    # Key configuration initialization and refresh, called from the Blender
    # window manager on startup and refresh.
    active_config = _preferences.keymap.active_keyconfig

    # Load the default key configuration.
    default_filepath = preset_find("blender", "keyconfig")
    keyconfig_set(default_filepath)

    # Set the active key configuration if different
    filepath = preset_find(active_config, "keyconfig")

    if filepath and filepath != default_filepath:
        keyconfig_set(filepath)


def keyconfig_set(filepath, report=None):
    from os.path import basename, splitext

    if _bpy.app.debug_python:
        print("loading preset:", filepath)

    keyconfigs = _bpy.context.window_manager.keyconfigs

    try:
        error_msg = ""
        execfile(filepath)
    except:
        import traceback
        error_msg = traceback.format_exc()

    name = splitext(basename(filepath))[0]
    kc_new = keyconfigs.get(name)

    if error_msg:
        if report is not None:
            report({'ERROR'}, error_msg)
        print(error_msg)
        if kc_new is not None:
            keyconfigs.remove(kc_new)
        return False

    # Get name, exception for default keymap to keep backwards compatibility.
    if kc_new is None:
        if report is not None:
            report({'ERROR'}, "Failed to load keymap %r" % filepath)
        return False
    else:
        keyconfigs.active = kc_new
        return True


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


def register_classes_factory(classes):
    """
    Utility function to create register and unregister functions
    which simply registers and unregisters a sequence of classes.
    """
    def register():
        from bpy.utils import register_class
        for cls in classes:
            register_class(cls)

    def unregister():
        from bpy.utils import unregister_class
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
    :type module_name: string
    :arg submodule_names: List of submodule names to load and unload.
    :type submodule_names: list of strings
    :return: register and unregister functions.
    :rtype: tuple pair of functions
    """

    module = None
    submodules = []

    def register():
        nonlocal module
        module = __import__(name=module_name, fromlist=submodule_names)
        submodules[:] = [getattr(module, name) for name in submodule_names]
        for mod in submodules:
            mod.register()

    def unregister():
        from sys import modules
        for mod in reversed(submodules):
            mod.unregister()
            name = mod.__name__
            delattr(module, name.partition(".")[2])
            del modules[name]
        submodules.clear()

    return register, unregister


# -----------------------------------------------------------------------------
# Tool Registration


def register_tool(tool_cls, *, after=None, separator=False, group=False):
    """
    Register a tool in the toolbar.

    :arg tool: A tool subclass.
    :type tool: :class:`bpy.types.WorkSpaceTool` subclass.
    :arg space_type: Space type identifier.
    :type space_type: string
    :arg after: Optional identifiers this tool will be added after.
    :type after: collection of strings or None.
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
        raise Exception(f"Space type {space_type!r} has no toolbar")
    tools = cls._tools[context_mode]

    # First sanity check
    from bpy.types import WorkSpaceTool
    tools_id = {
        item.idname for item in ToolSelectPanelHelper._tools_flatten(tools)
        if item is not None
    }
    if not issubclass(tool_cls, WorkSpaceTool):
        raise Exception(f"Expected WorkSpaceTool subclass, not {type(tool_cls)!r}")
    if tool_cls.bl_idname in tools_id:
        raise Exception(f"Tool {tool_cls.bl_idname!r} already exists!")
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
            "widget": getattr(tool_cls, "bl_widget", None),
            "keymap": getattr(tool_cls, "bl_keymap", None),
            "data_block": getattr(tool_cls, "bl_data_block", None),
            "operator": getattr(tool_cls, "bl_operator", None),
            "draw_settings": getattr(tool_cls, "draw_settings", None),
            "draw_cursor": getattr(tool_cls, "draw_cursor", None),
        })
        tool_cls._bl_tool = tool_def

        keymap_data = tool_def.keymap
        if keymap_data is not None:
            if context_mode is None:
                context_descr = "All"
            else:
                context_descr = context_mode.replace("_", " ").title()
            from bpy import context
            wm = context.window_manager
            kc = wm.keyconfigs.default
            if callable(keymap_data[0]):
                cls._km_action_simple(kc, context_descr, tool_def.label, keymap_data)
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
        raise Exception(f"Space type {space_type!r} has no toolbar")
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
        raise Exception(f"Unable to remove {tool_cls!r}")
    del tool_cls._bl_tool

    keymap_data = tool_def.keymap
    if keymap_data is not None:
        from bpy import context
        wm = context.window_manager
        kc = wm.keyconfigs.default
        km = kc.keymaps.get(keymap_data[0])
        if km is None:
            print("Warning keymap {keymap_data[0]!r} not found!")
        else:
            kc.keymaps.remove(km)


# -----------------------------------------------------------------------------
# Manual lookups, each function has to return a basepath and a sequence
# of...

# we start with the built-in default mapping
def _blender_default_map():
    import rna_manual_reference as ref_mod
    ret = (ref_mod.url_manual_prefix, ref_mod.url_manual_mapping)
    # avoid storing in memory
    del _sys.modules["rna_manual_reference"]
    return ret


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
        except:
            print("Error calling %r" % cb)
            import traceback
            traceback.print_exc()
            continue

        yield prefix, url_manual_mapping


# Build an RNA path from struct/property/enum names.
def make_rna_paths(struct_name, prop_name, enum_name):
    """
    Create RNA "paths" from given names.

    :arg struct_name: Name of a RNA struct (like e.g. "Scene").
    :type struct_name: string
    :arg prop_name: Name of a RNA struct's property.
    :type prop_name: string
    :arg enum_name: Name of a RNA enum identifier.
    :type enum_name: string
    :return: A triple of three "RNA paths"
       (most_complete_path, "struct.prop", "struct.prop:'enum'").
       If no enum_name is given, the third element will always be void.
    :rtype: tuple of strings
    """
    src = src_rna = src_enum = ""
    if struct_name:
        if prop_name:
            src = src_rna = ".".join((struct_name, prop_name))
            if enum_name:
                src = src_enum = "%s:'%s'" % (src_rna, enum_name)
        else:
            src = src_rna = struct_name
    return src, src_rna, src_enum
