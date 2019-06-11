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
import bpy
from bpy.types import Panel, Menu
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

        col = layout.column()
        col.label(text="Layers:")
        col.prop(arm, "layers", text="")
        col.label(text="Protected Layers:")
        col.prop(arm, "layers_protected", text="")


class DATA_PT_display(ArmatureButtonsPanel, Panel):
    bl_label = "Viewport Display"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        ob = context.object
        arm = context.armature

        layout.prop(arm, "display_type", text="Display As")

        flow = layout.grid_flow(row_major=False, columns=0, even_columns=False, even_rows=False, align=True)
        col = flow.column()
        col.prop(arm, "show_names", text="Names")
        col = flow.column()
        col.prop(arm, "show_axes", text="Axes")
        col = flow.column()
        col.prop(arm, "show_bone_custom_shapes", text="Shapes")
        col = flow.column()
        col.prop(arm, "show_group_colors", text="Group Colors")
        if ob:
            col = flow.column()
            col.prop(ob, "show_in_front", text="In Front")


class DATA_MT_bone_group_context_menu(Menu):
    bl_label = "Bone Group Specials"

    def draw(self, _context):
        layout = self.layout

        layout.operator("pose.group_sort", icon='SORTALPHA')


class DATA_PT_bone_groups(ArmatureButtonsPanel, Panel):
    bl_label = "Bone Groups"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        return (context.object and context.object.type == 'ARMATURE' and context.object.pose)

    def draw(self, context):
        layout = self.layout

        ob = context.object
        pose = ob.pose
        group = pose.bone_groups.active

        row = layout.row()

        rows = 1
        if group:
            rows = 4
        row.template_list(
            "UI_UL_list",
            "bone_groups",
            pose,
            "bone_groups",
            pose.bone_groups,
            "active_index",
            rows=rows,
        )

        col = row.column(align=True)
        col.active = (ob.proxy is None)
        col.operator("pose.group_add", icon='ADD', text="")
        col.operator("pose.group_remove", icon='REMOVE', text="")
        col.menu("DATA_MT_bone_group_context_menu", icon='DOWNARROW_HLT', text="")
        if group:
            col.separator()
            col.operator("pose.group_move", icon='TRIA_UP', text="").direction = 'UP'
            col.operator("pose.group_move", icon='TRIA_DOWN', text="").direction = 'DOWN'

            split = layout.split()
            split.active = (ob.proxy is None)

            col = split.column()
            col.prop(group, "color_set")
            if group.color_set:
                col = split.column()
                sub = col.row(align=True)
                sub.enabled = group.is_custom_color_set  # only custom colors are editable
                sub.prop(group.colors, "normal", text="")
                sub.prop(group.colors, "select", text="")
                sub.prop(group.colors, "active", text="")

        row = layout.row()
        row.active = (ob.proxy is None)

        sub = row.row(align=True)
        sub.operator("pose.group_assign", text="Assign")
        # row.operator("pose.bone_group_remove_from", text="Remove")
        sub.operator("pose.group_unassign", text="Remove")

        sub = row.row(align=True)
        sub.operator("pose.group_select", text="Select")
        sub.operator("pose.group_deselect", text="Deselect")


class DATA_PT_pose_library(ArmatureButtonsPanel, Panel):
    bl_label = "Pose Library"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        return (context.object and context.object.type == 'ARMATURE' and context.object.pose)

    def draw(self, context):
        layout = self.layout

        ob = context.object
        poselib = ob.pose_library

        layout.template_ID(ob, "pose_library", new="poselib.new", unlink="poselib.unlink")

        if poselib:
            # warning about poselib being in an invalid state
            if poselib.fcurves and not poselib.pose_markers:
                layout.label(icon='ERROR', text="Error: Potentially corrupt library, run 'Sanitize' operator to fix")

            # list of poses in pose library
            row = layout.row()
            row.template_list("UI_UL_list", "pose_markers", poselib, "pose_markers",
                              poselib.pose_markers, "active_index", rows=5)

            # column of operators for active pose
            # - goes beside list
            col = row.column(align=True)

            # invoke should still be used for 'add', as it is needed to allow
            # add/replace options to be used properly
            col.operator("poselib.pose_add", icon='ADD', text="")

            col.operator_context = 'EXEC_DEFAULT'  # exec not invoke, so that menu doesn't need showing

            pose_marker_active = poselib.pose_markers.active

            if pose_marker_active is not None:
                col.operator("poselib.pose_remove", icon='REMOVE', text="")
                col.operator(
                    "poselib.apply_pose",
                    icon='ZOOM_SELECTED',
                    text="",
                ).pose_index = poselib.pose_markers.active_index

            col.operator("poselib.action_sanitize", icon='HELP', text="")  # XXX: put in menu?

            if pose_marker_active is not None:
                col.operator("poselib.pose_move", icon='TRIA_UP', text="").direction = 'UP'
                col.operator("poselib.pose_move", icon='TRIA_DOWN', text="").direction = 'DOWN'


class DATA_PT_iksolver_itasc(ArmatureButtonsPanel, Panel):
    bl_label = "Inverse Kinematics"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        ob = context.object
        return (ob and ob.pose)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        ob = context.object
        itasc = ob.pose.ik_param

        layout.prop(ob.pose, "ik_solver")

        if itasc:
            layout.prop(itasc, "mode")
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
    #bl_label = "Bones Motion Paths"
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
    #bl_label = "Bones Motion Paths"
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


classes = (
    DATA_PT_context_arm,
    DATA_PT_skeleton,
    DATA_MT_bone_group_context_menu,
    DATA_PT_bone_groups,
    DATA_PT_pose_library,
    DATA_PT_motion_paths,
    DATA_PT_motion_paths_display,
    DATA_PT_display,
    DATA_PT_iksolver_itasc,
    DATA_PT_custom_props_arm,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
