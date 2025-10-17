#!/usr/bin/env python
# SPDX-FileCopyrightText: 2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
Command for managing Blender extensions.
"""


import argparse
import contextlib
import hashlib  # for SHA1 check-summing files.
import io
import json
import os
import re
import shutil
import signal  # Override `Ctrl-C`.
import sys
import tomllib
import urllib.error  # For `URLError`.
import urllib.parse  # For `urljoin`.
import urllib.request  # For accessing remote `https://` paths.
import zipfile


from typing import (
    Any,
    Iterator,
    IO,
    NamedTuple,
)
from collections.abc import (
    Callable,
    Sequence,
)

ArgsSubparseFn = Callable[["argparse._SubParsersAction[argparse.ArgumentParser]"], None]

REQUEST_EXIT = False

# When set, ignore broken pipe exceptions (these occur when the calling processes is closed).
FORCE_EXIT_OK = False


def signal_handler_sigint(_sig: int, _frame: Any) -> None:
    # pylint: disable-next=global-statement
    global REQUEST_EXIT
    REQUEST_EXIT = True


# A primitive type that can be communicated via message passing.
PrimType = int | str
PrimTypeOrSeq = PrimType | Sequence[PrimType]

MessageFn = Callable[[str, PrimTypeOrSeq], bool]

VERSION = "0.1"

PKG_EXT = ".zip"
# PKG_JSON_INFO = "index.json"

PKG_REPO_LIST_FILENAME = "index.json"

# Only for building.
PKG_MANIFEST_FILENAME_TOML = "blender_manifest.toml"

# This directory is in the local repository.
REPO_LOCAL_PRIVATE_DIR = ".blender_ext"

URL_KNOWN_PREFIX = ("http://", "https://", "file://")

MESSAGE_TYPES = {
    # Status report about what is being done.
    'STATUS',
    # A special kind of message used to denote progress & can be used to show a progress bar.
    'PROGRESS',
    # A problem was detected the user should be aware of which does not prevent the action from completing.
    # In Blender these are reported as warnings,
    # this means they are shown in the status-bar as well as being available in the "Info" editor,
    # unlike `ERROR` & `FATAL_ERROR` which present a blocking popup.
    'WARN',
    # Failure to complete all actions, some may have succeeded.
    'ERROR',
    # An error causing the operation not to complete as expected.
    # Where possible, failure states should be detected and exit before performing any destructive operation.
    'FATAL_ERROR',
    # TODO: check on refactoring this type away as it's use could be avoided entirely.
    'PATH',
    # Must always be the last message.
    'DONE',
}

RE_MANIFEST_SEMVER = re.compile(
    r'^'
    r'(?P<major>0|[1-9]\d*)\.'
    r'(?P<minor>0|[1-9]\d*)\.'
    r'(?P<patch>0|[1-9]\d*)'
    r'(?:-(?P<prerelease>(?:0|[1-9]\d*|\d*[a-zA-Z-][0-9a-zA-Z-]*)(?:\.(?:0|[1-9]\d*|\d*[a-zA-Z-][0-9a-zA-Z-]*))*))?'
    r'(?:\+(?P<buildmetadata>[0-9a-zA-Z-]+(?:\.[0-9a-zA-Z-]+)*))?$'
)

# Ensure names (for example), don't contain control characters.
RE_CONTROL_CHARS = re.compile(r'[\x00-\x1f\x7f-\x9f]')

# Use to extract a Python version tag: `py3`, `cp311` etc, from a wheel's filename.
RE_PYTHON_WHEEL_VERSION_TAG = re.compile("([a-zA-Z]+)([0-9]+)")

# Progress updates are displayed after each chunk of this size is downloaded.
# Small values add unnecessary overhead showing progress, large values will make
# progress not update often enough.
#
# Note that this could be dynamic although it's not a priority.
#
# 16kb to be responsive even on slow connections.
CHUNK_SIZE_DEFAULT = 1 << 14

# Short descriptions for the UI:
# Used for project tag-line & permissions values.
TERSE_DESCRIPTION_MAX_LENGTH = 64

# Enforce naming spec:
# https://packaging.python.org/en/latest/specifications/binary-distribution-format/#file-name-convention
# This also defines the name spec:
WHEEL_FILENAME_SPEC = "{distribution}-{version}(-{build tag})?-{python tag}-{abi tag}-{platform tag}.whl"

# Default HTML for `server-generate`.
# Intentionally very basic, users may define their own `--html-template`.
HTML_TEMPLATE = '''\
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <title>Blender Extensions</title>
</head>
<body>
<p>Blender Extension Listing:</p>
${body}
<center><p>Built ${date}</p></center>
</body>
</html>
'''


# -----------------------------------------------------------------------------
# Workarounds

def _worlaround_win32_ssl_cert_failure() -> None:
    # Applies workaround by `pukkandan` on GITHUB at run-time:
    # See: https://github.com/python/cpython/pull/91740
    import ssl

    class SSLContext_DUMMY(ssl.SSLContext):
        def _load_windows_store_certs(self, storename: str, purpose: ssl.Purpose) -> bytearray:
            # WIN32 only.
            enum_certificates = getattr(ssl, "enum_certificates", None)
            assert callable(enum_certificates)
            certs = bytearray()
            try:
                for cert, encoding, trust in enum_certificates(storename):
                    try:
                        self.load_verify_locations(cadata=cert)
                    except ssl.SSLError:
                        # warnings.warn("Bad certificate in Windows certificate store")
                        pass
                    else:
                        # CA certs are never PKCS#7 encoded
                        if encoding == "x509_asn":
                            if trust is True or purpose.oid in trust:
                                certs.extend(cert)
            except PermissionError:
                # warnings.warn("unable to enumerate Windows certificate store")
                pass
            # NOTE(@ideasman42): Python never uses this return value internally.
            # Keep it for consistency.
            return certs

    # pylint: disable-next=protected-access
    ssl.SSLContext._load_windows_store_certs = SSLContext_DUMMY._load_windows_store_certs  # type: ignore


# -----------------------------------------------------------------------------
# Argument Overrides

class _ArgsDefaultOverride:
    __slots__ = (
        "build_valid_tags",
    )

    def __init__(self) -> None:
        self.build_valid_tags = ""


# Support overriding this value so Blender can default to a different tags file.
ARG_DEFAULTS_OVERRIDE = _ArgsDefaultOverride()
del _ArgsDefaultOverride


# Standard out may be communicating with a parent process,
# arbitrary prints are NOT acceptable.


# pylint: disable-next=redefined-builtin
def print(*args: Any, **kw: dict[str, Any]) -> None:
    raise Exception("Illegal print(*({!r}), **{{{!r}}})".format(args, kw))

# # Useful for testing.
# def print(*args: Any, **kw: dict[str, Any]):
#     __builtins__["print"](*args, **kw, file=open('/tmp/output.txt', 'a'))


def any_as_none(_arg: Any) -> None:
    pass


def debug_stack_trace_to_file() -> None:
    """
    Debugging.
    """
    import inspect
    stack = inspect.stack(context=1)
    with open("/tmp/out.txt", "w", encoding="utf-8") as fh:
        for frame_info in stack[1:]:
            fh.write("{:s}:{:d}: {:s}\n".format(
                frame_info.filename,
                frame_info.lineno,
                frame_info.function,
            ))


class MessageLogger:
    __slots__ = (
        "msg_fn",
    )

    def __init__(self, msg_fn: MessageFn) -> None:
        self.msg_fn = msg_fn

    def done(self) -> bool:
        return self.msg_fn("DONE", "")

    def warn(self, s: str) -> bool:
        return self.msg_fn("WARN", s)

    def error(self, s: str) -> bool:
        return self.msg_fn("ERROR", s)

    def fatal_error(self, s: str) -> bool:
        return self.msg_fn("FATAL_ERROR", s)

    def status(self, s: str) -> bool:
        return self.msg_fn("STATUS", s)

    def path(self, s: str) -> bool:
        return self.msg_fn("PATH", s)

    def progress(self, s: str, progress: int, progress_range: int, unit: str) -> bool:
        assert unit == 'BYTE'
        return self.msg_fn("PROGRESS", (s, unit, progress, progress_range))


def force_exit_ok_enable() -> None:
    # pylint: disable-next=global-statement
    global FORCE_EXIT_OK
    FORCE_EXIT_OK = True
    # Without this, some errors are printed on exit.
    sys.unraisablehook = lambda _ex: None


# -----------------------------------------------------------------------------
# Generic Functions

def execfile(filepath: str) -> dict[str, Any]:
    global_namespace = {"__file__": filepath, "__name__": "__main__"}
    with open(filepath, "rb") as fh:
        # pylint: disable-next=exec-used
        exec(compile(fh.read(), filepath, 'exec'), global_namespace)
    return global_namespace


def size_as_fmt_string(num: float, *, precision: int = 1) -> str:
    for unit in ("B", "KB", "MB", "GB", "TB", "PB", "EB", "ZB"):
        if abs(num) < 1024.0:
            return "{:3.{:d}f}{:s}".format(num, precision, unit)
        num /= 1024.0
    unit = "YB"
    return "{:.{:d}f}{:s}".format(num, precision, unit)


def read_with_timeout(fh: IO[bytes], size: int, *, timeout_in_seconds: float) -> bytes | None:
    # TODO: implement timeout (TimeoutError).
    _ = timeout_in_seconds
    return fh.read(size)


class CleanupPathsContext:
    __slots__ = (
        "files",
        "directories",
    )

    def __init__(self, *, files: Sequence[str], directories: Sequence[str]) -> None:
        self.files = files
        self.directories = directories

    def __enter__(self) -> "CleanupPathsContext":
        return self

    def __exit__(self, _ty: Any, _value: Any, _traceback: Any) -> None:
        for f in self.files:
            if not os.path.exists(f):
                continue
            try:
                os.unlink(f)
            except Exception:
                pass

        for d in self.directories:
            if not os.path.exists(d):
                continue
            try:
                shutil.rmtree(d)
            except Exception:
                pass


# -----------------------------------------------------------------------------
# Generic Functions

class PkgRepoData(NamedTuple):
    version: str
    blocklist: list[dict[str, Any]]
    data: list[dict[str, Any]]


class PkgManifest_Build(NamedTuple):
    """Package Build Information (for the "build" sub-command)."""
    paths: list[str] | None
    paths_exclude_pattern: list[str] | None

    @staticmethod
    def _from_dict_impl(
            manifest_build_dict: dict[str, Any],
            *,
            extra_paths: Sequence[str],
            all_errors: bool,
    ) -> "PkgManifest_Build | list[str]":  # NOTE: quotes can be removed from typing in Py3.12+.
        # TODO: generalize the type checks, see: `pkg_manifest_is_valid_or_error_impl`.
        error_list = []
        if value := manifest_build_dict.get("paths"):
            if not isinstance(value, list):
                error_list.append("[build]: \"paths\" must be a list, not a {!r}".format(type(value)))
            else:
                value = [*value, *extra_paths]
                if (error := pkg_manifest_validate_field_build_path_list(value, strict=True)) is not None:
                    error_list.append(error)
            if not all_errors:
                return error_list
            paths = value
        else:
            paths = None

        if value := manifest_build_dict.get("paths_exclude_pattern"):
            if not isinstance(value, list):
                error_list.append("[build]: \"paths_exclude_pattern\" must be a list, not a {!r}".format(type(value)))
            elif (error := pkg_manifest_validate_field_any_list_of_non_empty_strings(value, strict=True)) is not None:
                error_list.append(error)
            if not all_errors:
                return error_list
            paths_exclude_pattern = value
        else:
            paths_exclude_pattern = None

        if (paths is not None) and (paths_exclude_pattern is not None):
            error_list.append("[build]: declaring both \"paths\" and \"paths_exclude_pattern\" is not supported")

        if error_list:
            return error_list

        return PkgManifest_Build(
            paths=paths,
            paths_exclude_pattern=paths_exclude_pattern,
        )

    @staticmethod
    def from_dict_all_errors(
            manifest_build_dict: dict[str, Any],
            extra_paths: Sequence[str],
    ) -> "PkgManifest_Build | list[str]":  # NOTE: quotes can be removed from typing in Py3.12+.
        return PkgManifest_Build._from_dict_impl(
            manifest_build_dict,
            extra_paths=extra_paths,
            all_errors=True,
        )


class PkgManifest(NamedTuple):
    """Package Information."""
    schema_version: str
    id: str
    name: str
    tagline: str
    version: str
    type: str
    maintainer: str
    license: list[str]
    blender_version_min: str

    # Optional (set all defaults).
    blender_version_max: str | None = None
    website: str | None = None
    copyright: list[str] | None = None
    permissions: list[str] | None = None
    tags: list[str] | None = None
    platforms: list[str] | None = None
    wheels: list[str] | None = None


class PkgManifest_Archive(NamedTuple):
    """Package Information with archive information."""
    # NOTE: no support for default values (unlike `PkgManifest`).
    manifest: PkgManifest
    archive_size: int
    archive_hash: str
    archive_url: str


class PkgServerRepoConfig(NamedTuple):
    """Server configuration (for generating repositories)."""
    schema_version: str
    blocklist: list[dict[str, Any]]


# -----------------------------------------------------------------------------
# Generic Functions


def path_to_url(path: str) -> str:
    from urllib.parse import urljoin
    from urllib.request import pathname2url
    return urljoin("file:", pathname2url(path))


def path_from_url(path: str) -> str:
    from urllib.parse import urlparse, unquote
    p = urlparse(path)
    path_unquote = unquote(p.path)
    if sys.platform == "win32":
        # MS-Windows needs special handling for drive letters.
        # `file:///C:/test` is converted to `/C:/test` which must skip the leading slash.
        if (p.netloc == "") and re.match("/[A-Za-z]:", path_unquote):
            result = path_unquote[1:]
        else:
            # Handle UNC paths: `\\HOST\share\path` as a URL on MS-Windows.
            # - MS-Edge: `file://HOST/share/path` where `netloc="HOST"`, `path="/share/path"`.
            # - Firefox: `file://///HOST/share/path`  where `netloc=""`, `path="///share/path"`.
            if p.netloc:
                result = "//{:s}/{:s}".format(p.netloc, path_unquote.lstrip("/"))
            else:
                result = "//{:s}".format(path_unquote.lstrip("/"))
    else:
        result = os.path.join(p.netloc, path_unquote)

    return result


def random_acii_lines(*, seed: int | str, width: int) -> Iterator[str]:
    """
    Generate random ASCII text [A-Za-z0-9].
    Intended not to compress well, it's possible to simulate downloading a large package.
    """
    import random
    import string

    chars_init = string.ascii_letters + string.digits
    chars = chars_init
    while len(chars) < width:
        chars = chars + chars_init

    r = random.Random(seed)
    chars_list = list(chars)
    while True:
        r.shuffle(chars_list)
        yield "".join(chars_list[:width])


def sha256_from_file_or_error(
        filepath: str,
        block_size: int = 1 << 20,
        hash_prefix: bool = False,
) -> tuple[int, str] | str:
    """
    Returns an arbitrary sized unique ASCII string based on the file contents.
    (exact hashing method may change).
    """
    try:
        # pylint: disable-next=consider-using-with
        fh_context = open(filepath, 'rb')
    except Exception as ex:
        return "error opening file: {:s}".format(str(ex))

    with contextlib.closing(fh_context) as fh:
        size = 0
        sha256 = hashlib.new('sha256')
        while True:
            try:
                data = fh.read(block_size)
            except Exception as ex:
                return "error reading file: {:s}".format(str(ex))

            if not data:
                break
            sha256.update(data)
            size += len(data)
        # Skip the `0x`.
        return size, ("sha256:" + sha256.hexdigest()) if hash_prefix else sha256.hexdigest()


def scandir_recursive_impl(
        base_path: str,
        path: str,
        *,
        filter_fn: Callable[[str, bool], bool],
) -> Iterator[tuple[str, str]]:
    """Recursively yield DirEntry objects for given directory."""
    for entry in os.scandir(path):
        if entry.is_symlink():
            continue

        entry_path = entry.path
        entry_path_relateive = os.path.relpath(entry_path, base_path)

        is_dir = entry.is_dir()
        if not filter_fn(entry_path_relateive, is_dir):
            continue

        if is_dir:
            yield from scandir_recursive_impl(
                base_path,
                entry_path,
                filter_fn=filter_fn,
            )
        elif entry.is_file():
            yield entry_path, entry_path_relateive


def scandir_recursive(
        path: str,
        filter_fn: Callable[[str, bool], bool],
) -> Iterator[tuple[str, str]]:
    yield from scandir_recursive_impl(path, path, filter_fn=filter_fn)


def rmtree_with_fallback_or_error(
        path: str,
        *,
        remove_file: bool = True,
        remove_link: bool = True,
) -> str | None:
    """
    Remove a directory, with optional fallbacks to removing files & links.
    Use this when a directory is expected, but there is the possibility
    that there is a file or symbolic-link which should be removed instead.

    Intended to be used for user managed files,
    where removal is required and we can't be certain of the kind of file.

    On failure, a string will be returned containing the first error.
    """

    # Note that `shutil.rmtree` has link detection that doesn't match `os.path.islink` exactly,
    # so use it's callback that raises a link error and remove the link in that case.
    errors = []

    # *DEPRECATED* 2024/07/01 Remove when 3.11 is dropped.
    if sys.version_info >= (3, 12):
        shutil.rmtree(path, onexc=lambda *args: errors.append(args))
    else:
        # Ignore as the deprecated logic is only used for older Python versions.
        # pylint: disable-next=deprecated-argument
        shutil.rmtree(path, onerror=lambda *args: errors.append((args[0], args[1], args[2][1])))

    # Happy path (for practically all cases).
    if not errors:
        return None

    is_file = False
    is_link = False

    for err_type, _err_path, ex in errors:
        if isinstance(ex, NotADirectoryError):
            if err_type is os.rmdir:
                is_file = True
        if isinstance(ex, OSError):
            if err_type is os.path.islink:
                is_link = True

    do_unlink = False
    if is_file:
        if remove_file:
            do_unlink = True
    if is_link:
        if remove_link:
            do_unlink = True

    if do_unlink:
        # Replace errors with the failure state of `os.unlink`.
        errors.clear()
        try:
            os.unlink(path)
        except Exception as ex:
            errors.append((os.unlink, path, ex))

    if errors:
        # Other information may be useful but it's too verbose to forward to user messages
        # and is more for debugging purposes.
        return str(errors[0][2])

    return None


def rmtree_with_fallback_or_error_pseudo_atomic(
        path: str,
        *,
        temp_prefix_and_suffix: tuple[str, str],
        remove_file: bool = True,
        remove_link: bool = True,
) -> str | None:

    # It's possible the directory doesn't exist, only attempt a rename if it does.
    try:
        isdir = os.path.isdir(path)
    except Exception:
        isdir = False

    if isdir:
        # Apply the prefix/suffix.
        path_base_dirname, path_base_filename = os.path.split(path.rstrip(os.sep))
        path_base = os.path.join(
            path_base_dirname,
            temp_prefix_and_suffix[0] + path_base_filename + temp_prefix_and_suffix[1],
        )
        del path_base_dirname, path_base_filename

        path_test = path_base
        test_count = 0
        # Unlikely this exists.
        while os.path.lexists(path_test):
            path_test = "{:s}{:d}".format(path_base, test_count)
            test_count += 1
            # Very unlikely, something is likely incorrect in the setup, avoid hanging.
            if test_count > 1000:
                return "Unable to create a new path at: {:s}".format(path_test)

        # NOTE(@ideasman42): on WIN32 renaming a directory will fail if any files within the directory are open.
        # The rename is important, the reasoning is as follows.
        # - If the rename fails, the entire directory is left as-is.
        #   This is done because in the case of an upgrade we *never* want to leave the extension
        #   in a broken state, with some files removed and the directory locked,
        #   meaning that an updated extension cannot be written to the destination.
        # - If the rename succeeds but deleting the directory fails (unlikely but possible),
        #   then at least the directory name is available (necessary for an upgrade).
        #   The directory will use the `temp_prefix_and_suffix` can be removed later.
        #
        # On other systems, renaming before removal isn't important but is harmless,
        # so keep it to avoid logic diverging.
        #
        # See #128175.

        try:
            os.rename(path, path_test)
        except Exception as ex:
            ex_str = str(ex)
            if isinstance(ex, PermissionError):
                if sys.platform == "win32":
                    if "The process cannot access the file because it is being used by another process" in ex_str:
                        return "locked by another process: {:s}".format(path)
            return ex_str

        path = path_test

    return rmtree_with_fallback_or_error(
        path,
        remove_file=remove_file,
        remove_link=remove_link,
    )


def build_paths_expand_iter(
        path: str,
        path_list: Sequence[str],
) -> Iterator[tuple[str, str]]:
    """
    Expand paths from a path list which always uses "/" slashes.
    """
    path_swap = os.sep != "/"
    path_strip = path.rstrip(os.sep)
    for filepath in path_list:
        # This is needed so it's possible to compare relative paths against string literals.
        # So it's possible to know if a path list includes a path or not.
        #
        # Simple path normalization:
        # - Remove redundant slashes.
        # - Strip all `./`.
        while "//" in filepath:
            filepath = filepath.replace("//", "/")
        while filepath.startswith("./"):
            filepath = filepath[2:]

        if path_swap:
            filepath = filepath.replace("/", "\\")

        # Avoid `os.path.join(path, filepath)` because `path` is ignored `filepath` is an absolute path.
        # In the contest of declaring build paths we *never* want to reference an absolute directory
        # such as `C:\path` or `/tmp/path`.
        yield (
            "{:s}{:s}{:s}".format(path_strip, os.sep, filepath.lstrip(os.sep)),
            filepath,
        )


def filepath_skip_compress(filepath: str) -> bool:
    """
    Return true when this file shouldn't be compressed while archiving.
    Speeds up archive creation, especially for large ``*.whl`` files.
    """
    # NOTE: for now use simple extension check, we could check the magic number too.
    return filepath.lower().endswith((
        # Python wheels.
        ".whl",
        # Archives (exclude historic formats: `*.arj`, `*.lha` ... etc).
        ".bz2",
        ".gz",
        ".lz4",
        ".lzma",
        ".rar",
        ".xz",
        ".zip",
        ".zst",
        # TAR combinations.
        ".tbz2",
        ".tgz",
        ".txz",
        ".tzst",
    ))


def pkg_manifest_from_dict_and_validate_impl(
        data: dict[Any, Any],
        *,
        from_repo: bool,
        all_errors: bool,
        strict: bool,
) -> PkgManifest | list[str]:
    error_list = []
    # Validate the dictionary.
    if all_errors:
        if (x := pkg_manifest_is_valid_or_error_all(data, from_repo=from_repo, strict=strict)) is not None:
            error_list.extend(x)
    else:
        if (error_msg := pkg_manifest_is_valid_or_error(data, from_repo=from_repo, strict=strict)) is not None:
            error_list.append(error_msg)

    if error_list:
        return error_list

    values: list[str] = []
    for key in PkgManifest._fields:
        val = data.get(key, ...)
        if val is ...:
            # pylint: disable-next=no-member,protected-access
            val = PkgManifest._field_defaults.get(key, ...)
        # `pkg_manifest_is_valid_or_error{_all}` will have caught this, assert all the same.
        assert val is not ...
        values.append(val)

    kw_args: dict[str, Any] = dict(zip(PkgManifest._fields, values, strict=True))
    manifest = PkgManifest(**kw_args)

    # There could be other validation, leave these as-is.
    return manifest


def pkg_manifest_from_dict_and_validate(
        data: dict[Any, Any],
        from_repo: bool,
        strict: bool,
) -> PkgManifest | str:
    manifest = pkg_manifest_from_dict_and_validate_impl(data, from_repo=from_repo, all_errors=False, strict=strict)
    if isinstance(manifest, list):
        return manifest[0]
    return manifest


def pkg_manifest_from_dict_and_validate_all_errros(
        data: dict[Any, Any],
        from_repo: bool,
        strict: bool,
) -> PkgManifest | list[str]:
    """
    Validate the manifest and return all errors.
    """
    return pkg_manifest_from_dict_and_validate_impl(data, from_repo=from_repo, all_errors=True, strict=strict)


def pkg_manifest_archive_from_dict_and_validate(
        data: dict[Any, Any],
        strict: bool,
) -> PkgManifest_Archive | str:
    manifest = pkg_manifest_from_dict_and_validate(data, from_repo=True, strict=strict)
    if isinstance(manifest, str):
        return manifest

    assert isinstance(manifest, PkgManifest)
    return PkgManifest_Archive(
        manifest=manifest,
        archive_size=data["archive_size"],
        # Repositories that use their own hash generation may use capitals,
        # ensure always lowercase for comparison (hashes generated here are always lowercase).
        archive_hash=data["archive_hash"].lower(),
        archive_url=data["archive_url"],
    )


def pkg_manifest_from_toml_and_validate_all_errors(
        filepath: str,
        strict: bool,
) -> PkgManifest | list[str]:
    """
    This function is responsible for not letting invalid manifest from creating packages with ID names
    or versions that would not properly install.

    The caller is expected to use exception handling and forward any errors to the user.
    """
    try:
        with open(filepath, "rb") as fh:
            data = tomllib.load(fh)
    except Exception as ex:
        return [str(ex)]

    return pkg_manifest_from_dict_and_validate_all_errros(data, from_repo=False, strict=strict)


def pkg_zipfile_detect_subdir_or_none(
        zip_fh: zipfile.ZipFile,
) -> str | None:
    if PKG_MANIFEST_FILENAME_TOML in zip_fh.NameToInfo:
        return ""
    # Support one directory containing the expected TOML.
    # ZIP's always use "/" (not platform specific).
    test_suffix = "/" + PKG_MANIFEST_FILENAME_TOML

    base_dir = None
    for filename in zip_fh.NameToInfo.keys():
        if filename.startswith("."):
            continue
        if not filename.endswith(test_suffix):
            continue
        # Only a single directory (for sanity sake).
        if filename.find("/", len(filename) - len(test_suffix)) == -1:
            continue

        # Multiple packages in a single archive, bail out as this is not a supported scenario.
        if base_dir is not None:
            base_dir = None
            break

        # Don't break in case there are multiple, in that case this function should return None.
        base_dir = filename[:-len(PKG_MANIFEST_FILENAME_TOML)]

    return base_dir


def pkg_manifest_from_zipfile_and_validate_impl(
        zip_fh: zipfile.ZipFile,
        archive_subdir: str,
        all_errors: bool,
        strict: bool,
) -> PkgManifest | list[str]:
    """
    Validate the manifest and return all errors.
    """
    # `archive_subdir` from `pkg_zipfile_detect_subdir_or_none`.
    assert archive_subdir == "" or archive_subdir.endswith("/")

    try:
        file_content = zip_fh.read(archive_subdir + PKG_MANIFEST_FILENAME_TOML)
    except KeyError:
        # TODO: check if there is a nicer way to handle this?
        # From a quick look there doesn't seem to be a good way
        # to do this using public methods.
        file_content = None

    if file_content is None:
        return ["Archive does not contain a manifest"]

    if isinstance((manifest_dict := toml_from_bytes_or_error(file_content)), str):
        return ["Archive contains a manifest that could not be parsed {:s}".format(manifest_dict)]

    assert isinstance(manifest_dict, dict)
    pkg_manifest_dict_apply_build_generated_table(manifest_dict)

    return pkg_manifest_from_dict_and_validate_impl(
        manifest_dict,
        from_repo=False,
        all_errors=all_errors,
        strict=strict,
    )


def pkg_manifest_from_zipfile_and_validate(
        zip_fh: zipfile.ZipFile,
        archive_subdir: str,
        strict: bool,
) -> PkgManifest | str:
    manifest = pkg_manifest_from_zipfile_and_validate_impl(
        zip_fh,
        archive_subdir,
        all_errors=False,
        strict=strict,
    )
    if isinstance(manifest, list):
        return manifest[0]
    return manifest


def pkg_manifest_from_zipfile_and_validate_all_errors(
        zip_fh: zipfile.ZipFile,
        archive_subdir: str,
        strict: bool,
) -> PkgManifest | list[str]:
    return pkg_manifest_from_zipfile_and_validate_impl(
        zip_fh,
        archive_subdir,
        all_errors=True,
        strict=strict,
    )


def pkg_manifest_from_archive_and_validate(
        filepath: str,
        strict: bool,
) -> PkgManifest | str:
    try:
        # pylint: disable-next=consider-using-with
        zip_fh_context = zipfile.ZipFile(filepath, mode="r")
    except Exception as ex:
        return "Error extracting archive \"{:s}\"".format(str(ex))

    with contextlib.closing(zip_fh_context) as zip_fh:
        if (archive_subdir := pkg_zipfile_detect_subdir_or_none(zip_fh)) is None:
            return "Archive has no manifest: \"{:s}\"".format(PKG_MANIFEST_FILENAME_TOML)
        return pkg_manifest_from_zipfile_and_validate(zip_fh, archive_subdir, strict=strict)


def pkg_server_repo_config_from_toml_and_validate(
        filepath: str,
) -> PkgServerRepoConfig | str:

    if isinstance(result := toml_from_filepath_or_error(filepath), str):
        return result

    if not (field_schema_version := result.get("schema_version", "")):
        return "missing \"schema_version\" field"

    if not (field_blocklist := result.get("blocklist", "")):
        return "missing \"blocklist\" field"

    for item in field_blocklist:
        if not isinstance(item, dict):
            return "blocklist contains non dictionary item, found ({:s})".format(str(type(item)))
        if not isinstance(value := item.get("id"), str):
            return "blocklist items must have have a string typed \"id\" entry, found {:s}".format(str(type(value)))
        if not isinstance(value := item.get("reason"), str):
            return "blocklist items must have have a string typed \"reason\" entry, found {:s}".format(str(type(value)))

    return PkgServerRepoConfig(
        schema_version=field_schema_version,
        blocklist=field_blocklist,
    )


def pkg_is_legacy_addon(filepath: str) -> bool:
    # Python file is legacy.
    if os.path.splitext(filepath)[1].lower() == ".py":
        return True

    try:
        # pylint: disable-next=consider-using-with
        zip_fh_context = zipfile.ZipFile(filepath, mode="r")
    except Exception:
        return False

    with contextlib.closing(zip_fh_context) as zip_fh:
        # If manifest not legacy.
        if pkg_zipfile_detect_subdir_or_none(zip_fh) is not None:
            return False

        # If any Python file contains bl_info it's legacy.
        for filename in zip_fh_context.NameToInfo.keys():
            if filename.startswith("."):
                continue
            if not filename.lower().endswith(".py"):
                continue
            try:
                file_content = zip_fh.read(filename)
            except Exception:
                file_content = None
            if file_content and file_content.find(b"bl_info") != -1:
                return True

    return False


def remote_url_has_filename_suffix(url: str) -> bool:
    # When the URL ends with `.json` it's assumed to be a URL that is inside a directory.
    # In these cases the file is stripped before constricting relative paths.
    return url.endswith("/" + PKG_REPO_LIST_FILENAME)


def remote_url_get(url: str) -> str:
    if url_is_filesystem(url):
        if remote_url_has_filename_suffix(url):
            return url
        # Add the default name to `file://` URL's if this isn't already a reference to a JSON.
        return "{:s}/{:s}".format(url.rstrip("/"), PKG_REPO_LIST_FILENAME)

    return url


def remote_url_params_strip(url: str) -> str:
    # Parse the URL to get its scheme, domain, and query parameters.
    parsed_url = urllib.parse.urlparse(url)

    # Combine the scheme, netloc, path without any other parameters, stripping the URL.
    new_url = urllib.parse.urlunparse((
        parsed_url.scheme,
        parsed_url.netloc,
        parsed_url.path,
        None,  # `parsed_url.params,`
        None,  # `parsed_url.query,`
        None,  # `parsed_url.fragment,`
    ))

    return new_url


def remote_url_validate_or_error(url: str) -> str | None:
    if url_has_known_prefix(url):
        return None
    return "remote URL doesn't begin with a known prefix: {:s}".format(" ".join(URL_KNOWN_PREFIX))


# -----------------------------------------------------------------------------
# TOML Helpers

def toml_repr_string(text: str) -> str:
    # Encode a string for literal inclusion as a value in a TOML file (including quotes).
    import string
    # NOTE: this could be empty, using literal characters ensures simple strings & paths are readable.
    literal_chars = set(string.digits + string.ascii_letters + "/_-. ")
    result = ["\""]
    for c in text:
        if c in literal_chars:
            result.append(c)
        elif (c_scalar := ord(c)) <= 0xffff:
            result.append("\\u{:04x}".format(c_scalar))
        else:
            result.append("\\U{:08x}".format(c_scalar))
    result.append("\"")
    return "".join(result)


# -----------------------------------------------------------------------------
# ZipFile Helpers

def zipfile_make_root_directory(
        zip_fh: zipfile.ZipFile,
        root_dir: str,
) -> None:
    """
    Make ``root_dir`` the new root of this ``zip_fh``, remove all other files.
    """
    # WARNING: this works but it's not pretty,
    # alternative solutions involve duplicating too much of ZipFile's internal logic.
    assert root_dir.endswith("/")
    filelist = zip_fh.filelist
    filelist_copy = filelist[:]
    filelist.clear()
    for member in filelist_copy:
        filename = member.filename
        if not filename.startswith(root_dir):
            continue
        # Ensure the path is not _ony_ the directory (can happen for some ZIP files).
        if not (filename := filename[len(root_dir):]):
            continue

        member.filename = filename
        filelist.append(member)


# -----------------------------------------------------------------------------
# Path Matching

class PathPatternMatch:
    """
    A pattern matching class that takes a list of patterns and has a ``test_path`` method.

    Patterns:
    - Matching that follows ``gitignore`` logic.

    Paths (passed to the ``test_path`` method):
    - All paths must use forward slashes.
    - All paths must be normalized and have no leading ``.`` or ``/`` characters.
    - Directories end with a trailing ``/``.

    Other notes:
    - Pattern matching doesn't require the paths to exist on the file-system.
    """
    # Implementation Notes:
    # - Path patterns use UNIX style glob.
    # - Matching doesn't depend on the order patterns are declared.
    # - Use of REGEX is an implementation detail, not exposed to the API.
    # - Glob uses using `fnmatch.translate` however this does not allow `*`
    #   to delimit on `/` which is necessary for `gitignore` style matching.
    #   So `/` are replaced with newlines, then REGEX multi-line logic is used
    #   to delimit the separators.
    # - This is used for building packages, so it doesn't have to to especially fast,
    #   although it shouldn't cause noticeable delays at build time.
    # - The test is located in: `../cli/test_path_pattern_match.py`

    __slots__ = (
        "_regex_list",
    )

    def __init__(self, path_patterns: list[str]):
        self._regex_list: list[tuple[bool, re.Pattern[str]]] = PathPatternMatch._pattern_match_as_regex(path_patterns)

    def test_path(self, path: str) -> bool:
        assert not path.startswith("/")
        path_test = path.rstrip("/").replace("/", "\n")
        if path.endswith("/"):
            path_test = path_test + "/"
        # For debugging.
        # print("`" + path_test + "`")
        result = False
        for negate, regex in self._regex_list:
            if regex.match(path_test):
                if negate:
                    result = False
                    break
                # Match but don't break as this may be negated in the future.
                result = True
        return result

    # Internal implementation.

    @staticmethod
    def _pattern_match_as_regex_single(pattern: str) -> str:
        from fnmatch import translate

        # Special case: `!` literal prefix, needed to avoid this being handled as negation.
        if pattern.startswith("\\!"):
            pattern = pattern[1:]

        # Avoid confusing pattern matching logic, strip duplicate slashes.
        while True:
            pattern_next = pattern.replace("//", "/")
            if len(pattern_next) != len(pattern):
                pattern = pattern_next
            else:
                del pattern_next
                break

        # Remove redundant leading/trailing "**"
        # Besides being redundant, they break `pattern_double_star_indices` checks below.
        while True:  # Strip end.
            if pattern.endswith("/**/"):
                pattern = pattern[:-3]
            elif pattern.endswith("/**"):
                pattern = pattern[:-2]
            else:
                break
        while True:  # Strip start.
            if pattern.startswith("/**/"):
                pattern = pattern[4:]
            elif pattern.startswith("**/"):
                pattern = pattern[3:]
            else:
                break
        while True:  # Strip middle.
            pattern_next = pattern.replace("/**/**/", "/**/")
            if len(pattern_next) != len(pattern):
                pattern = pattern_next
            else:
                del pattern_next
                break

        any_prefix = True
        only_directory = False

        if pattern.startswith("/"):
            any_prefix = False
            pattern = pattern.lstrip("/")

        if pattern.endswith("/"):
            only_directory = True
            pattern = pattern.rstrip("/")

        # Separate components:
        pattern_split = pattern.split("/")

        pattern_double_star_indices = []

        for i, elem in enumerate(pattern_split):
            if elem == "**":
                # Note on `**` matching.
                # Supporting this is complicated considerably by having to support
                # `a/**/b` matching `a/b` (as well as `a/x/b` & `a/x/y/z/b`).
                # Without the requirement to match `a/b` we could simply do this:
                # `pattern_split[i] = "(?s:.*)"`
                # However that assumes a path separator before & after the expression.
                #
                # Instead, build a list of double-star indices which are joined to the surrounding elements.
                pattern_double_star_indices.append(i)
                continue

            # Some manipulation is needed for the REGEX result of `translate`.
            #
            # - Always adds an "end-of-string" match which isn't desired here.
            #
            elem_regex = translate(pattern_split[i]).removesuffix("\\Z")
            # Don't match newlines.
            if elem_regex.startswith("(?s:"):
                elem_regex = "(?:" + elem_regex[4:]

            pattern_split[i] = elem_regex

        for i in reversed(pattern_double_star_indices):
            assert pattern_split[i] == "**"
            pattern_triple = pattern_split[i - 1: i + 2]
            assert len(pattern_triple) == 3
            assert pattern_triple[1] == "**"
            del pattern_split[i - 1:i + 1]
            pattern_split[i - 1] = (
                pattern_triple[0] +
                "(?:\n|\n(?s:.*)\n)?" +
                pattern_triple[2]
            )
        del pattern_double_star_indices

        # Convert path separators.
        pattern = "\\n".join(pattern_split)

        if any_prefix:
            # Ensure the preceding text ends with a slash (or nothing).
            pattern = "(?s:.*)^" + pattern
        else:
            # Match string start (not line start).
            pattern = "\\A" + pattern

        if only_directory:
            # Ensure this only ever matches a directly.
            pattern = pattern + "[\\n/]"
        else:
            # Ensure this isn't part of a longer string.
            pattern = pattern + "/?\\Z"

        return pattern

    @staticmethod
    def _pattern_match_as_regex(path_patterns: Sequence[str]) -> list[tuple[bool, re.Pattern[str]]]:
        # First group negative-positive expressions.
        pattern_groups: list[tuple[bool, list[str]]] = []
        for pattern in path_patterns:
            if pattern.startswith("!"):
                pattern = pattern.lstrip("!")
                negate = True
            else:
                negate = False
            if not pattern:
                continue

            if not pattern_groups:
                pattern_groups.append((negate, []))

            pattern_regex = PathPatternMatch._pattern_match_as_regex_single(pattern)
            if pattern_groups[-1][0] == negate:
                pattern_groups[-1][1].append(pattern_regex)
            else:
                pattern_groups.append((negate, [pattern_regex]))

        result: list[tuple[bool, re.Pattern[str]]] = []
        for negate, pattern_list in pattern_groups:
            result.append((negate, re.compile("(?:{:s})".format("|".join(pattern_list)), re.MULTILINE)))
        # print(result)
        return result


# -----------------------------------------------------------------------------
# URL Downloading


# NOTE:
# - Using return arguments isn't ideal but is better than including
#   a static value in the iterator.
# - Other data could be added here as needed (response headers if the caller needs them).
class DataRetrieveInfo:
    """
    When accessing a file from a URL or from the file-system,
    this is a "return" argument so the caller can know the size of the chunks it's iterating over,
    or -1 when the size is not known.
    """
    __slots__ = (
        "size_hint",
    )
    size_hint: int

    def __init__(self) -> None:
        self.size_hint = -1


# Originally based on `urllib.request.urlretrieve`.
def url_retrieve_to_data_iter(
        url: str,
        *,
        data: Any | None = None,
        headers: dict[str, str],
        chunk_size: int,
        timeout_in_seconds: float,
        retrieve_info: DataRetrieveInfo,
) -> Iterator[bytes]:
    """
    Iterate over byte data downloaded from a URL
    limited to ``chunk_size``.

    - The ``retrieve_info.size_hint``
      will be set once the iterator starts and can be used for progress display.
    - The iterator will start with an empty block, so the size can be known
      before time is spent downloading data.
    """
    from urllib.error import ContentTooShortError
    from urllib.request import urlopen

    request = urllib.request.Request(
        url,
        data=data,
        headers=headers,
    )

    with (
            urlopen(request, timeout=timeout_in_seconds) if (timeout_in_seconds > 0.0) else
            urlopen(request)
    ) as fp:
        response_headers = fp.info()

        size = -1
        read = 0
        if "content-length" in response_headers:
            size = int(response_headers["Content-Length"])

        retrieve_info.size_hint = size

        # Yield an empty block so progress display may start.
        yield b""

        if timeout_in_seconds <= 0.0:
            while True:
                block = fp.read(chunk_size)
                if not block:
                    break
                read += len(block)
                yield block
        else:
            while True:
                block = read_with_timeout(fp, chunk_size, timeout_in_seconds=timeout_in_seconds)
                if not block:
                    break
                read += len(block)
                yield block

    if size >= 0 and read < size:
        raise ContentTooShortError(
            "retrieval incomplete: got only {:d} out of {:d} bytes".format(read, size),
            response_headers,
        )


# See `url_retrieve_to_data_iter` doc-string.
def url_retrieve_to_filepath_iter(
        url: str,
        filepath: str,
        *,
        headers: dict[str, str],
        data: Any | None = None,
        chunk_size: int,
        timeout_in_seconds: float,
        retrieve_info: DataRetrieveInfo,
) -> Iterator[int]:
    # Handle temporary file setup.
    with open(filepath, 'wb') as fh_output:
        for block in url_retrieve_to_data_iter(
                url,
                headers=headers,
                data=data,
                chunk_size=chunk_size,
                timeout_in_seconds=timeout_in_seconds,
                retrieve_info=retrieve_info,
        ):
            fh_output.write(block)
            yield len(block)


# See `url_retrieve_to_data_iter` doc-string.
def filepath_retrieve_to_filepath_iter(
        filepath_src: str,
        filepath: str,
        *,
        chunk_size: int,
        timeout_in_seconds: float,
        retrieve_info: DataRetrieveInfo,
) -> Iterator[int]:
    # TODO: `timeout_in_seconds`.
    # Handle temporary file setup.
    _ = timeout_in_seconds
    with open(filepath_src, 'rb') as fh_input:
        retrieve_info.size_hint = os.fstat(fh_input.fileno()).st_size
        yield 0
        with open(filepath, 'wb') as fh_output:
            while (block := fh_input.read(chunk_size)):
                fh_output.write(block)
                yield len(block)


def url_retrieve_to_data_iter_or_filesystem(
        url: str,
        headers: dict[str, str],
        *,
        chunk_size: int,
        timeout_in_seconds: float,
        retrieve_info: DataRetrieveInfo,
) -> Iterator[bytes]:
    if url_is_filesystem(url):
        with open(path_from_url(url), "rb") as fh_source:
            retrieve_info.size_hint = os.fstat(fh_source.fileno()).st_size
            yield b""
            while (block := fh_source.read(chunk_size)):
                yield block
    else:
        yield from url_retrieve_to_data_iter(
            url,
            headers=headers,
            chunk_size=chunk_size,
            timeout_in_seconds=timeout_in_seconds,
            retrieve_info=retrieve_info,
        )


# See `url_retrieve_to_data_iter` doc-string.
def url_retrieve_to_filepath_iter_or_filesystem(
        url: str,
        filepath: str,
        *,
        headers: dict[str, str],
        chunk_size: int,
        timeout_in_seconds: float,
        retrieve_info: DataRetrieveInfo,
) -> Iterator[int]:
    """
    Callers should catch: ``(Exception, KeyboardInterrupt)`` and convert them to message using:
    ``url_retrieve_exception_as_message``.
    """
    if url_is_filesystem(url):
        yield from filepath_retrieve_to_filepath_iter(
            path_from_url(url),
            filepath,
            chunk_size=chunk_size,
            timeout_in_seconds=timeout_in_seconds,
            retrieve_info=retrieve_info,
        )
    else:
        yield from url_retrieve_to_filepath_iter(
            url,
            filepath,
            headers=headers,
            chunk_size=chunk_size,
            timeout_in_seconds=timeout_in_seconds,
            retrieve_info=retrieve_info,
        )


def url_retrieve_exception_is_connectivity(
        ex: Exception | KeyboardInterrupt,
) -> bool:
    if isinstance(ex, FileNotFoundError):
        return True
    if isinstance(ex, TimeoutError):
        return True
    # Covers `HTTPError` too.
    if isinstance(ex, urllib.error.URLError):
        return True

    return False


def url_retrieve_exception_as_message(
        ex: Exception | KeyboardInterrupt,
        *,
        prefix: str,
        url: str,
) -> str:
    """
    Provides more user friendly messages when reading from a URL fails.
    """
    # These exceptions may occur when reading from the file-system or a URL.
    url_strip = remote_url_params_strip(url)
    if isinstance(ex, FileNotFoundError):
        return "{:s}: file-not-found ({:s}) reading {!r}!".format(prefix, str(ex), url_strip)
    if isinstance(ex, TimeoutError):
        return "{:s}: timeout ({:s}) reading {!r}!".format(prefix, str(ex), url_strip)
    if isinstance(ex, urllib.error.URLError):
        if isinstance(ex, urllib.error.HTTPError):
            if ex.code == 403:
                return "{:s}: HTTP error (403) access token may be incorrect, reading {!r}!".format(prefix, url_strip)
            return "{:s}: HTTP error ({:s}) reading {!r}!".format(prefix, str(ex), url_strip)
        return "{:s}: URL error ({:s}) reading {!r}!".format(prefix, str(ex), url_strip)

    return "{:s}: unexpected error ({:s}) reading {!r}!".format(prefix, str(ex), url_strip)


def pkg_idname_is_valid_or_error(pkg_idname: str) -> str | None:
    if not pkg_idname.isidentifier():
        return "Not a valid Python identifier"
    if "__" in pkg_idname:
        return "Only single separators are supported"
    if pkg_idname.startswith("_"):
        return "Names must not start with a \"_\""
    if pkg_idname.endswith("_"):
        return "Names must not end with a \"_\""
    return None


def pkg_manifest_validate_terse_description_or_error(value: str) -> str | None:
    # Could be an argument.
    length_limit = TERSE_DESCRIPTION_MAX_LENGTH
    if (length_limit != -1) and (len(value) > length_limit):
        return "a value no longer than {:d} characters expected, found {:d}".format(length_limit, len(value))

    if (error := pkg_manifest_validate_field_any_non_empty_string_stripped_no_control_chars(value, True)) is not None:
        return error

    # As we don't have a reliable (unicode aware) punctuation check, just check the last character is alpha/numeric.
    if value[-1].isalnum():
        pass  # OK.
    elif value[-1] in {")", "]", "}"}:
        pass  # Allow closing brackets (sometimes used to mention formats).
    else:
        return "alphanumeric suffix expected, the string must not end with punctuation"
    return None


# -----------------------------------------------------------------------------
# Manifest Validation (Tags)


def pkg_manifest_tags_load_valid_map_from_python(
        valid_tags_filepath: str,
) -> str | dict[str, set[str]]:
    try:
        data = execfile(valid_tags_filepath)
    except Exception as ex:
        return "Python evaluation error ({:s})".format(str(ex))

    result = {}
    for key, key_extension_type in (("addons", "add-on"), ("themes", "theme")):
        if (value := data.get(key)) is None:
            return "missing key \"{:s}\"".format(key)
        if not isinstance(value, set):
            return "key \"{:s}\" must be a set, not a {:s}".format(key, str(type(value)))
        for tag in value:
            if not isinstance(tag, str):
                return "key \"{:s}\" must contain strings, found a {:s}".format(key, str(type(tag)))

        result[key_extension_type] = value

    return result


def pkg_manifest_tags_load_valid_map_from_json(
        valid_tags_filepath: str,
) -> str | dict[str, set[str]]:
    try:
        with open(valid_tags_filepath, "rb") as fh:
            data = json.load(fh)
    except Exception as ex:
        return "JSON evaluation error ({:s})".format(str(ex))

    if not isinstance(data, dict):
        return "JSON must contain a dict not a {:s}".format(str(type(data)))

    result = {}
    for key in ("add-on", "theme"):
        if (value := data.get(key)) is None:
            return "missing key \"{:s}\"".format(key)
        if not isinstance(value, list):
            return "key \"{:s}\" must be a list, not a {:s}".format(key, str(type(value)))
        for tag in value:
            if not isinstance(tag, str):
                return "key \"{:s}\" must contain strings, found a {:s}".format(key, str(type(tag)))

        result[key] = set(value)

    return result


def pkg_manifest_tags_load_valid_map(
        valid_tags_filepath: str,
) -> str | dict[str, set[str]]:
    # Allow Python data (Blender stores this internally).
    if valid_tags_filepath.endswith(".py"):
        return pkg_manifest_tags_load_valid_map_from_python(valid_tags_filepath)
    return pkg_manifest_tags_load_valid_map_from_json(valid_tags_filepath)


def pkg_manifest_tags_valid_or_error(
        valid_tags_data: dict[str, Any],
        manifest_type: str,
        manifest_tags: list[str],
) -> str | None:
    valid_tags = valid_tags_data[manifest_type]
    for tag in manifest_tags:
        if tag not in valid_tags:
            return (
                "found invalid tag \"{:s}\" not found in:\n"
                "({:s})"
            ).format(tag, ", ".join(sorted(valid_tags)))
    return None


# -----------------------------------------------------------------------------
# Manifest Validation (Generic Callbacks)
#
# NOTE: regarding the `strict` argument, this was added because we may want to tighten
# guidelines without causing existing repositories to fail.
#
# Strict is used:
# - When building packages.
# - When validating packages from the command line.
#
# However manifests from severs that don't adhere to strict rules are not prevented from loading.

# pylint: disable-next=useless-return
def pkg_manifest_validate_field_nop(
        value: Any,
        strict: bool,
) -> str | None:
    _ = strict, value
    return None


def pkg_manifest_validate_field_any_non_empty_string(
    value: str,
    strict: bool,
) -> str | None:
    _ = strict
    if not value.strip():
        return "A non-empty string expected"
    return None


def pkg_manifest_validate_field_any_non_empty_string_stripped_no_control_chars(
        value: str,
        strict: bool,
) -> str | None:
    _ = strict
    value_strip = value.strip()
    if not value_strip:
        return "a non-empty string expected"
    if value != value_strip:
        return "text without leading/trailing white space expected"
    for _ in RE_CONTROL_CHARS.finditer(value):
        return "text without any control characters expected"
    return None


def pkg_manifest_validate_field_any_list_of_non_empty_strings(value: list[Any], strict: bool) -> str | None:
    _ = strict
    for i, tag in enumerate(value):
        if not isinstance(tag, str):
            return "at index {:d} must be a string not a {:s}".format(i, str(type(tag)))
        if not tag.strip():
            return "at index {:d} must be a non-empty string".format(i)
    return None


def pkg_manifest_validate_field_any_non_empty_list_of_non_empty_strings(
        value: list[Any],
        strict: bool,
) -> str | None:
    if not value:
        return "list may not be empty"

    return pkg_manifest_validate_field_any_list_of_non_empty_strings(value, strict)


def pkg_manifest_validate_field_any_version(
        value: str,
        strict: bool,
) -> str | None:
    _ = strict
    if not RE_MANIFEST_SEMVER.match(value):
        return "to be a semantic-version, found {!r}".format(value)
    return None


def pkg_manifest_validate_field_any_version_primitive(
        value: str,
        strict: bool,
) -> str | None:
    _ = strict
    # Parse simple `1.2.3`, `1.2` & `1` numbers.
    for number in value.split("."):
        if not number.isdigit():
            return "must be numbers separated by single \".\" characters, found \"{:s}\"".format(value)
    return None


def pkg_manifest_validate_field_any_version_primitive_or_empty(
        value: str,
        strict: bool,
) -> str | None:
    if value:
        return pkg_manifest_validate_field_any_version_primitive(value, strict)
    return None

# -----------------------------------------------------------------------------
# Manifest Validation (Specific Callbacks)


def pkg_manifest_validate_field_idname(value: str, strict: bool) -> str | None:
    _ = strict
    return pkg_idname_is_valid_or_error(value)


def pkg_manifest_validate_field_type(value: str, strict: bool) -> str | None:
    _ = strict
    # NOTE: add "keymap" in the future.
    value_expected = {"add-on", "theme"}
    if value not in value_expected:
        return "Expected to be one of [{:s}], found {!r}".format(", ".join(value_expected), value)
    return None


def pkg_manifest_validate_field_blender_version(
        value: str,
        strict: bool,
) -> str | None:
    if (error := pkg_manifest_validate_field_any_version_primitive(value, strict)) is not None:
        return error

    if strict:
        # NOTE: Blender's extension support allows `X`, `X.X`, `X.X.X`,
        # Blender's own extensions site doesn't, so require this for validation.
        if value.count(".") != 2:
            return "expected 3 numbers separated by \".\", found \"{:s}\"".format(value)

    return None


def pkg_manifest_validate_field_blender_version_or_empty(
        value: str,
        strict: bool,
) -> str | None:
    if value:
        return pkg_manifest_validate_field_blender_version(value, strict)

    return None


def pkg_manifest_validate_field_tagline(value: str, strict: bool) -> str | None:
    if strict:
        return pkg_manifest_validate_terse_description_or_error(value)
    else:
        if (error := pkg_manifest_validate_field_any_non_empty_string(value, strict)) is not None:
            return error

    return None


def pkg_manifest_validate_field_copyright(
        value: list[str],
        strict: bool,
) -> str | None:
    if strict:
        for i, copyrignt_text in enumerate(value):
            if not isinstance(copyrignt_text, str):
                return "at index {:d} must be a string not a {:s}".format(i, str(type(copyrignt_text)))

            year, name = copyrignt_text.partition(" ")[0::2]
            year_valid = False
            if (year_split := year.partition("-"))[1]:
                if year_split[0].isdigit() and year_split[2].isdigit():
                    year_valid = True
            else:
                if year.isdigit():
                    year_valid = True

            if not year_valid:
                return "at index {:d} must be a number or two numbers separated by \"-\"".format(i)
            if not name.strip():
                return "at index {:d} name may not be empty".format(i)
        return None
    else:
        return pkg_manifest_validate_field_any_list_of_non_empty_strings(value, strict)


def pkg_manifest_validate_field_permissions(
        value: (
            # `dict[str, str]` is expected but at this point it's only guaranteed to be a dict.
            dict[Any, Any] |
            # Kept for old files.
            list[Any]
        ),
        strict: bool,
) -> str | None:

    keys_valid = {
        "files",
        "network",
        "clipboard",
        "camera",
        "microphone",
    }

    if strict:
        # A list may be passed in when not-strict.
        if not isinstance(value, dict):
            return "permissions must be a table of strings, not a {:s}".format(str(type(value)))

        for item_key, item_value in value.items():
            # Validate the key.
            if not isinstance(item_key, str):
                return "key \"{:s}\" must be a string not a {:s}".format(str(item_key), str(type(item_key)))
            if item_key not in keys_valid:
                return "value of \"{:s}\" must be a value in {!r}".format(item_key, tuple(keys_valid))

            # Validate the value.
            if not isinstance(item_value, str):
                return "value of \"{:s}\" must be a string not a {:s}".format(item_key, str(type(item_value)))

            if (error := pkg_manifest_validate_terse_description_or_error(item_value)) is not None:
                return "value of \"{:s}\": {:s}".format(item_key, error)

    else:
        if isinstance(value, dict):
            for item_key, item_value in value.items():
                if not isinstance(item_key, str):
                    return "key \"{:s}\" must be a string not a {:s}".format(str(item_key), str(type(item_key)))
                if not isinstance(item_value, str):
                    return "value of \"{:s}\" must be a string not a {:s}".format(item_key, str(type(item_value)))
        elif isinstance(value, list):
            # Historic beta convention, keep for compatibility.
            for i, item in enumerate(value):
                if not isinstance(item, str):
                    return "Expected item at index {:d} to be an int not a {:s}".format(i, str(type(item)))
        else:
            # The caller doesn't allow this.
            assert False, "internal error, disallowed type"

    return None


def pkg_manifest_validate_field_build_path_list(value: list[Any], strict: bool) -> str | None:
    _ = strict
    value_duplicate_check: set[str] = set()

    for item in value:
        if not isinstance(item, str):
            return "Expected \"paths\" to be a list of strings, found \"{:s}\"".format(str(type(item)))
        if not item:
            return "Expected \"paths\" items to be a non-empty string"
        if "\\" in item:
            return "Expected \"paths\" items to use \"/\" slashes, found: \"{:s}\"".format(item)
        if "\n" in item:
            return "Expected \"paths\" items to contain single lines, found: \"{:s}\"".format(item)
        # TODO: properly handle WIN32 absolute paths.
        if item.startswith("/"):
            return "Expected \"paths\" to be relative, found: \"{:s}\"".format(item)

        # Disallow references to `../../path` as this wont map into a the archive properly.
        # Further it may provide a security problem.
        item_native = os.path.normpath(item if os.sep == "/" else item.replace("/", "\\"))
        if item_native.startswith(".." + os.sep):
            return "Expected \"paths\" items to reference paths within a directory, found: \"{:s}\"".format(item)

        # Disallow duplicate names (when lower-case) to avoid collisions on case insensitive file-systems.
        item_native_lower = item_native.lower()
        len_prev = len(value_duplicate_check)
        value_duplicate_check.add(item_native_lower)
        if len_prev == len(value_duplicate_check):
            return "Expected \"paths\" to contain unique paths, duplicate found: \"{:s}\"".format(item)

        # Having to support this optionally ends up being reasonably complicated.
        # Simply throw an error if it's included, so it can be added at build time.
        if item_native == PKG_MANIFEST_FILENAME_TOML:
            return "Expected \"paths\" not to contain the manifest, found: \"{:s}\"".format(item)

        # NOTE: other checks could be added here, (exclude control characters for example).
        # Such cases are quite unlikely so supporting them isn't so important.

    return None


def pkg_manifest_validate_field_wheels(
        value: list[Any],
        strict: bool,
) -> str | None:
    if (error := pkg_manifest_validate_field_any_list_of_non_empty_strings(value, strict)) is not None:
        return error

    for wheel in value:
        if "\"" in wheel:
            return "wheel paths most not contain quotes, found {!r}".format(wheel)
        if "\\" in wheel:
            return "wheel paths must use forward slashes, found {!r}".format(wheel)

        if (error := pkg_manifest_validate_field_any_non_empty_string_stripped_no_control_chars(
                wheel, True,
        )) is not None:
            return "wheel paths detected: {:s}, found {!r}".format(error, wheel)

        wheel_filename = os.path.basename(wheel)
        if not wheel_filename.lower().endswith(".whl"):
            return "wheel paths must end with \".whl\", found {!r}".format(wheel)

        wheel_filename_split = wheel_filename.split("-")
        # pylint: disable-next=superfluous-parens
        if not (5 <= len(wheel_filename_split) <= 6):
            return "wheel filename must follow the spec \"{:s}\", found {!r}".format(
                WHEEL_FILENAME_SPEC,
                wheel_filename,
            )

    return None


def pkg_manifest_validate_field_archive_size(
    value: int,
    strict: bool,
) -> str | None:
    _ = strict
    if value <= 0:
        return "to be a positive integer, found {!r}".format(value)
    return None


def pkg_manifest_validate_field_archive_hash(
        value: str,
        strict: bool,
) -> str | None:
    _ = strict
    import string
    # Expect: `sha256:{HASH}`.
    # In the future we may support multiple hash types.
    value_hash_type, value_sep, x_val_hash_data = value.partition(":")
    if not value_sep:
        return "Must have a \":\" separator {!r}".format(value)
    del value_sep
    if value_hash_type == "sha256":
        if len(x_val_hash_data) != 64 or x_val_hash_data.strip(string.hexdigits):
            return "Must be 64 hex-digits, found {!r}".format(value)
    else:
        return "Must use a prefix in [\"sha256\"], found {!r}".format(value_hash_type)
    return None


# Keep in sync with `PkgManifest`.
# key, type, check_fn.
pkg_manifest_known_keys_and_types: tuple[
    tuple[
        str,
        type | tuple[type, ...],
        Callable[[Any, bool], str | None],
    ],
    ...,
] = (
    ("id", str, pkg_manifest_validate_field_idname),
    ("schema_version", str, pkg_manifest_validate_field_any_version),
    ("name", str, pkg_manifest_validate_field_any_non_empty_string_stripped_no_control_chars),
    ("tagline", str, pkg_manifest_validate_field_tagline),
    ("version", str, pkg_manifest_validate_field_any_version),
    ("type", str, pkg_manifest_validate_field_type),
    ("maintainer", str, pkg_manifest_validate_field_any_non_empty_string_stripped_no_control_chars),
    ("license", list, pkg_manifest_validate_field_any_non_empty_list_of_non_empty_strings),
    ("blender_version_min", str, pkg_manifest_validate_field_blender_version),

    # Optional.
    ("blender_version_max", str, pkg_manifest_validate_field_blender_version_or_empty),
    ("website", str, pkg_manifest_validate_field_any_non_empty_string_stripped_no_control_chars),
    ("copyright", list, pkg_manifest_validate_field_copyright),
    # Type should be `dict` eventually, some existing packages will have a list of strings instead.
    ("permissions", (dict, list), pkg_manifest_validate_field_permissions),
    ("tags", list, pkg_manifest_validate_field_any_non_empty_list_of_non_empty_strings),
    ("platforms", list, pkg_manifest_validate_field_any_non_empty_list_of_non_empty_strings),
    ("wheels", list, pkg_manifest_validate_field_wheels),
)

# Keep in sync with `PkgManifest_Archive`.
pkg_manifest_known_keys_and_types_from_repo: tuple[
    tuple[str, type, Callable[[Any, bool], str | None]],
    ...,
] = (
    ("archive_size", int, pkg_manifest_validate_field_archive_size),
    ("archive_hash", str, pkg_manifest_validate_field_archive_hash),
    ("archive_url", str, pkg_manifest_validate_field_nop),
)


# -----------------------------------------------------------------------------
# Manifest Validation

def pkg_manifest_is_valid_or_error_impl(
        data: dict[str, Any],
        *,
        from_repo: bool,
        all_errors: bool,
        strict: bool,
) -> list[str] | None:
    if not isinstance(data, dict):
        return ["Expected value to be a dict, not a {!r}".format(type(data))]

    assert len(pkg_manifest_known_keys_and_types) == len(PkgManifest._fields)
    # -1 because the manifest is an item.
    assert len(pkg_manifest_known_keys_and_types_from_repo) == len(PkgManifest_Archive._fields) - 1

    error_list = []

    value_extract: dict[str, object | None] = {}
    for known_types in (
            (pkg_manifest_known_keys_and_types, pkg_manifest_known_keys_and_types_from_repo) if from_repo else
            (pkg_manifest_known_keys_and_types, )
    ):
        for x_key, x_ty, x_check_fn in known_types:
            is_default_value = False
            x_val = data.get(x_key, ...)
            if x_val is ...:
                # pylint: disable-next=no-member, protected-access
                x_val = PkgManifest._field_defaults.get(x_key, ...)
                if from_repo:
                    if x_val is ...:
                        # pylint: disable-next=no-member, protected-access
                        x_val = PkgManifest_Archive._field_defaults.get(x_key, ...)
                if x_val is ...:
                    error_list.append("missing \"{:s}\"".format(x_key))
                    if not all_errors:
                        return error_list
                else:
                    is_default_value = True
                value_extract[x_key] = x_val
                continue

            # When the default value is None, skip all type checks.
            if not (is_default_value and x_val is None):
                if isinstance(x_val, x_ty):
                    pass
                else:
                    error_list.append("\"{:s}\" must be a {:s}, not a {:s}".format(
                        x_key, (
                            "[{:s}]".format(", ".join(x_ty_elem.__name__ for x_ty_elem in x_ty))
                            if isinstance(x_ty, tuple) else
                            x_ty.__name__
                        ),
                        type(x_val).__name__,
                    ))
                    if not all_errors:
                        return error_list
                    continue

                if (error_msg := x_check_fn(x_val, strict)) is not None:
                    error_list.append("key \"{:s}\" invalid: {:s}".format(x_key, error_msg))
                    if not all_errors:
                        return error_list
                    continue

            value_extract[x_key] = x_val

    if error_list:
        assert all_errors
        return error_list

    return None


def pkg_manifest_is_valid_or_error(
        data: dict[str, Any],
        *,
        from_repo: bool,
        strict: bool,
) -> str | None:
    error_list = pkg_manifest_is_valid_or_error_impl(
        data,
        from_repo=from_repo,
        all_errors=False,
        strict=strict,
    )
    if isinstance(error_list, list):
        return error_list[0]
    return None


def pkg_manifest_is_valid_or_error_all(
        data: dict[str, Any],
        *,
        from_repo: bool,
        strict: bool,
) -> list[str] | None:
    return pkg_manifest_is_valid_or_error_impl(
        data,
        from_repo=from_repo,
        all_errors=True,
        strict=strict,
    )


# -----------------------------------------------------------------------------
# Manifest Utilities

def pkg_manifest_dict_apply_build_generated_table(manifest_dict: dict[str, Any]) -> None:
    # Swap in values from `[build.generated]` if it exists:
    if (build_generated := manifest_dict.get("build", {}).get("generated")) is None:
        return

    if (platforms := build_generated.get("platforms")) is not None:
        manifest_dict["platforms"] = platforms

    if (wheels := build_generated.get("wheels")) is not None:
        manifest_dict["wheels"] = wheels


# -----------------------------------------------------------------------------
# Standalone Utilities

platform_system_replace = {
    "darwin": "macos",
}

platform_machine_replace = {
    "x86_64": "x64",
    "amd64": "x64",
    # Used on Linux for ARM64 (APPLE already uses `arm64`).
    "aarch64": "arm64",
    "aarch32": "arm32",
}

# Use when converting a Python `.whl` platform to a Blender `platform_from_this_system` platform.
platform_system_replace_for_wheels = {
    "macosx": "macos",
    "manylinux": "linux",
    "musllinux": "linux",
    "win": "windows",
}


def platform_from_this_system() -> str:
    import platform
    system = platform.system().lower()
    machine = platform.machine().lower()
    return "{:s}-{:s}".format(
        platform_system_replace.get(system, system),
        platform_machine_replace.get(machine, machine),
    )


def blender_platforms_from_wheel_platform(wheel_platform: str) -> list[str]:
    """
    Convert a wheel to a Blender compatible platform: e.g.
    - ``linux_x86_64``              -> ``linux-x64``.
    - ``manylinux_2_28_x86_64``     -> ``linux-x64``.
    - ``manylinux2014_aarch64``     -> ``linux-arm64``.
    - ``win_amd64``                 -> ``windows-x64``.
    - ``macosx_11_0_arm64``         -> ``macos-arm64``.
    - ``manylinux2014_x86_64``      -> ``linux-x64``.
    - ``macosx_10_9_universal2``    -> ``macos-x64``, ``macos-arm64``.
    """

    i = wheel_platform.find("_")
    if i == -1:
        # WARNING: this should never or almost never happen.
        # Return the result as we don't have a better alternative.
        return [wheel_platform]

    head = wheel_platform[:i]
    tail = wheel_platform[i + 1:]

    for wheel_src, blender_dst in platform_system_replace_for_wheels.items():
        if head == wheel_src:
            head = blender_dst
            break
        # Account for:
        # `manylinux2014` -> `linux`.
        # `win32` -> `windows`.
        if head.startswith(wheel_src) and head[len(wheel_src):].isdigit():
            head = blender_dst
            break

    for wheel_src, blender_dst in platform_machine_replace.items():
        if (tail == wheel_src) or (tail.endswith("_" + wheel_src)):
            # NOTE: in some cases this skips GLIBC versions.
            tail = blender_dst
            break
    else:
        # Avoid GLIBC or MACOS versions being included in the `machine` value.
        # This works as long as all known machine values are added to `platform_machine_replace`
        # (only `x86_64` at the moment).
        tail = tail.rpartition("_")[2]

    if (head == "macos") and (tail == "universal2"):
        tails = ["x64", "arm64"]
    else:
        tails = [tail]

    return ["{:s}-{:s}".format(head, tail) for tail in tails]


def blender_platform_compatible_with_wheel_platform(platform: str, wheel_platform: str) -> bool:
    assert platform
    if wheel_platform == "any":
        return True
    platforms_blender = blender_platforms_from_wheel_platform(wheel_platform)

    return platform in platforms_blender


def blender_platform_compatible_with_wheel_platform_from_filepath(platform: str, wheel_filepath: str) -> bool:
    wheel_filename = os.path.splitext(os.path.basename(wheel_filepath))[0]

    wheel_filename_split = wheel_filename.split("-")
    # This should be unreachable because the manifest has been validated, add assert.
    assert len(wheel_filename_split) >= 5, "Internal error, manifest validation disallows this"

    wheel_platform = wheel_filename_split[-1]

    return blender_platform_compatible_with_wheel_platform(platform, wheel_platform)


def python_versions_from_wheel_python_tag(python_tag: str) -> set[tuple[int] | tuple[int, int]] | str:
    """
    Return Python versions from a wheels ``python_tag``.
    """
    # The version may be:
    # `cp312` for CPython 3.12
    # `py2.py3` for both Python 2 & 3.

    # Based on the documentation as of 2024 and wheels used by existing extensions,
    # these are the only valid prefix values.
    version_prefix_known = {"py", "cp"}

    versions: set[tuple[int] | tuple[int, int]] = set()

    for tag in python_tag.split("."):
        m = RE_PYTHON_WHEEL_VERSION_TAG.match(tag)
        if m is None:
            return "wheel filename version could not be extracted from: \"{:s}\"".format(tag)

        version_prefix = m.group(1).lower()
        version_number = m.group(2)

        if version_prefix in version_prefix_known:
            if len(version_number) > 1:
                # Convert: `"311"` to `(3, 11)`.
                version: tuple[int, int] | tuple[int] = (int(version_number[:1]), int(version_number[1:]))
            else:
                # Common for "py3".
                version = (int(version_number), )
            versions.add(version)
        else:
            return (
                "wheel filename version prefix failed to be extracted "
                "found \"{:s}\" int \"{:s}\", expected a value in ({:s})"
            ).format(
                version_prefix,
                python_tag,
                ", ".join(sorted(version_prefix_known)),
            )

    return versions


def python_versions_from_wheel_abi_tag(
        abi_tag: str,
        *,
        stable_only: bool,
) -> set[tuple[int] | tuple[int, int]] | str:
    versions: set[tuple[int] | tuple[int, int]] = set()

    # Not yet needed, add as needed.
    if stable_only:
        for tag in abi_tag.split("."):
            if tag.startswith("abi") and tag[3:].isdigit():
                versions.add((int(tag[3:]),))
    else:
        # Not a problem to support this, currently it's not needed.
        raise NotImplementedError

    return versions


def python_versions_from_wheel(wheel_filename: str) -> set[tuple[int] | tuple[int, int]] | str:
    """
    Extract a set of Python versions from a list of wheels or return an error string.
    """
    wheel_filename_split = wheel_filename.split("-")

    # pylint: disable-next=superfluous-parens
    if not (5 <= len(wheel_filename_split) <= 6):
        return "wheel filename must follow the spec \"{:s}\", found {!r}".format(WHEEL_FILENAME_SPEC, wheel_filename)

    python_tag = wheel_filename_split[-3]
    abi_tag = wheel_filename_split[-2]

    # NOTE(@ideasman42): when the ABI is set, simply return the major version,
    # This is needed because older version of CPython (3.6) for example are compatible with newer versions of CPython,
    # but returning the old version causes it not to register as being compatible.
    # So return the ABI version to allow any version of CPython 3.x.
    #
    # There is a logical problem here: which is that a future wheel from CPython *should* be detected
    # as incompatible but wont be. To properly support this, extensions repository data would need to store
    # either store a separate ABI version or a `>=` version. In practice this isn't as bad as it sounds
    # because those packages typically won't support old versions of Blender known to use older Python versions,
    # although it will incorrectly exclude old versions of Blender which were built against newer versions of
    # CPython than the version used by official builds.
    python_versions_from_abi = python_versions_from_wheel_abi_tag(abi_tag, stable_only=True)
    if python_versions_from_abi:
        return python_versions_from_abi

    return python_versions_from_wheel_python_tag(python_tag)


def python_versions_from_wheels(wheel_files: Sequence[str]) -> set[tuple[int] | tuple[int, int]] | str:
    if not wheel_files:
        assert False, "unreachable"
        return ()

    version_major_only: set[tuple[int]] = set()
    version_major_minor: set[tuple[int, int]] = set()
    for filepath in wheel_files:
        if isinstance(result := python_versions_from_wheel(os.path.basename(filepath)), str):
            return result
        # Check for an empty set - no version info.
        if not result:
            continue

        for v in result:
            # Exclude support for historic Python versions.
            # While including this info is technically correct, it's *never* useful to include this info.
            # So listing this information in every server JSON listing is redundant.
            if v[0] <= 2:
                continue

            if len(v) == 1:
                version_major_only.add(v)
            else:
                version_major_minor.add(v)

    # Clean the versions, exclude `(3,)` when `(3, 11)` is present.
    for v in version_major_minor:
        version_major_only.discard((v[0],))

    return version_major_only | version_major_minor


def paths_filter_wheels_by_platform(
        wheels: list[str],
        platform: str,
) -> list[str]:
    """
    All paths are wheels with filenames that follow the wheel spec.
    Return wheels which are compatible with the ``platform``.
    """
    wheels_result: list[str] = []

    for wheel_filepath in wheels:
        if blender_platform_compatible_with_wheel_platform_from_filepath(platform, wheel_filepath):
            wheels_result.append(wheel_filepath)

    return wheels_result


def build_paths_filter_wheels_by_platform(
        build_paths: list[tuple[str, str]],
        platform: str,
) -> list[tuple[str, str]]:
    """
    All paths are wheels with filenames that follow the wheel spec.
    Return wheels which are compatible with the ``platform``.
    """
    build_paths_for_platform: list[tuple[str, str]] = []

    for item in build_paths:
        if blender_platform_compatible_with_wheel_platform_from_filepath(platform, item[1]):
            build_paths_for_platform.append(item)

    return build_paths_for_platform


def build_paths_filter_by_platform(
        build_paths: list[tuple[str, str]],
        wheel_range: tuple[int, int],
        platforms: tuple[str, ...],
) -> Iterator[tuple[list[tuple[str, str]], str]]:
    if not platforms:
        yield (build_paths, "")
        return

    if wheel_range[0] == wheel_range[1]:
        # Not an error, but there is no reason to split the packages in this case,
        # caller may warn about this although it's not an error.
        for platform in platforms:
            yield (build_paths, platform)
        return

    build_paths_head = build_paths[:wheel_range[0]]
    build_paths_wheels = build_paths[wheel_range[0]:wheel_range[1]]
    build_paths_tail = build_paths[wheel_range[1]:]

    for platform in platforms:
        wheels_for_platform = build_paths_filter_wheels_by_platform(build_paths_wheels, platform)
        yield (
            [
                *build_paths_head,
                *wheels_for_platform,
                *build_paths_tail,
            ],
            platform,
        )


def repository_filter_skip(
        item: dict[str, Any],
        *,
        filter_blender_version: tuple[int, int, int],
        filter_platform: str,
        filter_python_version: tuple[int, int, int],
        # When `skip_message_fn` is set, returning true must call the `skip_message_fn` function.
        skip_message_fn: Callable[[str], None] | None,
        error_fn: Callable[[Exception], None],
) -> bool:
    """
    This function takes an ``item`` which represents un-validated extension meta-data.
    Return True when the extension should be excluded.

    The meta-data is a subset of the ``blender_manifest.toml`` which is extracted
    into the ``index.json`` hosted by a remote server.

    Filtering will exclude extensions when:

    - They're incompatible with Blender, Python or the platform defined by the ``filter_*`` arguments.
      ``skip_message_fn`` callback will run with the cause of the incompatibility.
    - The meta-data is malformed, it doesn't confirm to ``blender_manifest.toml`` data-types.
      ``error_fn`` callback will run with the cause of the error.

    This is used so Blender's extensions listing only shows compatible extensions as well as
    reporting errors if the user attempts to install an extension which isn't compatible with their system.
    """

    if (platforms := item.get("platforms")) is not None:
        if not isinstance(platforms, list):
            # Possibly noisy, but this should *not* be happening on a regular basis.
            error_fn(TypeError("platforms is not a list, found a: {:s}".format(str(type(platforms)))))
        elif platforms and (filter_platform not in platforms):
            if skip_message_fn is not None:
                skip_message_fn("This platform ({:s}) isn't one of ({:s})".format(
                    filter_platform,
                    ", ".join(platforms),
                ))
            return True

    if filter_python_version != (0, 0, 0):
        if (python_versions := item.get("python_versions")) is not None:
            if not isinstance(python_versions, list):
                # Possibly noisy, but this should *not* be happening on a regular basis.
                error_fn(TypeError("python_versions is not a list, found a: {:s}".format(str(type(python_versions)))))
            elif python_versions:
                ok = True
                python_versions_as_set: set[str] = set()
                for v in python_versions:
                    if not isinstance(v, str):
                        error_fn(TypeError((
                            "python_versions is not a list of strings, "
                            "found an item of type: {:s}"
                        ).format(str(type(v)))))
                        ok = False
                        break
                    # The full Python version isn't explicitly disallowed,
                    # ignore trailing values only check major/minor.
                    if v.count(".") > 1:
                        v = ".".join(v.split(".")[:2])
                    python_versions_as_set.add(v)

                if ok:
                    # There is no need to do any complex extraction and comparison.
                    # Simply check if the `{major}.{minor}` or `{major}` exists in the set.
                    python_versions_as_set = set(python_versions)
                    filter_python_version_major_minor = "{:d}.{:d}".format(*filter_python_version[:2])
                    filter_python_version_major_only = "{:d}".format(filter_python_version[0])
                    if not (
                            filter_python_version_major_minor in python_versions_as_set or
                            filter_python_version_major_only in python_versions_as_set
                    ):
                        if skip_message_fn is not None:
                            skip_message_fn("This Python version ({:s}) isn't compatible with ({:s})".format(
                                filter_python_version_major_minor,
                                ", ".join(python_versions),
                            ))

                        return True
                    del filter_python_version_major_minor, filter_python_version_major_only, python_versions_as_set

    if filter_blender_version != (0, 0, 0):
        version_min_str = item.get("blender_version_min")
        version_max_str = item.get("blender_version_max")

        if not (isinstance(version_min_str, str) or version_min_str is None):
            error_fn(TypeError("blender_version_min expected a string, found: {:s}".format(str(type(version_min_str)))))
            version_min_str = None
        if not (isinstance(version_max_str, str) or version_max_str is None):
            error_fn(TypeError("blender_version_max expected a string, found: {:s}".format(str(type(version_max_str)))))
            version_max_str = None

        if version_min_str is None:
            version_min = None
        elif isinstance(version_min := blender_version_parse_any_or_error(version_min_str), str):
            error_fn(TypeError("blender_version_min invalid format: {:s}".format(version_min)))
            version_min = None

        if version_max_str is None:
            version_max = None
        elif isinstance(version_max := blender_version_parse_any_or_error(version_max_str), str):
            error_fn(TypeError("blender_version_max invalid format: {:s}".format(version_max)))
            version_max = None

        del version_min_str, version_max_str

        assert (isinstance(version_min, tuple) or version_min is None)
        assert (isinstance(version_max, tuple) or version_max is None)

        if (version_min is not None) and (filter_blender_version < version_min):
            # Blender is older than the packages minimum supported version.
            if skip_message_fn is not None:
                skip_message_fn("This Blender version ({:s}) doesn't meet the minimum supported version ({:s})".format(
                    ".".join(str(x) for x in filter_blender_version),
                    ".".join(str(x) for x in version_min),
                ))
            return True
        if (version_max is not None) and (filter_blender_version >= version_max):
            # Blender is newer or equal to the maximum value.
            if skip_message_fn is not None:
                skip_message_fn("This Blender version ({:s}) must be less than the maximum version ({:s})".format(
                    ".".join(str(x) for x in filter_blender_version),
                    ".".join(str(x) for x in version_max),
                ))
            return True

    return False


def generic_version_triple_parse_or_error(version: str, identifier: str) -> tuple[int, int, int] | str:
    try:
        version_tuple: tuple[int, ...] = tuple(int(x) for x in version.split("."))
    except Exception as ex:
        return "unable to parse {:s} version: {:s}, {:s}".format(identifier, version, str(ex))

    if not version_tuple:
        return "unable to parse empty {:s} version: {:s}".format(identifier, version)

    # `mypy` can't detect that this is guaranteed to be 3 items.
    return (
        version_tuple if (len(version_tuple) == 3) else
        (*version_tuple, 0, 0)[:3]  # type: ignore
    )


def blender_version_parse_or_error(version: str) -> tuple[int, int, int] | str:
    return generic_version_triple_parse_or_error(version, "Blender")


def python_version_parse_or_error(version: str) -> tuple[int, int, int] | str:
    return generic_version_triple_parse_or_error(version, "Python")


def blender_version_parse_any_or_error(version: Any) -> tuple[int, int, int] | str:
    if not isinstance(version, str):
        return "blender version should be a string, found a: {:s}".format(str(type(version)))

    result = blender_version_parse_or_error(version)
    assert isinstance(result, (tuple, str))
    return result


def url_request_headers_create(*, accept_json: bool, user_agent: str, access_token: str) -> dict[str, str]:
    headers = {}
    if accept_json:
        # Default for JSON requests this allows top-level URL's to be used.
        headers["Accept"] = "application/json"

    if user_agent:
        # Typically: `Blender/4.2.0 (Linux x84_64; cycle=alpha)`.
        headers["User-Agent"] = user_agent

    if access_token:
        headers["Authorization"] = "Bearer {:s}".format(access_token)

    return headers


def repo_json_is_valid_or_error(filepath: str) -> str | None:
    if not os.path.exists(filepath):
        return "File missing: " + filepath

    try:
        with open(filepath, "r", encoding="utf-8") as fh:
            result = json.load(fh)
    except Exception as ex:
        return str(ex)

    if not isinstance(result, dict):
        return "Expected a dictionary, not a {!r}".format(type(result))

    if (value := result.get("version")) is None:
        return "Expected a \"version\" key which was not found"
    if not isinstance(value, str):
        return "Expected \"version\" value to be a version string"

    if (value := result.get("blocklist")) is not None:
        if not isinstance(value, list):
            return "Expected \"blocklist\" to be a list, not a {:s}".format(str(type(value)))
        for item in value:
            if isinstance(item, dict):
                continue
            return "Expected \"blocklist\" to be a list of dictionaries, found {:s}".format(str(type(item)))

    if (value := result.get("data")) is None:
        return "Expected a \"data\" key which was not found"
    if not isinstance(value, list):
        return "Expected \"data\" value to be a list"

    for i, item in enumerate(value):

        if (pkg_idname := item.get("id")) is None:
            return "Expected item at index {:d} to have an \"id\"".format(i)

        if not isinstance(pkg_idname, str):
            return "Expected item at index {:d} to have a string id, not a {!r}".format(i, type(pkg_idname))

        if (error_msg := pkg_idname_is_valid_or_error(pkg_idname)) is not None:
            return "Expected key at index {:d} to be an identifier, \"{:s}\" failed: {:s}".format(
                i, pkg_idname, error_msg,
            )

        if (error_msg := pkg_manifest_is_valid_or_error(item, from_repo=True, strict=False)) is not None:
            return "Error at index {:d}: {:s}".format(i, error_msg)

    return None


def pkg_manifest_toml_is_valid_or_error(filepath: str, strict: bool) -> tuple[str | None, dict[str, Any]]:
    if not os.path.exists(filepath):
        return "File missing: " + filepath, {}

    try:
        with open(filepath, "rb") as fh:
            result = tomllib.load(fh)
    except Exception as ex:
        return str(ex), {}

    error = pkg_manifest_is_valid_or_error(result, from_repo=False, strict=strict)
    if error is not None:
        return error, {}
    return None, result


def pkg_manifest_detect_duplicates(
        pkg_items: list[tuple[
            PkgManifest,
            str,
            list[tuple[int] | tuple[int, int]],
        ]],
) -> str | None:
    """
    When a repository includes multiple packages with the same ID, ensure they don't conflict.

    Ensure packages have non-overlapping:
    - Platforms.
    - Blender versions.
    - Python versions.

    Return an error if they do, otherwise None.
    """

    # Dummy ranges for the purpose of valid comparisons.
    dummy_verion_min = 0, 0, 0
    dummy_verion_max = 1000, 0, 0

    dummy_platform = ""
    dummy_python_version = (0,)

    def parse_version_or_default(version: str | None, default: tuple[int, int, int]) -> tuple[int, int, int]:
        if version is None:
            return default
        if isinstance(version_parsed := blender_version_parse_or_error(version), str):
            # NOTE: any error here will have already been handled.
            assert False, "unreachable"
            return default
        return version_parsed

    def version_range_as_str(version_min: tuple[int, int, int], version_max: tuple[int, int, int]) -> str:
        dummy_min = version_min == dummy_verion_min
        dummy_max = version_max == dummy_verion_max
        if dummy_min and dummy_max:
            return "[undefined]"
        version_min_str = "..." if dummy_min else "{:d}.{:d}.{:d}".format(*version_min)
        version_max_str = "..." if dummy_max else "{:d}.{:d}.{:d}".format(*version_max)
        return "[{:s} -> {:s}]".format(version_min_str, version_max_str)

    def ordered_int_pair(a: int, b: int) -> tuple[int, int]:
        assert a != b
        if a > b:
            return b, a
        return a, b

    class PkgManifest_DupeInfo:
        __slots__ = (
            "manifest",
            "filename",
            "python_versions",
            "index",

            # Version range (min, max) or defaults.
            "blender_version_range",

            # Expand values so duplicates can be detected by comparing literal overlaps.
            "expanded_platforms",
            "expanded_python_versions",
        )

        def __init__(
                self,
                *,
                manifest: PkgManifest,
                filename: str,
                python_versions: list[tuple[int] | tuple[int, int]],
                index: int,
        ):
            self.manifest = manifest
            self.filename = filename
            self.python_versions: list[tuple[int] | tuple[int, int]] = python_versions
            self.index = index

            # NOTE: this assignment could be deferred as it's only needed in the case potential duplicates are found.
            self.blender_version_range = (
                parse_version_or_default(manifest.blender_version_min, dummy_verion_min),
                parse_version_or_default(manifest.blender_version_max, dummy_verion_max),
            )

            self.expanded_platforms: list[str] = []
            self.expanded_python_versions: list[tuple[int] | tuple[int, int]] = []

    pkg_items_dup = [
        PkgManifest_DupeInfo(
            manifest=manifest,
            filename=filename,
            python_versions=python_versions,
            index=i,
        ) for i, (manifest, filename, python_versions) in enumerate(pkg_items)
    ]

    # Store all configurations.
    platforms_all = set()
    python_versions_all = set()

    for item_dup in pkg_items_dup:
        item = item_dup.manifest
        if item.platforms:
            platforms_all.update(item.platforms)

        if item_dup.python_versions:
            python_versions_all.update(item_dup.python_versions)
        del item

    # Expand values.
    for item_dup in pkg_items_dup:
        item = item_dup.manifest
        if platforms_all:
            item_dup.expanded_platforms[:] = item.platforms or platforms_all
        else:
            item_dup.expanded_platforms[:] = [dummy_platform]

        if python_versions_all:
            item_dup.expanded_python_versions[:] = item_dup.python_versions or python_versions_all
        else:
            item_dup.expanded_python_versions[:] = [dummy_python_version]
        del item

    # Expand Python "major only" versions.
    # Some wheels define "py3" only, this will be something we have to deal with
    # when Python moves to Python 4.
    # It's important that Python 3.11 detects as conflicting with Python 3.
    python_versions_all_map_major_to_full: dict[int, list[tuple[int, int]]] = {}
    for python_version in python_versions_all:
        if len(python_version) != 2:
            continue
        python_version_major = python_version[0]
        if (python_version_major,) not in python_versions_all:
            continue

        if (python_versions := python_versions_all_map_major_to_full.get(python_version_major)) is None:
            python_versions = python_versions_all_map_major_to_full[python_version_major] = []
        python_versions.append(python_version)
        del python_versions

    for item_dup in pkg_items_dup:
        for python_version in item_dup.python_versions:
            if len(python_version) != 1:
                continue
            python_version_major = python_version[0]

            expanded_python_versions_set = set(item_dup.expanded_python_versions)
            if (python_versions_full := python_versions_all_map_major_to_full.get(python_version_major)) is not None:
                # Expand major to major-minor versions.
                item_dup.expanded_python_versions.extend([
                    v for v in python_versions_full
                    if v not in expanded_python_versions_set
                ])
            del python_versions_full

    # This can be expanded with additional values as needed.
    # We could in principle have ABI flags (debug/release) for example
    PkgCfgKey = tuple[
        # Platform.
        str,
        # Python Version.
        tuple[int] | tuple[int, int],
    ]

    pkg_items_dup_per_cfg: dict[PkgCfgKey, list[PkgManifest_DupeInfo]] = {}

    for item_dup in pkg_items_dup:
        for platform in item_dup.expanded_platforms:
            for python_version in item_dup.expanded_python_versions:
                key = (platform, python_version)
                if (pkg_items_cfg := pkg_items_dup_per_cfg.get(key)) is None:
                    pkg_items_cfg = pkg_items_dup_per_cfg[key] = []
                pkg_items_cfg.append(item_dup)
                del pkg_items_cfg

    # Don't report duplicates more than once.
    duplicate_indices: set[tuple[int, int]] = set()

    # Packages have been split by configuration, now detect version overlap.
    duplicates_found = []

    # NOTE: we might want to include the `key` in the message,
    # after all, it's useful to know if the conflict occurs between specific platforms or Python versions.

    for pkg_items_cfg in pkg_items_dup_per_cfg.values():
        # Must never be empty.
        assert pkg_items_cfg
        if len(pkg_items_cfg) == 1:
            continue

        # Sort by the version range so overlaps can be detected between adjacent members.
        pkg_items_cfg.sort(key=lambda item_dup: item_dup.blender_version_range)

        item_prev = pkg_items_cfg[0]
        for i in range(1, len(pkg_items_cfg)):
            item_curr = pkg_items_cfg[i]

            # Previous maximum is less than or equal to the current minimum, no overlap.
            if item_prev.blender_version_range[1] > item_curr.blender_version_range[0]:
                # Don't report multiple times.
                dup_key = ordered_int_pair(item_prev.index, item_curr.index)
                if dup_key not in duplicate_indices:
                    duplicate_indices.add(dup_key)
                    duplicates_found.append("{:s}={:s} & {:s}={:s}".format(
                        item_prev.filename, version_range_as_str(*item_prev.blender_version_range),
                        item_curr.filename, version_range_as_str(*item_curr.blender_version_range),
                    ))
            item_prev = item_curr

    if duplicates_found:
        return "{:d} duplicate(s) found, conflicting blender versions {:s}".format(
            len(duplicates_found),
            ", ".join(duplicates_found),
        )

    # No collisions found.
    return None


def toml_from_bytes_or_error(data: bytes) -> dict[str, Any] | str:
    try:
        result = tomllib.loads(data.decode('utf-8'))
        assert isinstance(result, dict)
        return result
    except Exception as ex:
        return str(ex)


def toml_from_filepath_or_error(filepath: str) -> dict[str, Any] | str:
    try:
        with open(filepath, "rb") as fh:
            data = fh.read()
        result = toml_from_bytes_or_error(data)
        return result
    except Exception as ex:
        return str(ex)


def repo_local_private_dir(*, local_dir: str) -> str:
    """
    Ensure the repos hidden directory exists.
    """
    return os.path.join(local_dir, REPO_LOCAL_PRIVATE_DIR)


def repo_local_private_dir_ensure(
        *,
        local_dir: str,
        error_fn: Callable[[Exception], None],
) -> str | None:
    """
    Ensure the repos hidden directory exists.
    """
    local_private_dir = repo_local_private_dir(local_dir=local_dir)
    if not os.path.isdir(local_private_dir):
        # Unlikely but possible `local_dir` is missing.
        try:
            os.makedirs(local_private_dir)
        except Exception as ex:
            error_fn(ex)
            return None
    return local_private_dir


def repo_local_private_dir_ensure_with_subdir(
        *,
        local_dir: str,
        subdir: str,
        error_fn: Callable[[Exception], None],
) -> str | None:
    """
    Return a local directory used to cache package downloads.
    """
    if (local_private_dir := repo_local_private_dir_ensure(
            local_dir=local_dir,
            error_fn=error_fn,
    )) is None:
        return None

    local_private_subdir = os.path.join(local_private_dir, subdir)
    if not os.path.isdir(local_private_subdir):
        # Unlikely but possible `local_dir` is missing.
        try:
            os.makedirs(local_private_subdir)
        except Exception as ex:
            error_fn(ex)
            return None
    return local_private_subdir


def repo_sync_from_remote(
        *,
        msglog: MessageLogger,
        remote_name: str,
        remote_url: str,
        local_dir: str,
        online_user_agent: str,
        access_token: str,
        timeout_in_seconds: float,
        demote_connection_errors_to_status: bool,
        extension_override: str,
) -> bool:
    """
    Load package information into the local path.
    """

    # Validate arguments.
    if (error := remote_url_validate_or_error(remote_url)) is not None:
        msglog.fatal_error(error)
        return False

    request_exit = False
    request_exit |= msglog.status("Checking repository \"{:s}\" for updates...".format(remote_name))
    if request_exit:
        return False

    if (local_private_dir := repo_local_private_dir_ensure(
            local_dir=local_dir,
            error_fn=lambda ex: any_as_none(
                msglog.fatal_error("Error creating private directory: {:s}".format(str(ex)))
            ),
    )) is None:
        return False

    remote_json_url = remote_url_get(remote_url)

    local_json_path = os.path.join(local_private_dir, PKG_REPO_LIST_FILENAME)
    local_json_path_temp = local_json_path + "@"

    assert extension_override != "@"
    if extension_override:
        local_json_path = local_json_path + extension_override

    if os.path.exists(local_json_path_temp):
        os.unlink(local_json_path_temp)

    with CleanupPathsContext(files=(local_json_path_temp,), directories=()):
        # TODO: time-out.
        request_exit |= msglog.status("Refreshing extensions list for \"{:s}\"...".format(remote_name))
        if request_exit:
            return False

        try:
            retrieve_info = DataRetrieveInfo()
            read_total = 0
            for read in url_retrieve_to_filepath_iter_or_filesystem(
                    remote_json_url,
                    local_json_path_temp,
                    headers=url_request_headers_create(
                        accept_json=True,
                        user_agent=online_user_agent,
                        access_token=access_token,
                    ),
                    chunk_size=CHUNK_SIZE_DEFAULT,
                    timeout_in_seconds=timeout_in_seconds,
                    retrieve_info=retrieve_info,
            ):
                request_exit |= msglog.progress("Downloading...", read_total, retrieve_info.size_hint, 'BYTE')
                if request_exit:
                    break
                read_total += read
            del read_total
            del retrieve_info
        except (Exception, KeyboardInterrupt) as ex:
            msg = url_retrieve_exception_as_message(ex, prefix="sync", url=remote_json_url)
            if demote_connection_errors_to_status and url_retrieve_exception_is_connectivity(ex):
                msglog.status(msg)
            else:
                msglog.fatal_error(msg)
            return False

        if request_exit:
            return False

        error_msg = repo_json_is_valid_or_error(local_json_path_temp)
        if error_msg is not None:
            msglog.fatal_error(
                "Repository error: invalid manifest ({:s}) for repository \"{:s}\"!".format(error_msg, remote_name),
            )
            return False
        del error_msg

        request_exit |= msglog.status("Extensions list for \"{:s}\" updated".format(remote_name))
        if request_exit:
            return False

        if os.path.exists(local_json_path):
            os.unlink(local_json_path)

        # If this is a valid JSON, overwrite the existing file.
        os.rename(local_json_path_temp, local_json_path)

        if extension_override:
            request_exit |= msglog.path(os.path.relpath(local_json_path, local_dir))

    return True


def repo_pkginfo_from_local_as_dict_or_error(*, local_dir: str) -> dict[str, Any] | str:
    """
    Load package cache.
    """
    local_private_dir = repo_local_private_dir(local_dir=local_dir)
    local_json_path = os.path.join(local_private_dir, PKG_REPO_LIST_FILENAME)

    # Don't check if the path exists, allow this to raise an exception.
    try:
        with open(local_json_path, "r", encoding="utf-8") as fh:
            result = json.load(fh)
    except Exception as ex:
        return str(ex)

    if not isinstance(result, dict):
        return "expected a dict, not a {:s}".format(str(type(result)))

    return result


def pkg_repo_data_from_json_or_error(json_data: dict[str, Any]) -> PkgRepoData | str:
    if not isinstance((version := json_data.get("version", "v1")), str):
        return "expected \"version\" to be a string"

    if not isinstance((blocklist := json_data.get("blocklist", [])), list):
        return "expected \"blocklist\" to be a list"
    for item in blocklist:
        if not isinstance(item, dict):
            return "expected \"blocklist\" contain dictionary items"

    if not isinstance((data := json_data.get("data", [])), list):
        return "expected \"data\" to be a list"
    for item in data:
        if not isinstance(item, dict):
            return "expected \"data\" contain dictionary items"

    result_new = PkgRepoData(
        version=version,
        blocklist=blocklist,
        data=data,
    )
    return result_new


def repo_pkginfo_from_local_or_none(*, local_dir: str) -> PkgRepoData | str:
    if isinstance((result := repo_pkginfo_from_local_as_dict_or_error(local_dir=local_dir)), str):
        return result
    return pkg_repo_data_from_json_or_error(result)


def url_has_known_prefix(path: str) -> bool:
    return path.startswith(URL_KNOWN_PREFIX)


def url_is_filesystem(url: str) -> bool:
    if url.startswith(("file://")):
        return True
    if url.startswith(URL_KNOWN_PREFIX):
        return False

    # Error handling must ensure this never happens.
    assert False, "unreachable, prefix not known"

    return False


# -----------------------------------------------------------------------------
# Generate Argument Handlers

def arg_handle_int_as_bool(value: str) -> bool:
    result = int(value)
    if result not in {0, 1}:
        raise argparse.ArgumentTypeError("Expected a 0 or 1")
    return bool(result)


def arg_handle_str_as_package_names(value: str) -> Sequence[str]:
    result = value.split(",")
    for pkg_idname in result:
        if (error_msg := pkg_idname_is_valid_or_error(pkg_idname)) is not None:
            raise argparse.ArgumentTypeError("Invalid name \"{:s}\". {:s}".format(pkg_idname, error_msg))
    return result


def arg_handle_str_as_temp_prefix_and_suffix(value: str) -> tuple[str, str]:
    if (value.count("/") != 1) and (len(value) > 1):
        raise argparse.ArgumentTypeError("Must contain a \"/\" character with a prefix and/or suffix")
    a, b = value.split("/", 1)
    return a, b


# -----------------------------------------------------------------------------
# Argument Handlers ("build" command)

def generic_arg_build_split_platforms(subparse: argparse.ArgumentParser) -> None:
    subparse.add_argument(
        "--split-platforms",
        dest="split_platforms",
        action="store_true",
        default=False,
        help=(
            "Build a separate package for each platform.\n"
            "Adding the platform as a file name suffix (before the extension).\n"
            "\n"
            "This can be useful to reduce the upload size of packages that bundle large\n"
            "platform-specific modules (``*.whl`` files)."
        ),
    )


def generic_arg_package_valid_tags(subparse: argparse.ArgumentParser) -> None:
    # NOTE(@ideasman42): when called from Blender tags for `extensions.blender.org` are enforced by default.
    # For `extensions.blender.org` this is enforced on the server side, so it's better developers see the error
    # on build/validate instead of uploading the package.
    # It's worth noting not all extensions will be hosted on `extensions.blender.org`,
    # 3rd party hosting should remain a first class citizen not some exceptional case.
    #
    # The rationale for applying these tags for all packages even accepting that not everyone is targeting
    # Blender's official repository is to avoid every extension defining their own tags.
    #
    # This has two down sides:
    # - Duplicate similar tags, e.g. `"render", "rendering"`, `"toon", "cartoon"` etc.
    # - Tag proliferation (100's of tags), makes the UI unusable.
    #   So even when all tags are valid and named well, having everyone defining
    #   their own tags results the user having to filter between too many options.
    #   Although a re-designed UI could account for this if it were important.
    #
    # Nevertheless, allow motivated developers to ignore the tags limitations as it's somewhat arbitrarily.
    # The default to apply these limits is a "nudge" to avoid additional tags from typos as well as a hint
    # that tags should be added to Blender's list if they're needed instead of being defined ad-hoc.

    subparse.add_argument(
        "--valid-tags",
        dest="valid_tags_filepath",
        default=ARG_DEFAULTS_OVERRIDE.build_valid_tags,
        metavar="VALID_TAGS_JSON",
        # NOTE(@ideasman42): Python input is also supported, intentionally undocumented for now,
        # since this is only supported as Blender's tags happen to be stored as a Python script - which may change.
        help=(
            "Reference a file path containing valid tags lists.\n"
            "\n"
            "If you wish to reference custom tags a ``.json`` file can be used.\n"
            "The contents must be a dictionary of lists where the ``key`` matches the extension type.\n"
            "\n"
            "For example:\n"
            "   ``{\"add-ons\": [\"Example\", \"Another\"], \"theme\": [\"Other\", \"Tags\"]}``\n"
            "\n"
            "To disable validating tags, pass in an empty path ``--valid-tags=\"\"``."
        ),
    )


# -----------------------------------------------------------------------------
# Argument Handlers ("server-generate" command)

def generic_arg_server_generate_repo_config(subparse: argparse.ArgumentParser) -> None:
    subparse.add_argument(
        "--repo-config",
        dest="repo_config",
        default="",
        metavar="REPO_CONFIG",
        help=(
            "An optional server configuration to include information which can't be detected.\n"
            "Defaults to ``blender_repo.toml`` (in the repository directory).\n"
            "\n"
            "This can be used to defined blocked extensions, for example ::\n"
            "\n"
            "   schema_version = \"1.0.0\"\n"
            "\n"
            "   [[blocklist]]\n"
            "   id = \"my_example_package\"\n"
            "   reason = \"Explanation for why this extension was blocked\"\n"
            "   [[blocklist]]\n"
            "   id = \"other_extenison\"\n"
            "   reason = \"Another reason for why this is blocked\"\n"
            "\n"
        ),
    )


def generic_arg_server_generate_html(subparse: argparse.ArgumentParser) -> None:
    subparse.add_argument(
        "--html",
        dest="html",
        action="store_true",
        default=False,
        help=(
            "Create a HTML file (``index.html``) as well as the repository JSON\n"
            "to support browsing extensions online with static-hosting."
        ),
    )


def generic_arg_server_generate_html_template(subparse: argparse.ArgumentParser) -> None:
    subparse.add_argument(
        "--html-template",
        dest="html_template",
        default="",
        metavar="HTML_TEMPLATE_FILE",
        help=(
            "An optional HTML file path to override the default HTML template with your own.\n"
            "\n"
            "The following keys will be replaced with generated contents:\n"
            "\n"
            "- ``${body}`` is replaced the extensions contents.\n"
            "- ``${date}`` is replaced the creation date.\n"
        ),
    )


# -----------------------------------------------------------------------------
# Generate Repository


def generic_arg_package_list_positional(subparse: argparse.ArgumentParser) -> None:
    subparse.add_argument(
        dest="packages",
        type=str,
        help=(
            "The packages to operate on (separated by ``,`` without spaces)."
        ),
    )


def generic_arg_file_list_positional(subparse: argparse.ArgumentParser) -> None:
    subparse.add_argument(
        dest="files",
        type=str,
        nargs="+",
        help=(
            "The files to operate on (one or more arguments)."
        ),
    )


def generic_arg_repo_dir(subparse: argparse.ArgumentParser) -> None:
    subparse.add_argument(
        "--repo-dir",
        dest="repo_dir",
        type=str,
        help=(
            "The remote repository directory."
        ),
        required=True,
    )


def generic_arg_remote_name(subparse: argparse.ArgumentParser) -> None:
    subparse.add_argument(
        "--remote-name",
        dest="remote_name",
        type=str,
        help=(
            "The remote repository name."
        ),
        default="",
        required=False,
    )


def generic_arg_remote_url(subparse: argparse.ArgumentParser) -> None:
    subparse.add_argument(
        "--remote-url",
        dest="remote_url",
        type=str,
        help=(
            "The remote repository URL."
        ),
        required=True,
    )


def generic_arg_local_dir(subparse: argparse.ArgumentParser) -> None:
    subparse.add_argument(
        "--local-dir",
        dest="local_dir",
        type=str,
        help=(
            "The local checkout."
        ),
        required=True,
    )


def generic_arg_user_dir(subparse: argparse.ArgumentParser) -> None:
    subparse.add_argument(
        "--user-dir",
        dest="user_dir",
        default="",
        type=str,
        help=(
            "Additional files associated with this package."
        ),
        required=False,
    )


def generic_arg_blender_version(subparse: argparse.ArgumentParser) -> None:
    subparse.add_argument(
        "--blender-version",
        dest="blender_version",
        default="0.0.0",
        type=str,
        help=(
            "The version of Blender used for selecting packages."
        ),
        required=False,
    )


def generic_arg_python_version(subparse: argparse.ArgumentParser) -> None:
    subparse.add_argument(
        "--python-version",
        dest="python_version",
        default="0.0.0",
        type=str,
        help=(
            "The version of Python used for selecting packages."
        ),
        required=False,
    )


def generic_arg_temp_prefix_and_suffix(subparse: argparse.ArgumentParser) -> None:
    subparse.add_argument(
        "--temp-prefix-and-suffix",
        dest="temp_prefix_and_suffix",
        default="./.temp",
        type=arg_handle_str_as_temp_prefix_and_suffix,
        help=(
            "The template to use when removing files. "
            "A slash separates the: `prefix/suffix`, digits may be appended"
        ),
        required=False,
    )


# Only for authoring.
def generic_arg_package_source_path_positional(subparse: argparse.ArgumentParser) -> None:
    subparse.add_argument(
        dest="source_path",
        nargs="?",
        default=".",
        type=str,
        metavar="SOURCE_PATH",
        help=(
            "The package source path (either directory containing package files or the package archive).\n"
            "This path must containing a ``{:s}`` manifest.\n"
            "\n"
            "Defaults to the current directory."
        ).format(PKG_MANIFEST_FILENAME_TOML),
    )


def generic_arg_package_source_dir(subparse: argparse.ArgumentParser) -> None:
    subparse.add_argument(
        "--source-dir",
        dest="source_dir",
        default=".",
        type=str,
        help=(
            "The package source directory containing a ``{:s}`` manifest.\n"
            "\n"
            "Default's to the current directory."
        ).format(PKG_MANIFEST_FILENAME_TOML),
    )


def generic_arg_package_output_dir(subparse: argparse.ArgumentParser) -> None:
    subparse.add_argument(
        "--output-dir",
        dest="output_dir",
        default=".",
        type=str,
        help=(
            "The package output directory.\n"
            "\n"
            "Default's to the current directory."
        ),
    )


def generic_arg_package_output_filepath(subparse: argparse.ArgumentParser) -> None:
    subparse.add_argument(
        "--output-filepath",
        dest="output_filepath",
        default="",
        type=str,
        help=(
            "The package output filepath (should include a ``{0:s}`` extension).\n"
            "\n"
            "Defaults to ``{{id}}-{{version}}{0:s}`` using values from the manifest."
        ).format(PKG_EXT),
    )


def generic_arg_output_type(subparse: argparse.ArgumentParser) -> None:
    subparse.add_argument(
        "--output-type",
        dest="output_type",
        type=str,
        choices=('TEXT', 'JSON', 'JSON_0'),
        default='TEXT',
        help=(
            "The output type:\n"
            "\n"
            "- TEXT: Plain text (default).\n"
            "- JSON: Separated by new-lines.\n"
            "- JSON_0: Separated null bytes.\n"
        ),
        required=False,
    )


def generic_arg_local_cache(subparse: argparse.ArgumentParser) -> None:
    subparse.add_argument(
        "--local-cache",
        dest="local_cache",
        type=arg_handle_int_as_bool,
        help=(
            "Use local cache, when disabled packages always download from remote."
        ),
        default=True,
        required=False,
    )


def generic_arg_online_user_agent(subparse: argparse.ArgumentParser) -> None:
    subparse.add_argument(
        "--online-user-agent",
        dest="online_user_agent",
        type=str,
        help=(
            "Use user-agent used for making web requests. "
            "Some web sites will reject requests when unset."
        ),
        default="",
        required=False,
    )


def generic_arg_access_token(subparse: argparse.ArgumentParser) -> None:
    subparse.add_argument(
        "--access-token",
        dest="access_token",
        type=str,
        help=(
            "Access token for remote repositories which require authorized access."
        ),
        default="",
        required=False,
    )


def generic_arg_verbose(subparse: argparse.ArgumentParser) -> None:
    subparse.add_argument(
        "--verbose",
        dest="verbose",
        action="store_true",
        default=False,
        help="Include verbose output.",
    )


def generic_arg_timeout(subparse: argparse.ArgumentParser) -> None:
    subparse.add_argument(
        "--timeout",
        dest="timeout",
        type=float,
        help=(
            "Timeout when reading from remote location."
        ),
        default=10.0,
        required=False,
    )


def generic_arg_ignore_broken_pipe(subparse: argparse.ArgumentParser) -> None:
    subparse.add_argument(
        "--force-exit-ok",
        dest="force_exit_ok",
        action="store_true",
        default=False,
        help=(
            "Silently ignore broken pipe, use when the caller may disconnect."
        ),
    )


def generic_arg_demote_connection_failure_to_status(subparse: argparse.ArgumentParser) -> None:
    subparse.add_argument(
        "--demote-connection-errors-to-status",
        dest="demote_connection_errors_to_status",
        action="store_true",
        default=False,
        help=(
            "Demote errors relating to connection failure to status updates.\n"
            "To be used when connection failure should not be considered an error."
        ),
    )


def generic_arg_extension_override(subparse: argparse.ArgumentParser) -> None:
    subparse.add_argument(
        "--extension-override",
        dest="extension_override",
        type=str,
        help=(
            "Use a non-standard extension. "
            "When a non-empty string, this extension is appended to paths written to. "
            "This allows the actual repository file to be left untouched so it can be replaced "
            "by the caller which can handle locking the repository."
        ),
        default="",
        required=False,
    )


class subcmd_server:

    def __new__(cls) -> Any:
        raise RuntimeError("{:s} should not be instantiated".format(cls))

    @staticmethod
    def _generate_html(
            msglog: MessageLogger,
            *,
            repo_dir: str,
            repo_data: list[dict[str, Any]],
            html_template_filepath: str,
    ) -> bool:
        import html
        import datetime
        from string import (
            Template,
            capwords,
        )

        filepath_repo_html = os.path.join(repo_dir, "index.html")

        fh = io.StringIO()

        # Group extensions by their type.
        repo_data_by_type: dict[str, list[dict[str, Any]]] = {}

        for manifest_dict in repo_data:
            manifest_type = manifest_dict["type"]
            try:
                repo_data_typed = repo_data_by_type[manifest_type]
            except KeyError:
                repo_data_typed = repo_data_by_type[manifest_type] = []
            repo_data_typed.append(manifest_dict)

        for manifest_type, repo_data_typed in sorted(repo_data_by_type.items(), key=lambda item: item[0]):
            # Type heading.
            fh.write("<p>{:s}</p>\n".format(capwords(manifest_type)))
            fh.write("<hr>\n")

            fh.write("<table>\n")
            fh.write("  <tr>\n")
            fh.write("    <th>ID</th>\n")
            fh.write("    <th>Name</th>\n")
            fh.write("    <th>Description</th>\n")
            fh.write("    <th>Website</th>\n")
            fh.write("    <th>Blender Versions</th>\n")
            fh.write("    <th>Python Versions</th>\n")
            fh.write("    <th>Platforms</th>\n")
            fh.write("    <th>Size</th>\n")
            fh.write("  </tr>\n")

            for manifest_dict in sorted(
                    repo_data_typed,
                    key=lambda manifest_dict: (manifest_dict["id"], manifest_dict["version"]),
            ):
                fh.write("  <tr>\n")

                platforms = manifest_dict.get("platforms", [])
                python_versions = manifest_dict.get("python_versions", [])

                # Parse the URL and add parameters use for drag & drop.
                parsed_url = urllib.parse.urlparse(manifest_dict["archive_url"])
                # We could support existing values, currently always empty.
                # `query = dict(urllib.parse.parse_qsl(parsed_url.query))`
                query = {"repository": "./index.json"}
                if (value := manifest_dict.get("blender_version_min", "")):
                    query["blender_version_min"] = value
                if (value := manifest_dict.get("blender_version_max", "")):
                    query["blender_version_max"] = value
                if platforms:
                    query["platforms"] = ",".join(platforms)
                if python_versions:
                    query["python_versions"] = ",".join(python_versions)
                del value

                id_and_link = "<a href=\"{:s}\">{:s}</a>".format(
                    urllib.parse.urlunparse((
                        parsed_url.scheme,
                        parsed_url.netloc,
                        parsed_url.path,
                        parsed_url.params,
                        urllib.parse.urlencode(query, doseq=True) if query else None,
                        parsed_url.fragment,
                    )),
                    html.escape("{:s}-{:s}".format(manifest_dict["id"], manifest_dict["version"])),
                )

                # Write the table data.
                fh.write("    <td><tt>{:s}</tt></td>\n".format(id_and_link))
                fh.write("    <td>{:s}</td>\n".format(html.escape(manifest_dict["name"])))
                fh.write("    <td>{:s}</td>\n".format(html.escape(manifest_dict["tagline"] or "<NA>")))
                if value := manifest_dict.get("website", ""):
                    fh.write("    <td><a href=\"{:s}\">link</a></td>\n".format(html.escape(value)))
                else:
                    fh.write("    <td>~</td>\n")
                del value
                blender_version_min = manifest_dict.get("blender_version_min", "")
                blender_version_max = manifest_dict.get("blender_version_max", "")
                if blender_version_min or blender_version_max:
                    blender_version_str = "{:s} - {:s}".format(
                        blender_version_min or "~",
                        blender_version_max or "~",
                    )
                else:
                    blender_version_str = "all"

                if python_versions:
                    python_version_str = ", ".join(python_versions)
                else:
                    python_version_str = "all"

                fh.write("    <td>{:s}</td>\n".format(html.escape(blender_version_str)))
                fh.write("    <td>{:s}</td>\n".format(html.escape(python_version_str)))
                fh.write("    <td>{:s}</td>\n".format(html.escape(", ".join(platforms) if platforms else "all")))
                fh.write("    <td>{:s}</td>\n".format(html.escape(size_as_fmt_string(manifest_dict["archive_size"]))))
                fh.write("  </tr>\n")

            fh.write("</table>\n")

        body = fh.getvalue()
        del fh

        html_template_text = ""
        if html_template_filepath:
            try:
                with open(html_template_filepath, "r", encoding="utf-8") as fh_html:
                    html_template_text = fh_html.read()
            except Exception as ex:
                msglog.fatal_error("HTML template failed to read: {:s}".format(str(ex)))
                return False
        else:
            html_template_text = HTML_TEMPLATE

        template = Template(html_template_text)
        del html_template_text

        try:
            result = template.substitute(
                body=body,
                date=html.escape(datetime.datetime.now(tz=datetime.timezone.utc).strftime("%Y-%m-%d, %H:%M")),
            )
        except KeyError as ex:
            msglog.fatal_error("HTML template error: {:s}".format(str(ex)))
            return False
        del template

        try:
            with open(filepath_repo_html, "w", encoding="utf-8") as fh_html:
                fh_html.write(result)
        except Exception as ex:
            msglog.fatal_error("HTML failed to write: {:s}".format(str(ex)))
            return False

        return True

    @staticmethod
    def generate(
            msglog: MessageLogger,
            *,
            repo_dir: str,
            repo_config_filepath: str,
            html: bool,
            html_template: str,
    ) -> bool:
        if url_has_known_prefix(repo_dir):
            msglog.fatal_error("Directory: {!r} must be a local path, not a URL!".format(repo_dir))
            return False

        if not os.path.isdir(repo_dir):
            msglog.fatal_error("Directory: {!r} not found!".format(repo_dir))
            return False

        # Server manifest (optional), use if found.
        server_manifest_default = "blender_repo.toml"
        if not repo_config_filepath:
            server_manifest_test = os.path.join(repo_dir, server_manifest_default)
            if os.path.exists(server_manifest_test):
                repo_config_filepath = server_manifest_test
            del server_manifest_test
        del server_manifest_default

        repo_config = None
        if repo_config_filepath:
            repo_config = pkg_server_repo_config_from_toml_and_validate(repo_config_filepath)
            if isinstance(repo_config, str):
                msglog.fatal_error("parsing repository configuration {!r}, {:s}".format(
                    repo_config,
                    repo_config_filepath,
                ))
                return False
            if repo_config.schema_version != "1.0.0":
                msglog.fatal_error("unsupported schema version {!r} in {:s}, expected 1.0.0".format(
                    repo_config.schema_version,
                    repo_config_filepath,
                ))
                return False
        assert repo_config is None or isinstance(repo_config, PkgServerRepoConfig)

        repo_data_idname_map: dict[str, list[tuple[PkgManifest, str, list[tuple[int] | tuple[int, int]]]]] = {}
        repo_data: list[dict[str, Any]] = []

        # Write package meta-data into each directory.
        repo_gen_dict = {
            "version": "v1",
            "blocklist": [] if repo_config is None else repo_config.blocklist,
            "data": repo_data,
        }

        del repo_config

        for entry in os.scandir(repo_dir):
            if not entry.name.endswith(PKG_EXT):
                continue
            # Temporary files (during generation) use a "." prefix, skip them.
            if entry.name.startswith("."):
                continue

            # Harmless, but skip directories.
            if entry.is_dir():
                msglog.warn("found unexpected directory {!r}".format(entry.name))
                continue

            filename = entry.name
            filepath = os.path.join(repo_dir, filename)
            manifest = pkg_manifest_from_archive_and_validate(filepath, strict=False)
            if isinstance(manifest, str):
                msglog.error("archive validation failed {!r}, error: {:s}".format(filepath, manifest))
                continue
            manifest_dict = manifest._asdict()

            pkg_idname = manifest_dict["id"]

            # Call all optional keys so the JSON never contains `null` items.
            for key, value in list(manifest_dict.items()):
                if value is None:
                    del manifest_dict[key]

            # Don't include these in the server listing.
            wheels: list[str] = manifest_dict.pop("wheels", [])

            # Extract the `python_versions` from wheels.
            python_versions_final: list[tuple[int] | tuple[int, int]] = []
            if wheels:
                if isinstance(python_versions := python_versions_from_wheels(wheels), str):
                    msglog.warn("unable to parse Python version from \"wheels\" ({:s}): {:s}".format(
                        python_versions,
                        filepath,
                    ))
                else:
                    python_versions_final[:] = sorted(python_versions)

                    manifest_dict["python_versions"] = [
                        ".".join(str(v) for v in version)
                        for version in python_versions_final
                    ]

            if (pkg_items := repo_data_idname_map.get(pkg_idname)) is None:
                pkg_items = repo_data_idname_map[pkg_idname] = []
            pkg_items.append((manifest, filename, python_versions_final))

            # These are added, ensure they don't exist.
            has_key_error = False
            for key in ("archive_url", "archive_size", "archive_hash"):
                if key not in manifest_dict:
                    continue
                msglog.error("malformed meta-data from {!r}, contains key it shouldn't: {:s}".format(filepath, key))
                has_key_error = True
            if has_key_error:
                continue

            # A relative URL.
            manifest_dict["archive_url"] = "./" + urllib.request.pathname2url(filename)

            # Add archive variables, see: `PkgManifest_Archive`.
            if isinstance((result := sha256_from_file_or_error(filepath, hash_prefix=True)), str):
                msglog.error("unable to calculate hash ({:s}): {:s}".format(result, filepath))
                continue
            manifest_dict["archive_size"], manifest_dict["archive_hash"] = result
            del result

            repo_data.append(manifest_dict)

        # Detect duplicates:
        # repo_data_idname_map
        for pkg_idname, pkg_items in repo_data_idname_map.items():
            if len(pkg_items) == 1:
                continue
            # Sort for predictable output.
            pkg_items.sort(key=lambda pkg_item_ext: pkg_item_ext[1])
            if (error := pkg_manifest_detect_duplicates(pkg_items)) is not None:
                msglog.warn("archive found with duplicates for id {:s}: {:s}".format(pkg_idname, error))

        if html:
            if not subcmd_server._generate_html(
                    msglog,
                    repo_dir=repo_dir,
                    repo_data=repo_data,
                    html_template_filepath=html_template,
            ):
                return False

        del repo_data_idname_map

        filepath_repo_json = os.path.join(repo_dir, PKG_REPO_LIST_FILENAME)

        try:
            with open(filepath_repo_json, "w", encoding="utf-8") as fh:
                json.dump(repo_gen_dict, fh, indent=2)
        except Exception as ex:
            msglog.fatal_error("failed to write repository: {:s}".format(str(ex)))
            return False

        msglog.status("found {:d} packages.".format(len(repo_data)))

        return True


class subcmd_client:

    def __new__(cls) -> Any:
        raise RuntimeError("{:s} should not be instantiated".format(cls))

    @staticmethod
    def list_packages(
            msglog: MessageLogger,
            *,
            remote_url: str,
            online_user_agent: str,
            access_token: str,
            timeout_in_seconds: float,
            demote_connection_errors_to_status: bool,
    ) -> bool:

        # Validate arguments.
        if (error := remote_url_validate_or_error(remote_url)) is not None:
            msglog.fatal_error(error)
            return False

        remote_json_url = remote_url_get(remote_url)

        # TODO: validate JSON content.
        try:
            result = io.BytesIO()
            for block in url_retrieve_to_data_iter_or_filesystem(
                    remote_json_url,
                    headers=url_request_headers_create(
                        accept_json=True,
                        user_agent=online_user_agent,
                        access_token=access_token,
                    ),
                    chunk_size=CHUNK_SIZE_DEFAULT,
                    timeout_in_seconds=timeout_in_seconds,
                    retrieve_info=DataRetrieveInfo(),  # Unused.
            ):
                result.write(block)

        except (Exception, KeyboardInterrupt) as ex:
            msg = url_retrieve_exception_as_message(ex, prefix="list", url=remote_json_url)
            if demote_connection_errors_to_status and url_retrieve_exception_is_connectivity(ex):
                msglog.status(msg)
            else:
                msglog.fatal_error(msg)
            return False

        result_bytes = result.getvalue()
        del result

        try:
            result_dict = json.loads(result_bytes)
        except Exception as ex:
            msglog.fatal_error("error loading JSON {:s}".format(str(ex)))
            return False

        if isinstance((repo_gen_dict := pkg_repo_data_from_json_or_error(result_dict)), str):
            msglog.fatal_error("unexpected contents in JSON {:s}".format(repo_gen_dict))
            return False
        del result_dict

        items: list[dict[str, Any]] = repo_gen_dict.data
        items.sort(key=lambda elem: elem.get("id", ""))

        request_exit = False
        for elem in items:
            request_exit |= msglog.status(
                "{:s}({:s}): {:s}".format(elem.get("id"), elem.get("version"), elem.get("name")),
            )
            if request_exit:
                return False

        return True

    @staticmethod
    def sync(
            msglog: MessageLogger,
            *,
            remote_url: str,
            remote_name: str,
            local_dir: str,
            online_user_agent: str,
            access_token: str,
            timeout_in_seconds: float,
            demote_connection_errors_to_status: bool,
            force_exit_ok: bool,
            extension_override: str,
    ) -> bool:
        if force_exit_ok:
            force_exit_ok_enable()

        success = repo_sync_from_remote(
            msglog=msglog,
            remote_name=remote_name,
            remote_url=remote_url,
            local_dir=local_dir,
            online_user_agent=online_user_agent,
            access_token=access_token,
            timeout_in_seconds=timeout_in_seconds,
            demote_connection_errors_to_status=demote_connection_errors_to_status,
            extension_override=extension_override,
        )
        return success

    @staticmethod
    def _install_package_from_file_impl(
            msglog: MessageLogger,
            *,
            local_dir: str,
            filepath_archive: str,
            blender_version_tuple: tuple[int, int, int],
            python_version_tuple: tuple[int, int, int],
            manifest_compare: PkgManifest | None,
            temp_prefix_and_suffix: tuple[str, str],
    ) -> bool:
        # NOTE: Don't use `FATAL_ERROR` because other packages will attempt to install.

        # Implement installing a package to a repository.
        # Used for installing from local cache as well as installing a local package from a file.

        # Remove `filepath_local_pkg_temp` if this block exits.
        directories_to_clean: list[str] = []
        with CleanupPathsContext(files=(), directories=directories_to_clean):
            try:
                # pylint: disable-next=consider-using-with
                zip_fh_context = zipfile.ZipFile(filepath_archive, mode="r")
            except Exception as ex:
                msglog.error("Error extracting archive: {:s}".format(str(ex)))
                return False

            with contextlib.closing(zip_fh_context) as zip_fh:
                archive_subdir = pkg_zipfile_detect_subdir_or_none(zip_fh)
                if archive_subdir is None:
                    msglog.error("Missing manifest from: {:s}".format(filepath_archive))
                    return False

                manifest = pkg_manifest_from_zipfile_and_validate(zip_fh, archive_subdir, strict=False)
                if isinstance(manifest, str):
                    msglog.error("Failed to load manifest from: {:s}".format(manifest))
                    return False

                if manifest_compare is not None:
                    # The archive ID name must match the server name,
                    # otherwise the package will install but not be able to collate
                    # the installed package with the remote ID.
                    if manifest_compare.id != manifest.id:
                        msglog.error(
                            "Package ID mismatch (remote: \"{:s}\", archive: \"{:s}\")".format(
                                manifest_compare.id,
                                manifest.id,
                            )
                        )
                        return False
                    if manifest_compare.version != manifest.version:
                        msglog.error(
                            "Package version mismatch (remote: \"{:s}\", archive: \"{:s}\")".format(
                                manifest_compare.version,
                                manifest.version,
                            )
                        )
                        return False

                if repository_filter_skip(
                    # Converting back to a dict is awkward but harmless,
                    # done since some callers only have a dictionary.
                    manifest._asdict(),
                    filter_blender_version=blender_version_tuple,
                    filter_platform=platform_from_this_system(),
                    filter_python_version=python_version_tuple,
                    skip_message_fn=lambda message: any_as_none(
                        msglog.error("{:s}: {:s}".format(manifest.id, message))
                    ),
                    error_fn=lambda ex: any_as_none(
                        msglog.error("{:s}: {:s}".format(manifest.id, str(ex)))
                    ),
                ):
                    return False

                # We have the cache, extract it to a directory.
                # This will be a directory.
                filepath_local_pkg = os.path.join(local_dir, manifest.id)

                # First extract into a temporary directory, validate the package is not corrupt,
                # then move the package to it's expected location.
                filepath_local_pkg_temp = filepath_local_pkg + "@"

                # It's unlikely this exist, nevertheless if it does - it must be removed.
                if os.path.lexists(filepath_local_pkg_temp):
                    if (error := rmtree_with_fallback_or_error(filepath_local_pkg_temp)) is not None:
                        msglog.error(
                            "Failed to remove temporary directory for \"{:s}\": {:s}".format(manifest.id, error),
                        )
                        return False

                directories_to_clean.append(filepath_local_pkg_temp)

                if archive_subdir:
                    zipfile_make_root_directory(zip_fh, archive_subdir)
                del archive_subdir

                try:
                    for member in zip_fh.infolist():
                        zip_fh.extract(member, filepath_local_pkg_temp)
                except Exception as ex:
                    msglog.error("Failed to extract files for \"{:s}\": {:s}".format(manifest.id, str(ex)))
                    return False

            is_reinstall = False
            # Even though this is expected to be a directory,
            # check for any file since the existence of a file should not break installation.
            # Besides users manually creating files, this could occur from broken symbolic-links
            # or an incorrectly repaired corrupt file-system.
            if os.path.lexists(filepath_local_pkg):
                if (error := rmtree_with_fallback_or_error_pseudo_atomic(
                        filepath_local_pkg,
                        temp_prefix_and_suffix=temp_prefix_and_suffix,
                )) is not None:
                    if os.path.lexists(filepath_local_pkg):
                        msglog.error("Failed to remove or relocate existing directory for \"{:s}\": {:s}".format(
                            manifest.id,
                            error,
                        ))
                        return False

                    msglog.status("Relocated directory that could not be removed \"{:s}\": {:s}".format(
                        manifest.id,
                        error,
                    ))

                is_reinstall = True

            # While renaming should never fail, it's always possible file-system operations fail.
            # Unlike other actions, failure here causes the extension to be uninstalled.
            #
            # There is little that can be done about this, being able to create a temporary
            # directory and move it into the destination is required for installation.
            # When that fails - the best that can be done is to communicate the failure, see: #130211.
            try:
                os.rename(filepath_local_pkg_temp, filepath_local_pkg)
            except Exception as ex:
                msglog.error("Failed to rename directory, causing unexpected removal \"{:s}\": {:s}".format(
                    manifest.id,
                    str(ex),
                ))
                return False

            directories_to_clean.remove(filepath_local_pkg_temp)

        if is_reinstall:
            msglog.status("Reinstalled \"{:s}\"".format(manifest.id))
        else:
            msglog.status("Installed \"{:s}\"".format(manifest.id))

        return True

    @staticmethod
    def install_packages_from_files(
            msglog: MessageLogger,
            *,
            local_dir: str,
            package_files: Sequence[str],
            blender_version: str,
            python_version: str,
            temp_prefix_and_suffix: tuple[str, str],
    ) -> bool:
        if not os.path.exists(local_dir):
            msglog.fatal_error("destination directory \"{:s}\" does not exist".format(local_dir))
            return False

        if isinstance(blender_version_tuple := blender_version_parse_or_error(blender_version), str):
            msglog.fatal_error(blender_version_tuple)
            return False
        assert isinstance(blender_version_tuple, tuple)

        if isinstance(python_version_tuple := python_version_parse_or_error(python_version), str):
            msglog.fatal_error(python_version_tuple)
            return False
        assert isinstance(python_version_tuple, tuple)

        # This is a simple file extraction, the main difference is that it validates the manifest before installing.
        directories_to_clean: list[str] = []
        with CleanupPathsContext(files=(), directories=directories_to_clean):
            for filepath_archive in package_files:
                if not subcmd_client._install_package_from_file_impl(
                        msglog,
                        local_dir=local_dir,
                        filepath_archive=filepath_archive,
                        blender_version_tuple=blender_version_tuple,
                        python_version_tuple=python_version_tuple,
                        # There is no manifest from the repository, leave this unset.
                        manifest_compare=None,
                        temp_prefix_and_suffix=temp_prefix_and_suffix,
                ):
                    # The package failed to install.
                    continue

        return True

    @staticmethod
    def install_packages(
            msglog: MessageLogger,
            *,
            remote_url: str,
            local_dir: str,
            local_cache: bool,
            packages: Sequence[str],
            online_user_agent: str,
            blender_version: str,
            python_version: str,
            access_token: str,
            timeout_in_seconds: float,
            temp_prefix_and_suffix: tuple[str, str],
    ) -> bool:

        # Validate arguments.
        if (error := remote_url_validate_or_error(remote_url)) is not None:
            msglog.fatal_error(error)
            return False

        if isinstance(blender_version_tuple := blender_version_parse_or_error(blender_version), str):
            msglog.fatal_error(blender_version_tuple)
            return False
        assert isinstance(blender_version_tuple, tuple)

        if isinstance(python_version_tuple := python_version_parse_or_error(python_version), str):
            msglog.fatal_error(python_version_tuple)
            return False
        assert isinstance(python_version_tuple, tuple)

        # Extract...
        if isinstance((pkg_repo_data := repo_pkginfo_from_local_or_none(local_dir=local_dir)), str):
            msglog.fatal_error("Error loading package repository: {:s}".format(pkg_repo_data))
            return False

        # Ensure a private directory so a local cache can be created.
        if (local_cache_dir := repo_local_private_dir_ensure_with_subdir(
                local_dir=local_dir,
                subdir="cache",
                error_fn=lambda ex: any_as_none(
                    msglog.fatal_error("Error creating cache directory: {:s}".format(str(ex)))
                ),
        )) is None:
            return False

        # Most likely this doesn't have duplicates,but any errors procured by duplicates
        # are likely to be obtuse enough that it's better to guarantee there are none.
        packages_as_set = set(packages)
        packages = tuple(sorted(packages_as_set))

        # Needed so relative paths can be properly calculated.
        remote_url_strip = remote_url_params_strip(remote_url)

        # TODO: filter by version and platform.
        json_data_pkg_info = [
            pkg_info for pkg_info in pkg_repo_data.data
            # The `id` key should always exist, avoid raising an unhandled exception here if it's not.
            if pkg_info.get("id", "") in packages_as_set
        ]

        # Narrow down:
        json_data_pkg_info_map: dict[str, list[dict[str, Any]]] = {pkg_idname: [] for pkg_idname in packages}
        for pkg_info in json_data_pkg_info:
            json_data_pkg_info_map[pkg_info["id"]].append(pkg_info)

        # NOTE: we could have full validation as a separate function,
        # currently install is the only place this is needed.
        json_data_pkg_block_map = {
            pkg_idname: pkg_block.get("reason", "Unknown")
            for pkg_block in pkg_repo_data.blocklist
            if (pkg_idname := pkg_block.get("id"))
        }

        platform_this = platform_from_this_system()

        has_fatal_error = False
        packages_info: list[PkgManifest_Archive] = []
        for pkg_idname, pkg_info_list in json_data_pkg_info_map.items():
            if not pkg_info_list:
                msglog.fatal_error("Package \"{:s}\", not found".format(pkg_idname))
                has_fatal_error = True
                continue

            if (result := json_data_pkg_block_map.get(pkg_idname)) is not None:
                msglog.fatal_error("Package \"{:s}\", is blocked: {:s}".format(pkg_idname, result))
                has_fatal_error = True
                continue

            pkg_info_list = [
                pkg_info for pkg_info in pkg_info_list
                if not repository_filter_skip(
                    pkg_info,
                    filter_blender_version=blender_version_tuple,
                    filter_platform=platform_this,
                    filter_python_version=python_version_tuple,
                    skip_message_fn=None,
                    error_fn=lambda ex: any_as_none(
                        # pylint: disable-next=cell-var-from-loop
                        msglog.error("{:s}: {:s}".format(pkg_idname, str(ex))),
                    ),
                )
            ]

            if not pkg_info_list:
                msglog.fatal_error(
                    "Package \"{:s}\", found but not compatible with this system".format(pkg_idname),
                )
                has_fatal_error = True
                continue

            # TODO: use a tie breaker.
            pkg_info = pkg_info_list[0]

            manifest_archive = pkg_manifest_archive_from_dict_and_validate(pkg_info, strict=False)
            if isinstance(manifest_archive, str):
                msglog.fatal_error("Package malformed meta-data for \"{:s}\", error: {:s}".format(
                    pkg_idname,
                    manifest_archive,
                ))
                has_fatal_error = True
                continue

            packages_info.append(manifest_archive)

        if has_fatal_error:
            return False
        del has_fatal_error

        request_exit = False

        # Ensure all cache is cleared (when `local_cache` is disabled) no matter the cause of exiting.
        files_to_clean: list[str] = []
        with CleanupPathsContext(files=files_to_clean, directories=()):
            for manifest_archive in packages_info:
                pkg_idname = manifest_archive.manifest.id
                # Archive name.
                archive_size_expected = manifest_archive.archive_size
                archive_hash_expected = manifest_archive.archive_hash
                pkg_archive_url = manifest_archive.archive_url

                # Local path.
                filepath_local_cache_archive = os.path.join(local_cache_dir, pkg_idname + PKG_EXT)

                if not local_cache:
                    files_to_clean.append(filepath_local_cache_archive)

                # Remote path.
                if pkg_archive_url.startswith("./"):
                    if remote_url_has_filename_suffix(remote_url_strip):
                        filepath_remote_archive = remote_url_strip.rpartition("/")[0] + pkg_archive_url[1:]
                    else:
                        filepath_remote_archive = remote_url_strip.rstrip("/") + pkg_archive_url[1:]
                else:
                    filepath_remote_archive = pkg_archive_url

                # Check if the cache should be used.
                found = False
                if os.path.exists(filepath_local_cache_archive):
                    if local_cache:
                        if isinstance((result := sha256_from_file_or_error(
                                filepath_local_cache_archive,
                                hash_prefix=True,
                        )), str):
                            # Only a warning because it's not a problem to re-download the file.
                            msglog.warn("unable to calculate hash for cache: {:s}".format(result))
                        elif result == (archive_size_expected, archive_hash_expected):
                            found = True
                    if not found:
                        os.unlink(filepath_local_cache_archive)

                if not found:
                    # Create `filepath_local_cache_archive`.
                    filename_archive_size_test = 0
                    sha256 = hashlib.new('sha256')

                    # NOTE(@ideasman42): There is more logic in the try/except block than I'd like.
                    # Refactoring could be done to avoid that but it ends up making logic difficult to follow.
                    try:
                        with open(filepath_local_cache_archive, "wb") as fh_cache:
                            for block in url_retrieve_to_data_iter_or_filesystem(
                                    filepath_remote_archive,
                                    headers=url_request_headers_create(
                                        accept_json=False,
                                        user_agent=online_user_agent,
                                        access_token=access_token,
                                    ),
                                    chunk_size=CHUNK_SIZE_DEFAULT,
                                    timeout_in_seconds=timeout_in_seconds,
                                    retrieve_info=DataRetrieveInfo(),  # Unused.
                            ):
                                request_exit |= msglog.progress(
                                    "Downloading \"{:s}\"".format(pkg_idname),
                                    filename_archive_size_test,
                                    archive_size_expected,
                                    'BYTE',
                                )
                                if request_exit:
                                    break
                                fh_cache.write(block)
                                sha256.update(block)
                                filename_archive_size_test += len(block)

                    except (Exception, KeyboardInterrupt) as ex:
                        # NOTE: don't support `demote_connection_errors_to_status` here because a connection
                        # failure on installing *is* an error by definition.
                        # Unlike querying information which might reasonably be skipped.
                        msglog.fatal_error(
                            url_retrieve_exception_as_message(
                                ex, prefix="install", url=filepath_remote_archive))
                        return False

                    if request_exit:
                        return False

                    # Validate:
                    if filename_archive_size_test != archive_size_expected:
                        msglog.fatal_error("Archive size mismatch \"{:s}\", expected {:d}, was {:d}".format(
                            pkg_idname,
                            archive_size_expected,
                            filename_archive_size_test,
                        ))
                        return False
                    filename_archive_hash_test = "sha256:" + sha256.hexdigest()
                    if filename_archive_hash_test != archive_hash_expected:
                        msglog.fatal_error("Archive checksum mismatch \"{:s}\", expected {:s}, was {:s}".format(
                            pkg_idname,
                            archive_hash_expected,
                            filename_archive_hash_test,
                        ))
                        return False
                    del filename_archive_size_test
                    del filename_archive_hash_test
                del found
                del filepath_local_cache_archive

            # All packages have been downloaded, install them.
            for manifest_archive in packages_info:
                filepath_local_cache_archive = os.path.join(local_cache_dir, manifest_archive.manifest.id + PKG_EXT)

                if not subcmd_client._install_package_from_file_impl(
                        msglog,
                        local_dir=local_dir,
                        filepath_archive=filepath_local_cache_archive,
                        blender_version_tuple=blender_version_tuple,
                        python_version_tuple=python_version_tuple,
                        manifest_compare=manifest_archive.manifest,
                        temp_prefix_and_suffix=temp_prefix_and_suffix,
                ):
                    # The package failed to install.
                    continue

        return True

    @staticmethod
    def uninstall_packages(
            msglog: MessageLogger,
            *,
            local_dir: str,
            user_dir: str,
            packages: Sequence[str],
            temp_prefix_and_suffix: tuple[str, str],
    ) -> bool:
        if not os.path.isdir(local_dir):
            msglog.fatal_error("Missing local \"{:s}\"".format(local_dir))
            return False

        # Ensure a private directory so a local cache can be created.
        # TODO: don't create (it's only accessed for file removal).
        if (local_cache_dir := repo_local_private_dir_ensure_with_subdir(
                local_dir=local_dir,
                subdir="cache",
                error_fn=lambda ex: any_as_none(
                    msglog.fatal_error("Error creating cache directory: {:s}".format(str(ex)))
                ),
        )) is None:
            return False

        # Most likely this doesn't have duplicates,but any errors procured by duplicates
        # are likely to be obtuse enough that it's better to guarantee there are none.
        packages = tuple(sorted(set(packages)))

        packages_valid = []

        has_fatal_error = False
        for pkg_idname in packages:
            # As this simply removes the directories right now,
            # validate this path cannot be used for an unexpected outcome,
            # or using `../../` to remove directories that shouldn't.
            if (pkg_idname in {"", ".", ".."}) or ("\\" in pkg_idname or "/" in pkg_idname):
                msglog.fatal_error("Package name invalid \"{:s}\"".format(pkg_idname))
                has_fatal_error = True
                continue

            # This will be a directory.
            filepath_local_pkg = os.path.join(local_dir, pkg_idname)
            if not os.path.isdir(filepath_local_pkg):
                msglog.fatal_error("Package not found \"{:s}\"".format(pkg_idname))
                has_fatal_error = True
                continue

            packages_valid.append(pkg_idname)
        del filepath_local_pkg

        if has_fatal_error:
            return False

        files_to_clean: list[str] = []
        with CleanupPathsContext(files=files_to_clean, directories=()):
            for pkg_idname in packages_valid:
                filepath_local_pkg = os.path.join(local_dir, pkg_idname)

                # First try and rename which will fail on WIN32 when one of the files is locked.

                if (error := rmtree_with_fallback_or_error_pseudo_atomic(
                        filepath_local_pkg,
                        temp_prefix_and_suffix=temp_prefix_and_suffix,
                )) is not None:
                    msglog.error("Failure to remove \"{:s}\" with error ({:s})".format(pkg_idname, error))
                    continue

                msglog.status("Removed \"{:s}\"".format(pkg_idname))

                filepath_local_cache_archive = os.path.join(local_cache_dir, pkg_idname + PKG_EXT)
                if os.path.exists(filepath_local_cache_archive):
                    files_to_clean.append(filepath_local_cache_archive)

                if user_dir:
                    filepath_user_pkg = os.path.join(user_dir, pkg_idname)
                    if os.path.exists(filepath_user_pkg):
                        if (error := rmtree_with_fallback_or_error_pseudo_atomic(
                                filepath_user_pkg,
                                temp_prefix_and_suffix=temp_prefix_and_suffix,
                        )) is not None:
                            msglog.error(
                                "Failure to remove \"{:s}\" user files with error ({:s})".format(pkg_idname, error),
                            )
                        else:
                            msglog.status("Removed cache \"{:s}\"".format(pkg_idname))

        return True


class subcmd_author:

    @staticmethod
    def build(
            msglog: MessageLogger,
            *,
            pkg_source_dir: str,
            pkg_output_dir: str,
            pkg_output_filepath: str,
            split_platforms: bool,
            valid_tags_filepath: str,
            verbose: bool,
    ) -> bool:
        if not os.path.isdir(pkg_source_dir):
            msglog.fatal_error("Missing local \"{:s}\"".format(pkg_source_dir))
            return False

        if pkg_output_dir != "." and pkg_output_filepath != "":
            msglog.fatal_error("Both output directory & output filepath set, set one or the other")
            return False

        pkg_manifest_filepath = os.path.join(pkg_source_dir, PKG_MANIFEST_FILENAME_TOML)

        if not os.path.exists(pkg_manifest_filepath):
            msglog.fatal_error("File \"{:s}\" not found!".format(pkg_manifest_filepath))
            return False

        # TODO: don't use this line, because the build information needs to be extracted too.
        # This should be refactored so the manifest could *optionally* load `[build]` info.
        # `manifest = pkg_manifest_from_toml_and_validate_all_errors(pkg_manifest_filepath, strict=True)`
        try:
            with open(pkg_manifest_filepath, "rb") as fh:
                manifest_data = tomllib.load(fh)
        except Exception as ex:
            msglog.fatal_error("Error parsing TOML \"{:s}\" {:s}".format(pkg_manifest_filepath, str(ex)))
            return False

        manifest = pkg_manifest_from_dict_and_validate_all_errros(manifest_data, from_repo=False, strict=True)
        if isinstance(manifest, list):
            for error_msg in manifest:
                msglog.fatal_error("Error parsing TOML \"{:s}\" {:s}".format(pkg_manifest_filepath, error_msg))
            return False

        if split_platforms:
            # NOTE: while this could be made into a warning which disables `split_platforms`,
            # this could result in further problems for automated tasks which operate on the output
            # where they would expect a platform suffix on each archive. So consider this an error.
            if not manifest.platforms:
                msglog.fatal_error(
                    "Error in arguments \"--split-platforms\" with a manifest that does not declare \"platforms\"",
                )
                return False

        if valid_tags_filepath:
            if subcmd_author._validate_tags(
                    msglog,
                    manifest=manifest,
                    pkg_manifest_filepath=pkg_manifest_filepath,
                    valid_tags_filepath=valid_tags_filepath,
            ) is False:
                return False

        if (manifest_build_data := manifest_data.get("build")) is not None:
            if "generated" in manifest_build_data:
                msglog.fatal_error(
                    "Error in TOML \"{:s}\" contains reserved value: [build.generated]".format(pkg_manifest_filepath),
                )
                return False

        # Always include wheels & manifest.
        build_paths_extra = (
            # Inclusion of the manifest is implicit.
            # No need to require the manifest to include itself.
            PKG_MANIFEST_FILENAME_TOML,
            *(manifest.wheels or ()),
        )
        build_paths_wheel_range = 1, 1 + len(manifest.wheels or ())
        # Exclude when checking file listing because the extra paths are mixed with user defined paths,
        # and including the manifest raises an error.
        build_paths_extra_skip_index = 1

        if manifest_build_data is not None:
            manifest_build_test = PkgManifest_Build.from_dict_all_errors(
                manifest_build_data,
                extra_paths=build_paths_extra[build_paths_extra_skip_index:],
            )
            if isinstance(manifest_build_test, list):
                for error_msg in manifest_build_test:
                    msglog.fatal_error(
                        "Error parsing TOML \"{:s}\" {:s}".format(pkg_manifest_filepath, error_msg)
                    )
                return False
            manifest_build = manifest_build_test
            del manifest_build_test
        else:
            # Make default build options if none are provided.
            manifest_build = PkgManifest_Build(
                paths=None,
                # Limit exclusions to:
                # - Python cache since extensions are written in Python.
                # - Dot-files since this is standard *enough*.
                # - ZIP archives to exclude packages that have been build.
                # - BLEND file backups since this is for Blender extensions,
                #   it makes sense to skip them.
                #
                # Further, it's not the purpose of this exclusion list to support all known file-system lint,
                # as it changes over time and *could* result in false positives.
                #
                # Extension authors are expected to declare exclude patterns based on their development environment.
                paths_exclude_pattern=[
                    "__pycache__/",
                    # Hidden dot-files.
                    ".*",
                    # Any packages built in-source.
                    "/*.zip",
                    # Backup `.blend` files.
                    "*.blend[1-9]",
                ],
            )

        del manifest_build_data, manifest_data

        build_paths_exclude_pattern: PathPatternMatch | None = None
        if manifest_build.paths_exclude_pattern is not None:
            build_paths_exclude_pattern = PathPatternMatch(manifest_build.paths_exclude_pattern)

        build_paths: list[tuple[str, str]] = []

        # Manifest & wheels.
        if build_paths_extra:
            build_paths.extend(build_paths_expand_iter(
                pkg_source_dir,
                # When "paths" is set, paths after `build_paths_extra_skip_index` have been added,
                # see: `PkgManifest_Build.from_dict_all_errors`.
                build_paths_extra if manifest_build.paths is None else
                build_paths_extra[:build_paths_extra_skip_index],
            ))

        if manifest_build.paths is not None:
            build_paths.extend(build_paths_expand_iter(pkg_source_dir, manifest_build.paths))
        else:
            # Mixing literal and pattern matched lists of files is a hassle.
            # De-duplicate canonical root-relative path names.
            def filepath_canonical_from_relative(filepath_rel: str) -> str:
                filepath_rel = os.path.normpath(filepath_rel)
                if os.sep == "\\":
                    filepath_rel = filepath_rel.replace("\\", "/")
                return filepath_rel

            # Use lowercase to prevent duplicates on MS-Windows.
            build_paths_extra_canonical: set[str] = set(
                filepath_canonical_from_relative(f).lower()
                for f in build_paths_extra
            )

            # Scanning the file-system may fail, surround by try/except.
            try:
                if build_paths_exclude_pattern:
                    def scandir_filter_with_paths_exclude_pattern(filepath: str, is_dir: bool) -> bool:
                        # Returning true includes the file.
                        assert build_paths_exclude_pattern is not None
                        if os.sep == "\\":
                            filepath = filepath.replace("\\", "/")
                        filepath_canonical = filepath
                        if is_dir:
                            assert not filepath.endswith("/")
                            filepath = filepath + "/"
                        assert not filepath.startswith(("/", "./", "../"))
                        result = not build_paths_exclude_pattern.test_path(filepath)
                        if result and (not is_dir):
                            # Finally check the path isn't one of the known paths.
                            if filepath_canonical.lower() in build_paths_extra_canonical:
                                result = False
                        return result

                    build_paths.extend(
                        scandir_recursive(
                            pkg_source_dir,
                            filter_fn=scandir_filter_with_paths_exclude_pattern,
                        ),
                    )
                else:
                    # In this case there isn't really a good option, just ignore all dot-files.
                    def scandir_filter_fallback(filepath: str, is_dir: bool) -> bool:
                        # Returning true includes the file.
                        result = not os.path.basename(filepath).startswith(".")
                        if result and (not is_dir):
                            # Finally check the path isn't one of the known paths.
                            if filepath_canonical_from_relative(filepath).lower() in build_paths_extra_canonical:
                                result = False
                        return result

                    build_paths.extend(scandir_recursive(pkg_source_dir, filter_fn=scandir_filter_fallback))

                del build_paths_extra_canonical

            except Exception as ex:
                msglog.fatal_error("Error building path list \"{:s}\"".format(str(ex)))
                return False

        if manifest.type == "add-on":
            # We could have a more generic way to find expected files,
            # for now perform specific checks.
            is_valid_python_package = False
            for _, filepath_rel in build_paths:
                # Other Python module suffixes besides `.py` can be used.
                if filepath_rel.startswith("__init__."):
                    is_valid_python_package = True
                    break
            if not is_valid_python_package:
                msglog.fatal_error("Not a Python package: missing \"__init__.*\" file, typically \"__init__.py\"")
                return False
            del is_valid_python_package

        request_exit = False

        # A pass-through when there are no platforms to split.
        for build_paths_for_platform, platform in build_paths_filter_by_platform(
            build_paths,
            build_paths_wheel_range,
            tuple(manifest.platforms) if (split_platforms and manifest.platforms) else (),
        ):
            if pkg_output_filepath != "":
                # The directory may be empty, that is fine as join handles this correctly.
                pkg_dirpath, pkg_filename = os.path.split(pkg_output_filepath)

                if platform:
                    pkg_filename, pkg_filename_ext = os.path.splitext(pkg_filename)
                    pkg_filename = "{:s}-{:s}{:s}".format(
                        pkg_filename,
                        platform.replace("-", "_"),
                        pkg_filename_ext,
                    )
                    del pkg_filename_ext
                    outfile = os.path.join(pkg_dirpath, pkg_filename)
                else:
                    outfile = pkg_output_filepath

                outfile_temp = os.path.join(pkg_dirpath, "." + pkg_filename)
                del pkg_dirpath
            else:
                if platform:
                    pkg_filename = "{:s}-{:s}-{:s}{:s}".format(
                        manifest.id,
                        manifest.version,
                        platform.replace("-", "_"),
                        PKG_EXT,
                    )
                else:
                    pkg_filename = "{:s}-{:s}{:s}".format(
                        manifest.id,
                        manifest.version,
                        PKG_EXT,
                    )
                outfile = os.path.join(pkg_output_dir, pkg_filename)
                outfile_temp = os.path.join(pkg_output_dir, "." + pkg_filename)

            request_exit |= msglog.status("building: {:s}".format(pkg_filename))
            if request_exit:
                return False

            with CleanupPathsContext(files=(outfile_temp,), directories=()):
                try:
                    # pylint: disable-next=consider-using-with
                    zip_fh_context = zipfile.ZipFile(outfile_temp, 'w', zipfile.ZIP_DEFLATED, compresslevel=9)
                except Exception as ex:
                    msglog.fatal_error("Error creating archive \"{:s}\"".format(str(ex)))
                    return False

                with contextlib.closing(zip_fh_context) as zip_fh:
                    for filepath_abs, filepath_rel in build_paths_for_platform:

                        zip_data_override: bytes | None = None
                        if platform and (filepath_rel == PKG_MANIFEST_FILENAME_TOML):
                            zip_data_override = b"".join((
                                b"\n",
                                b"\n",
                                b"# BEGIN GENERATED CONTENT.\n",
                                b"# This must not be included in source manifests.\n",
                                b"[build.generated]\n",
                                "platforms = [{:s}]\n".format(toml_repr_string(platform)).encode("utf-8"),
                                # Including wheels simplifies server side check as this list can be tested
                                # without the server having to filter by platform too.
                                b"wheels = [",
                                ", ".join([
                                    toml_repr_string(wheel) for wheel in paths_filter_wheels_by_platform(
                                        manifest.wheels or [],
                                        platform,
                                    )
                                ]).encode("utf-8"),
                                b"]\n"
                                b"# END GENERATED CONTENT.\n",
                            ))
                            try:
                                with open(filepath_abs, "rb") as temp_fh:
                                    zip_data_override = temp_fh.read() + zip_data_override
                            except Exception as ex:
                                msglog.fatal_error("Error overriding manifest \"{:s}\"".format(str(ex)))
                                return False

                        # Handy for testing that sub-directories:
                        # zip_fh.write(filepath_abs, manifest.id + "/" + filepath_rel)
                        compress_type = zipfile.ZIP_STORED if filepath_skip_compress(filepath_abs) else None
                        try:
                            if zip_data_override is not None:
                                zip_fh.writestr(filepath_rel, zip_data_override, compress_type=compress_type)
                            else:
                                zip_fh.write(filepath_abs, filepath_rel, compress_type=compress_type)
                        except FileNotFoundError:
                            msglog.fatal_error("Error adding to archive, file not found: \"{:s}\"".format(filepath_rel))
                            return False
                        except Exception as ex:
                            msglog.fatal_error("Error adding to archive \"{:s}\"".format(str(ex)))
                            return False

                        if verbose:
                            msglog.status("add: {:s}".format(filepath_rel))

                    request_exit |= msglog.status("complete")
                    if request_exit:
                        return False

                if os.path.exists(outfile):
                    os.unlink(outfile)
                os.rename(outfile_temp, outfile)

        msglog.status("created: \"{:s}\", {:d}".format(outfile, os.path.getsize(outfile)))
        return True

    @staticmethod
    def _validate_tags(
            msglog: MessageLogger,
            *,
            manifest: PkgManifest,
            # NOTE: This path is only for inclusion in the error message,
            # the path may not exist on the file-system (it may refer to a path inside an archive for example).
            pkg_manifest_filepath: str,
            valid_tags_filepath: str,
    ) -> bool:
        assert valid_tags_filepath
        if manifest.tags is not None:
            if isinstance(valid_tags_data := pkg_manifest_tags_load_valid_map(valid_tags_filepath), str):
                msglog.fatal_error(
                    "Error in TAGS \"{:s}\" loading tags: {:s}".format(valid_tags_filepath, valid_tags_data),
                )
                return False
            if (error := pkg_manifest_tags_valid_or_error(valid_tags_data, manifest.type, manifest.tags)) is not None:
                msglog.fatal_error((
                    "Error in TOML \"{:s}\" loading tags: {:s}\n"
                    "Either correct the tag or disable validation using an empty tags argument --valid-tags=\"\", "
                    "see --help text for details."
                ).format(pkg_manifest_filepath, error))
                return False
        return True

    @staticmethod
    def _validate_directory(
            msglog: MessageLogger,
            *,
            pkg_source_dir: str,
            valid_tags_filepath: str,
    ) -> bool:
        pkg_manifest_filepath = os.path.join(pkg_source_dir, PKG_MANIFEST_FILENAME_TOML)

        if not os.path.exists(pkg_manifest_filepath):
            msglog.fatal_error("Error, file \"{:s}\" not found!".format(pkg_manifest_filepath))
            return False

        # Demote errors to status as the function of this action is to check the manifest is stable.
        manifest = pkg_manifest_from_toml_and_validate_all_errors(pkg_manifest_filepath, strict=True)
        if isinstance(manifest, list):
            msglog.status("Error parsing TOML \"{:s}\"".format(pkg_manifest_filepath))
            for error_msg in manifest:
                msglog.status(error_msg)
            return False

        expected_files = []
        if manifest.type == "add-on":
            expected_files.append("__init__.py")
        ok = True
        for filepath in expected_files:
            if not os.path.exists(os.path.join(pkg_source_dir, filepath)):
                msglog.status("Error, file missing from {:s}: \"{:s}\"".format(
                    manifest.type,
                    filepath,
                ))
                ok = False
        if not ok:
            return False

        if valid_tags_filepath:
            if subcmd_author._validate_tags(
                    msglog,
                    manifest=manifest,
                    pkg_manifest_filepath=pkg_manifest_filepath,
                    valid_tags_filepath=valid_tags_filepath,
            ) is False:
                return False

        msglog.status("Success parsing TOML in \"{:s}\"".format(pkg_source_dir))
        return True

    @staticmethod
    def _validate_archive(
            msglog: MessageLogger,
            *,
            pkg_source_archive: str,
            valid_tags_filepath: str,
    ) -> bool:
        # NOTE(@ideasman42): having `_validate_directory` & `_validate_archive`
        # use separate code-paths isn't ideal in some respects however currently the difference
        # doesn't cause a lot of duplication.
        #
        # Operate on the archive directly because:
        # - Validating the manifest *does* use shared logic.
        # - It's faster for large archives or archives with a large number of files
        #   which will litter the file-system.
        # - There is already a validation function which is used before installing an archive.
        #
        # If it's ever causes too much code-duplication we can always
        # extract the archive into a temporary directory and run validation there.

        try:
            # pylint: disable-next=consider-using-with
            zip_fh_context = zipfile.ZipFile(pkg_source_archive, mode="r")
        except Exception as ex:
            msglog.status("Error extracting archive \"{:s}\"".format(str(ex)))
            return False

        with contextlib.closing(zip_fh_context) as zip_fh:
            if (archive_subdir := pkg_zipfile_detect_subdir_or_none(zip_fh)) is None:
                msglog.fatal_error("Error, archive has no manifest: \"{:s}\"".format(PKG_MANIFEST_FILENAME_TOML))
                return False
            # Demote errors to status as the function of this action is to check the manifest is stable.
            manifest = pkg_manifest_from_zipfile_and_validate_all_errors(zip_fh, archive_subdir, strict=True)
            if isinstance(manifest, list):
                msglog.fatal_error("Error parsing TOML in \"{:s}\"".format(pkg_source_archive))
                for error_msg in manifest:
                    msglog.fatal_error(error_msg)
                return False

            if valid_tags_filepath:
                if subcmd_author._validate_tags(
                        msglog,
                        manifest=manifest,
                        # Only for the error message, use the ZIP relative path.
                        pkg_manifest_filepath=(
                            "{:s}/{:s}".format(archive_subdir, PKG_MANIFEST_FILENAME_TOML) if archive_subdir else
                            PKG_MANIFEST_FILENAME_TOML
                        ),
                        valid_tags_filepath=valid_tags_filepath,
                ) is False:
                    return False

            # NOTE: this is arguably *not* manifest validation, the check could be refactored out.
            # Currently we always want to check both and it's useful to do that while the informatio
            expected_files = []
            if manifest.type == "add-on":
                if archive_subdir:
                    assert archive_subdir.endswith("/")
                    expected_files.append(archive_subdir + "__init__.py")
                else:
                    expected_files.append("__init__.py")
            ok = True
            for filepath in expected_files:
                if zip_fh.NameToInfo.get(filepath) is None:
                    msglog.fatal_error("Error, file missing from {:s}: \"{:s}\"".format(
                        manifest.type,
                        filepath,
                    ))
                    ok = False
            if not ok:
                return False

        msglog.status("Success parsing TOML in \"{:s}\"".format(pkg_source_archive))
        return True

    @staticmethod
    def validate(
            msglog: MessageLogger,
            *,
            source_path: str,
            valid_tags_filepath: str,
    ) -> bool:
        if os.path.isdir(source_path):
            result = subcmd_author._validate_directory(
                msglog,
                pkg_source_dir=source_path,
                valid_tags_filepath=valid_tags_filepath,
            )
        else:
            result = subcmd_author._validate_archive(
                msglog,
                pkg_source_archive=source_path,
                valid_tags_filepath=valid_tags_filepath,
            )
        return result


class subcmd_dummy:

    @staticmethod
    def repo(
            msglog: MessageLogger,
            *,
            repo_dir: str,
            package_names: Sequence[str],
    ) -> bool:

        def msg_fn_no_done(ty: str, data: PrimTypeOrSeq) -> bool:
            if ty == 'DONE':
                return False
            return msglog.msg_fn(ty, data)

        # Ensure package names are valid.
        package_names = tuple(set(package_names))
        for pkg_idname in package_names:
            if (error_msg := pkg_idname_is_valid_or_error(pkg_idname)) is None:
                continue
            msglog.fatal_error(
                "key \"id\", \"{:s}\" doesn't match expected format, \"{:s}\"".format(pkg_idname, error_msg),
            )
            return False

        if url_has_known_prefix(repo_dir):
            msglog.fatal_error("Generating a repository on a remote path is not supported")
            return False

        # Unlike most other commands, create the repo_dir it doesn't already exist.
        if not os.path.exists(repo_dir):
            try:
                os.makedirs(repo_dir)
            except Exception as ex:
                msglog.fatal_error("Failed to create \"{:s}\" with error: {!r}".format(repo_dir, ex))
                return False

        import tempfile
        for pkg_idname in package_names:
            with tempfile.TemporaryDirectory(suffix="pkg-repo") as temp_dir_source:
                pkg_src_dir = os.path.join(temp_dir_source, pkg_idname)
                os.makedirs(pkg_src_dir)
                pkg_name = pkg_idname.replace("_", " ").title()
                with open(os.path.join(pkg_src_dir, PKG_MANIFEST_FILENAME_TOML), "w", encoding="utf-8") as fh:
                    fh.write("""# Example\n""")
                    fh.write("""schema_version = "1.0.0"\n""")
                    fh.write("""id = "{:s}"\n""".format(pkg_idname))
                    fh.write("""name = "{:s}"\n""".format(pkg_name))
                    fh.write("""type = "add-on"\n""")
                    fh.write("""tags = ["UV"]\n""")
                    fh.write("""maintainer = "Maintainer Name <username@addr.com>"\n""")
                    fh.write("""license = ["SPDX:GPL-2.0-or-later"]\n""")
                    fh.write("""version = "1.0.0"\n""")
                    fh.write("""tagline = "This is a tagline"\n""")
                    fh.write("""blender_version_min = "0.0.0"\n""")
                    fh.write("""[permissions]\n""")
                    for value in ("files", "network", "clipboard", "camera", "microphone"):
                        fh.write("""{:s} = "Example text"\n""".format(value))

                with open(os.path.join(pkg_src_dir, "__init__.py"), "w", encoding="utf-8") as fh:
                    fh.write("""
def register():
    print("Register:", __name__)

def unregister():
    print("Unregister:", __name__)
""")

                    fh.write("""# Dummy.\n""")
                    # Generate some random ASCII-data.
                    for i, line in enumerate(random_acii_lines(seed=pkg_idname, width=80)):
                        if i > 1000:
                            break
                        fh.write("# {:s}\n".format(line))

                # Write a sub-directory (check this is working).
                docs = os.path.join(pkg_src_dir, "docs")
                os.makedirs(docs)
                with open(os.path.join(docs, "readme.txt"), "w", encoding="utf-8") as fh:
                    fh.write("Example readme.")

                # `{cmd} build --pkg-source-dir {pkg_src_dir} --pkg-output-dir {repo_dir}`.
                if not subcmd_author.build(
                    MessageLogger(msg_fn_no_done),
                    pkg_source_dir=pkg_src_dir,
                    pkg_output_dir=repo_dir,
                    pkg_output_filepath="",
                    split_platforms=False,
                    valid_tags_filepath="",
                    verbose=False,
                ):
                    # Error running command.
                    return False

        # `{cmd} server-generate --repo-dir {repo_dir}`.
        if not subcmd_server.generate(
            MessageLogger(msg_fn_no_done),
            repo_dir=repo_dir,
            repo_config_filepath="",
            html=True,
            html_template="",
        ):
            # Error running command.
            return False

        msglog.done()
        return True

    @staticmethod
    def progress(
            msglog: MessageLogger,
            *,
            time_duration: float,
            time_delay: float,
            steps_limit: int,
    ) -> bool:
        import time
        request_exit = False
        time_start = time.time() if (time_duration > 0.0) else 0.0
        size_beg = 0
        size_end = steps_limit
        while time_duration == 0.0 or (time.time() - time_start < time_duration):
            request_exit |= msglog.progress("Demo", size_beg, size_end, 'BYTE')
            if request_exit:
                break
            size_beg += 1
            if size_beg > size_end:
                # Limit by the number of steps.
                if time_duration == 0.0:
                    break
                size_beg = 0
            time.sleep(time_delay)
        if request_exit:
            msglog.done()
            return False

        msglog.done()
        return True


# -----------------------------------------------------------------------------
# Server Manipulating Actions


def argparse_create_server_generate(
        subparsers: "argparse._SubParsersAction[argparse.ArgumentParser]",
        args_internal: bool,
) -> None:
    subparse = subparsers.add_parser(
        "server-generate",
        help="Create a listing from all packages.",
        description=(
            "Generate a listing of all packages stored in a directory.\n"
            "This can be used to host packages which only requires static-file hosting."
        ),
        formatter_class=argparse.RawTextHelpFormatter,
    )

    generic_arg_repo_dir(subparse)
    generic_arg_server_generate_repo_config(subparse)
    generic_arg_server_generate_html(subparse)
    generic_arg_server_generate_html_template(subparse)
    if args_internal:
        generic_arg_output_type(subparse)

    subparse.set_defaults(
        func=lambda args: subcmd_server.generate(
            msglog_from_args(args),
            repo_dir=args.repo_dir,
            repo_config_filepath=args.repo_config,
            html=args.html,
            html_template=args.html_template,
        ),
    )


# -----------------------------------------------------------------------------
# Client Queries

def argparse_create_client_list(subparsers: "argparse._SubParsersAction[argparse.ArgumentParser]") -> None:
    subparse = subparsers.add_parser(
        "list",
        help="List all available packages.",
        description="List all available packages.",
        formatter_class=argparse.RawTextHelpFormatter,
    )

    generic_arg_remote_url(subparse)
    generic_arg_local_dir(subparse)
    generic_arg_online_user_agent(subparse)
    generic_arg_access_token(subparse)

    generic_arg_output_type(subparse)
    generic_arg_timeout(subparse)
    generic_arg_demote_connection_failure_to_status(subparse)

    subparse.set_defaults(
        func=lambda args: subcmd_client.list_packages(
            msglog_from_args(args),
            remote_url=args.remote_url,
            online_user_agent=args.online_user_agent,
            access_token=args.access_token,
            timeout_in_seconds=args.timeout,
            demote_connection_errors_to_status=args.demote_connection_errors_to_status,
        ),
    )


# -----------------------------------------------------------------------------
# Client Manipulating Actions

def argparse_create_client_sync(subparsers: "argparse._SubParsersAction[argparse.ArgumentParser]") -> None:
    subparse = subparsers.add_parser(
        "sync",
        help="Refresh from the remote repository.",
        description="Refresh from remote repository (sync).",
        formatter_class=argparse.RawTextHelpFormatter,
    )

    generic_arg_remote_url(subparse)
    generic_arg_remote_name(subparse)
    generic_arg_local_dir(subparse)
    generic_arg_online_user_agent(subparse)
    generic_arg_access_token(subparse)

    generic_arg_output_type(subparse)
    generic_arg_timeout(subparse)
    generic_arg_ignore_broken_pipe(subparse)
    generic_arg_demote_connection_failure_to_status(subparse)
    generic_arg_extension_override(subparse)

    subparse.set_defaults(
        func=lambda args: subcmd_client.sync(
            msglog_from_args(args),
            remote_url=args.remote_url,
            remote_name=args.remote_name if args.remote_name else remote_url_params_strip(args.remote_url),
            local_dir=args.local_dir,
            online_user_agent=args.online_user_agent,
            access_token=args.access_token,
            timeout_in_seconds=args.timeout,
            demote_connection_errors_to_status=args.demote_connection_errors_to_status,
            force_exit_ok=args.force_exit_ok,
            extension_override=args.extension_override,
        ),
    )


def argparse_create_client_install_files(subparsers: "argparse._SubParsersAction[argparse.ArgumentParser]") -> None:
    subparse = subparsers.add_parser(
        "install-files",
        help="Install package from the file-system.",
        description="Install packages from the file-system.",
        formatter_class=argparse.RawTextHelpFormatter,
    )
    generic_arg_file_list_positional(subparse)

    generic_arg_local_dir(subparse)
    generic_arg_blender_version(subparse)
    generic_arg_python_version(subparse)

    generic_arg_temp_prefix_and_suffix(subparse)

    generic_arg_output_type(subparse)

    subparse.set_defaults(
        func=lambda args: subcmd_client.install_packages_from_files(
            msglog_from_args(args),
            local_dir=args.local_dir,
            package_files=args.files,
            blender_version=args.blender_version,
            python_version=args.python_version,
            temp_prefix_and_suffix=args.temp_prefix_and_suffix,
        ),
    )


def argparse_create_client_install(subparsers: "argparse._SubParsersAction[argparse.ArgumentParser]") -> None:
    subparse = subparsers.add_parser(
        "install",
        help="Install package.",
        description="Install the package.",
        formatter_class=argparse.RawTextHelpFormatter,
    )
    generic_arg_package_list_positional(subparse)

    generic_arg_remote_url(subparse)
    generic_arg_local_dir(subparse)
    generic_arg_local_cache(subparse)
    generic_arg_online_user_agent(subparse)
    generic_arg_blender_version(subparse)
    generic_arg_python_version(subparse)
    generic_arg_access_token(subparse)

    generic_arg_temp_prefix_and_suffix(subparse)

    generic_arg_output_type(subparse)
    generic_arg_timeout(subparse)

    subparse.set_defaults(
        func=lambda args: subcmd_client.install_packages(
            msglog_from_args(args),
            remote_url=args.remote_url,
            local_dir=args.local_dir,
            local_cache=args.local_cache,
            packages=args.packages.split(","),
            online_user_agent=args.online_user_agent,
            blender_version=args.blender_version,
            python_version=args.python_version,
            access_token=args.access_token,
            timeout_in_seconds=args.timeout,
            temp_prefix_and_suffix=args.temp_prefix_and_suffix,
        ),
    )


def argparse_create_client_uninstall(subparsers: "argparse._SubParsersAction[argparse.ArgumentParser]") -> None:
    subparse = subparsers.add_parser(
        "uninstall",
        help="Uninstall a package.",
        description="Uninstall the package.",
        formatter_class=argparse.RawTextHelpFormatter,
    )
    generic_arg_package_list_positional(subparse)

    generic_arg_local_dir(subparse)
    generic_arg_user_dir(subparse)

    generic_arg_temp_prefix_and_suffix(subparse)

    generic_arg_output_type(subparse)

    subparse.set_defaults(
        func=lambda args: subcmd_client.uninstall_packages(
            msglog_from_args(args),
            local_dir=args.local_dir,
            user_dir=args.user_dir,
            packages=args.packages.split(","),
            temp_prefix_and_suffix=args.temp_prefix_and_suffix,
        ),
    )


# -----------------------------------------------------------------------------
# Authoring Actions

def argparse_create_author_build(
        subparsers: "argparse._SubParsersAction[argparse.ArgumentParser]",
        args_internal: bool,
) -> None:
    subparse = subparsers.add_parser(
        "build",
        help="Build a package.",
        description="Build a package in the current directory.",
        formatter_class=argparse.RawTextHelpFormatter,
    )

    generic_arg_package_source_dir(subparse)
    generic_arg_package_output_dir(subparse)
    generic_arg_package_output_filepath(subparse)
    generic_arg_package_valid_tags(subparse)
    generic_arg_build_split_platforms(subparse)
    generic_arg_verbose(subparse)

    if args_internal:
        generic_arg_output_type(subparse)

    subparse.set_defaults(
        func=lambda args: subcmd_author.build(
            msglog_from_args(args),
            pkg_source_dir=args.source_dir,
            pkg_output_dir=args.output_dir,
            pkg_output_filepath=args.output_filepath,
            split_platforms=args.split_platforms,
            valid_tags_filepath=args.valid_tags_filepath,
            verbose=args.verbose,
        ),
    )


def argparse_create_author_validate(
        subparsers: "argparse._SubParsersAction[argparse.ArgumentParser]",
        args_internal: bool,
) -> None:
    subparse = subparsers.add_parser(
        "validate",
        help="Validate a package.",
        description="Validate the package meta-data in the current directory.",
        formatter_class=argparse.RawTextHelpFormatter,
    )
    generic_arg_package_source_path_positional(subparse)
    generic_arg_package_valid_tags(subparse)

    if args_internal:
        generic_arg_output_type(subparse)

    subparse.set_defaults(
        func=lambda args: subcmd_author.validate(
            msglog_from_args(args),
            source_path=args.source_path,
            valid_tags_filepath=args.valid_tags_filepath,
        ),
    )


# -----------------------------------------------------------------------------
# Dummy Repo


def argparse_create_dummy_repo(subparsers: "argparse._SubParsersAction[argparse.ArgumentParser]") -> None:
    subparse = subparsers.add_parser(
        "dummy-repo",
        help="Create a dummy repository.",
        description="Create a dummy repository, intended for testing.",
        formatter_class=argparse.RawTextHelpFormatter,
    )

    subparse.add_argument(
        "--package-names",
        dest="package_names",
        type=arg_handle_str_as_package_names,
        help=(
            "Comma separated list of package names to create (no-spaces)."
        ),
        required=True,
    )

    generic_arg_output_type(subparse)
    generic_arg_repo_dir(subparse)

    subparse.set_defaults(
        func=lambda args: subcmd_dummy.repo(
            msglog_from_args(args),
            repo_dir=args.repo_dir,
            package_names=args.package_names,
        ),
    )


# -----------------------------------------------------------------------------
# Dummy Output

def argparse_create_dummy_progress(subparsers: "argparse._SubParsersAction[argparse.ArgumentParser]") -> None:
    subparse = subparsers.add_parser(
        "dummy-progress",
        help="Dummy progress output.",
        description="Demo output, included for testing.",
        formatter_class=argparse.RawTextHelpFormatter,
    )

    subparse.add_argument(
        "--time-duration",
        dest="time_duration",
        type=float,
        help=(
            "How long to run the demo for (zero to run forever)."
        ),
        default=0.0,
    )
    subparse.add_argument(
        "--time-delay",
        dest="time_delay",
        type=float,
        help=(
            "Delay between updates."
        ),
        default=0.05,
    )

    subparse.add_argument(
        "--steps-limit",
        dest="steps_limit",
        type=int,
        help=(
            "The number of steps to report"
        ),
        default=100,
    )

    generic_arg_output_type(subparse)

    subparse.set_defaults(
        func=lambda args: subcmd_dummy.progress(
            msglog_from_args(args),
            time_duration=args.time_duration,
            time_delay=args.time_delay,
            steps_limit=max(1, args.steps_limit),
        ),
    )


# -----------------------------------------------------------------------------
# Top Level Argument Parser

def argparse_create(
        args_internal: bool = True,
        args_extra_subcommands_fn: ArgsSubparseFn | None = None,
        prog: str | None = None,
) -> argparse.ArgumentParser:

    parser = argparse.ArgumentParser(
        prog=prog or "blender_ext",
        description=__doc__,
        formatter_class=argparse.RawTextHelpFormatter,
    )

    subparsers = parser.add_subparsers(
        title="subcommands",
        description="",
        help="",
    )

    argparse_create_server_generate(subparsers, args_internal)

    if args_internal:
        # Queries.
        argparse_create_client_list(subparsers)

        # Manipulating Actions.
        argparse_create_client_sync(subparsers)
        argparse_create_client_install_files(subparsers)
        argparse_create_client_install(subparsers)
        argparse_create_client_uninstall(subparsers)

        # Dummy commands.
        argparse_create_dummy_repo(subparsers)
        argparse_create_dummy_progress(subparsers)

    # Authoring Commands.
    argparse_create_author_build(subparsers, args_internal)
    argparse_create_author_validate(subparsers, args_internal)

    if args_extra_subcommands_fn is not None:
        args_extra_subcommands_fn(subparsers)

    return parser


# -----------------------------------------------------------------------------
# Message Printing Functions

# Follows `MessageFn` convention.
def msg_print_text(ty: str, data: PrimTypeOrSeq) -> bool:
    assert ty in MESSAGE_TYPES

    if isinstance(data, (list, tuple)):
        data_str = ", ".join(str(x) for x in data)
    else:
        data_str = str(data)

    # Don't prefix status as it's noise for users.
    if ty == 'STATUS':
        sys.stdout.write("{:s}\n".format(data_str))
    else:
        sys.stdout.write("{:s}: {:s}\n".format(ty, data_str))

    return REQUEST_EXIT


def msg_print_json_impl(ty: str, data: PrimTypeOrSeq) -> bool:
    assert ty in MESSAGE_TYPES
    sys.stdout.write(json.dumps([ty, data]))
    return REQUEST_EXIT


def msg_print_json(ty: str, data: PrimTypeOrSeq) -> bool:
    msg_print_json_impl(ty, data)
    sys.stdout.write("\n")
    sys.stdout.flush()
    return REQUEST_EXIT


def msg_print_json_0(ty: str, data: PrimTypeOrSeq) -> bool:
    msg_print_json_impl(ty, data)
    sys.stdout.write("\0")
    sys.stdout.flush()
    return REQUEST_EXIT


def msglog_from_args(args: argparse.Namespace) -> MessageLogger:
    # Will be None when running form Blender.
    output_type = getattr(args, "output_type", 'TEXT')

    match output_type:
        case 'JSON':
            return MessageLogger(msg_print_json)
        case 'JSON_0':
            return MessageLogger(msg_print_json_0)
        case 'TEXT':
            return MessageLogger(msg_print_text)

    raise Exception("Unknown output!")


# -----------------------------------------------------------------------------
# Main Function

def main(
        argv: list[str] | None = None,
        args_internal: bool = True,
        args_extra_subcommands_fn: ArgsSubparseFn | None = None,
        prog: str | None = None,
) -> int:
    # NOTE: only manipulate Python's run-time such as encoding & SIGINT when running stand-alone.

    # Run early to prevent a `KeyboardInterrupt` exception.
    signal.signal(signal.SIGINT, signal_handler_sigint)
    if sys.platform == "win32":
        # WIN32 needs to check for break as sending SIGINT isn't supported from the caller, see #131947.
        signal.signal(signal.SIGBREAK, signal_handler_sigint)

    # Needed on WIN32 which doesn't default to `utf-8`.
    for fh in (sys.stdout, sys.stderr):
        # While this is typically the case, is only guaranteed to be `TextIO` so check `reconfigure` is available.
        if not isinstance(fh, io.TextIOWrapper):
            continue
        # pylint: disable-next=no-member; False positive.
        if fh.encoding.lower().partition(":")[0] == "utf-8":
            continue
        fh.reconfigure(encoding="utf-8")

    if "--version" in sys.argv:
        sys.stdout.write("{:s}\n".format(VERSION))
        return 0

    if (sys.platform == "win32") and (sys.version_info < (3, 12, 6)):
        _worlaround_win32_ssl_cert_failure()

    parser = argparse_create(
        args_internal=args_internal,
        args_extra_subcommands_fn=args_extra_subcommands_fn,
        prog=prog,
    )
    args = parser.parse_args(argv)
    # Call sub-parser callback.
    if not hasattr(args, "func"):
        parser.print_help()
        return 0

    result = args.func(args)
    assert isinstance(result, bool)
    return 0 if result else 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except BrokenPipeError as ex:
        # When called for notifications, a broken pipe may occur if the caller closes soon after activating.
        # In this case a broken pipe is expected and not something we want to avoid.
        # This most only ever be set if canceling will *not* leave the repository in a corrupt sate.
        if not FORCE_EXIT_OK:
            raise ex
