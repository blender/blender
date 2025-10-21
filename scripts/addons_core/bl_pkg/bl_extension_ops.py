# SPDX-FileCopyrightText: 2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
Blender, thin wrapper around ``blender_extension_utils``.
Where the operator shows progress, any errors and supports canceling operations.
"""

__all__ = (
    "extension_repos_read",
    "pkg_wheel_filter",
)

import os

from functools import partial

from typing import (
    NamedTuple,
)

import bpy

from bpy.types import (
    Operator,
)
from bpy.props import (
    BoolProperty,
    CollectionProperty,
    EnumProperty,
    StringProperty,
    IntProperty,
)
from bpy.app.translations import (
    pgettext_iface as iface_,
    pgettext_tip as tip_,
    pgettext_rpt as rpt_,
)

from . import (
    cookie_from_session,
    repo_cache_store_ensure,
    repo_stats_calc,
    repo_status_text,
)

rna_prop_url = StringProperty(name="URL", subtype='FILE_PATH', options={'HIDDEN'})
rna_prop_directory = StringProperty(name="Repo Directory", subtype='DIR_PATH')
rna_prop_repo_index = IntProperty(name="Repo Index", default=-1)
rna_prop_remote_url = StringProperty(name="Repo URL", subtype='FILE_PATH')
rna_prop_pkg_id = StringProperty(name="Package ID")

rna_prop_enable_on_install = BoolProperty(
    name="Enable on Install",
    description="Enable after installing",
    default=True,
    # This is done as "enabling" has security implications (running code after the action).
    # Using the last value would mean an action that isn't expected to enable the extension might unintentionally do so.
    options={'SKIP_SAVE'}
)
rna_prop_enable_on_install_type_map = {
    "add-on": "Enable Add-on",
    "theme": "Set Current Theme",
}

_ext_base_pkg_idname = "bl_ext"
_ext_base_pkg_idname_with_dot = _ext_base_pkg_idname + "."


def url_append_defaults(url):
    import sys
    from .bl_extension_utils import url_append_query_for_blender
    return url_append_query_for_blender(
        url=url,
        blender_version=bpy.app.version,
        python_version=sys.version_info[:3],
    )


def url_normalize(url):
    # Ensure consistent use of `file://` so multiple representations aren't considered different repositories.
    # Currently this only makes changes for UNC paths on WIN32.
    import sys
    import re

    prefix = "file://"
    if url.startswith(prefix):
        if sys.platform == "win32":
            # Not a drive, e.g. `file:///C:/path`.
            path_lstrip = url[len(prefix):].lstrip("/")
            if re.match("[A-Za-z]:", path_lstrip) is None:
                # Ensure:
                # MS-Edge uses: `file://HOST/share/path`
                # Firefox uses: `file://///HOST/share/path`
                # Both can work prefer the shorter one.
                url = prefix + path_lstrip
    return url


def rna_prop_repo_enum_valid_only_itemf(_self, context):
    if context is None:
        result = []
    else:
        # Split local/remote (local is always first) because
        # installing into a remote - while supported is more of a corner case.
        result = []
        repos_valid = list(repo_iter_valid_only(context, exclude_remote=False, exclude_system=True))
        # The UI-list sorts alphabetically, do the same here.
        repos_valid.sort(key=lambda repo_item: repo_item.name.casefold())
        has_local = False
        has_remote = False
        for repo_item in repos_valid:
            if repo_item.use_remote_url:
                has_remote = True
                continue
            has_local = True
            result.append((repo_item.module, repo_item.name, "", 'DISK_DRIVE', len(result)))
        if has_remote:
            if has_local:
                result.append(None)

            for repo_item in repos_valid:
                if not repo_item.use_remote_url:
                    continue
                result.append((repo_item.module, repo_item.name, "", 'INTERNET', len(result)))

    # Prevent the strings from being freed.
    rna_prop_repo_enum_valid_only_itemf.result = result
    return result


def repo_lookup_by_index_or_none(index):
    extensions = bpy.context.preferences.extensions
    extension_repos = extensions.repos
    try:
        return extension_repos[index]
    except IndexError:
        return None


def repo_lookup_by_index_or_none_with_report(index, report_fn):
    result = repo_lookup_by_index_or_none(index)
    if result is None:
        report_fn({'WARNING'}, "Called with invalid index")
    return result


def repo_user_directory(repo_module_name):
    path = bpy.utils.user_resource('EXTENSIONS')
    # Technically possible this is empty but in practice never happens.
    if path:
        path = os.path.join(path, ".user", repo_module_name)
    return path


is_background = bpy.app.background

# Execute tasks concurrently.
is_concurrent = True

# Selected check-boxes.
blender_extension_mark = set()
blender_extension_show = set()


# Map the enum value to the value in the manifest.
blender_filter_by_type_map = {
    "ALL": "",
    "ADDON": "add-on",
    "KEYMAP": "keymap",
    "THEME": "theme",
}


# -----------------------------------------------------------------------------
# Signal Context Manager (Catch Control-C)
#


class CheckSIGINT_Context:
    __slots__ = (
        "has_interrupt",
        "_old_fn",
    )

    def _signal_handler_sigint(self, _, __):
        self.has_interrupt = True
        print("INTERRUPT")

    def __init__(self):
        self.has_interrupt = False
        self._old_fn = None

    def __enter__(self):
        import signal
        self._old_fn = signal.signal(signal.SIGINT, self._signal_handler_sigint)
        return self

    def __exit__(self, _ty, _value, _traceback):
        import signal
        signal.signal(signal.SIGINT, self._old_fn or signal.SIG_DFL)


# -----------------------------------------------------------------------------
# Operator Notify State
#
# Support for

class OperatorNonBlockingSyncHelper:
    __slots__ = (
        "repo_name",
        "started",
        "completed",
    )

    def __init__(self, *, repo_name):
        self.repo_name = repo_name
        self.started = False
        self.completed = False

    def begin(self, region):
        assert self.started is False
        self.started = True

        from .bl_extension_notify import (
            update_ui_region_register,
            update_non_blocking,
        )

        repos_all = extension_repos_read()
        if self.repo_name:
            repos_notify = [repo for repo in repos_all if repo.name == self.repo_name]
        else:
            repos_notify = [repo for repo in repos_all if repo.remote_url]

        if not repos_notify:
            self.completed = True
            return

        update_ui_region_register(region)

        update_non_blocking(repos_fn=lambda: [(repo, True) for repo in repos_notify], immediate=True)

        # Redraw to get the updated notify text, even if it is just to say "Starting...".
        region.tag_redraw()
        region.tag_refresh_ui()

    def draw(self, context, op):
        region = context.region_popup
        from .bl_extension_notify import (
            update_ui_text,
            update_in_progress,
            update_ui_region_unregister,
        )
        if not self.started:
            self.begin(region)
            return

        if not update_in_progress():
            # No updates in progress, show the actual UI.
            update_ui_region_unregister(region)
            region.tag_redraw()
            region.tag_refresh_ui()
            self.completed = True
            return

        layout = op.layout
        text, icon = update_ui_text()
        layout.label(text=text, icon=icon)
        # Only show a "Cancel" button while the update is in progress.
        layout.template_popup_confirm("", text="", cancel_text="Cancel")


# -----------------------------------------------------------------------------
# Internal Utilities
#

def _extensions_repo_temp_files_make_stale(
        repo_directory,  # `str`
):  # `-> None`

    # NOTE: this function should run after any operation
    # which may have attempted to remove a directory but only successfully renamed it.
    # The extension sub-process could communicate this back to this process but it's
    # reasonably involved and only avoids a repository file-system scan after each operation.
    # Scan repository directories and clear files with a specific prefix & suffix.
    import addon_utils
    from .bl_extension_utils import (
        scandir_with_demoted_errors,
        PKG_TEMP_PREFIX_AND_SUFFIX,
    )

    paths_stale = []

    prefix, suffix = PKG_TEMP_PREFIX_AND_SUFFIX
    for entry in scandir_with_demoted_errors(repo_directory):
        filename = entry.name
        if not filename.startswith(prefix):
            continue
        # Check the `filename` ends with `suffix` or suffix & digits `suffix123`.
        i = filename.rfind(suffix)
        if i == -1:
            continue
        ext_end = filename[i + len(suffix):]
        if ext_end and (not ext_end.isdigit()):
            continue

        paths_stale.append(os.path.join(repo_directory, filename))

    if not paths_stale:
        return

    addon_utils.stale_pending_stage_paths(repo_directory, paths_stale)


def _extensions_repo_uninstall_stale_package_fallback(
        repo_directory,  # `str`
        pkg_id_sequence,  # `list[str]`
):  # `-> None`
    # If uninstall failed, make the package stale (by renaming it & queue to remove later).
    import addon_utils

    paths_stale = []
    for pkg_id in pkg_id_sequence:
        path_abs = os.path.join(repo_directory, pkg_id)
        if not os.path.exists(path_abs):
            continue
        paths_stale.append(path_abs)

    if not paths_stale:
        return

    addon_utils.stale_pending_stage_paths(repo_directory, paths_stale)


def _extensions_repo_install_stale_package_clear(
        repo_directory,  # `str`
        pkg_id_sequence,  # `list[str]`
):  # `-> None`
    # If install succeeds, ensure the package is not stale.
    #
    # This can happen when a package fails to remove (if one of it's files are locked),
    # it is queued for removal. Then the user successfully removes it & re-installs in.
    # In this case the package will be tagged for later removal, so ensure it's removed.
    import addon_utils

    paths_not_stale = []
    for pkg_id in pkg_id_sequence:
        path_abs = os.path.join(repo_directory, pkg_id)
        if not os.path.exists(path_abs):
            continue
        paths_not_stale.append(path_abs)

    if not paths_not_stale:
        return

    addon_utils.stale_pending_remove_paths(repo_directory, paths_not_stale)


def _sequence_split_with_job_limit(items, job_limit):
    # When only one job is allowed at a time, there is no advantage to splitting the sequence.
    if job_limit == 1:
        return (items,)
    return [(elem,) for elem in items]


def _preferences_repo_find_by_remote_url(context, remote_url):
    remote_url = remote_url.rstrip("/")
    prefs = context.preferences
    extension_repos = prefs.extensions.repos
    for i, repo in enumerate(extension_repos):
        if repo.use_remote_url and repo.remote_url.rstrip("/") == remote_url:
            return repo, i
    return None, -1


def extension_url_find_repo_index_and_pkg_id(url):
    from .bl_extension_utils import (
        pkg_manifest_archive_url_abs_from_remote_url,
    )
    # return repo_index, pkg_id

    # NOTE: we might want to use `urllib.parse.urlsplit` so it's possible to include variables in the URL.
    url_basename = url.rpartition("/")[2]

    repos_all = extension_repos_read()
    repo_cache_store = repo_cache_store_ensure()

    # Regarding `ignore_missing`, set to True, otherwise a user-repository
    # or a new repository that has not yet been initialize will report errors.
    # It's OK to silently ignore these.

    for repo_index, (
            pkg_manifest_local,
            pkg_manifest_remote,
    ) in enumerate(zip(
        repo_cache_store.pkg_manifest_from_local_ensure(error_fn=print, ignore_missing=True),
        repo_cache_store.pkg_manifest_from_remote_ensure(error_fn=print, ignore_missing=True),
        strict=True,
    )):
        # It's possible the remote repo could not be connected to when syncing.
        # Allow it to be None without raising an exception.
        if pkg_manifest_remote is None:
            continue

        repo = repos_all[repo_index]
        remote_url = repo.remote_url
        if not remote_url:
            continue
        for pkg_id, item_remote in pkg_manifest_remote.items():
            archive_url = item_remote.archive_url
            archive_url_basename = archive_url.rpartition("/")[2]
            # First compare the filenames, if this matches, check the full URL.
            if url_basename != archive_url_basename:
                continue

            # Calculate the absolute URL.
            archive_url_abs = pkg_manifest_archive_url_abs_from_remote_url(remote_url, archive_url)
            if archive_url_abs == url:
                return repo_index, repo.name, pkg_id, item_remote, pkg_manifest_local.get(pkg_id)

    return -1, "", "", None, None


def online_user_agent_from_blender():
    # NOTE: keep this brief and avoid `platform.platform()` which could identify individual users.
    # Produces something like this: `Blender/4.2.0 (Linux x86_64; cycle=alpha)` or similar.
    import platform
    return "Blender/{:d}.{:d}.{:d} ({:s} {:s}; cycle={:s})".format(
        *bpy.app.version,
        platform.system(),
        platform.machine(),
        bpy.app.version_cycle,
    )


def lock_result_any_failed_with_report(op, lock_result, report_type='ERROR'):
    """
    Convert any locking errors from ``bl_extension_utils.RepoLock.acquire`` into reports.

    Note that we might want to allow some repositories not to lock and still proceed (in the future).
    """
    any_errors = False

    # Hint for users as this is non-obvious, only show once.
    unlock_hint_text = (
        "\n"
        "If the lock was held by a Blender instance that exited unexpectedly,\n"
        "use: \"Force Unlock Repository\" to clear the lock.\n"
        "Access from the \"Repositories\" popover in the extensions preferences."
    )

    for directory, lock_result_for_repo in lock_result.items():
        if lock_result_for_repo is None:
            continue

        # NOTE: the lock operation could also store the repository names however it's a reasonable
        # amount of added boiler plate for minimal gain. Do a lookup here instead.
        # The chance the name can't be found is low: A repositories directory could have
        # been changed since the lock was requested, in practice it shouldn't happen.
        # If it does, using a fallback name is acceptable.
        repo_name = next(
            (repo.name for repo in extension_repos_read() if repo.directory == directory),
            "<unknown>",
        )

        print("Error locking repository \"{:s}\": {:s}".format(repo_name, lock_result_for_repo))
        op.report(
            {report_type},
            rpt_("Repository \"{:s}\": {:s}{:s}").format(
                repo_name,
                lock_result_for_repo,
                "" if any_errors else unlock_hint_text,
            ),
        )

        any_errors = True
    return any_errors


def pkg_info_check_exclude_filter_ex(name, tagline, search_casefold):
    return (
        (search_casefold in name.casefold() or search_casefold in iface_(name).casefold()) or
        (search_casefold in tagline.casefold() or search_casefold in iface_(tagline).casefold())
    )


def pkg_info_check_exclude_filter(item, search_casefold):
    return pkg_info_check_exclude_filter_ex(item.name, item.tagline, search_casefold)


def extension_theme_enable_filepath(filepath):
    bpy.ops.script.execute_preset(
        filepath=filepath,
        menu_idname="USERPREF_MT_interface_theme_presets",
    )


def extension_theme_enable(repo_directory, pkg_idname):
    from .bl_extension_utils import (
        pkg_theme_file_list,
    )
    # Enable the theme.
    theme_dir, theme_files = pkg_theme_file_list(repo_directory, pkg_idname)

    # NOTE: a theme package can contain multiple themes, in this case just use the first
    # as the list is sorted and picking any theme is arbitrary if there are multiple.
    if not theme_files:
        return

    extension_theme_enable_filepath(os.path.join(theme_dir, theme_files[0]))


def repo_iter_valid_only(context, *, exclude_remote, exclude_system):
    from . import repo_paths_or_none
    extension_repos = context.preferences.extensions.repos
    for repo_item in extension_repos:
        if not repo_item.enabled:
            continue
        if exclude_remote:
            if repo_item.use_remote_url:
                continue
        if exclude_system:
            if (not repo_item.use_remote_url) and (repo_item.source == 'SYSTEM'):
                continue
        # Ignore repositories that have invalid settings.
        directory, _remote_url = repo_paths_or_none(repo_item)
        if directory is None:
            continue
        yield repo_item


def wm_wait_cursor(value):
    for wm in bpy.data.window_managers:
        for window in wm.windows:
            if value:
                window.cursor_modal_set('WAIT')
            else:
                window.cursor_modal_restore()


def operator_finished_result(operator_result):
    # Inspect results for modal operator, return None when the result isn't known.
    if 'CANCELLED' in operator_result:
        return True
    if 'FINISHED' in operator_result:
        return False
    return None


def pkg_manifest_params_compatible_or_error_for_this_system(
    *,
    blender_version_min,  # `str`
    blender_version_max,  # `str`
    platforms,  # `list[str]`
    python_versions,  # `list[str]`
):  # `str | None`
    # Return true if the parameters are compatible with this system.
    import sys
    from .bl_extension_utils import (
        pkg_manifest_params_compatible_or_error,
        platform_from_this_system,
    )
    return pkg_manifest_params_compatible_or_error(
        # Parameters.
        blender_version_min=blender_version_min,
        blender_version_max=blender_version_max,
        platforms=platforms,
        python_versions=python_versions,
        # This system.
        this_platform=platform_from_this_system(),
        this_python_version=sys.version_info,
        this_blender_version=bpy.app.version,
        error_fn=print,
    )


# A named-tuple copy of `context.preferences.extensions.repos` (`bpy.types.UserExtensionRepo`).
# This is done for the following reasons.
#
# - Booleans `use_remote_url` & `use_access_token` have been "applied", so every time `remote_url`
#   is accessed there is no need to check `use_remote_url` first (same for access tokens).
#
# - When checking for updates in the background, it's possible the repository is freed between
#   starting a check for updates and when the check runs. Using a copy means there is no risk
#   accessing freed memory & crashing, although these cases still need to be handled logically
#   even if the crashes are avoided.
#
# - In practically all cases this data is read-only when used via package management.
#   A named tuple makes that explicit.
#
class RepoItem(NamedTuple):
    name: str
    directory: str
    source: str
    remote_url: str
    module: str
    use_cache: bool
    access_token: str


def repo_cache_store_refresh_from_prefs(repo_cache_store, include_disabled=False):
    from . import repo_paths_or_none
    extension_repos = bpy.context.preferences.extensions.repos
    repos = []
    for repo_item in extension_repos:
        if not include_disabled:
            if not repo_item.enabled:
                continue
        directory, remote_url = repo_paths_or_none(repo_item)
        if directory is None:
            continue
        repos.append((directory, remote_url))

    repo_cache_store.refresh_from_repos(repos=repos)
    # Return the repository directory & URL's as it can be useful to know which repositories are now available.
    # NOTE: it might be better to return a list of `RepoItem`, for now it's not needed.
    return repos


def _preferences_pkg_id_sequence_filter_enabled(
        repo_item,  # `RepoItem`
        pkg_id_sequence,  # `list[str]`
):  # `-> list[str]`
    import addon_utils
    result = []

    module_base_elem = (_ext_base_pkg_idname, repo_item.module)

    for pkg_id in pkg_id_sequence:
        addon_module_elem = (*module_base_elem, pkg_id)
        addon_module_name = ".".join(addon_module_elem)
        loaded_default, loaded_state = addon_utils.check(addon_module_name)

        if not (loaded_default or loaded_state):
            continue

        result.append(pkg_id)

    return result


def _preferences_ensure_disabled(
        *,
        repo_item,  # `RepoItem`
        pkg_id_sequence,  # `list[str]`
        default_set,  # `bool`
        error_fn,  # `Callable[[Exception], None]`
):  # `-> dict[str, tuple[boo, bool]]`
    import sys
    import addon_utils

    result = {}

    modules_clear = []

    module_base_elem = (_ext_base_pkg_idname, repo_item.module)

    repo_module = sys.modules.get(".".join(module_base_elem))
    if repo_module is None:
        print("Repo module \"{:s}\" not in \"sys.modules\", unexpected!".format(".".join(module_base_elem)))

    for pkg_id in pkg_id_sequence:
        addon_module_elem = (*module_base_elem, pkg_id)
        addon_module_name = ".".join(addon_module_elem)
        loaded_default, loaded_state = addon_utils.check(addon_module_name)

        result[addon_module_name] = loaded_default, loaded_state

        # Not loaded or default, skip.
        if not (loaded_default or loaded_state):
            continue

        # This report isn't needed, it just shows a warning in the case of irregularities
        # which may be useful when debugging issues.
        if repo_module is not None:
            if not hasattr(repo_module, pkg_id):
                print("Repo module \"{:s}.{:s}\" not a sub-module!".format(".".join(module_base_elem), pkg_id))

        addon_utils.disable(
            addon_module_name,
            default_set=default_set,
            refresh_handled=True,
            handle_error=error_fn,
        )

        modules_clear.append(pkg_id)

    # Clear modules.

    # Extensions, repository & final `.` to ensure the module is part of the repository.
    prefix_base = ".".join(module_base_elem) + "."
    # Needed for `startswith` check.
    prefix_addon_modules = {prefix_base + pkg_id for pkg_id in modules_clear}
    # Needed for `startswith` check (sub-modules).
    prefix_addon_modules_base = tuple(module + "." for module in prefix_addon_modules)

    # NOTE(@ideasman42): clearing the modules is not great practice,
    # however we need to ensure this is fully un-loaded then reloaded.
    for key in list(sys.modules.keys()):
        if not key.startswith(prefix_base):
            continue
        if not (
                # This module is the add-on.
                key in prefix_addon_modules or
                # This module is a sub-module of the add-on.
                key.startswith(prefix_addon_modules_base)
        ):
            continue

        # Use pop instead of del because there is a (very) small chance
        # that classes defined in a removed module define a `__del__` method manipulates modules.
        sys.modules.pop(key, None)

    # Now remove from the module from it's parent (when found).
    # Although in most cases this isn't needed because disabling the add-on typically deletes the module,
    # don't report a warning if this is the case.
    if repo_module is not None:
        for pkg_id in pkg_id_sequence:
            if not hasattr(repo_module, pkg_id):
                continue
            delattr(repo_module, pkg_id)

    return result


def _preferences_ensure_enabled(*, repo_item, pkg_id_sequence, result, handle_error):
    import addon_utils
    _ = repo_item, pkg_id_sequence
    for addon_module_name, (loaded_default, loaded_state) in result.items():
        # The module was not loaded, so no need to restore it.
        if not loaded_state:
            continue

        addon_utils.enable(
            addon_module_name,
            default_set=loaded_default,
            refresh_handled=True,
            handle_error=handle_error,
        )


def _preferences_ensure_enabled_all(*, addon_restore, handle_error):
    for repo_item, pkg_id_sequence, result in addon_restore:
        _preferences_ensure_enabled(
            repo_item=repo_item,
            pkg_id_sequence=pkg_id_sequence,
            result=result,
            handle_error=handle_error,
        )


def _preferences_install_post_enable_on_install(
        *,
        directory,
        pkg_manifest_local,
        pkg_id_sequence,
        # There were already installed and an attempt to enable it will have already been made.
        pkg_id_sequence_upgrade,
        handle_error,
):
    import addon_utils

    # It only ever makes sense to enable one theme.
    has_theme = False

    repo_item = _extensions_repo_from_directory(directory)
    for pkg_id in pkg_id_sequence:
        item_local = pkg_manifest_local.get(pkg_id)
        if item_local is None:
            # Unlikely but possible, do nothing in this case.
            print("Package should have been installed but not found:", pkg_id)
            return

        if item_local.type == "add-on":
            # Check if the add-on will have been enabled from re-installing.
            if pkg_id in pkg_id_sequence_upgrade:
                continue

            addon_module_name = "{:s}.{:s}.{:s}".format(_ext_base_pkg_idname, repo_item.module, pkg_id)
            addon_utils.enable(
                addon_module_name,
                default_set=True,
                # Handled by `_extensions_repo_sync_wheels`.
                refresh_handled=True,
                handle_error=handle_error,
            )
        elif item_local.type == "theme":
            if has_theme:
                continue
            extension_theme_enable(directory, pkg_id)
            has_theme = True


def _preferences_ui_redraw():
    for win in bpy.context.window_manager.windows:
        for area in win.screen.areas:
            if area.type != 'PREFERENCES':
                continue
            area.tag_redraw()


def _preferences_ui_refresh_addons():
    import addon_utils
    # TODO: make a public method.
    # pylint: disable-next=protected-access
    addon_utils._is_first_reset()


def extension_repos_read_index(index, *, include_disabled=False):
    from . import repo_paths_or_none
    extension_repos = bpy.context.preferences.extensions.repos
    index_test = 0
    for repo_item in extension_repos:
        if not include_disabled:
            if not repo_item.enabled:
                continue
        directory, remote_url = repo_paths_or_none(repo_item)
        if directory is None:
            continue

        if index == index_test:
            return RepoItem(
                name=repo_item.name,
                directory=directory,
                source="" if repo_item.use_remote_url else repo_item.source,
                remote_url=remote_url,
                module=repo_item.module,
                use_cache=repo_item.use_cache,
                access_token=repo_item.access_token if repo_item.use_access_token else "",
            )
        index_test += 1
    return None


def extension_repos_read(*, include_disabled=False, use_active_only=False):
    from . import repo_paths_or_none
    extensions = bpy.context.preferences.extensions
    extension_repos = extensions.repos
    result = []

    if use_active_only:
        try:
            extension_active = extension_repos[extensions.active_repo]
        except IndexError:
            return result

        extension_repos = [extension_active]
        del extension_active

    for repo_item in extension_repos:
        if not include_disabled:
            if not repo_item.enabled:
                continue

        # Ignore repositories that have invalid settings.
        directory, remote_url = repo_paths_or_none(repo_item)
        if directory is None:
            continue

        result.append(RepoItem(
            name=repo_item.name,
            directory=directory,
            source="" if repo_item.use_remote_url else repo_item.source,
            remote_url=remote_url,
            module=repo_item.module,
            use_cache=repo_item.use_cache,
            access_token=repo_item.access_token if repo_item.use_access_token else "",
        ))
    return result


def _extension_repos_index_from_directory(directory):
    directory = os.path.normpath(directory)
    repos_all = extension_repos_read()
    for i, repo_item in enumerate(repos_all):
        if os.path.normpath(repo_item.directory) == directory:
            return i
    if os.path.exists(directory):
        for i, repo_item in enumerate(repos_all):
            if os.path.normpath(repo_item.directory) == directory:
                return i
    return -1


def _extensions_repo_from_directory(directory):
    repos_all = extension_repos_read()
    repo_index = _extension_repos_index_from_directory(directory)
    if repo_index == -1:
        return None
    return repos_all[repo_index]


def _extensions_repo_from_directory_and_report(directory, report_fn):
    if not directory:
        report_fn({'ERROR', "Directory not set"})
        return None

    repo_item = _extensions_repo_from_directory(directory)
    if repo_item is None:
        report_fn({'ERROR'}, "Directory has no repo entry: {:s}".format(directory))
        return None
    return repo_item


def _pkg_marked_by_repo(repo_cache_store, pkg_manifest_all):
    # NOTE: pkg_manifest_all can be from local or remote source.
    from .bl_extension_ui import ExtensionUI_Visibility

    ui_visibility = None if is_background else ExtensionUI_Visibility(bpy.context, repo_cache_store)

    repo_pkg_map = {}
    for pkg_id, repo_index in blender_extension_mark:
        if (pkg_manifest := pkg_manifest_all[repo_index]) is None:
            continue

        if ui_visibility is not None:
            if not ui_visibility.test((pkg_id, repo_index)):
                continue
        else:
            # Background mode, just to a simple range check.
            # While this should be prevented, any marked packages out of the range will cause problems, skip them.
            if repo_index >= len(pkg_manifest_all):
                continue
            if (pkg_manifest := pkg_manifest_all[repo_index]) is None:
                continue

        item = pkg_manifest.get(pkg_id)
        if item is None:
            continue

        pkg_list = repo_pkg_map.get(repo_index)
        if pkg_list is None:
            pkg_list = repo_pkg_map[repo_index] = []
        pkg_list.append(pkg_id)
    return repo_pkg_map


# -----------------------------------------------------------------------------
# Wheel Handling
#

def _extensions_wheel_filter_for_this_system(wheels):

    # Copied from `wheel.bwheel_dist.get_platform(..)` which isn't part of Python.
    # This misses some additional checks which aren't supported by official Blender builds,
    # it's highly doubtful users ever run into this but we could add extend this if it's really needed.
    # (e.g. `linux-i686` on 64 bit systems & `linux-armv7l`).
    import sysconfig

    # When false, suppress printing for incompatible wheels.
    # This generally isn't a problem as it's common for an extension to include wheels for multiple platforms.
    # Printing is mainly useful when installation fails because none of the wheels are compatible.
    debug = bpy.app.debug_python

    platform_tag_current = sysconfig.get_platform().replace("-", "_")

    import sys
    from .bl_extension_utils import (
        python_versions_from_wheel_python_tag,
        python_versions_from_wheel_abi_tag,
    )

    python_version_current = sys.version_info[:2]

    # https://packaging.python.org/en/latest/specifications/binary-distribution-format/#file-name-convention
    # This also defines the name spec:
    # `{distribution}-{version}(-{build tag})?-{python tag}-{abi tag}-{platform tag}.whl`

    wheels_compatible = []
    for wheel in wheels:
        wheel_filename = wheel.rsplit("/", 1)[-1]

        # Handled by validation (paranoid).
        if not wheel_filename.lower().endswith(".whl"):
            print("Error: wheel doesn't end with \".whl\", skipping!")
            continue

        wheel_filename_split = wheel_filename[:-4].split("-")
        # Skipping, should never happen as validation will fail,
        # keep paranoid check although this might be removed in the future.
        # pylint: disable-next=superfluous-parens
        if not (5 <= len(wheel_filename_split) <= 6):
            print("Error: wheel doesn't follow naming spec \"{:s}\"".format(wheel_filename))
            continue

        python_tag, abi_tag, platform_tag = wheel_filename_split[-3:]

        # Perform Platform Checks.
        if platform_tag in {"any", platform_tag_current}:
            pass
        elif platform_tag_current.startswith("macosx_") and (
                # FIXME: `macosx_11.00` should be `macosx_11_0`.
                platform_tag.startswith("macosx_") and
                # Ignore the MACOSX version, ensure `arm64` suffix.
                (platform_tag.endswith("_" + platform_tag_current.rpartition("_")[2]) or
                 platform_tag.endswith("_universal2"))
        ):
            pass
        elif platform_tag_current.startswith("linux_") and (
                # May be `manylinux1` or `manylinux2010`.
                platform_tag.startswith("manylinux") and
                # Match against the architecture: `linux_x86_64` -> `_x86_64` (ensure the same suffix).
                # The GLIBC version is ignored because it will often be older.
                # Although we will probably want to detect incompatible GLIBC versions eventually.
                platform_tag.endswith("_" + platform_tag_current.partition("_")[2])
        ):
            pass
        else:
            if debug:
                print(
                    "Skipping wheel for other system",
                    "({:s} != {:s}):".format(platform_tag, platform_tag_current),
                    wheel_filename,
                )
            continue

        # Perform Python Version Checks.
        if isinstance(python_versions := python_versions_from_wheel_python_tag(python_tag), str):
            print("Error: wheel \"{:s}\" unable to parse Python version {:s}".format(wheel_filename, python_versions))
        else:
            python_version_is_compat = False
            for python_version in python_versions:
                if len(python_version) == 1:
                    if python_version_current[0] == python_version[0]:
                        python_version_is_compat = True
                        break
                else:
                    if python_version_current == python_version:
                        python_version_is_compat = True
                        break

                    # When there is a stable ABI: Allow an older Python wheel to be compatible
                    # with a newer Python as long as the older wheel uses the stable ABI, see:
                    # https://packaging.python.org/en/latest/specifications/platform-compatibility-tags/#abi-tag
                    if isinstance(
                            python_versions_stable_abi := python_versions_from_wheel_abi_tag(abi_tag, stable_only=True),
                            str,
                    ):
                        print("Error: wheel \"{:s}\" unable to parse Python ABI version {:s}".format(
                            wheel_filename, python_versions,
                        ))
                    elif (python_version_current[0],) in python_versions_stable_abi:
                        if python_version_current >= python_version:
                            python_version_is_compat = True
                            break

            if not python_version_is_compat:
                if debug:
                    print(
                        "Skipping wheel for other Python version",
                        "({:s}=>({:s}) not in {:d}.{:d}):".format(
                            python_tag,
                            ", ".join([".".join(str(i) for i in v) for v in python_versions]),
                            python_version_current[0],
                            python_version_current[1],
                        ),
                        wheel_filename,
                    )
                continue

        wheels_compatible.append(wheel)

    return wheels_compatible


def pkg_wheel_filter(
        repo_module,  # `str`
        pkg_id,  # `str`
        repo_directory,  # `str`
        wheels_rel,  # `list[str]`
):  # `-> tuple[str, list[str]]`
    # Filter only the wheels for this platform.
    wheels_rel = _extensions_wheel_filter_for_this_system(wheels_rel)
    if not wheels_rel:
        return None

    pkg_dirpath = os.path.join(repo_directory, pkg_id)

    wheels_abs = []
    for filepath_rel in wheels_rel:
        filepath_abs = os.path.join(pkg_dirpath, filepath_rel)
        if not os.path.exists(filepath_abs):
            continue
        wheels_abs.append(filepath_abs)

    if not wheels_abs:
        return None

    unique_pkg_id = "{:s}.{:s}".format(repo_module, pkg_id)
    return (unique_pkg_id, wheels_abs)


def _extension_repos_directory_to_module_map():
    return {repo.directory: repo.module for repo in bpy.context.preferences.extensions.repos if repo.enabled}


def _extensions_enabled():
    from addon_utils import check_extension
    extensions_enabled = set()
    extensions_prefix_len = len(_ext_base_pkg_idname_with_dot)
    for addon in bpy.context.preferences.addons:
        module_name = addon.module
        if check_extension(module_name):
            extensions_enabled.add(module_name[extensions_prefix_len:].partition(".")[0::2])
    return extensions_enabled


def _extensions_enabled_from_repo_directory_and_pkg_id_sequence(repo_directory_and_pkg_id_sequence):
    # Use to calculate extensions which will be enabled,
    # needed so the wheels for the extensions can be enabled before the add-on is enabled that uses them.
    extensions_enabled_pending = set()
    repo_directory_to_module_map = _extension_repos_directory_to_module_map()
    for repo_directory, pkg_id_sequence in repo_directory_and_pkg_id_sequence:
        repo_module = repo_directory_to_module_map[repo_directory]
        for pkg_id in pkg_id_sequence:
            extensions_enabled_pending.add((repo_module, pkg_id))
    return extensions_enabled_pending


def _extensions_repo_sync_wheels(
        repo_cache_store,  # `bl_extension_utils.RepoCacheStore`
        extensions_enabled,  # `set[tuple[str, str]]`
        *,
        error_fn,  # `Callable[[Exception], None]`
):  # `-> None`
    """
    This function collects all wheels from all packages and ensures the packages are either extracted or removed
    when they are no longer used.
    """
    import addon_utils

    repos_all = extension_repos_read()

    wheel_list = []

    for repo_index, pkg_manifest_local in enumerate(repo_cache_store.pkg_manifest_from_local_ensure(
            error_fn=error_fn,
            ignore_missing=True,
    )):
        repo = repos_all[repo_index]
        repo_module = repo.module
        repo_directory = repo.directory
        for pkg_id, item_local in pkg_manifest_local.items():

            # Check it's enabled before initializing its wheels.
            # NOTE: no need for compatibility checks here as only compatible items will be included.
            if (repo_module, pkg_id) not in extensions_enabled:
                continue

            wheels_rel = item_local.wheels
            if not wheels_rel:
                continue

            if (wheel_abs := pkg_wheel_filter(repo_module, pkg_id, repo_directory, wheels_rel)) is not None:
                wheel_list.append(wheel_abs)

    extensions = bpy.utils.user_resource('EXTENSIONS')
    local_dir = os.path.join(extensions, ".local")

    # WARNING: bad level call, avoid making this a public function just now.
    # pylint: disable-next=protected-access
    addon_utils._extension_sync_wheels(
        local_dir=local_dir,
        wheel_list=wheel_list,
        debug=bpy.app.debug_python,
        error_fn=error_fn,
    )


def _extensions_repo_refresh_on_change(
        repo_cache_store,  # `bl_extension_utils.RepoCacheStore`
        *,
        extensions_enabled,  # `set[tuple[str, str]] | None`
        compat_calc,  # `bool`
        stats_calc,  # `bool`
        error_fn,  # `Callable[[Exception], None]`
):  # `-> None`
    import addon_utils
    if extensions_enabled is not None:
        _extensions_repo_sync_wheels(
            repo_cache_store,
            extensions_enabled,
            error_fn=error_fn,
        )
    # Wheel sync handled above.

    if compat_calc:
        # NOTE: `extensions_enabled` may contain add-ons which are not yet enabled (these are pending).
        # They *must* have their compatibility information refreshed here,
        # even though compatibility is guaranteed based on the code-path that calls this function.
        #
        # Without updating compatibility information, un-installing the extensions won't detect the
        # add-on as having been removed and won't remove any wheels the extension may use, see #125958.
        addon_modules_pending = None if extensions_enabled is None else ([
            "{:s}{:s}.{:s}".format(_ext_base_pkg_idname_with_dot, repo_module, pkg_id)
            for repo_module, pkg_id in extensions_enabled
        ])

        addon_utils.extensions_refresh(
            ensure_wheels=False,
            addon_modules_pending=addon_modules_pending,
            handle_error=error_fn,
        )

    if stats_calc:
        repo_stats_calc()


def _extension_repo_directory_validate_module(repo_directory):
    # Call after any action which may have created the repository directory for the first time.
    # This is needed in case an import was attempted and failed.
    # Afterwards, the user can perform an action which creates the directory.
    # If the cache is not cleared, enabling the add-on will fail, see: #124457.
    from sys import path_importer_cache
    # If the directory has been cached as missing, remove the cache.
    if path_importer_cache.get(repo_directory, ...) is None:
        # While highly likely the directory exists, it's possible it failed to be created
        # (maybe there are no permissions?), if that's the case keep the cache.
        if os.path.exists(repo_directory):
            del path_importer_cache[repo_directory]


# -----------------------------------------------------------------------------
# Theme Handling
#

def _preferences_theme_state_create():
    from .bl_extension_utils import (
        file_mtime_or_none,
        scandir_with_demoted_errors,
    )
    filepath = bpy.context.preferences.themes[0].filepath
    if not filepath:
        return None, None

    if (result := file_mtime_or_none(filepath)) is not None:
        return result, filepath

    # It's possible the XML was renamed after upgrading, detect another.
    dirpath = os.path.dirname(filepath)

    # Not essential, just avoids a demoted error from `scandir` which seems like it may be a bug.
    if not os.path.exists(dirpath):
        return None, None

    filepath = ""
    for entry in scandir_with_demoted_errors(dirpath):
        if entry.is_dir():
            continue
        # There must only ever be one.
        if entry.name.lower().endswith(".xml"):
            if (result := file_mtime_or_none(entry.path)) is not None:
                return result, filepath
    return None, None


def _preferences_theme_state_restore(state):
    state_update = _preferences_theme_state_create()
    # Unchanged, return.
    if state == state_update:
        return

    # Uninstall:
    # The current theme was an extension that was uninstalled.
    if state[0] is not None and state_update[0] is None:
        bpy.ops.preferences.reset_default_theme()
        return

    # Update:
    if state_update[0] is not None:
        extension_theme_enable_filepath(state_update[1])


# -----------------------------------------------------------------------------
# Internal Implementation
#

def _is_modal(op):
    if is_background:
        return False
    if not op.options.is_invoke:
        return False
    return True


class CommandHandle:
    __slots__ = (
        "modal_timer",
        "cmd_batch",
        "wm",
        "request_exit",
    )

    def __init__(self):
        self.modal_timer = None
        self.cmd_batch = None
        self.wm = None
        self.request_exit = None

    @staticmethod
    def op_exec_from_iter(op, context, cmd_batch, is_modal):
        if not is_modal:
            with CheckSIGINT_Context() as sigint_ctx:
                has_request_exit = cmd_batch.exec_blocking(
                    report_fn=_report,
                    request_exit_fn=lambda: sigint_ctx.has_interrupt,
                    concurrent=is_concurrent,
                )
            if has_request_exit:
                op.report({'WARNING'}, "Command interrupted")
                return {'FINISHED'}

            return {'FINISHED'}

        handle = CommandHandle()
        handle.cmd_batch = cmd_batch
        handle.modal_timer = context.window_manager.event_timer_add(0.1, window=context.window)
        handle.wm = context.window_manager

        handle.wm.modal_handler_add(op)

        op.runtime_handle_set(handle)
        return {'RUNNING_MODAL'}

    def op_modal_step(self, op, context):
        command_result = self.cmd_batch.exec_non_blocking(
            request_exit=self.request_exit,
        )

        # Forward new messages to reports.
        msg_list_per_command = self.cmd_batch.calc_status_log_since_last_request_or_none()
        if msg_list_per_command is not None:
            for i, msg_list in enumerate(msg_list_per_command, 1):
                for (ty, msg) in msg_list:
                    if len(msg_list_per_command) > 1:
                        # These reports are flattened, note the process number that fails so
                        # whoever is reading the reports can make sense of the messages.
                        msg = "{:s} (process {:d} of {:d})".format(msg, i, len(msg_list_per_command))
                    if ty == 'STATUS':
                        op.report({'INFO'}, msg)
                    elif ty == 'WARNING':
                        op.report({'WARNING'}, msg)
                    elif ty in {'ERROR', 'FATAL_ERROR'}:
                        op.report({'ERROR'}, msg)
                    else:
                        print("Internal error, type", ty, "not accounted for!")
                        op.report({'INFO'}, "{:s}: {:s}".format(ty, msg))

        del msg_list_per_command

        # Avoid high CPU usage by only redrawing when there has been a change.
        msg_list = self.cmd_batch.calc_status_log_or_none()
        if msg_list is not None:
            context.workspace.status_text_set(
                " | ".join(
                    ["{:s}: {:s}".format(ty, str(msg)) for (ty, msg) in msg_list]
                )
            )

            # Setting every time is a bit odd. but OK.
            repo_status_text.title = self.cmd_batch.title
            repo_status_text.log = msg_list
            repo_status_text.running = True
            _preferences_ui_redraw()

        if command_result.all_complete:
            self.wm.event_timer_remove(self.modal_timer)
            op.runtime_handle_clear()
            context.workspace.status_text_set(None)
            repo_status_text.running = False

            if self.request_exit:
                return {'CANCELLED'}
            return {'FINISHED'}

        return {'RUNNING_MODAL'}

    def op_modal_impl(self, op, context, event):
        pass_through = True
        refresh = False
        if event.type == 'TIMER':
            refresh = True
        elif event.type == 'ESC':
            if not self.request_exit:
                print("Request exit!")
                self.request_exit = True
                refresh = True
                # This escape event was handled.
                pass_through = False

        if refresh:
            return self.op_modal_step(op, context)

        if pass_through:
            return {'RUNNING_MODAL', 'PASS_THROUGH'}
        return {'RUNNING_MODAL'}

    def op_modal_cancel(self, op, context):
        import time
        self.request_exit = True
        while operator_finished_result(self.op_modal_step(op, context)) is None:
            # Avoid high CPU use on exit.
            time.sleep(0.1)


def _report(ty, msg):
    if ty == 'DONE':
        assert msg == ""
        return

    if is_background:
        print(ty, msg)
        return


def _repo_dir_and_index_get(repo_index, directory, report_fn):
    if repo_index != -1:
        repo_item = extension_repos_read_index(repo_index)
        directory = repo_item.directory if (repo_item is not None) else ""
    if not directory:
        report_fn({'ERROR'}, "Repository not set")
    return directory


def _extensions_maybe_online_action_poll_impl(cls, repo, action):

    if repo is not None:
        if not repo.enabled:
            cls.poll_message_set("Active repository is disabled")
            return False

    if repo is None:
        # This may not be correct but it's a reasonable assumption.
        online_access_required = True
    else:
        # Check the specifics to allow refreshing a single repository from the popover.
        online_access_required = repo.use_remote_url and (not repo.remote_url.startswith("file://"))

    if online_access_required:
        if not bpy.app.online_access:
            # Split message into sentences for i18n.
            match action:
                case 'CHECK_UPDATES':
                    message = rpt_("Online access required to check for updates.")
                case 'INSTALL_UPDATES':
                    message = rpt_("Online access required to install updates.")
                case _:
                    assert False, "Unreachable"
            if bpy.app.online_access_override:
                message += " " + rpt_("Launch Blender without --offline-mode")
            else:
                message += " " + rpt_("Enable online access in System preferences")
            cls.poll_message_set(message)
            return False

    repos_all = extension_repos_read(use_active_only=False)
    if not repos_all:
        cls.poll_message_set("No repositories available")
        return False

    return True


# -----------------------------------------------------------------------------
# Public Repository Actions
#

class _ExtCmdMixIn:
    """
    Utility to execute mix-in.

    Sub-class must define.
    - bl_idname
    - bl_label
    - exec_command_iter
    - exec_command_finish
    """
    cls_slots = (
        "_runtime_handle",
    )

    @classmethod
    def __init_subclass__(cls) -> None:
        for attr in ("exec_command_iter", "exec_command_finish"):
            if getattr(cls, attr) is getattr(_ExtCmdMixIn, attr):
                raise Exception("Subclass did not define 'exec_command_iter'!")

    def exec_command_iter(self, is_modal):
        raise Exception("Subclass must define!")

    def exec_command_finish(self, canceled):
        raise Exception("Subclass must define!")

    def error_fn_from_exception(self, ex):
        # A bit silly setting every time, but it's needed to ensure there is a title.
        repo_status_text.log.append(("ERROR", str(ex)))

    def execute(self, context):
        is_modal = _is_modal(self)
        cmd_batch = self.exec_command_iter(is_modal)
        # It's possible the action could not be started.
        # In this case `exec_command_iter` should report an error.
        if cmd_batch is None:
            return {'CANCELLED'}

        # Needed in cast there are no commands within `cmd_batch`,
        # the title should still be set.
        repo_status_text.title = cmd_batch.title

        result = CommandHandle.op_exec_from_iter(self, context, cmd_batch, is_modal)
        if (canceled := operator_finished_result(result)) is not None:
            self.exec_command_finish(canceled)
        return result

    def modal(self, context, event):
        result = self._runtime_handle.op_modal_impl(self, context, event)
        if (canceled := operator_finished_result(result)) is not None:
            wm_wait_cursor(True)
            self.exec_command_finish(canceled)
            wm_wait_cursor(False)

        return result

    def cancel(self, context):
        # Happens when canceling before the operator has run any commands.
        # Canceling from an operator popup dialog for example.
        if not hasattr(self, "_runtime_handle"):
            return

        canceled = True
        self._runtime_handle.op_modal_cancel(self, context)
        self.exec_command_finish(canceled)

    def runtime_handle_set(self, runtime_handle):
        assert isinstance(runtime_handle, CommandHandle)
        # pylint: disable-next=attribute-defined-outside-init
        self._runtime_handle = runtime_handle

    def runtime_handle_clear(self):
        del self._runtime_handle


class EXTENSIONS_OT_repo_sync(Operator, _ExtCmdMixIn):
    bl_idname = "extensions.repo_sync"
    bl_label = "Ext Repo Sync"
    __slots__ = _ExtCmdMixIn.cls_slots

    repo_directory: rna_prop_directory
    repo_index: rna_prop_repo_index

    def exec_command_iter(self, is_modal):
        from . import bl_extension_utils

        directory = _repo_dir_and_index_get(self.repo_index, self.repo_directory, self.report)
        if not directory:
            return None

        if (repo_item := _extensions_repo_from_directory_and_report(directory, self.report)) is None:
            return None

        if not os.path.exists(directory):
            try:
                os.makedirs(directory)
            except Exception as ex:
                self.report({'ERROR'}, str(ex))
                return {'CANCELLED'}

        prefs = bpy.context.preferences

        # Needed to refresh.
        self.repo_directory = directory

        # See comment for `EXTENSIONS_OT_repo_sync_all`.
        repos_lock = []

        cmd_batch = []
        if repo_item.remote_url:
            cmd_batch.append(
                partial(
                    bl_extension_utils.repo_sync,
                    directory=directory,
                    remote_name=repo_item.name,
                    remote_url=url_append_defaults(repo_item.remote_url),
                    online_user_agent=online_user_agent_from_blender(),
                    access_token=repo_item.access_token,
                    timeout=prefs.system.network_timeout,
                    use_idle=is_modal,
                    python_args=bpy.app.python_args,
                )
            )
            repos_lock.append(repo_item.directory)

        # Lock repositories.
        # pylint: disable-next=attribute-defined-outside-init
        self.repo_lock = bl_extension_utils.RepoLock(
            repo_directories=repos_lock,
            cookie=cookie_from_session(),
        )
        if lock_result_any_failed_with_report(self, self.repo_lock.acquire()):
            return None

        return bl_extension_utils.CommandBatch(
            title="Sync",
            batch=cmd_batch,
            batch_job_limit=1,
        )

    def exec_command_finish(self, canceled):
        repo_cache_store = repo_cache_store_ensure()
        repo_cache_store_refresh_from_prefs(repo_cache_store)
        repo_cache_store.refresh_remote_from_directory(
            directory=self.repo_directory,
            error_fn=self.error_fn_from_exception,
            force=True,
        )

        # Unlock repositories.
        lock_result_any_failed_with_report(self, self.repo_lock.release(), report_type='WARNING')
        del self.repo_lock

        repo_stats_calc()

        _preferences_ui_redraw()


class EXTENSIONS_OT_repo_sync_all(Operator, _ExtCmdMixIn):
    """Refresh the list of extensions for all the remote repositories"""
    bl_idname = "extensions.repo_sync_all"
    bl_label = "Refresh Remote"
    __slots__ = _ExtCmdMixIn.cls_slots

    use_active_only: BoolProperty(
        name="Active Only",
        description="Only sync the active repository",
    )

    @classmethod
    def poll(cls, context):
        repo = getattr(context, "extension_repo", None)
        return _extensions_maybe_online_action_poll_impl(cls, repo, action='CHECK_UPDATES')

    @classmethod
    def description(cls, _context, props):
        if props.use_active_only:
            return tip_("Refresh the list of extensions for the active repository")
        return ""  # Default.

    def exec_command_iter(self, is_modal):
        from . import bl_extension_utils

        use_active_only = self.use_active_only
        repos_all = extension_repos_read(use_active_only=use_active_only)

        if not repos_all:
            if use_active_only:
                self.report({'INFO'}, "The active repository has invalid settings")
            else:
                assert False, "unreachable"  # Poll prevents this.
            return None

        for repo_item in repos_all:
            if not os.path.exists(repo_item.directory):
                try:
                    os.makedirs(repo_item.directory)
                except Exception as ex:
                    self.report({'WARNING'}, str(ex))
                    return None

        prefs = bpy.context.preferences

        # It's only required to lock remote repositories, local repositories can refresh without being modified,
        # this is essential for system repositories which may be read-only.
        repos_lock = []

        cmd_batch = []
        for repo_item in repos_all:
            # Local only repositories should still refresh, but not run the sync.
            if repo_item.remote_url:
                cmd_batch.append(partial(
                    bl_extension_utils.repo_sync,
                    directory=repo_item.directory,
                    remote_name=repo_item.name,
                    remote_url=url_append_defaults(repo_item.remote_url),
                    online_user_agent=online_user_agent_from_blender(),
                    access_token=repo_item.access_token,
                    timeout=prefs.system.network_timeout,
                    use_idle=is_modal,
                    python_args=bpy.app.python_args,
                ))
                repos_lock.append(repo_item.directory)

        # Lock repositories.
        # pylint: disable-next=attribute-defined-outside-init
        self.repo_lock = bl_extension_utils.RepoLock(
            repo_directories=repos_lock,
            cookie=cookie_from_session(),
        )
        if lock_result_any_failed_with_report(self, self.repo_lock.acquire()):
            return None

        return bl_extension_utils.CommandBatch(
            title="Sync \"{:s}\"".format(repos_all[0].name) if use_active_only else "Sync All",
            batch=cmd_batch,
            batch_job_limit=1,
        )

    def exec_command_finish(self, canceled):
        repo_cache_store = repo_cache_store_ensure()
        repo_cache_store_refresh_from_prefs(repo_cache_store)

        for repo_item in extension_repos_read():
            repo_cache_store.refresh_remote_from_directory(
                directory=repo_item.directory,
                error_fn=self.error_fn_from_exception,
                force=True,
            )

        # Unlock repositories.
        lock_result_any_failed_with_report(self, self.repo_lock.release(), report_type='WARNING')
        del self.repo_lock

        repo_stats_calc()

        _preferences_ui_redraw()


class EXTENSIONS_OT_repo_refresh_all(Operator):
    """Scan extension & legacy add-ons for changes to modules & meta-data (similar to restarting). """ \
        """Any issues are reported as warnings"""
    bl_idname = "extensions.repo_refresh_all"
    bl_label = "Refresh Local"

    use_active_only: BoolProperty(
        name="Active Only",
        description="Only refresh the active repository",
    )

    def _exceptions_as_report(self, repo_name, ex):
        self.report({'WARNING'}, "{:s}: {:s}".format(repo_name, str(ex)))

    def execute(self, _context):
        import importlib
        import addon_utils

        use_active_only = self.use_active_only
        repos_all = extension_repos_read(use_active_only=use_active_only)
        repo_cache_store = repo_cache_store_ensure()

        if not repos_all:
            if use_active_only:
                self.report({'INFO'}, "The active repository has invalid settings")
            else:
                assert False, "unreachable"  # Poll prevents this.
            return {'CANCELLED'}

        for repo_item in repos_all:
            # Re-generate JSON meta-data from TOML files (needed for offline repository).
            repo_cache_store.refresh_remote_from_directory(
                directory=repo_item.directory,
                # NOTE: this isn't a problem as the callback isn't stored.
                # pylint: disable-next=cell-var-from-loop
                error_fn=lambda ex: self._exceptions_as_report(repo_item.name, ex),
                force=True,
            )
            repo_cache_store.refresh_local_from_directory(
                directory=repo_item.directory,
                # NOTE: this isn't a problem as the callback isn't stored.
                # pylint: disable-next=cell-var-from-loop
                error_fn=lambda ex: self._exceptions_as_report(repo_item.name, ex),
            )

        # Ensure module cache is removed, especially module cache that has marked a module as missing.
        # This is necessary for extension repositories that:
        # - Did not exist on startup.
        # - An extension attempted to load (marking the module as missing).
        # - The user then manually creates the directory and installs extensions.
        # In this case any add-on will fail to enable as the cache stores the missing state.
        # For a closely related issue see: #124457.
        importlib.invalidate_caches()

        # In-line `bpy.ops.preferences.addon_refresh`.
        addon_utils.modules_refresh()
        # Ensure compatibility info and wheels is up to date.
        addon_utils.extensions_refresh(
            ensure_wheels=True,
            handle_error=lambda ex: self.report({'ERROR'}, str(ex)),
        )

        _preferences_ui_redraw()
        _preferences_ui_refresh_addons()

        return {'FINISHED'}


# Show a dialog when dropping an extensions for a disabled repository.
class EXTENSIONS_OT_repo_enable_from_drop(Operator):
    bl_idname = "extensions.repo_enable_from_drop"
    bl_label = "Enable Repository from Drop"
    bl_options = {'INTERNAL'}

    # pylint: disable-next=declare-non-slot
    repo_index: rna_prop_repo_index

    __slots__ = (
        "_repo_name",
        "_repo_remote_url",
    )

    def invoke(self, context, _event):
        print(self.repo_index)
        if (repo := repo_lookup_by_index_or_none_with_report(self.repo_index, self.report)) is None:
            return {'CANCELLED'}
        # pylint: disable-next=attribute-defined-outside-init
        self._repo_name = repo.name
        # pylint: disable-next=attribute-defined-outside-init
        self._repo_remote_url = repo.remote_url

        wm = context.window_manager
        wm.invoke_props_dialog(
            self,
            width=400,
            confirm_text="Enable Repository",
            title="Disabled Repository",
        )

        return {'RUNNING_MODAL'}

    def execute(self, _context):
        if (repo := repo_lookup_by_index_or_none(self.repo_index)) is not None:
            repo.enabled = True

        return {'CANCELLED'}

    def draw(self, _context):
        layout = self.layout
        col = layout.column()
        col.label(text="The dropped extension comes from a disabled repository.")
        col.label(text="Enable the repository and try again.")
        col.separator()

        box = col.box()
        subcol = box.column(align=True)
        subcol.label(text=iface_("Name: {:s}").format(self._repo_name), translate=False)
        subcol.label(text=iface_("URL: {:s}").format(self._repo_remote_url), translate=False)


class EXTENSIONS_OT_repo_unlock(Operator):
    """Remove the repository file-system lock"""
    bl_idname = "extensions.repo_unlock"
    bl_label = "Force Unlock Active Repository"
    bl_options = {'INTERNAL'}

    __slots__ = (
        "_repo_vars",
    )

    @classmethod
    def _poll_message_or_none(cls, repos):
        # Either return a message for why the lock cannot be unlocked, or,
        # the lock time and a possible error when accessing it.
        if not repos:
            return "Active repository is not enabled has invalid settings", None, None
        repo = repos[0]

        from . import bl_extension_utils
        result = bl_extension_utils.repo_lock_directory_query(repo.directory, cookie_from_session())
        if result is None:
            return "Active repository is not locked", None, None

        lock_is_ours, lock_mtime, lock_error = result
        del result
        if lock_is_ours:
            return (
                "Active repository lock held by this session, "
                "either wait until the operation is finished or restart Blender"
            ), lock_mtime, lock_error
        return None, lock_mtime, lock_error

    @classmethod
    def poll(cls, _context):
        lock_message, _lock_mtime, _lock_error = cls._poll_message_or_none(extension_repos_read(use_active_only=True))
        if lock_message is not None:
            cls.poll_message_set(lock_message)
            return False
        return True

    def invoke(self, context, _event):
        import time
        repos = extension_repos_read(use_active_only=True)
        lock_message, lock_mtime, lock_error = self._poll_message_or_none(extension_repos_read(use_active_only=True))
        if lock_message is not None:
            self.report({'ERROR'}, lock_message)
            return {'CANCELLED'}

        repo = repos[0]

        lock_age = 0.0
        if lock_mtime != 0.0:
            lock_age = time.time() - lock_mtime

        # pylint: disable-next=attribute-defined-outside-init
        self._repo_vars = repo.name, repo.directory, lock_age, lock_error

        wm = context.window_manager
        wm.invoke_props_dialog(
            self,
            # Extra wide to account for long paths.
            width=500,
            confirm_text="Force Unlock Repository",
            title="Force Unlock Repository",
            cancel_default=True,
        )
        return {'RUNNING_MODAL'}

    def execute(self, _context):
        from . import bl_extension_utils

        repo_name, repo_directory, _lock_age, _lock_error = self._repo_vars
        if (error := bl_extension_utils.repo_lock_directory_force_unlock(repo_directory)):
            self.report({'ERROR'}, rpt_("Force unlock failed: {:s}").format(error))
            return {'CANCELLED'}

        self.report({'INFO'}, rpt_("Unlocked: {:s}").format(repo_name))
        return {'FINISHED'}

    def draw(self, _context):
        from .bl_extension_utils import seconds_as_human_readable_text

        layout = self.layout
        col = layout.column()
        col.label(text="Warning! Before unlocking, ensure another instance of Blender is not running.")
        col.label(text="Force unlocking may be necessary in the case of a crash or power failure,")
        col.label(text="otherwise it should be avoided.")

        col.separator()

        repo_name, repo_directory, lock_age, lock_error = self._repo_vars

        box = col.box()
        subcol = box.column(align=True)
        subcol.label(text=iface_("Name: {:s}").format(repo_name), translate=False)
        subcol.label(text=iface_("Path: {:s}").format(repo_directory), translate=False)
        if lock_age != 0.0:
            subcol.label(text=iface_("Age: {:s}").format(seconds_as_human_readable_text(lock_age, 2)), translate=False)
        if lock_error:
            subcol.label(text=iface_("Error: {:s}").format(lock_error), translate=False)


class EXTENSIONS_OT_package_upgrade_all(Operator, _ExtCmdMixIn):
    """Upgrade all the extensions to their latest version for all the remote repositories"""
    bl_idname = "extensions.package_upgrade_all"
    bl_label = "Install Available Updates"
    __slots__ = (
        *_ExtCmdMixIn.cls_slots,
        "_repo_directories",
    )

    use_active_only: BoolProperty(
        name="Active Only",
        description="Only sync the active repository",
    )

    @classmethod
    def poll(cls, context):
        repo = getattr(context, "extension_repo", None)
        if repo is not None:
            # NOTE: we could simply not show this operator for local repositories as it's
            # arguably self evident that a local-only repository has nothing to upgrade from.
            # For now tell the user why they can't use this action.
            if not repo.use_remote_url:
                cls.poll_message_set("Upgrade is not supported for local repositories")
                return False

        return _extensions_maybe_online_action_poll_impl(cls, repo, action='INSTALL_UPDATES')

    @classmethod
    def description(cls, _context, props):
        if props.use_active_only:
            return tip_("Upgrade all the extensions to their latest version for the active repository")
        return ""  # Default.

    def exec_command_iter(self, is_modal):
        import sys
        from . import bl_extension_utils
        # pylint: disable-next=attribute-defined-outside-init
        self._repo_directories = set()
        # pylint: disable-next=attribute-defined-outside-init
        self._addon_restore = []
        # pylint: disable-next=attribute-defined-outside-init
        self._theme_restore = _preferences_theme_state_create()

        use_active_only = self.use_active_only

        repos_all = extension_repos_read(use_active_only=use_active_only)
        repo_cache_store = repo_cache_store_ensure()

        repo_directory_supset = [repo_entry.directory for repo_entry in repos_all] if use_active_only else None

        if not repos_all:
            if use_active_only:
                self.report({'INFO'}, "The active repository has invalid settings")
            else:
                assert False, "unreachable"  # Poll prevents this.
            return None

        prefs = bpy.context.preferences

        network_connection_limit = prefs.system.network_connection_limit

        # NOTE: Unless we have a "clear-cache" operator - there isn't a great place to apply cache-clearing.
        # So when cache is disabled simply clear all cache before performing an update.
        # Further, individual install & remove operation will manage the cache
        # for the individual packages being installed or removed.
        for repo_item in repos_all:
            if repo_item.use_cache:
                continue
            bl_extension_utils.pkg_repo_cache_clear(repo_item.directory)

        # Track add-ons to disable before uninstalling.
        handle_addons_info = []

        packages_to_upgrade = [[] for _ in range(len(repos_all))]
        package_count = 0

        pkg_manifest_local_all = list(repo_cache_store.pkg_manifest_from_local_ensure(
            error_fn=self.error_fn_from_exception,
            directory_subset=repo_directory_supset,
        ))
        for repo_index, pkg_manifest_remote in enumerate(repo_cache_store.pkg_manifest_from_remote_ensure(
            error_fn=self.error_fn_from_exception,
            directory_subset=repo_directory_supset,
        )):
            if pkg_manifest_remote is None:
                continue

            pkg_manifest_local = pkg_manifest_local_all[repo_index]
            if pkg_manifest_local is None:
                continue

            repo_item = repos_all[repo_index]
            for pkg_id, item_remote in pkg_manifest_remote.items():
                item_local = pkg_manifest_local.get(pkg_id)
                if item_local is None:
                    # Not installed.
                    continue
                if item_remote.block:
                    # Blocked, don't touch.
                    continue

                if item_remote.version != item_local.version:
                    packages_to_upgrade[repo_index].append(pkg_id)
                    package_count += 1

            if (pkg_id_sequence_upgrade := _preferences_pkg_id_sequence_filter_enabled(
                    repo_item,
                    packages_to_upgrade[repo_index],
            )):
                handle_addons_info.append((repo_item, pkg_id_sequence_upgrade))

        cmd_batch = []
        for repo_index, pkg_id_sequence in enumerate(packages_to_upgrade):
            if not pkg_id_sequence:
                continue

            repo_item = repos_all[repo_index]
            for pkg_id_sequence_iter in _sequence_split_with_job_limit(pkg_id_sequence, network_connection_limit):
                cmd_batch.append(partial(
                    bl_extension_utils.pkg_install,
                    directory=repo_item.directory,
                    remote_url=url_append_defaults(repo_item.remote_url),
                    pkg_id_sequence=pkg_id_sequence_iter,
                    online_user_agent=online_user_agent_from_blender(),
                    blender_version=bpy.app.version,
                    python_version=sys.version_info[:3],
                    access_token=repo_item.access_token,
                    timeout=prefs.system.network_timeout,
                    use_cache=repo_item.use_cache,
                    use_idle=is_modal,
                    python_args=bpy.app.python_args,
                ))
            self._repo_directories.add(repo_item.directory)

        if not cmd_batch:
            self.report({'INFO'}, "No installed packages to update")
            return None

        # Lock repositories.
        # pylint: disable-next=attribute-defined-outside-init
        self.repo_lock = bl_extension_utils.RepoLock(
            repo_directories=list(self._repo_directories),
            cookie=cookie_from_session(),
        )
        if lock_result_any_failed_with_report(self, self.repo_lock.acquire()):
            return None

        for repo_item, pkg_id_sequence in handle_addons_info:
            result = _preferences_ensure_disabled(
                repo_item=repo_item,
                pkg_id_sequence=pkg_id_sequence,
                default_set=False,
                error_fn=lambda ex: self.report({'ERROR'}, str(ex)),
            )
            self._addon_restore.append((repo_item, pkg_id_sequence, result))

        return bl_extension_utils.CommandBatch(
            title=(
                "Update {:d} Package(s) from \"{:s}\"".format(package_count, repos_all[0].name) if use_active_only else
                "Update {:d} Package(s)".format(package_count)
            ),
            batch=cmd_batch,
            batch_job_limit=network_connection_limit,
        )

    def exec_command_finish(self, canceled):

        # Unlock repositories.
        lock_result_any_failed_with_report(self, self.repo_lock.release(), report_type='WARNING')
        del self.repo_lock

        # Refresh installed packages for repositories that were operated on.
        repo_cache_store = repo_cache_store_ensure()
        for directory in self._repo_directories:
            repo_cache_store.refresh_local_from_directory(
                directory=directory,
                error_fn=self.error_fn_from_exception,
            )

        # TODO: it would be nice to include this message in the banner.
        def handle_error(ex):
            self.report({'ERROR'}, str(ex))

        # Ensure wheels are refreshed before re-enabling.
        _extensions_repo_refresh_on_change(
            repo_cache_store,
            extensions_enabled=set(
                (repo_item.module, pkg_id)
                for (repo_item, pkg_id_sequence, result) in self._addon_restore
                for pkg_id in pkg_id_sequence
            ),
            compat_calc=True,
            stats_calc=True,
            error_fn=handle_error,
        )

        _preferences_ensure_enabled_all(
            addon_restore=self._addon_restore,
            handle_error=handle_error,
        )
        _preferences_theme_state_restore(self._theme_restore)

        _preferences_ui_redraw()
        _preferences_ui_refresh_addons()


class EXTENSIONS_OT_package_install_marked(Operator, _ExtCmdMixIn):
    bl_idname = "extensions.package_install_marked"
    bl_label = "Ext Package Install_marked"
    __slots__ = (
        *_ExtCmdMixIn.cls_slots,
        "_repo_directories",
        "_repo_map_packages_addon_only",
    )

    enable_on_install: rna_prop_enable_on_install

    def exec_command_iter(self, is_modal):
        import sys
        from . import bl_extension_utils

        repos_all = extension_repos_read()
        repo_cache_store = repo_cache_store_ensure()

        pkg_manifest_remote_all = list(repo_cache_store.pkg_manifest_from_remote_ensure(
            error_fn=self.error_fn_from_exception,
        ))
        repo_pkg_map = _pkg_marked_by_repo(repo_cache_store, pkg_manifest_remote_all)
        # pylint: disable-next=attribute-defined-outside-init
        self._repo_directories = set()
        # pylint: disable-next=attribute-defined-outside-init
        self._repo_map_packages_addon_only = []
        package_count = 0

        prefs = bpy.context.preferences

        network_connection_limit = prefs.system.network_connection_limit

        cmd_batch = []
        for repo_index, pkg_id_sequence in sorted(repo_pkg_map.items()):
            repo_item = repos_all[repo_index]
            # Filter out already installed.
            pkg_manifest_local = repo_cache_store.refresh_local_from_directory(
                directory=repo_item.directory,
                error_fn=self.error_fn_from_exception,
            )
            if pkg_manifest_local is None:
                continue
            pkg_id_sequence = [pkg_id for pkg_id in pkg_id_sequence if pkg_id not in pkg_manifest_local]
            if not pkg_id_sequence:
                continue

            for pkg_id_sequence_iter in _sequence_split_with_job_limit(pkg_id_sequence, network_connection_limit):
                cmd_batch.append(partial(
                    bl_extension_utils.pkg_install,
                    directory=repo_item.directory,
                    remote_url=url_append_defaults(repo_item.remote_url),
                    pkg_id_sequence=pkg_id_sequence_iter,
                    online_user_agent=online_user_agent_from_blender(),
                    blender_version=bpy.app.version,
                    python_version=sys.version_info[:3],
                    access_token=repo_item.access_token,
                    timeout=prefs.system.network_timeout,
                    use_cache=repo_item.use_cache,
                    use_idle=is_modal,
                    python_args=bpy.app.python_args,
                ))

            self._repo_directories.add(repo_item.directory)
            package_count += len(pkg_id_sequence)

            # Filter out non add-on extensions.
            pkg_manifest_remote = pkg_manifest_remote_all[repo_index]

            pkg_id_sequence_addon_only = [
                pkg_id for pkg_id in pkg_id_sequence
                if pkg_manifest_remote[pkg_id].type == "add-on"
            ]
            if pkg_id_sequence_addon_only:
                self._repo_map_packages_addon_only.append((repo_item.directory, pkg_id_sequence_addon_only))

        if not cmd_batch:
            self.report({'ERROR'}, "No uninstalled packages marked")
            return None

        # Lock repositories.
        # pylint: disable-next=attribute-defined-outside-init
        self.repo_lock = bl_extension_utils.RepoLock(
            repo_directories=list(self._repo_directories),
            cookie=cookie_from_session(),
        )
        if lock_result_any_failed_with_report(self, self.repo_lock.acquire()):
            return None

        return bl_extension_utils.CommandBatch(
            title="Install {:d} Marked Package(s)".format(package_count),
            batch=cmd_batch,
            batch_job_limit=network_connection_limit,
        )

    def exec_command_finish(self, canceled):

        # Unlock repositories.
        lock_result_any_failed_with_report(self, self.repo_lock.release(), report_type='WARNING')
        del self.repo_lock

        # TODO: it would be nice to include this message in the banner.
        def handle_error(ex):
            self.report({'ERROR'}, str(ex))

        # Refresh installed packages for repositories that were operated on.
        repo_cache_store = repo_cache_store_ensure()
        for directory in self._repo_directories:
            repo_cache_store.refresh_local_from_directory(
                directory=directory,
                error_fn=self.error_fn_from_exception,
            )
            _extension_repo_directory_validate_module(directory)

        extensions_enabled = None
        if self.enable_on_install:
            extensions_enabled = _extensions_enabled()
            extensions_enabled.update(
                _extensions_enabled_from_repo_directory_and_pkg_id_sequence(
                    self._repo_map_packages_addon_only,
                )
            )

        _extensions_repo_refresh_on_change(
            repo_cache_store,
            extensions_enabled=extensions_enabled,
            compat_calc=True,
            stats_calc=True,
            error_fn=handle_error,
        )

        for directory, pkg_id_sequence in self._repo_map_packages_addon_only:

            pkg_manifest_local = repo_cache_store.refresh_local_from_directory(
                directory=directory,
                error_fn=self.error_fn_from_exception,
            )

            if self.enable_on_install:
                _preferences_install_post_enable_on_install(
                    directory=directory,
                    pkg_manifest_local=pkg_manifest_local,
                    pkg_id_sequence=pkg_id_sequence,
                    # Installed packages are always excluded.
                    pkg_id_sequence_upgrade=[],
                    handle_error=handle_error,
                )
            _extensions_repo_temp_files_make_stale(directory)
            _extensions_repo_install_stale_package_clear(directory, pkg_id_sequence)

        if self.enable_on_install:
            if (extensions_enabled_test := _extensions_enabled()) != extensions_enabled:
                # Some extensions could not be enabled, re-calculate wheels which may have been setup
                # in anticipation for the add-on working.
                _extensions_repo_refresh_on_change(
                    repo_cache_store,
                    extensions_enabled=extensions_enabled_test,
                    compat_calc=False,
                    stats_calc=False,
                    error_fn=handle_error,
                )

        _preferences_ui_redraw()
        _preferences_ui_refresh_addons()


class EXTENSIONS_OT_package_uninstall_marked(Operator, _ExtCmdMixIn):
    bl_idname = "extensions.package_uninstall_marked"
    bl_label = "Ext Package Uninstall Marked"
    __slots__ = (
        *_ExtCmdMixIn.cls_slots,
        "_repo_directories",
        "_pkg_id_sequence_from_directory"
    )

    def exec_command_iter(self, is_modal):
        from . import bl_extension_utils
        # TODO: check if the packages are already installed (notify the user).
        # Perhaps re-install?
        repos_all = extension_repos_read()
        repo_cache_store = repo_cache_store_ensure()

        pkg_manifest_local_all = list(repo_cache_store.pkg_manifest_from_local_ensure(
            error_fn=self.error_fn_from_exception,
        ))
        repo_pkg_map = _pkg_marked_by_repo(repo_cache_store, pkg_manifest_local_all)
        package_count = 0

        # pylint: disable-next=attribute-defined-outside-init
        self._repo_directories = set()
        # pylint: disable-next=attribute-defined-outside-init
        self._theme_restore = _preferences_theme_state_create()
        # pylint: disable-next=attribute-defined-outside-init
        self._pkg_id_sequence_from_directory = {}

        # Track add-ons to disable before uninstalling.
        handle_addons_info = []

        cmd_batch = []
        for repo_index, pkg_id_sequence in sorted(repo_pkg_map.items()):
            repo_item = repos_all[repo_index]

            # Filter out not installed.
            pkg_manifest_local = repo_cache_store.refresh_local_from_directory(
                directory=repo_item.directory,
                error_fn=self.error_fn_from_exception,
            )
            if pkg_manifest_local is None:
                continue
            pkg_id_sequence = [pkg_id for pkg_id in pkg_id_sequence if pkg_id in pkg_manifest_local]
            if not pkg_id_sequence:
                continue

            cmd_batch.append(
                partial(
                    bl_extension_utils.pkg_uninstall,
                    directory=repo_item.directory,
                    user_directory=repo_user_directory(repo_item.module),
                    pkg_id_sequence=pkg_id_sequence,
                    use_idle=is_modal,
                    python_args=bpy.app.python_args,
                ))
            self._repo_directories.add(repo_item.directory)
            package_count += len(pkg_id_sequence)

            handle_addons_info.append((repo_item, pkg_id_sequence))

            self._pkg_id_sequence_from_directory[repo_item.directory] = pkg_id_sequence

        if not cmd_batch:
            self.report({'ERROR'}, "No installed packages marked")
            return None

        # Lock repositories.
        # pylint: disable-next=attribute-defined-outside-init
        self.repo_lock = bl_extension_utils.RepoLock(
            repo_directories=list(self._repo_directories),
            cookie=cookie_from_session(),
        )
        if lock_result_any_failed_with_report(self, self.repo_lock.acquire()):
            return None

        for repo_item, pkg_id_sequence in handle_addons_info:
            # No need to store the result because the add-ons aren't going to be enabled again.
            _preferences_ensure_disabled(
                repo_item=repo_item,
                pkg_id_sequence=pkg_id_sequence,
                default_set=True,
                error_fn=lambda ex: self.report({'ERROR'}, str(ex)),
            )

        return bl_extension_utils.CommandBatch(
            title="Uninstall {:d} Marked Package(s)".format(package_count),
            batch=cmd_batch,
            batch_job_limit=1,
        )

    def exec_command_finish(self, canceled):

        # Unlock repositories.
        lock_result_any_failed_with_report(self, self.repo_lock.release(), report_type='WARNING')
        del self.repo_lock

        # TODO: it would be nice to include this message in the banner.
        def handle_error(ex):
            self.report({'ERROR'}, str(ex))

        for directory, pkg_id_sequence in self._pkg_id_sequence_from_directory.items():
            _extensions_repo_temp_files_make_stale(repo_directory=directory)
            _extensions_repo_uninstall_stale_package_fallback(
                repo_directory=directory,
                pkg_id_sequence=pkg_id_sequence,
            )

        # Refresh installed packages for repositories that were operated on.
        repo_cache_store = repo_cache_store_ensure()
        for directory in self._repo_directories:
            repo_cache_store.refresh_local_from_directory(
                directory=directory,
                error_fn=self.error_fn_from_exception,
            )

        _extensions_repo_refresh_on_change(
            repo_cache_store,
            extensions_enabled=_extensions_enabled(),
            compat_calc=True,
            stats_calc=True,
            error_fn=handle_error,
        )

        _preferences_theme_state_restore(self._theme_restore)

        _preferences_ui_redraw()
        _preferences_ui_refresh_addons()


class EXTENSIONS_OT_package_install_files(Operator, _ExtCmdMixIn):
    """Install extensions from files into a locally managed repository"""
    bl_idname = "extensions.package_install_files"
    bl_label = "Install from Disk"
    __slots__ = (
        *_ExtCmdMixIn.cls_slots,
        "repo_directory",
        "pkg_id_sequence"
    )

    # Dropping a file-path stores values in the class instance, values used are as follows:
    #
    # - None: Unset (not dropping), this value is read from the class.
    # - (pkg_id, pkg_type): Drop values have been extracted from the ZIP file.
    #   Where the `pkg_id` is the ID in the extensions manifest and the `pkg_type`
    #   is the type of extension see `rna_prop_enable_on_install_type_map` keys.
    _drop_variables = None
    # Used when dropping legacy add-ons:
    #
    # - None: Unset, not dropping a legacy add-on.
    # - True: Drop treats the `filepath` as a legacy add-on.
    #   `_drop_variables` will be None.
    _legacy_drop = None

    filter_glob: StringProperty(default="*.zip;*.py", options={'HIDDEN'})

    directory: StringProperty(
        name="Directory",
        subtype='DIR_PATH',
        default="",
    )
    files: CollectionProperty(
        type=bpy.types.OperatorFileListElement,
        options={'HIDDEN', 'SKIP_SAVE'}
    )

    # Use for for scripts.
    filepath: StringProperty(
        subtype='FILE_PATH',
    )

    repo: EnumProperty(
        name="User Repository",
        items=rna_prop_repo_enum_valid_only_itemf,
        description="The user repository to install extensions into",
    )

    enable_on_install: rna_prop_enable_on_install

    # Properties matching the legacy operator, not used by extension packages.
    target: EnumProperty(
        name="Legacy Target Path",
        items=bpy.types.PREFERENCES_OT_addon_install._target_path_items,
        description="Path to install legacy add-on packages to",
    )

    overwrite: BoolProperty(
        name="Legacy Overwrite",
        description="Remove existing add-ons with the same ID",
        default=True,
    )

    # Only used for code-path for dropping an extension.
    url: rna_prop_url

    def exec_command_iter(self, is_modal):
        import sys
        from . import bl_extension_utils
        from .bl_extension_utils import (
            pkg_manifest_dict_from_archive_or_error,
            pkg_is_legacy_addon,
        )

        # pylint: disable-next=attribute-defined-outside-init
        self._addon_restore = []
        # pylint: disable-next=attribute-defined-outside-init
        self._theme_restore = _preferences_theme_state_create()

        # Happens when run from scripts and this argument isn't passed in.
        if not self.properties.is_property_set("repo"):
            self.report({'ERROR'}, "Repository not set")
            return None

        # Repository accessed.
        repo_module_name = self.repo
        repo_item = next(
            (repo_item for repo_item in extension_repos_read() if repo_item.module == repo_module_name),
            None,
        )
        # This should really never happen as poll means this shouldn't be possible.
        assert repo_item is not None
        del repo_module_name
        # Done with the repository.

        source_files = [os.path.join(file.name) for file in self.files]
        source_directory = self.directory
        # Support a single `filepath`, more convenient when calling from scripts.
        if not (source_directory and source_files):
            source_directory, source_file = os.path.split(self.filepath)
            if not (source_directory and source_file):
                # Be specific with this error as a vague message is confusing when files
                # are passed via the command line.
                if source_directory or source_file:
                    if source_file:
                        self.report({'ERROR'}, "Unable to install from relative path")
                    else:
                        self.report({'ERROR'}, "Unable to install a directory")
                else:
                    self.report({'ERROR'}, "Unable to install from disk, no paths were defined")
                return None
            source_files = [source_file]
            del source_file
        assert len(source_files) > 0

        # Make absolute paths.
        source_files = [os.path.join(source_directory, filename) for filename in source_files]

        # Extract meta-data from package files.
        # Note that errors are ignored here, let the underlying install operation do this.
        pkg_id_sequence = []
        pkg_files = []
        pkg_legacy_files = []
        for source_filepath in source_files:
            if pkg_is_legacy_addon(source_filepath):
                pkg_legacy_files.append(source_filepath)
                continue
            pkg_files.append(source_filepath)

            result = pkg_manifest_dict_from_archive_or_error(source_filepath)
            if isinstance(result, str):
                continue
            pkg_id = result["id"]
            if pkg_id in pkg_id_sequence:
                continue
            pkg_id_sequence.append(pkg_id)

        directory = repo_item.directory
        assert directory != ""

        # Install legacy add-ons
        for source_filepath in pkg_legacy_files:
            self.exec_legacy(source_filepath)

        if not pkg_files:
            return None

        # Collect package ID's.
        # pylint: disable-next=attribute-defined-outside-init
        self.repo_directory = directory
        # pylint: disable-next=attribute-defined-outside-init
        self.pkg_id_sequence = pkg_id_sequence

        # Detect upgrade of enabled add-ons.
        if pkg_id_sequence:
            if pkg_id_sequence_upgrade := _preferences_pkg_id_sequence_filter_enabled(repo_item, pkg_id_sequence):
                result = _preferences_ensure_disabled(
                    repo_item=repo_item,
                    pkg_id_sequence=pkg_id_sequence_upgrade,
                    default_set=False,
                    error_fn=lambda ex: self.report({'ERROR'}, str(ex)),
                )
                self._addon_restore.append((repo_item, pkg_id_sequence_upgrade, result))
            del pkg_id_sequence_upgrade

        # Lock repositories.
        # pylint: disable-next=attribute-defined-outside-init
        self.repo_lock = bl_extension_utils.RepoLock(
            repo_directories=[repo_item.directory],
            cookie=cookie_from_session(),
        )
        if lock_result_any_failed_with_report(self, self.repo_lock.acquire()):
            return None

        return bl_extension_utils.CommandBatch(
            title="Install Package Files",
            batch=[
                partial(
                    bl_extension_utils.pkg_install_files,
                    directory=directory,
                    files=pkg_files,
                    blender_version=bpy.app.version,
                    python_version=sys.version_info[:3],
                    use_idle=is_modal,
                    python_args=bpy.app.python_args,
                )
            ],
            batch_job_limit=1,
        )

    def exec_command_finish(self, canceled):

        # Refresh installed packages for repositories that were operated on.
        repo_cache_store = repo_cache_store_ensure()

        # Re-generate JSON meta-data from TOML files (needed for offline repository).
        repo_cache_store.refresh_remote_from_directory(
            directory=self.repo_directory,
            error_fn=self.error_fn_from_exception,
            force=True,
        )
        _extension_repo_directory_validate_module(self.repo_directory)

        # Unlock repositories.
        lock_result_any_failed_with_report(self, self.repo_lock.release(), report_type='WARNING')
        del self.repo_lock

        # TODO: it would be nice to include this message in the banner.
        def handle_error(ex):
            self.report({'ERROR'}, str(ex))

        pkg_manifest_local = repo_cache_store.refresh_local_from_directory(
            directory=self.repo_directory,
            error_fn=self.error_fn_from_exception,
        )

        extensions_enabled = None
        if self.enable_on_install:
            extensions_enabled = _extensions_enabled()
            # We may want to support multiple.
            extensions_enabled.update(
                _extensions_enabled_from_repo_directory_and_pkg_id_sequence(
                    [(self.repo_directory, self.pkg_id_sequence)]
                )
            )

        _extensions_repo_refresh_on_change(
            repo_cache_store,
            extensions_enabled=extensions_enabled,
            compat_calc=True,
            stats_calc=True,
            error_fn=handle_error,
        )

        _preferences_ensure_enabled_all(
            addon_restore=self._addon_restore,
            handle_error=handle_error,
        )
        _preferences_theme_state_restore(self._theme_restore)

        if self._addon_restore:
            pkg_id_sequence_upgrade = self._addon_restore[0][1]
        else:
            pkg_id_sequence_upgrade = []

        if self.enable_on_install:
            _preferences_install_post_enable_on_install(
                directory=self.repo_directory,
                pkg_manifest_local=pkg_manifest_local,
                pkg_id_sequence=self.pkg_id_sequence,
                pkg_id_sequence_upgrade=pkg_id_sequence_upgrade,
                handle_error=handle_error,
            )

        if self.enable_on_install:
            if (extensions_enabled_test := _extensions_enabled()) != extensions_enabled:
                # Some extensions could not be enabled, re-calculate wheels which may have been setup
                # in anticipation for the add-on working.
                _extensions_repo_refresh_on_change(
                    repo_cache_store,
                    extensions_enabled=extensions_enabled_test,
                    compat_calc=False,
                    stats_calc=False,
                    error_fn=handle_error,
                )

        _extensions_repo_temp_files_make_stale(self.repo_directory)
        _extensions_repo_install_stale_package_clear(self.repo_directory, self.pkg_id_sequence)

        _preferences_ui_redraw()
        _preferences_ui_refresh_addons()

    def exec_legacy(self, filepath):
        backup_filepath = self.filepath
        self.filepath = filepath
        bpy.types.PREFERENCES_OT_addon_install.execute(self, bpy.context)
        self.filepath = backup_filepath

    @classmethod
    def poll(cls, context):
        if next(repo_iter_valid_only(context, exclude_remote=False, exclude_system=True), None) is None:
            cls.poll_message_set("There must be at least one user repository set to install extensions into")
            return False
        return True

    def invoke(self, context, event):

        # Regarding reusing the last repository.
        # - If it's a "local" repository, use it.
        # - If it's a "remote" repository, reset.
        # This is done because installing a file into a remote repository is a corner-case supported so
        # it's possible to download large extensions before installing as well as down-grading to older versions.
        # Installing into a remote repository should be intentional, not the default.
        # This could be annoying to users if they want to install many files into a remote repository,
        # in this case they would be better off using the file selector "Install from disk"
        # which supports selecting multiple files, although support for dropping multiple files would
        # also be good to support.
        if not self.properties.is_property_set("repo"):
            repos_valid = self._repos_valid_for_install(context)
            repo_module = self.repo
            if (repo_test := next((repo for repo in repos_valid if repo.module == repo_module), None)) is not None:
                if repo_test.use_remote_url:
                    self.properties.property_unset("repo")
            del repo_module, repo_test, repos_valid

        if self.properties.is_property_set("url"):
            return self._invoke_for_drop(context, event)

        # Ensure the value is marked as set (else an error is reported).
        self.repo = self.repo

        context.window_manager.fileselect_add(self)
        return {'RUNNING_MODAL'}

    def draw(self, context):
        if self._drop_variables is not None:
            self._draw_for_drop(context)
            return
        elif self._legacy_drop is not None:
            self._draw_for_legacy_drop(context)
            return

        # Override draw because the repository names may be over-long and not fit well in the UI.
        # Show the text & repository names in two separate rows.
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False
        layout.prop(self, "enable_on_install")

        header, body = layout.panel("extensions")
        header.label(text="Extensions")
        if body:
            body.prop(self, "repo", text="Repository")

        header, body = layout.panel("legacy", default_closed=True)
        header.label(text="Legacy Add-ons")

        row = header.row()
        row.alignment = 'RIGHT'
        row.emboss = 'NONE'
        row.operator("wm.doc_view_manual", icon='URL', text="").doc_id = "preferences.addon_install"

        if body:
            body.prop(self, "target", text="Target Path")
            body.prop(self, "overwrite", text="Overwrite")

    def _invoke_for_drop(self, context, _event):
        # Drop logic.
        print("DROP FILE:", self.url)

        # Blender calls the drop logic with an un-encoded file-path.
        # It would be nicer if it used the file URI schema,
        # however this is only activated from Blender's drop logic.
        #
        # TODO: even though Blender supports "remote" repositories referencing `file://` prefixed URL's.
        # These are not supported for dropping. Since at the time of dropping it's not known that the
        # path is referenced from a "local" repository or a "remote" that uses a `file://` URL.
        filepath = self.url

        from .bl_extension_utils import pkg_is_legacy_addon

        if not pkg_is_legacy_addon(filepath):
            from .bl_extension_utils import pkg_manifest_dict_from_archive_or_error

            repos_valid = self._repos_valid_for_install(context)
            if not repos_valid:
                self.report({'ERROR'}, "No user repositories")
                return {'CANCELLED'}

            if isinstance(result := pkg_manifest_dict_from_archive_or_error(filepath), str):
                self.report({'ERROR'}, rpt_("Error in manifest {:s}").format(result))
                return {'CANCELLED'}

            pkg_id = result["id"]
            pkg_type = result["type"]

            if not self.properties.is_property_set("repo"):
                if (repo := self._repo_detect_from_manifest_dict(result, repos_valid)) is not None:
                    self.repo = repo
                del repo

            self._drop_variables = pkg_id, pkg_type
            self._legacy_drop = None

            del result, pkg_id, pkg_type
        else:
            self._drop_variables = None
            self._legacy_drop = True

        # Set to it's self to the property is considered "set".
        self.repo = self.repo
        self.filepath = filepath

        wm = context.window_manager
        wm.invoke_props_dialog(self, width=400)

        return {'RUNNING_MODAL'}

    def _draw_for_drop(self, _context):

        layout = self.layout
        layout.operator_context = 'EXEC_DEFAULT'

        _pkg_id, pkg_type = self._drop_variables

        layout.label(text="User Repository")
        layout.prop(self, "repo", text="")

        layout.prop(self, "enable_on_install", text=rna_prop_enable_on_install_type_map[pkg_type])

    def _draw_for_legacy_drop(self, _context):

        layout = self.layout
        layout.operator_context = 'EXEC_DEFAULT'

        layout.label(text="Legacy Add-on")
        layout.prop(self, "target", text="Target")
        layout.prop(self, "overwrite", text="Overwrite")
        layout.prop(self, "enable_on_install")

    @staticmethod
    def _repos_valid_for_install(context):
        return list(repo_iter_valid_only(context, exclude_remote=False, exclude_system=True))

    # Use to set the repository default when dropping a file.
    # This is only used to set the default value.
    # If it fails to find a match the user may still select the repository,
    # this is just intended to handle the common case where a user may download an
    # extension from a remote repository they use, dropping the file into Blender.
    @staticmethod
    def _repo_detect_from_manifest_dict(manifest_dict, repos_valid):
        repos_valid = [
            repo_item for repo_item in repos_valid
            if repo_item.use_remote_url
        ]
        if not repos_valid:
            return None

        repo_cache_store = repo_cache_store_ensure()
        repo_cache_store_refresh_from_prefs(repo_cache_store)

        for repo_item in repos_valid:
            pkg_manifest_remote = repo_cache_store.refresh_remote_from_directory(
                directory=repo_item.directory,
                error_fn=print,
                force=False,
            )
            if pkg_manifest_remote is None:
                continue

            # NOTE: The exact method of matching extensions is a little arbitrary.
            # Use (id, type, (name or tagline)) since this has a good change of finding a correct match.
            # Since an extension might be renamed, check if the `name` or the `tagline` match.
            item_remote = pkg_manifest_remote.get(manifest_dict["id"])
            if item_remote is None:
                continue
            if item_remote.type != manifest_dict["type"]:
                continue
            if (
                    (item_remote.name != manifest_dict["name"]) and
                    (item_remote.tagline != manifest_dict["tagline"])
            ):
                continue

            return repo_item.module

        return None


class EXTENSIONS_OT_package_install(Operator, _ExtCmdMixIn):
    """Download and install the extension"""
    bl_idname = "extensions.package_install"
    bl_label = "Install Extension"
    __slots__ = _ExtCmdMixIn.cls_slots

    # Dropping a URL stores values in the class instance, values used are as follows:
    #
    # - None: Unset (not-dropping), this value is read from the class.
    # - A tuple containing values needed to execute the drop:
    #   `(repo_index: int, repo_name: str, pkg_id: str, item_remote: PkgManifest_Normalized)`.
    #
    #   NOTE: these values aren't set immediately when dropping as they
    #   require the local repository to sync first, so the up to date meta-data
    #   from the URL can be used to ensure the dropped extension is known
    #   and any errors are based on up to date information.
    #
    _drop_variables = None
    # Optional draw & keyword-arguments, return True to terminate drawing.
    _draw_override = None

    repo_directory: rna_prop_directory
    repo_index: rna_prop_repo_index

    pkg_id: rna_prop_pkg_id

    enable_on_install: rna_prop_enable_on_install

    # Only used for code-path for dropping an extension.
    url: rna_prop_url

    # NOTE: this can be removed once upgrading from 4.1 is no longer relevant.
    # Only used when moving from  previously built-in add-ons to extensions.
    do_legacy_replace: BoolProperty(
        name="Do Legacy Replace",
        default=False,
        options={'HIDDEN', 'SKIP_SAVE'}
    )

    @classmethod
    def poll(cls, _context):
        if not bpy.app.online_access:
            if bpy.app.online_access_override:
                cls.poll_message_set(
                    "Online access required to install or update. Launch Blender without --offline-mode")
            else:
                cls.poll_message_set(
                    "Online access required to install or update. Enable online access in System preferences")
            return False

        return True

    def exec_command_iter(self, is_modal):
        import sys
        from . import bl_extension_utils

        if not self._is_ready_to_execute():
            return None

        # pylint: disable-next=attribute-defined-outside-init
        self._addon_restore = []
        # pylint: disable-next=attribute-defined-outside-init
        self._theme_restore = _preferences_theme_state_create()

        directory = _repo_dir_and_index_get(self.repo_index, self.repo_directory, self.report)
        if not directory:
            return None
        self.repo_directory = directory

        if (repo_item := _extensions_repo_from_directory_and_report(directory, self.report)) is None:
            return None

        if not (pkg_id := self.pkg_id):
            self.report({'ERROR'}, "Package ID not set")
            return None

        prefs = bpy.context.preferences

        if pkg_id_sequence_upgrade := _preferences_pkg_id_sequence_filter_enabled(repo_item, [pkg_id]):
            result = _preferences_ensure_disabled(
                repo_item=repo_item,
                pkg_id_sequence=pkg_id_sequence_upgrade,
                default_set=False,
                error_fn=lambda ex: self.report({'ERROR'}, str(ex)),
            )
            self._addon_restore.append((repo_item, pkg_id_sequence_upgrade, result))
            del result
        del pkg_id_sequence_upgrade

        # Lock repositories.
        # pylint: disable-next=attribute-defined-outside-init
        self.repo_lock = bl_extension_utils.RepoLock(
            repo_directories=[repo_item.directory],
            cookie=cookie_from_session(),
        )
        if lock_result_any_failed_with_report(self, self.repo_lock.acquire()):
            return None

        return bl_extension_utils.CommandBatch(
            title="Install Package",
            batch=[
                partial(
                    bl_extension_utils.pkg_install,
                    directory=directory,
                    remote_url=url_append_defaults(repo_item.remote_url),
                    pkg_id_sequence=(pkg_id,),
                    online_user_agent=online_user_agent_from_blender(),
                    blender_version=bpy.app.version,
                    python_version=sys.version_info[:3],
                    access_token=repo_item.access_token,
                    timeout=prefs.system.network_timeout,
                    use_cache=repo_item.use_cache,
                    use_idle=is_modal,
                    python_args=bpy.app.python_args,
                )
            ],
            batch_job_limit=1,
        )

    def exec_command_finish(self, canceled):

        # TODO: it would be nice to include this message in the banner.
        def handle_error(ex):
            self.report({'ERROR'}, str(ex))

        # Unlock repositories.
        lock_result_any_failed_with_report(self, self.repo_lock.release(), report_type='WARNING')
        del self.repo_lock

        # Refresh installed packages for repositories that were operated on.
        repo_cache_store = repo_cache_store_ensure()

        pkg_manifest_local = repo_cache_store.refresh_local_from_directory(
            directory=self.repo_directory,
            error_fn=self.error_fn_from_exception,
        )
        _extension_repo_directory_validate_module(self.repo_directory)

        extensions_enabled = None
        if self.enable_on_install:
            extensions_enabled = _extensions_enabled()
            extensions_enabled.update(
                _extensions_enabled_from_repo_directory_and_pkg_id_sequence(
                    [(self.repo_directory, (self.pkg_id,))]
                )
            )

        _extensions_repo_refresh_on_change(
            repo_cache_store,
            extensions_enabled=extensions_enabled,
            compat_calc=True,
            stats_calc=True,
            error_fn=handle_error,
        )

        _preferences_ensure_enabled_all(
            addon_restore=self._addon_restore,
            handle_error=handle_error,
        )
        _preferences_theme_state_restore(self._theme_restore)

        if self._addon_restore:
            pkg_id_sequence_upgrade = self._addon_restore[0][1]
        else:
            pkg_id_sequence_upgrade = []

        if self.enable_on_install:
            _preferences_install_post_enable_on_install(
                directory=self.repo_directory,
                pkg_manifest_local=pkg_manifest_local,
                pkg_id_sequence=(self.pkg_id,),
                pkg_id_sequence_upgrade=pkg_id_sequence_upgrade,
                handle_error=handle_error,
            )

        if self.enable_on_install:
            if (extensions_enabled_test := _extensions_enabled()) != extensions_enabled:
                # Some extensions could not be enabled, re-calculate wheels which may have been setup
                # in anticipation for the add-on working.
                _extensions_repo_refresh_on_change(
                    repo_cache_store,
                    extensions_enabled=extensions_enabled_test,
                    compat_calc=False,
                    stats_calc=False,
                    error_fn=handle_error,
                )

        _extensions_repo_temp_files_make_stale(self.repo_directory)
        _extensions_repo_install_stale_package_clear(self.repo_directory, (self.pkg_id,))

        _preferences_ui_redraw()
        _preferences_ui_refresh_addons()

        # NOTE: this can be removed once upgrading from 4.1 is no longer relevant.
        if self.do_legacy_replace and (not canceled):
            self._do_legacy_replace(
                self.pkg_id,
                pkg_manifest_local,
                error_fn=lambda ex: self.report({'ERROR'}, str(ex)),
            )

    def invoke(self, context, event):
        # Only for drop logic!
        if self.properties.is_property_set("url"):
            return self._invoke_for_drop(context, event)

        return self.execute(context)

    def _is_ready_to_execute(self):
        # When a non-standard override is used, don't execute by pressing "Return"
        # because this may be an error or some other action.
        if self._draw_override is not None:
            return False
        return True

    def _invoke_for_drop(self, context, _event):
        from .bl_extension_utils import (
            url_parse_for_blender,
        )

        url = self.url
        print("DROP URL:", url)

        # Needed for UNC paths on WIN32.
        url = self.url = url_normalize(url)

        url, url_params = url_parse_for_blender(url)

        # Check if the extension is compatible with the current platform.
        # These values aren't required to be set, it just gives a more useful message than
        # failing as if the extension is not known (which happens because incompatible extensions are filtered out).
        #
        # Do this first because other issues may prompt the user to setup a new repository which is all for naught
        # if the extension isn't compatible with this system.
        if isinstance(error := pkg_manifest_params_compatible_or_error_for_this_system(
                blender_version_min=url_params.get("blender_version_min", ""),
                blender_version_max=url_params.get("blender_version_max", ""),
                platforms=[platform for platform in url_params.get("platforms", "").split(",") if platform],
                python_versions=[
                    python_version for python_version in url_params.get("python_versions", "").split(",")
                    if python_version
                ],
        ), str):
            self.report({'ERROR'}, rpt_("The extension is incompatible with this system:\n{:s}").format(error))
            return {'CANCELLED'}
        del error

        # Check if this is part of a disabled repository.
        repo_from_url_name = ""
        repo_from_url = None
        if remote_url := url_params.get("repository"):
            repo_from_url, repo_index_from_url = (
                (None, -1) if remote_url is None else
                _preferences_repo_find_by_remote_url(context, remote_url)
            )

            if repo_from_url:
                if not repo_from_url.enabled:
                    bpy.ops.extensions.repo_enable_from_drop('INVOKE_DEFAULT', repo_index=repo_index_from_url)
                    return {'CANCELLED'}
                repo_from_url_name = repo_from_url.name
            del repo_from_url, repo_index_from_url

        self._draw_override = (
            self._draw_override_progress,
            {
                "context": context,
                "op_notify": OperatorNonBlockingSyncHelper(repo_name=repo_from_url_name),
                "remote_url": remote_url,
                "repo_from_url_name": repo_from_url_name,
                "url": url,
            }
        )

        wm = context.window_manager
        wm.invoke_props_dialog(self, width=400)
        return {'RUNNING_MODAL'}

    def draw(self, context):
        while (draw_and_kwargs := self._draw_override) is not None:
            draw_fn, draw_kwargs = draw_and_kwargs
            if draw_fn(**draw_kwargs):
                # Drawing was done `self.layout` was populated with buttons
                # which will now be displayed.
                break
            # Otherwise draw the "next" function.

        if self._drop_variables is not None:
            self._draw_for_drop(context)

    def _draw_for_drop(self, _context):
        from .bl_extension_ui import (
            size_as_fmt_string,
        )
        layout = self.layout

        _repo_index, repo_name, _pkg_id, item_remote = self._drop_variables

        layout.label(
            text=iface_("Do you want to install the following {:s}?").format(item_remote.type),
            translate=False,
        )

        col = layout.column(align=True)
        col.label(text=iface_("Name: {:s}").format(item_remote.name), translate=False)
        col.label(text=iface_("Repository: {:s}").format(repo_name), translate=False)
        col.label(
            text=iface_("Size: {:s}").format(size_as_fmt_string(item_remote.archive_size, precision=0)),
            translate=False,
        )
        del col

        layout.separator()

        layout.prop(self, "enable_on_install", text=rna_prop_enable_on_install_type_map[item_remote.type])

    @staticmethod
    def _do_legacy_replace(pkg_id, pkg_manifest_local, error_fn):
        # Disables and add-on that was replaced by an extension,
        # use for upgrading 4.1 preferences or older.

        # Ensure the local meta-data exists, else there may have been a problem installing,
        # note that this does *not* check if the add-on could be enabled which is intentional.
        # It's only important the add-on installs to justify disabling the old add-on.
        # Note that there is no need to report if this was not found as failing to install will
        # already have reported.
        if not pkg_manifest_local.get(pkg_id):
            return

        from .bl_extension_ui import extensions_map_from_legacy_addons_reverse_lookup
        addon_module_name = extensions_map_from_legacy_addons_reverse_lookup(pkg_id)
        if not addon_module_name:
            # This shouldn't happen unless someone goes out of there way
            # to enable `do_legacy_replace` for a non-legacy extension.
            # Use a print here as it's such a corner case and harmless.
            print("Internal error, legacy lookup failed:", addon_module_name)
            return

        try:
            bpy.ops.preferences.addon_disable(module=addon_module_name)
        except Exception as ex:
            error_fn(ex)

    # -------------------------------------------------------------------------
    # Draw Overrides
    #
    # This attempts to make up for Blender's inability to chain dialogs together.
    # Use `self._draw_override` assigning new overrides or clearing for the default.

    # Pass 1: wait for progress to be complete.
    def _draw_override_progress(
            self,
            *,
            context,  # `bpy.types.Context`
            op_notify,  # `OperatorNonBlockingSyncHelper`
            remote_url,  # `str | None`
            repo_from_url_name,  # `str`
            url,  # `str`
    ):
        op_notify.draw(context, self)
        if op_notify.completed:
            self._draw_override = (
                self._draw_override_after_sync,
                {
                    "context": context,
                    "remote_url": remote_url,
                    "repo_from_url_name": repo_from_url_name,
                    "url": url,
                },
            )
            # Show the next immediately.
            return False

        # Drawing handled (draw again when refreshing).
        return True

    # Pass 2: take information from the repository into account.
    def _draw_override_after_sync(
            self,
            *,
            context,  # `bpy.types.Context`
            remote_url,   # `str | None`
            repo_from_url_name,  # `str`
            url,  # `str`
    ):
        from .bl_extension_utils import (
            platform_from_this_system,
        )

        # Quiet unused warning.
        _ = context

        # The parameters have already been handled.
        repo_index, repo_name, pkg_id, item_remote, item_local = extension_url_find_repo_index_and_pkg_id(url)

        if repo_index == -1:
            # The package ID could not be found, the two common causes for this error are:
            # - The platform or Blender version may be unsupported.
            # - The repository may not have been added.
            if repo_from_url_name:
                # NOTE: we *could* support reading the repository JSON that without compatibility filtering.
                # This would allow us to give a more detailed error message, noting that the extension was found
                # and the reason it isn't compatible. The down side of this is it would tie us to the decision to
                # keep syncing all extensions when Blender requests to sync with the server.
                # As some point we may want to sync only compatible extension meta-data
                # (to reduce the network overhead of incompatible packages).
                # So don't assume we have global knowledge of every URL.
                #
                # Rely on the version range on platform being included in the URL
                # (see `pkg_manifest_params_compatible_or_error_for_this_system`).
                # While this isn't a strict requirement for the server, we can assume they exist for the common case.
                self._draw_override = (
                    self._draw_override_errors,
                    {
                        "errors": [
                            "Repository found:",
                            "\"{:s}\"".format(repo_from_url_name),
                            lambda layout: layout.separator(),
                            "The extension dropped was not found in the remote repository.",
                            "Check this is part of the repository and compatible with:",
                            "Blender version {:s} on \"{:s}\".".format(
                                ".".join(str(v) for v in bpy.app.version), platform_from_this_system(),
                            )
                        ]
                    }
                )
                return False
            else:
                self._draw_override = (
                    self._draw_override_repo_add,
                    {"remote_url": "" if remote_url is None else remote_url}
                )
                return False

        if item_local is not None:
            if item_local.type == "add-on":
                message = rpt_("Add-on \"{:s}\" is already installed!")
            elif item_local.type == "theme":
                message = rpt_("Theme \"{:s}\" is already installed!")
            else:
                assert False, "Unreachable"
            self._draw_override = (
                self._draw_override_errors,
                {
                    "errors": [message.format(item_local.name)]
                }
            )
            return False

        if item_remote.block:
            self._draw_override = (
                self._draw_override_errors,
                {
                    "errors": [
                        (
                            "Repository \"{:s}\" has blocked \"{:s}\"\n"
                            "for the following reason:"
                        ).format(repo_name, pkg_id),
                        "  " + item_remote.block.reason,
                        "If you wish to install the extensions anyway,\n"
                        "manually download and install the extension from disk."
                    ]
                }
            )
            return False

        self._drop_variables = repo_index, repo_name, pkg_id, item_remote

        self.repo_index = repo_index
        self.pkg_id = pkg_id

        # Finally use the "actual" draw function.
        self._draw_override = None
        return True

    # Pass 3: errors (terminating).
    def _draw_override_errors(
            self,
            *,
            errors,
    ):
        layout = self.layout
        icon = 'ERROR'
        for error in errors:
            if isinstance(error, str):
                # Group text split by newlines more closely.
                # Without this, lines have too much vertical space.
                if "\n" in error:
                    layout_aligned = layout.column(align=True)
                    for error in error.split("\n"):
                        layout_aligned.label(text=error, translate=False, icon=icon)
                        icon = 'BLANK1'
                else:
                    layout.label(text=error, translate=False, icon=icon)
            else:
                error(layout)
            icon = 'BLANK1'

        # Only show a "Close" button since this is only showing a message.
        layout.template_popup_confirm("", text="", cancel_text="Close")
        return True

    # Pass 3: add-repository (terminating).
    def _draw_override_repo_add(
            self,
            *,
            remote_url,
    ):
        # Skip the URL prefix scheme, e.g. `https://` for less "noisy" output.
        url_split = remote_url.partition("://")
        url_for_display = url_split[2] if url_split[2] else remote_url

        layout = self.layout
        col = layout.column(align=True)
        col.label(text="The dropped extension comes from an unknown repository.")
        col.label(text="If you trust its source, add the repository and try again.")

        col.separator()
        if url_for_display:
            box = col.box()
            subcol = box.column(align=True)
            subcol.label(text=iface_("URL: {:s}").format(url_for_display), translate=False)
        else:
            col.label(text="Alternatively, download the extension to Install from Disk.")

        layout.operator_context = 'INVOKE_DEFAULT'
        props = layout.template_popup_confirm("preferences.extension_repo_add", text="Add Repository...")
        props.remote_url = remote_url
        return True


class EXTENSIONS_OT_package_uninstall(Operator, _ExtCmdMixIn):
    """Disable and uninstall the extension"""
    bl_idname = "extensions.package_uninstall"
    bl_label = "Ext Package Uninstall"
    __slots__ = _ExtCmdMixIn.cls_slots

    repo_directory: rna_prop_directory
    repo_index: rna_prop_repo_index

    pkg_id: rna_prop_pkg_id

    def exec_command_iter(self, is_modal):
        from . import bl_extension_utils

        # pylint: disable-next=attribute-defined-outside-init
        self._theme_restore = _preferences_theme_state_create()

        directory = _repo_dir_and_index_get(self.repo_index, self.repo_directory, self.report)
        if not directory:
            return None
        self.repo_directory = directory

        if (repo_item := _extensions_repo_from_directory_and_report(directory, self.report)) is None:
            return None

        if not (pkg_id := self.pkg_id):
            self.report({'ERROR'}, "Package ID not set")
            return None

        # No need to store the result because the add-ons aren't going to be enabled again.
        _preferences_ensure_disabled(
            repo_item=repo_item,
            pkg_id_sequence=(pkg_id,),
            default_set=True,
            error_fn=lambda ex: self.report({'ERROR'}, str(ex)),
        )

        # Lock repositories.
        # pylint: disable-next=attribute-defined-outside-init
        self.repo_lock = bl_extension_utils.RepoLock(
            repo_directories=[repo_item.directory],
            cookie=cookie_from_session(),
        )
        if lock_result_any_failed_with_report(self, self.repo_lock.acquire()):
            return None

        return bl_extension_utils.CommandBatch(
            title="Uninstall Package",
            batch=[
                partial(
                    bl_extension_utils.pkg_uninstall,
                    directory=directory,
                    user_directory=repo_user_directory(repo_item.module),
                    pkg_id_sequence=(pkg_id, ),
                    use_idle=is_modal,
                    python_args=bpy.app.python_args,
                ),
            ],
            batch_job_limit=1,
        )

    def exec_command_finish(self, canceled):

        # TODO: it would be nice to include this message in the banner.
        def handle_error(ex):
            self.report({'ERROR'}, str(ex))

        _extensions_repo_temp_files_make_stale(repo_directory=self.repo_directory)
        _extensions_repo_uninstall_stale_package_fallback(
            repo_directory=self.repo_directory,
            pkg_id_sequence=[self.pkg_id],
        )

        # Refresh installed packages for repositories that were operated on.
        repo_cache_store = repo_cache_store_ensure()
        repo_item = _extensions_repo_from_directory(self.repo_directory)

        if repo_item.remote_url == "":
            # Re-generate JSON meta-data from TOML files (needed for offline repository).
            # NOTE: This could be slow with many local extensions,
            # we could simply remove the package that was uninstalled.
            repo_cache_store.refresh_remote_from_directory(
                directory=self.repo_directory,
                error_fn=self.error_fn_from_exception,
                force=True,
            )
        del repo_item

        # Unlock repositories.
        lock_result_any_failed_with_report(self, self.repo_lock.release(), report_type='WARNING')
        del self.repo_lock

        repo_cache_store.refresh_local_from_directory(
            directory=self.repo_directory,
            error_fn=self.error_fn_from_exception,
        )

        _extensions_repo_refresh_on_change(
            repo_cache_store,
            extensions_enabled=_extensions_enabled(),
            compat_calc=True,
            stats_calc=True,
            error_fn=handle_error,
        )

        _preferences_theme_state_restore(self._theme_restore)

        _preferences_ui_redraw()
        _preferences_ui_refresh_addons()


# Only exists for an error message.
class EXTENSIONS_OT_package_uninstall_system(Operator):
    # Copy `EXTENSIONS_OT_package_uninstall` doc-string.
    bl_label = "Uninstall"

    bl_idname = "extensions.package_uninstall_system"
    bl_options = {'INTERNAL'}

    @classmethod
    def poll(cls, _contest):
        cls.poll_message_set("System extensions are read-only and cannot be uninstalled")
        return False

    @classmethod
    def description(cls, _context, _props):
        return tip_(EXTENSIONS_OT_package_uninstall.__doc__)

    def execute(self, _context):
        return {'CANCELLED'}


class EXTENSIONS_OT_package_disable(Operator):
    """Turn off this extension"""
    bl_idname = "extensions.package_disable"
    bl_label = "Disable extension"

    def execute(self, _context):
        self.report({'WARNING'}, "Disabling themes is not yet supported")
        return {'CANCELLED'}


class EXTENSIONS_OT_package_theme_enable(Operator):
    """Turn off this theme"""
    bl_idname = "extensions.package_theme_enable"
    bl_label = "Enable theme extension"

    pkg_id: rna_prop_pkg_id
    repo_index: rna_prop_repo_index

    def execute(self, _context):
        repo_item = extension_repos_read_index(self.repo_index)
        extension_theme_enable(repo_item.directory, self.pkg_id)
        print(repo_item.directory, self.pkg_id)
        return {'FINISHED'}


class EXTENSIONS_OT_package_theme_disable(Operator):
    """Turn off this theme"""
    bl_idname = "extensions.package_theme_disable"
    bl_label = "Disable theme extension"

    pkg_id: rna_prop_pkg_id
    repo_index: rna_prop_repo_index

    def execute(self, context):
        repo_item = extension_repos_read_index(self.repo_index)
        dirpath = os.path.join(repo_item.directory, self.pkg_id)
        if os.path.samefile(dirpath, os.path.dirname(context.preferences.themes[0].filepath)):
            bpy.ops.preferences.reset_default_theme()
        return {'FINISHED'}


# -----------------------------------------------------------------------------
# Non Wrapped Actions
#
# These actions don't wrap command line access.
#
# NOTE: create/destroy might not be best names.


class EXTENSIONS_OT_status_clear_errors(Operator):
    bl_idname = "extensions.status_clear_errors"
    bl_label = "Clear Status"

    def execute(self, _context):
        from .bl_extension_ui import display_errors
        display_errors.clear()
        _preferences_ui_redraw()
        return {'FINISHED'}


class EXTENSIONS_OT_status_clear(Operator):
    bl_idname = "extensions.status_clear"
    bl_label = "Clear Status"

    def execute(self, _context):
        repo_status_text.running = False
        repo_status_text.log.clear()
        _preferences_ui_redraw()
        return {'FINISHED'}


class EXTENSIONS_OT_package_mark_set(Operator):
    bl_idname = "extensions.package_mark_set"
    bl_label = "Mark Package"

    pkg_id: rna_prop_pkg_id
    repo_index: rna_prop_repo_index

    def execute(self, _context):
        key = (self.pkg_id, self.repo_index)
        blender_extension_mark.add(key)
        _preferences_ui_redraw()
        return {'FINISHED'}


class EXTENSIONS_OT_package_mark_clear(Operator):
    bl_idname = "extensions.package_mark_clear"
    bl_label = "Clear Marked Package"

    pkg_id: rna_prop_pkg_id
    repo_index: rna_prop_repo_index

    def execute(self, _context):
        key = (self.pkg_id, self.repo_index)
        blender_extension_mark.discard(key)
        _preferences_ui_redraw()
        return {'FINISHED'}


class EXTENSIONS_OT_package_mark_set_all(Operator):
    bl_idname = "extensions.package_mark_set_all"
    bl_label = "Mark All Packages"

    def execute(self, context):
        from .bl_extension_ui import ExtensionUI_Visibility

        repo_cache_store = repo_cache_store_ensure()

        ui_visibility = None if is_background else ExtensionUI_Visibility(context, repo_cache_store)

        for repo_index, (
                pkg_manifest_remote,
                pkg_manifest_local,
        ) in enumerate(zip(
            repo_cache_store.pkg_manifest_from_remote_ensure(error_fn=print),
            repo_cache_store.pkg_manifest_from_local_ensure(error_fn=print),
        )):
            if pkg_manifest_remote is not None:
                for pkg_id in pkg_manifest_remote.keys():
                    key = pkg_id, repo_index
                    if ui_visibility is not None:
                        if not ui_visibility.test(key):
                            continue
                    blender_extension_mark.add(key)
            if pkg_manifest_local is not None:
                for pkg_id in pkg_manifest_local.keys():
                    key = pkg_id, repo_index
                    if ui_visibility is not None:
                        if not ui_visibility.test(key):
                            continue
                    blender_extension_mark.add(key)
        _preferences_ui_redraw()
        return {'FINISHED'}


class EXTENSIONS_OT_package_mark_clear_all(Operator):
    bl_idname = "extensions.package_mark_clear_all"
    bl_label = "Clear All Marked Packages"

    def execute(self, _context):
        blender_extension_mark.clear()
        return {'FINISHED'}


class EXTENSIONS_OT_package_show_set(Operator):
    bl_idname = "extensions.package_show_set"
    bl_label = "Show Package Set"

    pkg_id: rna_prop_pkg_id
    repo_index: rna_prop_repo_index

    def execute(self, _context):
        key = (self.pkg_id, self.repo_index)
        blender_extension_show.add(key)
        _preferences_ui_redraw()
        return {'FINISHED'}


class EXTENSIONS_OT_package_show_clear(Operator):
    bl_idname = "extensions.package_show_clear"
    bl_label = "Show Package Clear"

    pkg_id: rna_prop_pkg_id
    repo_index: rna_prop_repo_index

    def execute(self, _context):
        key = (self.pkg_id, self.repo_index)
        blender_extension_show.discard(key)
        _preferences_ui_redraw()
        return {'FINISHED'}


class EXTENSIONS_OT_package_show_settings(Operator):
    bl_idname = "extensions.package_show_settings"
    bl_label = "Show Settings"

    pkg_id: rna_prop_pkg_id
    repo_index: rna_prop_repo_index

    def execute(self, _context):
        repo_item = extension_repos_read_index(self.repo_index)
        bpy.ops.preferences.addon_show(
            module="{:s}.{:s}.{:s}".format(_ext_base_pkg_idname, repo_item.module, self.pkg_id),
        )
        return {'FINISHED'}


# -----------------------------------------------------------------------------
# Testing Operators
#

class EXTENSIONS_OT_package_obselete_marked(Operator):
    """Zeroes package versions, useful for development - to test upgrading"""
    bl_idname = "extensions.package_obsolete_marked"
    bl_label = "Obsolete Marked"

    def execute(self, _context):
        from . import bl_extension_utils

        repos_all = extension_repos_read()
        repo_cache_store = repo_cache_store_ensure()

        pkg_manifest_local_all = list(repo_cache_store.pkg_manifest_from_local_ensure(error_fn=print))
        repo_pkg_map = _pkg_marked_by_repo(repo_cache_store, pkg_manifest_local_all)
        found = False

        repos_lock = [repos_all[repo_index].directory for repo_index in sorted(repo_pkg_map.keys())]

        with bl_extension_utils.RepoLockContext(
                repo_directories=repos_lock,
                cookie=cookie_from_session(),
        ) as lock_result:
            if lock_result_any_failed_with_report(self, lock_result):
                return {'CANCELLED'}

            directories_update = set()

            for repo_index, pkg_id_sequence in sorted(repo_pkg_map.items()):
                repo_item = repos_all[repo_index]
                pkg_manifest_local = repo_cache_store.refresh_local_from_directory(
                    repo_item.directory,
                    error_fn=print,
                )
                found_for_repo = False
                for pkg_id in pkg_id_sequence:
                    is_installed = pkg_id in pkg_manifest_local
                    if not is_installed:
                        continue

                    bl_extension_utils.pkg_make_obsolete_for_testing(repo_item.directory, pkg_id)
                    found = True
                    found_for_repo = True

                if found_for_repo:
                    directories_update.add(repo_item.directory)

            if not found:
                self.report({'ERROR'}, "No installed packages marked")
                return {'CANCELLED'}

            for directory in directories_update:
                repo_cache_store.refresh_remote_from_directory(
                    directory=directory,
                    error_fn=print,
                    force=True,
                )
                repo_cache_store.refresh_local_from_directory(
                    directory=directory,
                    error_fn=print,
                )

            repo_stats_calc()

            _preferences_ui_redraw()

        return {'FINISHED'}


class EXTENSIONS_OT_repo_lock_all(Operator):
    """Lock repositories - to test locking"""
    bl_idname = "extensions.repo_lock_all"
    bl_label = "Lock All Repositories (Testing)"

    lock = None

    def execute(self, _context):
        from . import bl_extension_utils

        repos_all = extension_repos_read()
        repos_lock = [repo_item.directory for repo_item in repos_all]

        lock_handle = bl_extension_utils.RepoLock(
            repo_directories=repos_lock,
            cookie=cookie_from_session(),
        )
        lock_result = lock_handle.acquire()
        if lock_result_any_failed_with_report(self, lock_result):
            # At least one lock failed, unlock all and return.
            lock_handle.release()
            return {'CANCELLED'}

        self.report({'INFO'}, rpt_("Locked {:d} repos(s)").format(len(lock_result)))
        EXTENSIONS_OT_repo_lock_all.lock = lock_handle
        return {'FINISHED'}


class EXTENSIONS_OT_repo_unlock_all(Operator):
    """Unlock repositories - to test unlocking"""
    bl_idname = "extensions.repo_unlock_all"
    bl_label = "Unlock All Repositories (Testing)"

    def execute(self, _context):
        lock_handle = EXTENSIONS_OT_repo_lock_all.lock
        if lock_handle is None:
            self.report({'ERROR'}, "Lock not held!")
            return {'CANCELLED'}

        lock_result = lock_handle.release()

        EXTENSIONS_OT_repo_lock_all.lock = None

        if lock_result_any_failed_with_report(self, lock_result):
            # This isn't canceled, but there were issues unlocking.
            return {'FINISHED'}

        self.report({'INFO'}, rpt_("Unlocked {:d} repos(s)").format(len(lock_result)))
        return {'FINISHED'}


class EXTENSIONS_OT_userpref_tags_set(Operator):
    """Set the value of all tags"""
    bl_idname = "extensions.userpref_tags_set"
    bl_label = "Set Extension Tags"
    bl_options = {'INTERNAL'}

    value: BoolProperty(
        name="Value",
        description="Enable or disable all tags",
        options={'SKIP_SAVE'},
    )
    data_path: StringProperty(
        name="Data Path",
        options={'SKIP_SAVE'},
    )

    def execute(self, context):
        from .bl_extension_ui import (
            tags_clear,
            tags_refresh,
        )

        wm = context.window_manager

        value = self.value
        tags_attr = self.data_path

        # Internal error, could happen if called from some unexpected place.
        if tags_attr not in {"extension_tags", "addon_tags"}:
            return {'CANCELLED'}

        tags_clear(wm, tags_attr)
        if value is False:
            tags_refresh(wm, tags_attr, default_value=False)

        _preferences_ui_redraw()
        return {'FINISHED'}


# NOTE: this is a modified version of `PREFERENCES_OT_addon_show`.
# It would make most sense to extend this operator to support showing extensions to upgrade (eventually).
class EXTENSIONS_OT_userpref_show_for_update(Operator):
    """Open extensions preferences"""
    bl_idname = "extensions.userpref_show_for_update"
    bl_label = ""
    bl_options = {'INTERNAL'}

    def execute(self, context):
        from .bl_extension_ui import tags_clear

        wm = context.window_manager
        prefs = context.preferences

        prefs.active_section = 'EXTENSIONS'

        # Extensions may be of any type, so show all.
        wm.extension_type = 'ALL'

        # Show only extensions that will be updated.
        wm.extension_show_panel_installed = True
        wm.extension_show_panel_available = False

        # Clear other filtering option.
        wm.extension_search = ""
        tags_clear(wm, "extension_tags")

        bpy.ops.screen.userpref_show('INVOKE_DEFAULT')

        return {'FINISHED'}


# NOTE: this is a wrapper for `SCREEN_OT_userpref_show`.
# It exists *only* to add a poll function which sets a message when offline mode is forced.
class EXTENSIONS_OT_userpref_show_online(Operator):
    """Show system preferences "Network" panel to allow online access"""
    bl_idname = "extensions.userpref_show_online"
    bl_label = ""
    bl_options = {'INTERNAL'}

    @classmethod
    def poll(cls, _context):
        if bpy.app.online_access_override:
            if not bpy.app.online_access:
                cls.poll_message_set("Blender was launched in offline mode, which cannot be changed at runtime")
                return False
        return True

    def execute(self, _context):
        bpy.ops.screen.userpref_show('INVOKE_DEFAULT', section='SYSTEM')
        return {'FINISHED'}


class EXTENSIONS_OT_userpref_allow_online(Operator):
    """Allow internet access. Blender may access configured online extension repositories. """ \
        """Installed third party add-ons may access the internet for their own functionality"""
    bl_idname = "extensions.userpref_allow_online"
    bl_label = ""
    bl_options = {'INTERNAL'}

    @classmethod
    def poll(cls, _context):
        if bpy.app.online_access_override:
            if not bpy.app.online_access:
                cls.poll_message_set("Blender was launched in offline mode, which cannot be changed at runtime")
                return False
        return True

    def execute(self, context):
        context.preferences.system.use_online_access = True
        return {'FINISHED'}


# NOTE: this is a wrapper for `extensions.userpref_allow_online`.
# It exists *only* show a dialog.
class EXTENSIONS_OT_userpref_allow_online_popup(Operator):
    """Allow internet access. Blender may access configured online extension repositories. """ \
        """Installed third party add-ons may access the internet for their own functionality"""
    bl_idname = "extensions.userpref_allow_online_popup"
    bl_label = ""
    bl_options = {'INTERNAL'}

    def execute(self, _context):
        bpy.ops.screen.userpref_show('INVOKE_DEFAULT', section='SYSTEM')
        return {'FINISHED'}

    def invoke(self, context, _event):
        wm = context.window_manager
        if bpy.app.online_access_override:
            # No Cancel/Confirm buttons.
            wm.invoke_popup(
                self,
                width=400,
            )
        else:
            wm.invoke_props_dialog(
                self,
                width=400,
                confirm_text="Allow Online Access",
                title="Install Extension",
            )
        return {'RUNNING_MODAL'}

    def draw(self, _context):
        layout = self.layout
        col = layout.column()
        if bpy.app.online_access_override:
            lines = (
                rpt_("Online access required to install or update."),
                "",
                rpt_("Launch Blender without --offline-mode"),
            )
        else:
            lines = (
                rpt_("Please enable Online Access from the System settings."),
                "",
                rpt_("Internet access is required to install extensions from the internet."),
            )
        for line in lines:
            col.label(text=line, translate=False)


# -----------------------------------------------------------------------------
# Register
#
classes = (
    EXTENSIONS_OT_repo_sync,
    EXTENSIONS_OT_repo_sync_all,
    EXTENSIONS_OT_repo_refresh_all,
    EXTENSIONS_OT_repo_enable_from_drop,
    EXTENSIONS_OT_repo_unlock,

    EXTENSIONS_OT_package_install_files,
    EXTENSIONS_OT_package_install,
    EXTENSIONS_OT_package_uninstall,
    EXTENSIONS_OT_package_uninstall_system,
    EXTENSIONS_OT_package_disable,

    EXTENSIONS_OT_package_theme_enable,
    EXTENSIONS_OT_package_theme_disable,

    EXTENSIONS_OT_package_upgrade_all,
    EXTENSIONS_OT_package_install_marked,
    EXTENSIONS_OT_package_uninstall_marked,

    # UI only operator (to select a package).
    EXTENSIONS_OT_status_clear_errors,
    EXTENSIONS_OT_status_clear,
    EXTENSIONS_OT_package_show_set,
    EXTENSIONS_OT_package_show_clear,
    EXTENSIONS_OT_package_mark_set,
    EXTENSIONS_OT_package_mark_clear,
    EXTENSIONS_OT_package_mark_set_all,
    EXTENSIONS_OT_package_mark_clear_all,
    EXTENSIONS_OT_package_show_settings,

    EXTENSIONS_OT_package_obselete_marked,
    EXTENSIONS_OT_repo_lock_all,
    EXTENSIONS_OT_repo_unlock_all,

    EXTENSIONS_OT_userpref_tags_set,
    EXTENSIONS_OT_userpref_show_for_update,
    EXTENSIONS_OT_userpref_show_online,
    EXTENSIONS_OT_userpref_allow_online,
    EXTENSIONS_OT_userpref_allow_online_popup,
)


def register():
    for cls in classes:
        bpy.utils.register_class(cls)


def unregister():
    for cls in classes:
        bpy.utils.unregister_class(cls)


if __name__ == "__main__":
    register()
