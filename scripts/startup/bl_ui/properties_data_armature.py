# SPDX-FileCopyrightText: 2009-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
from bpy.types import Panel, Menu, UIList
from rna_prop_ui import PropertyPanel

from bl_ui.properties_animviz import (
    MotionPathButtonsPanel,
    MotionPathButtonsPanel_display,
)


class ArmatureButtonsPanel:
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "data"

    @classmethod
    def poll(cls, context):
        return context.armature


class DATA_PT_context_arm(ArmatureButtonsPanel, Panel):
    bl_label = ""
    bl_options = {'HIDE_HEADER'}

    def draw(self, context):
        layout = self.layout

        ob = context.object
        arm = context.armature
        space = context.space_data

        if ob:
            layout.template_ID(ob, "data")
        elif arm:
            layout.template_ID(space, "pin_id")


class DATA_PT_skeleton(ArmatureButtonsPanel, Panel):
    bl_label = "Skeleton"

    def draw(self, context):
        layout = self.layout

        arm = context.armature

        layout.row().prop(arm, "pose_position", expand=True)


class DATA_PT_display(ArmatureButtonsPanel, Panel):
    bl_label = "Viewport Display"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        ob = context.object
        arm = context.armature

        layout.prop(arm, "display_type", text="Display As")

        col = layout.column(heading="Show")
        col.prop(arm, "show_names", text="Names")
        col.prop(arm, "show_bone_custom_shapes", text="Shapes")
        col.prop(arm, "show_group_colors", text="Group Colors")

        if ob:
            col.prop(ob, "show_in_front", text="In Front")

        col = layout.column(align=False, heading="Axes")
        row = col.row(align=True)
        row.prop(arm, "show_axes", text="")
        sub = row.row(align=True)
        sub.active = arm.show_axes
        sub.prop(arm, "axes_position", text="Position")

        sub = col.row(align=True)
        sub.prop(arm, "relation_line_position", text="Relations", expand=True)


class DATA_UL_bone_collections(UIList):
    def draw_item(self, _context, layout, armature, bcoll, _icon, _active_data, _active_propname, _index):
        active_bone = armature.edit_bones.active or armature.bones.active
        has_active_bone = active_bone and bcoll.name in active_bone.collections

        layout.prop(bcoll, "name", text="", emboss=False,
                    icon='DOT' if has_active_bone else 'BLANK1')
        layout.prop(bcoll, "is_visible", text="", emboss=False,
                    icon='HIDE_OFF' if bcoll.is_visible else 'HIDE_ON')


class DATA_PT_bone_collections(ArmatureButtonsPanel, Panel):
    bl_label = "Bone Collections"

    @classmethod
    def poll(cls, context):
        ob = context.object
        return (ob and ob.type == 'ARMATURE' and ob.pose)

    def draw(self, context):
        layout = self.layout

        ob = context.object
        arm = ob.data
        active_bcoll = arm.collections.active

        row = layout.row()

        rows = 1
        if active_bcoll:
            rows = 4

        row.template_list(
            "DATA_UL_bone_collections",
            "collections",
            arm,
            "collections",
            arm.collections,
            "active_index",
            rows=rows,
        )

        col = row.column(align=True)
        col.operator("armature.collection_add", icon='ADD', text="")
        col.operator("armature.collection_remove", icon='REMOVE', text="")
        if active_bcoll:
            col.separator()
            col.operator("armature.collection_move", icon='TRIA_UP', text="").direction = 'UP'
            col.operator("armature.collection_move", icon='TRIA_DOWN', text="").direction = 'DOWN'

        row = layout.row()

        sub = row.row(align=True)
        sub.operator("armature.collection_assign", text="Assign")
        sub.operator("armature.collection_unassign", text="Remove")

        sub = row.row(align=True)
        sub.operator("armature.collection_select", text="Select")
        sub.operator("armature.collection_deselect", text="Deselect")


class DATA_PT_iksolver_itasc(ArmatureButtonsPanel, Panel):
    bl_label = "Inverse Kinematics"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        ob = context.object
        return ob and ob.pose

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        ob = context.object
        itasc = ob.pose.ik_param

        layout.prop(ob.pose, "ik_solver")

        if itasc:
            layout.prop(itasc, "mode")
            layout.prop(itasc, "translate_root_bones")
            simulation = (itasc.mode == 'SIMULATION')
            if simulation:
                layout.prop(itasc, "reiteration_method", expand=False)

            col = layout.column()
            col.active = not simulation or itasc.reiteration_method != 'NEVER'
            col.prop(itasc, "precision")
            col.prop(itasc, "iterations")

            if simulation:
                col.prop(itasc, "use_auto_step")
                sub = layout.column(align=True)
                if itasc.use_auto_step:
                    sub.prop(itasc, "step_min", text="Steps Min")
                    sub.prop(itasc, "step_max", text="Max")
                else:
                    sub.prop(itasc, "step_count", text="Steps")

            col.prop(itasc, "solver")
            if simulation:
                col.prop(itasc, "feedback")
                col.prop(itasc, "velocity_max")
            if itasc.solver == 'DLS':
                col.separator()
                col.prop(itasc, "damping_max", text="Damping Max", slider=True)
                col.prop(itasc, "damping_epsilon", text="Damping Epsilon", slider=True)


class DATA_PT_motion_paths(MotionPathButtonsPanel, Panel):
    # bl_label = "Bones Motion Paths"
    bl_options = {'DEFAULT_CLOSED'}
    bl_context = "data"

    @classmethod
    def poll(cls, context):
        # XXX: include pose-mode check?
        return (context.object) and (context.armature)

    def draw(self, context):
        # layout = self.layout

        ob = context.object
        avs = ob.pose.animation_visualization

        pchan = context.active_pose_bone
        mpath = pchan.motion_path if pchan else None

        self.draw_settings(context, avs, mpath, bones=True)


class DATA_PT_motion_paths_display(MotionPathButtonsPanel_display, Panel):
    # bl_label = "Bones Motion Paths"
    bl_context = "data"
    bl_parent_id = "DATA_PT_motion_paths"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        # XXX: include pose-mode check?
        return (context.object) and (context.armature)

    def draw(self, context):
        # layout = self.layout

        ob = context.object
        avs = ob.pose.animation_visualization

        pchan = context.active_pose_bone
        mpath = pchan.motion_path if pchan else None

        self.draw_settings(context, avs, mpath, bones=True)


class DATA_PT_custom_props_arm(ArmatureButtonsPanel, PropertyPanel, Panel):
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}
    _context_path = "object.data"
    _property_type = bpy.types.Armature


class DATA_PT_custom_props_bcoll(ArmatureButtonsPanel, PropertyPanel, Panel):
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}
    _context_path = "armature.collections.active"
    _property_type = bpy.types.BoneCollection
    bl_parent_id = "DATA_PT_bone_collections"

    @classmethod
    def poll(cls, context):
        return context.armature and context.armature.collections.active


classes = (
    DATA_PT_context_arm,
    DATA_PT_skeleton,
    DATA_PT_bone_collections,
    DATA_UL_bone_collections,
    DATA_PT_motion_paths,
    DATA_PT_motion_paths_display,
    DATA_PT_display,
    DATA_PT_iksolver_itasc,
    DATA_PT_custom_props_arm,
    DATA_PT_custom_props_bcoll,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
