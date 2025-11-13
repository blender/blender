# SPDX-FileCopyrightText: 2011-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

from __future__ import annotations


import bpy
from bpy.types import Operator
from bpy.props import (
    IntProperty,
    BoolProperty,
    EnumProperty,
    StringProperty,
)
from bpy.app.translations import (
    pgettext_rpt as rpt_,
    contexts as i18n_contexts,
)


class ANIM_OT_keying_set_export(Operator):
    """Export Keying Set to a Python script"""
    bl_idname = "anim.keying_set_export"
    bl_label = "Export Keying Set..."

    filepath: StringProperty(
        subtype='FILE_PATH',
    )
    filter_folder: BoolProperty(
        name="Filter folders",
        default=True,
        options={'HIDDEN'},
    )
    filter_text: BoolProperty(
        name="Filter text",
        default=True,
        options={'HIDDEN'},
    )
    filter_python: BoolProperty(
        name="Filter Python",
        default=True,
        options={'HIDDEN'},
    )

    def execute(self, context):
        from bpy.utils import escape_identifier

        if not self.filepath:
            raise Exception("Filepath not set")

        f = open(self.filepath, "w", encoding="utf8")
        if not f:
            raise Exception("Could not open file")

        scene = context.scene
        ks = scene.keying_sets.active

        f.write("# Keying Set: {:s}\n".format(ks.bl_idname))

        f.write("import bpy\n\n")
        f.write("scene = bpy.context.scene\n\n")

        # Add KeyingSet and set general settings
        f.write("# Keying Set Level declarations\n")
        f.write("ks = scene.keying_sets.new(idname={!r}, name={!r})\n".format(ks.bl_idname, ks.bl_label))
        f.write("ks.bl_description = {!r}\n".format(ks.bl_description))

        # TODO: this isn't editable, it should be possible to set this flag for `scene.keying_sets.new`.
        # if not ks.is_path_absolute:
        #     f.write("ks.is_path_absolute = False\n")
        f.write("\n")

        f.write("ks.use_insertkey_needed = {!r}\n".format(ks.use_insertkey_needed))
        f.write("ks.use_insertkey_visual = {!r}\n".format(ks.use_insertkey_visual))
        f.write("\n")

        # --------------------------------------------------------
        # generate and write set of lookups for id's used in paths

        # cache for syncing ID-blocks to bpy paths + shorthand's
        id_to_paths_cache = {}

        for ksp in ks.paths:
            if ksp.id is None:
                continue
            if ksp.id in id_to_paths_cache:
                continue

            # - `idtype_list` is used to get the list of ID-data-blocks from
            #   `bpy.data.*` since this info isn't available elsewhere.
            # - `id.bl_rna.name` gives a name suitable for UI,
            #   with a capitalized first letter, but we need
            #   the plural form that's all lower case.
            # - special handling is needed for "nested" ID-blocks
            #   (e.g. node-tree in Material).
            if ksp.id.bl_rna.identifier.startswith("ShaderNodeTree"):
                # Find material or light using this node tree...
                id_bpy_path = "bpy.data.nodes[\"{:s}\"]"
                found = False

                for mat in bpy.data.materials:
                    if mat.node_tree == ksp.id:
                        id_bpy_path = "bpy.data.materials[\"{:s}\"].node_tree".format(escape_identifier(mat.name))
                        found = True
                        break

                if not found:
                    for light in bpy.data.lights:
                        if light.node_tree == ksp.id:
                            id_bpy_path = "bpy.data.lights[\"{:s}\"].node_tree".format(escape_identifier(light.name))
                            found = True
                            break

                if not found:
                    self.report(
                        {'WARNING'},
                        rpt_("Could not find material or light using Shader Node Tree - {:s}").format(str(ksp.id)),
                    )
            elif ksp.id.bl_rna.identifier.startswith("CompositorNodeTree"):
                # Find compositor node-tree using this node tree.
                for scene in bpy.data.scenes:
                    if scene.compositing_node_group == ksp.id:
                        id_bpy_path = "bpy.data.scenes[\"{:s}\"].compositing_node_group".format(
                            escape_identifier(scene.name))
                        break
                else:
                    self.report(
                        {'WARNING'},
                        rpt_("Could not find scene using Compositor Node Tree - {:s}").format(str(ksp.id)),
                    )
            elif ksp.id.bl_rna.name == "Key":
                # "keys" conflicts with a Python keyword, hence the simple solution won't work
                id_bpy_path = "bpy.data.shape_keys[\"{:s}\"]".format(escape_identifier(ksp.id.name))
            else:
                idtype_list = ksp.id.bl_rna.name.lower() + "s"
                id_bpy_path = "bpy.data.{:s}[\"{:s}\"]".format(idtype_list, escape_identifier(ksp.id.name))

            # shorthand ID for the ID-block (as used in the script)
            short_id = "id_{:d}".format(len(id_to_paths_cache))

            # store this in the cache now
            id_to_paths_cache[ksp.id] = [short_id, id_bpy_path]

        f.write("# ID's that are commonly used\n")
        for id_pair in id_to_paths_cache.values():
            f.write("{:s} = {:s}\n".format(id_pair[0], id_pair[1]))
        f.write("\n")

        # write paths
        f.write("# Path Definitions\n")
        for ksp in ks.paths:
            f.write("ksp = ks.paths.add(")

            # id-block + data_path
            if ksp.id:
                # find the relevant shorthand from the cache
                id_bpy_path = id_to_paths_cache[ksp.id][0]
            else:
                id_bpy_path = "None"  # XXX...
            f.write("{:s}, {!r}".format(id_bpy_path, ksp.data_path))

            # array index settings (if applicable)
            if ksp.use_entire_array:
                f.write(", index=-1")
            else:
                f.write(", index={:d}".format(ksp.array_index))

            # grouping settings (if applicable)
            # NOTE: the current default is KEYINGSET, but if this changes,
            # change this code too
            if ksp.group_method == 'NAMED':
                f.write(", group_method={!r}, group_name={!r}".format(ksp.group_method, ksp.group))
            elif ksp.group_method != 'KEYINGSET':
                f.write(", group_method={!r}".format(ksp.group_method))

            # finish off
            f.write(")\n")

        f.write("\n")
        f.close()

        return {'FINISHED'}

    def invoke(self, context, _event):
        wm = context.window_manager
        wm.fileselect_add(self)
        return {'RUNNING_MODAL'}


class NLA_OT_bake(Operator):
    """Bake all selected objects location/scale/rotation animation to an action"""
    bl_idname = "nla.bake"
    bl_label = "Bake Action"
    bl_options = {'REGISTER', 'UNDO'}

    frame_start: IntProperty(
        name="Start Frame",
        description="Start frame for baking",
        min=0, max=300000,
        default=1,
    )
    frame_end: IntProperty(
        name="End Frame",
        description="End frame for baking",
        min=1, max=300000,
        default=250,
    )
    step: IntProperty(
        name="Frame Step",
        description="Number of frames to skip forward while baking each frame",
        min=1, max=120,
        default=1,
    )
    only_selected: BoolProperty(
        name="Only Selected Bones",
        description="Only key selected bones (Pose baking only)",
        default=True,
    )
    visual_keying: BoolProperty(
        name="Visual Keying",
        description="Keyframe from the final transformations (with constraints applied)",
        default=False,
    )
    clear_constraints: BoolProperty(
        name="Clear Constraints",
        description=(
            "Remove all constraints from keyed object/bones. "
            "To get a correct bake with this setting Visual Keying should be enabled"
        ),
        default=False,
    )
    clear_parents: BoolProperty(
        name="Clear Parents",
        description="Bake animation onto the object then clear parents (objects only)",
        default=False,
    )
    use_current_action: BoolProperty(
        name="Overwrite Current Action",
        description="Bake animation into current action, instead of creating a new one "
        "(useful for baking only part of bones in an armature)",
        default=False,
    )
    clean_curves: BoolProperty(
        name="Clean Curves",
        description="After baking curves, remove redundant keys",
        default=False,
    )
    bake_types: EnumProperty(
        name="Bake Data",
        translation_context=i18n_contexts.id_action,
        description="Which data's transformations to bake",
        options={'ENUM_FLAG'},
        items=(
            ('POSE', "Pose", "Bake bones transformations"),
            ('OBJECT', "Object", "Bake object transformations"),
        ),
        default={'POSE'},
    )
    channel_types: EnumProperty(
        name="Channels",
        description="Which channels to bake",
        options={'ENUM_FLAG'},
        items=(
            ('LOCATION', "Location", "Bake location channels"),
            ('ROTATION', "Rotation", "Bake rotation channels"),
            ('SCALE', "Scale", "Bake scale channels"),
            ('BBONE', "B-Bone", "Bake B-Bone channels"),
            ('PROPS', "Custom Properties", "Bake custom properties"),
        ),
        default={'LOCATION', 'ROTATION', 'SCALE', 'BBONE', 'PROPS'},
    )

    def execute(self, context):
        from bpy_extras import anim_utils

        bake_options = anim_utils.BakeOptions(
            only_selected=self.only_selected,
            do_pose='POSE' in self.bake_types,
            do_object='OBJECT' in self.bake_types,
            do_visual_keying=self.visual_keying,
            do_constraint_clear=self.clear_constraints,
            do_parents_clear=self.clear_parents,
            do_clean=self.clean_curves,
            do_location='LOCATION' in self.channel_types,
            do_rotation='ROTATION' in self.channel_types,
            do_scale='SCALE' in self.channel_types,
            do_bbone='BBONE' in self.channel_types,
            do_custom_props='PROPS' in self.channel_types,
        )

        if bake_options.do_pose and self.only_selected:
            pose_bones = context.selected_pose_bones or []
            armatures = {pose_bone.id_data for pose_bone in pose_bones}
            objects = list(armatures)
        else:
            objects = context.selected_editable_objects
            if bake_options.do_pose and not bake_options.do_object:
                pose_object = getattr(context, "pose_object", None)
                if pose_object and pose_object not in objects:
                    # The active object might not be selected, but it is the one in pose mode.
                    # It can be assumed this pose needs baking.
                    objects.append(pose_object)
                objects = [obj for obj in objects if obj.pose is not None]

        object_action_pairs = (
            [(obj, getattr(obj.animation_data, "action", None)) for obj in objects]
            if self.use_current_action else
            [(obj, None) for obj in objects]
        )

        actions = anim_utils.bake_action_objects(
            object_action_pairs,
            frames=range(self.frame_start, self.frame_end + 1, self.step),
            bake_options=bake_options,
        )

        if not any(actions):
            self.report({'INFO'}, "Nothing to bake")
            return {'CANCELLED'}

        return {'FINISHED'}

    def invoke(self, context, _event):
        scene = context.scene
        if scene.use_preview_range:
            self.frame_start = scene.frame_preview_start
            self.frame_end = scene.frame_preview_end
        else:
            self.frame_start = scene.frame_start
            self.frame_end = scene.frame_end
        self.bake_types = {'POSE'} if context.mode == 'POSE' else {'OBJECT'}

        wm = context.window_manager
        return wm.invoke_props_dialog(self)


class ClearUselessActions(Operator):
    """Mark actions with no F-Curves for deletion after save and reload of """ \
        """file preserving \"action libraries\""""
    bl_idname = "anim.clear_useless_actions"
    bl_label = "Clear Useless Actions"
    bl_options = {'REGISTER', 'UNDO'}

    only_unused: BoolProperty(
        name="Only Unused",
        description="Only unused (Fake User only) actions get considered",
        default=True,
    )

    @classmethod
    def poll(cls, _context):
        return bool(bpy.data.actions)

    @staticmethod
    def has_fcurves(action: bpy.types.Action) -> bool:
        for layer in action.layers:
            for strip in layer.strips:
                assert strip.type == 'KEYFRAME'
                for channelbag in strip.channelbags:
                    if channelbag.fcurves:
                        return True
        return False

    def execute(self, _context):
        removed = 0

        for action in bpy.data.actions:
            # if only user is "fake" user...
            if (
                (self.only_unused is False) or
                (action.use_fake_user and action.users == 1)
            ):

                # if it has F-Curves, then it's a "action library"
                # (i.e. walk, wave, jump, etc.)
                # and should be left alone as that's what fake users are for!
                if not self.has_fcurves(action):
                    # mark action for deletion
                    action.user_clear()
                    removed += 1

        self.report({'INFO'}, rpt_("Removed {:d} empty and/or fake-user only Actions").format(removed))
        return {'FINISHED'}


class UpdateAnimatedTransformConstraint(Operator):
    """Update f-curves/drivers affecting Transform constraints (use it with files from 2.70 and earlier)"""
    bl_idname = "anim.update_animated_transform_constraints"
    bl_label = "Update Animated Transform Constraints"
    bl_options = {'REGISTER', 'UNDO'}

    use_convert_to_radians: BoolProperty(
        name="Convert to Radians",
        description=(
            "Convert f-curves/drivers affecting rotations to radians.\n"
            "Warning: Use this only once"
        ),
        default=True,
    )

    def execute(self, context):
        import _animsys_refactor as animsys_refactor
        from math import radians
        import io

        from_paths = {"from_max_x", "from_max_y", "from_max_z", "from_min_x", "from_min_y", "from_min_z"}
        to_paths = {"to_max_x", "to_max_y", "to_max_z", "to_min_x", "to_min_y", "to_min_z"}
        paths = from_paths | to_paths

        def update_cb(base, _class_name, old_path, fcurve, options):
            # print(options)

            def handle_deg2rad(fcurve):
                if fcurve is not None:
                    if hasattr(fcurve, "keyframes"):
                        for k in fcurve.keyframes:
                            k.co.y = radians(k.co.y)
                    for mod in fcurve.modifiers:
                        if mod.type == 'GENERATOR':
                            if mod.mode == 'POLYNOMIAL':
                                mod.coefficients[:] = [radians(c) for c in mod.coefficients]
                            else:  # if mod.type == 'POLYNOMIAL_FACTORISED':
                                mod.coefficients[:2] = [radians(c) for c in mod.coefficients[:2]]
                        elif mod.type == 'FNGENERATOR':
                            mod.amplitude = radians(mod.amplitude)
                    fcurve.update()

            data = ...
            try:
                data = eval("base." + old_path)
            except Exception:
                pass
            ret = (data, old_path)
            if isinstance(base, bpy.types.TransformConstraint) and data is not ...:
                new_path = None
                map_info = base.map_from if old_path in from_paths else base.map_to
                if map_info == 'ROTATION':
                    new_path = old_path + "_rot"
                    if options is not None and options["use_convert_to_radians"]:
                        handle_deg2rad(fcurve)
                elif map_info == 'SCALE':
                    new_path = old_path + "_scale"

                if new_path is not None:
                    data = ...
                    try:
                        data = eval("base." + new_path)
                    except Exception:
                        pass
                    ret = (data, new_path)
                    # print(ret)

            return ret

        options = {"use_convert_to_radians": self.use_convert_to_radians}
        replace_ls = [("TransformConstraint", p, update_cb, options) for p in paths]
        log = io.StringIO()

        animsys_refactor.update_data_paths(replace_ls, log)

        context.scene.frame_set(context.scene.frame_current)

        log = log.getvalue()
        if log:
            print(log)
            text = bpy.data.texts.new("UpdateAnimatedTransformConstraint Report")
            text.from_string(log)
            self.report({'INFO'}, rpt_("Complete report available on '{:s}' text data-block").format(text.name))
        return {'FINISHED'}


class ARMATURE_OT_copy_bone_color_to_selected(Operator):
    """Copy the bone color of the active bone to all selected bones"""
    bl_idname = "armature.copy_bone_color_to_selected"
    bl_label = "Copy Colors to Selected"
    bl_options = {'REGISTER', 'UNDO'}

    _bone_type_enum = [
        ('EDIT', "Bone", "Copy Bone colors from the active bone to all selected bones"),
        ('POSE', "Pose Bone", "Copy Pose Bone colors from the active pose bone to all selected pose bones"),
    ]

    bone_type: EnumProperty(
        name="Type",
        items=_bone_type_enum,
    )

    @classmethod
    def poll(cls, context):
        return context.mode in {'EDIT_ARMATURE', 'POSE'}

    def execute(self, context):
        match(self.bone_type, context.mode):
            # Armature in edit mode:
            case('POSE', 'EDIT_ARMATURE'):
                self.report({'ERROR'}, "Go to pose mode to copy pose bone colors")
                return {'OPERATOR_CANCELLED'}
            case('EDIT', 'EDIT_ARMATURE'):
                bone_source = context.active_bone
                bones_dest = context.selected_bones
                pose_bones_to_check = []

            # Armature in pose mode:
            case('POSE', 'POSE'):
                bone_source = context.active_pose_bone
                bones_dest = context.selected_pose_bones
                pose_bones_to_check = []
            case('EDIT', 'POSE'):
                bone_source = context.active_bone
                pose_bones_to_check = context.selected_pose_bones
                bones_dest = [posebone.bone for posebone in pose_bones_to_check]

            # Anything else:
            case _:
                self.report({'ERROR'}, rpt_("Cannot do anything in mode {!r}").format(context.mode))
                return {'CANCELLED'}

        if not bone_source:
            self.report({'ERROR'}, "No active bone to copy from")
            return {'CANCELLED'}

        if not bones_dest:
            self.report({'ERROR'}, "No selected bones to copy to")
            return {'CANCELLED'}

        num_pose_color_overrides = 0
        for index, bone_dest in enumerate(bones_dest):
            bone_dest.color.palette = bone_source.color.palette
            for custom_field in ("normal", "select", "active"):
                color = getattr(bone_source.color.custom, custom_field)
                setattr(bone_dest.color.custom, custom_field, color)

            if self.bone_type == 'EDIT' and pose_bones_to_check:
                pose_bone = pose_bones_to_check[index]
                if pose_bone.color.palette != 'DEFAULT':
                    # A pose color has been set, and we're now syncing edit bone
                    # colors. This means that the synced color will not be
                    # visible. Better to let the user know about this.
                    num_pose_color_overrides += 1

        if num_pose_color_overrides:
            self.report(
                {'INFO'},
                rpt_("Bone colors were synced; "
                     "for {:d} bones this will not be visible due to pose bone color overrides").format(
                    num_pose_color_overrides,
                ),
            )

        return {'FINISHED'}


def _armature_from_context(context):
    pin_armature = getattr(context, "armature", None)
    if pin_armature:
        return pin_armature
    ob = context.object
    if ob and ob.type == 'ARMATURE':
        return ob.data
    return None


class ARMATURE_OT_collection_show_all(Operator):
    """Show all bone collections"""
    bl_idname = "armature.collection_show_all"
    bl_label = "Show All"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        return _armature_from_context(context) is not None

    def execute(self, context):
        arm = _armature_from_context(context)
        for bcoll in arm.collections_all:
            bcoll.is_visible = True
        return {'FINISHED'}


class ARMATURE_OT_collection_unsolo_all(Operator):
    """Clear the 'solo' setting on all bone collections"""
    bl_idname = "armature.collection_unsolo_all"
    bl_label = "Un-solo All"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        armature = _armature_from_context(context)
        if not armature:
            return False
        if not armature.collections.is_solo_active:
            cls.poll_message_set("None of the bone collections is marked 'solo'")
            return False
        return True

    def execute(self, context):
        arm = _armature_from_context(context)
        for bcoll in arm.collections_all:
            bcoll.is_solo = False
        return {'FINISHED'}


class ARMATURE_OT_collection_remove_unused(Operator):
    """Remove all bone collections that have neither bones nor children. """ \
        """This is done recursively, so bone collections that only have unused children are also removed"""

    bl_idname = "armature.collection_remove_unused"
    bl_label = "Remove Unused Bone Collections"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        armature = _armature_from_context(context)
        if not armature:
            return False
        return len(armature.collections) > 0

    def execute(self, context):
        if context.mode == 'EDIT_ARMATURE':
            return self.execute_edit_mode(context)

        armature = _armature_from_context(context)

        # Build a set of bone collections that don't contain any bones, and
        # whose children also don't contain any bones.
        bcolls_to_remove = {
            bcoll
            for bcoll in armature.collections_all
            if len(bcoll.bones_recursive) == 0}

        if not bcolls_to_remove:
            self.report({'INFO'}, "All bone collections are in use")
            return {'CANCELLED'}

        self.remove_bcolls(armature, bcolls_to_remove)
        return {'FINISHED'}

    def execute_edit_mode(self, context):
        # BoneCollection.bones_recursive or .bones are not available in armature
        # edit mode, because that has a completely separate list of edit bones.
        # This is why edit mode needs separate handling.

        armature = _armature_from_context(context)
        bcolls_with_bones = {
            bcoll
            for ebone in armature.edit_bones
            for bcoll in ebone.collections
        }

        bcolls_to_remove = []
        for root in armature.collections:
            self.visit(root, bcolls_with_bones, bcolls_to_remove)

        if not bcolls_to_remove:
            self.report({'INFO'}, "All bone collections are in use")
            return {'CANCELLED'}

        self.remove_bcolls(armature, bcolls_to_remove)
        return {'FINISHED'}

    def visit(self, bcoll, bcolls_with_bones, bcolls_to_remove):
        has_bones = bcoll in bcolls_with_bones

        for child in bcoll.children:
            child_has_bones = self.visit(child, bcolls_with_bones, bcolls_to_remove)
            has_bones = has_bones or child_has_bones

        if not has_bones:
            bcolls_to_remove.append(bcoll)

        return has_bones

    def remove_bcolls(self, armature, bcolls_to_remove):
        # Count things before they get removed.
        num_bcolls_before_removal = len(armature.collections_all)
        num_bcolls_to_remove = len(bcolls_to_remove)

        # Create a copy of bcolls_to_remove so that it doesn't change when we
        # remove bone collections.
        for bcoll in reversed(list(bcolls_to_remove)):
            armature.collections.remove(bcoll)

        self.report(
            {'INFO'},
            rpt_("Removed {:d} of {:d} bone collections").format(
                num_bcolls_to_remove,
                num_bcolls_before_removal),
        )


class ANIM_OT_slot_new_for_id(Operator):
    """Create a new Action Slot for an ID.

    Note that _which_ ID should get this slot must be set in the 'animated_id' context pointer, using:

    >>> layout.context_pointer_set("animated_id", animated_id)

    When the ID already has a slot assigned, the newly-created slot will be
    named after it (ensuring uniqueness with a numerical suffix) and any
    animation data of the assigned slot will be duplicated for the new slot.
    """
    bl_idname = "anim.slot_new_for_id"
    bl_label = "New Slot"
    bl_description = "Create a new action slot for this data-block, to hold its animation"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        animated_id = getattr(context, "animated_id", None)
        if not animated_id:
            return False
        if not animated_id.animation_data or not animated_id.animation_data.action:
            cls.poll_message_set("An action slot can only be created when an action is assigned")
            return False
        if not animated_id.animation_data.action.is_action_layered:
            cls.poll_message_set("Action slots are only supported by layered Actions. Upgrade this Action first")
            return False
        if not animated_id.animation_data.action.is_editable:
            cls.poll_message_set("Creating a new Slot is not possible on a linked Action")
            return False
        return True

    def execute(self, context):
        animated_id = context.animated_id
        adt = animated_id.animation_data

        if adt.action_slot:
            slot = adt.action_slot.duplicate()
        else:
            slot_name = adt.last_slot_identifier[2:] or animated_id.name
            slot = adt.action.slots.new(animated_id.id_type, slot_name)

        adt.action_slot = slot
        return {'FINISHED'}


class ANIM_OT_slot_unassign_from_id(Operator):
    """Un-assign the assigned Action Slot from an ID.

    Note that _which_ ID should get this slot unassigned must be set in the
    "animated_id" context pointer, using:

    >>> layout.context_pointer_set("animated_id", animated_id)
    """
    bl_idname = "anim.slot_unassign_from_id"
    bl_label = "Unassign Slot"
    bl_description = "Un-assign the action slot, effectively making this data-block non-animated"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        animated_id = getattr(context, "animated_id", None)
        if not animated_id:
            return False
        if not animated_id.animation_data or not animated_id.animation_data.action_slot:
            cls.poll_message_set("This data-block has no Action slot assigned")
            return False
        return True

    def execute(self, context):
        animated_id = context.animated_id
        animated_id.animation_data.action_slot = None
        return {'FINISHED'}


class generic_slot_unassign_mixin:
    context_property_name = ""
    """Which context attribute to use to get the to-be-manipulated data-block."""

    @classmethod
    def poll(cls, context):
        slot_user = getattr(context, cls.context_property_name, None)
        if not slot_user:
            return False

        if not slot_user.action_slot:
            cls.poll_message_set("No Action slot is assigned, so there is nothing to un-assign")
            return False
        return True

    def execute(self, context):
        slot_user = getattr(context, self.context_property_name, None)
        slot_user.action_slot = None
        return {'FINISHED'}


class ANIM_OT_slot_unassign_from_nla_strip(generic_slot_unassign_mixin, Operator):
    """Un-assign the assigned Action Slot from an NLA strip.

    Note that _which_ NLA strip should get this slot unassigned must be set in
    the "nla_strip" context pointer, using:

    >>> layout.context_pointer_set("nla_strip", nla_strip)
    """
    bl_idname = "anim.slot_unassign_from_nla_strip"
    bl_label = "Unassign Slot"
    bl_description = "Un-assign the action slot from this NLA strip, effectively making it non-animated"
    bl_options = {'REGISTER', 'UNDO'}

    context_property_name = "nla_strip"


class ANIM_OT_slot_unassign_from_constraint(generic_slot_unassign_mixin, Operator):
    """Un-assign the assigned Action Slot from an Action constraint.

    Note that _which_ constraint should get this slot unassigned must be set in
    the "constraint" context pointer, using:

    >>> layout.context_pointer_set("constraint", constraint)
    """
    bl_idname = "anim.slot_unassign_from_constraint"
    bl_label = "Unassign Slot"
    bl_description = "Un-assign the action slot from this constraint"
    bl_options = {'REGISTER', 'UNDO'}

    context_property_name = "constraint"


# This is for the versioning from 4.5 to 5.0 and can be removed in 6.0.
class ANIM_OT_version_bone_hide_property(Operator):
    bl_idname = "anim.version_bone_hide_property"
    bl_label = "Version Bone Hide Property"
    bl_description = "Moves any F-Curves for the `hide` property of selected armatures " \
        "into the action of the object. This will only operate on the first layer " \
        "and strip of the action"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):

        if len(context.selected_objects) == 0:
            cls.poll_message_set("No objects selected")
            return False
        return True

    @staticmethod
    def find_property_fcurves(channelbag):
        fcurves = []
        for fcurve in channelbag.fcurves:
            if fcurve.data_path.startswith("bones[") and fcurve.data_path.endswith("].hide"):
                fcurves.append(fcurve)
        return fcurves

    def execute(self, context):
        from bpy_extras import anim_utils
        selected_armatures = []
        for arm_ob in context.selected_objects:
            if arm_ob.type != 'ARMATURE' or not arm_ob.data:
                continue
            armature = arm_ob.data
            assigned_channelbag = anim_utils.animdata_get_channelbag_for_assigned_slot(armature.animation_data)
            if not assigned_channelbag:
                # Armature not animated. Cannot have the FCurve we need.
                continue
            selected_armatures.append(arm_ob)

        if not selected_armatures:
            self.report({'WARNING'}, rpt_("No animated armatures selected"))
            return {'CANCELLED'}

        warn = True
        modified_armatures = []
        # The objects also have to be animated -> have an assigned action + slot.
        # This means we know with certainty which action to move the data into.
        for arm_ob in selected_armatures:
            ob_adt = arm_ob.animation_data
            arm_adt = arm_ob.data.animation_data
            if warn and (not ob_adt or not ob_adt.action or not ob_adt.action_slot):
                self.report({'WARNING'}, rpt_("Not all armature objects have an action and slot assigned"))
                # Only warn once.
                warn = False
                continue

            # Only armatures with an action and slot are added to `selected_armatures`.
            assert arm_adt is not None
            armature_channelbag = anim_utils.action_get_channelbag_for_slot(arm_adt.action, arm_adt.action_slot)
            if not armature_channelbag:
                continue

            fcurves = self.find_property_fcurves(armature_channelbag)

            if not fcurves:
                # No FCurves for the hide property found.
                continue

            # An action + slot is assigned, but that doesn't mean there is a layer and a strip.
            ob_channelbag = anim_utils.action_ensure_channelbag_for_slot(ob_adt.action, ob_adt.action_slot)

            for fcurve in fcurves:
                new_path = "pose." + fcurve.data_path
                if ob_channelbag.fcurves.find(new_path):
                    # FCurve for that property already exists.
                    continue

                ob_channelbag.fcurves.new_from_fcurve(fcurve, data_path=new_path)

            modified_armatures.append(arm_ob)

        if not modified_armatures:
            self.report({'WARNING'}, rpt_("No armature animation was modified"))
            return {'CANCELLED'}

        self.report({'INFO'}, rpt_(f"Modified the animation of {len(modified_armatures)} armatures"))
        for screen in bpy.data.screens:
            for area in screen.areas:
                area.tag_redraw()

        return {'FINISHED'}


classes = (
    ANIM_OT_keying_set_export,
    NLA_OT_bake,
    ClearUselessActions,
    UpdateAnimatedTransformConstraint,
    ARMATURE_OT_copy_bone_color_to_selected,
    ARMATURE_OT_collection_show_all,
    ARMATURE_OT_collection_unsolo_all,
    ARMATURE_OT_collection_remove_unused,
    ANIM_OT_slot_new_for_id,
    ANIM_OT_slot_unassign_from_id,
    ANIM_OT_slot_unassign_from_nla_strip,
    ANIM_OT_slot_unassign_from_constraint,
    ANIM_OT_version_bone_hide_property,
)
