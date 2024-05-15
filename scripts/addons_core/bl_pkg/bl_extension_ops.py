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
)

# Localize imports.
from . import (
    bl_extension_utils,
)  # noqa: E402

from . import (
    repo_status_text,
    cookie_from_session,
)

from .bl_extension_utils import (
    RepoLock,
    RepoLockContext,
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


def rna_prop_repo_enum_local_only_itemf(_self, context):
    if context is None:
        result = []
    else:
        result = [
            (
                repo_item.module,
                repo_item.name if repo_item.enabled else (repo_item.name + " (disabled)"),
                "",
            )
            for repo_item in repo_iter_valid_local_only(context)
        ]
    # Prevent the strings from being freed.
    rna_prop_repo_enum_local_only_itemf._result = result
    return result


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
# Internal Utilities
#

def extension_url_find_repo_index_and_pkg_id(url):
    from .bl_extension_utils import (
        pkg_manifest_archive_url_abs_from_remote_url,
    )
    from .bl_extension_ops import (
        extension_repos_read,
    )
    # return repo_index, pkg_id
    from . import repo_cache_store

    # NOTE: we might want to use `urllib.parse.urlsplit` so it's possible to include variables in the URL.
    url_basename = url.rpartition("/")[2]

    repos_all = extension_repos_read()

    for repo_index, (
            pkg_manifest_remote,
            pkg_manifest_local,
    ) in enumerate(zip(
        repo_cache_store.pkg_manifest_from_remote_ensure(error_fn=print),
        repo_cache_store.pkg_manifest_from_local_ensure(error_fn=print),
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
            archive_url = item_remote["archive_url"]
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


def pkg_info_check_exclude_filter_ex(name, tagline, search_lower):
    return (
        (search_lower in name.lower() or search_lower in iface_(name).lower()) or
        (search_lower in tagline.lower() or search_lower in iface_(tagline).lower())
    )


def pkg_info_check_exclude_filter(item, search_lower):
    return pkg_info_check_exclude_filter_ex(item["name"], item["tagline"], search_lower)


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


def repo_iter_valid_local_only(context):
    from . import repo_paths_or_none
    extension_repos = context.preferences.extensions.repos
    for repo_item in extension_repos:
        if not repo_item.enabled:
            continue
        # Ignore repositories that have invalid settings.
        directory, remote_url = repo_paths_or_none(repo_item)
        if directory is None:
            continue
        if remote_url:
            continue
        yield repo_item


class RepoItem(NamedTuple):
    name: str
    directory: str
    remote_url: str
    module: str
    use_cache: bool


def repo_cache_store_refresh_from_prefs(include_disabled=False):
    from . import repo_cache_store
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
    prefix_addon_modules_base = tuple([module + "." for module in prefix_addon_modules])

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

        if item_local["type"] == "add-on":
            # Check if the add-on will have been enabled from re-installing.
            if pkg_id in pkg_id_sequence_upgrade:
                continue

            addon_module_name = "bl_ext.{:s}.{:s}".format(repo_item.module, pkg_id)
            addon_utils.enable(addon_module_name, default_set=True, handle_error=handle_error)
        elif item_local["type"] == "theme":
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


def _preferences_ensure_sync():
    # TODO: define when/where exactly sync should be ensured.
    # This is a general issue:
    from . import repo_cache_store
    sync_required = False
    for repo_index, (
            pkg_manifest_remote,
            pkg_manifest_local,
    ) in enumerate(zip(
        repo_cache_store.pkg_manifest_from_remote_ensure(error_fn=print),
        repo_cache_store.pkg_manifest_from_local_ensure(error_fn=print),
    )):
        if pkg_manifest_remote is None:
            sync_required = True
            break
        if pkg_manifest_local is None:
            sync_required = True
            break

    if sync_required:
        for wm in bpy.data.window_managers:
            for win in wm.windows:
                win.cursor_set('WAIT')
        try:
            bpy.ops.bl_pkg.repo_sync_all()
        except BaseException as ex:
            print("Sync failed:", ex)

        for wm in bpy.data.window_managers:
            for win in wm.windows:
                win.cursor_set('DEFAULT')


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
                remote_url=remote_url,
                module=repo_item.module,
                use_cache=repo_item.use_cache,
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
            remote_url=remote_url,
            module=repo_item.module,
            use_cache=repo_item.use_cache,
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


def _pkg_marked_by_repo(pkg_manifest_all):
    # NOTE: pkg_manifest_all can be from local or remote source.
    wm = bpy.context.window_manager
    search_lower = wm.extension_search.lower()
    filter_by_type = blender_filter_by_type_map[wm.extension_type]

    repo_pkg_map = {}
    for pkg_id, repo_index in blender_extension_mark:
        # While this should be prevented, any marked packages out of the range will cause problems, skip them.
        if repo_index >= len(pkg_manifest_all):
            continue

        pkg_manifest = pkg_manifest_all[repo_index]
        item = pkg_manifest.get(pkg_id)
        if item is None:
            continue
        if filter_by_type and (filter_by_type != item["type"]):
            continue
        if search_lower and not pkg_info_check_exclude_filter(item, search_lower):
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
    for repo_index, pkg_manifest_local in enumerate(repo_cache_store.pkg_manifest_from_local_ensure(error_fn=print)):
        repo = repos_all[repo_index]
        repo_module = repo.module
        repo_directory = repo.directory
        for pkg_id, item_local in pkg_manifest_local.items():
            pkg_dirpath = os.path.join(repo_directory, pkg_id)
            wheels_rel = item_local.get("wheels", None)
            if wheels_rel is None:
                continue
            if not isinstance(wheels_rel, list):
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
        handle.modal_timer = context.window_manager.event_timer_add(0.01, window=context.window)
        handle.wm = context.window_manager

        handle.wm.modal_handler_add(op)
        op._runtime_handle = handle
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
            del op._runtime_handle
            context.workspace.status_text_set(None)
            repo_status_text.running = False
            return {'FINISHED'}

        return {'RUNNING_MODAL'}

    def op_modal_impl(self, op, context, event):
        refresh = False
        if event.type == 'TIMER':
            refresh = True
        elif event.type == 'ESC':
            if not self.request_exit:
                print("Request exit!")
                self.request_exit = True
                refresh = True

        if refresh:
            return self.op_modal_step(op, context)
        return {'RUNNING_MODAL'}


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


# -----------------------------------------------------------------------------
# Public Repository Actions
#

class _BlPkgCmdMixIn:
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
            if getattr(cls, attr) is getattr(_BlPkgCmdMixIn, attr):
                raise Exception("Subclass did not define 'exec_command_iter'!")

    def exec_command_iter(self, is_modal):
        raise Exception("Subclass must define!")

    def exec_command_finish(self):
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
        if 'FINISHED' in result:
            self.exec_command_finish()
        return result

    def modal(self, context, event):
        result = self._runtime_handle.op_modal_impl(self, context, event)
        if 'FINISHED' in result:
            self.exec_command_finish()
        return result


class BlPkgDummyProgress(Operator, _BlPkgCmdMixIn):
    bl_idname = "bl_pkg.dummy_progress"
    bl_label = "Ext Demo"
    __slots__ = _BlPkgCmdMixIn.cls_slots

    def exec_command_iter(self, is_modal):
        return bl_extension_utils.CommandBatch(
            title="Dummy Progress",
            batch=[
                partial(
                    bl_extension_utils.dummy_progress,
                    use_idle=is_modal,
                ),
            ],
        )

    def exec_command_finish(self):
        _preferences_ui_redraw()


class BlPkgRepoSync(Operator, _BlPkgCmdMixIn):
    bl_idname = "bl_pkg.repo_sync"
    bl_label = "Ext Repo Sync"
    __slots__ = _BlPkgCmdMixIn.cls_slots

    repo_directory: rna_prop_directory
    repo_index: rna_prop_repo_index

    def exec_command_iter(self, is_modal):
        directory = _repo_dir_and_index_get(self.repo_index, self.repo_directory, self.report)
        if not directory:
            return None

        if (repo_item := _extensions_repo_from_directory_and_report(directory, self.report)) is None:
            return None

        if not os.path.exists(directory):
            try:
                os.makedirs(directory)
            except BaseException as ex:
                self.report({'ERROR'}, str(ex))
                return {'CANCELLED'}

        # Needed to refresh.
        self.repo_directory = directory

        # Lock repositories.
        self.repo_lock = RepoLock(repo_directories=[directory], cookie=cookie_from_session())
        if lock_result_any_failed_with_report(self, self.repo_lock.acquire()):
            return None

        cmd_batch = []
        if repo_item.remote_url:
            cmd_batch.append(
                partial(
                    bl_extension_utils.repo_sync,
                    directory=directory,
                    remote_url=repo_item.remote_url,
                    online_user_agent=online_user_agent_from_blender(),
                    use_idle=is_modal,
                )
            )

        return bl_extension_utils.CommandBatch(
            title="Sync",
            batch=cmd_batch,
        )

    def exec_command_finish(self):
        from . import repo_cache_store

        repo_cache_store_refresh_from_prefs()
        repo_cache_store.refresh_remote_from_directory(
            directory=self.repo_directory,
            error_fn=self.error_fn_from_exception,
            force=True,
        )

        # Unlock repositories.
        lock_result_any_failed_with_report(self, self.repo_lock.release(), report_type='WARNING')
        del self.repo_lock

        _preferences_ui_redraw()


class BlPkgRepoSyncAll(Operator, _BlPkgCmdMixIn):
    bl_idname = "bl_pkg.repo_sync_all"
    bl_label = "Ext Repo Sync All"
    __slots__ = _BlPkgCmdMixIn.cls_slots

    use_active_only: BoolProperty(
        name="Active Only",
        description="Only sync the active repository",
    )

    def exec_command_iter(self, is_modal):
        use_active_only = self.use_active_only
        repos_all = extension_repos_read(use_active_only=use_active_only)

        if not repos_all:
            self.report({'INFO'}, "No repositories to sync")
            return None

        for repo_item in repos_all:
            if not os.path.exists(repo_item.directory):
                try:
                    os.makedirs(repo_item.directory)
                except BaseException as ex:
                    self.report({'WARNING'}, str(ex))
                    return None

        cmd_batch = []
        for repo_item in repos_all:
            # Local only repositories should still refresh, but not run the sync.
            if repo_item.remote_url:
                cmd_batch.append(partial(
                    bl_extension_utils.repo_sync,
                    directory=repo_item.directory,
                    remote_url=repo_item.remote_url,
                    online_user_agent=online_user_agent_from_blender(),
                    use_idle=is_modal,
                ))

        repos_lock = [repo_item.directory for repo_item in repos_all]

        # Lock repositories.
        self.repo_lock = RepoLock(repo_directories=repos_lock, cookie=cookie_from_session())
        if lock_result_any_failed_with_report(self, self.repo_lock.acquire()):
            return None

        return bl_extension_utils.CommandBatch(
            title="Sync \"{:s}\"".format(repos_all[0].name) if use_active_only else "Sync All",
            batch=cmd_batch,
        )

    def exec_command_finish(self):
        from . import repo_cache_store

        repo_cache_store_refresh_from_prefs()

        for repo_item in extension_repos_read():
            repo_cache_store.refresh_remote_from_directory(
                directory=repo_item.directory,
                error_fn=self.error_fn_from_exception,
                force=True,
            )

        # Unlock repositories.
        lock_result_any_failed_with_report(self, self.repo_lock.release(), report_type='WARNING')
        del self.repo_lock

        _preferences_ui_redraw()


class BlPkgPkgUpgradeAll(Operator, _BlPkgCmdMixIn):
    bl_idname = "bl_pkg.pkg_upgrade_all"
    bl_label = "Ext Package Upgrade All"
    __slots__ = _BlPkgCmdMixIn.cls_slots + (
        "_repo_directories",
    )

    use_active_only: BoolProperty(
        name="Active Only",
        description="Only sync the active repository",
    )

    def exec_command_iter(self, is_modal):
        from . import repo_cache_store
        self._repo_directories = set()
        self._addon_restore = []
        self._theme_restore = _preferences_theme_state_create()

        use_active_only = self.use_active_only
        repos_all = extension_repos_read(use_active_only=use_active_only)
        repo_directory_supset = [repo_entry.directory for repo_entry in repos_all] if use_active_only else None

        if not repos_all:
            self.report({'INFO'}, "No repositories to upgrade")
            return None

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

                if item_remote["version"] != item_local["version"]:
                    packages_to_upgrade[repo_index].append(pkg_id)
                    package_count += 1

            if packages_to_upgrade[repo_index]:
                handle_addons_info.append((repos_all[repo_index], list(packages_to_upgrade[repo_index])))

        cmd_batch = []
        for repo_index, pkg_id_sequence in enumerate(packages_to_upgrade):
            if not pkg_id_sequence:
                continue
            repo_item = repos_all[repo_index]
            cmd_batch.append(partial(
                bl_extension_utils.pkg_install,
                directory=repo_item.directory,
                remote_url=repo_item.remote_url,
                pkg_id_sequence=pkg_id_sequence,
                online_user_agent=online_user_agent_from_blender(),
                use_cache=repo_item.use_cache,
                use_idle=is_modal,
            ))
            self._repo_directories.add(repo_item.directory)

        if not cmd_batch:
            self.report({'INFO'}, "No installed packages to update")
            return None

        # Lock repositories.
        self.repo_lock = RepoLock(repo_directories=list(self._repo_directories), cookie=cookie_from_session())
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
        )

    def exec_command_finish(self):

        # Unlock repositories.
        lock_result_any_failed_with_report(self, self.repo_lock.release(), report_type='WARNING')
        del self.repo_lock

        # Refresh installed packages for repositories that were operated on.
        from . import repo_cache_store
        for directory in self._repo_directories:
            repo_cache_store.refresh_local_from_directory(
                directory=directory,
                error_fn=self.error_fn_from_exception,
            )

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


class BlPkgPkgInstallMarked(Operator, _BlPkgCmdMixIn):
    bl_idname = "bl_pkg.pkg_install_marked"
    bl_label = "Ext Package Install_marked"
    __slots__ = _BlPkgCmdMixIn.cls_slots + (
        "_repo_directories",
        "_repo_map_packages_addon_only",
    )

    enable_on_install: rna_prop_enable_on_install

    def exec_command_iter(self, is_modal):
        from . import repo_cache_store
        repos_all = extension_repos_read()
        pkg_manifest_remote_all = list(repo_cache_store.pkg_manifest_from_remote_ensure(
            error_fn=self.error_fn_from_exception,
        ))
        repo_pkg_map = _pkg_marked_by_repo(pkg_manifest_remote_all)
        self._repo_directories = set()
        self._repo_map_packages_addon_only = []
        package_count = 0

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

            cmd_batch.append(partial(
                bl_extension_utils.pkg_install,
                directory=repo_item.directory,
                remote_url=repo_item.remote_url,
                pkg_id_sequence=pkg_id_sequence,
                online_user_agent=online_user_agent_from_blender(),
                use_cache=repo_item.use_cache,
                use_idle=is_modal,
            ))
            self._repo_directories.add(repo_item.directory)
            package_count += len(pkg_id_sequence)

            # Filter out non add-on extensions.
            pkg_manifest_remote = pkg_manifest_remote_all[repo_index]

            pkg_id_sequence_addon_only = [
                pkg_id for pkg_id in pkg_id_sequence if pkg_manifest_remote[pkg_id]["type"] == "add-on"]
            if pkg_id_sequence_addon_only:
                self._repo_map_packages_addon_only.append((repo_item.directory, pkg_id_sequence_addon_only))

        if not cmd_batch:
            self.report({'ERROR'}, "No un-installed packages marked")
            return None

        # Lock repositories.
        self.repo_lock = RepoLock(repo_directories=list(self._repo_directories), cookie=cookie_from_session())
        if lock_result_any_failed_with_report(self, self.repo_lock.acquire()):
            return None

        return bl_extension_utils.CommandBatch(
            title="Install {:d} Marked Package(s)".format(package_count),
            batch=cmd_batch,
        )

    def exec_command_finish(self):

        # Unlock repositories.
        lock_result_any_failed_with_report(self, self.repo_lock.release(), report_type='WARNING')
        del self.repo_lock

        # Refresh installed packages for repositories that were operated on.
        from . import repo_cache_store
        for directory in self._repo_directories:
            repo_cache_store.refresh_local_from_directory(
                directory=directory,
                error_fn=self.error_fn_from_exception,
            )

        _extensions_repo_sync_wheels(repo_cache_store)

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


class BlPkgPkgUninstallMarked(Operator, _BlPkgCmdMixIn):
    bl_idname = "bl_pkg.pkg_uninstall_marked"
    bl_label = "Ext Package Uninstall_marked"
    __slots__ = _BlPkgCmdMixIn.cls_slots + (
        "_repo_directories",
    )

    def exec_command_iter(self, is_modal):
        from . import repo_cache_store
        # TODO: check if the packages are already installed (notify the user).
        # Perhaps re-install?
        repos_all = extension_repos_read()
        pkg_manifest_local_all = list(repo_cache_store.pkg_manifest_from_local_ensure(
            error_fn=self.error_fn_from_exception,
        ))
        repo_pkg_map = _pkg_marked_by_repo(pkg_manifest_local_all)
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
        self.repo_lock = RepoLock(repo_directories=list(self._repo_directories), cookie=cookie_from_session())
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
        )

    def exec_command_finish(self):

        # Unlock repositories.
        lock_result_any_failed_with_report(self, self.repo_lock.release(), report_type='WARNING')
        del self.repo_lock

        # Refresh installed packages for repositories that were operated on.
        from . import repo_cache_store
        for directory in self._repo_directories:
            repo_cache_store.refresh_local_from_directory(
                directory=directory,
                error_fn=self.error_fn_from_exception,
            )

        _extensions_repo_sync_wheels(repo_cache_store)

        _preferences_theme_state_restore(self._theme_restore)

        _preferences_ui_redraw()
        _preferences_ui_refresh_addons()


class BlPkgPkgInstallFiles(Operator, _BlPkgCmdMixIn):
    """Install an extension from a file into a locally managed repository"""
    bl_idname = "bl_pkg.pkg_install_files"
    bl_label = "Install from Disk"
    __slots__ = _BlPkgCmdMixIn.cls_slots + (
        "repo_directory",
        "pkg_id_sequence"
    )
    _drop_variables = None

    filter_glob: StringProperty(default="*.zip", options={'HIDDEN'})

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
        name="Local Repository",
        items=rna_prop_repo_enum_local_only_itemf,
        description="The local repository to install extensions into",
    )

    enable_on_install: rna_prop_enable_on_install

    # Only used for code-path for dropping an extension.
    url: rna_prop_url

    def exec_command_iter(self, is_modal):
        from .bl_extension_utils import (
            pkg_manifest_dict_from_file_or_error,
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
        for source_filepath in source_files:
            result = pkg_manifest_dict_from_file_or_error(source_filepath)
            if isinstance(result, str):
                continue
            pkg_id = result["id"]
            if pkg_id in pkg_id_sequence:
                continue
            pkg_id_sequence.append(pkg_id)

        directory = repo_item.directory
        assert directory != ""

        # Collect package ID's.
        self.repo_directory = directory
        self.pkg_id_sequence = pkg_id_sequence

        # Detect upgrade.
        if pkg_id_sequence:
            from . import repo_cache_store
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
        self.repo_lock = RepoLock(repo_directories=[repo_item.directory], cookie=cookie_from_session())
        if lock_result_any_failed_with_report(self, self.repo_lock.acquire()):
            return None

        return bl_extension_utils.CommandBatch(
            title="Install Package Files",
            batch=[
                partial(
                    bl_extension_utils.pkg_install_files,
                    directory=directory,
                    files=source_files,
                    use_idle=is_modal,
                )
            ],
        )

    def exec_command_finish(self):

        # Refresh installed packages for repositories that were operated on.
        from . import repo_cache_store

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

    @classmethod
    def poll(cls, context):
        if next(repo_iter_valid_local_only(context), None) is None:
            cls.poll_message_set("There must be at least one \"Local\" repository set to install extensions into")
            return False
        return True

    def invoke(self, context, event):
        if self.properties.is_property_set("url"):
            return self._invoke_for_drop(context, event)

        # Ensure the value is marked as set (else an error is reported).
        self.repo = self.repo

        context.window_manager.fileselect_add(self)
        return {'RUNNING_MODAL'}

    def draw(self, context):
        if self._drop_variables is not None:
            return self._draw_for_drop(context)

        # Override draw because the repository names may be over-long and not fit well in the UI.
        # Show the text & repository names in two separate rows.
        layout = self.layout
        col = layout.column()
        col.label(text="Local Repository:")
        col.prop(self, "repo", text="")

        layout.prop(self, "enable_on_install")

    def _invoke_for_drop(self, context, event):
        self._drop_variables = True
        # Drop logic.
        url = self.url
        print("DROP FILE:", url)

        from .bl_extension_ops import repo_iter_valid_local_only
        from .bl_extension_utils import pkg_manifest_dict_from_file_or_error

        if not list(repo_iter_valid_local_only(bpy.context)):
            self.report({'ERROR'}, "No Local Repositories")
            return {'CANCELLED'}

        if isinstance(result := pkg_manifest_dict_from_file_or_error(url), str):
            self.report({'ERROR'}, "Error in manifest {:s}".format(result))
            return {'CANCELLED'}

        pkg_id = result["id"]
        pkg_type = result["type"]
        del result

        self._drop_variables = pkg_id, pkg_type

        # Set to it's self to the property is considered "set".
        self.repo = self.repo
        self.filepath = url

        wm = context.window_manager
        wm.invoke_props_dialog(self)

        return {'RUNNING_MODAL'}

    def _draw_for_drop(self, context):

        layout = self.layout
        layout.operator_context = 'EXEC_DEFAULT'

        pkg_id, pkg_type = self._drop_variables

        layout.label(text="Local Repository")
        layout.prop(self, "repo", text="")

        layout.prop(self, "enable_on_install", text=rna_prop_enable_on_install_type_map[pkg_type])


class BlPkgPkgInstall(Operator, _BlPkgCmdMixIn):
    bl_idname = "bl_pkg.pkg_install"
    bl_label = "Install Extension"
    __slots__ = _BlPkgCmdMixIn.cls_slots

    _drop_variables = None

    repo_directory: rna_prop_directory
    repo_index: rna_prop_repo_index

    pkg_id: rna_prop_pkg_id

    enable_on_install: rna_prop_enable_on_install

    # Only used for code-path for dropping an extension.
    url: rna_prop_url

    def exec_command_iter(self, is_modal):
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

        # Detect upgrade.
        from . import repo_cache_store
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
        self.repo_lock = RepoLock(repo_directories=[repo_item.directory], cookie=cookie_from_session())
        if lock_result_any_failed_with_report(self, self.repo_lock.acquire()):
            return None

        return bl_extension_utils.CommandBatch(
            title="Install Package",
            batch=[
                partial(
                    bl_extension_utils.pkg_install,
                    directory=directory,
                    remote_url=repo_item.remote_url,
                    pkg_id_sequence=(pkg_id,),
                    online_user_agent=online_user_agent_from_blender(),
                    use_cache=repo_item.use_cache,
                    use_idle=is_modal,
                )
            ],
        )

    def exec_command_finish(self):

        # Unlock repositories.
        lock_result_any_failed_with_report(self, self.repo_lock.release(), report_type='WARNING')
        del self.repo_lock

        # Refresh installed packages for repositories that were operated on.
        from . import repo_cache_store
        pkg_manifest_local = repo_cache_store.refresh_local_from_directory(
            directory=self.repo_directory,
            error_fn=self.error_fn_from_exception,
        )

        _extensions_repo_sync_wheels(repo_cache_store)

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

    def invoke(self, context, event):
        # Only for drop logic!
        if self.properties.is_property_set("url"):
            return self._invoke_for_drop(context, event)

        return self.execute(context)

    def _invoke_for_drop(self, context, event):
        url = self.url
        print("DROP URL:", url)

        _preferences_ensure_sync()

        repo_index, repo_name, pkg_id, item_remote, item_local = extension_url_find_repo_index_and_pkg_id(url)

        if repo_index == -1:
            self.report({'ERROR'}, "Extension: URL not found in remote repositories!\n{:s}".format(url))
            return {'CANCELLED'}

        if item_local is not None:
            self.report({'ERROR'}, "Extension: \"{:s}\" Already installed!".format(pkg_id))
            return {'CANCELLED'}

        self._drop_variables = repo_index, repo_name, pkg_id, item_remote

        self.repo_index = repo_index
        self.pkg_id = pkg_id

        wm = context.window_manager
        wm.invoke_props_dialog(self)
        return {'RUNNING_MODAL'}

    def draw(self, context):
        if self._drop_variables is not None:
            return self._draw_for_drop(context)

    def _draw_for_drop(self, context):
        from .bl_extension_ui import (
            size_as_fmt_string,
        )
        layout = self.layout

        repo_index, repo_name, pkg_id, item_remote = self._drop_variables

        layout.label(text="Do you want to install the following {:s}?".format(item_remote["type"]))

        col = layout.column(align=True)
        col.label(text="Name: {:s}".format(item_remote["name"]))
        col.label(text="Repository: {:s}".format(repo_name))
        col.label(text="Size: {:s}".format(size_as_fmt_string(item_remote["archive_size"], precision=0)))
        del col

        layout.separator()

        layout.prop(self, "enable_on_install", text=rna_prop_enable_on_install_type_map[item_remote["type"]])


class BlPkgPkgUninstall(Operator, _BlPkgCmdMixIn):
    bl_idname = "bl_pkg.pkg_uninstall"
    bl_label = "Ext Package Uninstall"
    __slots__ = _BlPkgCmdMixIn.cls_slots

    repo_directory: rna_prop_directory
    repo_index: rna_prop_repo_index

    pkg_id: rna_prop_pkg_id

    def exec_command_iter(self, is_modal):

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
        self.repo_lock = RepoLock(repo_directories=[repo_item.directory], cookie=cookie_from_session())
        if lock_result_any_failed_with_report(self, self.repo_lock.acquire()):
            return None

        return bl_extension_utils.CommandBatch(
            title="Uninstall Package",
            batch=[
                partial(
                    bl_extension_utils.pkg_uninstall,
                    directory=directory,
                    pkg_id_sequence=(pkg_id, ),
                    use_idle=is_modal,
                ),
            ],
        )

    def exec_command_finish(self):

        # Refresh installed packages for repositories that were operated on.
        from . import repo_cache_store

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

        _preferences_theme_state_restore(self._theme_restore)

        _preferences_ui_redraw()
        _preferences_ui_refresh_addons()


class BlPkgPkgDisable_TODO(Operator):
    """Turn off this extension"""
    bl_idname = "bl_pkg.extension_disable"
    bl_label = "Disable extension"

    def execute(self, _context):
        self.report({'WARNING'}, "Disabling themes is not yet supported")
        return {'CANCELLED'}


class BlPkgPkgThemeEnable(Operator):
    """Turn off this theme"""
    bl_idname = "bl_pkg.extension_theme_enable"
    bl_label = "Enable theme extension"

    pkg_id: rna_prop_pkg_id
    repo_index: rna_prop_repo_index

    def execute(self, context):
        self.repo_index
        repo_item = extension_repos_read_index(self.repo_index)
        extension_theme_enable(repo_item.directory, self.pkg_id)
        print(repo_item.directory, self.pkg_id)
        return {'FINISHED'}


class BlPkgPkgThemeDisable(Operator):
    """Turn off this theme"""
    bl_idname = "bl_pkg.extension_theme_disable"
    bl_label = "Disable theme extension"

    pkg_id: rna_prop_pkg_id
    repo_index: rna_prop_repo_index

    def execute(self, context):
        import os
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


class BlPkgDisplayErrorsClear(Operator):
    bl_idname = "bl_pkg.pkg_display_errors_clear"
    bl_label = "Clear Status"

    def execute(self, _context):
        from .bl_extension_ui import display_errors
        display_errors.clear()
        _preferences_ui_redraw()
        return {'FINISHED'}


class BlPkgStatusClear(Operator):
    bl_idname = "bl_pkg.pkg_status_clear"
    bl_label = "Clear Status"

    def execute(self, _context):
        repo_status_text.running = False
        repo_status_text.log.clear()
        _preferences_ui_redraw()
        return {'FINISHED'}


class BlPkgPkgMarkSet(Operator):
    bl_idname = "bl_pkg.pkg_mark_set"
    bl_label = "Mark Package"

    pkg_id: rna_prop_pkg_id
    repo_index: rna_prop_repo_index

    def execute(self, _context):
        key = (self.pkg_id, self.repo_index)
        blender_extension_mark.add(key)
        _preferences_ui_redraw()
        return {'FINISHED'}


class BlPkgPkgMarkClear(Operator):
    bl_idname = "bl_pkg.pkg_mark_clear"
    bl_label = "Mark Package"

    pkg_id: rna_prop_pkg_id
    repo_index: rna_prop_repo_index

    def execute(self, _context):
        key = (self.pkg_id, self.repo_index)
        blender_extension_mark.discard(key)
        _preferences_ui_redraw()
        return {'FINISHED'}


class BlPkgPkgShowSet(Operator):
    bl_idname = "bl_pkg.pkg_show_set"
    bl_label = "Show Package Set"

    pkg_id: rna_prop_pkg_id
    repo_index: rna_prop_repo_index

    def execute(self, _context):
        key = (self.pkg_id, self.repo_index)
        blender_extension_show.add(key)
        _preferences_ui_redraw()
        return {'FINISHED'}


class BlPkgPkgShowClear(Operator):
    bl_idname = "bl_pkg.pkg_show_clear"
    bl_label = "Show Package Clear"

    pkg_id: rna_prop_pkg_id
    repo_index: rna_prop_repo_index

    def execute(self, _context):
        key = (self.pkg_id, self.repo_index)
        blender_extension_show.discard(key)
        _preferences_ui_redraw()
        return {'FINISHED'}


class BlPkgPkgShowSettings(Operator):
    bl_idname = "bl_pkg.pkg_show_settings"
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


class BlPkgObsoleteMarked(Operator):
    """Zeroes package versions, useful for development - to test upgrading"""
    bl_idname = "bl_pkg.obsolete_marked"
    bl_label = "Obsolete Marked"

    def execute(self, _context):
        from . import (
            repo_cache_store,
        )

        repos_all = extension_repos_read()
        pkg_manifest_local_all = list(repo_cache_store.pkg_manifest_from_local_ensure(error_fn=print))
        repo_pkg_map = _pkg_marked_by_repo(pkg_manifest_local_all)
        found = False

        repos_lock = [repos_all[repo_index].directory for repo_index in sorted(repo_pkg_map.keys())]

        with RepoLockContext(repo_directories=repos_lock, cookie=cookie_from_session()) as lock_result:
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
            _preferences_ui_redraw()

        return {'FINISHED'}


class BlPkgRepoLock(Operator):
    """Lock repositories - to test locking"""
    bl_idname = "bl_pkg.repo_lock"
    bl_label = "Lock Repository (Testing)"

    lock = None

    def execute(self, _context):
        repos_all = extension_repos_read()
        repos_lock = [repo_item.directory for repo_item in repos_all]

        lock_handle = RepoLock(repo_directories=repos_lock, cookie=cookie_from_session())
        lock_result = lock_handle.acquire()
        if lock_result_any_failed_with_report(self, lock_result):
            # At least one lock failed, unlock all and return.
            lock_handle.release()
            return {'CANCELLED'}

        self.report({'INFO'}, "Locked {:d} repos(s)".format(len(lock_result)))
        BlPkgRepoLock.lock = lock_handle
        return {'FINISHED'}


class BlPkgRepoUnlock(Operator):
    """Unlock repositories - to test unlocking"""
    bl_idname = "bl_pkg.repo_unlock"
    bl_label = "Unlock Repository (Testing)"

    def execute(self, _context):
        lock_handle = BlPkgRepoLock.lock
        if lock_handle is None:
            self.report({'ERROR'}, "Lock not held!")
            return {'CANCELLED'}

        lock_result = lock_handle.release()

        BlPkgRepoLock.lock = None

        if lock_result_any_failed_with_report(self, lock_result):
            # This isn't canceled, but there were issues unlocking.
            return {'FINISHED'}

        self.report({'INFO'}, "Unlocked {:d} repos(s)".format(len(lock_result)))
        return {'FINISHED'}


# NOTE: this is a modified version of `PREFERENCES_OT_addon_show`.
# It would make most sense to extend this operator to support showing extensions to upgrade (eventually).
class BlPkgShowUpgrade(Operator):
    """Show add-on preferences"""
    bl_idname = "bl_pkg.extensions_show_for_update"
    bl_label = ""
    bl_options = {'INTERNAL'}

    def execute(self, context):
        wm = context.window_manager
        prefs = context.preferences

        prefs.active_section = 'EXTENSIONS'
        prefs.view.show_addons_enabled_only = False

        # Show only extensions that will be updated.
        wm.extension_installed_only = False
        wm.extension_updates_only = True

        bpy.ops.screen.userpref_show('INVOKE_DEFAULT')

        return {'FINISHED'}


class BlPkgOnlineAccess(Operator):
    """Handle online access"""
    bl_idname = "bl_pkg.extension_online_access"
    bl_label = ""
    bl_options = {'INTERNAL'}

    enable: BoolProperty(
        name="Enable",
        default=False,
    )

    def execute(self, context):
        prefs = context.preferences

        remote_url = "https://extensions.blender.org/api/v1/extensions"

        if self.enable:
            extension_repos = prefs.extensions.repos
            repo_found = None
            for repo in extension_repos:
                if repo.remote_url == remote_url:
                    repo_found = repo
                    break
            if repo_found:
                repo_found.enabled = True
            else:
                # While not expected, we want to know if this ever occurs, don't fail silently.
                self.report({'WARNING'}, "Repository \"{:s}\" not found!".format(remote_url))

            # Run the first check for updates automatically.
            if bpy.ops.bl_pkg.repo_sync_all.poll():
                bpy.ops.bl_pkg.repo_sync_all()
        prefs.extensions.use_online_access_handled = True

        return {'FINISHED'}


class BlPkgEnableNotInstalled(Operator):
    """Turn on this extension"""
    bl_idname = "bl_pkg.extensions_enable_not_installed"
    bl_label = "Enable Extension"

    @classmethod
    def poll(cls, context):
        cls.poll_message_set("Extension needs to be installed before it can be enabled")
        return False

    def execute(self, context):
        # This operator only exists to be able to show disabled check-boxes for extensions
        # while giving users a reasonable explanation on why is that.
        return {'CANCELLED'}


# -----------------------------------------------------------------------------
# Register
#
classes = (
    BlPkgRepoSync,
    BlPkgRepoSyncAll,

    BlPkgPkgInstallFiles,
    BlPkgPkgInstall,
    BlPkgPkgUninstall,
    BlPkgPkgDisable_TODO,

    BlPkgPkgThemeEnable,
    BlPkgPkgThemeDisable,

    BlPkgPkgUpgradeAll,
    BlPkgPkgInstallMarked,
    BlPkgPkgUninstallMarked,

    # UI only operator (to select a package).
    BlPkgDisplayErrorsClear,
    BlPkgStatusClear,
    BlPkgPkgShowSet,
    BlPkgPkgShowClear,
    BlPkgPkgMarkSet,
    BlPkgPkgMarkClear,
    BlPkgPkgShowSettings,

    BlPkgObsoleteMarked,
    BlPkgRepoLock,
    BlPkgRepoUnlock,

    BlPkgShowUpgrade,
    BlPkgOnlineAccess,

    # Dummy, just shows a message.
    BlPkgEnableNotInstalled,

    # Dummy commands (for testing).
    BlPkgDummyProgress,
)


def register():
    for cls in classes:
        bpy.utils.register_class(cls)


def unregister():
    for cls in classes:
        bpy.utils.unregister_class(cls)


if __name__ == "__main__":
    register()
