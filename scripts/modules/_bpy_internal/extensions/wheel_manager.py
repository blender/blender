# SPDX-FileCopyrightText: 2024 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

# Ref: https://peps.python.org/pep-0491/
#      Deferred but seems to include valid info for existing wheels.

"""
This module takes wheels and applies them to a "managed" destination directory.
"""

__all__ = (
    "apply_action",
)

import contextlib
import os
import re
import shutil
import sys
import zipfile

from collections.abc import (
    Callable,
    Iterator,
)

WheelSource = tuple[
    # Key - doesn't matter what this is... it's just a handle.
    str,
    # A list of absolute wheel file-paths.
    list[str],
]


def _read_records_csv(filepath: str) -> list[list[str]]:
    import csv
    with open(filepath, encoding="utf8", errors="surrogateescape") as fh:
        return list(csv.reader(fh.read().splitlines()))


def _wheels_from_dir(dirpath: str) -> tuple[
        # The key is:
        #   wheel_id
        # The values are:
        #   Top level directories.
        dict[str, list[str]],
        # Unknown paths.
        list[str],
]:
    result: dict[str, list[str]] = {}
    paths_unused: set[str] = set()

    if not os.path.exists(dirpath):
        return result, list(paths_unused)

    for entry in os.scandir(dirpath):
        name = entry.name
        paths_unused.add(name)
        if not entry.is_dir():
            continue
        # TODO: is this part of the spec?
        name = entry.name
        if not name.endswith("-info"):
            continue
        filepath_record = os.path.join(entry.path, "RECORD")
        if not os.path.exists(filepath_record):
            continue

        record_rows = _read_records_csv(filepath_record)

        # Build top-level paths.
        toplevel_paths_set: set[str] = set()
        for row in record_rows:
            if not row:
                continue
            path_text = row[0]
            # Ensure paths separator is compatible.
            path_text = path_text.replace("\\", "/")
            # Ensure double slashes don't cause issues or "/./" doesn't complicate checking the head of the path.
            path_split = [
                elem for elem in path_text.split("/")
                if elem not in {"", "."}
            ]
            if not path_split:
                continue
            # These wont have been extracted.
            if path_split[0] in {"..", name}:
                continue

            toplevel_paths_set.add(path_split[0])

        # Some wheels contain `{name}.libs` which are *not* listed in `RECORD`.
        # Always add the path, the value will be skipped if it's missing.
        toplevel_paths_set.add(os.path.join(dirpath, name.partition("-")[0] + ".libs"))

        result[name] = list(sorted(toplevel_paths_set))
        del toplevel_paths_set

    for wheel_name, toplevel_paths in result.items():
        paths_unused.discard(wheel_name)
        for name in toplevel_paths:
            paths_unused.discard(name)

    paths_unused_list = list(sorted(paths_unused))

    return result, paths_unused_list


def _wheel_info_dir_from_zip(filepath_wheel: str) -> tuple[str, list[str]] | None:
    """
    Return:
    - The "*-info" directory name which contains meta-data.
    - The top-level path list (excluding "..").
    """
    dir_info = ""
    toplevel_paths: set[str] = set()

    with zipfile.ZipFile(filepath_wheel, mode="r") as zip_fh:
        # This file will always exist.
        for filepath_rel in zip_fh.namelist():
            path_split = [
                elem for elem in filepath_rel.split("/")
                if elem not in {"", "."}
            ]
            if not path_split:
                continue
            if path_split[0] == "..":
                continue

            if len(path_split) == 2:
                if path_split[1].upper() == "RECORD":
                    if path_split[0].endswith("-info"):
                        dir_info = path_split[0]

            toplevel_paths.add(path_split[0])

    if dir_info == "":
        return None
    toplevel_paths.discard(dir_info)
    toplevel_paths_list = list(sorted(toplevel_paths))
    return dir_info, toplevel_paths_list


def _rmtree_safe(dir_remove: str, expected_root: str) -> Exception | None:
    if not dir_remove.startswith(expected_root):
        raise Exception("Expected prefix not found")

    ex_result = None

    if sys.version_info < (3, 12):
        def on_error(*args) -> None:  # type: ignore
            nonlocal ex_result
            print("Failed to remove:", args)
            ex_result = args[2][0]

        shutil.rmtree(dir_remove, onerror=on_error)
    else:
        def on_exc(*args) -> None:  # type: ignore
            nonlocal ex_result
            print("Failed to remove:", args)
            ex_result = args[2]

        shutil.rmtree(dir_remove, onexc=on_exc)

    return ex_result


def _remove_safe(file_remove: str) -> Exception | None:
    ex_result = None

    try:
        os.remove(file_remove)
    except Exception as ex:
        ex_result = ex

    return ex_result


# -----------------------------------------------------------------------------
# Support for Wheel: Binary distribution format

def _wheel_parse_key_value(data: bytes) -> dict[bytes, bytes]:
    # Parse: `{module}.dist-info/WHEEL` format, parse it inline as
    # this doesn't seem to use an existing specification, it's simply key/value pairs.
    result = {}
    for line in data.split(b"\n"):
        key, sep, value = line.partition(b":")
        if not sep:
            continue
        if not key:
            continue
        result[key.strip()] = value.strip()
    return result


def _wheel_record_csv_remap(record_data: str, record_path_map: dict[str, str]) -> bytes:
    import csv
    from io import StringIO

    lines_remap = []

    for line in csv.reader(StringIO(record_data, newline="")):
        # It's expected to be 3, in this case we only care about the first element (the path),
        # however, if there are fewer items, this may be malformed or some unknown future format.
        # - Only handle lines containing 3 elements.
        # - Only manipulate the first element.
        if len(line) < 3:
            continue
        # Items 1 and 2 are hash_sum & size respectively.
        # If the files need to be modified these will need to be updated.
        path = line[0]
        if (path_remap := record_path_map.get(path)) is not None:
            print(path_remap)
            line = [path_remap, *line[0]]

        lines_remap.append(line)

    data = StringIO()
    writer = csv.writer(data, delimiter=",", quotechar='"', lineterminator="\n")
    writer.writerows(lines_remap)
    return data.getvalue().encode("utf8")


def _wheel_zipfile_normalize(
        zip_fh: zipfile.ZipFile,
        error_fn: Callable[[Exception], None],
) -> dict[str, bytes] | None:
    """
    Modify the ZIP file to account for Python's binary format.
    """

    member_dict = {}
    files_to_find = (".dist-info/WHEEL", ".dist-info/RECORD")

    for member in zip_fh.infolist():
        filename_orig = member.filename
        if (
                filename_orig.endswith(files_to_find) and
                # Unlikely but possible the names also exist in nested directories.
                (filename_orig.count("/") == 1)
        ):
            member_dict[os.path.basename(filename_orig)] = member
            if len(member_dict) == len(files_to_find):
                break

    if (
            ((member_wheel := member_dict.get("WHEEL")) is None) or
            ((member_record := member_dict.get("RECORD")) is None)
    ):
        return None

    try:
        wheel_data = zip_fh.read(member_wheel.filename)
    except Exception as ex:
        error_fn(ex)
        return None

    wheel_key_values = _wheel_parse_key_value(wheel_data)
    if wheel_key_values.get(b"Root-Is-Purelib", b"true").lower() != b"false":
        return None
    del wheel_key_values

    # The setting has been found: `Root-Is-Purelib: false`.
    # This requires the wheel to be mangled.
    #
    # - `{module-XXX}.dist-info/*` will have a:
    #   `{module-XXX}.data/purelib/`
    # - For a full list see:
    #   https://docs.python.org/3/library/sysconfig.html#installation-paths
    #
    # Note that PIP's `wheel` package has a `wheel/wheelfile.py`  file which is a useful reference.

    assert member_wheel.filename.endswith("/WHEEL")
    dirpath_dist_info = member_wheel.filename.removesuffix("/WHEEL")
    assert dirpath_dist_info.endswith(".dist-info")
    dirpath_data = dirpath_dist_info.removesuffix("dist-info") + "data"
    dirpath_data_with_slash = dirpath_data + "/"

    # https://docs.python.org/3/library/sysconfig.html#user-scheme
    user_scheme_map = {}
    data_map = {}
    record_path_map = {}

    # Simply strip the prefix in the case of `purelib` & `platlib`
    # so the modules are found in the expected directory.
    #
    # Note that we could support a "bin" and other directories however
    # for the purpose of Blender scripts, installing command line programs
    # for Blender's add-ons to access via `bin` is quite niche (although not impossible).
    #
    # For the time being this is *not* full support Python's "User scheme"
    # just enough to import modules.
    #
    # Omitting other directories such as "includes" & "scripts" means these will remain in the
    # `{module-XXX}.data/includes` sub-directory, support for them can always be added if needed.

    user_scheme_map["purelib"] = ""
    user_scheme_map["platlib"] = ""

    for member in zip_fh.infolist():
        filepath_orig = member.filename
        if not filepath_orig.startswith(dirpath_data_with_slash):
            continue

        path_base, path_tail = filepath_orig[len(dirpath_data_with_slash):].partition("/")[0::2]
        # The path may not contain a tail, skip these cases.
        if not path_tail:
            continue

        if (path_base_remap := user_scheme_map.get(path_base)) is None:
            continue

        if path_base_remap:
            filepath_remap = "{:s}/{:s}".format(path_base_remap, path_tail)
        else:
            filepath_remap = path_tail

        member.filename = filepath_remap

        record_path_map[filepath_orig] = filepath_remap

    try:
        data_map[member_record.filename] = _wheel_record_csv_remap(
            zip_fh.read(member_record.filename).decode("utf8"),
            record_path_map,
        )
    except Exception as ex:
        error_fn(ex)
        return None

    # Nothing to remap.
    if not record_path_map:
        return None

    return data_map


# -----------------------------------------------------------------------------
# Generic ZIP File Extractions

def _zipfile_extractall_safe(
        zip_fh: zipfile.ZipFile,
        path: str,
        path_restrict: str,
        *,
        error_fn: Callable[[Exception], None],
        remove_error_fn: Callable[[str, Exception], None],

        # Map zip-file data to bytes.
        # Only for small files as the mapped data needs to be held in memory.
        # As it happens for this use case, it's only needed for the CSV file listing.
        data_map: dict[str, bytes] | None,
) -> None:
    """
    A version of ``ZipFile.extractall`` that wont write to paths outside ``path_restrict``.

    Avoids writing this:
        ``zip_fh.extractall(zip_fh, path)``
    """
    sep = os.sep
    path_restrict = path_restrict.rstrip(sep)
    if sep == "\\":
        path_restrict = path_restrict.rstrip("/")
    path_restrict_with_slash = path_restrict + sep

    # Strip is probably not needed (only if multiple slashes exist).
    path_prefix = path[len(path_restrict_with_slash):].lstrip(sep)
    # Switch slashes forward.
    if sep == "\\":
        path_prefix = path_prefix.replace("\\", "/").rstrip("/") + "/"
    else:
        path_prefix = path_prefix + "/"

    path_restrict_with_slash = path_restrict + sep
    assert len(path) >= len(path_restrict_with_slash)
    if not path.startswith(path_restrict_with_slash):
        # This is an internal error if it ever happens.
        raise Exception("Expected the restricted directory to start with \"{:s}\"".format(path_restrict_with_slash))

    has_error = False
    member_index = 0

    # Use an iterator to avoid duplicating the checks (for the cleanup pass).
    def zip_iter_filtered(*, verbose: bool) -> Iterator[tuple[zipfile.ZipInfo, str, str]]:
        for member in zip_fh.infolist():
            filename_orig = member.filename
            filename_next = path_prefix + filename_orig

            # This isn't likely to happen so accept a noisy print here.
            # If this ends up happening more often, it could be suppressed.
            # (although this hints at bigger problems because we might be excluding necessary files).
            if os.path.normpath(filename_next).startswith(".." + sep):
                if verbose:
                    print("Skipping path:", filename_next, "that escapes:", path_restrict)
                continue
            yield member, filename_orig, filename_next

    for member, filename_orig, filename_next in zip_iter_filtered(verbose=True):
        # Increment before extracting, so a potential cleanup will a file that failed to extract.
        member_index += 1

        member.filename = filename_next

        data_transform = None if data_map is None else data_map.get(filename_orig)

        filepath_native = path_restrict + sep + filename_next.replace("/", sep)

        # Extraction can fail for many reasons, see: #132924.
        try:
            if data_transform is not None:
                with open(filepath_native, "wb") as fh:
                    fh.write(data_transform)
            else:
                zip_fh.extract(member, path_restrict)
        except Exception as ex:
            error_fn(ex)

            print("Failed to extract path:", filepath_native, "error", str(ex))
            remove_error_fn(filepath_native, ex)
            has_error = True

        member.filename = filename_orig

        if has_error:
            break

    # If the zip-file failed to extract, remove all files that were extracted.
    # This is done so failure to extract a file never results in a partially-working
    # state which can cause confusing situations for users.
    if has_error:
        # NOTE: this currently leaves empty directories which is not ideal.
        # It's possible to calculate directories created by this extraction but more involved.
        member_cleanup_len = member_index + 1
        member_index = 0

        for member, filename_orig, filename_next in zip_iter_filtered(verbose=False):
            member_index += 1
            if member_index >= member_cleanup_len:
                break

            filepath_native = path_restrict + sep + filename_next.replace("/", sep)
            try:
                os.unlink(filepath_native)
            except Exception as ex:
                remove_error_fn(filepath_native, ex)


# -----------------------------------------------------------------------------
# Wheel Utilities


WHEEL_VERSION_RE = re.compile(r"(\d+)?(?:\.(\d+))?(?:\.(\d+))")


def wheel_version_from_filename_for_cmp(
    filename: str,
) -> tuple[int, int, int, str]:
    """
    Extract the version number for comparison.
    Note that this only handled the first 3 numbers,
    the trailing text is compared as a string which is not technically correct
    however this is not a priority to support since scripts should only be including stable releases,
    so comparing the first 3 numbers is sufficient. The trailing string is just a tie breaker in the
    unlikely event it differs.

    If supporting the full spec, comparing: "1.1.dev6" with "1.1.6rc6" for e.g.
    we could support this doesn't seem especially important as extensions should use major releases.
    """
    filename_split = filename.split("-")
    if len(filename_split) >= 2:
        version = filename.split("-")[1]
        if (version_match := WHEEL_VERSION_RE.match(version)) is not None:
            groups = version_match.groups()
            # print(groups)
            return (
                int(groups[0]) if groups[0] is not None else 0,
                int(groups[1]) if groups[1] is not None else 0,
                int(groups[2]) if groups[2] is not None else 0,
                version[version_match.end():],
            )
    return (0, 0, 0, "")


def wheel_list_deduplicate_as_skip_set(
        wheel_list: list[WheelSource],
) -> set[str]:
    """
    Return all wheel paths to skip.
    """
    wheels_to_skip: set[str] = set()
    all_wheels: set[str] = {
        filepath
        for _, wheels in wheel_list
        for filepath in wheels
    }

    # NOTE: this is not optimized.
    # Probably speed is never an issue here, but this could be sped up.

    # Keep a map from the base name to the "best" wheel,
    # the other wheels get added to `wheels_to_skip` to be ignored.
    all_wheels_by_base: dict[str, str] = {}

    for wheel in all_wheels:
        wheel_filename = os.path.basename(wheel)
        wheel_base = wheel_filename.partition("-")[0]

        wheel_exists = all_wheels_by_base.get(wheel_base)
        if wheel_exists is None:
            all_wheels_by_base[wheel_base] = wheel
            continue

        wheel_exists_filename = os.path.basename(wheel_exists)
        if wheel_exists_filename == wheel_filename:
            # Should never happen because they are converted into a set before looping.
            assert wheel_exists != wheel
            # The same wheel is used in two different locations, use a tie breaker for predictability
            # although the result should be the same.
            if wheel_exists_filename < wheel_filename:
                all_wheels_by_base[wheel_base] = wheel
                wheels_to_skip.add(wheel_exists)
            else:
                wheels_to_skip.add(wheel)
        else:
            wheel_version = wheel_version_from_filename_for_cmp(wheel_filename)
            wheel_exists_version = wheel_version_from_filename_for_cmp(wheel_exists_filename)
            if (
                    (wheel_exists_version < wheel_version) or
                    # Tie breaker for predictability.
                    ((wheel_exists_version == wheel_version) and (wheel_exists_filename < wheel_filename))
            ):
                all_wheels_by_base[wheel_base] = wheel
                wheels_to_skip.add(wheel_exists)
            else:
                wheels_to_skip.add(wheel)

    return wheels_to_skip


# -----------------------------------------------------------------------------
# Public Function to Apply Wheels

def apply_action(
        *,
        local_dir: str,
        local_dir_site_packages: str,
        wheel_list: list[WheelSource],
        error_fn: Callable[[Exception], None],
        remove_error_fn: Callable[[str, Exception], None],
        debug: bool,
) -> None:
    """
    :arg local_dir:
       The location wheels are stored.
       Typically: ``~/.config/blender/4.2/extensions/.local``.

       WARNING: files under this directory may be removed.
    :arg local_dir_site_packages:
       The path which wheels are extracted into.
       Typically: ``~/.config/blender/4.2/extensions/.local/lib/python3.11/site-packages``.
    """

    # NOTE: we could avoid scanning the wheel directories however:
    # Recursively removing all paths on the users system can be considered relatively risky
    # even if this is located in a known location under the users home directory - better avoid.
    # So build a list of wheel paths and only remove the unused paths from this list.
    wheels_installed, _paths_unknown = _wheels_from_dir(local_dir_site_packages)

    # Wheels and their top level directories (which would be installed).
    wheels_packages: dict[str, list[str]] = {}

    # Map the wheel ID to path.
    wheels_dir_info_to_filepath_map: dict[str, str] = {}

    # NOTE(@ideasman42): the wheels skip-set only de-duplicates at the level of the base-name of the wheels filename.
    # So the wheel file-paths:
    # - `pip-24.0-py3-none-any.whl`
    # - `pip-22.1-py2-none-any.whl`
    # Will both extract the *base* name `pip`, de-duplicating by skipping the wheels with an older version number.
    # This is not fool-proof, because it is possible files inside the `.whl` conflict upon extraction.
    # In practice I consider this fairly unlikely because:
    # - Practically all wheels extract to their top-level module names.
    # - Modules are mainly downloaded from the Python package index.
    #
    # Having two modules conflict is possible but this is an issue outside of Blender,
    # as it's most likely quite rare and generally avoided with unique module names,
    # this is not considered a problem to "solve" at the moment.
    #
    # The one exception to this assumption is any extensions that bundle `.whl` files that aren't
    # available on the Python package index. In this case naming collisions are more likely.
    # This probably needs to be handled on a policy level - if the `.whl` author also maintains
    # the extension they can in all likelihood make the module a sub-module of the extension
    # without the need to use `.whl` files.
    wheels_to_skip = wheel_list_deduplicate_as_skip_set(wheel_list)

    for _key, wheels in wheel_list:
        for wheel in wheels:
            if wheel in wheels_to_skip:
                continue
            if (wheel_info := _wheel_info_dir_from_zip(wheel)) is None:
                continue
            dir_info, toplevel_paths_list = wheel_info
            wheels_packages[dir_info] = toplevel_paths_list

            wheels_dir_info_to_filepath_map[dir_info] = wheel

    # Now there is two sets of packages, the ones we need and the ones we have.

    # -----
    # Clear

    # First remove installed packages no longer needed:
    for dir_info, toplevel_paths_list in wheels_installed.items():
        if dir_info in wheels_packages:
            continue

        # Remove installed packages which aren't needed any longer.
        for filepath_rel in (dir_info, *toplevel_paths_list):
            filepath_abs = os.path.join(local_dir_site_packages, filepath_rel)
            if not os.path.exists(filepath_abs):
                continue

            if debug:
                print("removing wheel:", filepath_rel)

            ex: Exception | None = None
            if os.path.isdir(filepath_abs):
                ex = _rmtree_safe(filepath_abs, local_dir)
                # For symbolic-links, use remove as a fallback.
                if ex is not None:
                    if _remove_safe(filepath_abs) is None:
                        ex = None
            else:
                ex = _remove_safe(filepath_abs)

            if ex:
                if debug:
                    print("failed to remove:", filepath_rel, str(ex), "setting stale")

                # If the directory (or file) can't be removed, make it stale and try to remove it later.
                remove_error_fn(filepath_abs, ex)

    # -----
    # Setup

    # Install packages that need to be installed:
    for dir_info, toplevel_paths_list in wheels_packages.items():
        if dir_info in wheels_installed:
            continue

        if debug:
            for filepath_rel in toplevel_paths_list:
                print("adding wheel:", filepath_rel)
        filepath = wheels_dir_info_to_filepath_map[dir_info]
        # `ZipFile.extractall` is needed because some wheels contain paths that point to parent directories.
        # Handle this *safely* by allowing extracting to parent directories but limit this to the `local_dir`.

        try:
            # pylint: disable-next=consider-using-with
            zip_fh_context = zipfile.ZipFile(filepath, mode="r")
        except Exception as ex:
            print("Error ({:s}) opening zip-file: {:s}".format(str(ex), filepath))
            error_fn(ex)
            continue

        with contextlib.closing(zip_fh_context) as zip_fh:

            # Support non `Root-is-purelib` wheels, where the data needs to be remapped, see: .
            # Typically `data_map` will be none, see: #132843 for the use case that requires this functionality.
            #
            # NOTE: these wheels should be included in tests (generated and checked to properly install).
            # Unfortunately there doesn't seem to a be practical way to generate them using the `wheel` module.
            data_map = _wheel_zipfile_normalize(
                zip_fh,
                error_fn=error_fn,
            )

            _zipfile_extractall_safe(
                zip_fh,
                local_dir_site_packages,
                local_dir,
                error_fn=error_fn,
                remove_error_fn=remove_error_fn,
                data_map=data_map,
            )
