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
    Dict,
    Generator,
    IO,
    Optional,
    Sequence,
    List,
    Set,
    Tuple,
    Callable,
    NamedTuple,
    Union,
)

ArgsSubparseFn = Callable[["argparse._SubParsersAction[argparse.ArgumentParser]"], None]

REQUEST_EXIT = False

# When set, ignore broken pipe exceptions (these occur when the calling processes is closed).
FORCE_EXIT_OK = False


def signal_handler_sigint(_sig: int, _frame: Any) -> None:
    # pylint: disable-next=global-statement
    global REQUEST_EXIT
    REQUEST_EXIT = True


signal.signal(signal.SIGINT, signal_handler_sigint)


# A primitive type that can be communicated via message passing.
PrimType = Union[int, str]
PrimTypeOrSeq = Union[PrimType, Sequence[PrimType]]

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

MESSAGE_TYPES = {'STATUS', 'PROGRESS', 'WARN', 'ERROR', 'PATH', 'DONE'}

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

# Progress updates are displayed after each chunk of this size is downloaded.
# Small values add unnecessary overhead showing progress, large values will make
# progress not update often enough.
#
# Note that this could be dynamic although it's not a priority.
#
# 16kb to be responsive even on slow connections.
CHUNK_SIZE_DEFAULT = 1 << 14

# Standard out may be communicating with a parent process,
# arbitrary prints are NOT acceptable.


# pylint: disable-next=redefined-builtin
def print(*args: Any, **kw: Dict[str, Any]) -> None:
    raise Exception("Illegal print(*({!r}), **{{{!r}}})".format(args, kw))


def debug_stack_trace_to_file() -> None:
    """
    Debugging.
    """
    import inspect
    stack = inspect.stack(context=1)
    with open("/tmp/out.txt", "w") as fh:
        for frame_info in stack[1:]:
            fh.write("{:s}:{:d}: {:s}\n".format(
                frame_info.filename,
                frame_info.lineno,
                frame_info.function,
            ))


def message_done(msg_fn: MessageFn) -> bool:
    """
    Print a non-fatal warning.
    """
    return msg_fn("DONE", "")


def message_warn(msg_fn: MessageFn, s: str) -> bool:
    """
    Print a non-fatal warning.
    """
    return msg_fn("WARN", s)


def message_error(msg_fn: MessageFn, s: str) -> bool:
    """
    Print a fatal error.
    """
    return msg_fn("ERROR", s)


def message_status(msg_fn: MessageFn, s: str) -> bool:
    """
    Print a status message.
    """
    return msg_fn("STATUS", s)


def message_path(msg_fn: MessageFn, s: str) -> bool:
    """
    Print a path.
    """
    return msg_fn("PATH", s)


def message_progress(msg_fn: MessageFn, s: str, progress: int, progress_range: int, unit: str) -> bool:
    """
    Print a progress update.
    """
    assert unit == 'BYTE'
    return msg_fn("PROGRESS", (s, unit, progress, progress_range))


def force_exit_ok_enable() -> None:
    global FORCE_EXIT_OK
    FORCE_EXIT_OK = True
    # Without this, some errors are printed on exit.
    sys.unraisablehook = lambda _ex: None


# -----------------------------------------------------------------------------
# Generic Functions

def read_with_timeout(fh: IO[bytes], size: int, *, timeout_in_seconds: float) -> Optional[bytes]:
    # TODO: implement timeout (TimeoutError).
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
    blocklist: List[str]
    data: List[Dict[str, Any]]


class PkgManifest(NamedTuple):
    """Package Information."""
    schema_version: str
    id: str
    name: str
    tagline: str
    version: str
    type: str
    maintainer: str
    license: List[str]
    blender_version_min: str

    # Optional (set all defaults).
    blender_version_max: Optional[str] = None
    website: Optional[str] = None
    copyright: Optional[List[str]] = None
    permissions: Optional[List[str]] = None
    tags: Optional[List[str]] = None
    wheels: Optional[List[str]] = None


class PkgManifest_Archive(NamedTuple):
    """Package Information with archive information."""
    # NOTE: no support for default values (unlike `PkgManifest`).
    manifest: PkgManifest
    archive_size: int
    archive_hash: str
    archive_url: str


# -----------------------------------------------------------------------------
# Generic Functions


def path_to_url(path: str) -> str:
    from urllib.parse import urljoin
    from urllib.request import pathname2url
    return urljoin("file:", pathname2url(path))


def path_from_url(path: str) -> str:
    from urllib.parse import urlparse, unquote
    p = urlparse(path)
    return os.path.join(p.netloc, unquote(p.path))


def random_acii_lines(*, seed: Union[int, str], width: int) -> Generator[str, None, None]:
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


def sha256_from_file(filepath: str, block_size: int = 1 << 20, hash_prefix: bool = False) -> Tuple[int, str]:
    """
    Returns an arbitrary sized unique ASCII string based on the file contents.
    (exact hashing method may change).
    """
    with open(filepath, 'rb') as fh:
        size = 0
        sha256 = hashlib.new('sha256')
        while True:
            data = fh.read(block_size)
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
        filter_fn: Callable[[str], bool],
) -> Generator[Tuple[str, str], None, None]:
    """Recursively yield DirEntry objects for given directory."""
    for entry in os.scandir(path):
        if entry.is_symlink():
            continue

        entry_path = entry.path
        entry_path_relateive = os.path.relpath(entry_path, base_path)

        if not filter_fn(entry_path_relateive):
            continue

        if entry.is_dir():
            yield from scandir_recursive_impl(
                base_path,
                entry_path,
                filter_fn=filter_fn,
            )
        elif entry.is_file():
            yield entry_path, entry_path_relateive


def scandir_recursive(
        path: str,
        filter_fn: Callable[[str], bool],
) -> Generator[Tuple[str, str], None, None]:
    yield from scandir_recursive_impl(path, path, filter_fn=filter_fn)


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
        data: Dict[Any, Any],
        *,
        from_repo: bool,
        all_errors: bool,
        strict: bool,
) -> Union[PkgManifest, List[str]]:
    error_list = []
    # Validate the dictionary.
    if all_errors:
        if (x := pkg_manifest_is_valid_or_error_all(data, from_repo=from_repo, strict=strict)) is not None:
            error_list.extend(x)
    else:
        if (error_msg := pkg_manifest_is_valid_or_error(data, from_repo=from_repo, strict=strict)) is not None:
            error_list.append(error_msg)
            if not all_errors:
                return error_list

    values: List[str] = []
    for key in PkgManifest._fields:
        val = data.get(key, ...)
        if val is ...:
            val = PkgManifest._field_defaults.get(key, ...)
        # `pkg_manifest_is_valid_or_error{_all}` will have caught this, assert all the same.
        assert val is not ...
        values.append(val)

    kw_args: Dict[str, Any] = dict(zip(PkgManifest._fields, values, strict=True))
    manifest = PkgManifest(**kw_args)

    if error_list:
        assert all_errors
        return error_list

    # There could be other validation, leave these as-is.
    return manifest


def pkg_manifest_from_dict_and_validate(
        data: Dict[Any, Any],
        from_repo: bool,
        strict: bool,
) -> Union[PkgManifest, str]:
    manifest = pkg_manifest_from_dict_and_validate_impl(data, from_repo=from_repo, all_errors=False, strict=strict)
    if isinstance(manifest, list):
        return manifest[0]
    return manifest


def pkg_manifest_from_dict_and_validate_all_errros(
        data: Dict[Any, Any],
        from_repo: bool,
        strict: bool,
) -> Union[PkgManifest, List[str]]:
    """
    Validate the manifest and return all errors.
    """
    return pkg_manifest_from_dict_and_validate_impl(data, from_repo=from_repo, all_errors=True, strict=strict)


def pkg_manifest_archive_from_dict_and_validate(
        data: Dict[Any, Any],
        strict: bool,
) -> Union[PkgManifest_Archive, str]:
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
) -> Union[PkgManifest, List[str]]:
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
) -> Optional[str]:
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
) -> Union[PkgManifest, List[str]]:
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

    manifest_dict = toml_from_bytes(file_content)
    assert isinstance(manifest_dict, dict)

    # TODO: forward actual error.
    if manifest_dict is None:
        return ["Archive does not contain a manifest"]
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
) -> Union[PkgManifest, str]:
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
) -> Union[PkgManifest, List[str]]:
    return pkg_manifest_from_zipfile_and_validate_impl(
        zip_fh,
        archive_subdir,
        all_errors=True,
        strict=strict,
    )


def pkg_manifest_from_archive_and_validate(
        filepath: str,
        strict: bool,
) -> Union[PkgManifest, str]:
    try:
        zip_fh_context = zipfile.ZipFile(filepath, mode="r")
    except BaseException as ex:
        return "Error extracting archive \"{:s}\"".format(str(ex))

    with contextlib.closing(zip_fh_context) as zip_fh:
        if (archive_subdir := pkg_zipfile_detect_subdir_or_none(zip_fh)) is None:
            return "Archive has no manifest: \"{:s}\"".format(PKG_MANIFEST_FILENAME_TOML)
        return pkg_manifest_from_zipfile_and_validate(zip_fh, archive_subdir, strict=strict)


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
# URL Downloading

# Originally based on `urllib.request.urlretrieve`.


def url_retrieve_to_data_iter(
        url: str,
        *,
        data: Optional[Any] = None,
        headers: Dict[str, str],
        chunk_size: int,
        timeout_in_seconds: float,
) -> Generator[Tuple[bytes, int, Any], None, None]:
    """
    Retrieve a URL into a temporary location on disk.

    Requires a URL argument. If a filename is passed, it is used as
    the temporary file location. The reporthook argument should be
    a callable that accepts a block number, a read size, and the
    total file size of the URL target. The data argument should be
    valid URL encoded data.

    If a filename is passed and the URL points to a local resource,
    the result is a copy from local file to new file.

    Returns a tuple containing the path to the newly created
    data file as well as the resulting HTTPMessage object.
    """
    from urllib.error import ContentTooShortError
    from urllib.request import urlopen

    request = urllib.request.Request(
        url,
        data=data,
        headers=headers,
    )

    with contextlib.closing(urlopen(request, timeout=timeout_in_seconds)) as fp:
        response_headers = fp.info()

        size = -1
        read = 0
        if "content-length" in response_headers:
            size = int(response_headers["Content-Length"])

        yield (b'', size, response_headers)

        if timeout_in_seconds == -1.0:
            while True:
                block = fp.read(chunk_size)
                if not block:
                    break
                read += len(block)
                yield (block, size, response_headers)
        else:
            while True:
                block = read_with_timeout(fp, chunk_size, timeout_in_seconds=timeout_in_seconds)
                if not block:
                    break
                read += len(block)
                yield (block, size, response_headers)

    if size >= 0 and read < size:
        raise ContentTooShortError(
            "retrieval incomplete: got only %i out of %i bytes" % (read, size),
            response_headers,
        )


def url_retrieve_to_filepath_iter(
        url: str,
        filepath: str,
        *,
        headers: Dict[str, str],
        data: Optional[Any] = None,
        chunk_size: int,
        timeout_in_seconds: float,
) -> Generator[Tuple[int, int, Any], None, None]:
    # Handle temporary file setup.
    with open(filepath, 'wb') as fh_output:
        for block, size, response_headers in url_retrieve_to_data_iter(
                url,
                headers=headers,
                data=data,
                chunk_size=chunk_size,
                timeout_in_seconds=timeout_in_seconds,
        ):
            fh_output.write(block)
            yield (len(block), size, response_headers)


def filepath_retrieve_to_filepath_iter(
        filepath_src: str,
        filepath: str,
        *,
        chunk_size: int,
        timeout_in_seconds: float,
) -> Generator[Tuple[int, int], None, None]:
    # TODO: `timeout_in_seconds`.
    # Handle temporary file setup.
    with open(filepath_src, 'rb') as fh_input:
        size = os.fstat(fh_input.fileno()).st_size
        with open(filepath, 'wb') as fh_output:
            while (block := fh_input.read(chunk_size)):
                fh_output.write(block)
                yield (len(block), size)


def url_retrieve_to_data_iter_or_filesystem(
        url: str,
        headers: Dict[str, str],
        chunk_size: int,
        timeout_in_seconds: float,
) -> Generator[bytes, None, None]:
    if url_is_filesystem(url):
        with open(path_from_url(url), "rb") as fh_source:
            while (block := fh_source.read(chunk_size)):
                yield block
    else:
        for (
                block,
                _size,
                _response_headers,
        ) in url_retrieve_to_data_iter(
            url,
            headers=headers,
            chunk_size=chunk_size,
            timeout_in_seconds=timeout_in_seconds,
        ):
            yield block


def url_retrieve_to_filepath_iter_or_filesystem(
        url: str,
        filepath: str,
        headers: Dict[str, str],
        chunk_size: int,
        timeout_in_seconds: float,
) -> Generator[Tuple[int, int], None, None]:
    if url_is_filesystem(url):
        yield from filepath_retrieve_to_filepath_iter(
            path_from_url(url),
            filepath,
            chunk_size=chunk_size,
            timeout_in_seconds=timeout_in_seconds,
        )
    else:
        for (read, size, _response_headers) in url_retrieve_to_filepath_iter(
            url,
            filepath,
            headers=headers,
            chunk_size=chunk_size,
            timeout_in_seconds=timeout_in_seconds,
        ):
            yield (read, size)


def pkg_idname_is_valid_or_error(pkg_idname: str) -> Optional[str]:
    if not pkg_idname.isidentifier():
        return "Not a valid identifier"
    if "__" in pkg_idname:
        return "Only single separators are supported"
    if pkg_idname.startswith("_"):
        return "Names must not start with a \"_\""
    if pkg_idname.endswith("_"):
        return "Names must not end with a \"_\""
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

def pkg_manifest_validate_field_nop(
        value: Any,
        strict: bool,
) -> Optional[str]:
    _ = strict, value
    return None


def pkg_manifest_validate_field_any_non_empty_string(
    value: str,
    strict: bool,
) -> Optional[str]:
    _ = strict
    if not value.strip():
        return "A non-empty string expected"
    return None


def pkg_manifest_validate_field_any_non_empty_string_stripped_no_control_chars(
        value: str,
        strict: bool,
) -> Optional[str]:
    _ = strict
    value_strip = value.strip()
    if not value_strip:
        return "a non-empty string expected"
    if value != value_strip:
        return "text without leading/trailing white space expected"
    for _ in RE_CONTROL_CHARS.finditer(value):
        return "text without any control characters expected"
    return None


def pkg_manifest_validate_field_any_list_of_non_empty_strings(value: List[Any], strict: bool) -> Optional[str]:
    _ = strict
    for i, tag in enumerate(value):
        if not isinstance(tag, str):
            return "at index {:d} must be a string not a {:s}".format(i, str(type(tag)))
        if not tag.strip():
            return "at index {:d} must be a non-empty string".format(i)
    return None


def pkg_manifest_validate_field_any_non_empty_list_of_non_empty_strings(
        value: List[Any],
        strict: bool,
) -> Optional[str]:
    if not value:
        return "list may not be empty"

    return pkg_manifest_validate_field_any_list_of_non_empty_strings(value, strict)


def pkg_manifest_validate_field_any_version(
        value: str,
        strict: bool,
) -> Optional[str]:
    _ = strict
    if not RE_MANIFEST_SEMVER.match(value):
        return "to be a semantic-version, found {!r}".format(value)
    return None


def pkg_manifest_validate_field_any_version_primitive(
        value: str,
        strict: bool,
) -> Optional[str]:
    _ = strict
    # Parse simple `1.2.3`, `1.2` & `1` numbers.
    for number in value.split("."):
        if not number.isdigit():
            return "must be numbers separated by single \".\" characters, found \"{:s}\"".format(value)
    return None


def pkg_manifest_validate_field_any_version_primitive_or_empty(
        value: str,
        strict: bool,
) -> Optional[str]:
    if value:
        return pkg_manifest_validate_field_any_version_primitive(value, strict)
    return None

# -----------------------------------------------------------------------------
# Manifest Validation (Specific Callbacks)


def pkg_manifest_validate_field_idname(value: str, strict: bool) -> Optional[str]:
    _ = strict
    return pkg_idname_is_valid_or_error(value)


def pkg_manifest_validate_field_type(value: str, strict: bool) -> Optional[str]:
    _ = strict
    # NOTE: add "keymap" in the future.
    value_expected = {"add-on", "theme"}
    if value not in value_expected:
        return "Expected to be one of [{:s}], found {!r}".format(", ".join(value_expected), value)
    return None


def pkg_manifest_validate_field_tagline(value: str, strict: bool) -> Optional[str]:
    if strict:
        if (error := pkg_manifest_validate_field_any_non_empty_string_stripped_no_control_chars(value, strict)) is not None:
            return error

        # Additional requirements.
        if len(value) > 64:
            return "a value no longer than 64 characters expected, found {:d}".format(len(value))
        # As we don't have a reliable (unicode aware) punctuation check, just check the last character is alpha/numeric.
        if value[-1].isalnum():
            pass  # OK.
        elif value[-1] in {")", "]", "}"}:
            pass  # Allow closing brackets (sometimes used to mention formats).
        else:
            return "alpha-numeric suffix expected, the string must not end with punctuation"
    else:
        if (error := pkg_manifest_validate_field_any_non_empty_string(value, strict)) is not None:
            return error

    return None


def pkg_manifest_validate_field_wheels(
        value: List[Any],
        strict: bool,
) -> Optional[str]:
    if (error := pkg_manifest_validate_field_any_list_of_non_empty_strings(value, strict)) is not None:
        return error
    # Enforce naming spec:
    # https://packaging.python.org/en/latest/specifications/binary-distribution-format/#file-name-convention
    # This also defines the name spec:
    filename_spec = "{distribution}-{version}(-{build tag})?-{python tag}-{abi tag}-{platform tag}.whl"

    for wheel in value:
        if "\\" in wheel:
            return "wheel paths must use forward slashes, found {!r}".format(wheel)

        wheel_filename = os.path.basename(wheel)
        if not wheel_filename.lower().endswith(".whl"):
            return "wheel paths must end with \".whl\", found {!r}".format(wheel)

        wheel_filename_split = wheel_filename.split("-")
        if not (5 <= len(wheel_filename_split) <= 6):
            return "wheel filename must follow the spec \"{:s}\", found {!r}".format(filename_spec, wheel_filename)

    return None


def pkg_manifest_validate_field_archive_size(
    value: int,
    strict: bool,
) -> Optional[str]:
    _ = strict
    if value <= 0:
        return "to be a positive integer, found {!r}".format(value)
    return None


def pkg_manifest_validate_field_archive_hash(
        value: str,
        strict: bool,
) -> Optional[str]:
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
pkg_manifest_known_keys_and_types: Tuple[
    Tuple[str, type, Callable[[Any, bool], Optional[str]]],
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
    ("blender_version_min", str, pkg_manifest_validate_field_any_version_primitive),

    # Optional.
    ("blender_version_max", str, pkg_manifest_validate_field_any_version_primitive_or_empty),
    ("website", str, pkg_manifest_validate_field_any_non_empty_string_stripped_no_control_chars),
    ("copyright", list, pkg_manifest_validate_field_any_non_empty_list_of_non_empty_strings),
    ("permissions", list, pkg_manifest_validate_field_any_list_of_non_empty_strings),
    ("tags", list, pkg_manifest_validate_field_any_non_empty_list_of_non_empty_strings),
    ("wheels", list, pkg_manifest_validate_field_wheels),
)

# Keep in sync with `PkgManifest_Archive`.
pkg_manifest_known_keys_and_types_from_repo: Tuple[
    Tuple[str, type, Callable[[Any, bool], Optional[str]]],
    ...,
] = (
    ("archive_size", int, pkg_manifest_validate_field_archive_size),
    ("archive_hash", str, pkg_manifest_validate_field_archive_hash),
    ("archive_url", str, pkg_manifest_validate_field_nop),
)


# -----------------------------------------------------------------------------
# Manifest Validation

def pkg_manifest_is_valid_or_error_impl(
        data: Dict[str, Any],
        *,
        from_repo: bool,
        all_errors: bool,
        strict: bool,
) -> Optional[List[str]]:
    if not isinstance(data, dict):
        return ["Expected value to be a dict, not a {!r}".format(type(data))]

    assert len(pkg_manifest_known_keys_and_types) == len(PkgManifest._fields)
    # -1 because the manifest is an item.
    assert len(pkg_manifest_known_keys_and_types_from_repo) == len(PkgManifest_Archive._fields) - 1

    error_list = []

    value_extract: Dict[str, Optional[object]] = {}
    for known_types in (
            (pkg_manifest_known_keys_and_types, pkg_manifest_known_keys_and_types_from_repo) if from_repo else
            (pkg_manifest_known_keys_and_types, )
    ):
        for x_key, x_ty, x_check_fn in known_types:
            is_default_value = False
            x_val = data.get(x_key, ...)
            if x_val is ...:
                x_val = PkgManifest._field_defaults.get(x_key, ...)
                if from_repo:
                    if x_val is ...:
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
                if x_ty is None:
                    pass
                elif isinstance(x_val, x_ty):
                    pass
                else:
                    error_list.append("\"{:s}\" must be a {:s}, not a {:s}".format(
                        x_key,
                        x_ty.__name__,
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
        data: Dict[str, Any],
        *,
        from_repo: bool,
        strict: bool,
) -> Optional[str]:
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
        data: Dict[str, Any],
        *,
        from_repo: bool,
        strict: bool,
) -> Optional[List[str]]:
    return pkg_manifest_is_valid_or_error_impl(
        data,
        from_repo=from_repo,
        all_errors=True,
        strict=strict,
    )


# -----------------------------------------------------------------------------
# Standalone Utilities


def url_request_headers_create(*, accept_json: bool, user_agent: str) -> Dict[str, str]:
    headers = {}
    if accept_json:
        # Default for JSON requests this allows top-level URL's to be used.
        headers["Accept"] = "application/json"

    if user_agent:
        # Typically: `Blender/4.2.0 (Linux x84_64; cycle=alpha)`.
        headers["User-Agent"] = user_agent
    return headers


def repo_json_is_valid_or_error(filepath: str) -> Optional[str]:
    if not os.path.exists(filepath):
        return "File missing: " + filepath

    try:
        with open(filepath, "r", encoding="utf-8") as fh:
            result = json.load(fh)
    except BaseException as ex:
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
            if isinstance(item, str):
                continue
            return "Expected \"blocklist\" to be a list of strings, found {:s}".format(str(type(item)))

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


def pkg_manifest_toml_is_valid_or_error(filepath: str, strict: bool) -> Tuple[Optional[str], Dict[str, Any]]:
    if not os.path.exists(filepath):
        return "File missing: " + filepath, {}

    try:
        with open(filepath, "rb") as fh:
            result = tomllib.load(fh)
    except BaseException as ex:
        return str(ex), {}

    error = pkg_manifest_is_valid_or_error(result, from_repo=False, strict=strict)
    if error is not None:
        return error, {}
    return None, result


def toml_from_bytes(data: bytes) -> Optional[Dict[str, Any]]:
    result = tomllib.loads(data.decode('utf-8'))
    assert isinstance(result, dict)
    return result


def toml_from_filepath(filepath: str) -> Optional[Dict[str, Any]]:
    with open(filepath, "rb") as fh:
        data = fh.read()
    result = toml_from_bytes(data)
    return result


def repo_local_private_dir(*, local_dir: str) -> str:
    """
    Ensure the repos hidden directory exists.
    """
    return os.path.join(local_dir, REPO_LOCAL_PRIVATE_DIR)


def repo_local_private_dir_ensure(*, local_dir: str) -> str:
    """
    Ensure the repos hidden directory exists.
    """
    local_private_dir = repo_local_private_dir(local_dir=local_dir)
    if not os.path.isdir(local_private_dir):
        # Unlikely but possible `local_dir` is missing.
        os.makedirs(local_private_dir)
    return local_private_dir


def repo_local_private_dir_ensure_with_subdir(*, local_dir: str, subdir: str) -> str:
    """
    Return a local directory used to cache package downloads.
    """
    local_private_subdir = os.path.join(repo_local_private_dir_ensure(local_dir=local_dir), subdir)
    if not os.path.isdir(local_private_subdir):
        # Unlikely but possible `local_dir` is missing.
        os.makedirs(local_private_subdir)
    return local_private_subdir


def repo_sync_from_remote(
        *,
        msg_fn: MessageFn,
        remote_url: str,
        local_dir: str,
        online_user_agent: str,
        timeout_in_seconds: float,
        extension_override: str,
) -> bool:
    """
    Load package information into the local path.
    """
    request_exit = False
    request_exit |= message_status(msg_fn, "Sync repo: {:s}".format(remote_url))
    if request_exit:
        return False

    remote_json_url = remote_url_get(remote_url)

    local_private_dir = repo_local_private_dir_ensure(local_dir=local_dir)
    local_json_path = os.path.join(local_private_dir, PKG_REPO_LIST_FILENAME)
    local_json_path_temp = local_json_path + "@"

    assert extension_override != "@"
    if extension_override:
        local_json_path = local_json_path + extension_override

    if os.path.exists(local_json_path_temp):
        os.unlink(local_json_path_temp)

    with CleanupPathsContext(files=(local_json_path_temp,), directories=()):
        # TODO: time-out.
        request_exit |= message_status(msg_fn, "Sync downloading remote data")
        if request_exit:
            return False

        try:
            read_total = 0
            for (read, size) in url_retrieve_to_filepath_iter_or_filesystem(
                    remote_json_url,
                    local_json_path_temp,
                    headers=url_request_headers_create(accept_json=True, user_agent=online_user_agent),
                    chunk_size=CHUNK_SIZE_DEFAULT,
                    timeout_in_seconds=timeout_in_seconds,
            ):
                request_exit |= message_progress(msg_fn, "Downloading...", read_total, size, 'BYTE')
                if request_exit:
                    break
                read_total += read
            del read_total

        except FileNotFoundError as ex:
            message_error(msg_fn, "sync: file-not-found ({:s}) reading {!r}!".format(str(ex), remote_url))
            return False
        except TimeoutError as ex:
            message_error(msg_fn, "sync: timeout ({:s}) reading {!r}!".format(str(ex), remote_url))
            return False
        except urllib.error.URLError as ex:
            message_error(msg_fn, "sync: URL error ({:s}) reading {!r}!".format(str(ex), remote_url))
            return False
        except BaseException as ex:
            message_error(msg_fn, "sync: unexpected error ({:s}) reading {!r}!".format(str(ex), remote_url))
            return False

        if request_exit:
            return False

        error_msg = repo_json_is_valid_or_error(local_json_path_temp)
        if error_msg is not None:
            message_error(msg_fn, "sync: invalid manifest ({:s}) reading {!r}!".format(error_msg, remote_url))
            return False
        del error_msg

        request_exit |= message_status(msg_fn, "Sync complete: {:s}".format(remote_url))
        if request_exit:
            return False

        if os.path.exists(local_json_path):
            os.unlink(local_json_path)

        # If this is a valid JSON, overwrite the existing file.
        os.rename(local_json_path_temp, local_json_path)

        if extension_override:
            request_exit |= message_path(msg_fn, os.path.relpath(local_json_path, local_dir))

    return True


def repo_pkginfo_from_local(*, local_dir: str) -> Optional[Dict[str, Any]]:
    """
    Load package cache.
    """
    local_private_dir = repo_local_private_dir(local_dir=local_dir)
    local_json_path = os.path.join(local_private_dir, PKG_REPO_LIST_FILENAME)
    if not os.path.exists(local_json_path):
        return None

    with open(local_json_path, "r", encoding="utf-8") as fh:
        result = json.load(fh)
        assert isinstance(result, dict)

    return result


def pkg_repo_dat_from_json(json_data: Dict[str, Any]) -> PkgRepoData:
    result_new = PkgRepoData(
        version=json_data.get("version", "v1"),
        blocklist=json_data.get("blocklist", []),
        data=json_data.get("data", []),
    )
    return result_new


def repo_pkginfo_from_local_with_idname_as_key(*, local_dir: str) -> Optional[PkgRepoData]:
    result = repo_pkginfo_from_local(local_dir=local_dir)
    if result is None:
        return None
    return pkg_repo_dat_from_json(result)


def url_has_known_prefix(path: str) -> bool:
    return path.startswith(URL_KNOWN_PREFIX)


def url_is_filesystem(url: str) -> bool:
    if url.startswith(("file://")):
        return True
    if url.startswith(URL_KNOWN_PREFIX):
        return False

    # Argument parsing must ensure this never happens.
    raise ValueError("prefix not known")
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


def arg_handle_str_as_url(value: str) -> Sequence[str]:
    # Handle so unexpected URL's don't cause difficult to understand errors in inner logic.
    # The URL's themselves may be invalid still, this just fails early in the case of obvious oversights.
    if not url_has_known_prefix(value):
        raise argparse.ArgumentTypeError(
            "Invalid URL \"{:s}\", expected a prefix in {!r}".format(value, URL_KNOWN_PREFIX)
        )
    return value


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


def generic_arg_remote_url(subparse: argparse.ArgumentParser) -> None:
    subparse.add_argument(
        "--remote-url",
        dest="remote_url",
        type=arg_handle_str_as_url,
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


# Only for authoring.
def generic_arg_package_source_path_positional(subparse: argparse.ArgumentParser) -> None:
    subparse.add_argument(
        dest="source_path",
        type=str,
        nargs="?",
        default=".",
        metavar="SOURCE_PATH",
        help=(
            "The package source path "
            "(either directory containing package files or the package archive).\n"
            "This path must containing a ``{:s}`` manifest.\n"
            "\n"
            "The current directory ``.`` is default.".format(PKG_MANIFEST_FILENAME_TOML)
        ),
    )


def generic_arg_package_source_dir(subparse: argparse.ArgumentParser) -> None:
    subparse.add_argument(
        "--source-dir",
        dest="source_dir",
        type=str,
        help=(
            "The package source directory containing a ``{:s}`` manifest.".format(PKG_MANIFEST_FILENAME_TOML)
        ),
        default=".",
    )


def generic_arg_package_output_dir(subparse: argparse.ArgumentParser) -> None:
    subparse.add_argument(
        "--output-dir",
        dest="output_dir",
        type=str,
        help=(
            "The package output directory."
        ),
        default=".",
    )


def generic_arg_package_output_filepath(subparse: argparse.ArgumentParser) -> None:
    subparse.add_argument(
        "--output-filepath",
        dest="output_filepath",
        type=str,
        help=(
            "The package output filepath (should include a ``{:s}`` extension).".format(PKG_EXT)
        ),
        default=".",
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
            "- TEXT: Plain text.\n"
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
    def generate(
            msg_fn: MessageFn,
            *,
            repo_dir: str,
    ) -> bool:

        if url_has_known_prefix(repo_dir):
            message_error(msg_fn, "Directory: {!r} must be a local path, not a URL!".format(repo_dir))
            return False

        if not os.path.isdir(repo_dir):
            message_error(msg_fn, "Directory: {!r} not found!".format(repo_dir))
            return False

        repo_data_idname_unique: Set[str] = set()
        repo_data: List[Dict[str, Any]] = []
        # Write package meta-data into each directory.
        repo_gen_dict = {
            "version": "1",
            "blocklist": [],
            "data": repo_data,
        }
        for entry in os.scandir(repo_dir):
            if not entry.name.endswith(PKG_EXT):
                continue

            # Harmless, but skip directories.
            if entry.is_dir():
                message_warn(msg_fn, "found unexpected directory {!r}".format(entry.name))
                continue

            filename = entry.name
            filepath = os.path.join(repo_dir, filename)
            manifest = pkg_manifest_from_archive_and_validate(filepath, strict=False)
            if isinstance(manifest, str):
                message_warn(msg_fn, "archive validation failed {!r}, error: {:s}".format(filepath, manifest))
                continue
            manifest_dict = manifest._asdict()

            repo_data_idname_unique_len = len(repo_data_idname_unique)
            repo_data_idname_unique.add(manifest_dict["id"])
            if len(repo_data_idname_unique) == repo_data_idname_unique_len:
                message_warn(msg_fn, "archive found with duplicate id {!r}, {!r}".format(manifest_dict["id"], filepath))
                continue

            # Call all optional keys so the JSON never contains `null` items.
            for key, value in list(manifest_dict.items()):
                if value is None:
                    del manifest_dict[key]

            # These are added, ensure they don't exist.
            has_key_error = False
            for key in ("archive_url", "archive_size", "archive_hash"):
                if key not in manifest_dict:
                    continue
                message_warn(
                    msg_fn,
                    "malformed meta-data from {!r}, contains key it shouldn't: {:s}".format(filepath, key),
                )
                has_key_error = True
            if has_key_error:
                continue

            # A relative URL.
            manifest_dict["archive_url"] = "./" + urllib.request.pathname2url(filename)

            # Add archive variables, see: `PkgManifest_Archive`.
            (
                manifest_dict["archive_size"],
                manifest_dict["archive_hash"],
            ) = sha256_from_file(filepath, hash_prefix=True)

            repo_data.append(manifest_dict)

        filepath_repo_json = os.path.join(repo_dir, PKG_REPO_LIST_FILENAME)

        with open(filepath_repo_json, "w", encoding="utf-8") as fh:
            json.dump(repo_gen_dict, fh, indent=2)
        message_status(msg_fn, "found {:d} packages.".format(len(repo_data)))

        return True


class subcmd_client:

    def __new__(cls) -> Any:
        raise RuntimeError("{:s} should not be instantiated".format(cls))

    @staticmethod
    def list_packages(
            msg_fn: MessageFn,
            remote_url: str,
            online_user_agent: str,
            timeout_in_seconds: float,
    ) -> bool:
        remote_json_url = remote_url_get(remote_url)

        # TODO: validate JSON content.
        try:
            result = io.BytesIO()
            for block in url_retrieve_to_data_iter_or_filesystem(
                    remote_json_url,
                    headers=url_request_headers_create(accept_json=True, user_agent=online_user_agent),
                    chunk_size=CHUNK_SIZE_DEFAULT,
                    timeout_in_seconds=timeout_in_seconds,
            ):
                result.write(block)

        except FileNotFoundError as ex:
            message_error(msg_fn, "list: file-not-found ({:s}) reading {!r}!".format(str(ex), remote_url))
            return False
        except TimeoutError as ex:
            message_error(msg_fn, "list: timeout ({:s}) reading {!r}!".format(str(ex), remote_url))
            return False
        except urllib.error.URLError as ex:
            message_error(msg_fn, "list: URL error ({:s}) reading {!r}!".format(str(ex), remote_url))
            return False
        except BaseException as ex:
            message_error(msg_fn, "list: unexpected error ({:s}) reading {!r}!".format(str(ex), remote_url))
            return False

        result_str = result.getvalue().decode("utf-8")
        del result

        repo_gen_dict = pkg_repo_dat_from_json(json.loads(result_str))

        items: List[Dict[str, Any]] = repo_gen_dict.data
        items.sort(key=lambda elem: elem.get("id", ""))

        request_exit = False
        for elem in items:
            request_exit |= message_status(
                msg_fn,
                "{:s}({:s}): {:s}".format(elem.get("id"), elem.get("version"), elem.get("name")),
            )
            if request_exit:
                return False

        return True

    @staticmethod
    def sync(
            msg_fn: MessageFn,
            *,
            remote_url: str,
            local_dir: str,
            online_user_agent: str,
            timeout_in_seconds: float,
            force_exit_ok: bool,
            extension_override: str,
    ) -> bool:
        if force_exit_ok:
            force_exit_ok_enable()

        success = repo_sync_from_remote(
            msg_fn=msg_fn,
            remote_url=remote_url,
            local_dir=local_dir,
            online_user_agent=online_user_agent,
            timeout_in_seconds=timeout_in_seconds,
            extension_override=extension_override,
        )
        return success

    @staticmethod
    def _install_package_from_file_impl(
            msg_fn: MessageFn,
            *,
            local_dir: str,
            filepath_archive: str,
            manifest_compare: Optional[PkgManifest],
    ) -> bool:
        # Implement installing a package to a repository.
        # Used for installing from local cache as well as installing a local package from a file.

        # Remove `filepath_local_pkg_temp` if this block exits.
        directories_to_clean: List[str] = []
        with CleanupPathsContext(files=(), directories=directories_to_clean):
            try:
                zip_fh_context = zipfile.ZipFile(filepath_archive, mode="r")
            except BaseException as ex:
                message_warn(
                    msg_fn,
                    "Error extracting archive: {:s}".format(str(ex)),
                )
                return False

            with contextlib.closing(zip_fh_context) as zip_fh:
                archive_subdir = pkg_zipfile_detect_subdir_or_none(zip_fh)
                if archive_subdir is None:
                    message_warn(
                        msg_fn,
                        "Missing manifest from: {:s}".format(filepath_archive),
                    )
                    return False

                manifest = pkg_manifest_from_zipfile_and_validate(zip_fh, archive_subdir, strict=False)
                if isinstance(manifest, str):
                    message_warn(
                        msg_fn,
                        "Error loading manifest from: {:s}".format(manifest),
                    )
                    return False

                if manifest_compare is not None:
                    # The archive ID name must match the server name,
                    # otherwise the package will install but not be able to collate
                    # the installed package with the remote ID.
                    if manifest_compare.id != manifest.id:
                        message_warn(
                            msg_fn,
                            "Package ID mismatch (remote: \"{:s}\", archive: \"{:s}\")".format(
                                manifest_compare.id,
                                manifest.id,
                            )
                        )
                        return False
                    if manifest_compare.version != manifest.version:
                        message_warn(
                            msg_fn,
                            "Package version mismatch (remote: \"{:s}\", archive: \"{:s}\")".format(
                                manifest_compare.version,
                                manifest.version,
                            )
                        )
                        return False

                # We have the cache, extract it to a directory.
                # This will be a directory.
                filepath_local_pkg = os.path.join(local_dir, manifest.id)

                # First extract into a temporary directory, validate the package is not corrupt,
                # then move the package to it's expected location.
                filepath_local_pkg_temp = filepath_local_pkg + "@"

                # It's unlikely this exist, nevertheless if it does - it must be removed.
                if os.path.isdir(filepath_local_pkg_temp):
                    shutil.rmtree(filepath_local_pkg_temp)

                directories_to_clean.append(filepath_local_pkg_temp)

                if archive_subdir:
                    zipfile_make_root_directory(zip_fh, archive_subdir)
                del archive_subdir

                try:
                    for member in zip_fh.infolist():
                        zip_fh.extract(member, filepath_local_pkg_temp)
                except BaseException as ex:
                    message_warn(
                        msg_fn,
                        "Failed to extract files for \"{:s}\": {:s}".format(manifest.id, str(ex)),
                    )
                    return False

            is_reinstall = False
            if os.path.isdir(filepath_local_pkg):
                shutil.rmtree(filepath_local_pkg)
                is_reinstall = True

            os.rename(filepath_local_pkg_temp, filepath_local_pkg)
            directories_to_clean.remove(filepath_local_pkg_temp)

        if is_reinstall:
            message_status(msg_fn, "Re-Installed \"{:s}\"".format(manifest.id))
        else:
            message_status(msg_fn, "Installed \"{:s}\"".format(manifest.id))

        return True

    @staticmethod
    def install_packages_from_files(
            msg_fn: MessageFn,
            *,
            local_dir: str,
            package_files: Sequence[str],
    ) -> bool:
        if not os.path.exists(local_dir):
            message_error(msg_fn, "destination directory \"{:s}\" does not exist".format(local_dir))
            return False

        # This is a simple file extraction, the main difference is that it validates the manifest before installing.
        directories_to_clean: List[str] = []
        with CleanupPathsContext(files=(), directories=directories_to_clean):
            for filepath_archive in package_files:
                if not subcmd_client._install_package_from_file_impl(
                        msg_fn,
                        local_dir=local_dir,
                        filepath_archive=filepath_archive,
                        # There is no manifest from the repository, leave this unset.
                        manifest_compare=None,
                ):
                    # The package failed to install.
                    continue

        return True

    @staticmethod
    def install_packages(
            msg_fn: MessageFn,
            *,
            remote_url: str,
            local_dir: str,
            local_cache: bool,
            packages: Sequence[str],
            online_user_agent: str,
            timeout_in_seconds: float,
    ) -> bool:
        # Extract...
        pkg_repo_data = repo_pkginfo_from_local_with_idname_as_key(local_dir=local_dir)
        if pkg_repo_data is None:
            # TODO: raise warning.
            return False

        # Most likely this doesn't have duplicates,but any errors procured by duplicates
        # are likely to be obtuse enough that it's better to guarantee there are none.
        packages = tuple(sorted(set(packages)))

        # Ensure a private directory so a local cache can be created.
        local_cache_dir = repo_local_private_dir_ensure_with_subdir(local_dir=local_dir, subdir="cache")

        # TODO: this could be optimized to only lookup known ID's.
        json_data_pkg_info_map: Dict[str, Dict[str, Any]] = {
            pkg_info["id"]: pkg_info for pkg_info in pkg_repo_data.data
        }

        has_error = False
        packages_info: List[PkgManifest_Archive] = []
        for pkg_idname in packages:
            pkg_info = json_data_pkg_info_map.get(pkg_idname)
            if pkg_info is None:
                message_error(msg_fn, "Package \"{:s}\", not found".format(pkg_idname))
                has_error = True
                continue
            manifest_archive = pkg_manifest_archive_from_dict_and_validate(pkg_info, strict=False)
            if isinstance(manifest_archive, str):
                message_error(msg_fn, "Package malformed meta-data for \"{:s}\", error: {:s}".format(
                    pkg_idname,
                    manifest_archive,
                ))
                has_error = True
                continue

            packages_info.append(manifest_archive)

        if has_error:
            return False
        del has_error

        request_exit = False

        # Ensure all cache is cleared (when `local_cache` is disabled) no matter the cause of exiting.
        files_to_clean: List[str] = []
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
                    if remote_url_has_filename_suffix(remote_url):
                        filepath_remote_archive = remote_url.rpartition("/")[0] + pkg_archive_url[1:]
                    else:
                        filepath_remote_archive = remote_url.rstrip("/") + pkg_archive_url[1:]
                else:
                    filepath_remote_archive = pkg_archive_url

                # Check if the cache should be used.
                found = False
                if os.path.exists(filepath_local_cache_archive):
                    if (
                            local_cache and (
                                archive_size_expected,
                                archive_hash_expected,
                            ) == sha256_from_file(filepath_local_cache_archive, hash_prefix=True)
                    ):
                        found = True
                    else:
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
                                    headers=url_request_headers_create(accept_json=False, user_agent=online_user_agent),
                                    chunk_size=CHUNK_SIZE_DEFAULT,
                                    timeout_in_seconds=timeout_in_seconds,
                            ):
                                request_exit |= message_progress(
                                    msg_fn,
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

                    except FileNotFoundError as ex:
                        message_error(
                            msg_fn,
                            "install: file-not-found ({:s}) reading {!r}!".format(str(ex), filepath_remote_archive),
                        )
                        return False
                    except TimeoutError as ex:
                        message_error(
                            msg_fn,
                            "install: timeout ({:s}) reading {!r}!".format(str(ex), filepath_remote_archive),
                        )
                        return False
                    except urllib.error.URLError as ex:
                        message_error(
                            msg_fn,
                            "install: URL error ({:s}) reading {!r}!".format(str(ex), filepath_remote_archive),
                        )
                        return False
                    except BaseException as ex:
                        message_error(
                            msg_fn,
                            "install: unexpected error ({:s}) reading {!r}!".format(str(ex), filepath_remote_archive),
                        )
                        return False

                    if request_exit:
                        return False

                    # Validate:
                    if filename_archive_size_test != archive_size_expected:
                        message_warn(msg_fn, "Archive size mismatch \"{:s}\", expected {:d}, was {:d}".format(
                            pkg_idname,
                            archive_size_expected,
                            filename_archive_size_test,
                        ))
                        return False
                    filename_archive_hash_test = "sha256:" + sha256.hexdigest()
                    if filename_archive_hash_test != archive_hash_expected:
                        message_warn(msg_fn, "Archive checksum mismatch \"{:s}\", expected {:s}, was {:s}".format(
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
                        msg_fn,
                        local_dir=local_dir,
                        filepath_archive=filepath_local_cache_archive,
                        manifest_compare=manifest_archive.manifest,
                ):
                    # The package failed to install.
                    continue

        return True

    @staticmethod
    def uninstall_packages(
            msg_fn: MessageFn,
            *,
            local_dir: str,
            packages: Sequence[str],
    ) -> bool:
        if not os.path.isdir(local_dir):
            message_error(msg_fn, "Missing local \"{:s}\"".format(local_dir))
            return False

        # Most likely this doesn't have duplicates,but any errors procured by duplicates
        # are likely to be obtuse enough that it's better to guarantee there are none.
        packages = tuple(sorted(set(packages)))

        packages_valid = []

        error = False
        for pkg_idname in packages:
            # As this simply removes the directories right now,
            # validate this path cannot be used for an unexpected outcome,
            # or using `../../` to remove directories that shouldn't.
            if (pkg_idname in {"", ".", ".."}) or ("\\" in pkg_idname or "/" in pkg_idname):
                message_error(msg_fn, "Package name invalid \"{:s}\"".format(pkg_idname))
                error = True
                continue

            # This will be a directory.
            filepath_local_pkg = os.path.join(local_dir, pkg_idname)
            if not os.path.isdir(filepath_local_pkg):
                message_error(msg_fn, "Package not found \"{:s}\"".format(pkg_idname))
                error = True
                continue

            packages_valid.append(pkg_idname)
        del filepath_local_pkg

        if error:
            return False

        # Ensure a private directory so a local cache can be created.
        # TODO: don't create (it's only accessed for file removal).
        local_cache_dir = repo_local_private_dir_ensure_with_subdir(local_dir=local_dir, subdir="cache")

        files_to_clean: List[str] = []
        with CleanupPathsContext(files=files_to_clean, directories=()):
            for pkg_idname in packages_valid:
                filepath_local_pkg = os.path.join(local_dir, pkg_idname)
                try:
                    shutil.rmtree(filepath_local_pkg)
                except BaseException as ex:
                    message_error(msg_fn, "Failure to remove \"{:s}\" with error ({:s})".format(pkg_idname, str(ex)))
                    continue

                message_status(msg_fn, "Removed \"{:s}\"".format(pkg_idname))

                filepath_local_cache_archive = os.path.join(local_cache_dir, pkg_idname + PKG_EXT)
                if os.path.exists(filepath_local_cache_archive):
                    files_to_clean.append(filepath_local_cache_archive)

        return True


class subcmd_author:

    @staticmethod
    def build(
            msg_fn: MessageFn,
            *,
            pkg_source_dir: str,
            pkg_output_dir: str,
            pkg_output_filepath: str,
    ) -> bool:
        if not os.path.isdir(pkg_source_dir):
            message_error(msg_fn, "Missing local \"{:s}\"".format(pkg_source_dir))
            return False

        if pkg_output_dir != "." and pkg_output_filepath != ".":
            message_error(msg_fn, "Both output directory & output filepath set, set one or the other")
            return False

        pkg_manifest_filepath = os.path.join(pkg_source_dir, PKG_MANIFEST_FILENAME_TOML)

        if not os.path.exists(pkg_manifest_filepath):
            message_error(msg_fn, "File \"{:s}\" not found!".format(pkg_manifest_filepath))
            return False

        manifest = pkg_manifest_from_toml_and_validate_all_errors(pkg_manifest_filepath, strict=True)
        if isinstance(manifest, list):
            for error_msg in manifest:
                message_error(msg_fn, "Error parsing TOML \"{:s}\" {:s}".format(pkg_manifest_filepath, error_msg))
            return False

        pkg_filename = manifest.id + PKG_EXT

        if pkg_output_filepath != ".":
            outfile = pkg_output_filepath
        else:
            outfile = os.path.join(pkg_output_dir, pkg_filename)

        outfile_temp = outfile + "@"

        filenames_root_exclude = {
            pkg_filename,
            # It's possible a temporary file exists from a previous run which was not cleaned up.
            # Although in general this should be cleaned up - power failure etc may mean it exists.
            pkg_filename + "@",
            # This is added, converted from the TOML.
            PKG_REPO_LIST_FILENAME,

            # We could exclude the manifest: `PKG_MANIFEST_FILENAME_TOML`
            # but it's now used so a generation step isn't needed.
        }

        request_exit = False

        request_exit |= message_status(msg_fn, "Building {:s}".format(pkg_filename))
        if request_exit:
            return False

        with CleanupPathsContext(files=(outfile_temp,), directories=()):
            try:
                zip_fh_context = zipfile.ZipFile(outfile_temp, 'w', zipfile.ZIP_LZMA)
            except BaseException as ex:
                message_status(msg_fn, "Error creating archive \"{:s}\"".format(str(ex)))
                return False

            with contextlib.closing(zip_fh_context) as zip_fh:
                for filepath_abs, filepath_rel in scandir_recursive(
                        pkg_source_dir,
                        # Be more advanced in the future, for now ignore dot-files (`.git`) .. etc.
                        filter_fn=lambda x: not x.startswith(".")
                ):
                    if filepath_rel in filenames_root_exclude:
                        continue

                    # Handy for testing that sub-directories:
                    # zip_fh.write(filepath_abs, manifest.id + "/" + filepath_rel)
                    compress_type = zipfile.ZIP_STORED if filepath_skip_compress(filepath_abs) else None
                    try:
                        zip_fh.write(filepath_abs, filepath_rel, compress_type=compress_type)
                    except BaseException as ex:
                        message_status(msg_fn, "Error adding to archive \"{:s}\"".format(str(ex)))
                        return False

                request_exit |= message_status(msg_fn, "complete")
                if request_exit:
                    return False

            if os.path.exists(outfile):
                os.unlink(outfile)
            os.rename(outfile_temp, outfile)

        message_status(msg_fn, "created \"{:s}\", {:d}".format(outfile, os.path.getsize(outfile)))
        return True

    @staticmethod
    def _validate_directory(
            msg_fn: MessageFn,
            *,
            pkg_source_dir: str,
    ) -> bool:
        pkg_manifest_filepath = os.path.join(pkg_source_dir, PKG_MANIFEST_FILENAME_TOML)

        if not os.path.exists(pkg_manifest_filepath):
            message_error(msg_fn, "Error, file \"{:s}\" not found!".format(pkg_manifest_filepath))
            return False

        # Demote errors to status as the function of this action is to check the manifest is stable.
        manifest = pkg_manifest_from_toml_and_validate_all_errors(pkg_manifest_filepath, strict=True)
        if isinstance(manifest, list):
            message_status(msg_fn, "Error parsing TOML \"{:s}\"".format(pkg_manifest_filepath))
            for error_msg in manifest:
                message_status(msg_fn, error_msg)
            return False

        expected_files = []
        if manifest.type == "add-on":
            expected_files.append("__init__.py")
        ok = True
        for filepath in expected_files:
            if not os.path.exists(os.path.join(pkg_source_dir, filepath)):
                message_status(msg_fn, "Error, file missing from {:s}: \"{:s}\"".format(
                    manifest.type,
                    filepath,
                ))
                ok = False
        if not ok:
            return False

        message_status(msg_fn, "Success parsing TOML in \"{:s}\"".format(pkg_source_dir))
        return True

    @staticmethod
    def _validate_archive(
            msg_fn: MessageFn,
            *,
            pkg_source_archive: str,
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
            zip_fh_context = zipfile.ZipFile(pkg_source_archive, mode="r")
        except BaseException as ex:
            message_status(msg_fn, "Error extracting archive \"{:s}\"".format(str(ex)))
            return False

        with contextlib.closing(zip_fh_context) as zip_fh:
            if (archive_subdir := pkg_zipfile_detect_subdir_or_none(zip_fh)) is None:
                message_status(msg_fn, "Error, archive has no manifest: \"{:s}\"".format(PKG_MANIFEST_FILENAME_TOML))
                return False
            # Demote errors to status as the function of this action is to check the manifest is stable.
            manifest = pkg_manifest_from_zipfile_and_validate_all_errors(zip_fh, archive_subdir, strict=True)
            if isinstance(manifest, list):
                message_status(msg_fn, "Error parsing TOML in \"{:s}\"".format(pkg_source_archive))
                for error_msg in manifest:
                    message_status(msg_fn, error_msg)
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
                    message_status(msg_fn, "Error, file missing from {:s}: \"{:s}\"".format(
                        manifest.type,
                        filepath,
                    ))
                    ok = False
            if not ok:
                return False

        message_status(msg_fn, "Success parsing TOML in \"{:s}\"".format(pkg_source_archive))
        return True

    @staticmethod
    def validate(
            msg_fn: MessageFn,
            *,
            source_path: str,
    ) -> bool:
        if os.path.isdir(source_path):
            result = subcmd_author._validate_directory(msg_fn, pkg_source_dir=source_path)
        else:
            result = subcmd_author._validate_archive(msg_fn, pkg_source_archive=source_path)
        return result


class subcmd_dummy:

    @staticmethod
    def repo(
            msg_fn: MessageFn,
            *,
            repo_dir: str,
            package_names: Sequence[str],
    ) -> bool:

        def msg_fn_no_done(ty: str, data: PrimTypeOrSeq) -> bool:
            if ty == 'DONE':
                return False
            return msg_fn(ty, data)

        # Ensure package names are valid.
        package_names = tuple(set(package_names))
        for pkg_idname in package_names:
            if (error_msg := pkg_idname_is_valid_or_error(pkg_idname)) is None:
                continue
            message_error(
                msg_fn,
                "key \"id\", \"{:s}\" doesn't match expected format, \"{:s}\"".format(pkg_idname, error_msg),
            )
            return False

        if url_has_known_prefix(repo_dir):
            message_error(msg_fn, "Generating a repository on a remote path is not supported")
            return False

        # Unlike most other commands, create the repo_dir it doesn't already exist.
        if not os.path.exists(repo_dir):
            try:
                os.makedirs(repo_dir)
            except BaseException as ex:
                message_error(msg_fn, "Failed to create \"{:s}\" with error: {!r}".format(repo_dir, ex))
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
                    msg_fn_no_done,
                    pkg_source_dir=pkg_src_dir,
                    pkg_output_dir=repo_dir,
                    pkg_output_filepath=".",
                ):
                    # Error running command.
                    return False

        # `{cmd} server-generate --repo-dir {repo_dir}`.
        if not subcmd_server.generate(
            msg_fn_no_done,
            repo_dir=repo_dir,
        ):
            # Error running command.
            return False

        message_done(msg_fn)
        return True

    @staticmethod
    def progress(
            msg_fn: MessageFn,
            *,
            time_duration: float,
            time_delay: float,
    ) -> bool:
        import time
        request_exit = False
        time_start = time.time() if (time_duration > 0.0) else 0.0
        size_beg = 0
        size_end = 100
        while time_duration == 0.0 or (time.time() - time_start < time_duration):
            request_exit |= message_progress(msg_fn, "Demo", size_beg, size_end, 'BYTE')
            if request_exit:
                break
            size_beg += 1
            if size_beg > size_end:
                size_beg = 0
            time.sleep(time_delay)
        if request_exit:
            message_done(msg_fn)
            return False

        message_done(msg_fn)
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
    if args_internal:
        generic_arg_output_type(subparse)

    subparse.set_defaults(
        func=lambda args: subcmd_server.generate(
            msg_fn_from_args(args),
            repo_dir=args.repo_dir,
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

    generic_arg_output_type(subparse)
    generic_arg_timeout(subparse)

    subparse.set_defaults(
        func=lambda args: subcmd_client.list_packages(
            msg_fn_from_args(args),
            args.remote_url,
            online_user_agent=args.online_user_agent,
            timeout_in_seconds=args.timeout,
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
    generic_arg_local_dir(subparse)
    generic_arg_online_user_agent(subparse)

    generic_arg_output_type(subparse)
    generic_arg_timeout(subparse)
    generic_arg_ignore_broken_pipe(subparse)
    generic_arg_extension_override(subparse)

    subparse.set_defaults(
        func=lambda args: subcmd_client.sync(
            msg_fn_from_args(args),
            remote_url=args.remote_url,
            local_dir=args.local_dir,
            online_user_agent=args.online_user_agent,
            timeout_in_seconds=args.timeout,
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
    generic_arg_output_type(subparse)

    subparse.set_defaults(
        func=lambda args: subcmd_client.install_packages_from_files(
            msg_fn_from_args(args),
            local_dir=args.local_dir,
            package_files=args.files,
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

    generic_arg_output_type(subparse)
    generic_arg_timeout(subparse)

    subparse.set_defaults(
        func=lambda args: subcmd_client.install_packages(
            msg_fn_from_args(args),
            remote_url=args.remote_url,
            local_dir=args.local_dir,
            local_cache=args.local_cache,
            packages=args.packages.split(","),
            online_user_agent=args.online_user_agent,
            timeout_in_seconds=args.timeout,
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
    generic_arg_output_type(subparse)

    subparse.set_defaults(
        func=lambda args: subcmd_client.uninstall_packages(
            msg_fn_from_args(args),
            local_dir=args.local_dir,
            packages=args.packages.split(","),
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

    if args_internal:
        generic_arg_output_type(subparse)

    subparse.set_defaults(
        func=lambda args: subcmd_author.build(
            msg_fn_from_args(args),
            pkg_source_dir=args.source_dir,
            pkg_output_dir=args.output_dir,
            pkg_output_filepath=args.output_filepath,
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

    if args_internal:
        generic_arg_output_type(subparse)

    subparse.set_defaults(
        func=lambda args: subcmd_author.validate(
            msg_fn_from_args(args),
            source_path=args.source_path,
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
            msg_fn_from_args(args),
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
        description="Demo output.",
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

    generic_arg_output_type(subparse)

    subparse.set_defaults(
        func=lambda args: subcmd_dummy.progress(
            msg_fn_from_args(args),
            time_duration=args.time_duration,
            time_delay=args.time_delay,
        ),
    )


def argparse_create(
        args_internal: bool = True,
        args_extra_subcommands_fn: Optional[ArgsSubparseFn] = None,
        prog: Optional[str] = None,
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


def msg_fn_from_args(args: argparse.Namespace) -> MessageFn:
    # Will be None when running form Blender.
    output_type = getattr(args, "output_type", 'TEXT')

    match output_type:
        case 'JSON':
            return msg_print_json
        case 'JSON_0':
            return msg_print_json_0
        case 'TEXT':
            return msg_print_text

    raise Exception("Unknown output!")


def main(
        argv: Optional[List[str]] = None,
        args_internal: bool = True,
        args_extra_subcommands_fn: Optional[ArgsSubparseFn] = None,
        prog: Optional[str] = None,
) -> int:

    # Needed on WIN32 which doesn't default to `utf-8`.
    for fh in (sys.stdout, sys.stderr):
        # While this is typically the case, is only guaranteed to be `TextIO` so check `reconfigure` is available.
        if not isinstance(fh, io.TextIOWrapper):
            continue
        if fh.encoding.lower().partition(":")[0] == "utf-8":
            continue
        fh.reconfigure(encoding="utf-8")

    if "--version" in sys.argv:
        sys.stdout.write("{:s}\n".format(VERSION))
        return 0

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
