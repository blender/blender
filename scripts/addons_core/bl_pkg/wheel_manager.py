# SPDX-FileCopyrightText: 2024 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

# Ref: https://peps.python.org/pep-0491/
#      Deferred but seems to include valid info for existing wheels.

"""
This module takes wheels and applies them to a "managed" destination directory.
"""

__all__ = (
    "apply_action"
)

import os
import re
import shutil
import zipfile

from typing import (
    Dict,
    List,
    Optional,
    Set,
    Tuple,
)

WheelSource = Tuple[
    # Key - doesn't matter what this is... it's just a handle.
    str,
    # A list of absolute wheel file-paths.
    List[str],
]


def _read_records_csv(filepath: str) -> List[List[str]]:
    import csv
    with open(filepath, encoding="utf8", errors="surrogateescape") as fh:
        return list(csv.reader(fh.read().splitlines()))


def _wheels_from_dir(dirpath: str) -> Tuple[
        # The key is:
        #   wheel_id
        # The values are:
        #   Top level directories.
        Dict[str, List[str]],
        # Unknown path.s
        List[str],
]:
    result: Dict[str, List[str]] = {}
    paths_unused: Set[str] = set()

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
        toplevel_paths_set: Set[str] = set()
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

        result[name] = list(sorted(toplevel_paths_set))
        del toplevel_paths_set

    for wheel_name, toplevel_paths in result.items():
        paths_unused.discard(wheel_name)
        for name in toplevel_paths:
            paths_unused.discard(name)

    paths_unused_list = list(sorted(paths_unused))

    return result, paths_unused_list


def _wheel_info_dir_from_zip(filepath_wheel: str) -> Optional[Tuple[str, List[str]]]:
    """
    Return:
    - The "*-info" directory name which contains meta-data.
    - The top-level path list (excluding "..").
    """
    dir_info = ""
    toplevel_paths: Set[str] = set()

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


def _rmtree_safe(dir_remove: str, expected_root: str) -> None:
    if not dir_remove.startswith(expected_root):
        raise Exception("Expected prefix not found")
    shutil.rmtree(dir_remove)


def _zipfile_extractall_safe(
        zip_fh: zipfile.ZipFile,
        path: str,
        path_restrict: str,
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
        raise Exception("Expected the restricted directory to start with ")

    for member in zip_fh.infolist():
        filename_orig = member.filename
        member.filename = path_prefix + filename_orig
        # This isn't likely to happen so accept a noisy print here.
        # If this ends up happening more often, it could be suppressed.
        # (although this hints at bigger problems because we might be excluding necessary files).
        if os.path.normpath(member.filename).startswith(".." + sep):
            print("Skipping path:", member.filename, "that escapes:", path_restrict)
            continue
        zip_fh.extract(member, path_restrict)
        member.filename = filename_orig


WHEEL_VERSION_RE = re.compile(r"(\d+)?(?:\.(\d+))?(?:\.(\d+))")


def wheel_version_from_filename_for_cmp(
    filename: str,
) -> Tuple[int, int, int, str]:
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
        wheel_list: List[WheelSource],
) -> Set[str]:
    """
    Return all wheel paths to skip.
    """
    wheels_to_skip: Set[str] = set()
    all_wheels: Set[str] = {
        filepath
        for _, wheels in wheel_list
        for filepath in wheels
    }

    # NOTE: this is not optimized.
    # Probably speed is never an issue here, but this could be sped up.

    # Keep a map from the base name to the "best" wheel,
    # the other wheels get added to `wheels_to_skip` to be ignored.
    all_wheels_by_base: Dict[str, str] = {}

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


def apply_action(
        *,
        local_dir: str,
        local_dir_site_packages: str,
        wheel_list: List[WheelSource],
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
    debug = False

    # NOTE: we could avoid scanning the wheel directories however:
    # Recursively removing all paths on the users system can be considered relatively risky
    # even if this is located in a known location under the users home directory - better avoid.
    # So build a list of wheel paths and only remove the unused paths from this list.
    wheels_installed, paths_unknown = _wheels_from_dir(local_dir_site_packages)

    # Wheels and their top level directories (which would be installed).
    wheels_packages: Dict[str, List[str]] = {}

    # Map the wheel ID to path.
    wheels_dir_info_to_filepath_map: Dict[str, str] = {}

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

    for key, wheels in wheel_list:
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

            if os.path.isdir(filepath_abs):
                _rmtree_safe(filepath_abs, local_dir)
            else:
                os.remove(filepath_abs)

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
        with zipfile.ZipFile(filepath, mode="r") as zip_fh:
            _zipfile_extractall_safe(zip_fh, local_dir_site_packages, local_dir)
