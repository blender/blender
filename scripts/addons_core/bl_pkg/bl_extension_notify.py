# SPDX-FileCopyrightText: 2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
Startup notifications.
"""

__all__ = (
    "register",
    "unregister",
)


import os
import bpy
import sys

from . import bl_extension_ops
from . import bl_extension_utils

# Request the processes exit, then wait for them to exit.
# NOTE(@ideasman42): This is all well and good but any delays exiting are unwanted,
# only keep this as a reference and in case we can speed up forcing them to exit.
USE_GRACEFUL_EXIT = False

# Special value to signal no packages can be updated because all repositories are blocked by being offline.
STATE_DATA_ALL_OFFLINE = object()


# -----------------------------------------------------------------------------
# Internal Utilities

def sync_status_count_outdated_extensions(repos_notify):
    from . import repo_cache_store

    repos_notify_directories = [repo_item.directory for repo_item in repos_notify]

    package_count = 0

    for (
            pkg_manifest_remote,
            pkg_manifest_local,
    ) in zip(
        repo_cache_store.pkg_manifest_from_remote_ensure(
            error_fn=print,
            directory_subset=repos_notify_directories,
        ),
        repo_cache_store.pkg_manifest_from_local_ensure(
            error_fn=print,
            directory_subset=repos_notify_directories,
            # Needed as these have been updated.
            check_files=True,
        ),
    ):
        if pkg_manifest_remote is None:
            continue
        if pkg_manifest_local is None:
            continue

        for pkg_id, item_remote in pkg_manifest_remote.items():
            item_local = pkg_manifest_local.get(pkg_id)
            if item_local is None:
                # Not installed.
                continue

            if item_remote["version"] != item_local["version"]:
                package_count += 1
    return package_count


# -----------------------------------------------------------------------------
# Update Iterator
#
# This is a black-box which handled running the updates, yielding status text.

def sync_apply_locked(repos_notify, repos_notify_files, unique_ext):
    """
    Move files with a unique extension to their final location
    with a locked repository to ensure multiple Blender instances never overwrite
    repositories at the same time.

    Lock the repositories for the shortest time reasonably possible.
    If locking fails, this is OK as it's possible another Blender got here first.

    Another reason this is needed is exiting Blender will close the sync sub-processes,
    this is OK as long as the final location of the repositories JSON isn't being written
    to the moment Blender and it's sub-processes exit.
    """
    # TODO: handle the case of cruft being left behind, perhaps detect previous
    # files created with a `unique_ext` (`@{HEX}` extension) and removing them.
    # Although this shouldn't happen on a regular basis. Only when exiting immediately after launching
    # Blender and even then the user would need to be *lucky*.
    from . import cookie_from_session

    any_lock_errors = False
    repo_directories = [repo_item.directory for repo_item in repos_notify]
    with bl_extension_utils.RepoLockContext(
            repo_directories=repo_directories,
            cookie=cookie_from_session(),
    ) as lock_result:
        for directory, repo_files in zip(repo_directories, repos_notify_files):
            repo_files = [os.path.join(directory, filepath_rel) for filepath_rel in repo_files]

            # If locking failed, remove the temporary files that were written to.
            if (lock_result_for_repo := lock_result[directory]) is not None:
                sys.stderr.write("Warning \"{:s}\" locking \"{:s}\"\n".format(lock_result_for_repo, directory))
                any_lock_errors = True
                for filepath in repo_files:
                    # Don't check this exists as it always should, showing an error if it doesn't is fine.
                    try:
                        os.remove(filepath)
                    except Exception as ex:
                        sys.stderr.write("Failed to remove file: {:s}\n".format(str(ex)))
                continue

            # Locking worked, rename the files.
            for filepath in repo_files:
                filepath_dst = filepath[:-len(unique_ext)]
                if os.path.exists(filepath_dst):
                    try:
                        os.remove(filepath_dst)
                    except Exception as ex:
                        sys.stderr.write("Failed to remove file before renaming: {:s}\n".format(str(ex)))
                        continue
                # Not expected to fail, in the case it might (corrupt file-system for e.g.),
                # the script should continue executing.
                try:
                    os.rename(filepath, filepath_dst)
                except Exception as ex:
                    sys.stderr.write("Failed to rename file: {:s}\n".format(str(ex)))

    return any_lock_errors


def sync_status_generator(repos_notify):

    # Generator results...
    # -> None: do nothing.
    # -> (text, ICON_ID, NUMBER_OF_UPDATES)

    # ################ #
    # Setup The Update #
    # ################ #

    repos_notify_orig = repos_notify
    if not bpy.app.online_access:
        repos_notify = [repo for repo in repos_notify if repo.remote_url.startswith("file://")]
        if not repos_notify:
            # Special case, early exit.
            yield (STATE_DATA_ALL_OFFLINE, 0, ())
            return

    yield None

    any_offline = len(repos_notify) != len(repos_notify_orig)
    del repos_notify_orig

    # An extension unique to this session.
    unique_ext = "@{:x}".format(os.getpid())

    from functools import partial

    cmd_batch_partial = []
    for repo_item in repos_notify:
        # Local only repositories should still refresh, but not run the sync.
        assert repo_item.remote_url
        cmd_batch_partial.append(partial(
            bl_extension_utils.repo_sync,
            directory=repo_item.directory,
            remote_url=bl_extension_ops.url_params_append_defaults(repo_item.remote_url),
            online_user_agent=bl_extension_ops.online_user_agent_from_blender(),
            access_token=repo_item.access_token if repo_item.use_access_token else "",
            # Never sleep while there is no input, as this blocks Blender.
            use_idle=False,
            # Needed so the user can exit blender without warnings about a broken pipe.
            # TODO: write to a temporary location, once done:
            # There is no chance of corrupt data as the data isn't written directly to the target JSON.
            force_exit_ok=not USE_GRACEFUL_EXIT,
            extension_override=unique_ext,
        ))

    yield None

    # repos_lock = [repo_item.directory for repo_item in self.repos_notify]

    # Lock repositories.
    # self.repo_lock = bl_extension_utils.RepoLock(repo_directories=repos_lock, cookie=cookie_from_session())

    import atexit

    cmd_batch = None

    def cmd_force_quit():
        if cmd_batch is None:
            return
        cmd_batch.exec_non_blocking(request_exit=True)

        if USE_GRACEFUL_EXIT:
            import time
            # Force all commands to close.
            while not cmd_batch.exec_non_blocking(request_exit=True).all_complete:
                # Avoid high CPU usage on exit.
                time.sleep(0.01)

    atexit.register(cmd_force_quit)

    cmd_batch = bl_extension_utils.CommandBatch(
        # Used as a prefix in status.
        title="Update",
        batch=cmd_batch_partial,
    )
    del cmd_batch_partial

    yield None

    # ############## #
    # Run The Update #
    # ############## #

    # The count is unknown.
    update_total = -1
    any_lock_errors = False

    repos_notify_files = [[] for _ in repos_notify]

    is_debug = bpy.app.debug
    while True:
        command_result = cmd_batch.exec_non_blocking(
            # TODO: if Blender requested an exit... this should request exit here.
            request_exit=False,
        )
        # Forward new messages to reports.
        msg_list_per_command = cmd_batch.calc_status_log_since_last_request_or_none()
        if msg_list_per_command is not None:
            for i, msg_list in enumerate(msg_list_per_command):
                for (ty, msg) in msg_list:
                    if ty == 'PATH':
                        if not msg.endswith(unique_ext):
                            print("Unexpected path:", msg)
                        repos_notify_files[i].append(msg)
                        continue

                    # Always show warnings & errors in the output, otherwise there is no way
                    # to troubleshoot when checking for updates fails.
                    if not (is_debug or ty in {'WARN', 'ERROR'}):
                        continue

                    # TODO: output this information to a place for users, if they want to debug.
                    if len(msg_list_per_command) > 1:
                        # These reports are flattened, note the process number that fails so
                        # whoever is reading the reports can make sense of the messages.
                        msg = "{:s} (process {:d} of {:d})".format(msg, i + 1, len(msg_list_per_command))
                    if ty == 'STATUS':
                        print('INFO', msg)
                    else:
                        print(ty, msg)

        # TODO: more elegant way to detect changes.
        # Re-calculating the same information each time then checking if it's different isn't great.
        if command_result.status_data_changed:
            extra_warnings = []
            if command_result.all_complete:
                any_lock_errors = sync_apply_locked(repos_notify, repos_notify_files, unique_ext)
                update_total = sync_status_count_outdated_extensions(repos_notify)
                if any_lock_errors:
                    extra_warnings.append(" Failed to acquire lock!")
            if any_offline:
                extra_warnings.append(" Skipping online repositories!")
            yield (cmd_batch.calc_status_data(), update_total, extra_warnings)
        else:
            yield None

        if command_result.all_complete:
            break

    atexit.unregister(cmd_force_quit)

    # ################### #
    # Finalize The Update #
    # ################### #

    yield None

    # Unlock repositories.
    # lock_result_any_failed_with_report(self, self.repo_lock.release(), report_type='WARNING')
    # self.repo_lock = None


# -----------------------------------------------------------------------------
# Private API

# The timer before running the timer (initially).
TIME_WAIT_INIT = 0.05
# The time between calling the timer.
TIME_WAIT_STEP = 0.1

state_text = (
    "Checking for updates...",
)


class NotifyHandle:
    __slots__ = (
        "splash_region",
        "state",

        "sync_generator",
        "sync_info",
    )

    def __init__(self, repos_notify):
        self.splash_region = None
        self.state = 0
        # We could start the generator separately, this seems OK here for now.
        self.sync_generator = iter(sync_status_generator(repos_notify))
        # status_data, update_count, extra_warnings.
        self.sync_info = None


# When non-null, the timer is running.
_notify = None


def _region_exists(region):
    # TODO: this is a workaround for there being no good way to inspect temporary regions.
    # A better solution could be to store the `PyObject` in the `ARegion` so that it gets invalidated when freed.
    # This is a bigger change though - so use the context override as a way to check if a region is valid.
    exists = False
    try:
        with bpy.context.temp_override(region=region):
            exists = True
    except TypeError:
        pass
    return exists


def _ui_refresh_timer():
    if _notify is None:
        return None

    default_wait = TIME_WAIT_STEP

    sync_info = next(_notify.sync_generator, ...)
    # If the generator exited, early exit here.
    if sync_info is ...:
        return None
    if sync_info is None:
        # Nothing changed, no action is needed (waiting for a response).
        return default_wait

    # Re-display.
    assert isinstance(sync_info, tuple)
    assert len(sync_info) == 3

    _notify.sync_info = sync_info

    # Check if the splash_region is valid.
    if _notify.splash_region is not None:
        if not _region_exists(_notify.splash_region):
            _notify.splash_region = None
            return None
        _notify.splash_region.tag_redraw()
        _notify.splash_region.tag_refresh_ui()

    # TODO: redraw the status bar.

    return default_wait


def splash_draw_status_fn(self, context):
    if _notify.splash_region is None:
        _notify.splash_region = context.region_popup

    if _notify.sync_info is None:
        self.layout.label(text="Updates starting...")
    elif _notify.sync_info[0] is STATE_DATA_ALL_OFFLINE:
        # The special case is ugly but showing this operator doesn't fit well with other kinds of status updates.
        self.layout.operator("bl_pkg.extensions_show_online_prefs", text="Offline mode", icon='ORPHAN_DATA')
    else:
        status_data, update_count, extra_warnings = _notify.sync_info
        text, icon = bl_extension_utils.CommandBatch.calc_status_text_icon_from_data(status_data, update_count)
        # Not more than 1-2 of these (failed to lock, some repositories offline .. etc).
        for warning in extra_warnings:
            text = text + warning
        row = self.layout.row(align=True)
        if update_count > 0:
            row.operator("bl_pkg.extensions_show_for_update", text=text, icon=icon)
        else:
            row.label(text=text, icon=icon)

    self.layout.separator()
    self.layout.separator()


# -----------------------------------------------------------------------------
# Public API


def register(repos_notify):
    global _notify
    _notify = NotifyHandle(repos_notify)
    bpy.types.WM_MT_splash.append(splash_draw_status_fn)
    bpy.app.timers.register(_ui_refresh_timer, first_interval=TIME_WAIT_INIT)


def unregister():
    global _notify
    assert _notify is not None
    _notify = None

    bpy.types.WM_MT_splash.remove(splash_draw_status_fn)
    # This timer is responsible for un-registering itself.
    # `bpy.app.timers.unregister(_ui_refresh_timer)`
