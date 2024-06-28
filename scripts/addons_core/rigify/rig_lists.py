# SPDX-FileCopyrightText: 2013-2022 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import os
import traceback
import importlib
import typing

from typing import Optional, Iterable

from .utils.rig import RIG_DIR

from . import feature_set_list


def get_rigs(base_dir: str, base_path: list[str], *,
             path: Iterable[str] = (),
             feature_set=feature_set_list.DEFAULT_NAME):
    """ Recursively searches for rig types, and returns a list.

    Args:
        base_dir:      root directory
        base_path:     base dir where rigs are stored
        path:          rig path inside the base dir
        feature_set:   feature set that is being loaded
    """

    rig_table = {}
    impl_rigs = {}

    dir_path = os.path.join(base_dir, *path)

    try:
        files = os.listdir(dir_path)
    except FileNotFoundError:
        files = []

    files.sort()

    for f in files:
        is_dir = os.path.isdir(os.path.join(dir_path, f))  # Whether the file is a directory

        # Stop cases
        if f[0] in [".", "_"]:
            continue
        if f.count(".") >= 2 or (is_dir and "." in f):
            print("Warning: %r, filename contains a '.', skipping" % os.path.join(*base_path, *path, f))
            continue

        if is_dir:
            # Check for sub-rigs
            sub_rigs, sub_impls = get_rigs(base_dir, base_path, path=[*path, f], feature_set=feature_set)
            rig_table.update(sub_rigs)
            impl_rigs.update(sub_impls)
        elif f.endswith(".py"):
            # Check straight-up python files
            sub_path = [*path, f[:-3]]
            key = '.'.join(sub_path)
            # Don't reload rig modules - it breaks isinstance
            rig_module = importlib.import_module('.'.join(base_path + sub_path))
            if hasattr(rig_module, "Rig"):
                rig_table[key] = {"module": rig_module,
                                  "feature_set": feature_set}
            if hasattr(rig_module, 'IMPLEMENTATION') and rig_module.IMPLEMENTATION:
                impl_rigs[key] = rig_module

    return rig_table, impl_rigs


# Public variables
rigs = {}
implementation_rigs = {}


def get_rig_class(name: str) -> Optional[typing.Type]:
    try:
        return rigs[name]["module"].Rig
    except (KeyError, AttributeError):
        return None


def get_internal_rigs():
    global rigs, implementation_rigs

    base_rigify_dir = os.path.dirname(__file__)
    base_rigify_path = __name__.split('.')[:-1]

    rigs, implementation_rigs = get_rigs(os.path.join(base_rigify_dir, RIG_DIR),
                                         [*base_rigify_path, RIG_DIR])


def get_external_rigs(set_list):
    # Clear and fill rigify rigs and implementation rigs public variables
    for rig in list(rigs.keys()):
        if rigs[rig]["feature_set"] != feature_set_list.DEFAULT_NAME:
            rigs.pop(rig)
            if rig in implementation_rigs:
                implementation_rigs.pop(rig)

    # Get external rigs
    for feature_set in set_list:
        # noinspection PyBroadException
        try:
            base_dir, base_path = feature_set_list.get_dir_path(feature_set, RIG_DIR)

            external_rigs, external_impl_rigs = get_rigs(base_dir, base_path, feature_set=feature_set)
        except Exception:
            print(f"Rigify Error: Could not load feature set '{feature_set}' rigs: exception occurred.\n")
            traceback.print_exc()
            print("")
            feature_set_list.mark_feature_set_exception(feature_set)
            continue

        rigs.update(external_rigs)
        implementation_rigs.update(external_impl_rigs)
