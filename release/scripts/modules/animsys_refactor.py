# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>

"""
This module has utility functions for renaming
rna values in fcurves and drivers.

Currently unused, but might become useful later again.
"""

import sys
import bpy


IS_TESTING = False


def drepr(string):
    # is there a less crappy way to do this in python?, re.escape also escapes
    # single quotes strings so cant use it.
    return '"%s"' % repr(string)[1:-1].replace("\"", "\\\"").replace("\\'", "'")


def classes_recursive(base_type, clss=None):
    if clss is None:
        clss = [base_type]
    else:
        clss.append(base_type)

    for base_type_iter in base_type.__bases__:
        if base_type_iter is not object:
            classes_recursive(base_type_iter, clss)

    return clss


class DataPathBuilder(object):
    """Dummy class used to parse fcurve and driver data paths."""
    __slots__ = ("data_path", )

    def __init__(self, attrs):
        self.data_path = attrs

    def __getattr__(self, attr):
        str_value = ".%s" % attr
        return DataPathBuilder(self.data_path + (str_value, ))

    def __getitem__(self, key):
        if type(key) is int:
            str_value = '[%d]' % key
        elif type(key) is str:
            str_value = '[%s]' % drepr(key)
        else:
            raise Exception("unsupported accessor %r of type %r (internal error)" % (key, type(key)))
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
                    for class_name, item_new, options in rna_update_from_map.get(item[1:], []) + [(None, item[1:], None)]:
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
                                    #print("base." + item_new)
                                    base_new = eval("base." + item_new)
                                    break  # found, don't keep looking
                                except:
                                    pass
                    item_new = "." + item_new
                else:
                    item_new = item
                    try:
                        base_new = eval("base" + item_new)
                    except:
                        pass

                if base_new is Ellipsis:
                    print("Failed to resolve data path:", self.data_path, file=log)
                base = base_new
            else:
                item_new = item

            pairs.append((item_new, base))
        return pairs


def id_iter():
    type_iter = type(bpy.data.objects)

    for attr in dir(bpy.data):
        data_iter = getattr(bpy.data, attr, None)
        if type(data_iter) == type_iter:
            for id_data in data_iter:
                if id_data.library is None:
                    yield id_data


def anim_data_actions(anim_data):
    actions = []
    actions.append(anim_data.action)
    for track in anim_data.nla_tracks:
        for strip in track.strips:
            actions.append(strip.action)

    # filter out None
    return [act for act in actions if act]


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

    rna_update_from_map = {}
    for ren_class, ren_from, ren_to, options in rna_update:
        rna_update_from_map.setdefault(ren_from, []).append((ren_class, ren_to, options))

    for id_data in id_iter():
        # check node-trees too
        anim_data_ls = [(id_data, getattr(id_data, "animation_data", None))]
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
                    print("driver-fcurve (%s): %s -> %s" % (id_data.name, data_path, data_path_new), file=log)

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
                                    print("driver (%s): %s -> %s" % (id_data_other.name, data_path, data_path_new),
                                          file=log)

            for action in anim_data_actions(anim_data):
                for fcu in action.fcurves:
                    data_path = fcu.data_path
                    data_path_new = find_path_new(anim_data_base, data_path, rna_update_from_map, fcu, log)
                    # print(data_path_new)
                    if data_path_new != data_path:
                        if not IS_TESTING:
                            fcu.data_path = data_path_new
                        print("fcurve (%s): %s -> %s" % (id_data.name, data_path, data_path_new), file=log)


if __name__ == "__main__":

    # Example, should be called externally
    # (class, from, to or to_callback, callback_options)
    replace_ls = [
        ("AnimVizMotionPaths", "frame_after", "frame_after", None),
        ("AnimVizMotionPaths", "frame_before", "frame_before", None),
        ("AnimVizOnionSkinning", "frame_after", "frame_after", None),
    ]

    update_data_paths(replace_ls)
