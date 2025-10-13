# SPDX-FileCopyrightText: 2010-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# This file defines a set of methods that are useful for various
# Relative Keying Set (RKS) related operations, such as: callbacks
# for polling, iterator callbacks, and also generate callbacks.
# All of these can be used in conjunction with the others.

__all__ = (
    "path_add_property",
    "RKS_POLL_selected_objects",
    "RKS_POLL_selected_bones",
    "RKS_POLL_selected_items",
    "RKS_ITER_selected_objects",
    "RKS_ITER_selected_bones",
    "RKS_ITER_selected_item",
    "RKS_GEN_available",
    "RKS_GEN_custom_props",
    "RKS_GEN_location",
    "RKS_GEN_rotation",
    "RKS_GEN_scaling",
    "RKS_GEN_bendy_bones",
)

import bpy


###########################
# General Utilities


# Append the specified property name on the existing path.
def path_add_property(path, prop):
    if path:
        return path + "." + prop
    else:
        return prop

###########################
# Poll Callbacks


# selected objects (active object must be in object mode)
def RKS_POLL_selected_objects(_ksi, context):
    if context.area.type == 'SEQUENCE_EDITOR':
        return False

    ob = context.active_object
    if ob:
        return ob.mode == 'OBJECT'
    else:
        return bool(context.selected_objects)


# selected bones
def RKS_POLL_selected_bones(_ksi, context):
    if context.area.type == 'SEQUENCE_EDITOR':
        return False

    # we must be in Pose Mode, and there must be some bones selected
    ob = context.active_object
    if ob and ob.mode == 'POSE':
        if context.active_pose_bone or context.selected_pose_bones:
            return True

    # nothing selected
    return False

# selected vse strip


def RKS_POLL_selected_strip(_ksi, context):
    if context.active_strip or context.selected_strips:
        return True

    # nothing selected
    return False


# selected bones, objects or strips
def RKS_POLL_selected_items(ksi, context):
    return (RKS_POLL_selected_bones(ksi, context) or
            RKS_POLL_selected_objects(ksi, context) or
            RKS_POLL_selected_strip(ksi, context))

# selected bones or objects


def RKS_POLL_selected_bones_or_objects(ksi, context):
    return (RKS_POLL_selected_bones(ksi, context) or
            RKS_POLL_selected_objects(ksi, context))

###########################
# Iterator Callbacks


# All selected objects or pose bones, depending on which we've got.
def RKS_ITER_selected_item(ksi, context, ks):
    if context.area.type == 'SEQUENCE_EDITOR':
        if context.selected_strips:
            for strip in context.selected_strips:
                ksi.generate(context, ks, strip)
        return

    ob = context.active_object
    if ob and ob.mode == 'POSE':
        for bone in context.selected_pose_bones:
            ksi.generate(context, ks, bone)
    elif context.selected_objects:
        for ob in context.selected_objects:
            ksi.generate(context, ks, ob)


# All selected objects only.
def RKS_ITER_selected_objects(ksi, context, ks):
    for ob in context.selected_objects:
        ksi.generate(context, ks, ob)


# All selected bones only.
def RKS_ITER_selected_bones(ksi, context, ks):
    for bone in context.selected_pose_bones:
        ksi.generate(context, ks, bone)

###########################
# Generate Callbacks


# "Available" F-Curves.
def RKS_GEN_available(_ksi, _context, ks, data):
    from bpy_extras import anim_utils

    # try to get the animation data associated with the closest
    # ID-block to the data (neither of which may exist/be easy to find)
    id_block = data.id_data
    adt = getattr(id_block, "animation_data", None)

    # there must also be an active action...
    if adt is None or adt.action is None:
        return

    # if we haven't got an ID-block as 'data', try to restrict
    # paths added to only those which branch off from here
    # i.e. for bones
    if id_block != data:
        basePath = data.path_from_id()
    else:
        basePath = None  # this is not needed...

    # for each F-Curve, include a path to key it
    # NOTE: we don't need to set the group settings here
    cbag = anim_utils.action_get_channelbag_for_slot(adt.action, adt.action_slot)
    if not cbag:
        return

    for fcu in cbag.fcurves:
        if basePath:
            if basePath in fcu.data_path:
                ks.paths.add(id_block, fcu.data_path, index=fcu.array_index)
        else:
            ks.paths.add(id_block, fcu.data_path, index=fcu.array_index)

# ------


# get ID block and based ID path for transform generators
# private function
def get_transform_generators_base_info(data):
    # ID-block for the data
    id_block = data.id_data

    # get base path and grouping method/name
    if isinstance(data, bpy.types.ID):
        # no path in this case
        path = ""

        # transform data on ID-blocks directly should get grouped under a
        # hardcoded label ("Object Transforms") so that they get grouped
        # consistently when keyframed directly
        grouping = "Object Transforms"
    else:
        # get the path to the ID-block
        path = data.path_from_id()

        # try to use the name of the data element to group the F-Curve
        # else fall back on the KeyingSet name
        grouping = getattr(data, "name", None)

    # return the ID-block and the path
    return id_block, path, grouping


# Location
def RKS_GEN_location(_ksi, _context, ks, data):
    # get id-block and path info
    id_block, base_path, grouping = get_transform_generators_base_info(data)

    if isinstance(data, bpy.types.Strip):
        path_x = path_add_property(base_path, "transform.offset_x")
        path_y = path_add_property(base_path, "transform.offset_y")
        if grouping:
            ks.paths.add(id_block, path_x, group_method='NAMED', group_name=grouping)
            ks.paths.add(id_block, path_y, group_method='NAMED', group_name=grouping)
        else:
            ks.paths.add(id_block, path_x)
            ks.paths.add(id_block, path_y)
        return

    # add the property name to the base path
    path = path_add_property(base_path, "location")

    # add Keying Set entry for this...
    if grouping:
        ks.paths.add(id_block, path, group_method='NAMED', group_name=grouping)
    else:
        ks.paths.add(id_block, path)


# Rotation
def RKS_GEN_rotation(_ksi, _context, ks, data):
    # get id-block and path info
    id_block, base_path, grouping = get_transform_generators_base_info(data)

    # add the property name to the base path
    if isinstance(data, bpy.types.Strip):
        path = path_add_property(base_path, "transform.rotation")
        if grouping:
            ks.paths.add(id_block, path, group_method='NAMED', group_name=grouping)
        else:
            ks.paths.add(id_block, path)
        return
    #   rotation mode affects the property used
    if data.rotation_mode == 'QUATERNION':
        path = path_add_property(base_path, "rotation_quaternion")
    elif data.rotation_mode == 'AXIS_ANGLE':
        path = path_add_property(base_path, "rotation_axis_angle")
    else:
        path = path_add_property(base_path, "rotation_euler")

    # add Keying Set entry for this...
    if grouping:
        ks.paths.add(id_block, path, group_method='NAMED', group_name=grouping)
    else:
        ks.paths.add(id_block, path)


# Scaling
def RKS_GEN_scaling(_ksi, _context, ks, data):
    # get id-block and path info
    id_block, base_path, grouping = get_transform_generators_base_info(data)

    if isinstance(data, bpy.types.Strip):
        path_x = path_add_property(base_path, "transform.scale_x")
        path_y = path_add_property(base_path, "transform.scale_y")
        if grouping:
            ks.paths.add(id_block, path_x, group_method='NAMED', group_name=grouping)
            ks.paths.add(id_block, path_y, group_method='NAMED', group_name=grouping)
        else:
            ks.paths.add(id_block, path_x)
            ks.paths.add(id_block, path_y)
        return
    # add the property name to the base path
    path = path_add_property(base_path, "scale")

    # add Keying Set entry for this...
    if grouping:
        ks.paths.add(id_block, path, group_method='NAMED', group_name=grouping)
    else:
        ks.paths.add(id_block, path)


# Custom Properties
def RKS_GEN_custom_props(_ksi, _context, ks, data):
    # get id-block and path info
    id_block, base_path, grouping = get_transform_generators_base_info(data)

    # Only some RNA types can be animated.
    prop_type_compat = {bpy.types.BoolProperty,
                        bpy.types.IntProperty,
                        bpy.types.FloatProperty,
                        bpy.types.EnumProperty}

    # When working with a pose, 'id_block' is the armature object (which should
    # get the animation data), whereas 'data' is the bone being keyed.
    for cprop_name in data.keys():
        # ignore special "_RNA_UI" used for UI editing
        if cprop_name == "_RNA_UI":
            continue
        if cprop_name in data.bl_rna.properties and not data.bl_rna.properties[cprop_name].is_animatable:
            continue

        if cprop_name in data.bl_rna.properties:
            prop_path = cprop_name
        else:
            prop_path = '["{:s}"]'.format(bpy.utils.escape_identifier(cprop_name))

        try:
            rna_property = data.path_resolve(prop_path, False)
        except ValueError:
            # Can technically happen, but there is no known case.
            continue
        if rna_property is None:
            # In this case the property cannot be converted to an
            # FCurve-compatible value, so we can't keyframe it anyways.
            continue
        if rna_property.rna_type not in prop_type_compat:
            continue

        path = "{:s}{:s}".format(base_path, prop_path)
        if grouping:
            ks.paths.add(id_block, path, group_method='NAMED', group_name=grouping)
        else:
            ks.paths.add(id_block, path)

# ------


# Property identifiers for Bendy Bones
bbone_property_ids = (
    "bbone_curveinx",
    "bbone_curveiny",
    "bbone_curveoutx",
    "bbone_curveouty",

    "bbone_rollin",
    "bbone_rollout",

    "bbone_scalein",
    "bbone_scaleout",

    # NOTE: These are in the nested bone struct
    # Do it this way to force them to be included
    # in whatever actions are being keyed here
    "bone.bbone_in",
    "bone.bbone_out",
)


# Add Keying Set entries for bendy bones
def RKS_GEN_bendy_bones(_ksi, _context, ks, data):
    # get id-block and path info
    # NOTE: This assumes that we're dealing with a bone here...
    id_block, base_path, grouping = get_transform_generators_base_info(data)

    # for each of the bendy bone properties, add a Keying Set entry for it...
    for propname in bbone_property_ids:
        # add the property name to the base path
        path = path_add_property(base_path, propname)

        # add Keying Set entry for this...
        if grouping:
            ks.paths.add(id_block, path, group_method='NAMED', group_name=grouping)
        else:
            ks.paths.add(id_block, path)
