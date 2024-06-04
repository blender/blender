# SPDX-FileCopyrightText: 2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

# Notifications used by:
# - The preferences when checking first displaying the extensions view.

__all__ = (
    "update_non_blocking",
    "update_in_progress",
    "update_ui_text",
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

# `wmWindowManager.extensions_updates` from C++
WM_EXTENSIONS_UPDATE_UNSET = -2
WM_EXTENSIONS_UPDATE_CHECKING = -1


# -----------------------------------------------------------------------------
# Internal Utilities

def sync_status_count_outdated_extensions(repos_notify):
    from . import repo_stats_calc_outdated_for_repo_directory

    package_count = 0

    for repo_item in repos_notify:
        package_count += repo_stats_calc_outdated_for_repo_directory(repo_item.directory)

    return package_count


# -----------------------------------------------------------------------------
# Update Iterator
#
# This is a black-box which handled running the updates, yielding status text.

def sync_calc_stale_repo_directories(repos_notify):
    # Check for the unlikely event that the state of repositories has changed since checking for updated began.
    # Do this by checking for directories since renaming or even disabling a repository need not prevent the
    # listing from being updated. Only detect changes to the (directory + URL) which define the source/destination.
    repo_state_from_prefs = set(
        (repo.directory, repo.remote_url)
        for repo in bpy.context.preferences.extensions.repos
    )
    repo_state_from_notify = set(
        (repo.directory, repo.remote_url)
        for repo in repos_notify
    )

    repo_directories_skip = set()
    for directory, _remote_url in (repo_state_from_notify - repo_state_from_prefs):
        repo_directories_skip.add(directory)

    return repo_directories_skip


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

    repo_directories_stale = sync_calc_stale_repo_directories(repos_notify)

    any_lock_errors = False
    any_stale_errors = False

    repo_directories = [repo_item.directory for repo_item in repos_notify]
    with bl_extension_utils.RepoLockContext(
            repo_directories=repo_directories,
            cookie=cookie_from_session(),
    ) as lock_result:
        for directory, repo_files in zip(repo_directories, repos_notify_files):
            repo_files = [os.path.join(directory, filepath_rel) for filepath_rel in repo_files]

            # If locking failed, remove the temporary files that were written to.
            has_error = False
            if directory in repo_directories_stale:
                # Unlikely but possible repositories change or are removed after check starts.
                sys.stderr.write("Warning \"{:s}\" has changed or been removed (skipping)\n".format(directory))
                any_stale_errors = True
                has_error = True
            elif (lock_result_for_repo := lock_result[directory]) is not None:
                sys.stderr.write("Warning \"{:s}\" locking \"{:s}\"\n".format(lock_result_for_repo, directory))
                any_lock_errors = True
                has_error = True

            if has_error:
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

    return any_lock_errors, any_stale_errors


def sync_status_generator(repos_fn):
    import atexit

    # Generator results...
    # -> None: do nothing.
    # -> (text, ICON_ID, NUMBER_OF_UPDATES)

    # ################ #
    # Setup The Update #
    # ################ #

    yield None

    # Calculate the repositories.
    # This may be an expensive so yield once before running.
    repos_and_do_online = list(repos_fn())
    assert isinstance(repos_and_do_online, list)

    if not bpy.app.online_access:
        # Allow file-system only sync.
        repos_and_do_online = [
            (repo, do_online_sync) for repo, do_online_sync in repos_and_do_online
            if repo.remote_url.startswith("file://")
        ]

    if not repos_and_do_online:
        return

    # An extension unique to this session.
    unique_ext = "@{:x}".format(os.getpid())

    from functools import partial

    cmd_batch_partial = []
    for repo_item, do_online_sync in repos_and_do_online:
        # Local only repositories should still refresh, but not run the sync.
        assert repo_item.remote_url
        cmd_batch_partial.append(partial(
            bl_extension_utils.repo_sync,
            directory=repo_item.directory,
            remote_name=repo_item.name,
            remote_url=bl_extension_ops.url_append_defaults(repo_item.remote_url),
            online_user_agent=bl_extension_ops.online_user_agent_from_blender(),
            access_token=repo_item.access_token,
            # Never sleep while there is no input, as this blocks Blender.
            use_idle=False,
            # Needed so the user can exit blender without warnings about a broken pipe.
            # TODO: write to a temporary location, once done:
            # There is no chance of corrupt data as the data isn't written directly to the target JSON.
            force_exit_ok=not USE_GRACEFUL_EXIT,
            dry_run=not do_online_sync,
            extension_override=unique_ext,
            # Demote connection errors to status which means they won't be shown in the console.
            # Do this as errors are expected when the computer is not connected to the internet.
            # Either run with debugging, or manually click update button in preferences.
            demote_connection_errors_to_status=not bpy.app.debug,
        ))

    yield None

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

    repos_notify_files = [[] for _ in repos_and_do_online]

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

                # ################### #
                # Finalize The Update #
                # ################### #
                repos_notify = [repo for repo, _do_online_sync in repos_and_do_online]
                any_lock_errors, any_stale_errors = sync_apply_locked(repos_notify, repos_notify_files, unique_ext)
                update_total = sync_status_count_outdated_extensions(repos_notify)
                if any_lock_errors:
                    extra_warnings.append(" Failed to acquire lock!")
                if any_stale_errors:
                    extra_warnings.append(" Unexpected change in repository!")
            yield (cmd_batch.calc_status_data(), update_total, extra_warnings)
        else:
            yield None

        if command_result.all_complete:
            break

    atexit.unregister(cmd_force_quit)

    yield None


# -----------------------------------------------------------------------------
# Private API

# The timer before running the timer (initially).
TIME_WAIT_INIT = 0.05
# The time between calling the timer.
TIME_WAIT_STEP = 0.1


class NotifyHandle:
    __slots__ = (
        "sync_info",

        "_repos_fn",
        "is_complete",
        "_sync_generator",
    )

    def __init__(self, repos_fn):
        self._repos_fn = repos_fn
        self._sync_generator = None
        self.is_complete = False
        # status_data, update_count, extra_warnings.
        self.sync_info = None

    def run(self):
        assert self._sync_generator is None
        self._sync_generator = iter(sync_status_generator(self._repos_fn))

    def run_ensure(self):
        if self.is_running():
            return
        self.run()

    def run_step(self):
        assert self._sync_generator is not None
        sync_info = next(self._sync_generator, ...)
        if sync_info is ...:
            self.is_complete = True
        if isinstance(sync_info, tuple):
            self.sync_info = sync_info
        return sync_info

    def is_running(self):
        return self._sync_generator is not None

    def updates_count(self):
        if self.sync_info is None:
            return WM_EXTENSIONS_UPDATE_CHECKING
        _status_data, update_count, _extra_warnings = self.sync_info
        return update_count

    def ui_text(self):
        if self.sync_info is None:
            return "Checking for Extension Updates", 'NONE', WM_EXTENSIONS_UPDATE_CHECKING
        status_data, update_count, extra_warnings = self.sync_info
        text, icon = bl_extension_utils.CommandBatch.calc_status_text_icon_from_data(
            status_data, update_count,
        )
        # Not more than 1-2 of these (failed to lock, some repositories offline .. etc).
        for warning in extra_warnings:
            text = text + warning
        return text, icon, update_count


# A list of `NotifyHandle`, only the first item is allowed to be running.
_notify_queue = []


def _ui_refresh_apply(*, notify):
    # Ensure the preferences are redrawn when the update is complete.
    if bpy.context.preferences.active_section == 'EXTENSIONS':
        for wm in bpy.data.window_managers:
            for win in wm.windows:
                for area in win.screen.areas:
                    if area.type != 'PREFERENCES':
                        continue
                    for region in area.regions:
                        if region.type != 'WINDOW':
                            continue
                        region.tag_redraw()


def _ui_refresh_timer():
    if not _notify_queue:
        if wm.extensions_updates == WM_EXTENSIONS_UPDATE_CHECKING:
            wm.extensions_updates = WM_EXTENSIONS_UPDATE_UNSET
        return None

    wm = bpy.context.window_manager
    notify = _notify_queue[0]
    notify.run_ensure()

    default_wait = TIME_WAIT_STEP

    if notify.is_complete:
        sync_info = ...
    else:
        sync_info = notify.run_step()
        if sync_info is None:
            # Nothing changed, no action is needed (waiting for a response).
            return default_wait

        # Some content was found, set checking.
        # Avoid doing this early because the icon flickers in cases when
        # it's not needed and it gets turned off quickly.
        if wm.extensions_updates == WM_EXTENSIONS_UPDATE_UNSET:
            wm.extensions_updates = WM_EXTENSIONS_UPDATE_CHECKING

    # If the generator exited, either step to the next action or early exit here.
    if sync_info is ...:
        _ui_refresh_apply(notify=notify)
        if len(_notify_queue) <= 1:
            # Keep `_notify_queuy[0]` because we may want to keep accessing the text even when updates are complete.
            if wm.extensions_updates == WM_EXTENSIONS_UPDATE_CHECKING:
                wm.extensions_updates = WM_EXTENSIONS_UPDATE_UNSET
            return None
        # Move onto the next item.
        del _notify_queue[0]
        return default_wait

    # TODO: redraw the status bar.
    _ui_refresh_apply(notify=notify)

    update_count = notify.updates_count()
    if update_count != wm.extensions_updates:
        wm.extensions_updates = update_count

    return default_wait


# -----------------------------------------------------------------------------
# Public API


def update_non_blocking(*, repos_fn):
    # Perform a non-blocking update on ``repos``.
    # Updates are queued in case some are already running.
    # `repos_fn` A generator or function that returns a list of ``(RepoItem, do_online_sync)`` pairs.
    # Some repositories don't check for update on startup for e.g.

    _notify_queue.append(NotifyHandle(repos_fn))
    if not bpy.app.timers.is_registered(_ui_refresh_timer):
        bpy.app.timers.register(_ui_refresh_timer, first_interval=TIME_WAIT_INIT, persistent=True)
    return True


def update_in_progress():
    if _notify_queue:
        if not _notify_queue[0].is_complete:
            return True
    return None


def update_ui_text():
    if _notify_queue:
        text, icon, _update_count = _notify_queue[0].ui_text()
    else:
        text = ""
        icon = 'NONE'
    return text, icon
