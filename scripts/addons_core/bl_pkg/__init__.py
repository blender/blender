# SPDX-FileCopyrightText: 2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

bl_info = {
    "name": "Blender Extensions",
    "author": "Campbell Barton",
    "version": (0, 0, 1),
    "blender": (4, 0, 0),
    "location": "Edit -> Preferences -> Extensions",
    "description": "Extension repository support for remote repositories",
    "warning": "",
    # "doc_url": "{BLENDER_MANUAL_URL}/addons/bl_pkg/bl_pkg.html",
    "support": 'OFFICIAL',
    "category": "System",
}

if "bpy" in locals():
    import importlib
    from . import (
        bl_extension_ops,
        bl_extension_ui,
        bl_extension_utils,
    )
    importlib.reload(bl_extension_ops)
    importlib.reload(bl_extension_ui)
    importlib.reload(bl_extension_utils)
    del (
        bl_extension_ops,
        bl_extension_ui,
        bl_extension_utils,
    )
    del importlib

import bpy

from bpy.props import (
    BoolProperty,
    EnumProperty,
    IntProperty,
    StringProperty,
)

from bpy.types import (
    AddonPreferences,
)


class BlExtPreferences(AddonPreferences):
    bl_idname = __name__
    timeout: IntProperty(
        name="Time Out",
        default=10,
    )
    show_development_reports: BoolProperty(
        name="Show Development Reports",
        description=(
            "Show the result of running commands in the main interface "
            "this has the advantage that multiple processes that run at once have their errors properly grouped "
            "which is not the case for reports which are mixed together"
        ),
        default=False,
    )


class StatusInfoUI:
    __slots__ = (
        # The the title of the status/notification.
        "title",
        # The result of an operation.
        "log",
        # Set to true when running (via a modal operator).
        "running",
    )

    def __init__(self):
        self.log = []
        self.title = ""
        self.running = False

    def from_message(self, title, text):
        log_new = []
        for line in text.split("\n"):
            if not (line := line.rstrip()):
                continue
            # Don't show any prefix for "Info" since this is implied.
            log_new.append(('STATUS', line.removeprefix("Info: ")))
        if not log_new:
            return

        self.title = title
        self.running = False
        self.log = log_new


def cookie_from_session():
    # This path is a unique string for this session.
    # Don't use a constant as it may be changed at run-time.
    return bpy.app.tempdir


# -----------------------------------------------------------------------------
# Shared Low Level Utilities

def repo_paths_or_none(repo_item):
    if (directory := repo_item.directory) == "":
        return None, None
    if repo_item.use_remote_url:
        if not (remote_url := repo_item.remote_url):
            return None, None
    else:
        remote_url = ""
    return directory, remote_url


def repo_active_or_none():
    prefs = bpy.context.preferences
    extensions = prefs.extensions
    active_extension_index = extensions.active_repo
    try:
        active_repo = None if active_extension_index < 0 else extensions.repos[active_extension_index]
    except IndexError:
        active_repo = None
    return active_repo


def print_debug(*args, **kw):
    if not bpy.app.debug:
        return
    print(*args, **kw)


use_repos_to_notify = False


def repos_to_notify():
    import os
    from .bl_extension_utils import (
        scandir_with_demoted_errors,
        PKG_MANIFEST_FILENAME_TOML,
    )

    repos_notify = []
    if not bpy.app.background:
        # To use notifications on startup requires:
        # - The splash displayed.
        # - The status bar displayed.
        #
        # Since it's not all that common to disable the status bar just run notifications
        # if any repositories are marked to run notifications.

        online_access = bpy.app.online_access
        prefs = bpy.context.preferences
        extension_repos = prefs.extensions.repos
        for repo_item in extension_repos:
            if not repo_item.enabled:
                continue
            if not repo_item.use_sync_on_startup:
                continue
            if not repo_item.use_remote_url:
                continue
            remote_url = repo_item.remote_url
            # Invalid, if there is no remote path this can't update.
            if not remote_url:
                continue

            if online_access:
                # All URL's may be accessed.
                pass
            else:
                # Allow remote file-system repositories even when online access is disabled.
                if not remote_url.startswith("file://"):
                    continue

            # WARNING: this could be a more expensive check, use a "reasonable" guess.
            # This is technically incorrect because knowing if a repository has any installed
            # packages requires reading it's meta-data and comparing it with the directory contents.
            # Chances are - if the directory contains *any* directories containing a package manifest
            # this means it has packages installed.
            #
            # Simply check the repositories directory isn't empty (ignoring dot-files).
            # Importantly, this may be false positives but *not* false negatives.
            repo_is_empty = True
            repo_directory = repo_item.directory
            if os.path.isdir(repo_directory):
                for entry in scandir_with_demoted_errors(repo_directory):
                    if not entry.is_dir():
                        continue
                    if entry.name.startswith("."):
                        continue
                    if not os.path.exists(os.path.join(entry.path, PKG_MANIFEST_FILENAME_TOML)):
                        continue
                    repo_is_empty = False
                    break
            if repo_is_empty:
                continue

            repos_notify.append(repo_item)
    return repos_notify


# -----------------------------------------------------------------------------
# Handlers

@bpy.app.handlers.persistent
def extenion_repos_sync(*_):
    # This is called from operators (create or an explicit call to sync)
    # so calling a modal operator is "safe".
    if (active_repo := repo_active_or_none()) is None:
        return

    print_debug("SYNC:", active_repo.name)
    # There may be nothing to upgrade.

    from contextlib import redirect_stdout
    import io
    stdout = io.StringIO()

    with redirect_stdout(stdout):
        bpy.ops.bl_pkg.repo_sync_all('INVOKE_DEFAULT', use_active_only=True)

    if text := stdout.getvalue():
        repo_status_text.from_message("Sync \"{:s}\"".format(active_repo.name), text)


@bpy.app.handlers.persistent
def extenion_repos_upgrade(*_):
    # This is called from operators (create or an explicit call to sync)
    # so calling a modal operator is "safe".
    if (active_repo := repo_active_or_none()) is None:
        return

    print_debug("UPGRADE:", active_repo.name)

    from contextlib import redirect_stdout
    import io
    stdout = io.StringIO()

    with redirect_stdout(stdout):
        bpy.ops.bl_pkg.pkg_upgrade_all('INVOKE_DEFAULT', use_active_only=True)

    if text := stdout.getvalue():
        repo_status_text.from_message("Upgrade \"{:s}\"".format(active_repo.name), text)


@bpy.app.handlers.persistent
def extenion_repos_files_clear(directory, _):
    # Perform a "safe" file deletion by only removing files known to be either
    # packages or known extension meta-data.
    #
    # Safer because removing a repository which points to an arbitrary path
    # has the potential to wipe user data #119481.
    import shutil
    import os
    from .bl_extension_utils import (
        scandir_with_demoted_errors,
        PKG_MANIFEST_FILENAME_TOML,
    )
    # Unlikely but possible a new repository is immediately removed before initializing,
    # avoid errors in this case.
    if not os.path.isdir(directory):
        return

    if os.path.isdir(path := os.path.join(directory, ".blender_ext")):
        try:
            shutil.rmtree(path)
        except BaseException as ex:
            print("Failed to remove files", ex)

    for entry in scandir_with_demoted_errors(directory):
        if not entry.is_dir():
            continue
        path = entry.path
        if not os.path.exists(os.path.join(path, PKG_MANIFEST_FILENAME_TOML)):
            continue
        try:
            shutil.rmtree(path)
        except BaseException as ex:
            print("Failed to remove files", ex)


# -----------------------------------------------------------------------------
# Wrap Handlers

_monkeypatch_extenions_repos_update_dirs = set()


def monkeypatch_extenions_repos_update_pre_impl():
    _monkeypatch_extenions_repos_update_dirs.clear()

    extension_repos = bpy.context.preferences.extensions.repos
    for repo_item in extension_repos:
        if not repo_item.enabled:
            continue
        directory, _repo_path = repo_paths_or_none(repo_item)
        if directory is None:
            continue

        _monkeypatch_extenions_repos_update_dirs.add(directory)


def monkeypatch_extenions_repos_update_post_impl():
    import os
    from . import bl_extension_ops

    bl_extension_ops.repo_cache_store_refresh_from_prefs()

    # Refresh newly added directories.
    extension_repos = bpy.context.preferences.extensions.repos
    for repo_item in extension_repos:
        if not repo_item.enabled:
            continue
        directory, _repo_path = repo_paths_or_none(repo_item)
        if directory is None:
            continue
        # Happens for newly added extension directories.
        if not os.path.exists(directory):
            continue
        if directory in _monkeypatch_extenions_repos_update_dirs:
            continue
        # Ignore missing because the new repo might not have a JSON file.
        repo_cache_store.refresh_remote_from_directory(directory=directory, error_fn=print, force=True)
        repo_cache_store.refresh_local_from_directory(directory=directory, error_fn=print, ignore_missing=True)

    _monkeypatch_extenions_repos_update_dirs.clear()


@bpy.app.handlers.persistent
def monkeypatch_extensions_repos_update_pre(*_):
    print_debug("PRE:")
    try:
        monkeypatch_extenions_repos_update_pre_impl()
    except BaseException as ex:
        print_debug("ERROR", str(ex))
    try:
        monkeypatch_extensions_repos_update_pre._fn_orig()
    except BaseException as ex:
        print_debug("ERROR", str(ex))


@bpy.app.handlers.persistent
def monkeypatch_extenions_repos_update_post(*_):
    print_debug("POST:")
    try:
        monkeypatch_extenions_repos_update_post._fn_orig()
    except BaseException as ex:
        print_debug("ERROR", str(ex))
    try:
        monkeypatch_extenions_repos_update_post_impl()
    except BaseException as ex:
        print_debug("ERROR", str(ex))


def monkeypatch_install():
    import addon_utils

    handlers = bpy.app.handlers._extension_repos_update_pre
    fn_orig = addon_utils._initialize_extension_repos_pre
    fn_override = monkeypatch_extensions_repos_update_pre
    for i, fn in enumerate(handlers):
        if fn is fn_orig:
            handlers[i] = fn_override
            fn_override._fn_orig = fn_orig
            break

    handlers = bpy.app.handlers._extension_repos_update_post
    fn_orig = addon_utils._initialize_extension_repos_post
    fn_override = monkeypatch_extenions_repos_update_post
    for i, fn in enumerate(handlers):
        if fn is fn_orig:
            handlers[i] = fn_override
            fn_override._fn_orig = fn_orig
            break


def monkeypatch_uninstall():
    handlers = bpy.app.handlers._extension_repos_update_pre
    fn_override = monkeypatch_extensions_repos_update_pre
    for i in range(len(handlers)):
        fn = handlers[i]
        if fn is fn_override:
            handlers[i] = fn_override._fn_orig
            del fn_override._fn_orig
            break

    handlers = bpy.app.handlers._extension_repos_update_post
    fn_override = monkeypatch_extenions_repos_update_post
    for i in range(len(handlers)):
        fn = handlers[i]
        if fn is fn_override:
            handlers[i] = fn_override._fn_orig
            del fn_override._fn_orig
            break


# Text to display in the UI (while running...).
repo_status_text = StatusInfoUI()

# Singleton to cache all repositories JSON data and handles refreshing.
repo_cache_store = None


# -----------------------------------------------------------------------------
# Theme Integration

def theme_preset_draw(menu, context):
    from .bl_extension_utils import (
        pkg_theme_file_list,
    )
    layout = menu.layout
    repos_all = [
        repo_item for repo_item in context.preferences.extensions.repos
        if repo_item.enabled
    ]
    if not repos_all:
        return
    import os
    menu_idname = type(menu).__name__
    for i, pkg_manifest_local in enumerate(repo_cache_store.pkg_manifest_from_local_ensure(error_fn=print)):
        if pkg_manifest_local is None:
            continue
        repo_item = repos_all[i]
        directory = repo_item.directory
        for pkg_idname, value in pkg_manifest_local.items():
            if value["type"] != "theme":
                continue

            theme_dir, theme_files = pkg_theme_file_list(directory, pkg_idname)
            for filename in theme_files:
                props = layout.operator(menu.preset_operator, text=bpy.path.display_name(filename))
                props.filepath = os.path.join(theme_dir, filename)
                props.menu_idname = menu_idname


def cli_extension(argv):
    from . import bl_extension_cli
    return bl_extension_cli.cli_extension_handler(argv)


# -----------------------------------------------------------------------------
# Registration

classes = (
    BlExtPreferences,
)

cli_commands = []


def register():
    # pylint: disable-next=global-statement
    global repo_cache_store

    from bpy.types import WindowManager
    from . import (
        bl_extension_ops,
        bl_extension_ui,
        bl_extension_utils,
    )

    if repo_cache_store is None:
        repo_cache_store = bl_extension_utils.RepoCacheStore()
    else:
        repo_cache_store.clear()
    bl_extension_ops.repo_cache_store_refresh_from_prefs()

    for cls in classes:
        bpy.utils.register_class(cls)

    bl_extension_ops.register()
    bl_extension_ui.register()

    WindowManager.extension_search = StringProperty(
        name="Filter",
        description="Filter by extension name, author & category",
        options={'TEXTEDIT_UPDATE'},
    )
    WindowManager.extension_type = EnumProperty(
        items=(
            ('ADDON', "Add-ons", "Only show add-ons"),
            ('THEME', "Themes", "Only show themes"),
        ),
        name="Filter by Type",
        description="Show extensions by type",
        default='ADDON',
    )
    WindowManager.extension_enabled_only = BoolProperty(
        name="Show Enabled Extensions",
        description="Only show enabled extensions",
    )
    WindowManager.extension_updates_only = BoolProperty(
        name="Show Updates Available",
        description="Only show extensions with updates available",
    )
    WindowManager.extension_installed_only = BoolProperty(
        name="Show Installed Extensions",
        description="Only show installed extensions",
    )
    WindowManager.extension_show_legacy_addons = BoolProperty(
        name="Show Legacy Add-ons",
        description="Only show extensions, hiding legacy add-ons",
        default=True,
    )

    from bl_ui.space_userpref import USERPREF_MT_interface_theme_presets
    USERPREF_MT_interface_theme_presets.append(theme_preset_draw)

    handlers = bpy.app.handlers._extension_repos_sync
    handlers.append(extenion_repos_sync)

    handlers = bpy.app.handlers._extension_repos_upgrade
    handlers.append(extenion_repos_upgrade)

    handlers = bpy.app.handlers._extension_repos_files_clear
    handlers.append(extenion_repos_files_clear)

    cli_commands.append(bpy.utils.register_cli_command("extension", cli_extension))

    global use_repos_to_notify
    if (repos_notify := repos_to_notify()):
        use_repos_to_notify = True
        from . import bl_extension_notify
        bl_extension_notify.register(repos_notify)
    del repos_notify

    monkeypatch_install()


def unregister():
    # pylint: disable-next=global-statement
    global repo_cache_store

    from bpy.types import WindowManager
    from . import (
        bl_extension_ops,
        bl_extension_ui,
    )

    bl_extension_ops.unregister()
    bl_extension_ui.unregister()

    del WindowManager.extension_search
    del WindowManager.extension_type
    del WindowManager.extension_enabled_only
    del WindowManager.extension_installed_only
    del WindowManager.extension_show_legacy_addons

    for cls in classes:
        bpy.utils.unregister_class(cls)

    if repo_cache_store is None:
        pass
    else:
        repo_cache_store.clear()
        repo_cache_store = None

    from bl_ui.space_userpref import USERPREF_MT_interface_theme_presets
    USERPREF_MT_interface_theme_presets.remove(theme_preset_draw)

    handlers = bpy.app.handlers._extension_repos_sync
    if extenion_repos_sync in handlers:
        handlers.remove(extenion_repos_sync)

    handlers = bpy.app.handlers._extension_repos_upgrade
    if extenion_repos_upgrade in handlers:
        handlers.remove(extenion_repos_upgrade)

    handlers = bpy.app.handlers._extension_repos_files_clear
    if extenion_repos_files_clear in handlers:
        handlers.remove(extenion_repos_files_clear)

    for cmd in cli_commands:
        bpy.utils.unregister_cli_command(cmd)
    cli_commands.clear()

    global use_repos_to_notify
    if use_repos_to_notify:
        use_repos_to_notify = False
        from . import bl_extension_notify
        bl_extension_notify.unregister()

    monkeypatch_uninstall()
