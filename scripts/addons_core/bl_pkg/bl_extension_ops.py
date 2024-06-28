# SPDX-FileCopyrightText: 2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
Blender, thin wrapper around ``blender_extension_utils``.
Where the operator shows progress, any errors and supports canceling operations.
"""

__all__ = (
    "extension_repos_read",
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
    pgettext_rpt as rpt_,

)

from . import (
    cookie_from_session,
    repo_cache_store_ensure,
    repo_stats_calc,
    repo_status_text,
)

rna_prop_url = StringProperty(name="URL", subtype='FILE_PATH', options={'HIDDEN'})
rna_prop_directory = StringProperty(name="Repo Directory", subtype='FILE_PATH')
rna_prop_repo_index = IntProperty(name="Repo Index", default=-1)
rna_prop_remote_url = StringProperty(name="Repo URL", subtype='FILE_PATH')
rna_prop_pkg_id = StringProperty(name="Package ID")

rna_prop_enable_on_install = BoolProperty(
    name="Enable on Install",
    description="Enable after installing",
    default=True,
)
rna_prop_enable_on_install_type_map = {
    "add-on": "Enable Add-on",
    "theme": "Set Current Theme",
}


def url_append_defaults(url):
    from .bl_extension_utils import url_append_query_for_blender
    return url_append_query_for_blender(url, blender_version=bpy.app.version)


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

        layout = _operator_draw_hide_buttons_hack(op.layout)
        text, icon = update_ui_text()
        layout.label(text=text, icon=icon)


# -----------------------------------------------------------------------------
# Internal Utilities
#

def _operator_draw_hide_buttons_hack(layout):
    # EVIL! There is no good way to hide button on operator dialogs,
    # so use a bad way (scale them to oblivion!)
    # This could be supported by the internals, for now it's not though.
    col = layout.column()
    y = 1000.0
    col.scale_y = y
    layout.scale_y = 1.0 / y
    return col


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

    for repo_index, (
            pkg_manifest_local,
            pkg_manifest_remote,
    ) in enumerate(zip(
        repo_cache_store.pkg_manifest_from_local_ensure(error_fn=print),
        repo_cache_store.pkg_manifest_from_remote_ensure(error_fn=print),
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
    for directory, lock_result_for_repo in lock_result.items():
        if lock_result_for_repo is None:
            continue
        print("Error \"{:s}\" locking \"{:s}\"".format(lock_result_for_repo, repr(directory)))
        op.report({report_type}, lock_result_for_repo)
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
        directory, remote_url = repo_paths_or_none(repo_item)
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
    platforms,  # `List[str]`
):  # `Optional[str]`
    # Return true if the parameters are compatible with this system.
    from .bl_extension_utils import (
        pkg_manifest_params_compatible_or_error,
        platform_from_this_system,
    )
    return pkg_manifest_params_compatible_or_error(
        # Parameters.
        blender_version_min=blender_version_min,
        blender_version_max=blender_version_max,
        platforms=platforms,
        # This system.
        this_platform=platform_from_this_system(),
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


def _preferences_ensure_disabled(*, repo_item, pkg_id_sequence, default_set):
    import sys
    import addon_utils

    result = {}
    errors = []

    def handle_error(ex):
        print("Error:", ex)
        errors.append(str(ex))

    modules_clear = []

    module_base_elem = ("bl_ext", repo_item.module)

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

        addon_utils.disable(addon_module_name, default_set=default_set, handle_error=handle_error)

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

    return result, errors


def _preferences_ensure_enabled(*, repo_item, pkg_id_sequence, result, handle_error):
    import addon_utils
    for addon_module_name, (loaded_default, loaded_state) in result.items():
        # The module was not loaded, so no need to restore it.
        if not loaded_state:
            continue

        addon_utils.enable(addon_module_name, default_set=loaded_default, handle_error=handle_error)


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

            addon_module_name = "bl_ext.{:s}.{:s}".format(repo_item.module, pkg_id)
            addon_utils.enable(addon_module_name, default_set=True, handle_error=handle_error)
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
    addon_utils.modules._is_first = True


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

def _extensions_wheel_filter_for_platform(wheels):

    # Copied from `wheel.bwheel_dist.get_platform(..)` which isn't part of Python.
    # This misses some additional checks which aren't supported by official Blender builds,
    # it's highly doubtful users ever run into this but we could add extend this if it's really needed.
    # (e.g. `linux-i686` on 64 bit systems & `linux-armv7l`).
    import sysconfig
    platform_tag_current = sysconfig.get_platform().replace("-", "_")

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
        # TODO: Match Python & ABI tags.
        _python_tag, _abi_tag, platform_tag = wheel_filename_split[-3:]

        if platform_tag in {"any", platform_tag_current}:
            pass
        elif platform_tag_current.startswith("macosx_") and (
                # FIXME: `macosx_11.00` should be `macosx_11_0`.
                platform_tag.startswith("macosx_") and
                # Ignore the MACOSX version, ensure `arm64` suffix.
                platform_tag.endswith("_" + platform_tag_current.rpartition("_")[2])
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
            # Useful to know, can quiet print in the future.
            print(
                "Skipping wheel for other system",
                "({:s} != {:s}):".format(platform_tag, platform_tag_current),
                wheel_filename,
            )
            continue

        wheels_compatible.append(wheel)
    return wheels_compatible


def _extensions_repo_sync_wheels(repo_cache_store):
    """
    This function collects all wheels from all packages and ensures the packages are either extracted or removed
    when they are no longer used.
    """
    from .bl_extension_local import sync

    repos_all = extension_repos_read()

    wheel_list = []

    for repo_index, pkg_manifest_local in enumerate(repo_cache_store.pkg_manifest_from_local_ensure(
            error_fn=print,
            ignore_missing=True,
    )):
        repo = repos_all[repo_index]
        repo_module = repo.module
        repo_directory = repo.directory
        for pkg_id, item_local in pkg_manifest_local.items():
            pkg_dirpath = os.path.join(repo_directory, pkg_id)
            wheels_rel = item_local.wheels
            if not wheels_rel:
                continue

            # Filter only the wheels for this platform.
            wheels_rel = _extensions_wheel_filter_for_platform(wheels_rel)
            if not wheels_rel:
                continue

            wheels_abs = []
            for filepath_rel in wheels_rel:
                filepath_abs = os.path.join(pkg_dirpath, filepath_rel)
                if not os.path.exists(filepath_abs):
                    continue
                wheels_abs.append(filepath_abs)

            if not wheels_abs:
                continue

            unique_pkg_id = "{:s}.{:s}".format(repo_module, pkg_id)
            wheel_list.append((unique_pkg_id, wheels_abs))

    extensions = bpy.utils.user_resource('EXTENSIONS')
    local_dir = os.path.join(extensions, ".local")

    sync(
        local_dir=local_dir,
        wheel_list=wheel_list,
    )


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
                    else:
                        op.report({'WARNING'}, msg)
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


class EXTENSIONS_OT_dummy_progress(Operator, _ExtCmdMixIn):
    bl_idname = "extensions.dummy_progress"
    bl_label = "Ext Demo"
    __slots__ = _ExtCmdMixIn.cls_slots

    def exec_command_iter(self, is_modal):
        from . import bl_extension_utils

        return bl_extension_utils.CommandBatch(
            title="Dummy Progress",
            batch=[
                partial(
                    bl_extension_utils.dummy_progress,
                    use_idle=is_modal,
                ),
            ],
            batch_job_limit=1,
        )

    def exec_command_finish(self, canceled):
        _preferences_ui_redraw()


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
                )
            )
            repos_lock.append(repo_item.directory)

        # Lock repositories.
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
            return "Refresh the list of extensions for the active repository"
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
                ))
                repos_lock.append(repo_item.directory)

        # Lock repositories.
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

    def _exceptions_as_report(self, repo_name, ex):
        self.report({'WARNING'}, "{:s}: {:s}".format(repo_name, str(ex)))

    def execute(self, _context):
        import addon_utils

        repos_all = extension_repos_read()
        repo_cache_store = repo_cache_store_ensure()

        for repo_item in repos_all:
            # Re-generate JSON meta-data from TOML files (needed for offline repository).
            repo_cache_store.refresh_remote_from_directory(
                directory=repo_item.directory,
                error_fn=lambda ex: self._exceptions_as_report(repo_item.name, ex),
                force=True,
            )
            repo_cache_store.refresh_local_from_directory(
                directory=repo_item.directory,
                error_fn=lambda ex: self._exceptions_as_report(repo_item.name, ex),
            )

        # In-line `bpy.ops.preferences.addon_refresh`.
        addon_utils.modules_refresh()

        _preferences_ui_redraw()
        _preferences_ui_refresh_addons()

        return {'FINISHED'}


# Show a dialog when dropping an extensions for a disabled repository.
class EXTENSIONS_OT_repo_enable_from_drop(Operator):
    bl_idname = "extensions.repo_enable_from_drop"
    bl_label = "Enable Repository from Drop"
    bl_options = {'INTERNAL'}

    repo_index: rna_prop_repo_index

    __slots__ = (
        "_repo_name",
        "_repo_remote_url",
    )

    def invoke(self, context, _event):
        print(self.repo_index)
        if (repo := repo_lookup_by_index_or_none_with_report(self.repo_index, self.report)) is None:
            return {'CANCELLED'}
        self._repo_name = repo.name
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
            return "Upgrade all the extensions to their latest version for the active repository"
        return ""  # Default.

    def exec_command_iter(self, is_modal):
        from . import bl_extension_utils
        self._repo_directories = set()
        self._addon_restore = []
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

            for pkg_id, item_remote in pkg_manifest_remote.items():
                item_local = pkg_manifest_local.get(pkg_id)
                if item_local is None:
                    # Not installed.
                    continue

                if item_remote.version != item_local.version:
                    packages_to_upgrade[repo_index].append(pkg_id)
                    package_count += 1

            if packages_to_upgrade[repo_index]:
                handle_addons_info.append((repos_all[repo_index], list(packages_to_upgrade[repo_index])))

        cmd_batch = []
        for repo_index, pkg_id_sequence in enumerate(packages_to_upgrade):
            if not pkg_id_sequence:
                continue

            repo_item = repos_all[repo_index]
            for pkg_id_sequence in _sequence_split_with_job_limit(pkg_id_sequence, network_connection_limit):
                cmd_batch.append(partial(
                    bl_extension_utils.pkg_install,
                    directory=repo_item.directory,
                    remote_url=url_append_defaults(repo_item.remote_url),
                    pkg_id_sequence=pkg_id_sequence,
                    online_user_agent=online_user_agent_from_blender(),
                    blender_version=bpy.app.version,
                    access_token=repo_item.access_token,
                    timeout=prefs.system.network_timeout,
                    use_cache=repo_item.use_cache,
                    use_idle=is_modal,
                ))
            self._repo_directories.add(repo_item.directory)

        if not cmd_batch:
            self.report({'INFO'}, "No installed packages to update")
            return None

        # Lock repositories.
        self.repo_lock = bl_extension_utils.RepoLock(
            repo_directories=list(self._repo_directories),
            cookie=cookie_from_session(),
        )
        if lock_result_any_failed_with_report(self, self.repo_lock.acquire()):
            return None

        for repo_item, pkg_id_sequence in handle_addons_info:
            result, errors = _preferences_ensure_disabled(
                repo_item=repo_item,
                pkg_id_sequence=pkg_id_sequence,
                default_set=False,
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

        repo_stats_calc()

        # TODO: it would be nice to include this message in the banner.
        def handle_error(ex):
            self.report({'ERROR'}, str(ex))

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
        from . import bl_extension_utils

        repos_all = extension_repos_read()
        repo_cache_store = repo_cache_store_ensure()

        pkg_manifest_remote_all = list(repo_cache_store.pkg_manifest_from_remote_ensure(
            error_fn=self.error_fn_from_exception,
        ))
        repo_pkg_map = _pkg_marked_by_repo(repo_cache_store, pkg_manifest_remote_all)
        self._repo_directories = set()
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

            for pkg_id_sequence in _sequence_split_with_job_limit(pkg_id_sequence, network_connection_limit):
                cmd_batch.append(partial(
                    bl_extension_utils.pkg_install,
                    directory=repo_item.directory,
                    remote_url=url_append_defaults(repo_item.remote_url),
                    pkg_id_sequence=pkg_id_sequence,
                    online_user_agent=online_user_agent_from_blender(),
                    blender_version=bpy.app.version,
                    access_token=repo_item.access_token,
                    timeout=prefs.system.network_timeout,
                    use_cache=repo_item.use_cache,
                    use_idle=is_modal,
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
            self.report({'ERROR'}, "No un-installed packages marked")
            return None

        # Lock repositories.
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

        # Refresh installed packages for repositories that were operated on.
        repo_cache_store = repo_cache_store_ensure()
        for directory in self._repo_directories:
            repo_cache_store.refresh_local_from_directory(
                directory=directory,
                error_fn=self.error_fn_from_exception,
            )

        _extensions_repo_sync_wheels(repo_cache_store)
        repo_stats_calc()

        # TODO: it would be nice to include this message in the banner.
        def handle_error(ex):
            self.report({'ERROR'}, str(ex))

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

        _preferences_ui_redraw()
        _preferences_ui_refresh_addons()


class EXTENSIONS_OT_package_uninstall_marked(Operator, _ExtCmdMixIn):
    bl_idname = "extensions.package_uninstall_marked"
    bl_label = "Ext Package Uninstall Marked"
    __slots__ = (
        *_ExtCmdMixIn.cls_slots,
        "_repo_directories",
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

        self._repo_directories = set()
        self._theme_restore = _preferences_theme_state_create()

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
                ))
            self._repo_directories.add(repo_item.directory)
            package_count += len(pkg_id_sequence)

            handle_addons_info.append((repo_item, pkg_id_sequence))

        if not cmd_batch:
            self.report({'ERROR'}, "No installed packages marked")
            return None

        # Lock repositories.
        self.repo_lock = bl_extension_utils.RepoLock(
            repo_directories=list(self._repo_directories),
            cookie=cookie_from_session(),
        )
        if lock_result_any_failed_with_report(self, self.repo_lock.acquire()):
            return None

        for repo_item, pkg_id_sequence in handle_addons_info:
            # No need to store the result (`_`) because the add-ons aren't going to be enabled again.
            _, errors = _preferences_ensure_disabled(
                repo_item=repo_item,
                pkg_id_sequence=pkg_id_sequence,
                default_set=True,
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

        # Refresh installed packages for repositories that were operated on.
        repo_cache_store = repo_cache_store_ensure()
        for directory in self._repo_directories:
            repo_cache_store.refresh_local_from_directory(
                directory=directory,
                error_fn=self.error_fn_from_exception,
            )

        _extensions_repo_sync_wheels(repo_cache_store)
        repo_stats_calc()

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
    _drop_variables = None
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
        from . import bl_extension_utils
        from .bl_extension_utils import (
            pkg_manifest_dict_from_archive_or_error,
            pkg_is_legacy_addon,
        )

        self._addon_restore = []
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
        self.repo_directory = directory
        self.pkg_id_sequence = pkg_id_sequence

        # Detect upgrade.
        if pkg_id_sequence:
            repo_cache_store = repo_cache_store_ensure()
            pkg_manifest_local = repo_cache_store.refresh_local_from_directory(
                directory=self.repo_directory,
                error_fn=self.error_fn_from_exception,
            )
            if pkg_manifest_local is not None:
                pkg_id_sequence_upgrade = [pkg_id for pkg_id in pkg_id_sequence if pkg_id in pkg_manifest_local]
                if pkg_id_sequence_upgrade:
                    result, errors = _preferences_ensure_disabled(
                        repo_item=repo_item,
                        pkg_id_sequence=pkg_id_sequence_upgrade,
                        default_set=False,
                    )
                    self._addon_restore.append((repo_item, pkg_id_sequence_upgrade, result))
            del repo_cache_store, pkg_manifest_local

        # Lock repositories.
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
                    use_idle=is_modal,
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

        # Unlock repositories.
        lock_result_any_failed_with_report(self, self.repo_lock.release(), report_type='WARNING')
        del self.repo_lock

        pkg_manifest_local = repo_cache_store.refresh_local_from_directory(
            directory=self.repo_directory,
            error_fn=self.error_fn_from_exception,
        )

        _extensions_repo_sync_wheels(repo_cache_store)
        repo_stats_calc()

        # TODO: it would be nice to include this message in the banner.

        def handle_error(ex):
            self.report({'ERROR'}, str(ex))

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
        # it's possible to download large extensions before installing or to down-grade to older versions.
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
            self._drop_variables = True
            self._legacy_drop = None

            from .bl_extension_utils import pkg_manifest_dict_from_archive_or_error

            if not self._repos_valid_for_install(context):
                self.report({'ERROR'}, "No user repositories")
                return {'CANCELLED'}

            if isinstance(result := pkg_manifest_dict_from_archive_or_error(filepath), str):
                self.report({'ERROR'}, "Error in manifest {:s}".format(result))
                return {'CANCELLED'}

            pkg_id = result["id"]
            pkg_type = result["type"]
            del result

            self._drop_variables = pkg_id, pkg_type
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


class EXTENSIONS_OT_package_install(Operator, _ExtCmdMixIn):
    """Download and install the extension"""
    bl_idname = "extensions.package_install"
    bl_label = "Install Extension"
    __slots__ = _ExtCmdMixIn.cls_slots

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
        from . import bl_extension_utils

        if not self._is_ready_to_execute():
            return None

        self._addon_restore = []
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

        # Detect upgrade.
        repo_cache_store = repo_cache_store_ensure()
        pkg_manifest_local = repo_cache_store.refresh_local_from_directory(
            directory=self.repo_directory,
            error_fn=self.error_fn_from_exception,
        )
        is_installed = pkg_manifest_local is not None and (pkg_id in pkg_manifest_local)
        del repo_cache_store, pkg_manifest_local

        if is_installed:
            pkg_id_sequence = (pkg_id,)
            result, errors = _preferences_ensure_disabled(
                repo_item=repo_item,
                pkg_id_sequence=pkg_id_sequence,
                default_set=False,
            )
            self._addon_restore.append((repo_item, pkg_id_sequence, result))
            del pkg_id_sequence

        # Lock repositories.
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
                    access_token=repo_item.access_token,
                    timeout=prefs.system.network_timeout,
                    use_cache=repo_item.use_cache,
                    use_idle=is_modal,
                )
            ],
            batch_job_limit=1,
        )

    def exec_command_finish(self, canceled):

        # Unlock repositories.
        lock_result_any_failed_with_report(self, self.repo_lock.release(), report_type='WARNING')
        del self.repo_lock

        # Refresh installed packages for repositories that were operated on.
        repo_cache_store = repo_cache_store_ensure()

        pkg_manifest_local = repo_cache_store.refresh_local_from_directory(
            directory=self.repo_directory,
            error_fn=self.error_fn_from_exception,
        )

        _extensions_repo_sync_wheels(repo_cache_store)
        repo_stats_calc()

        # TODO: it would be nice to include this message in the banner.
        def handle_error(ex):
            self.report({'ERROR'}, str(ex))

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

        _preferences_ui_redraw()
        _preferences_ui_refresh_addons()

        # NOTE: this can be removed once upgrading from 4.1 is no longer relevant.
        if self.do_legacy_replace and (not canceled):
            self._do_legacy_replace(self.pkg_id, pkg_manifest_local)

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
        ), str):
            self.report({'ERROR'}, iface_("The extension is incompatible with this system:\n{:s}").format(error))
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

        _repo_index, repo_name, pkg_id, item_remote = self._drop_variables

        layout.label(text="Do you want to install the following {:s}?".format(item_remote.type))

        col = layout.column(align=True)
        col.label(text="Name: {:s}".format(item_remote.name))
        col.label(text="Repository: {:s}".format(repo_name))
        col.label(text="Size: {:s}".format(size_as_fmt_string(item_remote.archive_size, precision=0)))
        del col

        layout.separator()

        layout.prop(self, "enable_on_install", text=rna_prop_enable_on_install_type_map[item_remote.type])

    @staticmethod
    def _do_legacy_replace(pkg_id, pkg_manifest_local):
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

        bpy.ops.preferences.addon_disable(module=addon_module_name)

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
            remote_url,  # `Optional[str]`
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
            _context,  # `bpy.types.Context`
            remote_url,   # `Optional[str]`
            repo_from_url_name,  # `str`
            url,  # `str`
    ):
        import string
        from .bl_extension_utils import (
            platform_from_this_system,
        )

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
                            "The extension dropped may be incompatible.",
                            "Check for an extension compatible with:",
                            "Blender v{:s} on \"{:s}\".".format(
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
            self._draw_override = (
                self._draw_override_errors,
                {
                    "errors": [
                        iface_("{:s} \"{:s}\" already installed!").format(
                            iface_(string.capwords(item_local.type)),
                            item_local.name,
                        )
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
        layout = _operator_draw_hide_buttons_hack(self.layout)
        icon = 'ERROR'
        for error in errors:
            if isinstance(error, str):
                layout.label(text=error, translate=False, icon=icon)
            else:
                error(layout)
            icon = 'BLANK1'
        return True

    # Pass 3: add-repository (terminating).
    def _draw_override_repo_add(
            self,
            *,
            remote_url,
    ):
        # Skip the URL prefix scheme, e.g. `https://` for less "noisy" outpout.
        url_split = remote_url.partition("://")
        url_for_display = url_split[2] if url_split[2] else remote_url

        layout = _operator_draw_hide_buttons_hack(self.layout)
        col = layout.column(align=True)
        col.label(text="The dropped extension comes from an unknown repository.")
        col.label(text="If you trust its source, add the repository and try again.")

        col.separator()
        if url_for_display:
            box = col.box()
            subcol = box.column(align=True)
            subcol.label(text=iface_("URL: {:s}").format(url_for_display), translate=False)
        else:
            col.label(text="Alternatively download the extension to Install from Disk.")

        layout.operator_context = 'INVOKE_DEFAULT'
        props = layout.operator("preferences.extension_repo_add", text="Add Repository...")
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

        _, errors = _preferences_ensure_disabled(
            repo_item=repo_item,
            pkg_id_sequence=(pkg_id,),
            default_set=True,
        )

        # Lock repositories.
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
                ),
            ],
            batch_job_limit=1,
        )

    def exec_command_finish(self, canceled):

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

        _extensions_repo_sync_wheels(repo_cache_store)
        repo_stats_calc()

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
        return EXTENSIONS_OT_package_uninstall.__doc__

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
        bpy.ops.preferences.addon_show(module="bl_ext.{:s}.{:s}".format(repo_item.module, self.pkg_id))
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


class EXTENSIONS_OT_repo_lock(Operator):
    """Lock repositories - to test locking"""
    bl_idname = "extensions.repo_lock"
    bl_label = "Lock Repository (Testing)"

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

        self.report({'INFO'}, "Locked {:d} repos(s)".format(len(lock_result)))
        EXTENSIONS_OT_repo_lock.lock = lock_handle
        return {'FINISHED'}


class EXTENSIONS_OT_repo_unlock(Operator):
    """Unlock repositories - to test unlocking"""
    bl_idname = "extensions.repo_unlock"
    bl_label = "Unlock Repository (Testing)"

    def execute(self, _context):
        lock_handle = EXTENSIONS_OT_repo_lock.lock
        if lock_handle is None:
            self.report({'ERROR'}, "Lock not held!")
            return {'CANCELLED'}

        lock_result = lock_handle.release()

        EXTENSIONS_OT_repo_lock.lock = None

        if lock_result_any_failed_with_report(self, lock_result):
            # This isn't canceled, but there were issues unlocking.
            return {'FINISHED'}

        self.report({'INFO'}, "Unlocked {:d} repos(s)".format(len(lock_result)))
        return {'FINISHED'}


# NOTE: this is a modified version of `PREFERENCES_OT_addon_show`.
# It would make most sense to extend this operator to support showing extensions to upgrade (eventually).
class EXTENSIONS_OT_userpref_show_for_update(Operator):
    """Open extensions preferences"""
    bl_idname = "extensions.userpref_show_for_update"
    bl_label = ""
    bl_options = {'INTERNAL'}

    def execute(self, context):
        wm = context.window_manager
        prefs = context.preferences

        prefs.active_section = 'EXTENSIONS'

        # Show only extensions that will be updated.
        wm.extension_show_panel_installed = True
        wm.extension_show_panel_available = False

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
                cls.poll_message_set("Blender was launched in offline-mode which cannot be changed at runtime")
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
                cls.poll_message_set("Blender was launched in offline-mode which cannot be changed at runtime")
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
                "Online access required to install or update.",
                "",
                "Launch Blender without --offline-mode"
            )
        else:
            lines = (
                "Please turn Online Access on the System settings.",
                "",
                "Internet access is required to install extensions from the internet."
            )
        for line in lines:
            col.label(text=line)


class EXTENSIONS_OT_package_enable_not_installed(Operator):
    """Turn on this extension"""
    bl_idname = "extensions.package_enable_not_installed"
    bl_label = "Enable Extension"

    @classmethod
    def poll(cls, _context):
        cls.poll_message_set("Extension needs to be installed before it can be enabled")
        return False

    def execute(self, _context):
        # This operator only exists to be able to show disabled check-boxes for extensions
        # while giving users a reasonable explanation on why is that.
        return {'CANCELLED'}


# -----------------------------------------------------------------------------
# Register
#
classes = (
    EXTENSIONS_OT_repo_sync,
    EXTENSIONS_OT_repo_sync_all,
    EXTENSIONS_OT_repo_refresh_all,
    EXTENSIONS_OT_repo_enable_from_drop,

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
    EXTENSIONS_OT_repo_lock,
    EXTENSIONS_OT_repo_unlock,

    EXTENSIONS_OT_userpref_show_for_update,
    EXTENSIONS_OT_userpref_show_online,
    EXTENSIONS_OT_userpref_allow_online,
    EXTENSIONS_OT_userpref_allow_online_popup,

    # Dummy, just shows a message.
    EXTENSIONS_OT_package_enable_not_installed,

    # Dummy commands (for testing).
    EXTENSIONS_OT_dummy_progress,
)


def register():
    for cls in classes:
        bpy.utils.register_class(cls)


def unregister():
    for cls in classes:
        bpy.utils.unregister_class(cls)


if __name__ == "__main__":
    register()
