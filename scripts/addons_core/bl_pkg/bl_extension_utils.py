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

    # Public Stand-Alone Utilities.
    "pkg_theme_file_list",
    "pkg_manifest_params_compatible_or_error",
    "platform_from_this_system",
    "url_append_query_for_blender",
    "url_parse_for_blender",
    "seconds_as_human_readable_text",
    "file_mtime_or_none",
    "scandir_with_demoted_errors",
    "rmtree_with_fallback_or_error",

    # Public API.
    "json_from_filepath",
    "toml_from_filepath",
    "json_to_filepath",

    "pkg_manifest_dict_is_valid_or_error",
    "pkg_manifest_dict_from_archive_or_error",
    "pkg_manifest_archive_url_abs_from_remote_url",

    "python_versions_from_wheel_python_tag",

    "CommandBatch",
    "RepoCacheStore",

    # Directory Lock.
    "RepoLock",
    "RepoLockContext",

    "repo_lock_directory_query",
)

import abc
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
    IO,
    NamedTuple,
)
from collections.abc import (
    Callable,
    Generator,
    Iterator,
    Sequence,
)

BASE_DIR = os.path.abspath(os.path.dirname(__file__))

# This directory is in the local repository.
REPO_LOCAL_PRIVATE_DIR = ".blender_ext"
# Locate inside `REPO_LOCAL_PRIVATE_DIR`.
REPO_LOCAL_PRIVATE_LOCK = "index.lock"

PKG_REPO_LIST_FILENAME = "index.json"
PKG_MANIFEST_FILENAME_TOML = "blender_manifest.toml"
PKG_EXT = ".zip"

# Components to use when creating temporary directory.
# Note that digits may be added to the suffix avoid conflicts.
PKG_TEMP_PREFIX_AND_SUFFIX = (".", ".~temp~")

# Add this to the local JSON file.
REPO_LOCAL_JSON = os.path.join(REPO_LOCAL_PRIVATE_DIR, PKG_REPO_LIST_FILENAME)

# An item we communicate back to Blender.
InfoItem = tuple[str, Any]
InfoItemSeq = Sequence[InfoItem]

COMPLETE_ITEM = ('DONE', "")

# Time to wait when there is no output, avoid 0 as it causes high CPU usage.
IDLE_WAIT_ON_READ = 0.05
# IDLE_WAIT_ON_READ = 0.2


# -----------------------------------------------------------------------------
# Typing Stubs
#
# These functions exist to allow messages to be translated,
# without having to depend on Blender-only modules, as this module is fully type-checked
# as well as being used outside of Blender.


# Maybe overwritten by `bpy.app.translations.pgettext_rpt`.
def rpt_(text: str) -> str:
    return text

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

    def file_handle_non_blocking_is_error_blocking(ex: Exception) -> bool:
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

    def file_handle_non_blocking_is_error_blocking(ex: Exception) -> bool:
        if not isinstance(ex, BlockingIOError):
            return False
        return True


def file_mtime_or_none(filepath: str) -> int | None:
    try:
        # For some reason `mypy` thinks this is a float.
        return int(os.stat(filepath)[stat.ST_MTIME])
    except FileNotFoundError:
        return None


def file_mtime_or_none_with_error_fn(
        filepath: str,
        *,
        error_fn: Callable[[Exception], None],
) -> int | None:
    try:
        # For some reason `mypy` thinks this is a float.
        return int(os.stat(filepath)[stat.ST_MTIME])
    except FileNotFoundError:
        pass
    except Exception as ex:
        error_fn(ex)
    return None


def scandir_with_demoted_errors(path: str) -> Iterator[os.DirEntry[str]]:
    try:
        yield from os.scandir(path)
    except Exception as ex:
        print("Error: scandir", ex)


def rmtree_with_fallback_or_error(
        path: str,
        *,
        remove_file: bool = True,
        remove_link: bool = True,
) -> str | None:
    from .cli.blender_ext import rmtree_with_fallback_or_error as fn
    result = fn(
        path,
        remove_file=remove_file,
        remove_link=remove_link,
    )
    assert result is None or isinstance(result, str)
    return result


def blender_ext_cmd(python_args: Sequence[str]) -> Sequence[str]:
    return (
        sys.executable,
        *python_args,
        os.path.normpath(os.path.join(BASE_DIR, "cli", "blender_ext.py")),
    )


# -----------------------------------------------------------------------------
# Call JSON.
#

def command_output_from_json_0(
        args: Sequence[str],
        use_idle: bool,
        *,
        python_args: Sequence[str],
) -> Generator[InfoItemSeq, bool, None]:
    cmd = [*blender_ext_cmd(python_args), *args, "--output-type=JSON_0"]
    # Note that the context-manager isn't used to wait until the process is finished as
    # the function only finishes when `poll()` is not none, it's just use to ensure file-handles
    # are closed before this function exits, this only seems to be a problem on WIN32.

    # WIN32 needs to use a separate process-group else Blender will receive the "break", see #131947.
    creationflags = 0
    if sys.platform == "win32":
        creationflags = subprocess.CREATE_NEW_PROCESS_GROUP

    with subprocess.Popen(cmd, stdout=subprocess.PIPE, creationflags=creationflags) as ps:
        stdout = ps.stdout
        assert stdout is not None

        # Needed so whatever is available can be read (without waiting).
        file_handle_make_non_blocking(stdout)

        chunk_list = []
        request_exit_signal_sent = False

        while True:
            # It's possible this is multiple chunks.
            try:
                chunk = stdout.read()
            except Exception as ex:
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
                if sys.platform == "win32":
                    # Caught by the `signal.SIGBREAK` signal handler.
                    ps.send_signal(signal.CTRL_BREAK_EVENT)
                else:
                    ps.send_signal(signal.SIGINT)
                request_exit_signal_sent = True


# -----------------------------------------------------------------------------
# Internal Functions.
#

# pylint: disable-next=useless-return
def repositories_validate_or_errors(repos: Sequence[str]) -> InfoItemSeq | None:
    _ = repos
    return None


def repository_iter_package_dirs(
        directory: str,
        *,
        error_fn: Callable[[Exception], None],
        ignore_missing: bool = False,
) -> Iterator[os.DirEntry[str]]:
    try:
        dir_entries = os.scandir(directory)
    except Exception as ex:
        # The `isinstance` check is ignored, suppress warning.
        # pylint: disable-next=no-member
        if not (ignore_missing and isinstance(ex, FileNotFoundError) and ex.filename == directory):
            error_fn(ex)
        dir_entries = None

    for entry in (dir_entries if dir_entries is not None else ()):
        # Only check directories.
        if not entry.is_dir(follow_symlinks=True):
            continue

        dirname = entry.name

        # Simply ignore these paths without any warnings (accounts for `.git`, `__pycache__`, etc).
        if dirname.startswith((".", "_")):
            continue

        # Report any paths that cannot be used.
        if not dirname.isidentifier():
            error_fn(Exception("\"{:s}\" is not a supported module name, skipping".format(
                os.path.join(directory, dirname)
            )))
            continue

        yield entry


def license_info_to_text(license_list: Sequence[str]) -> str:
    # See: https://spdx.org/licenses/
    # - Note that we could include all, for now only common, GPL compatible licenses.
    # - Note that many of the human descriptions are not especially more humanly readable
    #   than the short versions, so it's questionable if we should attempt to add all of these.
    _spdx_id_to_text = {
        "GPL-2.0-only": "GNU General Public License v2.0 only",
        "GPL-2.0-or-later": "GNU General Public License v2.0 or later",
        "GPL-3.0-only": "GNU General Public License v3.0 only",
        "GPL-3.0-or-later": "GNU General Public License v3.0 or later",
    }
    result = []
    for item in license_list:
        if item.startswith("SPDX:"):
            item = item[5:]
            item = _spdx_id_to_text.get(item, item)
        result.append(item)
    return ", ".join(result)


# -----------------------------------------------------------------------------
# Public Stand-Alone Utilities
#

def pkg_theme_file_list(directory: str, pkg_idname: str) -> tuple[str, list[str]]:
    theme_dir = os.path.join(directory, pkg_idname)
    theme_files = [
        filename for entry in os.scandir(theme_dir)
        if ((not entry.is_dir()) and
            (not (filename := entry.name).startswith(".")) and
            filename.lower().endswith(".xml"))
    ]
    theme_files.sort()
    return theme_dir, theme_files


def repo_index_outdated(directory: str) -> bool:
    filepath_json = os.path.join(directory, REPO_LOCAL_JSON)
    mtime = file_mtime_or_none(filepath_json)
    if mtime is None:
        return True

    # Refresh once every 24 hours.
    age_in_seconds = time.time() - mtime
    max_age_in_seconds = 3600.0 * 24.0
    # Use abs in case clock moved backwards.
    return abs(age_in_seconds) > max_age_in_seconds


def platform_from_this_system() -> str:
    from .cli.blender_ext import platform_from_this_system as platform_from_this_system_impl
    result = platform_from_this_system_impl()
    assert isinstance(result, str)
    return result


def _url_append_query(url: str, query: dict[str, str]) -> str:
    import urllib
    import urllib.parse

    # Remove empty parameters.
    query = {key: value for key, value in query.items() if value is not None and value != ""}
    if not query:
        return url

    # Decompose the URL into components.
    parsed_url = urllib.parse.urlparse(url)

    # Combine existing query parameters with new parameters
    query_existing = urllib.parse.parse_qsl(parsed_url.query)
    query_all = dict(query_existing)
    query_all.update(query)

    # Encode all parameters into a new query string.
    query_all_encoded = urllib.parse.urlencode(query_all)

    # Construct the URL with additional queries.
    new_url = urllib.parse.urlunparse((
        parsed_url.scheme,
        parsed_url.netloc,
        parsed_url.path,
        parsed_url.params,
        query_all_encoded,
        parsed_url.fragment,
    ))

    return new_url


def url_append_query_for_blender(
        *,
        url: str,
        blender_version: tuple[int, int, int],
        python_version: tuple[int, int, int],
) -> str:
    # `blender_version` is typically `bpy.app.version`.

    # While this won't cause errors, it's redundant to add this information to file URL's.
    if url.startswith("file://"):
        return url

    query = {
        "platform": platform_from_this_system(),
        "blender_version": "{:d}.{:d}.{:d}".format(*blender_version),
        "python_version": "{:d}.{:d}.{:d}".format(*python_version),
    }
    return _url_append_query(url, query)


def url_parse_for_blender(url: str) -> tuple[str, dict[str, str]]:
    # Split the URL into components:
    # - The stripped: `scheme + netloc + path`
    # - Known query values used by Blender.
    #   Extract `?repository=...` value from the URL and return it.
    #   Concatenating it where appropriate.
    #
    import urllib
    import urllib.parse

    parsed_url = urllib.parse.urlparse(url)
    query = urllib.parse.parse_qsl(parsed_url.query)

    url_strip = urllib.parse.urlunparse((
        parsed_url.scheme,
        parsed_url.netloc,
        parsed_url.path,
        None,  # `parsed_url.params,`
        None,  # `parsed_url.query,`
        None,  # `parsed_url.fragment,`
    ))

    query_known = {}
    for key, value in query:
        value_xform = None
        match key:
            case "blender_version_min" | "blender_version_max" | "python_versions" | "platforms":
                if value:
                    value_xform = value
            case "repository":
                if value:
                    if value.startswith("/"):
                        value_xform = urllib.parse.urlunparse((
                            parsed_url.scheme,
                            parsed_url.netloc,
                            value[1:],
                            None,  # `parsed_url.params,`
                            None,  # `parsed_url.query,`
                            None,  # `parsed_url.fragment,`
                        ))
                    elif value.startswith("./"):
                        value_xform = urllib.parse.urlunparse((
                            parsed_url.scheme,
                            parsed_url.netloc,
                            parsed_url.path.rsplit("/", 1)[0] + value[1:],
                            None,  # `parsed_url.params,`
                            None,  # `parsed_url.query,`
                            None,  # `parsed_url.fragment,`
                        ))
                    else:
                        value_xform = value
        if value_xform is not None:
            query_known[key] = value_xform

    return url_strip, query_known


def seconds_as_human_readable_text(seconds: float, unit_num: int) -> str:
    seconds_units = (
        ("year", "years", 31_556_952.0),
        ("week", "weeks", 604_800.0),
        ("day", "days", 86400.0),
        ("hour", "hours", 3600.0),
        ("minute", "minutes", 60.0),
        ("second", "seconds", 1.0),
    )
    result = []
    for unit_text, unit_text_plural, unit_value in seconds_units:
        if seconds >= unit_value:
            unit_count = int(seconds / unit_value)
            seconds -= (unit_count * unit_value)
            if unit_count > 1:
                result.append("{:d} {:s}".format(unit_count, unit_text_plural))
            else:
                result.append("{:d} {:s}".format(unit_count, unit_text))
            if len(result) == unit_num:
                break

    # For short time periods, always show something.
    if not result:
        result.append("{:.02g} {:s}".format(seconds / unit_value, unit_text_plural))

    return ", ".join(result)


# -----------------------------------------------------------------------------
# Public Repository Actions
#

def repo_sync(
        *,
        directory: str,
        remote_name: str,
        remote_url: str,
        online_user_agent: str,
        access_token: str,
        timeout: float,
        use_idle: bool,
        python_args: Sequence[str],
        force_exit_ok: bool = False,
        dry_run: bool = False,
        demote_connection_errors_to_status: bool = False,
        extension_override: str = "",
) -> Iterator[InfoItemSeq]:
    """
    Implementation:
    ``bpy.ops.ext.repo_sync(directory)``.
    """
    if dry_run:
        yield [COMPLETE_ITEM]
        return

    yield from command_output_from_json_0([
        "sync",
        "--local-dir", directory,
        "--remote-name", remote_name,
        "--remote-url", remote_url,
        "--online-user-agent", online_user_agent,
        "--access-token", access_token,
        "--timeout", "{:g}".format(timeout),
        *(("--force-exit-ok",) if force_exit_ok else ()),
        *(("--demote-connection-errors-to-status",) if demote_connection_errors_to_status else ()),
        *(("--extension-override", extension_override) if extension_override else ()),
    ], use_idle=use_idle, python_args=python_args)
    yield [COMPLETE_ITEM]


def repo_upgrade(
        *,
        directory: str,
        remote_url: str,
        online_user_agent: str,
        access_token: str,
        use_idle: bool,
        python_args: Sequence[str],
) -> Iterator[InfoItemSeq]:
    """
    Implementation:
    ``bpy.ops.ext.repo_upgrade(directory)``.
    """
    yield from command_output_from_json_0([
        "upgrade",
        "--local-dir", directory,
        "--remote-url", remote_url,
        "--online-user-agent", online_user_agent,
        "--access-token", access_token,
        "--temp-prefix-and-suffix", "/".join(PKG_TEMP_PREFIX_AND_SUFFIX),
    ], use_idle=use_idle, python_args=python_args)
    yield [COMPLETE_ITEM]


def repo_listing(
        *,
        repos: Sequence[str],
) -> Iterator[InfoItemSeq]:
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
        blender_version: tuple[int, int, int],
        python_version: tuple[int, int, int],
        use_idle: bool,
        python_args: Sequence[str],
) -> Iterator[InfoItemSeq]:
    """
    Implementation:
    ``bpy.ops.ext.pkg_install_files(directory, files)``.
    """
    yield from command_output_from_json_0([
        "install-files", *files,
        "--local-dir", directory,
        "--blender-version", "{:d}.{:d}.{:d}".format(*blender_version),
        "--python-version", "{:d}.{:d}.{:d}".format(*python_version),
        "--temp-prefix-and-suffix", "/".join(PKG_TEMP_PREFIX_AND_SUFFIX),
    ], use_idle=use_idle, python_args=python_args)
    yield [COMPLETE_ITEM]


def pkg_install(
        *,
        directory: str,
        remote_url: str,
        pkg_id_sequence: Sequence[str],
        blender_version: tuple[int, int, int],
        python_version: tuple[int, int, int],
        online_user_agent: str,
        access_token: str,
        timeout: float,
        use_cache: bool,
        use_idle: bool,
        python_args: Sequence[str],
) -> Iterator[InfoItemSeq]:
    """
    Implementation:
    ``bpy.ops.ext.pkg_install(directory, pkg_id)``.
    """
    yield from command_output_from_json_0([
        "install", ",".join(pkg_id_sequence),
        "--local-dir", directory,
        "--remote-url", remote_url,
        "--blender-version", "{:d}.{:d}.{:d}".format(*blender_version),
        "--python-version", "{:d}.{:d}.{:d}".format(*python_version),
        "--online-user-agent", online_user_agent,
        "--access-token", access_token,
        "--local-cache", str(int(use_cache)),
        "--timeout", "{:g}".format(timeout),
        "--temp-prefix-and-suffix", "/".join(PKG_TEMP_PREFIX_AND_SUFFIX),
    ], use_idle=use_idle, python_args=python_args)
    yield [COMPLETE_ITEM]


def pkg_uninstall(
        *,
        directory: str,
        user_directory: str,
        pkg_id_sequence: Sequence[str],
        use_idle: bool,
        python_args: Sequence[str],
) -> Iterator[InfoItemSeq]:
    """
    Implementation:
    ``bpy.ops.ext.pkg_uninstall(directory, pkg_id)``.
    """
    yield from command_output_from_json_0([
        "uninstall", ",".join(pkg_id_sequence),
        "--local-dir", directory,
        "--user-dir", user_directory,
        "--temp-prefix-and-suffix", "/".join(PKG_TEMP_PREFIX_AND_SUFFIX),
    ], use_idle=use_idle, python_args=python_args)
    yield [COMPLETE_ITEM]


# -----------------------------------------------------------------------------
# Public (non-command-line-wrapping) functions
#

def json_from_filepath(filepath_json: str) -> dict[str, Any] | None:
    if os.path.exists(filepath_json):
        with open(filepath_json, "r", encoding="utf-8") as fh:
            result = json.loads(fh.read())
            assert isinstance(result, dict)
            return result
    return None


def toml_from_filepath(filepath_json: str) -> dict[str, Any] | None:
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

    def key_replace(_match: re.Match[str]) -> str:
        return "version = \"0.0.0\""

    data = re.sub(r"^\s*version\s*=\s*\"[^\"]+\"", key_replace, data, flags=re.MULTILINE)
    with open(filepath, "w", encoding="utf-8") as fh:
        fh.write(data)


def pkg_manifest_dict_is_valid_or_error(
        data: dict[str, Any],
        from_repo: bool,
        strict: bool,
) -> str | None:
    # Exception! In in general `cli` shouldn't be considered a Python module,
    # it's validation function is handy to reuse.
    from .cli.blender_ext import pkg_manifest_from_dict_and_validate
    assert "id" in data
    result = pkg_manifest_from_dict_and_validate(data, from_repo=from_repo, strict=strict)
    if isinstance(result, str):
        return result
    return None


def pkg_manifest_dict_from_archive_or_error(
        filepath: str,
) -> dict[str, Any] | str:
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


def pkg_manifest_dict_apply_build_generated_table(manifest_dict: dict[str, Any]) -> None:
    from .cli.blender_ext import pkg_manifest_dict_apply_build_generated_table as fn
    fn(manifest_dict)


def pkg_is_legacy_addon(filepath: str) -> bool:
    from .cli.blender_ext import pkg_is_legacy_addon as pkg_is_legacy_addon_extern
    result = pkg_is_legacy_addon_extern(filepath)
    assert isinstance(result, bool)
    return result


def pkg_repo_cache_clear(local_dir: str) -> None:
    local_cache_dir = os.path.join(local_dir, REPO_LOCAL_PRIVATE_DIR, "cache")
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
        except Exception as ex:
            print("Error: unlink", ex)


def python_versions_from_wheel_python_tag(python_tag: str) -> set[tuple[int] | tuple[int, int]] | str:
    from .cli.blender_ext import python_versions_from_wheel_python_tag as fn
    result = fn(python_tag)
    assert isinstance(result, (set, str))
    return result


def python_versions_from_wheel_abi_tag(abi_tag: str, *, stable_only: bool) -> set[tuple[int] | tuple[int, int]] | str:
    from .cli.blender_ext import python_versions_from_wheel_abi_tag as fn
    result = fn(abi_tag, stable_only=stable_only)
    assert isinstance(result, (set, str))
    return result


def python_versions_from_wheels(wheel_files: Sequence[str]) -> set[tuple[int] | tuple[int, int]] | str:
    from .cli.blender_ext import python_versions_from_wheels as fn
    result = fn(wheel_files)
    assert isinstance(result, (set, str))
    return result


# -----------------------------------------------------------------------------
# Public Command Pool (non-command-line wrapper)
#

InfoItemCallable = Callable[[], Generator[InfoItemSeq, bool, None]]


class CommandBatchItem:
    __slots__ = (
        "fn_with_args",
        "fn_iter",
        "status",
        "has_fatal_error",
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
        self.fn_iter: Generator[InfoItemSeq, bool, None] | None = None
        self.status = CommandBatchItem.STATUS_NOT_YET_STARTED
        self.has_fatal_error = False
        self.has_error = False
        self.has_warning = False
        self.msg_log: list[tuple[str, Any]] = []
        self.msg_log_len_last = 0
        self.msg_type = ""
        self.msg_info = ""

    def invoke(self) -> Generator[InfoItemSeq, bool, None]:
        return self.fn_with_args()


class CommandBatch_ExecNonBlockingResult(NamedTuple):
    # A message list for each command, aligned to `CommandBatchItem._batch`.
    messages: tuple[list[tuple[str, str]], ...]
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
    """
    This class manages running command-line programs as sub-processes, abstracting away process management,
    performing non-blocking reads to access JSON output.

    The sub-processes must conform to the following constraints:

    - Only output JSON to the STDOUT.
    - Exit gracefully when: SIGINT signal is sent
      (``signal.CTRL_BREAK_EVENT`` on WIN32).
    - Errors must be caught and forwarded as JSON error messages.
      Unhandled exceptions are not expected and and will produce ugly
      messages from the STDERR output.

    The user of this class creates the class with all known jobs,
    setting the limit for the number of jobs that run simultaneously.

    The caller can then monitor the processes:
    - By calling ``exec_blocking``.
    - Or by periodically calling ``exec_non_blocking``.

      Canceling is performed by calling ``exec_non_blocking`` with ``request_exit=True``.
    """
    __slots__ = (
        "title",

        "_batch",
        "_batch_job_limit",
        "_request_exit",
        "_log_added_since_accessed",
    )

    def __init__(
            self,
            *,
            title: str,
            batch: Sequence[InfoItemCallable],
            batch_job_limit: int,
    ):
        self.title = title
        self._batch = [CommandBatchItem(fn_with_args) for fn_with_args in batch]
        self._batch_job_limit = batch_job_limit
        self._request_exit = False
        self._log_added_since_accessed = True

    def _exec_blocking_single(
            self,
            report_fn: Callable[[str, str], None],
            # TODO: investigate using this or removing it.
            # pylint: disable-next=unused-argument
            request_exit_fn: Callable[[], bool],
    ) -> bool:
        for cmd in self._batch:
            assert cmd.fn_iter is None
            cmd.fn_iter = cmd.invoke()
            request_exit: bool | None = None
            while True:
                try:
                    # Request `request_exit` starts off as None, then it's a boolean.
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
        command_output: tuple[list[tuple[str, str]], ...] = tuple([] for _ in range(len(self._batch)))

        if request_exit:
            self._request_exit = True

        status_data_changed = False

        # NOTE: the method of limiting the number of running jobs won't be efficient
        # with large numbers of jobs (tens of thousands or more), since all jobs are iterated over each time.
        # To support this a queue of not-yet-started jobs could be used ... or this whole function could be re-thought.
        # At this point in time using such large numbers of jobs isn't likely, so accept the simple loop each time
        # this function is called.
        batch_job_limit = self._batch_job_limit

        running_count = 0
        complete_count = 0

        for cmd_index in reversed(range(len(self._batch))):
            cmd = self._batch[cmd_index]
            if cmd.status == CommandBatchItem.STATUS_COMPLETE:
                complete_count += 1
                continue

            send_arg: bool | None = self._request_exit

            # First time initialization.
            if cmd.fn_iter is None:
                if 0 != batch_job_limit and running_count >= batch_job_limit:
                    # Try again later.
                    continue

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
                        if ty == 'FATAL_ERROR':
                            if not cmd.has_fatal_error:
                                cmd.has_fatal_error = True
                                status_data_changed = True
                        elif ty == 'ERROR':
                            if not cmd.has_error:
                                cmd.has_error = True
                                status_data_changed = True
                        elif ty == 'WARNING':
                            if not cmd.has_warning:
                                cmd.has_warning = True
                                status_data_changed = True
                        cmd.msg_log.append((ty, msg))

            if cmd.status == CommandBatchItem.STATUS_RUNNING:
                running_count += 1

        # Check if all are complete.
        assert complete_count == len([cmd for cmd in self._batch if cmd.status == CommandBatchItem.STATUS_COMPLETE])
        all_complete = complete_count == len(self._batch)
        return CommandBatch_ExecNonBlockingResult(
            messages=command_output,
            all_complete=all_complete,
            status_data_changed=status_data_changed,
        )

    def calc_status_string(self) -> list[str]:
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
            if cmd.has_fatal_error or cmd.has_error or cmd.has_warning:
                failure_count += 1
        return CommandBatch_StatusFlag(
            flag=status_flag,
            failure_count=failure_count,
            count=len(self._batch),
        )

    @staticmethod
    def calc_status_text_icon_from_data(
            status_data: CommandBatch_StatusFlag,
            update_count: int,
    ) -> tuple[str, str]:
        # Generate a nice UI string for a status-bar & splash screen (must be short).
        #
        # NOTE: this is (arguably) UI logic, it's just nice to have it here
        # as it avoids using low-level flags externally.
        #
        # FIXME: this text assumed a "sync" operation.
        if status_data.failure_count == 0:
            fail_text = ""
        elif status_data.failure_count == status_data.count:
            fail_text = rpt_(", failed")
        else:
            fail_text = rpt_(", some actions failed")

        if (
                status_data.flag == (1 << CommandBatchItem.STATUS_NOT_YET_STARTED) or
                status_data.flag & (1 << CommandBatchItem.STATUS_RUNNING)
        ):
            return rpt_("Checking for Extension Updates{:s}").format(fail_text), 'SORTTIME'

        if status_data.flag == 1 << CommandBatchItem.STATUS_COMPLETE:
            if update_count > 0:
                # NOTE: the UI design in #120612 has the number of extensions available in icon.
                # Include in the text as this is not yet supported.
                return rpt_("Extensions Updates Available ({:d}){:s}").format(update_count, fail_text), 'INTERNET'
            return rpt_("All Extensions Up-to-date{:s}").format(fail_text), 'CHECKMARK'

        # Should never reach this line!
        return rpt_("Internal error, unknown state!{:s}").format(fail_text), 'ERROR'

    def calc_status_log_or_none(self) -> list[tuple[str, str]] | None:
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

    def calc_status_log_since_last_request_or_none(self) -> list[list[tuple[str, str]]] | None:
        """
        Return a list of new errors per command or None when none are found.
        """
        result: list[list[tuple[str, str]]] = [[] for _ in range(len(self._batch))]
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
# Internal Repo Data Source
#

class PkgBlock_Normalized(NamedTuple):
    reason: str

    @staticmethod
    def from_dict_with_error_fn(
        block_dict: dict[str, Any],
        *,
        # Only for useful error messages.
        pkg_idname: str,
        error_fn: Callable[[Exception], None],
    ) -> "PkgBlock_Normalized | None":  # NOTE: quotes can be removed from typing in Py3.12+.

        try:
            reason = block_dict["reason"]
        except KeyError as ex:
            error_fn(KeyError("{:s}: missing key {:s}".format(pkg_idname, str(ex))))
            return None

        return PkgBlock_Normalized(
            reason=reason,
        )


# See similar named tuple: `bl_pkg.cli.blender_ext.PkgManifest`.
# This type is loaded from an external source and had it's valued parsed into a known "normalized" state.
# Some transformation is performed to the purpose of displaying in the UI although this type isn't specifically for UI.
class PkgManifest_Normalized(NamedTuple):
    # Intentionally excluded:
    # - `id`: The caller must know the ID and is typically stored as part of a dictionary
    #   where the `id` is the key and `PkgManifest_Normalized` is the value.
    # - `schema_version`: any versioning should be handled as part of normalization.
    # - `blender_version_max`: this is used to exclude packages, part of filtering before inclusion.
    name: str
    tagline: str
    version: str
    type: str
    maintainer: str
    license: str

    # Optional.
    website: str
    permissions: dict[str, str]
    tags: tuple[str]
    wheels: tuple[str]

    # Remote.
    archive_size: int
    archive_url: str

    # Taken from the `blocklist`.
    block: PkgBlock_Normalized | None

    @staticmethod
    def from_dict_with_error_fn(
        manifest_dict: dict[str, Any],
        *,
        # Only for useful error messages.
        pkg_idname: str,
        pkg_block: PkgBlock_Normalized | None,
        error_fn: Callable[[Exception], None],
    ) -> "PkgManifest_Normalized | None":
        # NOTE: it is expected there are no errors here for typical usage.
        # Any errors here will return none with a terse message which is not intended to
        # be helpful for debugging, besides letting users/developers know there is a problem.
        #
        # This is done because it's expected the data from repositories is valid and
        # anyone developing packages runs the "validate" function before publishing for others to use.
        #
        # Checks here are mainly to prevent corrupt/invalid repositories or TOML files
        # from breaking Blender's internal functionality.

        try:
            field_name = manifest_dict["name"]
            field_tagline = manifest_dict["tagline"]
            field_version = manifest_dict["version"]
            field_type = manifest_dict["type"]
            field_maintainer = manifest_dict["maintainer"]
            field_license = manifest_dict["license"]

            # Optional.
            field_website = manifest_dict.get("website", "")
            field_permissions: list[str] | dict[str, str] = manifest_dict.get("permissions", {})
            field_tags = manifest_dict.get("tags", [])
            field_wheels = manifest_dict.get("wheels", [])

            # Remote only (not found in TOML files).
            field_archive_size = manifest_dict.get("archive_size", 0)
            field_archive_url = manifest_dict.get("archive_url", "")

        except KeyError as ex:
            error_fn(KeyError("{:s}: missing key {:s}".format(pkg_idname, str(ex))))
            return None

        # This is an old (now unsupported) format, convert into a dictionary.
        if isinstance(field_permissions, list):
            field_permissions = {key: "Undefined" for key in field_permissions}

        try:
            if not (isinstance(field_name, str) and field_name):
                raise TypeError("{:s}: \"name\" must be a non-empty string".format(pkg_idname))

            if not isinstance(field_tagline, str):
                raise TypeError("{:s}: \"tagline\" must be a string".format(pkg_idname))

            if not (isinstance(field_version, str) and field_version):
                raise TypeError("{:s}: \"version\" must be a non-empty string".format(pkg_idname))

            if not (isinstance(field_type, str) and field_type):
                raise TypeError("{:s}: \"type\" must be a non-empty string".format(pkg_idname))

            if not (isinstance(field_maintainer, str) and field_maintainer):
                raise TypeError("{:s}: \"maintainer\" must be a non-empty string".format(pkg_idname))

            if not (
                    isinstance(field_license, list) and
                    field_license and
                    (not any(1 for x in field_license if not isinstance(x, str)))
            ):
                raise TypeError("{:s}: \"license\" must be a non-empty list of strings".format(pkg_idname))

            # Optional.
            if not isinstance(field_website, str):
                raise TypeError("{:s}: \"website\" must be a string".format(pkg_idname))

            if not (
                    isinstance(field_permissions, dict) and
                    (
                        not any(
                            1 for k, v in field_permissions.items()
                            if not (isinstance(k, str) and isinstance(v, str))
                        )
                    )
            ):
                raise TypeError("{:s}: \"permissions\" must be a non-empty list of strings".format(pkg_idname))

            if not (isinstance(field_tags, list) and (not any(1 for x in field_tags if not isinstance(x, str)))):
                raise TypeError("{:s}: \"tags\" must be a non-empty list of strings".format(pkg_idname))

            if not (isinstance(field_wheels, list) and (not any(1 for x in field_wheels if not isinstance(x, str)))):
                raise TypeError("{:s}: \"wheels\" must be a non-empty list of strings".format(pkg_idname))

            # Remote only.
            if not isinstance(field_archive_size, int):
                raise TypeError("{:s}: \"archive_size\" must be an int".format(pkg_idname))

            if not isinstance(field_archive_url, str):
                raise TypeError("{:s}: \"archive_url\" must a string".format(pkg_idname))

        except TypeError as ex:
            error_fn(ex)
            return None

        import re
        return PkgManifest_Normalized(
            name=field_name,
            tagline=field_tagline,
            version=field_version,
            type=field_type,
            # Remove the maintainers email while it's not private, showing prominently
            # could cause maintainers to get direct emails instead of issue tracking systems.
            maintainer=re.sub(r"\s*<.*?>", "", field_maintainer),
            license=license_info_to_text(field_license),

            # Optional.
            website=field_website,
            permissions=field_permissions,
            tags=tuple(field_tags),
            wheels=tuple(field_wheels),

            archive_size=field_archive_size,
            archive_url=field_archive_url,

            block=pkg_block,
        )


def repository_id_with_error_fn(
        item: dict[str, Any],
        *,
        repo_directory: str,
        error_fn: Callable[[Exception], None],
) -> str | None:
    if not (pkg_idname := item.get("id", "")):
        error_fn(ValueError("{:s}: \"id\" missing".format(repo_directory)))
        return None

    if not isinstance(pkg_idname, str):
        error_fn(ValueError("{:s}: \"id\" must be a string".format(repo_directory)))
        return None

    return pkg_idname


# Values used to exclude incompatible packages when listing & installing.
class PkgManifest_FilterParams(NamedTuple):
    platform: str
    blender_version: tuple[int, int, int]
    python_version: tuple[int, int, int]


def repository_filter_skip(
        item: dict[str, Any],
        filter_params: PkgManifest_FilterParams,
        error_fn: Callable[[Exception], None],
) -> bool:
    from .cli.blender_ext import repository_filter_skip as repository_filter_skip_impl
    result = repository_filter_skip_impl(
        item,
        filter_blender_version=filter_params.blender_version,
        filter_python_version=filter_params.python_version,
        filter_platform=filter_params.platform,
        skip_message_fn=None,
        error_fn=error_fn,
    )
    assert isinstance(result, bool)
    return result


def pkg_manifest_params_compatible_or_error(
        *,
        blender_version_min: str,
        blender_version_max: str,
        platforms: list[str],
        python_versions: list[str],
        this_platform: str,
        this_blender_version: tuple[int, int, int],
        this_python_version: tuple[int, int, int],
        error_fn: Callable[[Exception], None],
) -> str | None:
    from .cli.blender_ext import repository_filter_skip as fn

    # Weak, create the minimum information for a manifest to be checked against.
    item: dict[str, Any] = {}
    if blender_version_min:
        item["blender_version_min"] = blender_version_min
    if blender_version_max:
        item["blender_version_max"] = blender_version_max
    if platforms:
        item["platforms"] = platforms
    if python_versions:
        item["python_versions"] = python_versions

    result_report = []
    result = fn(
        item=item,
        filter_blender_version=this_blender_version,
        filter_python_version=this_python_version,
        filter_platform=this_platform,
        # pylint: disable-next=unnecessary-lambda
        skip_message_fn=lambda msg: result_report.append(msg),
        error_fn=error_fn,
    )
    assert isinstance(result, bool)
    if result:
        assert len(result_report) > 0
        return "\n".join(result_report)
    return None


def repository_parse_blocklist(
        data: list[dict[str, Any]],
        *,
        repo_directory: str,
        error_fn: Callable[[Exception], None],
) -> dict[str, PkgBlock_Normalized]:
    pkg_block_map = {}

    for item in data:
        if not isinstance(item, dict):
            error_fn(Exception("found non dict item in repository \"blocklist\", found {:s}".format(str(type(item)))))
            continue

        if (pkg_idname := repository_id_with_error_fn(
                item,
                repo_directory=repo_directory,
                error_fn=error_fn,
        )) is None:
            continue
        if (value := PkgBlock_Normalized.from_dict_with_error_fn(
                item,
                pkg_idname=pkg_idname,
                error_fn=error_fn,
        )) is None:
            # NOTE: typically we would skip invalid items
            # however as it's known this ID is blocked, create a dummy item.
            value = PkgBlock_Normalized(
                reason="Unknown (parse error)",
            )

        pkg_block_map[pkg_idname] = value

    return pkg_block_map


def repository_parse_data_filtered(
        data: list[dict[str, Any]],
        *,
        repo_directory: str,
        filter_params: PkgManifest_FilterParams,
        pkg_block_map: dict[str, PkgBlock_Normalized],
        error_fn: Callable[[Exception], None],
) -> dict[str, PkgManifest_Normalized]:
    pkg_manifest_map = {}
    for item in data:
        if not isinstance(item, dict):
            error_fn(Exception("found non dict item in repository \"data\", found {:s}".format(str(type(item)))))
            continue

        if (pkg_idname := repository_id_with_error_fn(
                item,
                repo_directory=repo_directory,
                error_fn=error_fn,
        )) is None:
            continue

        # No need to call: `pkg_manifest_dict_apply_build_generated_table(item_local)`
        # Because these values will have been applied when generating the JSON.
        assert "generated" not in item.get("build", {})

        if repository_filter_skip(item, filter_params, error_fn):
            continue

        if (value := PkgManifest_Normalized.from_dict_with_error_fn(
                item,
                pkg_idname=pkg_idname,
                pkg_block=pkg_block_map.get(pkg_idname),
                error_fn=error_fn,
        )) is None:
            continue

        pkg_manifest_map[pkg_idname] = value

    return pkg_manifest_map


class RepoRemoteData(NamedTuple):
    version: str
    # Converted from the `data` & `blocklist` fields.
    pkg_manifest_map: dict[str, PkgManifest_Normalized]


class _RepoDataSouce_ABC(metaclass=abc.ABCMeta):
    """
    The purpose of this class is to be a source for the repository data.

    Assumptions made by the implementation:
    - Data is stored externally (such as a file-system).
    - Data can be loaded in a single (blocking) operation.
    - Data is small enough to fit in memory.
    - It's faster to detect invalid cache than it is to load the data.
    """
    __slots__ = (
    )

    @abc.abstractmethod
    def exists(self) -> bool:
        raise Exception("Caller must define")

    @abc.abstractmethod
    def cache_is_valid(
            self,
            *,
            error_fn: Callable[[Exception], None],
    ) -> bool:
        raise Exception("Caller must define")

    @abc.abstractmethod
    def cache_clear(self) -> None:
        raise Exception("Caller must define")

    @abc.abstractmethod
    def cache_data(self) -> RepoRemoteData | None:
        raise Exception("Caller must define")

    # Should not be called directly use `data(..)` which supports cache.
    @abc.abstractmethod
    def _data_load(
            self,
            *,
            error_fn: Callable[[Exception], None],
    ) -> RepoRemoteData | None:
        raise Exception("Caller must define")

    def data(
            self,
            *,
            cache_validate: bool,
            force: bool,
            error_fn: Callable[[Exception], None],
    ) -> RepoRemoteData | None:
        if not self.exists():
            self.cache_clear()
            return None

        if force:
            self.cache_clear()
        elif cache_validate:
            if not self.cache_is_valid(error_fn=error_fn):
                self.cache_clear()

        if (data := self.cache_data()) is None:
            data = self._data_load(error_fn=error_fn)
        return data


class _RepoDataSouce_JSON(_RepoDataSouce_ABC):
    __slots__ = (
        "_data",

        "_filepath",
        "_filter_params",
        "_mtime",
    )

    def __init__(
            self,
            directory: str,
            filter_params: PkgManifest_FilterParams,
    ):
        filepath = os.path.join(directory, REPO_LOCAL_JSON)

        self._filepath: str = filepath
        self._mtime: int = 0
        self._filter_params: PkgManifest_FilterParams = filter_params
        self._data: RepoRemoteData | None = None

    def exists(self) -> bool:
        try:
            return os.path.exists(self._filepath)
        except Exception:
            return False

    def cache_is_valid(
            self,
            *,
            error_fn: Callable[[Exception], None],
    ) -> bool:
        if self._mtime == 0:
            return False
        if not self.exists():
            return False
        return self._mtime == file_mtime_or_none_with_error_fn(self._filepath, error_fn=error_fn)

    def cache_clear(self) -> None:
        self._data = None
        self._mtime = 0

    def cache_data(self) -> RepoRemoteData | None:
        return self._data

    def _data_load(
            self,
            *,
            error_fn: Callable[[Exception], None],
    ) -> RepoRemoteData | None:
        assert self.exists()

        data = None
        mtime = file_mtime_or_none_with_error_fn(self._filepath, error_fn=error_fn) or 0

        data_dict: dict[str, Any] = {}
        if mtime != 0:
            try:
                data_dict = json_from_filepath(self._filepath) or {}
            except Exception as ex:
                error_fn(ex)
            else:
                # This is *not* a full validation,
                # just skip malformed JSON files as they're likely to cause issues later on.
                if not isinstance(data_dict, dict):
                    error_fn(Exception("Remote repository data from {:s} must be a dict not a {:s}".format(
                        self._filepath,
                        str(type(data_dict)),
                    )))
                    data_dict = {}

                if not isinstance(data_dict.get("data"), list):
                    error_fn(Exception("Remote repository data from {:s} must contain a \"data\" list".format(
                        self._filepath,
                    )))
                    data_dict = {}

        # It's important to assign this value even if it's "empty",
        # otherwise corrupt files will be detected as unset and continuously attempt to load.

        repo_directory = os.path.dirname(self._filepath)

        # Useful for testing:
        # `data_dict["blocklist"] = [{"id": "math_vis_console", "reason": "This is blocked"}]`

        pkg_block_map = repository_parse_blocklist(
            data_dict.get("blocklist", []),
            repo_directory=repo_directory,
            error_fn=error_fn,
        )

        pkg_manifest_map = repository_parse_data_filtered(
            data_dict.get("data", []),
            repo_directory=repo_directory,
            filter_params=self._filter_params,
            pkg_block_map=pkg_block_map,
            error_fn=error_fn,
        )

        data = RepoRemoteData(
            version=data_dict.get("version", "v1"),
            pkg_manifest_map=pkg_manifest_map,
        )

        self._data = data
        self._mtime = mtime

        return data


class _RepoDataSouce_TOML_FILES(_RepoDataSouce_ABC):
    __slots__ = (
        "_data",

        "_directory",
        "_filter_params",
        "_mtime_for_each_package",
    )

    def __init__(
            self,
            directory: str,
            filter_params: PkgManifest_FilterParams,
    ):
        self._directory: str = directory
        self._filter_params = filter_params
        self._mtime_for_each_package: dict[str, int] | None = None
        self._data: RepoRemoteData | None = None

    def exists(self) -> bool:
        try:
            return os.path.isdir(self._directory)
        except Exception:
            return False

    def cache_is_valid(
            self,
            *,
            error_fn: Callable[[Exception], None],
    ) -> bool:
        if self._mtime_for_each_package is None:
            return False
        if not self.exists():
            return False

        if self._mtime_for_each_package_changed(
                directory=self._directory,
                mtime_for_each_package=self._mtime_for_each_package,
                error_fn=error_fn,
        ):
            return False

        return True

    def cache_clear(self) -> None:
        self._data = None
        self._mtime_for_each_package = None

    def cache_data(self) -> RepoRemoteData | None:
        return self._data

    def _data_load(
            self,
            *,
            error_fn: Callable[[Exception], None],
    ) -> RepoRemoteData | None:
        assert self.exists()

        mtime_for_each_package = self._mtime_for_each_package_create(
            directory=self._directory,
            error_fn=error_fn,
        )

        pkg_manifest_map: dict[str, PkgManifest_Normalized] = {}
        for dirname in mtime_for_each_package.keys():
            filepath_toml = os.path.join(self._directory, dirname, PKG_MANIFEST_FILENAME_TOML)
            try:
                item_local = toml_from_filepath(filepath_toml)
            except Exception as ex:
                item_local = None
                error_fn(ex)

            if item_local is None:
                continue

            # Unlikely but possible.
            if (pkg_idname := repository_id_with_error_fn(
                    item_local,
                    repo_directory=self._directory,
                    error_fn=error_fn,
            )) is None:
                continue

            # Apply generated variables before filtering.
            pkg_manifest_dict_apply_build_generated_table(item_local)

            if repository_filter_skip(item_local, self._filter_params, error_fn):
                continue

            if (value := PkgManifest_Normalized.from_dict_with_error_fn(
                    item_local,
                    pkg_idname=pkg_idname,
                    pkg_block=None,
                    error_fn=error_fn,
            )) is None:
                continue

            pkg_manifest_map[dirname] = value

        # Begin: transform to list with ID's in item.
        # TODO: this transform can probably be removed and the internal format can change
        # to use the same structure as the actual JSON.
        data = RepoRemoteData(
            version="v1",
            pkg_manifest_map=pkg_manifest_map,
        )
        # End: compatibility change.

        self._data = data
        self._mtime_for_each_package = mtime_for_each_package

        return data

    @classmethod
    def _mtime_for_each_package_create(
            cls,
            *,
            directory: str,
            error_fn: Callable[[Exception], None],
    ) -> dict[str, int]:
        # Caller must check `self.exists()`.
        assert os.path.isdir(directory)

        mtime_for_each_package: dict[str, int] = {}

        for entry in repository_iter_package_dirs(directory, error_fn=error_fn):
            dirname = entry.name
            filepath_toml = os.path.join(directory, dirname, PKG_MANIFEST_FILENAME_TOML)
            mtime_for_each_package[dirname] = file_mtime_or_none_with_error_fn(filepath_toml, error_fn=error_fn) or 0

        return mtime_for_each_package

    @classmethod
    def _mtime_for_each_package_changed(
            cls,
            *,
            directory: str,
            mtime_for_each_package: dict[str, int],
            error_fn: Callable[[Exception], None],
    ) -> bool:
        """
        Detect a change and return as early as possibly.
        Ideally this would not have to scan many files, since this could become *expensive*
        with very large repositories however as each package has its own TOML,
        there is no viable alternative.
        """
        # Caller must check `self.exists()`.
        assert os.path.isdir(directory)

        package_count = 0
        for entry in repository_iter_package_dirs(directory, error_fn=error_fn):
            filename = entry.name
            mtime_ref = mtime_for_each_package.get(filename)
            if mtime_ref is None:
                return True

            filepath_toml = os.path.join(directory, filename, PKG_MANIFEST_FILENAME_TOML)
            if mtime_ref != (file_mtime_or_none_with_error_fn(filepath_toml, error_fn=error_fn) or 0):
                return True
            package_count += 1

        if package_count != len(mtime_for_each_package):
            return True

        return False


# -----------------------------------------------------------------------------
# Public Repo Cache (non-command-line wrapper)


class _RepoCacheEntry:
    __slots__ = (
        "directory",
        "remote_url",

        "_pkg_manifest_local",
        "_pkg_manifest_remote",
        "_pkg_manifest_remote_data_source",
        "_pkg_manifest_remote_has_warning",

    )

    def __init__(
            self,
            directory: str,
            remote_url: str,
            filter_params: PkgManifest_FilterParams,
    ) -> None:
        assert directory != ""
        self.directory = directory
        self.remote_url = remote_url
        # Manifest data per package loaded from the packages local JSON.
        # TODO(@ideasman42): use `_RepoDataSouce_ABC` for `pkg_manifest_local`.
        self._pkg_manifest_local: dict[str, PkgManifest_Normalized] | None = None
        self._pkg_manifest_remote: dict[str, PkgManifest_Normalized] | None = None
        self._pkg_manifest_remote_data_source: _RepoDataSouce_ABC = (
            _RepoDataSouce_JSON(directory, filter_params) if remote_url else
            _RepoDataSouce_TOML_FILES(directory, filter_params)
        )
        # Avoid many noisy prints.
        self._pkg_manifest_remote_has_warning = False

    def _json_data_ensure(
            self,
            *,
            error_fn: Callable[[Exception], None],
            check_files: bool = False,
            ignore_missing: bool = False,
    ) -> dict[str, PkgManifest_Normalized] | None:
        data = self._pkg_manifest_remote_data_source.data(
            cache_validate=check_files,
            force=False,
            error_fn=error_fn,
        )

        pkg_manifest_remote: dict[str, PkgManifest_Normalized] | None = None
        if data is not None:
            pkg_manifest_remote = data.pkg_manifest_map

        if pkg_manifest_remote is not self._pkg_manifest_remote:
            self._pkg_manifest_remote = pkg_manifest_remote

        if pkg_manifest_remote is None:
            if not ignore_missing:
                # NOTE: this warning will occur when setting up a new repository.
                # It could be removed but it's also useful to know when the JSON is missing.
                if self.remote_url:
                    if not self._pkg_manifest_remote_has_warning:
                        print("Repository data:", self.directory, "not found, sync required!")
                        self._pkg_manifest_remote_has_warning = True

        return self._pkg_manifest_remote

    def _json_data_refresh(
            self,
            *,
            error_fn: Callable[[Exception], None],
            force: bool = False,
    ) -> dict[str, PkgManifest_Normalized] | None:
        data = self._pkg_manifest_remote_data_source.data(
            cache_validate=True,
            force=force,
            error_fn=error_fn,
        )

        pkg_manifest_remote: dict[str, PkgManifest_Normalized] | None = None
        if data is not None:
            pkg_manifest_remote = data.pkg_manifest_map

        if pkg_manifest_remote is not self._pkg_manifest_remote:
            self._pkg_manifest_remote = pkg_manifest_remote

        return pkg_manifest_remote

    def pkg_manifest_from_local_ensure(
            self,
            *,
            error_fn: Callable[[Exception], None],
            ignore_missing: bool = False,
    ) -> dict[str, PkgManifest_Normalized] | None:
        # Important for local-only repositories (where the directory name defines the ID).
        has_remote = self.remote_url != ""

        if self._pkg_manifest_local is None:
            pkg_manifest_local = {}

            for entry in repository_iter_package_dirs(
                    self.directory,
                    ignore_missing=ignore_missing,
                    error_fn=error_fn,
            ):
                dirname = entry.name
                filepath_toml = os.path.join(self.directory, dirname, PKG_MANIFEST_FILENAME_TOML)
                try:
                    item_local = toml_from_filepath(filepath_toml)
                except Exception as ex:
                    item_local = None
                    error_fn(ex)

                if item_local is None:
                    continue

                if (pkg_idname := repository_id_with_error_fn(
                        item_local,
                        repo_directory=self.directory,
                        error_fn=error_fn,
                )) is None:
                    continue

                if has_remote:
                    # This should never happen, the user may have manually renamed a directory.
                    if pkg_idname != dirname:
                        print("Skipping package with inconsistent name: \"{:s}\" mismatch \"{:s}\"".format(
                            dirname,
                            pkg_idname,
                        ))
                        continue
                else:
                    pkg_idname = dirname

                # Validate so local-only packages with invalid manifests aren't used.
                if (error_str := pkg_manifest_dict_is_valid_or_error(item_local, from_repo=False, strict=False)):
                    error_fn(Exception(error_str))
                    continue

                if (value := PkgManifest_Normalized.from_dict_with_error_fn(
                        item_local,
                        pkg_idname=pkg_idname,
                        pkg_block=None,
                        error_fn=error_fn,
                )) is not None:
                    pkg_manifest_local[pkg_idname] = value
                del value
            self._pkg_manifest_local = pkg_manifest_local
        return self._pkg_manifest_local

    def pkg_manifest_from_remote_ensure(
            self,
            *,
            error_fn: Callable[[Exception], None],
            ignore_missing: bool = False,
    ) -> dict[str, PkgManifest_Normalized] | None:
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
        "_filter_params",
        "_is_init",
    )

    def __init__(
        self,
            *,
            blender_version: tuple[int, int, int],
            python_version: tuple[int, int, int],
    ) -> None:
        self._repos: list[_RepoCacheEntry] = []
        self._filter_params = PkgManifest_FilterParams(
            platform=platform_from_this_system(),
            blender_version=blender_version,
            python_version=python_version,
        )
        self._is_init = False

    def is_init(self) -> bool:
        return self._is_init

    def refresh_from_repos(
            self, *,
            repos: list[tuple[str, str]],
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
                repo_entry_test = _RepoCacheEntry(directory, remote_url, self._filter_params)
            self._repos.append(repo_entry_test)
        self._is_init = True

    def refresh_remote_from_directory(
            self,
            directory: str,
            *,
            error_fn: Callable[[Exception], None],
            force: bool = False,
    ) -> dict[str, PkgManifest_Normalized] | None:
        for repo_entry in self._repos:
            if directory == repo_entry.directory:
                # pylint: disable-next=protected-access
                return repo_entry._json_data_refresh(force=force, error_fn=error_fn)
        raise ValueError("Directory {:s} not a known repo".format(directory))

    def refresh_local_from_directory(
            self,
            directory: str,
            *,
            error_fn: Callable[[Exception], None],
            ignore_missing: bool = False,
    ) -> dict[str, PkgManifest_Normalized] | None:
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
            error_fn: Callable[[Exception], None],
            check_files: bool = False,
            ignore_missing: bool = False,
            directory_subset: set[str] | None = None,
    ) -> Iterator[dict[str, PkgManifest_Normalized] | None]:
        for repo_entry in self._repos:
            if directory_subset is not None:
                if repo_entry.directory not in directory_subset:
                    continue

            # While we could yield a valid manifest here,
            # leave it to the caller to skip "remote" data for local-only repositories.
            if repo_entry.remote_url:
                # pylint: disable-next=protected-access
                yield repo_entry._json_data_ensure(
                    check_files=check_files,
                    ignore_missing=ignore_missing,
                    error_fn=error_fn,
                )
            else:
                yield None

    def pkg_manifest_from_local_ensure(
            self,
            *,
            error_fn: Callable[[Exception], None],
            check_files: bool = False,
            ignore_missing: bool = False,
            directory_subset: set[str] | None = None,
    ) -> Iterator[dict[str, PkgManifest_Normalized] | None]:
        for repo_entry in self._repos:
            if directory_subset is not None:
                if repo_entry.directory not in directory_subset:
                    continue
            if check_files:
                repo_entry.force_local_refresh()
            yield repo_entry.pkg_manifest_from_local_ensure(
                ignore_missing=ignore_missing,
                error_fn=error_fn,
            )

    def clear(self) -> None:
        self._repos.clear()
        self._is_init = False


# -----------------------------------------------------------------------------
# Public Repo Lock
#

# Currently this is based on a path, this gives significant room without the risk of not being large enough.
# The size limit is used to prevent over-allocating memory in the unlikely case a lot of data
# is written into the lock file.
_REPO_LOCK_SIZE_LIMIT = 16384


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
        assert len(cookie) <= _REPO_LOCK_SIZE_LIMIT, "Unreachable"
        self._repo_directories = tuple(repo_directories)
        self._repo_lock_files: list[tuple[str, str]] = []
        self._held = False
        self._cookie = cookie

    def __del__(self) -> None:
        if not self._held:
            return
        sys.stderr.write("{:s}: freed without releasing lock!".format(type(self).__name__))

    @staticmethod
    def _is_locked_with_stale_cookie_removal(local_lock_file: str, cookie: str) -> str | None:
        if os.path.exists(local_lock_file):
            try:
                with open(local_lock_file, "r", encoding="utf8") as fh:
                    data = fh.read(_REPO_LOCK_SIZE_LIMIT)
            except Exception as ex:
                return "lock file could not be read ({:s})".format(str(ex))

            # The lock is held.
            if os.path.exists(data):
                if data == cookie:
                    return "lock is already held by this session"
                return "lock is held by other session \"{:s}\"".format(data)

            # The lock is held (but stale), remove it.
            try:
                os.remove(local_lock_file)
            except Exception as ex:
                return "lock file could not be removed ({:s})".format(str(ex))
        return None

    def acquire(self) -> dict[str, str | None]:
        """
        Return directories and the lock status,
        with None if locking succeeded.
        """
        if self._held:
            raise Exception("acquire(): called with an existing lock!")
        if not os.path.exists(self._cookie):
            raise Exception("acquire(): cookie doesn't exist! (when it should)")

        # Assume all succeed.
        result: dict[str, str | None] = {directory: None for directory in self._repo_directories}
        for directory in self._repo_directories:
            local_private_dir = os.path.join(directory, REPO_LOCAL_PRIVATE_DIR)

            # This most likely exists, create if it doesn't.
            if not os.path.isdir(local_private_dir):
                try:
                    os.makedirs(local_private_dir)
                except Exception as ex:
                    # Likely no permissions or read-only file-system.
                    result[directory] = "lock directory could not be created ({:s})".format(str(ex))
                    continue

            local_lock_file = os.path.join(local_private_dir, REPO_LOCAL_PRIVATE_LOCK)
            # Attempt to get the lock, kick out stale locks.
            if (lock_msg := self._is_locked_with_stale_cookie_removal(local_lock_file, self._cookie)) is not None:
                result[directory] = "lock exists ({:s})".format(lock_msg)
                continue
            try:
                with open(local_lock_file, "w", encoding="utf8") as fh:
                    fh.write(self._cookie)
            except Exception as ex:
                result[directory] = "lock could not be created ({:s})".format(str(ex))
                # Remove if it was created (but failed to write)... disk-full?
                try:
                    os.remove(local_lock_file)
                except Exception:
                    pass
                continue

            # Success, the file is locked.
            self._repo_lock_files.append((directory, local_lock_file))
        self._held = True
        return result

    def release(self) -> dict[str, str | None]:
        # NOTE: lots of error checks here, mostly to give insights in the very unlikely case this fails.
        if not self._held:
            raise Exception("release(): called without a lock!")

        result: dict[str, str | None] = {directory: None for directory in self._repo_directories}
        for directory, local_lock_file in self._repo_lock_files:
            if not os.path.exists(local_lock_file):
                result[directory] = "release(): lock missing when expected, continuing."
                continue
            try:
                with open(local_lock_file, "r", encoding="utf8") as fh:
                    data = fh.read(_REPO_LOCK_SIZE_LIMIT)
            except Exception as ex:
                result[directory] = "release(): lock file could not be read ({:s})".format(str(ex))
                continue
            # Owned by another application, this shouldn't happen.
            if data != self._cookie:
                result[directory] = "release(): lock was unexpectedly stolen by another program ({:s})".format(data)
                continue

            # This is our lock file, we're allowed to remove it!
            try:
                os.remove(local_lock_file)
            except Exception as ex:
                result[directory] = "release(): failed to remove file ({!r})".format(ex)

        self._held = False
        return result


class RepoLockContext:
    __slots__ = (
        "_repo_lock",
    )

    def __init__(self, *, repo_directories: Sequence[str], cookie: str):
        self._repo_lock = RepoLock(repo_directories=repo_directories, cookie=cookie)

    def __enter__(self) -> dict[str, str | None]:
        return self._repo_lock.acquire()

    def __exit__(self, _ty: Any, _value: Any, _traceback: Any) -> None:
        self._repo_lock.release()


# -----------------------------------------------------------------------------
# Public Repo Lock Query & Unlock Support
#

def repo_lock_directory_query(
        directory: str,
        cookie: str,
) -> tuple[bool, float, str] | None:
    local_lock_file = os.path.join(directory, REPO_LOCAL_PRIVATE_DIR, REPO_LOCAL_PRIVATE_LOCK)

    cookie_is_ours = False
    cookie_mtime = 0.0
    cookie_error = ""

    try:
        cookie_stat = os.stat(local_lock_file)
    except FileNotFoundError:
        return None
    except Exception as ex:
        cookie_error = "lock file could not stat: {:s}".format(str(ex))
    else:
        cookie_mtime = cookie_stat[stat.ST_MTIME]

        data = ""
        try:
            with open(local_lock_file, "r", encoding="utf8") as fh:
                data = fh.read(_REPO_LOCK_SIZE_LIMIT)
        except Exception as ex:
            cookie_error = "lock file could not be read: {:s}".format(str(ex))

        cookie_is_ours = cookie == data

    return cookie_is_ours, cookie_mtime, cookie_error


def repo_lock_directory_force_unlock(
        directory: str,
) -> str | None:
    local_lock_file = os.path.join(directory, REPO_LOCAL_PRIVATE_DIR, REPO_LOCAL_PRIVATE_LOCK)
    try:
        os.remove(local_lock_file)
    except Exception as ex:
        return str(ex)
    return None
