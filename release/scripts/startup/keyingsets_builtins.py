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
Built-In Keying Sets
None of these Keying Sets should be removed, as these
are needed by various parts of Blender in order for them
to work correctly.

Beware also about changing the order that these are defined
here, since this can result in old files referring to the
wrong Keying Set as the active one, potentially resulting
in lost (i.e. unkeyed) animation.
"""

import bpy
import keyingsets_utils
from bpy.types import KeyingSetInfo

###############################
# Built-In KeyingSets


# "Defines"
# Keep these in sync with those in ED_keyframing.h!
ANIM_KS_LOCATION_ID = "Location"
ANIM_KS_ROTATION_ID = "Rotation"
ANIM_KS_SCALING_ID = "Scaling"
ANIM_KS_LOC_ROT_SCALE_ID = "LocRotScale"
ANIM_KS_AVAILABLE_ID = "Available"
ANIM_KS_WHOLE_CHARACTER_ID = "Whole Character"


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
    bl_label = "Scaling"

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
    bl_label = "LocRot"

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
    bl_label = "LocScale"

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
    """
    Insert a keyframe on each of the location, rotation, and scale channels
    """
    bl_idname = ANIM_KS_LOC_ROT_SCALE_ID
    bl_label = "LocRotScale"

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


# RotScale
class BUILTIN_KSI_RotScale(KeyingSetInfo):
    """Insert a keyframe on each of the rotation and scale channels"""
    bl_label = "RotScale"

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


# Location
class BUILTIN_KSI_VisualLoc(KeyingSetInfo):
    """
    Insert a keyframe on each of the location channels, taking into account
    effects of constraints and relationships
    """
    bl_label = "Visual Location"

    bl_options = {'INSERTKEY_VISUAL'}

    # poll - use predefined callback for selected bones/objects
    poll = keyingsets_utils.RKS_POLL_selected_items

    # iterator - use callback for selected bones/objects
    iterator = keyingsets_utils.RKS_ITER_selected_item

    # generator - use callback for location
    generate = keyingsets_utils.RKS_GEN_location


# Rotation
class BUILTIN_KSI_VisualRot(KeyingSetInfo):
    """
    Insert a keyframe on each of the rotation channels, taking into account
    effects of constraints and relationships
    """
    bl_label = "Visual Rotation"

    bl_options = {'INSERTKEY_VISUAL'}

    # poll - use predefined callback for selected bones/objects
    poll = keyingsets_utils.RKS_POLL_selected_items

    # iterator - use callback for selected bones/objects
    iterator = keyingsets_utils.RKS_ITER_selected_item

    # generator - use callback for rotation
    generate = keyingsets_utils.RKS_GEN_rotation


# VisualLocRot
class BUILTIN_KSI_VisualLocRot(KeyingSetInfo):
    """
    Insert a keyframe on each of the location and rotation channels,
    taking into account effects of constraints and relationships
    """
    bl_label = "Visual LocRot"

    bl_options = {'INSERTKEY_VISUAL'}

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

# ------------


# Available
class BUILTIN_KSI_Available(KeyingSetInfo):
    """Insert a keyframe on each of the already existing F-Curves"""
    bl_idname = ANIM_KS_AVAILABLE_ID
    bl_label = "Available"

    # poll - selected objects or selected object with animation data
    def poll(ksi, context):
        ob = context.active_object
        if ob:
            # TODO: this fails if one animation-less object is active, but many others are selected
            return ob.animation_data and ob.animation_data.action
        else:
            return bool(context.selected_objects)

    # iterator - use callback for selected bones/objects
    iterator = keyingsets_utils.RKS_ITER_selected_item

    # generator - use callback for doing this
    generate = keyingsets_utils.RKS_GEN_available

###############################


# All properties that are likely to get animated in a character rig
class BUILTIN_KSI_WholeCharacter(KeyingSetInfo):
    """
    Insert a keyframe for all properties that are likely to get animated in a
    character rig (useful when blocking out a shot)
    """
    bl_idname = ANIM_KS_WHOLE_CHARACTER_ID
    bl_label = "Whole Character"

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
    def poll(ksi, context):
        return ((context.active_object) and (context.active_object.pose) and
                (context.active_object.mode == 'POSE'))

    # iterator - all bones regardless of selection
    def iterator(ksi, context, ks):
        for bone in context.active_object.pose.bones:
            if not bone.name.startswith(BUILTIN_KSI_WholeCharacter.badBonePrefixes):
                ksi.generate(context, ks, bone)

    # generator - all unlocked bone transforms + custom properties
    def generate(ksi, context, ks, bone):
        # loc, rot, scale - only include unlocked ones
        ksi.doLoc(ks, bone)

        if bone.rotation_mode in {'QUATERNION', 'AXIS_ANGLE'}:
            ksi.doRot4d(ks, bone)
        else:
            ksi.doRot3d(ks, bone)
        ksi.doScale(ks, bone)

        # custom props?
        ksi.doCustomProps(ks, bone)

    # ----------------

    # helper to add some bone's property to the Keying Set
    def addProp(ksi, ks, bone, prop, index=-1, use_groups=True):
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
            ks.paths.add(id_block, path, index, group_method='NAMED', group_name=bone.name)
        else:
            ks.paths.add(id_block, path, index)

    # ----------------

    # location properties
    def doLoc(ksi, ks, bone):
        if bone.lock_location == (False, False, False):
            ksi.addProp(ks, bone, "location")
        else:
            for i in range(3):
                if not bone.lock_location[i]:
                    ksi.addProp(ks, bone, "location", i)

    # rotation properties
    def doRot4d(ksi, ks, bone):
        # rotation mode affects the property used
        if bone.rotation_mode == 'QUATERNION':
            prop = "rotation_quaternion"
        elif bone.rotation_mode == 'AXIS_ANGLE':
            prop = "rotation_axis_angle"

        # add rotation properties if they will
        if bone.lock_rotations_4d:
            # can check individually
            if (bone.lock_rotation == (False, False, False)) and (bone.lock_rotation_w == False):
                ksi.addProp(ks, bone, prop)
            else:
                if bone.lock_rotation_w == False:
                    ksi.addProp(ks, bone, prop, 0)  # w = 0

                for i in range(3):
                    if not bone.lock_rotation[i]:
                        ksi.addProp(ks, bone, prop, i + 1)  # i + 1, since here x/y/z = 1,2,3, and w=0
        elif True not in bone.lock_rotation:
            # if axis-angle rotations get locked as eulers, then it's too messy to allow anything
            # other than all open unless we keyframe the whole lot
            ksi.addProp(ks, bone, prop)

    def doRot3d(ksi, ks, bone):
        if bone.lock_rotation == (False, False, False):
            ksi.addProp(ks, bone, "rotation_euler")
        else:
            for i in range(3):
                if not bone.lock_rotation[i]:
                    ksi.addProp(ks, bone, "rotation_euler", i)

    # scale properties
    def doScale(ksi, ks, bone):
        if bone.lock_scale == (0, 0, 0):
            ksi.addProp(ks, bone, "scale")
        else:
            for i in range(3):
                if not bone.lock_scale[i]:
                    ksi.addProp(ks, bone, "scale", i)

    # ----------------

    # custom properties
    def doCustomProps(ksi, ks, bone):

        prop_type_compat = {bpy.types.BoolProperty,
                            bpy.types.IntProperty,
                            bpy.types.FloatProperty}

        # go over all custom properties for bone
        for prop in bone.keys():
            # ignore special "_RNA_UI" used for UI editing
            if prop == "_RNA_UI":
                continue

            # for now, just add all of 'em
            prop_rna = type(bone).bl_rna.properties.get(prop, None)
            if prop_rna is None:
                prop_path = '["%s"]' % prop
                if bone.path_resolve(prop_path, False).rna_type in prop_type_compat:
                    ksi.addProp(ks, bone, prop_path)
            elif prop_rna.is_animatable:
                ksi.addProp(ks, bone, prop)


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
    def generate(ksi, context, ks, data):
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
    def generate(ksi, context, ks, data):
        # get id-block and path info
        id_block, base_path, grouping = keyingsets_utils.get_transform_generators_base_info(data)

        # add the property name to the base path
        #   rotation mode affects the property used
        if data.rotation_mode == 'QUATERNION':
            path = keyingsets_utils.path_add_property(base_path, "delta_rotation_quaternion")
        elif data.rotation_mode == 'AXIS_ANGLE':
            # XXX: for now, this is not available yet
            #path = path_add_property(base_path, "delta_rotation_axis_angle")
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
    """Insert keyframes for additional scaling factor"""
    bl_label = "Delta Scale"

    # poll - selected objects only (and only if active object in object mode)
    poll = keyingsets_utils.RKS_POLL_selected_objects

    # iterator - selected objects only
    iterator = keyingsets_utils.RKS_ITER_selected_objects

    # generator - delta location channels only
    def generate(ksi, context, ks, data):
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


def register():
    bpy.utils.register_module(__name__)


def unregister():
    bpy.utils.unregister_module(__name__)


if __name__ == "__main__":
    register()
