# SPDX-FileCopyrightText: 2011-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

__all__ = (
    "paths",
    "modules",
    "check",
    "check_extension",
    "enable",
    "disable",
    "disable_all",
    "reset_all",
    "module_bl_info",
)

import bpy as _bpy
_preferences = _bpy.context.preferences

error_encoding = False
# (name, file, path)
error_duplicates = []
addons_fake_modules = {}


# called only once at startup, avoids calling 'reset_all', correct but slower.
def _initialize_once():
    for path in paths():
        _bpy.utils._sys_path_ensure_append(path)

    _initialize_extensions_repos_once()

    for addon in _preferences.addons:
        enable(addon.module)

    _initialize_ensure_extensions_addon()


def paths():
    return [
        path for subdir in (
            # RELEASE SCRIPTS: official scripts distributed in Blender releases.
            "addons",
            # CONTRIB SCRIPTS: good for testing but not official scripts yet
            # if folder addons_contrib/ exists, scripts in there will be loaded too.
            "addons_contrib",
        )
        for path in _bpy.utils.script_paths(subdir=subdir)
    ]


# A version of `paths` that includes extension repositories returning a list `(path, package)` pairs.
#
# Notes on the ``package`` value.
#
# - For top-level modules (the "addons" directories, the value is an empty string)
#   because those add-ons can be imported directly.
# - For extension repositories the value is a module string (which can be imported for example)
#   where any modules within the `path` can be imported as a sub-module.
#   So for example, given a list value of: `("/tmp/repo", "bl_ext.temp_repo")`.
#
#   An add-on located at `/tmp/repo/my_handy_addon.py` will have a unique module path of:
#   `bl_ext.temp_repo.my_handy_addon`, which can be imported and will be the value of it's `Addon.module`.
def _paths_with_extension_repos():

    import os
    addon_paths = [(path, "") for path in paths()]
    if _preferences.experimental.use_extension_repos:
        for repo in _preferences.filepaths.extension_repos:
            if not repo.enabled:
                continue
            dirpath = repo.directory
            if not os.path.isdir(dirpath):
                continue
            addon_paths.append((dirpath, "%s.%s" % (_ext_base_pkg_idname, repo.module)))

    return addon_paths


def _fake_module(mod_name, mod_path, speedy=True, force_support=None):
    global error_encoding
    import os

    if _bpy.app.debug_python:
        print("fake_module", mod_path, mod_name)

    if mod_name.startswith(_ext_base_pkg_idname_with_dot):
        return _fake_module_from_extension(mod_name, mod_path, force_support=force_support)

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
    except BaseException:
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
            print("AST error parsing bl_info for:", repr(mod_path))
            import traceback
            traceback.print_exc()
            return None

        if force_support is not None:
            mod.bl_info["support"] = force_support

        return mod
    else:
        print("Warning: add-on missing 'bl_info', this can cause poor performance!:", repr(mod_path))
        return None


def modules_refresh(*, module_cache=addons_fake_modules):
    global error_encoding
    import os

    error_encoding = False
    error_duplicates.clear()

    modules_stale = set(module_cache.keys())

    for path, pkg_id in _paths_with_extension_repos():

        # Force all user contributed add-ons to be 'TESTING'.
        force_support = 'TESTING' if ((not pkg_id) and path.endswith("addons_contrib")) else None

        for mod_name, mod_path in _bpy.path.module_names(path, package=pkg_id):
            modules_stale.discard(mod_name)
            mod = module_cache.get(mod_name)
            if mod is not None:
                if mod.__file__ != mod_path:
                    print(
                        "multiple addons with the same name:\n"
                        "  %r\n"
                        "  %r" % (mod.__file__, mod_path)
                    )
                    error_duplicates.append((mod.bl_info["name"], mod.__file__, mod_path))

                elif (
                        (mod.__time__ != os.path.getmtime(metadata_path := mod_path)) if not pkg_id else
                        # Check the manifest time as this is the source of the cache.
                        (mod.__time_manifest__ != os.path.getmtime(metadata_path := mod.__file_manifest__))
                ):
                    print("reloading addon meta-data:", mod_name, repr(metadata_path), "(time-stamp change detected)")
                    del module_cache[mod_name]
                    mod = None

            if mod is None:
                mod = _fake_module(
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


def modules(*, module_cache=addons_fake_modules, refresh=True):
    if refresh or ((module_cache is addons_fake_modules) and modules._is_first):
        modules_refresh(module_cache=module_cache)
        modules._is_first = False

    mod_list = list(module_cache.values())
    mod_list.sort(
        key=lambda mod: (
            mod.bl_info.get("category", ""),
            mod.bl_info.get("name", ""),
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
    loaded_default = module_name in _preferences.addons

    mod = sys.modules.get(module_name)
    loaded_state = (
        (mod is not None) and
        getattr(mod, "__addon_enabled__", Ellipsis)
    )

    if loaded_state is Ellipsis:
        print(
            "Warning: addon-module", module_name, "found module "
            "but without '__addon_enabled__' field, "
            "possible name collision from file:",
            repr(getattr(mod, "__file__", "<unknown>")),
        )

        loaded_state = False

    if mod and getattr(mod, "__addon_persistent__", False):
        loaded_default = True

    return loaded_default, loaded_state


def check_extension(module_name):
    """
    Return true if the module is an extension.
    """
    return module_name.startswith(_ext_base_pkg_idname_with_dot)


# utility functions


def _addon_ensure(module_name):
    addons = _preferences.addons
    addon = addons.get(module_name)
    if not addon:
        addon = addons.new()
        addon.module = module_name


def _addon_remove(module_name):
    addons = _preferences.addons

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
    import importlib
    from bpy_restrict_state import RestrictBlend

    is_extension = module_name.startswith(_ext_base_pkg_idname_with_dot)

    if handle_error is None:
        def handle_error(_ex):
            import traceback
            traceback.print_exc()

    # reload if the mtime changes
    mod = sys.modules.get(module_name)
    # chances of the file _not_ existing are low, but it could be removed

    # Set to `mod.__file__` or None.
    mod_file = None

    if (
            (mod is not None) and
            (mod_file := mod.__file__) is not None and
            os.path.exists(mod_file)
    ):

        if getattr(mod, "__addon_enabled__", False):
            # This is an unlikely situation,
            # re-register if the module is enabled.
            # Note: the UI doesn't allow this to happen,
            # in most cases the caller should 'check()' first.
            try:
                mod.unregister()
            except BaseException as ex:
                print("Exception in module unregister():", (mod_file or module_name))
                handle_error(ex)
                return None

        mod.__addon_enabled__ = False
        mtime_orig = getattr(mod, "__time__", 0)
        mtime_new = os.path.getmtime(mod_file)
        if mtime_orig != mtime_new:
            print("module changed on disk:", repr(mod_file), "reloading...")

            try:
                importlib.reload(mod)
            except BaseException as ex:
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

    # Disable the context: using the context at all
    # while loading an addon is really bad, don't do it!
    with RestrictBlend():

        # 1) try import
        try:
            # Use instead of `__import__` so that sub-modules can eventually be supported.
            # This is also documented to be the preferred way to import modules.
            mod = importlib.import_module(module_name)
            if (mod_file := mod.__file__) is None:
                # This can happen when the addon has been removed but there are
                # residual `.pyc` files left behind.
                raise ImportError(name=module_name)
            mod.__time__ = os.path.getmtime(mod_file)
            mod.__addon_enabled__ = False
        except BaseException as ex:
            # If the add-on doesn't exist, don't print full trace-back because the back-trace is in this case
            # is verbose without any useful details. A missing path is better communicated in a short message.
            # Account for `ImportError` & `ModuleNotFoundError`.
            if isinstance(ex, ImportError):
                if ex.name == module_name:
                    print("Add-on not loaded: \"%s\", cause: %s" % (module_name, str(ex)))

                # Issue with an add-on from an extension repository, report a useful message.
                elif is_extension and module_name.startswith(ex.name + "."):
                    repo_id = module_name[len(_ext_base_pkg_idname_with_dot):].rpartition(".")[0]
                    repo = next(
                        (repo for repo in _preferences.filepaths.extension_repos if repo.module == repo_id),
                        None,
                    )
                    if repo is None:
                        print(
                            "Add-on not loaded: \"%s\", cause: extension repository \"%s\" doesn't exist" %
                            (module_name, repo_id)
                        )
                    elif not repo.enabled:
                        print(
                            "Add-on not loaded: \"%s\", cause: extension repository \"%s\" is disabled" %
                            (module_name, repo_id)
                        )
                    else:
                        # The repository exists and is enabled, it should have imported.
                        print("Add-on not loaded: \"%s\", cause: %s" % (module_name, str(ex)))
                else:
                    handle_error(ex)
            else:
                handle_error(ex)

            if default_set:
                _addon_remove(module_name)
            return None

        if is_extension:
            # Handle the case the an extension has `bl_info` (which is not used for extensions).
            # Note that internally a `bl_info` is added based on the extensions manifest - for compatibility.
            # So it's important not to use this one.
            bl_info = getattr(mod, "bl_info", None)
            if bl_info is not None:
                # Use `_init` to detect when `bl_info` was generated from the manifest, see: `_bl_info_from_extension`.
                if type(bl_info) is dict and "_init" not in bl_info:
                    # This print is noisy, hide behind a debug flag.
                    # Once `bl_info` is fully deprecated this should be changed to always print a warning.
                    if _bpy.app.debug_python:
                        print(
                            "Add-on \"%s\" has a \"bl_info\" which will be ignored in favor of \"%s\"" %
                            (module_name, _ext_manifest_filename_toml)
                        )
                # Always remove as this is not expected to exist and will be lazily initialized.
                del mod.bl_info

        # 2) Try register collected modules.
        # Removed register_module, addons need to handle their own registration now.

        from _bpy import _bl_owner_id_get, _bl_owner_id_set
        owner_id_prev = _bl_owner_id_get()
        _bl_owner_id_set(module_name)

        # 3) Try run the modules register function.
        try:
            mod.register()
        except BaseException as ex:
            print("Exception in module register():", (mod_file or module_name))
            handle_error(ex)
            del sys.modules[module_name]
            if default_set:
                _addon_remove(module_name)
            return None
        finally:
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
        def handle_error(_ex):
            import traceback
            traceback.print_exc()

    mod = sys.modules.get(module_name)

    # Possible this add-on is from a previous session and didn't load a
    # module this time. So even if the module is not found, still disable
    # the add-on in the user preferences.
    if mod and getattr(mod, "__addon_enabled__", False) is not False:
        mod.__addon_enabled__ = False
        mod.__addon_persistent__ = False

        try:
            mod.unregister()
        except BaseException as ex:
            mod_path = getattr(mod, "__file__", module_name)
            print("Exception in module unregister():", repr(mod_path))
            del mod_path
            handle_error(ex)
    else:
        print(
            "addon_utils.disable: %s not %s" % (
                module_name,
                "loaded" if mod is None else "enabled")
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

    for path, pkg_id in _paths_with_extension_repos():
        if not pkg_id:
            _bpy.utils._sys_path_ensure_append(path)

        for mod_name, _mod_path in _bpy.path.module_names(path, package=pkg_id):
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
    # Collect modules to disable first because dict can be modified as we disable.

    # NOTE: don't use `getattr(item[1], "__addon_enabled__", False)` because this runs on all modules,
    # including 3rd party modules unrelated to Blender.
    #
    # Some modules may have their own `__getattr__` and either:
    # - Not raise an `AttributeError` (is they should),
    #   causing `hasattr` & `getattr` to raise an exception instead of treating the attribute as missing.
    # - Generate modules dynamically, modifying `sys.modules` which is being iterated over,
    #   causing a RuntimeError: "dictionary changed size during iteration".
    #
    # Either way, running 3rd party logic here can cause undefined behavior.
    # Use direct `__dict__` access to bypass `__getattr__`, see: #111649.
    addon_modules = [
        item for item in sys.modules.items()
        if type(mod_dict := getattr(item[1], "__dict__", None)) is dict
        if mod_dict.get("__addon_enabled__")
    ]
    # Check the enabled state again since it's possible the disable call
    # of one add-on disables others.
    for mod_name, mod in addon_modules:
        if getattr(mod, "__addon_enabled__", False):
            disable(mod_name)


def _blender_manual_url_prefix():
    return "https://docs.blender.org/manual/%s/%d.%d" % (_bpy.utils.manual_language_code(), *_bpy.app.version[:2])


def _bl_info_basis():
    return {
        "name": "",
        "author": "",
        "version": (),
        "blender": (),
        "location": "",
        "description": "",
        "doc_url": "",
        "support": 'COMMUNITY',
        "category": "",
        "warning": "",
        "show_expanded": False,
    }


def module_bl_info(mod, *, info_basis=None):
    if info_basis is None:
        info_basis = _bl_info_basis()

    addon_info = getattr(mod, "bl_info", {})

    # avoid re-initializing
    if "_init" in addon_info:
        return addon_info

    if not addon_info:
        if mod.__name__.startswith(_ext_base_pkg_idname_with_dot):
            addon_info, filepath_toml = _bl_info_from_extension(mod.__name__, mod.__file__)
            if addon_info is None:
                # Unexpected, this is a malformed extension if meta-data can't be loaded.
                print("module_bl_info: failed to extract meta-data from", filepath_toml)
                # Continue to initialize dummy data.
                addon_info = {}

        mod.bl_info = addon_info

    for key, value in info_basis.items():
        addon_info.setdefault(key, value)

    if not addon_info["name"]:
        addon_info["name"] = mod.__name__

    doc_url = addon_info["doc_url"]
    if doc_url:
        doc_url_prefix = "{BLENDER_MANUAL_URL}"
        if doc_url_prefix in doc_url:
            addon_info["doc_url"] = doc_url.replace(
                doc_url_prefix,
                _blender_manual_url_prefix(),
            )

    addon_info["_init"] = None
    return addon_info


# -----------------------------------------------------------------------------
# Extension Utilities

def _bl_info_from_extension(mod_name, mod_path, force_support=None):
    # Extract the `bl_info` from an extensions manifest.
    # This is returned as a module which has a `bl_info` variable.
    # When support for non-extension add-ons is dropped (Blender v5.0 perhaps)
    # this can be updated not to use a fake module.
    import os
    import tomllib

    bl_info = _bl_info_basis()

    filepath_toml = os.path.join(os.path.dirname(mod_path), _ext_manifest_filename_toml)
    try:
        with open(filepath_toml, "rb") as fh:
            data = tomllib.load(fh)
    except FileNotFoundError:
        print("Warning: add-on missing manifest, this can cause poor performance!:", repr(filepath_toml))
        return None, filepath_toml
    except BaseException as ex:
        print("Error:", str(ex), "in", filepath_toml)
        return None, filepath_toml

    # This isn't a full validation which happens on package install/update.
    if (value := data.get("name", None)) is None:
        print("Error: missing \"name\" from in", filepath_toml)
        return None, filepath_toml
    if type(value) is not str:
        print("Error: \"name\" is not a string in", filepath_toml)
        return None, filepath_toml
    bl_info["name"] = value

    if (value := data.get("version", None)) is None:
        print("Error: missing \"version\" from in", filepath_toml)
        return None, filepath_toml
    if type(value) is not str:
        print("Error: \"version\" is not a string in", filepath_toml)
        return None, filepath_toml
    bl_info["version"] = value

    if (value := data.get("blender_version_min", None)) is None:
        print("Error: missing \"blender_version_min\" from in", filepath_toml)
        return None, filepath_toml
    if type(value) is not str:
        print("Error: \"blender_version_min\" is not a string in", filepath_toml)
        return None, filepath_toml
    try:
        value = tuple(int(x) for x in value.split("."))
    except BaseException as ex:
        print("Error:", str(ex), "in \"blender_version_min\"", filepath_toml)
        return None, filepath_toml
    bl_info["blender"] = value

    if (value := data.get("maintainer", None)) is None:
        print("Error: missing \"author\" from in", filepath_toml)
        return None, filepath_toml
    if type(value) is not str:
        print("Error: \"maintainer\" is not a string", filepath_toml)
        return None, filepath_toml
    bl_info["author"] = value

    bl_info["category"] = "Development"  # Dummy, will be removed.

    if force_support is not None:
        bl_info["support"] = force_support
    return bl_info, filepath_toml


def _fake_module_from_extension(mod_name, mod_path, force_support=None):
    import os

    bl_info, filepath_toml = _bl_info_from_extension(mod_name, mod_path, force_support=force_support)
    if bl_info is None:
        return None

    ModuleType = type(os)
    mod = ModuleType(mod_name)
    mod.bl_info = bl_info
    mod.__file__ = mod_path
    mod.__time__ = os.path.getmtime(mod_path)

    # NOTE(@ideasman42): Add non-standard manifest variables to the "fake" module,
    # this isn't ideal as it moves further away from the return value being minimal fake-module
    # (where `__name__` and `__file__` are typically used).
    # A custom type could be used, however this needs to be done carefully
    # as all users of `addon_utils.modules(..)` need to be updated.
    mod.__file_manifest__ = filepath_toml
    mod.__time_manifest__ = os.path.getmtime(filepath_toml)
    return mod


# -----------------------------------------------------------------------------
# Extensions

def _initialize_ensure_extensions_addon():
    if _preferences.experimental.use_extension_repos:
        module_name = "bl_pkg"
        if module_name not in _preferences.addons:
            enable(module_name, default_set=True, persistent=True)


# Module-like class, store singletons.
class _ext_global:
    __slots__ = ()

    # Store a map of `preferences.filepaths.extension_repos` -> `module_id`.
    # Only needed to detect renaming between `bpy.app.handlers.extension_repos_update_{pre & post}` events.
    #
    # The first dictionary is for enabled repositories, the second for disabled repositories
    # which can be ignored in most cases and is only needed for a module rename.
    idmap_pair = {}, {}

    # The base package created by `JunctionModuleHandle`.
    module_handle = None


# The name (in `sys.modules`) keep this short because it's stored as part of add-on modules name.
_ext_base_pkg_idname = "bl_ext"
_ext_base_pkg_idname_with_dot = _ext_base_pkg_idname + "."
_ext_manifest_filename_toml = "blender_manifest.toml"


def _extension_preferences_idmap():
    repos_idmap = {}
    repos_idmap_disabled = {}
    if _preferences.experimental.use_extension_repos:
        for repo in _preferences.filepaths.extension_repos:
            if repo.enabled:
                repos_idmap[repo.as_pointer()] = repo.module
            else:
                repos_idmap_disabled[repo.as_pointer()] = repo.module
    return repos_idmap, repos_idmap_disabled


def _extension_dirpath_from_preferences():
    repos_dict = {}
    if _preferences.experimental.use_extension_repos:
        for repo in _preferences.filepaths.extension_repos:
            if not repo.enabled:
                continue
            repos_dict[repo.module] = repo.directory
    return repos_dict


def _extension_dirpath_from_handle():
    repos_info = {}
    for module_id, module in _ext_global.module_handle.submodule_items():
        # Account for it being unset although this should never happen unless script authors
        # meddle with the modules.
        try:
            dirpath = module.__path__[0]
        except BaseException:
            dirpath = ""
        repos_info[module_id] = dirpath
    return repos_info


# Ensure the add-ons follow changes to repositories, enabling, disabling and module renaming.
def _initialize_extension_repos_post_addons_prepare(
        module_handle,
        *,
        submodules_del,
        submodules_add,
        submodules_rename_module,
        submodules_del_disabled,
        submodules_rename_module_disabled,
):
    addons_to_enable = []
    if not (
            submodules_del or
            submodules_add or
            submodules_rename_module or
            submodules_del_disabled or
            submodules_rename_module_disabled
    ):
        return addons_to_enable

    # All preferences info.
    # Map: `repo_id -> {submodule_id -> addon, ...}`.
    addon_userdef_info = {}
    for addon in _preferences.addons:
        module = addon.module
        if not module.startswith(_ext_base_pkg_idname_with_dot):
            continue
        module_id, submodule_id = module[len(_ext_base_pkg_idname_with_dot):].partition(".")[0::2]
        try:
            addon_userdef_info[module_id][submodule_id] = addon
        except KeyError:
            addon_userdef_info[module_id] = {submodule_id: addon}

    # All run-time info.
    # Map: `module_id -> {submodule_id -> module, ...}`.
    addon_runtime_info = {}
    for module_id, repo_module in module_handle.submodule_items():
        extensions_info = {}
        for submodule_id in dir(repo_module):
            if submodule_id.startswith("_"):
                continue
            mod = getattr(repo_module, submodule_id)
            # Filter out non add-on, non-modules.
            if not hasattr(mod, "__addon_enabled__"):
                continue
            extensions_info[submodule_id] = mod
        addon_runtime_info[module_id] = extensions_info
        del extensions_info

    # Apply changes to add-ons.
    if submodules_add:
        # Re-enable add-ons that exist in the user preferences,
        # this lets the add-ons state be restored when toggling a repository.
        for module_id, _dirpath in submodules_add:
            repo_userdef = addon_userdef_info.get(module_id, {})
            repo_runtime = addon_runtime_info.get(module_id, {})

            for submodule_id, addon in repo_userdef.items():
                module_name_next = "%s.%s.%s" % (_ext_base_pkg_idname, module_id, submodule_id)
                # Only default & persistent add-ons are kept for re-activation.
                default_set = True
                persistent = True
                addons_to_enable.append((module_name_next, addon, default_set, persistent))

    for module_id_prev, module_id_next in submodules_rename_module:
        repo_userdef = addon_userdef_info.get(module_id_prev, {})
        repo_runtime = addon_runtime_info.get(module_id_prev, {})
        for submodule_id, mod in repo_runtime.items():
            if not getattr(mod, "__addon_enabled__", False):
                continue
            module_name_prev = "%s.%s.%s" % (_ext_base_pkg_idname, module_id_prev, submodule_id)
            module_name_next = "%s.%s.%s" % (_ext_base_pkg_idname, module_id_next, submodule_id)
            disable(module_name_prev, default_set=False)
            addon = repo_userdef.get(submodule_id)
            default_set = addon is not None
            persistent = getattr(mod, "__addon_persistent__", False)
            addons_to_enable.append((module_name_next, addon, default_set, persistent))

    for module_id_prev, module_id_next in submodules_rename_module_disabled:
        repo_userdef = addon_userdef_info.get(module_id_prev, {})
        repo_runtime = addon_runtime_info.get(module_id_prev, {})
        for submodule_id, addon in repo_userdef.items():
            mod = repo_runtime.get(submodule_id)
            if mod is not None and getattr(mod, "__addon_enabled__", False):
                continue
            # Either there is no run-time data or the module wasn't enabled.
            # Rename the add-on without enabling it so the next time it's enabled it's preferences are kept.
            module_name_next = "%s.%s.%s" % (_ext_base_pkg_idname, module_id_next, submodule_id)
            addon.module = module_name_next

    if submodules_del:
        repo_module_map = {repo.module: repo for repo in _preferences.filepaths.extension_repos}
        for module_id in submodules_del:
            repo_userdef = addon_userdef_info.get(module_id, {})
            repo_runtime = addon_runtime_info.get(module_id, {})

            repo = repo_module_map.get(module_id)
            default_set = True
            if repo and not repo.enabled:
                # The repository exists but has been disabled, keep the add-on preferences
                # because the user may want to re-enable the repository temporarily.
                default_set = False

            for submodule_id, mod in repo_runtime.items():
                module_name_prev = "%s.%s.%s" % (_ext_base_pkg_idname, module_id, submodule_id)
                disable(module_name_prev, default_set=default_set)
            del repo
        del repo_module_map

    if submodules_del_disabled:
        for module_id_prev in submodules_del_disabled:
            repo_userdef = addon_userdef_info.get(module_id_prev, {})
            for submodule_id in repo_userdef.keys():
                module_name_prev = "%s.%s.%s" % (_ext_base_pkg_idname, module_id_prev, submodule_id)
                disable(module_name_prev, default_set=True)

    return addons_to_enable


# Enable add-ons after the modules have been manipulated.
def _initialize_extension_repos_post_addons_restore(addons_to_enable):
    if not addons_to_enable:
        return

    for (module_name_next, addon, default_set, persistent) in addons_to_enable:
        # Ensure the preferences are kept.
        if addon is not None:
            addon.module = module_name_next
        enable(module_name_next, default_set=default_set, persistent=persistent)
    # Needed for module rename.
    modules._is_first = True


# Use `bpy.app.handlers.extension_repos_update_{pre/post}` to track changes to extension repositories
# and sync the changes to the Python module.


@_bpy.app.handlers.persistent
def _initialize_extension_repos_pre(*_):
    _ext_global.idmap_pair = _extension_preferences_idmap()


@_bpy.app.handlers.persistent
def _initialize_extension_repos_post(*_, is_first=False):

    # When enabling extensions for the first time, ensure the add-on is enabled.
    _initialize_ensure_extensions_addon()

    do_addons = not is_first

    # Map `module_id` -> `dirpath`.
    repos_info_prev = _extension_dirpath_from_handle()
    repos_info_next = _extension_dirpath_from_preferences()

    # Map `repo.as_pointer()` -> `module_id`.
    repos_idmap_prev, repos_idmap_prev_disabled = _ext_global.idmap_pair
    repos_idmap_next, repos_idmap_next_disabled = _extension_preferences_idmap()

    # Map `module_id` -> `repo.as_pointer()`.
    repos_idmap_next_reverse = {value: key for key, value in repos_idmap_next.items()}

    # Mainly needed when the `preferences.experimental.use_extension_repos` option is enabled at run-time.
    #
    # Filter `repos_idmap_prev` so only items which were also in the `repos_info_prev` are included.
    # This is an awkward situation, they should be in sync, however when enabling the experimental option
    # means the preferences wont have changed, but the module will not be in sync with the preferences.
    # Support this by removing items in `repos_idmap_prev` which aren't also initialized in the managed package.
    #
    # The only situation this would be useful to keep is if we want to support renaming a package
    # that manipulates all add-ons using it, when those add-ons are in the preferences but have not had
    # their package loaded. It's possible we want to do this but is also reasonably obscure.
    for repo_id_prev, module_id_prev in list(repos_idmap_prev.items()):
        if module_id_prev not in repos_info_prev:
            del repos_idmap_prev[repo_id_prev]

    submodules_add = []  # List of module names to add: `(module_id, dirpath)`.
    submodules_del = []  # List of module names to remove: `module_id`.
    submodules_rename_module = []  # List of module names: `(module_id_src, module_id_dst)`.
    submodules_rename_dirpath = []  # List of module names: `(module_id, dirpath)`.

    renamed_prev = set()
    renamed_next = set()

    # Detect rename modules & module directories.
    for module_id_next, dirpath_next in repos_info_next.items():
        # Lookup never fails, as the "next" values use: `preferences.filepaths.extension_repos`.
        repo_id = repos_idmap_next_reverse[module_id_next]
        # Lookup may fail if this is a newly added module.
        # Don't attempt to setup `submodules_add` though as it's possible
        # the module name persists while the underlying `repo_id` changes.
        module_id_prev = repos_idmap_prev.get(repo_id)
        if module_id_prev is None:
            continue

        # Detect rename.
        if module_id_next != module_id_prev:
            submodules_rename_module.append((module_id_prev, module_id_next))
            renamed_prev.add(module_id_prev)
            renamed_next.add(module_id_next)

        # Detect `dirpath` change.
        if dirpath_next != repos_info_prev[module_id_prev]:
            submodules_rename_dirpath.append((module_id_next, dirpath_next))

    # Detect added modules.
    for module_id, dirpath in repos_info_next.items():
        if (module_id not in repos_info_prev) and (module_id not in renamed_next):
            submodules_add.append((module_id, dirpath))
    # Detect deleted modules.
    for module_id, _dirpath in repos_info_prev.items():
        if (module_id not in repos_info_next) and (module_id not in renamed_prev):
            submodules_del.append(module_id)

    if do_addons:
        submodules_del_disabled = []  # A version of `submodules_del` for disabled repositories.
        submodules_rename_module_disabled = []  # A version of `submodules_rename_module` for disabled repositories.

        # Detect deleted modules.
        for repo_id_prev, module_id_prev in repos_idmap_prev_disabled.items():
            if (
                    (repo_id_prev not in repos_idmap_next_disabled) and
                    (repo_id_prev not in repos_idmap_next)
            ):
                submodules_del_disabled.append(module_id_prev)

        # Detect rename of disabled modules.
        for repo_id_next, module_id_next in repos_idmap_next_disabled.items():
            module_id_prev = repos_idmap_prev_disabled.get(repo_id_next)
            if module_id_prev is None:
                continue
            # Detect rename.
            if module_id_next != module_id_prev:
                submodules_rename_module_disabled.append((module_id_prev, module_id_next))

        addons_to_enable = _initialize_extension_repos_post_addons_prepare(
            _ext_global.module_handle,
            submodules_del=submodules_del,
            submodules_add=submodules_add,
            submodules_rename_module=submodules_rename_module,
            submodules_del_disabled=submodules_del_disabled,
            submodules_rename_module_disabled=submodules_rename_module_disabled,
        )
        del submodules_del_disabled, submodules_rename_module_disabled

        # Apply changes to the `_ext_base_pkg_idname` named module so it matches extension data from the preferences.
    module_handle = _ext_global.module_handle
    for module_id in submodules_del:
        module_handle.unregister_submodule(module_id)
    for module_id, dirpath in submodules_add:
        module_handle.register_submodule(module_id, dirpath)
    for module_id_prev, module_id_next in submodules_rename_module:
        module_handle.rename_submodule(module_id_prev, module_id_next)
    for module_id, dirpath in submodules_rename_dirpath:
        module_handle.rename_directory(module_id, dirpath)

    _ext_global.idmap_pair[0].clear()
    _ext_global.idmap_pair[1].clear()

    if do_addons:
        _initialize_extension_repos_post_addons_restore(addons_to_enable)

    # Force refreshing if directory paths change.
    if submodules_del or submodules_add or submodules_rename_dirpath:
        modules._is_first = True


def _initialize_extensions_repos_once():
    from bpy_extras.extensions.junction_module import JunctionModuleHandle
    module_handle = JunctionModuleHandle(_ext_base_pkg_idname)
    module_handle.register_module()
    _ext_global.module_handle = module_handle

    # Setup repositories for the first time.
    # Intentionally don't call `_initialize_extension_repos_pre` as this is the first time,
    # the previous state is not useful to read.
    _initialize_extension_repos_post(is_first=True)

    # Internal handlers intended for Blender's own handling of repositories.
    _bpy.app.handlers._extension_repos_update_pre.append(_initialize_extension_repos_pre)
    _bpy.app.handlers._extension_repos_update_post.append(_initialize_extension_repos_post)
