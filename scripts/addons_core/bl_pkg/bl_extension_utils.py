# SPDX-FileCopyrightText: 2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
Non-blocking access to package management.

- No ``bpy`` module use.
"""

__all__ = (
    # Public Repository Actions.
    "repo_sync",
    "repo_upgrade",
    "repo_listing",

    # Public Package Actions.
    "pkg_install_files",
    "pkg_install",
    "pkg_uninstall",

    "pkg_make_obsolete_for_testing",

    "dummy_progress",

    # Public Stand-Alone Utilities.
    "pkg_theme_file_list",
    "file_mtime_or_none",

    # Public API.
    "json_from_filepath",
    "toml_from_filepath",
    "json_to_filepath",

    "pkg_manifest_dict_is_valid_or_error",
    "pkg_manifest_dict_from_file_or_error",
    "pkg_manifest_archive_url_abs_from_remote_url",

    "CommandBatch",
    "RepoCacheStore",

    # Directory Lock.
    "RepoLock",
    "RepoLockContext",
)

import json
import os
import sys
import signal
import stat
import subprocess
import time
import tomllib


from typing import (
    Any,
    Callable,
    Generator,
    IO,
    List,
    Optional,
    Dict,
    NamedTuple,
    Sequence,
    Set,
    Tuple,
    Union,
)

BASE_DIR = os.path.abspath(os.path.dirname(__file__))

BLENDER_EXT_CMD = (
    # When run from within Blender, it will point to Blender's local Python binary.
    sys.executable,
    os.path.normpath(os.path.join(BASE_DIR, "cli", "blender_ext.py")),
)

# This directory is in the local repository.
REPO_LOCAL_PRIVATE_DIR = ".blender_ext"
# Locate inside `REPO_LOCAL_PRIVATE_DIR`.
REPO_LOCAL_PRIVATE_LOCK = "index.lock"

PKG_REPO_LIST_FILENAME = "index.json"
PKG_MANIFEST_FILENAME_TOML = "blender_manifest.toml"
PKG_EXT = ".zip"

# Add this to the local JSON file.
REPO_LOCAL_JSON = os.path.join(REPO_LOCAL_PRIVATE_DIR, PKG_REPO_LIST_FILENAME)

# An item we communicate back to Blender.
InfoItem = Tuple[str, Any]
InfoItemSeq = Sequence[InfoItem]

COMPLETE_ITEM = ('DONE', "")

# Time to wait when there is no output, avoid 0 as it causes high CPU usage.
IDLE_WAIT_ON_READ = 0.05
# IDLE_WAIT_ON_READ = 0.2


# -----------------------------------------------------------------------------
# Internal Functions.
#

if sys.platform == "win32":
    # See: https://stackoverflow.com/a/35052424/432509
    def file_handle_make_non_blocking(file_handle: IO[bytes]) -> None:
        # Constant could define globally but avoid polluting the name-space
        # thanks to: https://stackoverflow.com/questions/34504970
        import msvcrt
        from ctypes import (
            POINTER,
            WinError,
            byref,
            windll,
            wintypes,
        )
        from ctypes.wintypes import (
            BOOL,
            DWORD,
            HANDLE,
        )

        LPDWORD = POINTER(DWORD)

        PIPE_NOWAIT = wintypes.DWORD(0x00000001)

        # Set non-blocking.
        SetNamedPipeHandleState = windll.kernel32.SetNamedPipeHandleState
        SetNamedPipeHandleState.argtypes = [HANDLE, LPDWORD, LPDWORD, LPDWORD]
        SetNamedPipeHandleState.restype = BOOL
        os_handle = msvcrt.get_osfhandle(file_handle.fileno())
        res = windll.kernel32.SetNamedPipeHandleState(os_handle, byref(PIPE_NOWAIT), None, None)
        if res == 0:
            print(WinError())

    def file_handle_non_blocking_is_error_blocking(ex: BaseException) -> bool:
        if not isinstance(ex, OSError):
            return False
        from ctypes import GetLastError
        ERROR_NO_DATA = 232
        # This is sometimes zero, `ex.args == (22, "Invalid argument")`
        # This could be checked but for now ignore all zero errors.
        return (GetLastError() in {0, ERROR_NO_DATA})

else:
    def file_handle_make_non_blocking(file_handle: IO[bytes]) -> None:
        import fcntl
        # Get current `file_handle` flags.
        flags = fcntl.fcntl(file_handle.fileno(), fcntl.F_GETFL)
        fcntl.fcntl(file_handle, fcntl.F_SETFL, flags | os.O_NONBLOCK)

    def file_handle_non_blocking_is_error_blocking(ex: BaseException) -> bool:
        if not isinstance(ex, BlockingIOError):
            return False
        return True


def file_mtime_or_none(filepath: str) -> Optional[int]:
    try:
        # For some reason `mypy` thinks this is a float.
        return int(os.stat(filepath)[stat.ST_MTIME])
    except FileNotFoundError:
        return None


def scandir_with_demoted_errors(path: str) -> Generator[os.DirEntry[str], None, None]:
    try:
        for entry in os.scandir(path):
            yield entry
    except BaseException as ex:
        print("Error: scandir", ex)


# -----------------------------------------------------------------------------
# Call JSON.
#

def non_blocking_call(cmd: Sequence[str]) -> subprocess.Popen[bytes]:
    # pylint: disable-next=consider-using-with
    ps = subprocess.Popen(cmd, stdout=subprocess.PIPE)
    stdout = ps.stdout
    assert stdout is not None
    # Needed so whatever is available can be read (without waiting).
    file_handle_make_non_blocking(stdout)
    return ps


def command_output_from_json_0(
        args: Sequence[str],
        use_idle: bool,
) -> Generator[InfoItemSeq, bool, None]:
    cmd = [*BLENDER_EXT_CMD, *args, "--output-type=JSON_0"]
    ps = non_blocking_call(cmd)
    stdout = ps.stdout
    assert stdout is not None
    chunk_list = []
    request_exit_signal_sent = False

    while True:
        # It's possible this is multiple chunks.
        try:
            chunk = stdout.read()
        except BaseException as ex:
            if not file_handle_non_blocking_is_error_blocking(ex):
                raise ex
            chunk = b''

        json_messages = []

        if not chunk:
            if ps.poll() is not None:
                break
            if use_idle:
                time.sleep(IDLE_WAIT_ON_READ)
        elif (chunk_zero_index := chunk.find(b'\0')) == -1:
            chunk_list.append(chunk)
        else:
            # Extract contiguous data from `chunk_list`.
            chunk_list.append(chunk[:chunk_zero_index])

            json_bytes_list = [b''.join(chunk_list)]
            chunk_list.clear()

            # There may be data afterwards, even whole chunks.
            if chunk_zero_index + 1 != len(chunk):
                chunk = chunk[chunk_zero_index + 1:]
                # Add whole chunks.
                while (chunk_zero_index := chunk.find(b'\0')) != -1:
                    json_bytes_list.append(chunk[:chunk_zero_index])
                    chunk = chunk[chunk_zero_index + 1:]
                if chunk:
                    chunk_list.append(chunk)

            request_exit = False

            for json_bytes in json_bytes_list:
                json_data = json.loads(json_bytes.decode("utf-8"))

                assert len(json_data) == 2
                assert isinstance(json_data[0], str)

                json_messages.append((json_data[0], json_data[1]))

        # Yield even when `json_messages`, otherwise this generator can block.
        # It also means a request to exit might not be responded to soon enough.
        request_exit = yield json_messages
        if request_exit and not request_exit_signal_sent:
            ps.send_signal(signal.SIGINT)
            request_exit_signal_sent = True


# -----------------------------------------------------------------------------
# Internal Functions.
#


def repositories_validate_or_errors(repos: Sequence[str]) -> Optional[InfoItemSeq]:
    return None


# -----------------------------------------------------------------------------
# Public Stand-Alone Utilities
#

def pkg_theme_file_list(directory: str, pkg_idname: str) -> Tuple[str, List[str]]:
    theme_dir = os.path.join(directory, pkg_idname)
    theme_files = [
        filename for entry in os.scandir(theme_dir)
        if ((not entry.is_dir()) and
            (not (filename := entry.name).startswith(".")) and
            filename.lower().endswith(".xml"))
    ]
    theme_files.sort()
    return theme_dir, theme_files


# -----------------------------------------------------------------------------
# Public Repository Actions
#

def repo_sync(
        *,
        directory: str,
        remote_url: str,
        online_user_agent: str,
        use_idle: bool,
        force_exit_ok: bool = False,
        extension_override: str = "",
) -> Generator[InfoItemSeq, None, None]:
    """
    Implementation:
    ``bpy.ops.ext.repo_sync(directory)``.
    """
    yield from command_output_from_json_0([
        "sync",
        "--local-dir", directory,
        "--remote-url", remote_url,
        "--online-user-agent", online_user_agent,
        *(("--force-exit-ok",) if force_exit_ok else ()),
        *(("--extension-override", extension_override) if extension_override else ()),
    ], use_idle=use_idle)
    yield [COMPLETE_ITEM]


def repo_upgrade(
        *,
        directory: str,
        remote_url: str,
        online_user_agent: str,
        use_idle: bool,
) -> Generator[InfoItemSeq, None, None]:
    """
    Implementation:
    ``bpy.ops.ext.repo_upgrade(directory)``.
    """
    yield from command_output_from_json_0([
        "upgrade",
        "--local-dir", directory,
        "--remote-url", remote_url,
        "--online-user-agent", online_user_agent,
    ], use_idle=use_idle)
    yield [COMPLETE_ITEM]


def repo_listing(
        *,
        repos: Sequence[str],
) -> Generator[InfoItemSeq, None, None]:
    """
    Implementation:
    ``bpy.ops.ext.repo_listing(directory)``.
    """
    if result := repositories_validate_or_errors(repos):
        yield result
        return

    yield [COMPLETE_ITEM]


# -----------------------------------------------------------------------------
# Public Package Actions
#

def pkg_install_files(
        *,
        directory: str,
        files: Sequence[str],
        use_idle: bool,
) -> Generator[InfoItemSeq, None, None]:
    """
    Implementation:
    ``bpy.ops.ext.pkg_install_files(directory, files)``.
    """
    yield from command_output_from_json_0([
        "install-files", *files,
        "--local-dir", directory,
    ], use_idle=use_idle)
    yield [COMPLETE_ITEM]


def pkg_install(
        *,
        directory: str,
        remote_url: str,
        pkg_id_sequence: Sequence[str],
        online_user_agent: str,
        use_cache: bool,
        use_idle: bool,
) -> Generator[InfoItemSeq, None, None]:
    """
    Implementation:
    ``bpy.ops.ext.pkg_install(directory, pkg_id)``.
    """
    yield from command_output_from_json_0([
        "install", ",".join(pkg_id_sequence),
        "--local-dir", directory,
        "--remote-url", remote_url,
        "--online-user-agent", online_user_agent,
        "--local-cache", str(int(use_cache)),
    ], use_idle=use_idle)
    yield [COMPLETE_ITEM]


def pkg_uninstall(
        *,
        directory: str,
        pkg_id_sequence: Sequence[str],
        use_idle: bool,
) -> Generator[InfoItemSeq, None, None]:
    """
    Implementation:
    ``bpy.ops.ext.pkg_uninstall(directory, pkg_id)``.
    """
    yield from command_output_from_json_0([
        "uninstall", ",".join(pkg_id_sequence),
        "--local-dir", directory,
    ], use_idle=use_idle)
    yield [COMPLETE_ITEM]


# -----------------------------------------------------------------------------
# Public Demo Actions
#

def dummy_progress(
        *,
        use_idle: bool,
) -> Generator[InfoItemSeq, bool, None]:
    """
    Implementation:
    ``bpy.ops.ext.dummy_progress()``.
    """
    yield from command_output_from_json_0(["dummy-progress", "--time-duration=1.0"], use_idle=use_idle)
    yield [COMPLETE_ITEM]


# -----------------------------------------------------------------------------
# Public (non-command-line-wrapping) functions
#

def json_from_filepath(filepath_json: str) -> Optional[Dict[str, Any]]:
    if os.path.exists(filepath_json):
        with open(filepath_json, "r", encoding="utf-8") as fh:
            result = json.loads(fh.read())
            assert isinstance(result, dict)
            return result
    return None


def toml_from_filepath(filepath_json: str) -> Optional[Dict[str, Any]]:
    if os.path.exists(filepath_json):
        with open(filepath_json, "r", encoding="utf-8") as fh:
            return tomllib.loads(fh.read())
    return None


def json_to_filepath(filepath_json: str, data: Any) -> None:
    with open(filepath_json, "w", encoding="utf-8") as fh:
        fh.write(json.dumps(data))


def pkg_make_obsolete_for_testing(local_dir: str, pkg_id: str) -> None:
    import re
    filepath = os.path.join(local_dir, pkg_id, PKG_MANIFEST_FILENAME_TOML)
    # Weak! use basic matching to replace the version, not nice but OK as a debugging option.
    with open(filepath, "r", encoding="utf-8") as fh:
        data = fh.read()

    def key_replace(match: re.Match[str]) -> str:
        return "version = \"0.0.0\""

    data = re.sub(r"^\s*version\s*=\s*\"[^\"]+\"", key_replace, data, flags=re.MULTILINE)
    with open(filepath, "w", encoding="utf-8") as fh:
        fh.write(data)


def pkg_manifest_dict_is_valid_or_error(
        data: Dict[str, Any],
        from_repo: bool,
        strict: bool,
) -> Optional[str]:
    # Exception! In in general `cli` shouldn't be considered a Python module,
    # it's validation function is handy to reuse.
    from .cli.blender_ext import pkg_manifest_from_dict_and_validate
    assert "id" in data
    result = pkg_manifest_from_dict_and_validate(data, from_repo=from_repo, strict=strict)
    if isinstance(result, str):
        return result
    return None


def pkg_manifest_dict_from_file_or_error(
        filepath: str,
) -> Union[Dict[str, Any], str]:
    from .cli.blender_ext import pkg_manifest_from_archive_and_validate
    result = pkg_manifest_from_archive_and_validate(filepath, strict=False)
    if isinstance(result, str):
        return result
    # Else convert the named-tuple into a dictionary.
    result_dict = result._asdict()
    assert isinstance(result_dict, dict)
    return result_dict


def pkg_manifest_archive_url_abs_from_remote_url(remote_url: str, archive_url: str) -> str:
    from .cli.blender_ext import remote_url_has_filename_suffix
    if archive_url.startswith("./"):
        if remote_url_has_filename_suffix(remote_url):
            # The URL contains the JSON name, strip this off before adding the package name.
            archive_url = remote_url[:-len(PKG_REPO_LIST_FILENAME)] + archive_url[2:]
        else:
            assert remote_url.startswith(("http://", "https://", "file://"))
            # Simply add to the URL.
            archive_url = remote_url.rstrip("/") + archive_url[1:]
    return archive_url


def pkg_repo_cache_clear(local_dir: str) -> None:
    local_cache_dir = os.path.join(local_dir, ".blender_ext", "cache")
    if not os.path.isdir(local_cache_dir):
        return

    for entry in scandir_with_demoted_errors(local_cache_dir):
        if entry.is_dir(follow_symlinks=False):
            continue
        if not entry.name.endswith(PKG_EXT):
            continue

        # Should never fail unless the file-system has permissions issues or corruption.
        try:
            os.unlink(entry.path)
        except BaseException as ex:
            print("Error: unlink", ex)


# -----------------------------------------------------------------------------
# Public Command Pool (non-command-line wrapper)
#

InfoItemCallable = Callable[[], Generator[InfoItemSeq, bool, None]]


class CommandBatchItem:
    __slots__ = (
        "fn_with_args",
        "fn_iter",
        "status",
        "has_error",
        "has_warning",
        "msg_log",
        "msg_log_len_last",

        "msg_type",
        "msg_info",
    )

    STATUS_NOT_YET_STARTED = 0
    STATUS_RUNNING = 1
    STATUS_COMPLETE = 2

    def __init__(self, fn_with_args: InfoItemCallable):
        self.fn_with_args = fn_with_args
        self.fn_iter: Optional[Generator[InfoItemSeq, bool, None]] = None
        self.status = CommandBatchItem.STATUS_NOT_YET_STARTED
        self.has_error = False
        self.has_warning = False
        self.msg_log: List[Tuple[str, Any]] = []
        self.msg_log_len_last = 0
        self.msg_type = ""
        self.msg_info = ""

    def invoke(self) -> Generator[InfoItemSeq, bool, None]:
        return self.fn_with_args()


class CommandBatch_ExecNonBlockingResult(NamedTuple):
    # A message list for each command, aligned to `CommandBatchItem._batch`.
    messages: Tuple[List[Tuple[str, str]], ...]
    # When true, the status of all commands is `CommandBatchItem.STATUS_COMPLETE`.
    all_complete: bool
    # When true, `calc_status_data` will return a different result.
    status_data_changed: bool


class CommandBatch_StatusFlag(NamedTuple):
    flag: int
    failure_count: int
    # This error seems to be a bug in `mypy-v1.10.0`.
    count: int  # type: ignore


class CommandBatch:
    __slots__ = (
        "title",

        "_batch",
        "_request_exit",
        "_log_added_since_accessed",
    )

    def __init__(
            self,
            *,
            title: str,
            batch: Sequence[InfoItemCallable],
    ):
        self.title = title
        self._batch = [CommandBatchItem(fn_with_args) for fn_with_args in batch]
        self._request_exit = False
        self._log_added_since_accessed = True

    def _exec_blocking_single(
            self,
            report_fn: Callable[[str, str], None],
            request_exit_fn: Callable[[], bool],
    ) -> bool:
        for cmd in self._batch:
            assert cmd.fn_iter is None
            cmd.fn_iter = cmd.invoke()
            request_exit: Optional[bool] = None
            while True:
                try:
                    # Request `request_exit` starts of as None, then it's a boolean.
                    json_messages = cmd.fn_iter.send(request_exit)  # type: ignore
                except StopIteration:
                    break

                for ty, msg in json_messages:
                    report_fn(ty, msg)

                if request_exit is None:
                    request_exit = False

            if request_exit is True:
                break
        if request_exit is None:
            return True
        return request_exit

    def _exec_blocking_multi(
            self,
            *,
            report_fn: Callable[[str, str], None],
            request_exit_fn: Callable[[], bool],
    ) -> bool:
        # TODO, concurrent execution.
        return self._exec_blocking_single(report_fn, request_exit_fn)

    def exec_blocking(
            self,
            report_fn: Callable[[str, str], None],
            request_exit_fn: Callable[[], bool],
            concurrent: bool,
    ) -> bool:
        # Blocking execution & finish.
        if concurrent:
            return self._exec_blocking_multi(
                report_fn=report_fn,
                request_exit_fn=request_exit_fn,
            )
        return self._exec_blocking_single(report_fn, request_exit_fn)

    def exec_non_blocking(
            self,
            *,
            request_exit: bool,
    ) -> CommandBatch_ExecNonBlockingResult:
        """
        Return the result of running multiple commands.
        """
        command_output: Tuple[List[Tuple[str, str]], ...] = tuple([] for _ in range(len(self._batch)))

        if request_exit:
            self._request_exit = True

        status_data_changed = False

        complete_count = 0
        for cmd_index in reversed(range(len(self._batch))):
            cmd = self._batch[cmd_index]
            if cmd.status == CommandBatchItem.STATUS_COMPLETE:
                complete_count += 1
                continue

            send_arg: Optional[bool] = self._request_exit

            # First time initialization.
            if cmd.fn_iter is None:
                cmd.fn_iter = cmd.invoke()
                cmd.status = CommandBatchItem.STATUS_RUNNING
                status_data_changed = True
                send_arg = None

            try:
                json_messages = cmd.fn_iter.send(send_arg)  # type: ignore
            except StopIteration:
                # FIXME: This should not happen, we should get a "DONE" instead.
                cmd.status = CommandBatchItem.STATUS_COMPLETE
                complete_count += 1
                status_data_changed = True
                continue

            if json_messages:
                for ty, msg in json_messages:
                    self._log_added_since_accessed = True

                    cmd.msg_type = ty
                    cmd.msg_info = msg
                    if ty == 'DONE':
                        assert msg == ""
                        cmd.status = CommandBatchItem.STATUS_COMPLETE
                        complete_count += 1
                        status_data_changed = True
                        break

                    command_output[cmd_index].append((ty, msg))
                    if ty != 'PROGRESS':
                        if ty == 'ERROR':
                            if not cmd.has_error:
                                cmd.has_error = True
                                status_data_changed = True
                        elif ty == 'WARNING':
                            if not cmd.has_warning:
                                cmd.has_warning = True
                                status_data_changed = True
                        cmd.msg_log.append((ty, msg))

        # Check if all are complete.
        assert complete_count == len([cmd for cmd in self._batch if cmd.status == CommandBatchItem.STATUS_COMPLETE])
        all_complete = (complete_count == len(self._batch))
        return CommandBatch_ExecNonBlockingResult(
            messages=command_output,
            all_complete=all_complete,
            status_data_changed=status_data_changed,
        )

    def calc_status_string(self) -> List[str]:
        return [
            "{:s}: {:s}".format(cmd.msg_type, cmd.msg_info)
            for cmd in self._batch if (cmd.msg_type or cmd.msg_info)
        ]

    def calc_status_data(self) -> CommandBatch_StatusFlag:
        """
        A single string for all commands
        """
        status_flag = 0
        failure_count = 0
        for cmd in self._batch:
            status_flag |= 1 << cmd.status
            if cmd.has_error or cmd.has_warning:
                failure_count += 1
        return CommandBatch_StatusFlag(
            flag=status_flag,
            failure_count=failure_count,
            count=len(self._batch),
        )

    @staticmethod
    def calc_status_text_icon_from_data(status_data: CommandBatch_StatusFlag, update_count: int) -> Tuple[str, str]:
        # Generate a nice UI string for a status-bar & splash screen (must be short).
        #
        # NOTE: this is (arguably) UI logic, it's just nice to have it here
        # as it avoids using low-level flags externally.
        #
        # FIXME: this text assumed a "sync" operation.
        if status_data.failure_count == 0:
            fail_text = ""
        elif status_data.failure_count == status_data.count:
            fail_text = ", failed"
        else:
            fail_text = ", some actions failed"

        if status_data.flag == 1 << CommandBatchItem.STATUS_NOT_YET_STARTED:
            return "Starting Extension Updates{:s}".format(fail_text), 'SORTTIME'
        if status_data.flag == 1 << CommandBatchItem.STATUS_COMPLETE:
            if update_count > 0:
                # NOTE: the UI design in #120612 has the number of extensions available in icon.
                # Include in the text as this is not yet supported.
                return "Extensions Updates Available ({:d}){:s}".format(update_count, fail_text), 'INTERNET'
            return "All Extensions Up-to-date{:s}".format(fail_text), 'CHECKMARK'
        if status_data.flag & 1 << CommandBatchItem.STATUS_RUNNING:
            return "Checking for Extension Updates{:s}".format(fail_text), 'SORTTIME'

        # Should never reach this line!
        return "Internal error, unknown state!{:s}".format(fail_text), 'ERROR'

    def calc_status_log_or_none(self) -> Optional[List[Tuple[str, str]]]:
        """
        Return the log or None if there were no changes since the last call.
        """
        if self._log_added_since_accessed is False:
            return None
        self._log_added_since_accessed = False

        return [
            (ty, msg)
            for cmd in self._batch
            for ty, msg in (cmd.msg_log + ([(cmd.msg_type, cmd.msg_info)] if cmd.msg_type == 'PROGRESS' else []))
        ]

    def calc_status_log_since_last_request_or_none(self) -> Optional[List[List[Tuple[str, str]]]]:
        """
        Return a list of new errors per command or None when none are found.
        """
        result: List[List[Tuple[str, str]]] = [[] for _ in range(len(self._batch))]
        found = False
        for cmd_index, cmd in enumerate(self._batch):
            msg_log_len = len(cmd.msg_log)
            if cmd.msg_log_len_last == msg_log_len:
                continue
            assert cmd.msg_log_len_last < msg_log_len
            result[cmd_index] = cmd.msg_log[cmd.msg_log_len_last:]
            cmd.msg_log_len_last = len(cmd.msg_log)
            found = True

        return result if found else None


# -----------------------------------------------------------------------------
# Public Repo Cache (non-command-line wrapper)
#

class _RepoCacheEntry:
    __slots__ = (
        "directory",
        "remote_url",

        "_pkg_manifest_local",
        "_pkg_manifest_remote",
        "_pkg_manifest_remote_mtime",
        "_pkg_manifest_remote_has_warning"
    )

    def __init__(self, directory: str, remote_url: str) -> None:
        assert directory != ""
        self.directory = directory
        self.remote_url = remote_url
        # Manifest data per package loaded from the packages local JSON.
        self._pkg_manifest_local: Optional[Dict[str, Dict[str, Any]]] = None
        self._pkg_manifest_remote: Optional[Dict[str, Dict[str, Any]]] = None
        self._pkg_manifest_remote_mtime = 0
        # Avoid many noisy prints.
        self._pkg_manifest_remote_has_warning = False

    def _json_data_ensure(
            self,
            *,
            error_fn: Callable[[BaseException], None],
            check_files: bool = False,
            ignore_missing: bool = False,
    ) -> Any:
        if self._pkg_manifest_remote is not None:
            if check_files:
                self._json_data_refresh(error_fn=error_fn)
            return self._pkg_manifest_remote

        filepath_json = os.path.join(self.directory, REPO_LOCAL_JSON)

        try:
            self._pkg_manifest_remote = json_from_filepath(filepath_json)
        except BaseException as ex:
            self._pkg_manifest_remote = None
            error_fn(ex)

        self._pkg_manifest_local = None
        if self._pkg_manifest_remote is not None:
            json_mtime = file_mtime_or_none(filepath_json)
            assert json_mtime is not None
            self._pkg_manifest_remote_mtime = json_mtime
            self._pkg_manifest_local = None
            self._pkg_manifest_remote_has_warning = False
        else:
            if not ignore_missing:
                # NOTE: this warning will occur when setting up a new repository.
                # It could be removed but it's also useful to know when the JSON is missing.
                if self.remote_url:
                    if not self._pkg_manifest_remote_has_warning:
                        print("Repository file:", filepath_json, "not found, sync required!")
                        self._pkg_manifest_remote_has_warning = True

        return self._pkg_manifest_remote

    def _json_data_refresh_from_toml(
            self,
            *,
            error_fn: Callable[[BaseException], None],
            force: bool = False,
    ) -> None:
        assert self.remote_url == ""
        # Since there is no remote repo the ID name is defined by the directory name only.
        local_json_data = self.pkg_manifest_from_local_ensure(error_fn=error_fn)
        if local_json_data is None:
            return

        filepath_json = os.path.join(self.directory, REPO_LOCAL_JSON)

        # We might want to adjust where this happens, create the directory here
        # because this could be a fresh repo might not have been initialized until now.
        directory = os.path.dirname(filepath_json)
        try:
            # A symbolic-link that's followed (good), if it exists and is a file an error is raised here and returned.
            if not os.path.isdir(directory):
                os.makedirs(directory, exist_ok=True)
        except BaseException as ex:
            error_fn(ex)
            return
        del directory

        with open(filepath_json, "w", encoding="utf-8") as fh:
            # Indent because it can be useful to check this file if there are any issues.

            # Begin: transform to list with ID's in item.
            # TODO: this transform can probably be removed and the internal format can change
            # to use the same structure as the actual JSON.
            local_json_data_compat = {
                "version": "v1",
                "blocklist": [],
                "data": [
                    {"id": pkg_idname, **value}
                    for pkg_idname, value in local_json_data.items()
                ],
            }
            # End: compatibility change.

            fh.write(json.dumps(local_json_data_compat, indent=2))

    def _json_data_refresh(
            self,
            *,
            error_fn: Callable[[BaseException], None],
            force: bool = False,
    ) -> None:
        if force or (self._pkg_manifest_remote is None) or (self._pkg_manifest_remote_mtime == 0):
            self._pkg_manifest_remote = None
            self._pkg_manifest_remote_mtime = 0
            self._pkg_manifest_local = None

        # Detect a local-only repository, there is no server to sync with
        # so generate the JSON from the TOML files.
        # While redundant this avoids having support multiple code-paths for local-only/remote repos.
        if self.remote_url == "":
            self._json_data_refresh_from_toml(error_fn=error_fn, force=force)

        filepath_json = os.path.join(self.directory, REPO_LOCAL_JSON)
        mtime_test = file_mtime_or_none(filepath_json)
        if self._pkg_manifest_remote is not None:
            # TODO: check the time of every installed package.
            if mtime_test == self._pkg_manifest_remote_mtime:
                return

        try:
            self._pkg_manifest_remote = json_from_filepath(filepath_json)
        except BaseException as ex:
            self._pkg_manifest_remote = None
            error_fn(ex)

        self._pkg_manifest_local = None
        if self._pkg_manifest_remote is not None:
            json_mtime = file_mtime_or_none(filepath_json)
            assert json_mtime is not None
            self._pkg_manifest_remote_mtime = json_mtime

    def pkg_manifest_from_local_ensure(
            self,
            *,
            error_fn: Callable[[BaseException], None],
            ignore_missing: bool = False,
    ) -> Optional[Dict[str, Dict[str, Any]]]:
        # Important for local-only repositories (where the directory name defines the ID).
        has_remote = self.remote_url != ""

        if self._pkg_manifest_local is None:
            self._json_data_ensure(
                ignore_missing=ignore_missing,
                error_fn=error_fn,
            )
            pkg_manifest_local = {}
            try:
                dir_entries = os.scandir(self.directory)
            except BaseException as ex:
                dir_entries = None
                error_fn(ex)

            for entry in (dir_entries if dir_entries is not None else ()):
                # Only check directories.
                if not entry.is_dir(follow_symlinks=True):
                    continue

                filename = entry.name

                # Simply ignore these paths without any warnings (accounts for `.git`, `__pycache__`, etc).
                if filename.startswith((".", "_")):
                    continue

                # Report any paths that cannot be used.
                if not filename.isidentifier():
                    error_fn(Exception("\"{:s}\" is not a supported module name, skipping".format(
                        os.path.join(self.directory, filename)
                    )))
                    continue

                filepath_toml = os.path.join(self.directory, filename, PKG_MANIFEST_FILENAME_TOML)
                try:
                    item_local = toml_from_filepath(filepath_toml)
                except BaseException as ex:
                    item_local = None
                    error_fn(ex)

                if item_local is None:
                    continue

                pkg_idname = item_local["id"]
                if has_remote:
                    # This should never happen, the user may have manually renamed a directory.
                    if pkg_idname != filename:
                        print("Skipping package with inconsistent name: \"{:s}\" mismatch \"{:s}\"".format(
                            filename,
                            pkg_idname,
                        ))
                        continue
                else:
                    pkg_idname = filename

                # Validate so local-only packages with invalid manifests aren't used.
                if (error_str := pkg_manifest_dict_is_valid_or_error(item_local, from_repo=False, strict=False)):
                    error_fn(Exception(error_str))
                    continue

                pkg_manifest_local[pkg_idname] = item_local
            self._pkg_manifest_local = pkg_manifest_local
        return self._pkg_manifest_local

    def pkg_manifest_from_remote_ensure(
            self,
            *,
            error_fn: Callable[[BaseException], None],
            ignore_missing: bool = False,
    ) -> Optional[Dict[str, Dict[str, Any]]]:
        if self._pkg_manifest_remote is None:
            self._json_data_ensure(
                ignore_missing=ignore_missing,
                error_fn=error_fn,
            )
        return self._pkg_manifest_remote

    def force_local_refresh(self) -> None:
        self._pkg_manifest_local = None


class RepoCacheStore:
    __slots__ = (
        "_repos",
        "_is_init",
    )

    def __init__(self) -> None:
        self._repos: List[_RepoCacheEntry] = []
        self._is_init = False

    def is_init(self) -> bool:
        return self._is_init

    def refresh_from_repos(
            self, *,
            repos: List[Tuple[str, str]],
            force: bool = False,
    ) -> None:
        """
        Initialize or update repositories.
        """
        repos_prev = {}
        if not force:
            for repo_entry in self._repos:
                repos_prev[repo_entry.directory, repo_entry.remote_url] = repo_entry
        self._repos.clear()

        for directory, remote_url in repos:
            repo_entry_test = repos_prev.get((directory, remote_url))
            if repo_entry_test is None:
                repo_entry_test = _RepoCacheEntry(directory, remote_url)
            self._repos.append(repo_entry_test)
        self._is_init = True

    def refresh_remote_from_directory(
            self,
            directory: str,
            *,
            error_fn: Callable[[BaseException], None],
            force: bool = False,
    ) -> None:
        for repo_entry in self._repos:
            if directory == repo_entry.directory:
                repo_entry._json_data_refresh(force=force, error_fn=error_fn)
                return
        raise ValueError("Directory {:s} not a known repo".format(directory))

    def refresh_local_from_directory(
            self,
            directory: str,
            *,
            error_fn: Callable[[BaseException], None],
            ignore_missing: bool = False,
            directory_subset: Optional[Set[str]] = None,
    ) -> Optional[Dict[str, Dict[str, Any]]]:
        for repo_entry in self._repos:
            if directory == repo_entry.directory:
                # Force refresh.
                repo_entry.force_local_refresh()
                return repo_entry.pkg_manifest_from_local_ensure(
                    ignore_missing=ignore_missing,
                    error_fn=error_fn,
                )
        raise ValueError("Directory {:s} not a known repo".format(directory))

    def pkg_manifest_from_remote_ensure(
            self,
            *,
            error_fn: Callable[[BaseException], None],
            check_files: bool = False,
            ignore_missing: bool = False,
            directory_subset: Optional[Set[str]] = None,
    ) -> Generator[Optional[Dict[str, Dict[str, Any]]], None, None]:
        for repo_entry in self._repos:
            if directory_subset is not None:
                if repo_entry.directory not in directory_subset:
                    continue

            json_data = repo_entry._json_data_ensure(
                check_files=check_files,
                ignore_missing=ignore_missing,
                error_fn=error_fn,
            )
            if json_data is None:
                # The repository may be fresh, not yet initialized.
                yield None
            else:
                pkg_manifest_remote = {}
                # "data" should always exist, it's not the purpose of this function to fully validate though.
                json_items = json_data.get("data")
                if json_items is None:
                    error_fn(ValueError("JSON was missing \"data\" key"))
                    yield None
                else:
                    for item_remote in json_items:
                        # TODO(@ideasman42): we may want to include the "id", as part of moving to a new format
                        # the "id" used not to be part of each item so users of this API assume it's not.
                        # The `item_remote` could be used in-place however that needs further testing.
                        item_remove_copy = item_remote.copy()
                        pkg_idname = item_remove_copy.pop("id")
                        pkg_manifest_remote[pkg_idname] = item_remove_copy
                    yield pkg_manifest_remote

    def pkg_manifest_from_local_ensure(
            self,
            *,
            error_fn: Callable[[BaseException], None],
            check_files: bool = False,
            directory_subset: Optional[Set[str]] = None,
    ) -> Generator[Optional[Dict[str, Dict[str, Any]]], None, None]:
        for repo_entry in self._repos:
            if directory_subset is not None:
                if repo_entry.directory not in directory_subset:
                    continue
            if check_files:
                repo_entry.force_local_refresh()
            yield repo_entry.pkg_manifest_from_local_ensure(error_fn=error_fn)

    def clear(self) -> None:
        self._repos.clear()
        self._is_init = False


# -----------------------------------------------------------------------------
# Public Repo Lock
#


class RepoLock:
    """
    Lock multiple repositories, one or all may fail,
    it's up to the caller to check.

    Access via the ``RepoLockContext`` where possible to avoid the lock being left held.
    """
    __slots__ = (
        "_repo_directories",
        "_repo_lock_files",
        "_cookie",
        "_held",
    )

    def __init__(self, *, repo_directories: Sequence[str], cookie: str):
        """
        :arg repo_directories:
            Directories to attempt to lock.
        :arg cookie:
            A path which is used as a reference.
            It must point to a path that exists.
            When a lock exists, check if the cookie path exists, if it doesn't, allow acquiring the lock.
        """
        self._repo_directories = tuple(repo_directories)
        self._repo_lock_files: List[Tuple[str, str]] = []
        self._held = False
        self._cookie = cookie

    def __del__(self) -> None:
        if not self._held:
            return
        sys.stderr.write("{:s}: freed without releasing lock!".format(type(self).__name__))

    @staticmethod
    def _is_locked_with_stale_cookie_removal(local_lock_file: str, cookie: str) -> Optional[str]:
        if os.path.exists(local_lock_file):
            try:
                with open(local_lock_file, "r", encoding="utf8") as fh:
                    data = fh.read()
            except BaseException as ex:
                return "lock file could not be read: {:s}".format(str(ex))

            # The lock is held.
            if os.path.exists(data):
                if data == cookie:
                    return "lock is already held by this session"
                return "lock is held by other session: {:s}".format(data)

            # The lock is held (but stale), remove it.
            try:
                os.remove(local_lock_file)
            except BaseException as ex:
                return "lock file could not be removed: {:s}".format(str(ex))
        return None

    def acquire(self) -> Dict[str, Optional[str]]:
        """
        Return directories and the lock status,
        with None if locking succeeded.
        """
        if self._held:
            raise Exception("acquire(): called with an existing lock!")
        if not os.path.exists(self._cookie):
            raise Exception("acquire(): cookie doesn't exist! (when it should)")

        # Assume all succeed.
        result: Dict[str, Optional[str]] = {directory: None for directory in self._repo_directories}
        for directory in self._repo_directories:
            local_private_dir = os.path.join(directory, REPO_LOCAL_PRIVATE_DIR)

            # This most likely exists, create if it doesn't.
            if not os.path.isdir(local_private_dir):
                os.makedirs(local_private_dir)

            local_lock_file = os.path.join(local_private_dir, REPO_LOCAL_PRIVATE_LOCK)
            # Attempt to get the lock, kick out stale locks.
            if (lock_msg := self._is_locked_with_stale_cookie_removal(local_lock_file, self._cookie)) is not None:
                result[directory] = "Lock exists: {:s}".format(lock_msg)
                continue
            try:
                with open(local_lock_file, "w", encoding="utf8") as fh:
                    fh.write(self._cookie)
            except BaseException as ex:
                result[directory] = "Lock could not be created: {:s}".format(str(ex))
                # Remove if it was created (but failed to write)... disk-full?
                try:
                    os.remove(local_lock_file)
                except BaseException:
                    pass
                continue

            # Success, the file is locked.
            self._repo_lock_files.append((directory, local_lock_file))
        self._held = True
        return result

    def release(self) -> Dict[str, Optional[str]]:
        # NOTE: lots of error checks here, mostly to give insights in the very unlikely case this fails.
        if not self._held:
            raise Exception("release(): called without a lock!")

        result: Dict[str, Optional[str]] = {directory: None for directory in self._repo_directories}
        for directory, local_lock_file in self._repo_lock_files:
            if not os.path.exists(local_lock_file):
                result[directory] = "release(): lock missing when expected, continuing."
                continue
            try:
                with open(local_lock_file, "r", encoding="utf8") as fh:
                    data = fh.read()
            except BaseException as ex:
                result[directory] = "release(): lock file could not be read: {:s}".format(str(ex))
                continue
            # Owned by another application, this shouldn't happen.
            if data != self._cookie:
                result[directory] = "release(): lock was unexpectedly stolen by another program: {:s}".format(data)
                continue

            # This is our lock file, we're allowed to remove it!
            try:
                os.remove(local_lock_file)
            except BaseException as ex:
                result[directory] = "release(): failed to remove file {!r}".format(ex)

        self._held = False
        return result


class RepoLockContext:
    __slots__ = (
        "_repo_lock",
    )

    def __init__(self, *, repo_directories: Sequence[str], cookie: str):
        self._repo_lock = RepoLock(repo_directories=repo_directories, cookie=cookie)

    def __enter__(self) -> Dict[str, Optional[str]]:
        return self._repo_lock.acquire()

    def __exit__(self, _ty: Any, _value: Any, _traceback: Any) -> None:
        self._repo_lock.release()
