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
    "extensions_refresh",
    "stale_pending_remove_paths",
    "stale_pending_stage_paths",
)

import bpy as _bpy
_preferences = _bpy.context.preferences

error_encoding = False
# (name, file, path)
error_duplicates = []
addons_fake_modules = {}

# Global cached extensions, set before loading extensions on startup.
# `{addon_module_name: "Reason for incompatibility", ...}`
_extensions_incompatible = {}
# Global extension warnings, lazily calculated when displaying extensions.
# `{addon_module_name: "Warning", ...}`
_extensions_warnings = {}

# Filename used for stale files (which we can't delete).
_stale_filename = ".~stale~"


# called only once at startup, avoids calling 'reset_all', correct but slower.
def _initialize_once():
    for path in paths():
        _bpy.utils._sys_path_ensure_append(path)

    _stale_pending_check_and_remove_once()

    _initialize_extensions_repos_once()

    for addon in _preferences.addons:
        enable(
            addon.module,
            # Ensured by `_initialize_extensions_repos_once`.
            refresh_handled=True,
        )

    _initialize_ensure_extensions_addon()


def paths():
    import os

    paths = []
    for i, p in enumerate(_bpy.utils.script_paths()):
        # Bundled add-ons are always first.
        # Since this isn't officially part of the API, print an error so this never silently fails.
        addon_dir = os.path.join(p, "addons_core" if i == 0 else "addons")
        if os.path.isdir(addon_dir):
            paths.append(addon_dir)
        elif i == 0:
            print("Internal error:", addon_dir, "was not found!")
    return paths


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
    for repo in _preferences.extensions.repos:
        if not repo.enabled:
            continue
        dirpath = repo.directory
        if not os.path.isdir(dirpath):
            continue
        addon_paths.append((dirpath, "{:s}.{:s}".format(_ext_base_pkg_idname, repo.module)))

    return addon_paths


def _fake_module(mod_name, mod_path, speedy=True):
    global error_encoding
    import os

    if _bpy.app.debug_python:
        print("fake_module", mod_path, mod_name)

    if mod_name.startswith(_ext_base_pkg_idname_with_dot):
        return _fake_module_from_extension(mod_name, mod_path)

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
            line = ""
            while not line.startswith("bl_info"):
                try:
                    line = line_iter.readline()
                except UnicodeDecodeError as ex:
                    if not error_encoding:
                        error_encoding = True
                        print("Error reading file as UTF-8:", mod_path, ex)
                    return None

                if len(line) == 0:
                    break
            while line.rstrip():
                lines.append(line)
                try:
                    line = line_iter.readline()
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
    except Exception:
        print("Syntax error 'ast.parse' cannot read:", repr(mod_path))
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
        except Exception:
            print("AST error parsing bl_info for:", repr(mod_path))
            import traceback
            traceback.print_exc()
            return None

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
        for mod_name, mod_path in _bpy.path.module_names(path, package=pkg_id):
            modules_stale.discard(mod_name)
            mod = module_cache.get(mod_name)
            if mod is not None:
                if mod.__file__ != mod_path:
                    print(
                        "multiple addons with the same name:\n"
                        "  {!r}\n"
                        "  {!r}".format(mod.__file__, mod_path)
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

        # Dictionaries are ordered in more recent versions of Python,
        # re-create the dictionary from sorted items.
        # This avoids having to sort on every call to this function.
        module_cache_items = list(module_cache.items())
        # Sort by name with the module name as a tie breaker.
        module_cache_items.sort(key=lambda item: ((item[1].bl_info.get("name") or item[0]).casefold(), item[0]))
        module_cache.clear()
        module_cache.update((key, value) for key, value in module_cache_items)

    return module_cache.values()


modules._is_first = True


def check(module_name):
    """
    Returns the loaded state of the addon.

    :arg module_name: The name of the addon and module.
    :type module_name: str
    :return: (loaded_default, loaded_state)
    :rtype: tuple[bool, bool]
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


def enable(module_name, *, default_set=False, persistent=False, refresh_handled=False, handle_error=None):
    """
    Enables an addon by name.

    :arg module_name: the name of the addon and module.
    :type module_name: str
    :arg default_set: Set the user-preference.
    :type default_set: bool
    :arg persistent: Ensure the addon is enabled for the entire session (after loading new files).
    :type persistent: bool
    :arg refresh_handled: When true, :func:`extensions_refresh` must have been called with ``module_name``
       included in ``addon_modules_pending``.
       This should be used to avoid many calls to refresh extensions when enabling multiple add-ons at once.
    :type refresh_handled: bool
    :arg handle_error: Called in the case of an error, taking an exception argument.
    :type handle_error: Callable[[Exception], None] | None
    :return: the loaded module or None on failure.
    :rtype: ModuleType
    """

    import os
    import sys
    import importlib
    from _bpy_restrict_state import RestrictBlend

    if handle_error is None:
        def handle_error(ex):
            if isinstance(ex, ImportError):
                # NOTE: checking "Add-on " prefix is rather weak,
                # it's just a way to avoid the noise of a full trace-back when
                # an add-on is simply missing on the file-system.
                if (type(msg := ex.msg) is str) and msg.startswith("Add-on "):
                    print(msg)
                    return
            import traceback
            traceback.print_exc()

    if (is_extension := module_name.startswith(_ext_base_pkg_idname_with_dot)):
        if not refresh_handled:
            extensions_refresh(
                addon_modules_pending=[module_name],
                handle_error=handle_error,
            )

        # Ensure the extensions are compatible.
        if _extensions_incompatible:
            if (error := _extensions_incompatible.get(
                    module_name[len(_ext_base_pkg_idname_with_dot):].partition(".")[0::2],
            )):
                try:
                    raise RuntimeError("Extension {:s} is incompatible ({:s})".format(module_name, error))
                except RuntimeError as ex:
                    handle_error(ex)
                    # No need to call `extensions_refresh` because incompatible extensions
                    # will not have their wheels installed.
                    return None

        # NOTE: from now on, before returning None, `extensions_refresh()` must be called
        # to ensure wheels setup in anticipation for this extension being used are removed upon failure.

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
            except Exception as ex:
                print("Exception in module unregister():", (mod_file or module_name))
                handle_error(ex)
                if is_extension and not refresh_handled:
                    extensions_refresh(handle_error=handle_error)
                return None

        mod.__addon_enabled__ = False
        mtime_orig = getattr(mod, "__time__", 0)
        mtime_new = os.path.getmtime(mod_file)
        if mtime_orig != mtime_new:
            print("module changed on disk:", repr(mod_file), "reloading...")

            try:
                importlib.reload(mod)
            except Exception as ex:
                handle_error(ex)
                del sys.modules[module_name]

                if is_extension and not refresh_handled:
                    extensions_refresh(handle_error=handle_error)
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
                # This can happen when:
                # - The add-on has been removed but there are residual `.pyc` files left behind.
                # - An extension is a directory that doesn't contain an `__init__.py` file.
                #
                # Include a message otherwise the "cause:" for failing to load the module is left blank.
                # Include the `__path__` when available so there is a reference to the location that failed to load.
                raise ImportError(
                    "module loaded with no associated file, __path__={!r}, aborting!".format(
                        getattr(mod, "__path__", None)
                    ),
                    name=module_name,
                )
            mod.__time__ = os.path.getmtime(mod_file)
            mod.__addon_enabled__ = False
        except Exception as ex:
            # If the add-on doesn't exist, don't print full trace-back because the back-trace is in this case
            # is verbose without any useful details. A missing path is better communicated in a short message.
            # Account for `ImportError` & `ModuleNotFoundError`.
            if isinstance(ex, ImportError):
                if ex.name == module_name:
                    ex.msg = "Add-on not loaded: \"{:s}\", cause: {:s}".format(module_name, str(ex))

                # Issue with an add-on from an extension repository, report a useful message.
                elif is_extension and module_name.startswith(ex.name + "."):
                    repo_id = module_name[len(_ext_base_pkg_idname_with_dot):].rpartition(".")[0]
                    repo = next(
                        (repo for repo in _preferences.extensions.repos if repo.module == repo_id),
                        None,
                    )
                    if repo is None:
                        ex.msg = (
                            "Add-on not loaded: \"{:s}\", cause: extension repository \"{:s}\" doesn't exist".format(
                                module_name, repo_id,
                            )
                        )
                    elif not repo.enabled:
                        ex.msg = (
                            "Add-on not loaded: \"{:s}\", cause: extension repository \"{:s}\" is disabled".format(
                                module_name, repo_id,
                            )
                        )
                    else:
                        # The repository exists and is enabled, it should have imported.
                        ex.msg = "Add-on not loaded: \"{:s}\", cause: {:s}".format(module_name, str(ex))

            handle_error(ex)

            if default_set:
                _addon_remove(module_name)
            if is_extension and not refresh_handled:
                extensions_refresh(handle_error=handle_error)
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
                            "Add-on \"{:s}\" has a \"bl_info\" which will be ignored in favor of \"{:s}\"".format(
                                module_name, _ext_manifest_filename_toml,
                            )
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
        except Exception as ex:
            print("Exception in module register():", (mod_file or module_name))
            handle_error(ex)
            del sys.modules[module_name]
            if default_set:
                _addon_remove(module_name)
            if is_extension and not refresh_handled:
                extensions_refresh(handle_error=handle_error)
            return None
        finally:
            _bl_owner_id_set(owner_id_prev)

    # * OK loaded successfully! *
    mod.__addon_enabled__ = True
    mod.__addon_persistent__ = persistent

    if _bpy.app.debug_python:
        print("\taddon_utils.enable", mod.__name__)

    return mod


def disable(module_name, *, default_set=False, refresh_handled=False, handle_error=None):
    """
    Disables an addon by name.

    :arg module_name: The name of the addon and module.
    :type module_name: str
    :arg default_set: Set the user-preference.
    :type default_set: bool
    :arg handle_error: Called in the case of an error, taking an exception argument.
    :type handle_error: Callable[[Exception], None] | None
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
        except Exception as ex:
            mod_path = getattr(mod, "__file__", module_name)
            print("Exception in module unregister():", repr(mod_path))
            del mod_path
            handle_error(ex)
    else:
        print(
            "addon_utils.disable: {:s} not {:s}".format(
                module_name,
                "loaded" if mod is None else "enabled",
            )
        )

    # could be in more than once, unlikely but better do this just in case.
    if default_set:
        _addon_remove(module_name)

    if not refresh_handled:
        extensions_refresh(handle_error=handle_error)

    if _bpy.app.debug_python:
        print("\taddon_utils.disable", module_name)


def reset_all(*, reload_scripts=False):
    """
    Sets the addon state based on the user preferences.
    """
    import sys

    # Ensures stale `addons_fake_modules` isn't used.
    modules._is_first = True
    addons_fake_modules.clear()

    # Update extensions compatibility (after reloading preferences).
    # Potentially refreshing wheels too.
    extensions_refresh()

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
                enable(mod_name, refresh_handled=True)
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
    modules = sys.modules.copy()
    addon_modules = [
        item for item in modules.items()
        if type(mod_dict := getattr(item[1], "__dict__", None)) is dict
        if mod_dict.get("__addon_enabled__")
    ]
    # Check the enabled state again since it's possible the disable call
    # of one add-on disables others.
    for mod_name, mod in addon_modules:
        if getattr(mod, "__addon_enabled__", False):
            disable(mod_name, refresh_handled=True)


def _blender_manual_url_prefix():
    return "https://docs.blender.org/manual/{:s}/{:d}.{:d}".format(
        _bpy.utils.manual_language_code(),
        *_bpy.app.version[:2],
    )


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

    # Remove the maintainers email while it's not private, showing prominently
    # could cause maintainers to get direct emails instead of issue tracking systems.
    import re
    if "author" in addon_info:
        addon_info["author"] = re.sub(r"\s*<.*?>", "", addon_info["author"])

    addon_info["_init"] = None
    return addon_info


# -----------------------------------------------------------------------------
# Stale File Handling
#
# Notes:
# - On startup, a file exists that indicates cleanup is needed.
#   In the common case the file doesn't exist.
#   Otherwise module paths are scanned for files to remove.
# - Since errors resolving paths to remove could result in user data loss,
#   ensure the paths are always within the (extension/add-on/app-template) directory.
# - File locking isn't used, if multiple Blender instances start at the
#   same time and try to remove the same files, this won't cause errors.
#   Even so, remove the checking file immediately avoid unnecessary
#   file-system access overhead for other Blender instances.
#
# For more implementation details see `_bpy_internal.extensions.stale_file_manager`.
# This mainly impacts WIN32 which can't remove open file handles, see: #77837 & #125049.
#
# Use for all systems as the problem can impact any system if file removal fails
# for any reason (typically permissions or file-system error).

def _stale_pending_filepath():
    # When this file exists, stale file removal is pending.
    # Try to remove stale files next launch.
    import os
    return os.path.join(_bpy.utils.user_resource('CONFIG'), "stale-pending")


def _stale_pending_stage(debug):
    import os

    stale_pending_filepath = _stale_pending_filepath()
    dirpath = os.path.dirname(stale_pending_filepath)

    if os.path.exists(stale_pending_filepath):
        return

    try:
        os.makedirs(dirpath, exist_ok=True)
        with open(stale_pending_filepath, "wb") as _:
            pass
    except Exception as ex:
        print("Unable to set stale files pending:", str(ex))


def _stale_file_directory_iter():
    import os

    for repo in _preferences.extensions.repos:
        if not repo.enabled:
            continue
        if repo.source == 'SYSTEM':
            continue
        yield repo.directory

    # Skip `addons_core` because add-ons because these will never be uninstalled by the user.
    yield from paths()[1:]

    # The `local_dir`, for wheels.
    yield os.path.join(_bpy.utils.user_resource('EXTENSIONS'), ".local")

    # The `path_app_templates`, for user app-templates.
    yield _bpy.utils.user_resource(
        'SCRIPTS',
        path=os.path.join("startup", "bl_app_templates_user"),
        create=False,
    )


def _stale_pending_check_and_remove_once():
    # This runs on every startup, early exit if no stale data removal is staged.
    import os
    stale_pending_filepath = _stale_pending_filepath()
    if not os.path.exists(stale_pending_filepath):
        return

    # Some stale data needs to be removed, this is an exceptional case.
    # Allow for slower logic than is typically accepted on startup.
    from _bpy_internal.extensions.stale_file_manager import StaleFiles
    debug = _bpy.app.debug_python

    # Remove the pending file if all are removed.
    is_empty = True

    for dirpath in _stale_file_directory_iter():
        if not os.path.exists(os.path.join(dirpath, _stale_filename)):
            continue

        try:
            stale_handle = StaleFiles(
                base_directory=dirpath,
                stale_filename=_stale_filename,
                debug=debug,
            )
            stale_handle.state_load(check_exists=True)
            if not stale_handle.is_empty():
                stale_handle.state_remove_all()
                if not stale_handle.is_empty():
                    is_empty = False
            if stale_handle.is_modified():
                stale_handle.state_store(check_exists=False)
        except Exception as ex:
            print("Unexpected error clearing stale data, this is a bug!", str(ex))

    if is_empty:
        try:
            os.remove(stale_pending_filepath)
        except Exception as ex:
            if debug:
                print("Failed to remove stale-pending file:", str(ex))


def stale_pending_stage_paths(path_base, paths):
    # - `path_base` must a directory iterated over by `_stale_file_directory_iter`.
    #   Otherwise the stale files will never be removed.
    # - `paths` must be absolute paths which could not be removed.
    #   They must be located within `base_path` otherwise they cannot be removed.
    from _bpy_internal.extensions.stale_file_manager import StaleFiles

    debug = _bpy.app.debug_python

    stale_handle = StaleFiles(
        base_directory=path_base,
        stale_filename=_stale_filename,
        debug=debug,
    )
    # Already checked.
    if stale_handle.state_load_add_and_store(paths=paths):
        # Force clearing stale files on next restart.
        _stale_pending_stage(debug)


def stale_pending_remove_paths(path_base, paths):
    # The reverse of: `stale_pending_stage_paths`.
    from _bpy_internal.extensions.stale_file_manager import StaleFiles

    debug = _bpy.app.debug_python

    stale_handle = StaleFiles(
        base_directory=path_base,
        stale_filename=_stale_filename,
        debug=debug,
    )
    # Already checked.
    if stale_handle.state_load_remove_and_store(paths=paths):
        # Don't attempt to reverse the `_stale_pending_stage` call.
        # This is not trivial since other repositories may need to be cleared.
        # There will be a minor performance hit on restart but this is enough
        # of a corner case that it's not worth attempting to calculate if
        # removal of pending files is needed or not.
        pass


# -----------------------------------------------------------------------------
# Extension Pre-Flight Compatibility Check
#
# Check extension compatibility on startup so any extensions which are incompatible with Blender are marked as
# incompatible and wont be loaded. This cache avoids having to scan all extensions on startup on *every* startup.
#
# Implementation:
#
# The emphasis for this cache is to have minimum overhead for the common case where:
# - The simple case where there are no extensions enabled (running tests, background tasks etc).
# - The more involved case where extensions are enabled and have not changed since last time Blender started.
#   In this case do as little as possible since it runs on every startup, the following steps are unavoidable.
# - When reading compatibility cache, then run the following tests, regenerating when changes are detected.
#   - Compare with previous blender version/platform.
#   - Stat the manifests of all enabled extensions, testing that their modification-time and size are unchanged.
# - When any changes are detected,
#   regenerate compatibility information which does more expensive operations
#   (loading manifests, check version ranges etc).
#
# Other notes:
#
# - This internal format may change at any point, regenerating the cache should be reasonably fast
#   but may introduce a small but noticeable pause on startup for user configurations that contain many extensions.
# - Failure to load will simply ignore the file and regenerate the file as needed.
#
# Format:
#
# - The cache is ZLIB compressed pickled Python dictionary.
# - The dictionary keys are as follows:
#   `"blender": (bpy.app.version, platform.system(), platform.machine(), python_version, magic_number)`
#   `"filesystem": [(repo_module, pkg_id, manifest_time, manifest_size), ...]`
#   `"incompatible": {(repo_module, pkg_id): "Reason for being incompatible", ...}`
#


def _pickle_zlib_file_read(filepath):
    import pickle
    import gzip

    with gzip.GzipFile(filepath, "rb") as fh:
        data = pickle.load(fh)
    return data


def _pickle_zlib_file_write(filepath, data) -> None:
    import pickle
    import gzip

    with gzip.GzipFile(filepath, "wb", compresslevel=9) as fh:
        pickle.dump(data, fh)


def _extension_repos_module_to_directory_map():
    return {repo.module: repo.directory for repo in _preferences.extensions.repos if repo.enabled}


def _extension_compat_cache_update_needed(
        cache_data,  # `dict[str, Any]`
        blender_id,  # `tuple[Any, ...]`
        extensions_enabled,  # `set[tuple[str, str]]`
        print_debug,  # `Callable[[Any], None] | None`
):  # `-> bool`

    # Detect when Blender itself changes.
    if cache_data.get("blender") != blender_id:
        if print_debug is not None:
            print_debug("blender changed")
        return True

    # Detect when any of the extensions paths change.
    cache_filesystem = cache_data.get("filesystem", [])

    # Avoid touching the file-system if at all possible.
    # When the length is the same and all cached ID's are in this set, we can be sure they are a 1:1 patch.
    if len(cache_filesystem) != len(extensions_enabled):
        if print_debug is not None:
            print_debug("length changes ({:d} -> {:d}).".format(len(cache_filesystem), len(extensions_enabled)))
        return True

    from os import stat
    from os.path import join
    repos_module_to_directory_map = _extension_repos_module_to_directory_map()

    for repo_module, pkg_id, cache_stat_time, cache_stat_size in cache_filesystem:
        if (repo_module, pkg_id) not in extensions_enabled:
            if print_debug is not None:
                print_debug("\"{:s}.{:s}\" no longer enabled.".format(repo_module, pkg_id))
            return True

        if repo_directory := repos_module_to_directory_map.get(repo_module, ""):
            pkg_manifest_filepath = join(repo_directory, pkg_id, _ext_manifest_filename_toml)
        else:
            pkg_manifest_filepath = ""

        # It's possible an extension has been set as an add-on but cannot find the repository it came from.
        # In this case behave as if the file can't be found (because it can't) instead of ignoring it.
        # This is done because it's important to match.
        if pkg_manifest_filepath:
            try:
                statinfo = stat(pkg_manifest_filepath)
            except Exception:
                statinfo = None
        else:
            statinfo = None

        if statinfo is None:
            test_time = 0
            test_size = 0
        else:
            test_time = statinfo.st_mtime
            test_size = statinfo.st_size

        # Detect changes to any files manifest.
        if cache_stat_time != test_time:
            if print_debug is not None:
                print_debug("\"{:s}.{:s}\" time changed ({:g} -> {:g}).".format(
                    repo_module, pkg_id, cache_stat_time, test_time,
                ))
            return True
        if cache_stat_size != test_size:
            if print_debug is not None:
                print_debug("\"{:s}.{:s}\" size changed ({:d} -> {:d}).".format(
                    repo_module, pkg_id, cache_stat_size, test_size,
                ))
            return True

    return False


# This function should not run every startup, so it can afford to be slower,
# although users should not have to wait for it either.
def _extension_compat_cache_create(
        blender_id,  # `tuple[Any, ...]`
        extensions_enabled,  # `set[tuple[str, str]]`
        wheel_list,  # `list[tuple[str, list[str]]]`
        print_debug,  # `Callable[[Any], None] | None`
):  # `-> dict[str, Any]`
    import os
    from os.path import join

    filesystem = []
    incompatible = {}

    cache_data = {
        "blender": blender_id,
        "filesystem": filesystem,
        "incompatible": incompatible,
    }

    repos_module_to_directory_map = _extension_repos_module_to_directory_map()

    # Only import this module once (if at all).
    bl_pkg = None

    for repo_module, pkg_id in extensions_enabled:
        if repo_directory := repos_module_to_directory_map.get(repo_module, ""):
            pkg_manifest_filepath = join(repo_directory, pkg_id, _ext_manifest_filename_toml)
        else:
            pkg_manifest_filepath = ""
            if print_debug is not None:
                print_debug("directory for module \"{:s}\" not found!".format(repo_module))

        if pkg_manifest_filepath:
            try:
                statinfo = os.stat(pkg_manifest_filepath)
            except Exception:
                statinfo = None
                if print_debug is not None:
                    print_debug("unable to find \"{:s}\"".format(pkg_manifest_filepath))
        else:
            statinfo = None

        if statinfo is None:
            test_time = 0.0
            test_size = 0
        else:
            test_time = statinfo.st_mtime
            test_size = statinfo.st_size
            # Store the reason for failure, to print when attempting to load.

            # Only load the module once.
            if bl_pkg is None:
                # Without `bl_pkg.__time__` this will detect as having been changed and
                # reload the module when loading the add-on.
                import bl_pkg
                if getattr(bl_pkg, "__time__", 0) == 0:
                    try:
                        bl_pkg.__time__ = os.path.getmtime(bl_pkg.__file__)
                    except Exception as ex:
                        if print_debug is not None:
                            print_debug(str(ex))

            if (error := bl_pkg.manifest_compatible_with_wheel_data_or_error(
                    pkg_manifest_filepath,
                    repo_module,
                    pkg_id,
                    repo_directory,
                    wheel_list,
            )) is not None:
                incompatible[(repo_module, pkg_id)] = error

        filesystem.append((repo_module, pkg_id, test_time, test_size))

    return cache_data


def _initialize_extensions_compat_ensure_up_to_date(extensions_directory, extensions_enabled, print_debug):
    import os
    import platform
    import sys

    global _extensions_incompatible

    updated = False
    wheel_list = []

    # Number to bump to change this format and force re-generation.
    magic_number = 0

    blender_id = (_bpy.app.version, platform.system(), platform.machine(), sys.version_info[0:2], magic_number)

    filepath_compat = os.path.join(extensions_directory, ".cache", "compat.dat")

    # Cache data contains a dict of:
    # {
    #   "blender": (...)
    #   "paths": [path data to detect changes]
    #   "incompatible": {set of incompatible extensions}
    # }
    if os.path.exists(filepath_compat):
        try:
            cache_data = _pickle_zlib_file_read(filepath_compat)
        except Exception as ex:
            cache_data = None
            # While this should not happen continuously (that would point to writing invalid cache),
            # it is not a problem if there is some corruption with the cache and it needs to be re-generated.
            # Show a message since this should be a rare occurrence - if it happens often it's likely to be a bug.
            print("Extensions: reading cache failed ({:s}), creating...".format(str(ex)))
    else:
        cache_data = None
        if print_debug is not None:
            print_debug("doesn't exist, creating...")

    if cache_data is not None:
        # NOTE: the exception handling here is fairly paranoid and accounts for invalid values in the loaded cache.
        # An example would be values expected to be lists/dictionaries being other types (None or strings for example).
        # While this should not happen, some bad value should not prevent Blender from loading properly,
        # so report the error and regenerate cache.
        try:
            if _extension_compat_cache_update_needed(cache_data, blender_id, extensions_enabled, print_debug):
                cache_data = None
        except Exception:
            print("Extension: unexpected error reading cache, this is a bug! (regenerating)")
            import traceback
            traceback.print_exc()
            cache_data = None

    if cache_data is None:
        cache_data = _extension_compat_cache_create(blender_id, extensions_enabled, wheel_list, print_debug)
        try:
            os.makedirs(os.path.dirname(filepath_compat), exist_ok=True)
            _pickle_zlib_file_write(filepath_compat, cache_data)
            if print_debug is not None:
                print_debug("update written to disk.")
        except Exception as ex:
            # Should be rare but should not cause this function to fail.
            print("Extensions: writing cache failed ({:s}).".format(str(ex)))

        # Set to true even when not written to disk as the run-time data *has* been updated,
        # cache will attempt to be generated next time this is called.
        updated = True
    else:
        if print_debug is not None:
            print_debug("up to date.")

    _extensions_incompatible = cache_data["incompatible"]

    return updated, wheel_list


def _initialize_extensions_compat_ensure_up_to_date_wheels(extensions_directory, wheel_list, debug, error_fn):
    import os
    _extension_sync_wheels(
        local_dir=os.path.join(extensions_directory, ".local"),
        wheel_list=wheel_list,
        debug=debug,
        error_fn=error_fn,
    )


def _initialize_extensions_compat_data(
        extensions_directory,  # `str`
        *,
        ensure_wheels,  # `bool`
        addon_modules_pending,  # `Sequence[str] | None`
        use_startup_fastpath,  # `bool`
        error_fn,  # `Callable[[Exception], None] | None`
):
    # WARNING: this function must *never* raise an exception because it would interfere with low level initialization.
    # As the function deals with file IO, use what are typically over zealous exception checks so as to rule out
    # interfering with Blender loading properly in unexpected cases such as disk-full, read-only file-system
    # or any other rare but possible scenarios.

    _extensions_incompatible.clear()

    # Create a set of all extension ID's.
    extensions_enabled = set()
    extensions_prefix_len = len(_ext_base_pkg_idname_with_dot)
    for addon in _preferences.addons:
        module_name = addon.module
        if check_extension(module_name):
            extensions_enabled.add(module_name[extensions_prefix_len:].partition(".")[0::2])

    if addon_modules_pending is not None:
        for module_name in addon_modules_pending:
            if check_extension(module_name):
                extensions_enabled.add(module_name[extensions_prefix_len:].partition(".")[0::2])

    debug = _bpy.app.debug_python
    print_debug = (lambda *args, **kwargs: print("Extension version cache:", *args, **kwargs)) if debug else None

    # Early exit, use for automated tests.
    # Avoid (relatively) expensive file-system scanning if at all possible.
    #
    # - On startup when there are no extensions enabled, scanning and synchronizing wheels
    #   adds unnecessary overhead. Especially considering this will run for automated tasks.
    # - When disabling an add-on from the UI, there may be no extensions enabled afterwards,
    #   however the extension that was disabled may have had wheels installed which must be removed,
    #   so in this case it's important not to skip synchronizing wheels, see: #125958.
    if use_startup_fastpath and (not extensions_enabled):
        if print_debug is not None:
            print_debug("no extensions, skipping cache data.")
        return

    # While this isn't expected to fail, any failure here is a bug
    # but it should not cause Blender's startup to fail.
    try:
        updated, wheel_list = _initialize_extensions_compat_ensure_up_to_date(
            extensions_directory,
            extensions_enabled,
            print_debug,
        )
    except Exception:
        print("Extension: unexpected error detecting cache, this is a bug!")
        import traceback
        traceback.print_exc()
        updated = False

    if ensure_wheels:
        if updated:
            if error_fn is None:
                def error_fn(ex):
                    print("Error:", str(ex))

            try:
                _initialize_extensions_compat_ensure_up_to_date_wheels(
                    extensions_directory,
                    wheel_list,
                    debug,
                    error_fn=error_fn,
                )
            except Exception:
                print("Extension: unexpected error updating wheels, this is a bug!")
                import traceback
                traceback.print_exc()


# -----------------------------------------------------------------------------
# Extension Utilities

def _version_int_left_digits(x):
    # Parse as integer until the first non-digit.
    return int(x[:next((i for i, c in enumerate(x) if not c.isdigit()), len(x))] or "0")


def _bl_info_from_extension(mod_name, mod_path):
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
    except Exception as ex:
        print("Error:", str(ex), "in", filepath_toml)
        return None, filepath_toml

    # This isn't a full validation which happens on package install/update.
    if (value := data.get("name", None)) is None:
        print("Error: missing \"name\" in", filepath_toml)
        return None, filepath_toml
    if type(value) is not str:
        print("Error: \"name\" is not a string in", filepath_toml)
        return None, filepath_toml
    bl_info["name"] = value

    if (value := data.get("version", None)) is None:
        print("Error: missing \"version\" in", filepath_toml)
        return None, filepath_toml
    if type(value) is not str:
        print("Error: \"version\" is not a string in", filepath_toml)
        return None, filepath_toml
    try:
        value = tuple(
            (int if i < 2 else _version_int_left_digits)(x)
            for i, x in enumerate(value.split(".", 2))
        )
    except Exception as ex:
        print("Error: \"version\" is not a semantic version (X.Y.Z) in ", filepath_toml, str(ex))
        return None, filepath_toml
    bl_info["version"] = value

    if (value := data.get("blender_version_min", None)) is None:
        print("Error: missing \"blender_version_min\" in", filepath_toml)
        return None, filepath_toml
    if type(value) is not str:
        print("Error: \"blender_version_min\" is not a string in", filepath_toml)
        return None, filepath_toml
    try:
        value = tuple(int(x) for x in value.split("."))
    except Exception as ex:
        print("Error:", str(ex), "in \"blender_version_min\"", filepath_toml)
        return None, filepath_toml
    bl_info["blender"] = value

    # Only print warnings since description is not a mandatory field.
    if (value := data.get("tagline", None)) is None:
        print("Warning: missing \"tagline\" in", filepath_toml)
    elif type(value) is not str:
        print("Warning: \"tagline\" is not a string", filepath_toml)
    else:
        bl_info["description"] = value

    if (value := data.get("maintainer", None)) is None:
        print("Error: missing \"author\" in", filepath_toml)
        return None, filepath_toml
    if type(value) is not str:
        print("Error: \"maintainer\" is not a string", filepath_toml)
        return None, filepath_toml
    bl_info["author"] = value

    bl_info["category"] = "Development"  # Dummy, will be removed.

    return bl_info, filepath_toml


def _fake_module_from_extension(mod_name, mod_path):
    import os

    bl_info, filepath_toml = _bl_info_from_extension(mod_name, mod_path)
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


def _extension_sync_wheels(
        *,
        local_dir,  # `str`
        wheel_list,  # `list[WheelSource]`
        debug,           # `bool`
        error_fn,  # `Callable[[Exception], None]`
):  # `-> None`
    import os
    import sys
    from _bpy_internal.extensions.wheel_manager import apply_action

    local_dir_site_packages = os.path.join(
        local_dir,
        "lib",
        "python{:d}.{:d}".format(*sys.version_info[0:2]),
        "site-packages",
    )

    paths_stale = []

    def remove_error_fn(filepath: str, _ex: Exception) -> None:
        paths_stale.append(filepath)

    apply_action(
        local_dir=local_dir,
        local_dir_site_packages=local_dir_site_packages,
        wheel_list=wheel_list,
        error_fn=error_fn,
        remove_error_fn=remove_error_fn,
        debug=debug,
    )

    if paths_stale:
        stale_pending_stage_paths(local_dir, paths_stale)

    if os.path.exists(local_dir_site_packages):
        if local_dir_site_packages not in sys.path:
            sys.path.append(local_dir_site_packages)


# -----------------------------------------------------------------------------
# Extensions

def _initialize_ensure_extensions_addon():
    module_name = "bl_pkg"
    if module_name not in _preferences.addons:
        enable(module_name, default_set=True, persistent=True)


# Module-like class, store singletons.
class _ext_global:
    __slots__ = ()

    # Store a map of `preferences.extensions.repos` -> `module_id`.
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


def _extension_module_name_decompose(package):
    # Returns the repository module name and the extensions ID from an extensions module name (``__package__``).
    #
    # :arg module_name: The extensions module name.
    # :type module_name: str
    # :return: (repo_module_name, extension_id)
    # :rtype: tuple[str, str]

    if not package.startswith(_ext_base_pkg_idname_with_dot):
        raise ValueError("The \"package\" does not name an extension")

    repo_module, pkg_idname = package[len(_ext_base_pkg_idname_with_dot):].partition(".")[0::2]
    if not (repo_module and pkg_idname):
        raise ValueError("The \"package\" is expected to be a module name containing 3 components")

    if "." in pkg_idname:
        raise ValueError("The \"package\" is expected to be a module name containing 3 components, found {:d}".format(
            pkg_idname.count(".") + 3
        ))

    # Unlikely but possible.
    if not (repo_module.isidentifier() and pkg_idname.isidentifier()):
        raise ValueError("The \"package\" contains non-identifier characters")

    return repo_module, pkg_idname


def _extension_preferences_idmap():
    repos_idmap = {}
    repos_idmap_disabled = {}
    for repo in _preferences.extensions.repos:
        if repo.enabled:
            repos_idmap[repo.as_pointer()] = repo.module
        else:
            repos_idmap_disabled[repo.as_pointer()] = repo.module
    return repos_idmap, repos_idmap_disabled


def _extension_dirpath_from_preferences():
    repos_dict = {}
    for repo in _preferences.extensions.repos:
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
        except Exception:
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
                module_name_next = "{:s}.{:s}.{:s}".format(_ext_base_pkg_idname, module_id, submodule_id)
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
            module_name_prev = "{:s}.{:s}.{:s}".format(_ext_base_pkg_idname, module_id_prev, submodule_id)
            module_name_next = "{:s}.{:s}.{:s}".format(_ext_base_pkg_idname, module_id_next, submodule_id)
            disable(module_name_prev, default_set=False, refresh_handled=True)
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
            module_name_next = "{:s}.{:s}.{:s}".format(_ext_base_pkg_idname, module_id_next, submodule_id)
            addon.module = module_name_next

    if submodules_del:
        repo_module_map = {repo.module: repo for repo in _preferences.extensions.repos}
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
                module_name_prev = "{:s}.{:s}.{:s}".format(_ext_base_pkg_idname, module_id, submodule_id)
                disable(module_name_prev, default_set=default_set, refresh_handled=True)
            del repo
        del repo_module_map

    if submodules_del_disabled:
        for module_id_prev in submodules_del_disabled:
            repo_userdef = addon_userdef_info.get(module_id_prev, {})
            for submodule_id in repo_userdef.keys():
                module_name_prev = "{:s}.{:s}.{:s}".format(_ext_base_pkg_idname, module_id_prev, submodule_id)
                disable(module_name_prev, default_set=True, refresh_handled=True)

    return addons_to_enable


# Enable add-ons after the modules have been manipulated.
def _initialize_extension_repos_post_addons_restore(addons_to_enable):
    if not addons_to_enable:
        return

    # Important to refresh wheels & compatibility data before enabling.
    extensions_refresh(addon_modules_pending=[module_name_next for (module_name_next, _, _, _) in addons_to_enable])

    any_failed = False
    for (module_name_next, addon, default_set, persistent) in addons_to_enable:
        # Ensure the preferences are kept.
        if addon is not None:
            addon.module = module_name_next
        if enable(module_name_next, default_set=default_set, persistent=persistent) is None:
            any_failed = True

    # Remove wheels for any add-ons that failed to enable.
    if any_failed:
        extensions_refresh()

    # Needed for module rename.
    _is_first_reset()


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

    # Mainly needed when the state of repositories changes at run-time:
    # factory settings then load preferences for example.
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
        # Lookup never fails, as the "next" values use: `preferences.extensions.repos`.
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
        _is_first_reset()


def _initialize_extensions_site_packages(*, extensions_directory, create=False):
    # Add extension site-packages to `sys.path` (if it exists).
    # Use for wheels.
    import os
    import sys

    # NOTE: follow the structure of `~/.local/lib/python#.##/site-packages`
    # because some wheels contain paths pointing to parent directories,
    # referencing `../../../bin` for example - to install binaries into `~/.local/bin`,
    # so this can't simply be treated as a module directory unless those files would be excluded
    # which may interfere with the wheels functionality.
    site_packages = os.path.join(
        extensions_directory,
        ".local",
        "lib",
        "python{:d}.{:d}".format(sys.version_info.major, sys.version_info.minor),
        "site-packages",
    )
    if create:
        if not os.path.exists(site_packages):
            os.makedirs(site_packages)
        found = True
    else:
        found = os.path.exists(site_packages)

    if found:
        # Ensure the wheels `site-packages` are added before all other site-packages.
        # This is important for extensions modules get priority over system modules.
        # Without this, installing a module into the systems site-packages (`/usr/lib/python#.##/site-packages`)
        # could break an extension which already had a different version of this module installed locally.
        from site import getsitepackages
        index = None
        if builtin_site_packages := set(getsitepackages()):
            for i, dirpath in enumerate(sys.path):
                if dirpath in builtin_site_packages:
                    index = i
                    break
        if index is None:
            sys.path.append(site_packages)
        else:
            sys.path.insert(index, site_packages)
    else:
        try:
            sys.path.remove(site_packages)
        except ValueError:
            pass

    return site_packages if found else None


def _initialize_extensions_repos_once():
    from _bpy_internal.extensions.junction_module import JunctionModuleHandle
    module_handle = JunctionModuleHandle(_ext_base_pkg_idname)
    module_handle.register_module()
    _ext_global.module_handle = module_handle

    extensions_directory = _bpy.utils.user_resource('EXTENSIONS')

    # Ensure extensions wheels can be loaded (when found).
    _initialize_extensions_site_packages(extensions_directory=extensions_directory)

    # Ensure extension compatibility data has been loaded and matches the manifests.
    _initialize_extensions_compat_data(
        extensions_directory,
        ensure_wheels=True,
        addon_modules_pending=None,
        use_startup_fastpath=True,
        # Runs on startup, fall back to printing.
        error_fn=None,
    )

    # Setup repositories for the first time.
    # Intentionally don't call `_initialize_extension_repos_pre` as this is the first time,
    # the previous state is not useful to read.
    _initialize_extension_repos_post(is_first=True)

    # Internal handlers intended for Blender's own handling of repositories.
    _bpy.app.handlers._extension_repos_update_pre.append(_initialize_extension_repos_pre)
    _bpy.app.handlers._extension_repos_update_post.append(_initialize_extension_repos_post)


# -----------------------------------------------------------------------------
# Extension Public API

def extensions_refresh(
        ensure_wheels=True,
        addon_modules_pending=None,
        handle_error=None,
):
    """
    Ensure data relating to extensions is up to date.
    This should be called after extensions on the file-system have changed.

    :arg ensure_wheels: When true, refresh installed wheels with wheels used by extensions.
    :type ensure_wheels: bool
    :arg addon_modules_pending: Refresh these add-ons by listing their package names, as if they are enabled.
       This is needed so wheels can be setup before the add-on is enabled.
    :type addon_modules_pending: Sequence[str] | None
    :arg handle_error: Called in the case of an error, taking an exception argument.
    :type handle_error: Callable[[Exception], None] | None
    """

    # Ensure any changes to extensions refresh `_extensions_incompatible`.
    _initialize_extensions_compat_data(
        _bpy.utils.user_resource('EXTENSIONS'),
        ensure_wheels=ensure_wheels,
        addon_modules_pending=addon_modules_pending,
        use_startup_fastpath=False,
        error_fn=handle_error,
    )


def _extensions_warnings_get():
    if _extensions_warnings_get._is_first is False:
        return _extensions_warnings

    # Calculate warnings which are shown in the UI but not calculated at load time
    # because this incurs some overhead.
    #
    # Currently this checks for scripts violating policies:
    # - Adding their directories or sub-directories to `sys.path`.
    # - Loading any bundled scripts as modules directly into `sys.modules`.
    #
    # These warnings are shown:
    # - In the add-on UI.
    # - In the extension UI.
    # - When listing extensions via `blender -c extension list`.

    import sys
    import os

    _extensions_warnings_get._is_first = False
    _extensions_warnings.clear()

    # This could be empty, it just avoid a lot of redundant lookups to skip known module paths.
    dirs_skip_expected = (
        os.path.normpath(os.path.join(os.path.dirname(_bpy.__file__), "..")) + os.sep,
        os.path.normpath(os.path.join(os.path.dirname(__import__("bl_ui").__file__), "..")) + os.sep,
        os.path.normpath(os.path.dirname(os.__file__)) + os.sep,
        # Legacy add add-on paths.
        *(os.path.normpath(path) + os.sep for path in paths()),
    )

    extensions_directory_map = {}
    modules_other = []

    for module_name, module in sys.modules.items():

        if module_name == "__main__":
            continue

        module_file = getattr(module, "__file__", None) or ""
        if not module_file:
            # In most cases these are PY-CAPI modules.
            continue

        module_file = os.path.normpath(module_file)

        if module_file.startswith(dirs_skip_expected):
            continue

        if module_name.startswith(_ext_base_pkg_idname_with_dot):
            # Check this is a sub-module (an extension).
            if module_name.find(".", len(_ext_base_pkg_idname_with_dot)) != -1:
                # Ignore extension sub-modules because there is no need to handle their directories.
                # The extensions directory accounts for any paths which may be found in the sub-modules path.
                if module_name.count(".") > 2:
                    continue
                extensions_directory_map[module_name] = os.path.dirname(module_file) + os.sep
        else:
            # Any non extension module.
            modules_other.append((module_name, module_file))

    dirs_extensions = tuple(path for path in extensions_directory_map.values())
    dirs_extensions_noslash = set(path.rstrip(os.sep) for path in dirs_extensions)
    if dirs_extensions:
        for module_other_name, module_other_file in modules_other:
            if not module_other_file.startswith(dirs_extensions):
                continue

            # Need 2x lookups, not ideal but `str.startswith` doesn't let us know which argument matched.
            found = False
            for module_name, module_dirpath in extensions_directory_map.items():
                if not module_other_file.startswith(module_dirpath):
                    continue
                try:
                    warning_list = _extensions_warnings[module_name]
                except KeyError:
                    warning_list = _extensions_warnings[module_name] = []
                warning_list.append("Policy violation with top level module: {:s}".format(module_other_name))
                found = True
                break
            assert found

        for path in sys.path:
            path = os.path.normpath(path)
            if path.startswith(dirs_skip_expected):
                continue

            if not (path in dirs_extensions_noslash or path.startswith(dirs_extensions)):
                continue

            found = False
            for module_name, module_dirpath in extensions_directory_map.items():
                if not (path == module_dirpath.rstrip(os.sep) or path.startswith(module_dirpath)):
                    continue
                try:
                    warning_list = _extensions_warnings[module_name]
                except KeyError:
                    warning_list = _extensions_warnings[module_name] = []
                # Use an extension relative path as an absolute path may be too verbose for the UI.
                warning_list.append(
                    "Policy violation with sys.path: {:s}".format(
                        ".{:s}{:s}".format(os.sep, os.path.relpath(path, module_dirpath))
                    )
                )
                found = True
                break
            assert found

    return _extensions_warnings


_extensions_warnings_get._is_first = True


def _is_first_reset():
    # Reset all values which are lazily initialized,
    # use this to force re-creating extension warnings and cached modules.
    _extensions_warnings_get._is_first = True
    modules._is_first = True
