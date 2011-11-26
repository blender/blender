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
    "reset_all",
    "module_bl_info",
    )

import bpy as _bpy


error_duplicates = False
error_encoding = False
addons_fake_modules = {}


def paths():
    # RELEASE SCRIPTS: official scripts distributed in Blender releases
    paths = _bpy.utils.script_paths("addons")

    # CONTRIB SCRIPTS: good for testing but not official scripts yet
    # if folder addons_contrib/ exists, scripts in there will be loaded too
    paths += _bpy.utils.script_paths("addons_contrib")

    # EXTERN SCRIPTS: external projects scripts
    # if folder addons_extern/ exists, scripts in there will be loaded too
    paths += _bpy.utils.script_paths("addons_extern")

    return paths


def modules(module_cache):
    global error_duplicates
    global error_encoding
    import os

    error_duplicates = False
    error_encoding = False

    path_list = paths()

    # fake module importing
    def fake_module(mod_name, mod_path, speedy=True, force_support=None):
        global error_encoding

        if _bpy.app.debug:
            print("fake_module", mod_path, mod_name)
        import ast
        ModuleType = type(ast)
        file_mod = open(mod_path, "r", encoding='UTF-8')
        if speedy:
            lines = []
            line_iter = iter(file_mod)
            l = ""
            while not l.startswith("bl_info"):
                try:
                    l = line_iter.readline()
                except UnicodeDecodeError as e:
                    if not error_encoding:
                        error_encoding = True
                        print("Error reading file as UTF-8:", mod_path, e)
                    file_mod.close()
                    return None

                if len(l) == 0:
                    break
            while l.rstrip():
                lines.append(l)
                try:
                    l = line_iter.readline()
                except UnicodeDecodeError as e:
                    if not error_encoding:
                        error_encoding = True
                        print("Error reading file as UTF-8:", mod_path, e)
                    file_mod.close()
                    return None

            data = "".join(lines)

        else:
            data = file_mod.read()

        file_mod.close()

        try:
            ast_data = ast.parse(data, filename=mod_path)
        except:
            print("Syntax error 'ast.parse' can't read %r" % mod_path)
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
                print("AST error in module %s" % mod_name)
                import traceback
                traceback.print_exc()
                raise

            if force_support is not None:
                mod.bl_info["support"] = force_support

            return mod
        else:
            return None

    modules_stale = set(module_cache.keys())

    for path in path_list:

        # force all contrib addons to be 'TESTING'
        if path.endswith("addons_contrib") or path.endswith("addons_extern"):
            force_support = 'TESTING'
        else:
            force_support = None

        for mod_name, mod_path in _bpy.path.module_names(path):
            modules_stale -= {mod_name}
            mod = module_cache.get(mod_name)
            if mod:
                if mod.__file__ != mod_path:
                    print("multiple addons with the same name:\n  %r\n  %r" %
                          (mod.__file__, mod_path))
                    error_duplicates = True

                elif mod.__time__ != os.path.getmtime(mod_path):
                    print("reloading addon:",
                          mod_name,
                          mod.__time__,
                          os.path.getmtime(mod_path),
                          mod_path,
                          )
                    del module_cache[mod_name]
                    mod = None

            if mod is None:
                mod = fake_module(mod_name,
                                  mod_path,
                                  force_support=force_support)
                if mod:
                    module_cache[mod_name] = mod

    # just in case we get stale modules, not likely
    for mod_stale in modules_stale:
        del module_cache[mod_stale]
    del modules_stale

    mod_list = list(module_cache.values())
    mod_list.sort(key=lambda mod: (mod.bl_info['category'],
                                   mod.bl_info['name'],
                                   ))
    return mod_list


def check(module_name):
    """
    Returns the loaded state of the addon.

    :arg module_name: The name of the addon and module.
    :type module_name: string
    :return: (loaded_default, loaded_state)
    :rtype: tuple of booleans
    """
    import sys
    loaded_default = module_name in _bpy.context.user_preferences.addons

    mod = sys.modules.get(module_name)
    loaded_state = mod and getattr(mod, "__addon_enabled__", Ellipsis)

    if loaded_state is Ellipsis:
        print("Warning: addon-module %r found module "
               "but without __addon_enabled__ field, "
               "possible name collision from file: %r" %
               (module_name, getattr(mod, "__file__", "<unknown>")))

        loaded_state = False

    return loaded_default, loaded_state


def enable(module_name, default_set=True):
    """
    Enables an addon by name.

    :arg module_name: The name of the addon and module.
    :type module_name: string
    :return: the loaded module or None on failure.
    :rtype: module
    """

    import os
    import sys
    import imp

    def handle_error():
        import traceback
        traceback.print_exc()

    # reload if the mtime changes
    mod = sys.modules.get(module_name)
    # chances of the file _not_ existing are low, but it could be removed
    if mod and os.path.exists(mod.__file__):
        mod.__addon_enabled__ = False
        mtime_orig = getattr(mod, "__time__", 0)
        mtime_new = os.path.getmtime(mod.__file__)
        if mtime_orig != mtime_new:
            print("module changed on disk:", mod.__file__, "reloading...")

            try:
                imp.reload(mod)
            except:
                handle_error()
                del sys.modules[module_name]
                return None
            mod.__addon_enabled__ = False

    # Split registering up into 3 steps so we can undo
    # if it fails par way through.

    # 1) try import
    try:
        mod = __import__(module_name)
        mod.__time__ = os.path.getmtime(mod.__file__)
        mod.__addon_enabled__ = False
    except:
        handle_error()
        return None

    # 2) try register collected modules
    # removed, addons need to handle own registration now.

    # 3) try run the modules register function
    try:
        mod.register()
    except:
        handle_error()
        del sys.modules[module_name]
        return None

    # * OK loaded successfully! *
    if default_set:
        # just in case its enabled already
        ext = _bpy.context.user_preferences.addons.get(module_name)
        if not ext:
            ext = _bpy.context.user_preferences.addons.new()
            ext.module = module_name

    mod.__addon_enabled__ = True

    if _bpy.app.debug:
        print("\taddon_utils.enable", mod.__name__)

    return mod


def disable(module_name, default_set=True):
    """
    Disables an addon by name.

    :arg module_name: The name of the addon and module.
    :type module_name: string
    """
    import sys
    mod = sys.modules.get(module_name)

    # possible this addon is from a previous session and didn't load a
    # module this time. So even if the module is not found, still disable
    # the addon in the user prefs.
    if mod:
        mod.__addon_enabled__ = False

        try:
            mod.unregister()
        except:
            import traceback
            traceback.print_exc()
    else:
        print("addon_utils.disable", module_name, "not loaded")

    # could be in more then once, unlikely but better do this just in case.
    addons = _bpy.context.user_preferences.addons

    if default_set:
        while module_name in addons:
            addon = addons.get(module_name)
            if addon:
                addons.remove(addon)

    if _bpy.app.debug:
        print("\taddon_utils.disable", module_name)


def reset_all(reload_scripts=False):
    """
    Sets the addon state based on the user preferences.
    """
    import sys
    import imp

    # RELEASE SCRIPTS: official scripts distributed in Blender releases
    paths_list = paths()

    for path in paths_list:
        _bpy.utils._sys_path_ensure(path)
        for mod_name, mod_path in _bpy.path.module_names(path):
            is_enabled, is_loaded = check(mod_name)

            # first check if reload is needed before changing state.
            if reload_scripts:
                mod = sys.modules.get(mod_name)
                if mod:
                    imp.reload(mod)

            if is_enabled == is_loaded:
                pass
            elif is_enabled:
                enable(mod_name)
            elif is_loaded:
                print("\taddon_utils.reset_all unloading", mod_name)
                disable(mod_name)


def module_bl_info(mod, info_basis={"name": "",
                                    "author": "",
                                    "version": (),
                                    "blender": (),
                                    "api": 0,
                                    "location": "",
                                    "description": "",
                                    "wiki_url": "",
                                    "tracker_url": "",
                                    "support": 'COMMUNITY',
                                    "category": "",
                                    "warning": "",
                                    "show_expanded": False,
                                    }
                   ):

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

    addon_info["_init"] = None
    return addon_info
