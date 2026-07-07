# SPDX-FileCopyrightText: 2010-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
This module has utility functions for renaming
rna values in fcurves and drivers.

Currently unused, but might become useful later again.
"""
__all__ = (
    "update_data_paths",
)

import sys

import bpy

IS_TESTING = False


def classes_recursive(base_type, clss=None):
    if clss is None:
        clss = [base_type]
    else:
        clss.append(base_type)

    for base_type_iter in base_type.__bases__:
        if base_type_iter is not object:
            classes_recursive(base_type_iter, clss)

    return clss


class DataPathBuilder:
    """Dummy class used to parse fcurve and driver data paths."""
    __slots__ = ("data_path", )

    def __init__(self, attrs):
        self.data_path = attrs

    def __getattr__(self, attr):
        str_value = ".{:s}".format(attr)
        return DataPathBuilder(self.data_path + (str_value, ))

    def __getitem__(self, key):
        if type(key) is int:
            str_value = '[{:d}]'.format(key)
        elif type(key) is str:
            str_value = '["{:s}"]'.format(bpy.utils.escape_identifier(key))
        else:
            raise Exception("unsupported accessor {!r} of type {!r} (internal error)".format(key, type(key)))
        return DataPathBuilder(self.data_path + (str_value, ))

    def resolve(self, real_base, rna_update_from_map, fcurve, log):
        """Return (attribute, value) pairs."""
        pairs = []
        base = real_base
        for item in self.data_path:
            if base is not Ellipsis:
                base_new = Ellipsis
                # find the new name
                if item.startswith("."):
                    for class_name, item_new, options in (
                            rna_update_from_map.get(item[1:], []) +
                            [(None, item[1:], None)]
                    ):
                        if callable(item_new):
                            # No type check here, callback is assumed to know what it's doing.
                            base_new, item_new = item_new(base, class_name, item[1:], fcurve, options)
                            if base_new is not Ellipsis:
                                break  # found, don't keep looking
                        else:
                            # Type check!
                            type_ok = True
                            if class_name is not None:
                                type_ok = False
                                for base_type in classes_recursive(type(base)):
                                    if base_type.__name__ == class_name:
                                        type_ok = True
                                        break
                            if type_ok:
                                try:
                                    # print("base." + item_new)
                                    base_new = eval("base." + item_new)
                                    break  # found, don't keep looking
                                except Exception:
                                    pass
                    item_new = "." + item_new
                else:
                    item_new = item
                    try:
                        base_new = eval("base" + item_new)
                    except Exception:
                        pass

                if base_new is Ellipsis:
                    print("Failed to resolve data path:", self.data_path, file=log)
                base = base_new
            else:
                item_new = item

            pairs.append((item_new, base))
        return pairs


def id_iter():
    from bpy.types import bpy_prop_collection
    assert isinstance(bpy.data.objects, bpy_prop_collection)

    for attr in dir(bpy.data):
        data_iter = getattr(bpy.data, attr, None)
        if isinstance(data_iter, bpy_prop_collection):
            for id_data in data_iter:
                if id_data.library is None:
                    yield id_data


def anim_data_actions(anim_data) -> list[tuple[bpy.types.Action, bpy.types.ActionSlot]]:
    actions = []
    actions.append((anim_data.action, anim_data.action_slot))
    for track in anim_data.nla_tracks:
        for strip in track.strips:
            actions.append((strip.action, strip.action_slot))

    # Filter out None actions/slots, because if either is None, there is no animation.
    return [(act, slot) for (act, slot) in actions if act and slot]


def find_path_new(id_data, data_path, rna_update_from_map, fcurve, log):
    # note!, id_data can be ID type or a node tree
    # ignore ID props for now
    if data_path.startswith("["):
        return data_path

    # recursive path fixing, likely will be one in most cases.
    data_path_builder = eval("DataPathBuilder(tuple())." + data_path)
    data_resolve = data_path_builder.resolve(id_data, rna_update_from_map, fcurve, log)

    path_new = [pair[0] for pair in data_resolve]

    return "".join(path_new)[1:]  # skip the first "."


def update_data_paths(rna_update, log=sys.stdout):
    """
    rna_update triple [(class_name, from, to or to_callback, callback options), ...]
    to_callback is a function with this signature: update_cb(base, class_name, old_path, fcurve, options)
                where base is current object, class_name is the expected type name of base (callback has to handle
                this), old_path it the org name of base's property, fcurve is the affected fcurve (!),
                and options is an opaque data.
                class_name, fcurve and options may be None!
    """
    from bpy_extras import anim_utils

    rna_update_from_map = {}
    for ren_class, ren_from, ren_to, options in rna_update:
        rna_update_from_map.setdefault(ren_from, []).append((ren_class, ren_to, options))

    for id_data in id_iter():
        anim_data_ls: list[tuple[bpy.types.ID, bpy.types.AnimData | None]] = [
            (id_data, getattr(id_data, "animation_data", None))]
        # check node-trees too
        node_tree = getattr(id_data, "node_tree", None)
        if node_tree:
            anim_data_ls.append((node_tree, node_tree.animation_data))

        for anim_data_base, anim_data in anim_data_ls:
            if anim_data is None:
                continue

            for fcurve in anim_data.drivers:
                data_path = fcurve.data_path
                data_path_new = find_path_new(anim_data_base, data_path, rna_update_from_map, fcurve, log)
                # print(data_path_new)
                if data_path_new != data_path:
                    if not IS_TESTING:
                        fcurve.data_path = data_path_new
                        fcurve.driver.is_valid = True  # reset to allow this to work again
                    print(
                        "driver-fcurve ({:s}): {:s} -> {:s}".format(id_data.name, data_path, data_path_new),
                        file=log,
                    )

                for var in fcurve.driver.variables:
                    if var.type == 'SINGLE_PROP':
                        for tar in var.targets:
                            id_data_other = tar.id
                            data_path = tar.data_path

                            if id_data_other and data_path:
                                data_path_new = find_path_new(id_data_other, data_path, rna_update_from_map, None, log)
                                # print(data_path_new)
                                if data_path_new != data_path:
                                    if not IS_TESTING:
                                        tar.data_path = data_path_new
                                    print(
                                        "driver ({:s}): {:s} -> {:s}".format(
                                            id_data_other.name,
                                            data_path,
                                            data_path_new,
                                        ),
                                        file=log,
                                    )

            for action, action_slot in anim_data_actions(anim_data):
                channelbag = anim_utils.action_get_channelbag_for_slot(action, action_slot)
                if not channelbag:
                    continue
                for fcu in channelbag.fcurves:
                    data_path = fcu.data_path
                    data_path_new = find_path_new(anim_data_base, data_path, rna_update_from_map, fcu, log)
                    # print(data_path_new)
                    if data_path_new != data_path:
                        if not IS_TESTING:
                            fcu.data_path = data_path_new
                        print("fcurve ({:s}): {:s} -> {:s}".format(id_data.name, data_path, data_path_new), file=log)


if __name__ == "__main__":

    # Example, should be called externally
    # (class, from, to or to_callback, callback_options)
    replace_ls = [
        ("AnimVizMotionPaths", "frame_after", "frame_after", None),
        ("AnimVizMotionPaths", "frame_before", "frame_before", None),
        ("AnimVizOnionSkinning", "frame_after", "frame_after", None),
    ]

    update_data_paths(replace_ls)
