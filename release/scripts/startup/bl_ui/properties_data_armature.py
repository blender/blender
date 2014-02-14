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


class ArmatureButtonsPanel():
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

        layout.prop(arm, "pose_position", expand=True)

        col = layout.column()
        col.label(text="Layers:")
        col.prop(arm, "layers", text="")
        col.label(text="Protected Layers:")
        col.prop(arm, "layers_protected", text="")

        if context.scene.render.engine == 'BLENDER_GAME':
            col = layout.column()
            col.label(text="Deform:")
            col.prop(arm, "deform_method", expand=True)


class DATA_PT_display(ArmatureButtonsPanel, Panel):
    bl_label = "Display"

    def draw(self, context):
        layout = self.layout

        ob = context.object
        arm = context.armature

        layout.prop(arm, "draw_type", expand=True)

        split = layout.split()

        col = split.column()
        col.prop(arm, "show_names", text="Names")
        col.prop(arm, "show_axes", text="Axes")
        col.prop(arm, "show_bone_custom_shapes", text="Shapes")

        col = split.column()
        col.prop(arm, "show_group_colors", text="Colors")
        if ob:
            col.prop(ob, "show_x_ray", text="X-Ray")
        col.prop(arm, "use_deform_delay", text="Delay Refresh")


class DATA_PT_bone_group_specials(Menu):
    bl_label = "Bone Group Specials"

    def draw(self, context):
        layout = self.layout

        layout.operator("pose.group_sort", icon='SORTALPHA')


class DATA_PT_bone_groups(ArmatureButtonsPanel, Panel):
    bl_label = "Bone Groups"

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
        row.template_list("UI_UL_list", "bone_groups", pose, "bone_groups", pose.bone_groups, "active_index", rows=rows)

        col = row.column(align=True)
        col.active = (ob.proxy is None)
        col.operator("pose.group_add", icon='ZOOMIN', text="")
        col.operator("pose.group_remove", icon='ZOOMOUT', text="")
        col.menu("DATA_PT_bone_group_specials", icon='DOWNARROW_HLT', text="")
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
                sub.prop(group.colors, "normal", text="")
                sub.prop(group.colors, "select", text="")
                sub.prop(group.colors, "active", text="")

        row = layout.row()
        row.active = (ob.proxy is None)

        sub = row.row(align=True)
        sub.operator("pose.group_assign", text="Assign")
        sub.operator("pose.group_unassign", text="Remove")  # row.operator("pose.bone_group_remove_from", text="Remove")

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
            # list of poses in pose library
            row = layout.row()
            row.template_list("UI_UL_list", "pose_markers", poselib, "pose_markers",
                              poselib.pose_markers, "active_index", rows=3)

            # column of operators for active pose
            # - goes beside list
            col = row.column(align=True)
            col.active = (poselib.library is None)

            # invoke should still be used for 'add', as it is needed to allow
            # add/replace options to be used properly
            col.operator("poselib.pose_add", icon='ZOOMIN', text="")

            col.operator_context = 'EXEC_DEFAULT'  # exec not invoke, so that menu doesn't need showing

            pose_marker_active = poselib.pose_markers.active

            if pose_marker_active is not None:
                col.operator("poselib.pose_remove", icon='ZOOMOUT', text="")
                col.operator("poselib.apply_pose", icon='ZOOM_SELECTED', text="").pose_index = poselib.pose_markers.active_index

            col.operator("poselib.action_sanitize", icon='HELP', text="")  # XXX: put in menu?


# TODO: this panel will soon be deprecated too
class DATA_PT_ghost(ArmatureButtonsPanel, Panel):
    bl_label = "Ghost"

    def draw(self, context):
        layout = self.layout

        arm = context.armature

        layout.prop(arm, "ghost_type", expand=True)

        split = layout.split()

        col = split.column(align=True)

        if arm.ghost_type == 'RANGE':
            col.prop(arm, "ghost_frame_start", text="Start")
            col.prop(arm, "ghost_frame_end", text="End")
            col.prop(arm, "ghost_size", text="Step")
        elif arm.ghost_type == 'CURRENT_FRAME':
            col.prop(arm, "ghost_step", text="Range")
            col.prop(arm, "ghost_size", text="Step")

        col = split.column()
        col.label(text="Display:")
        col.prop(arm, "show_only_ghost_selected", text="Selected Only")


class DATA_PT_iksolver_itasc(ArmatureButtonsPanel, Panel):
    bl_label = "Inverse Kinematics"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        ob = context.object
        return (ob and ob.pose)

    def draw(self, context):
        layout = self.layout

        ob = context.object
        itasc = ob.pose.ik_param

        layout.prop(ob.pose, "ik_solver")

        if itasc:
            layout.prop(itasc, "mode", expand=True)
            simulation = (itasc.mode == 'SIMULATION')
            if simulation:
                layout.label(text="Reiteration:")
                layout.prop(itasc, "reiteration_method", expand=True)

            row = layout.row()
            row.active = not simulation or itasc.reiteration_method != 'NEVER'
            row.prop(itasc, "precision")
            row.prop(itasc, "iterations")

            if simulation:
                layout.prop(itasc, "use_auto_step")
                row = layout.row()
                if itasc.use_auto_step:
                    row.prop(itasc, "step_min", text="Min")
                    row.prop(itasc, "step_max", text="Max")
                else:
                    row.prop(itasc, "step_count")

            layout.prop(itasc, "solver")
            if simulation:
                layout.prop(itasc, "feedback")
                layout.prop(itasc, "velocity_max")
            if itasc.solver == 'DLS':
                row = layout.row()
                row.prop(itasc, "damping_max", text="Damp", slider=True)
                row.prop(itasc, "damping_epsilon", text="Eps", slider=True)

from bl_ui.properties_animviz import (MotionPathButtonsPanel,
                                      OnionSkinButtonsPanel,
                                      )


class DATA_PT_motion_paths(MotionPathButtonsPanel, Panel):
    #bl_label = "Bones Motion Paths"
    bl_context = "data"

    @classmethod
    def poll(cls, context):
        # XXX: include pose-mode check?
        return (context.object) and (context.armature)

    def draw(self, context):
        layout = self.layout

        ob = context.object
        avs = ob.pose.animation_visualization

        pchan = context.active_pose_bone
        mpath = pchan.motion_path if pchan else None

        self.draw_settings(context, avs, mpath, bones=True)


class DATA_PT_onion_skinning(OnionSkinButtonsPanel):  # , Panel): # inherit from panel when ready
    #bl_label = "Bones Onion Skinning"
    bl_context = "data"

    @classmethod
    def poll(cls, context):
        # XXX: include pose-mode check?
        return (context.object) and (context.armature)

    def draw(self, context):
        ob = context.object

        self.draw_settings(context, ob.pose.animation_visualization, bones=True)


class DATA_PT_custom_props_arm(ArmatureButtonsPanel, PropertyPanel, Panel):
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}
    _context_path = "object.data"
    _property_type = bpy.types.Armature

if __name__ == "__main__":  # only for live edit.
    bpy.utils.register_module(__name__)
