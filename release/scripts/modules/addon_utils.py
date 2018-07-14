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

__all__ = (
    "paths",
    "modules",
    "check",
    "enable",
    "disable",
    "disable_all",
    "reset_all",
    "module_bl_info",
)

import bpy as _bpy
_user_preferences = _bpy.context.user_preferences

error_encoding = False
# (name, file, path)
error_duplicates = []
addons_fake_modules = {}


# called only once at startup, avoids calling 'reset_all', correct but slower.
def _initialize():
    path_list = paths()
    for path in path_list:
        _bpy.utils._sys_path_ensure(path)
    for addon in _user_preferences.addons:
        enable(addon.module)


def paths():
    # RELEASE SCRIPTS: official scripts distributed in Blender releases
    addon_paths = _bpy.utils.script_paths("addons")

    # CONTRIB SCRIPTS: good for testing but not official scripts yet
    # if folder addons_contrib/ exists, scripts in there will be loaded too
    addon_paths += _bpy.utils.script_paths("addons_contrib")

    return addon_paths


def modules_refresh(module_cache=addons_fake_modules):
    global error_encoding
    import os

    error_encoding = False
    error_duplicates.clear()

    path_list = paths()

    # fake module importing
    def fake_module(mod_name, mod_path, speedy=True, force_support=None):
        global error_encoding

        if _bpy.app.debug_python:
            print("fake_module", mod_path, mod_name)
        import ast
        ModuleType = type(ast)
        try:
            file_mod = open(mod_path, "r", encoding='UTF-8')
        except OSError as ex:
            print("Error opening file:", mod_path, ex)
            return None

        with file_mod:
            if speedy:
                lines = []
                line_iter = iter(file_mod)
                l = ""
                while not l.startswith("bl_info"):
                    try:
                        l = line_iter.readline()
                    except UnicodeDecodeError as ex:
                        if not error_encoding:
                            error_encoding = True
                            print("Error reading file as UTF-8:", mod_path, ex)
                        return None

                    if len(l) == 0:
                        break
                while l.rstrip():
                    lines.append(l)
                    try:
                        l = line_iter.readline()
                    except UnicodeDecodeError as ex:
                        if not error_encoding:
                            error_encoding = True
                            print("Error reading file as UTF-8:", mod_path, ex)
                        return None

                data = "".join(lines)

            else:
                data = file_mod.read()
        del file_mod

        try:
            ast_data = ast.parse(data, filename=mod_path)
        except:
            print("Syntax error 'ast.parse' can't read:", repr(mod_path))
            import traceback
            traceback.print_exc()
            ast_data = None

        body_info = None

        if ast_data:
            for body in ast_data.body:
                if body.__class__ == ast.Assign:
                    if len(body.targets) == 1:
                        if getattr(body.targets[0], "id", "") == "bl_info":
                            body_info = body
                            break

        if body_info:
            try:
                mod = ModuleType(mod_name)
                mod.bl_info = ast.literal_eval(body.value)
                mod.__file__ = mod_path
                mod.__time__ = os.path.getmtime(mod_path)
            except:
                print("AST error parsing bl_info for:", mod_name)
                import traceback
                traceback.print_exc()
                raise

            if force_support is not None:
                mod.bl_info["support"] = force_support

            return mod
        else:
            print(
                "fake_module: addon missing 'bl_info' "
                "gives bad performance!:",
                repr(mod_path),
            )
            return None

    modules_stale = set(module_cache.keys())

    for path in path_list:

        # force all contrib addons to be 'TESTING'
        if path.endswith(("addons_contrib", )):
            force_support = 'TESTING'
        else:
            force_support = None

        for mod_name, mod_path in _bpy.path.module_names(path):
            modules_stale.discard(mod_name)
            mod = module_cache.get(mod_name)
            if mod:
                if mod.__file__ != mod_path:
                    print(
                        "multiple addons with the same name:\n"
                        "  " f"{mod.__file__!r}" "\n"
                        "  " f"{mod_path!r}"
                    )
                    error_duplicates.append((mod.bl_info["name"], mod.__file__, mod_path))

                elif mod.__time__ != os.path.getmtime(mod_path):
                    print(
                        "reloading addon:",
                        mod_name,
                        mod.__time__,
                        os.path.getmtime(mod_path),
                        repr(mod_path),
                    )
                    del module_cache[mod_name]
                    mod = None

            if mod is None:
                mod = fake_module(
                    mod_name,
                    mod_path,
                    force_support=force_support,
                )
                if mod:
                    module_cache[mod_name] = mod

    # just in case we get stale modules, not likely
    for mod_stale in modules_stale:
        del module_cache[mod_stale]
    del modules_stale


def modules(module_cache=addons_fake_modules, *, refresh=True):
    if refresh or ((module_cache is addons_fake_modules) and modules._is_first):
        modules_refresh(module_cache)
        modules._is_first = False

    mod_list = list(module_cache.values())
    mod_list.sort(
        key=lambda mod: (
            mod.bl_info["category"],
            mod.bl_info["name"],
        )
    )
    return mod_list


modules._is_first = True


def check(module_name):
    """
    Returns the loaded state of the addon.

    :arg module_name: The name of the addon and module.
    :type module_name: string
    :return: (loaded_default, loaded_state)
    :rtype: tuple of booleans
    """
    import sys
    loaded_default = module_name in _user_preferences.addons

    mod = sys.modules.get(module_name)
    loaded_state = (
        (mod is not None) and
        getattr(mod, "__addon_enabled__", Ellipsis)
    )

    if loaded_state is Ellipsis:
        print(
            "Warning: addon-module " f"{module_name:s}" " found module "
            "but without '__addon_enabled__' field, "
            "possible name collision from file:",
            repr(getattr(mod, "__file__", "<unknown>")),
        )

        loaded_state = False

    if mod and getattr(mod, "__addon_persistent__", False):
        loaded_default = True

    return loaded_default, loaded_state

# utility functions


def _addon_ensure(module_name):
    addons = _user_preferences.addons
    addon = addons.get(module_name)
    if not addon:
        addon = addons.new()
        addon.module = module_name


def _addon_remove(module_name):
    addons = _user_preferences.addons

    while module_name in addons:
        addon = addons.get(module_name)
        if addon:
            addons.remove(addon)


def enable(module_name, *, default_set=False, persistent=False, handle_error=None):
    """
    Enables an addon by name.

    :arg module_name: the name of the addon and module.
    :type module_name: string
    :arg default_set: Set the user-preference.
    :type default_set: bool
    :arg persistent: Ensure the addon is enabled for the entire session (after loading new files).
    :type persistent: bool
    :arg handle_error: Called in the case of an error, taking an exception argument.
    :type handle_error: function
    :return: the loaded module or None on failure.
    :rtype: module
    """

    import os
    import sys
    from bpy_restrict_state import RestrictBlend

    if handle_error is None:
        def handle_error(ex):
            import traceback
            traceback.print_exc()

    # reload if the mtime changes
    mod = sys.modules.get(module_name)
    # chances of the file _not_ existing are low, but it could be removed
    if mod and os.path.exists(mod.__file__):

        if getattr(mod, "__addon_enabled__", False):
            # This is an unlikely situation,
            # re-register if the module is enabled.
            # Note: the UI doesn't allow this to happen,
            # in most cases the caller should 'check()' first.
            try:
                mod.unregister()
            except Exception as ex:
                print(
                    "Exception in module unregister():",
                    repr(getattr(mod, "__file__", module_name)),
                )
                handle_error(ex)
                return None

        mod.__addon_enabled__ = False
        mtime_orig = getattr(mod, "__time__", 0)
        mtime_new = os.path.getmtime(mod.__file__)
        if mtime_orig != mtime_new:
            import importlib
            print("module changed on disk:", repr(mod.__file__), "reloading...")

            try:
                importlib.reload(mod)
            except Exception as ex:
                handle_error(ex)
                del sys.modules[module_name]
                return None
            mod.__addon_enabled__ = False

    # add the addon first it may want to initialize its own preferences.
    # must remove on fail through.
    if default_set:
        _addon_ensure(module_name)

    # Split registering up into 3 steps so we can undo
    # if it fails par way through.

    # disable the context, using the context at all is
    # really bad while loading an addon, don't do it!
    with RestrictBlend():

        # 1) try import
        try:
            mod = __import__(module_name)
            mod.__time__ = os.path.getmtime(mod.__file__)
            mod.__addon_enabled__ = False
        except Exception as ex:
            # if the addon doesn't exist, dont print full traceback
            if type(ex) is ImportError and ex.name == module_name:
                print("addon not found:", repr(module_name))
            else:
                handle_error(ex)

            if default_set:
                _addon_remove(module_name)
            return None

        # 1.1) fail when add-on is too old
        # This is a temporary 2.8x migration check, so we can manage addons that are supported.

        # Silent default, we know these need updating.
        if module_name in {
			"io_anim_bvh",
			"io_mesh_ply",
			"io_mesh_stl",
			"io_mesh_uv_layout",
			"io_scene_3ds",
			"io_scene_fbx",
			"io_scene_obj",
			"io_scene_x3d",
        }:
            return None

        try:
            if mod.bl_info.get("blender", (0, 0, 0)) < (2, 80, 0):
                raise Exception(f"Add-on '{module_name:s}' has not been upgraded to 2.8, ignoring")
        except Exception as ex:
            handle_error(ex)
            return None

        # 2) try register collected modules
        # removed, addons need to handle own registration now.

        use_owner = mod.bl_info.get("use_owner", True)
        if use_owner:
            from _bpy import _bl_owner_id_get, _bl_owner_id_set
            owner_id_prev = _bl_owner_id_get()
            _bl_owner_id_set(module_name)

        # 3) try run the modules register function
        try:
            mod.register()
        except Exception as ex:
            print(
                "Exception in module register():",
                getattr(mod, "__file__", module_name),
            )
            handle_error(ex)
            del sys.modules[module_name]
            if default_set:
                _addon_remove(module_name)
            return None
        finally:
            if use_owner:
                _bl_owner_id_set(owner_id_prev)

    # * OK loaded successfully! *
    mod.__addon_enabled__ = True
    mod.__addon_persistent__ = persistent

    if _bpy.app.debug_python:
        print("\taddon_utils.enable", mod.__name__)

    return mod


def disable(module_name, *, default_set=False, handle_error=None):
    """
    Disables an addon by name.

    :arg module_name: The name of the addon and module.
    :type module_name: string
    :arg default_set: Set the user-preference.
    :type default_set: bool
    :arg handle_error: Called in the case of an error, taking an exception argument.
    :type handle_error: function
    """
    import sys

    if handle_error is None:
        def handle_error(ex):
            import traceback
            traceback.print_exc()

    mod = sys.modules.get(module_name)

    # possible this addon is from a previous session and didn't load a
    # module this time. So even if the module is not found, still disable
    # the addon in the user prefs.
    if mod and getattr(mod, "__addon_enabled__", False) is not False:
        mod.__addon_enabled__ = False
        mod.__addon_persistent = False

        try:
            mod.unregister()
        except Exception as ex:
            mod_path = getattr(mod, "__file__", module_name)
            print("Exception in module unregister():", repr(mod_path))
            del mod_path
            handle_error(ex)
    else:
        print(
            "addon_utils.disable: " f"{module_name:s}" " not",
            ("disabled" if mod is None else "loaded")
        )

    # could be in more than once, unlikely but better do this just in case.
    if default_set:
        _addon_remove(module_name)

    if _bpy.app.debug_python:
        print("\taddon_utils.disable", module_name)


def reset_all(*, reload_scripts=False):
    """
    Sets the addon state based on the user preferences.
    """
    import sys

    # initializes addons_fake_modules
    modules_refresh()

    # RELEASE SCRIPTS: official scripts distributed in Blender releases
    paths_list = paths()

    for path in paths_list:
        _bpy.utils._sys_path_ensure(path)
        for mod_name, mod_path in _bpy.path.module_names(path):
            is_enabled, is_loaded = check(mod_name)

            # first check if reload is needed before changing state.
            if reload_scripts:
                import importlib
                mod = sys.modules.get(mod_name)
                if mod:
                    importlib.reload(mod)

            if is_enabled == is_loaded:
                pass
            elif is_enabled:
                enable(mod_name)
            elif is_loaded:
                print("\taddon_utils.reset_all unloading", mod_name)
                disable(mod_name)


def disable_all():
    import sys
    for mod_name, mod in sys.modules.items():
        if getattr(mod, "__addon_enabled__", False):
            disable(mod_name)


def module_bl_info(mod, info_basis=None):
    if info_basis is None:
        info_basis = {
            "name": "",
            "author": "",
            "version": (),
            "blender": (),
            "location": "",
            "description": "",
            "wiki_url": "",
            "support": 'COMMUNITY',
            "category": "",
            "warning": "",
            "show_expanded": False,
            "use_owner": True,
        }

    addon_info = getattr(mod, "bl_info", {})

    # avoid re-initializing
    if "_init" in addon_info:
        return addon_info

    if not addon_info:
        mod.bl_info = addon_info

    for key, value in info_basis.items():
        addon_info.setdefault(key, value)

    if not addon_info["name"]:
        addon_info["name"] = mod.__name__

    # Temporary auto-magic, don't use_owner for import export menus.
    if mod.bl_info["category"] == "Import-Export":
        mod.bl_info["use_owner"] = False

    addon_info["_init"] = None
    return addon_info
