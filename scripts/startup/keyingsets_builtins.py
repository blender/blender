# SPDX-FileCopyrightText: 2010-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
Built-In Keying Sets
None of these Keying Sets should be removed, as these are needed by various parts of Blender in order for them
to work correctly.

Beware also about changing the order that these are defined here, since this can result in old files referring to the
wrong Keying Set as the active one, potentially resulting in lost (i.e. unkeyed) animation.

Note that these classes cannot be subclassed further; only direct subclasses of KeyingSetInfo
are supported.
"""

import bpy
import _keyingsets_utils as keyingsets_utils
from bpy.types import KeyingSetInfo

###############################
# Built-In KeyingSets


# "Defines"
# Keep these in sync with those in ED_keyframing.hh!
ANIM_KS_LOCATION_ID = "Location"
ANIM_KS_ROTATION_ID = "Rotation"
ANIM_KS_SCALING_ID = "Scaling"
ANIM_KS_LOC_ROT_SCALE_ID = "LocRotScale"
ANIM_KS_LOC_ROT_SCALE_CPROP_ID = "LocRotScaleCProp"
ANIM_KS_AVAILABLE_ID = "Available"
ANIM_KS_WHOLE_CHARACTER_ID = "WholeCharacter"
ANIM_KS_WHOLE_CHARACTER_SELECTED_ID = "WholeCharacterSelected"


# Location
class BUILTIN_KSI_Location(KeyingSetInfo):
    """Insert a keyframe on each of the location channels"""
    bl_idname = ANIM_KS_LOCATION_ID
    bl_label = "Location"

    # poll - use predefined callback for selected bones/objects
    poll = keyingsets_utils.RKS_POLL_selected_items

    # iterator - use callback for selected bones/objects
    iterator = keyingsets_utils.RKS_ITER_selected_item

    # generator - use callback for location
    generate = keyingsets_utils.RKS_GEN_location


# Rotation
class BUILTIN_KSI_Rotation(KeyingSetInfo):
    """Insert a keyframe on each of the rotation channels"""
    bl_idname = ANIM_KS_ROTATION_ID
    bl_label = "Rotation"

    # poll - use predefined callback for selected bones/objects
    poll = keyingsets_utils.RKS_POLL_selected_items

    # iterator - use callback for selected bones/objects
    iterator = keyingsets_utils.RKS_ITER_selected_item

    # generator - use callback for rotation
    generate = keyingsets_utils.RKS_GEN_rotation


# Scale
class BUILTIN_KSI_Scaling(KeyingSetInfo):
    """Insert a keyframe on each of the scale channels"""
    bl_idname = ANIM_KS_SCALING_ID
    bl_label = "Scale"

    # poll - use predefined callback for selected bones/objects
    poll = keyingsets_utils.RKS_POLL_selected_items

    # iterator - use callback for selected bones/objects
    iterator = keyingsets_utils.RKS_ITER_selected_item

    # generator - use callback for scaling
    generate = keyingsets_utils.RKS_GEN_scaling

# ------------


# LocRot
class BUILTIN_KSI_LocRot(KeyingSetInfo):
    """Insert a keyframe on each of the location and rotation channels"""
    bl_label = "Location & Rotation"

    # poll - use predefined callback for selected bones/objects
    poll = keyingsets_utils.RKS_POLL_selected_items

    # iterator - use callback for selected bones/objects
    iterator = keyingsets_utils.RKS_ITER_selected_item

    # generator
    def generate(self, context, ks, data):
        # location
        keyingsets_utils.RKS_GEN_location(self, context, ks, data)
        # rotation
        keyingsets_utils.RKS_GEN_rotation(self, context, ks, data)


# LocScale
class BUILTIN_KSI_LocScale(KeyingSetInfo):
    """Insert a keyframe on each of the location and scale channels"""
    bl_label = "Location & Scale"

    # poll - use predefined callback for selected bones/objects
    poll = keyingsets_utils.RKS_POLL_selected_items

    # iterator - use callback for selected bones/objects
    iterator = keyingsets_utils.RKS_ITER_selected_item

    # generator
    def generate(self, context, ks, data):
        # location
        keyingsets_utils.RKS_GEN_location(self, context, ks, data)
        # scale
        keyingsets_utils.RKS_GEN_scaling(self, context, ks, data)


# LocRotScale
class BUILTIN_KSI_LocRotScale(KeyingSetInfo):
    """Insert a keyframe on each of the location, rotation, and scale channels"""
    bl_idname = ANIM_KS_LOC_ROT_SCALE_ID
    bl_label = "Location, Rotation & Scale"

    # poll - use predefined callback for selected bones/objects
    poll = keyingsets_utils.RKS_POLL_selected_items

    # iterator - use callback for selected bones/objects
    iterator = keyingsets_utils.RKS_ITER_selected_item

    # generator
    def generate(self, context, ks, data):
        # location
        keyingsets_utils.RKS_GEN_location(self, context, ks, data)
        # rotation
        keyingsets_utils.RKS_GEN_rotation(self, context, ks, data)
        # scale
        keyingsets_utils.RKS_GEN_scaling(self, context, ks, data)


# LocRotScaleCProp
class BUILTIN_KSI_LocRotScaleCProp(KeyingSetInfo):
    """Key location/rotation/scale as well as custom properties"""
    bl_idname = ANIM_KS_LOC_ROT_SCALE_CPROP_ID
    bl_label = "Location, Rotation, Scale & Custom Properties"

    poll = keyingsets_utils.RKS_POLL_selected_items
    iterator = keyingsets_utils.RKS_ITER_selected_item

    def generate(self, context, ks, data):
        keyingsets_utils.RKS_GEN_location(self, context, ks, data)
        keyingsets_utils.RKS_GEN_rotation(self, context, ks, data)
        keyingsets_utils.RKS_GEN_scaling(self, context, ks, data)
        keyingsets_utils.RKS_GEN_custom_props(self, context, ks, data)


# RotScale
class BUILTIN_KSI_RotScale(KeyingSetInfo):
    """Insert a keyframe on each of the rotation and scale channels"""
    bl_label = "Rotation & Scale"

    # poll - use predefined callback for selected bones/objects
    poll = keyingsets_utils.RKS_POLL_selected_items

    # iterator - use callback for selected bones/objects
    iterator = keyingsets_utils.RKS_ITER_selected_item

    # generator
    def generate(self, context, ks, data):
        # rotation
        keyingsets_utils.RKS_GEN_rotation(self, context, ks, data)
        # scaling
        keyingsets_utils.RKS_GEN_scaling(self, context, ks, data)

# ------------


# Bendy Bones
class BUILTIN_KSI_BendyBones(KeyingSetInfo):
    """Insert a keyframe for each of the B-Bone shape properties"""
    bl_label = "B-Bone Shape"

    # poll - use callback for selected bones
    poll = keyingsets_utils.RKS_POLL_selected_bones

    # iterator - use callback for selected bones
    iterator = keyingsets_utils.RKS_ITER_selected_bones

    # generator - use generator for bendy bone properties
    generate = keyingsets_utils.RKS_GEN_bendy_bones

# ------------


# VisualLocation
class BUILTIN_KSI_VisualLoc(KeyingSetInfo):
    """Insert a keyframe on each of the location channels, """ \
        """taking into account effects of constraints and relationships"""
    bl_label = "Visual Location"

    bl_options = {'INSERTKEY_VISUAL'}

    # poll - use predefined callback for selected bones/objects
    poll = keyingsets_utils.RKS_POLL_selected_bones_or_objects

    # iterator - use callback for selected bones/objects
    iterator = keyingsets_utils.RKS_ITER_selected_item

    # generator - use callback for location
    generate = keyingsets_utils.RKS_GEN_location


# VisualRotation
class BUILTIN_KSI_VisualRot(KeyingSetInfo):
    """Insert a keyframe on each of the rotation channels, """ \
        """taking into account effects of constraints and relationships"""
    bl_label = "Visual Rotation"

    bl_options = {'INSERTKEY_VISUAL'}

    # poll - use predefined callback for selected bones/objects
    poll = keyingsets_utils.RKS_POLL_selected_bones_or_objects

    # iterator - use callback for selected bones/objects
    iterator = keyingsets_utils.RKS_ITER_selected_item

    # generator - use callback for rotation
    generate = keyingsets_utils.RKS_GEN_rotation


# VisualScaling
class BUILTIN_KSI_VisualScaling(KeyingSetInfo):
    """Insert a keyframe on each of the scale channels, """ \
        """taking into account effects of constraints and relationships"""
    bl_label = "Visual Scale"

    bl_options = {'INSERTKEY_VISUAL'}

    # poll - use predefined callback for selected bones/objects
    poll = keyingsets_utils.RKS_POLL_selected_bones_or_objects

    # iterator - use callback for selected bones/objects
    iterator = keyingsets_utils.RKS_ITER_selected_item

    # generator - use callback for location
    generate = keyingsets_utils.RKS_GEN_scaling


# VisualLocRot
class BUILTIN_KSI_VisualLocRot(KeyingSetInfo):
    """Insert a keyframe on each of the location and rotation channels, """ \
        """taking into account effects of constraints and relationships"""
    bl_label = "Visual Location & Rotation"

    bl_options = {'INSERTKEY_VISUAL'}

    # poll - use predefined callback for selected bones/objects
    poll = keyingsets_utils.RKS_POLL_selected_bones_or_objects

    # iterator - use callback for selected bones/objects
    iterator = keyingsets_utils.RKS_ITER_selected_item

    # generator
    def generate(self, context, ks, data):
        # location
        keyingsets_utils.RKS_GEN_location(self, context, ks, data)
        # rotation
        keyingsets_utils.RKS_GEN_rotation(self, context, ks, data)


# VisualLocScale
class BUILTIN_KSI_VisualLocScale(KeyingSetInfo):
    """Insert a keyframe on each of the location and scale channels, """ \
        """taking into account effects of constraints and relationships"""
    bl_label = "Visual Location & Scale"

    bl_options = {'INSERTKEY_VISUAL'}

    # poll - use predefined callback for selected bones/objects
    poll = keyingsets_utils.RKS_POLL_selected_bones_or_objects

    # iterator - use callback for selected bones/objects
    iterator = keyingsets_utils.RKS_ITER_selected_item

    # generator
    def generate(self, context, ks, data):
        # location
        keyingsets_utils.RKS_GEN_location(self, context, ks, data)
        # scaling
        keyingsets_utils.RKS_GEN_scaling(self, context, ks, data)


# VisualLocRotScale
class BUILTIN_KSI_VisualLocRotScale(KeyingSetInfo):
    """Insert a keyframe on each of the location, """ \
        """rotation and scale channels, taking into account effects of constraints and relationships"""
    bl_label = "Visual Location, Rotation & Scale"

    bl_options = {'INSERTKEY_VISUAL'}

    # poll - use predefined callback for selected bones/objects
    poll = keyingsets_utils.RKS_POLL_selected_bones_or_objects

    # iterator - use callback for selected bones/objects
    iterator = keyingsets_utils.RKS_ITER_selected_item

    # generator
    def generate(self, context, ks, data):
        # location
        keyingsets_utils.RKS_GEN_location(self, context, ks, data)
        # rotation
        keyingsets_utils.RKS_GEN_rotation(self, context, ks, data)
        # scaling
        keyingsets_utils.RKS_GEN_scaling(self, context, ks, data)


# VisualRotScale
class BUILTIN_KSI_VisualRotScale(KeyingSetInfo):
    """Insert a keyframe on each of the rotation and scale channels, """ \
        """taking into account effects of constraints and relationships"""
    bl_label = "Visual Rotation & Scale"

    bl_options = {'INSERTKEY_VISUAL'}

    # poll - use predefined callback for selected bones/objects
    poll = keyingsets_utils.RKS_POLL_selected_bones_or_objects

    # iterator - use callback for selected bones/objects
    iterator = keyingsets_utils.RKS_ITER_selected_item

    # generator
    def generate(self, context, ks, data):
        # rotation
        keyingsets_utils.RKS_GEN_rotation(self, context, ks, data)
        # scaling
        keyingsets_utils.RKS_GEN_scaling(self, context, ks, data)

# ------------


# Available
class BUILTIN_KSI_Available(KeyingSetInfo):
    """Insert a keyframe on each of the already existing F-Curves"""
    bl_idname = ANIM_KS_AVAILABLE_ID
    bl_label = "Available"

    def poll(self, context):
        # Skip checking for available channels to prevent hotkeys from
        # getting mixed up in the Insert Keyframe Menu (see #127175).
        return bool(context.selected_objects)

    # iterator - use callback for selected bones/objects
    iterator = keyingsets_utils.RKS_ITER_selected_item

    # generator - use callback for doing this
    generate = keyingsets_utils.RKS_GEN_available

###############################


class WholeCharacterMixin:
    # these prefixes should be avoided, as they are not really bones
    # that animators should be touching (or need to touch)
    badBonePrefixes = (
        'DEF',
        'GEO',
        'MCH',
        'ORG',
        'COR',
        'VIS',
        # ... more can be added here as you need in your own rigs ...
    )

    # poll - pose-mode on active object only
    def poll(self, context):
        return ((context.active_object) and (context.active_object.pose) and
                (context.active_object.mode == 'POSE'))

    # iterator - all bones regardless of selection
    def iterator(self, context, ks):
        for bone in context.active_object.pose.bones:
            if not bone.name.startswith(self.badBonePrefixes):
                self.generate(context, ks, bone)

    # generator - all unlocked bone transforms + custom properties
    def generate(self, context, ks, bone):
        # loc, rot, scale - only include unlocked ones
        if not bone.bone.use_connect:
            self.doLoc(ks, bone)

        if bone.rotation_mode in {'QUATERNION', 'AXIS_ANGLE'}:
            self.doRot4d(ks, bone)
        else:
            self.doRot3d(ks, bone)
        self.doScale(ks, bone)

        # bbone properties?
        self.doBBone(context, ks, bone)

        # custom props?
        self.doCustomProps(ks, bone)
    # ----------------

    # helper to add some bone's property to the Keying Set
    def addProp(self, ks, bone, prop, index=-1, use_groups=True):
        # add the property name to the base path
        id_path = bone.path_from_id()
        id_block = bone.id_data

        if prop.startswith('['):
            # custom properties
            path = id_path + prop
        else:
            # standard transforms/properties
            path = keyingsets_utils.path_add_property(id_path, prop)

        # add Keying Set entry for this...
        if use_groups:
            ks.paths.add(id_block, path, index=index, group_method='NAMED', group_name=bone.name)
        else:
            ks.paths.add(id_block, path, index=index)

    # ----------------

    # location properties
    def doLoc(self, ks, bone):
        if bone.lock_location == (False, False, False):
            self.addProp(ks, bone, "location")
        else:
            for i in range(3):
                if not bone.lock_location[i]:
                    self.addProp(ks, bone, "location", i)

    # rotation properties
    def doRot4d(self, ks, bone):
        # rotation mode affects the property used
        if bone.rotation_mode == 'QUATERNION':
            prop = "rotation_quaternion"
        elif bone.rotation_mode == 'AXIS_ANGLE':
            prop = "rotation_axis_angle"

        # add rotation properties if they will
        if bone.lock_rotations_4d:
            # can check individually
            if (bone.lock_rotation == (False, False, False)) and (bone.lock_rotation_w is False):
                self.addProp(ks, bone, prop)
            else:
                if bone.lock_rotation_w is False:
                    self.addProp(ks, bone, prop, 0)  # w = 0

                for i in range(3):
                    if not bone.lock_rotation[i]:
                        self.addProp(ks, bone, prop, i + 1)  # i + 1, since here x/y/z = 1,2,3, and w=0
        elif True not in bone.lock_rotation:
            # if axis-angle rotations get locked as eulers, then it's too messy to allow anything
            # other than all open unless we keyframe the whole lot
            self.addProp(ks, bone, prop)

    def doRot3d(self, ks, bone):
        if bone.lock_rotation == (False, False, False):
            self.addProp(ks, bone, "rotation_euler")
        else:
            for i in range(3):
                if not bone.lock_rotation[i]:
                    self.addProp(ks, bone, "rotation_euler", i)

    # scale properties
    def doScale(self, ks, bone):
        if bone.lock_scale == (0, 0, 0):
            self.addProp(ks, bone, "scale")
        else:
            for i in range(3):
                if not bone.lock_scale[i]:
                    self.addProp(ks, bone, "scale", i)

    # ----------------

    # bendy bone properties
    def doBBone(self, context, ks, pchan):
        bone = pchan.bone

        # This check is crude, but is the best we can do for now
        # It simply adds all of these if the bbone has segments
        # (and the bone is a control bone). This may lead to some
        # false positives...
        if bone.bbone_segments > 1:
            keyingsets_utils.RKS_GEN_bendy_bones(self, context, ks, pchan)

    # ----------------

    # custom properties
    def doCustomProps(self, ks, bone):

        prop_type_compat = {
            bpy.types.BoolProperty,
            bpy.types.IntProperty,
            bpy.types.FloatProperty,
            bpy.types.EnumProperty,
        }

        # go over all custom properties for bone
        for prop in bone.keys():
            # for now, just add all of 'em
            prop_rna = type(bone).bl_rna.properties.get(prop, None)
            if prop_rna is None:
                prop_path = '["{:s}"]'.format(bpy.utils.escape_identifier(prop))
                try:
                    rna_property = bone.path_resolve(prop_path, False)
                except ValueError:
                    # This happens when a custom property is set to None. In that case it cannot
                    # be converted to an FCurve-compatible value, so we can't keyframe it anyway.
                    continue
                if not rna_property:
                    # Failure to resolve property.
                    continue
                if rna_property.rna_type in prop_type_compat:
                    self.addProp(ks, bone, prop_path)
            elif prop_rna.is_animatable:
                self.addProp(ks, bone, prop)


class BUILTIN_KSI_WholeCharacter(WholeCharacterMixin, KeyingSetInfo):
    """Insert a keyframe for all properties that are likely to get animated in a character rig """ \
        """(useful when blocking out a shot)"""
    bl_idname = ANIM_KS_WHOLE_CHARACTER_ID
    bl_label = "Whole Character"


class BUILTIN_KSI_WholeCharacterSelected(WholeCharacterMixin, KeyingSetInfo):
    """Insert a keyframe for all properties that are likely to get animated in a character rig """ \
        """(only selected bones)"""
    bl_idname = ANIM_KS_WHOLE_CHARACTER_SELECTED_ID
    bl_label = "Whole Character (Selected Bones Only)"

    # iterator - all bones regardless of selection
    def iterator(self, context, ks):
        # Use either the selected bones, or all of them if none are selected.
        bones = context.selected_pose_bones_from_active_object or context.active_object.pose.bones

        for bone in bones:
            if bone.name.startswith(self.badBonePrefixes):
                continue
            self.generate(context, ks, bone)


###############################

# Delta Location


class BUILTIN_KSI_DeltaLocation(KeyingSetInfo):
    """Insert keyframes for additional location offset"""
    bl_label = "Delta Location"

    # poll - selected objects only (and only if active object in object mode)
    poll = keyingsets_utils.RKS_POLL_selected_objects

    # iterator - selected objects only
    iterator = keyingsets_utils.RKS_ITER_selected_objects

    # generator - delta location channels only
    def generate(self, _context, ks, data):
        # get id-block and path info
        id_block, base_path, grouping = keyingsets_utils.get_transform_generators_base_info(data)

        # add the property name to the base path
        path = keyingsets_utils.path_add_property(base_path, "delta_location")

        # add Keying Set entry for this...
        if grouping:
            ks.paths.add(id_block, path, group_method='NAMED', group_name=grouping)
        else:
            ks.paths.add(id_block, path)


# Delta Rotation
class BUILTIN_KSI_DeltaRotation(KeyingSetInfo):
    """Insert keyframes for additional rotation offset"""
    bl_label = "Delta Rotation"

    # poll - selected objects only (and only if active object in object mode)
    poll = keyingsets_utils.RKS_POLL_selected_objects

    # iterator - selected objects only
    iterator = keyingsets_utils.RKS_ITER_selected_objects

    # generator - delta location channels only
    def generate(self, _context, ks, data):
        # get id-block and path info
        id_block, base_path, grouping = keyingsets_utils.get_transform_generators_base_info(data)

        # add the property name to the base path
        #   rotation mode affects the property used
        if data.rotation_mode == 'QUATERNION':
            path = keyingsets_utils.path_add_property(base_path, "delta_rotation_quaternion")
        elif data.rotation_mode == 'AXIS_ANGLE':
            # XXX: for now, this is not available yet
            # path = path_add_property(base_path, "delta_rotation_axis_angle")
            return
        else:
            path = keyingsets_utils.path_add_property(base_path, "delta_rotation_euler")

        # add Keying Set entry for this...
        if grouping:
            ks.paths.add(id_block, path, group_method='NAMED', group_name=grouping)
        else:
            ks.paths.add(id_block, path)


# Delta Scale
class BUILTIN_KSI_DeltaScale(KeyingSetInfo):
    """Insert keyframes for additional scale factor"""
    bl_label = "Delta Scale"

    # poll - selected objects only (and only if active object in object mode)
    poll = keyingsets_utils.RKS_POLL_selected_objects

    # iterator - selected objects only
    iterator = keyingsets_utils.RKS_ITER_selected_objects

    # generator - delta location channels only
    def generate(self, _context, ks, data):
        # get id-block and path info
        id_block, base_path, grouping = keyingsets_utils.get_transform_generators_base_info(data)

        # add the property name to the base path
        path = keyingsets_utils.path_add_property(base_path, "delta_scale")

        # add Keying Set entry for this...
        if grouping:
            ks.paths.add(id_block, path, group_method='NAMED', group_name=grouping)
        else:
            ks.paths.add(id_block, path)

###############################


# Note that this controls order of options in `insert keyframe` menu.
# Better try to keep some logical order here beyond mere alphabetical one, also because of menu entries shortcut.
# See also #51867.
classes = (
    BUILTIN_KSI_Available,
    BUILTIN_KSI_Location,
    BUILTIN_KSI_Rotation,
    BUILTIN_KSI_Scaling,
    BUILTIN_KSI_LocRot,
    BUILTIN_KSI_LocRotScale,
    BUILTIN_KSI_LocRotScaleCProp,
    BUILTIN_KSI_LocScale,
    BUILTIN_KSI_RotScale,
    BUILTIN_KSI_DeltaLocation,
    BUILTIN_KSI_DeltaRotation,
    BUILTIN_KSI_DeltaScale,
    BUILTIN_KSI_VisualLoc,
    BUILTIN_KSI_VisualRot,
    BUILTIN_KSI_VisualScaling,
    BUILTIN_KSI_VisualLocRot,
    BUILTIN_KSI_VisualLocRotScale,
    BUILTIN_KSI_VisualLocScale,
    BUILTIN_KSI_VisualRotScale,
    BUILTIN_KSI_BendyBones,
    BUILTIN_KSI_WholeCharacter,
    BUILTIN_KSI_WholeCharacterSelected,
)


def register():
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)


def unregister():
    from bpy.utils import unregister_class
    for cls in classes:
        unregister_class(cls)


if __name__ == "__main__":
    register()
