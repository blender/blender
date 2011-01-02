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
from rna_prop_ui import PropertyPanel


class ArmatureButtonsPanel():
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "data"

    @classmethod
    def poll(cls, context):
        return context.armature


class DATA_PT_context_arm(ArmatureButtonsPanel, bpy.types.Panel):
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


class DATA_PT_skeleton(ArmatureButtonsPanel, bpy.types.Panel):
    bl_label = "Skeleton"

    def draw(self, context):
        layout = self.layout

        arm = context.armature

        layout.prop(arm, "pose_position", expand=True)

        split = layout.split()

        col = split.column()
        col.label(text="Layers:")
        col.prop(arm, "layers", text="")
        col.label(text="Protected Layers:")
        col.prop(arm, "layers_protected", text="")

        col.label(text="Deform:")

        split = layout.split()

        col = split.column()
        col.prop(arm, "use_deform_vertex_groups", text="Vertex Groups")
        col.prop(arm, "use_deform_envelopes", text="Envelopes")

        col = split.column()
        col.prop(arm, "use_deform_preserve_volume", text="Quaternion")


class DATA_PT_display(ArmatureButtonsPanel, bpy.types.Panel):
    bl_label = "Display"

    def draw(self, context):
        layout = self.layout

        ob = context.object
        arm = context.armature

        layout.row().prop(arm, "draw_type", expand=True)

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


class DATA_PT_bone_groups(ArmatureButtonsPanel, bpy.types.Panel):
    bl_label = "Bone Groups"

    @classmethod
    def poll(cls, context):
        return (context.object and context.object.type == 'ARMATURE' and context.object.pose)

    def draw(self, context):
        layout = self.layout

        ob = context.object
        pose = ob.pose

        row = layout.row()
        row.template_list(pose, "bone_groups", pose.bone_groups, "active_index", rows=2)

        col = row.column(align=True)
        col.active = (ob.proxy is None)
        col.operator("pose.group_add", icon='ZOOMIN', text="")
        col.operator("pose.group_remove", icon='ZOOMOUT', text="")

        group = pose.bone_groups.active
        if group:
            col = layout.column()
            col.active = (ob.proxy is None)
            col.prop(group, "name")

            split = layout.split()
            split.active = (ob.proxy is None)

            col = split.column()
            col.prop(group, "color_set")
            if group.color_set:
                col = split.column()
                subrow = col.row(align=True)
                subrow.prop(group.colors, "normal", text="")
                subrow.prop(group.colors, "select", text="")
                subrow.prop(group.colors, "active", text="")

        row = layout.row()
        row.active = (ob.proxy is None)

        sub = row.row(align=True)
        sub.operator("pose.group_assign", text="Assign")
        sub.operator("pose.group_unassign", text="Remove")  # row.operator("pose.bone_group_remove_from", text="Remove")

        sub = row.row(align=True)
        sub.operator("pose.group_select", text="Select")
        sub.operator("pose.group_deselect", text="Deselect")


# TODO: this panel will soon be depreceated too
class DATA_PT_ghost(ArmatureButtonsPanel, bpy.types.Panel):
    bl_label = "Ghost"

    def draw(self, context):
        layout = self.layout

        arm = context.armature

        layout.prop(arm, "ghost_type", expand=True)

        split = layout.split()

        col = split.column()

        sub = col.column(align=True)
        if arm.ghost_type == 'RANGE':
            sub.prop(arm, "ghost_frame_start", text="Start")
            sub.prop(arm, "ghost_frame_end", text="End")
            sub.prop(arm, "ghost_size", text="Step")
        elif arm.ghost_type == 'CURRENT_FRAME':
            sub.prop(arm, "ghost_step", text="Range")
            sub.prop(arm, "ghost_size", text="Step")

        col = split.column()
        col.label(text="Display:")
        col.prop(arm, "show_only_ghost_selected", text="Selected Only")


class DATA_PT_iksolver_itasc(ArmatureButtonsPanel, bpy.types.Panel):
    bl_label = "iTaSC parameters"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        ob = context.object
        return (ob and ob.pose)

    def draw(self, context):
        layout = self.layout

        ob = context.object

        itasc = ob.pose.ik_param

        row = layout.row()
        row.prop(ob.pose, "ik_solver")

        if itasc:
            layout.prop(itasc, "mode", expand=True)
            simulation = (itasc.mode == 'SIMULATION')
            if simulation:
                layout.label(text="Reiteration:")
                layout.prop(itasc, "reiteration_method", expand=True)

            split = layout.split()
            split.active = not simulation or itasc.reiteration_method != 'NEVER'
            col = split.column()
            col.prop(itasc, "precision")

            col = split.column()
            col.prop(itasc, "iterations")

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

from properties_animviz import MotionPathButtonsPanel, OnionSkinButtonsPanel


class DATA_PT_motion_paths(MotionPathButtonsPanel, bpy.types.Panel):
    #bl_label = "Bones Motion Paths"
    bl_context = "data"

    @classmethod
    def poll(cls, context):
        # XXX: include posemode check?
        return (context.object) and (context.armature)

    def draw(self, context):
        layout = self.layout

        ob = context.object

        self.draw_settings(context, ob.pose.animation_visualisation, bones=True)

        layout.separator()

        split = layout.split()

        col = split.column()
        col.operator("pose.paths_calculate", text="Calculate Paths")

        col = split.column()
        col.operator("pose.paths_clear", text="Clear Paths")


class DATA_PT_onion_skinning(OnionSkinButtonsPanel):  # , bpy.types.Panel): # inherit from panel when ready
    #bl_label = "Bones Onion Skinning"
    bl_context = "data"

    @classmethod
    def poll(cls, context):
        # XXX: include posemode check?
        return (context.object) and (context.armature)

    def draw(self, context):
        layout = self.layout

        ob = context.object

        self.draw_settings(context, ob.pose.animation_visualisation, bones=True)


class DATA_PT_custom_props_arm(ArmatureButtonsPanel, PropertyPanel, bpy.types.Panel):
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}
    _context_path = "object.data"
    _property_type = bpy.types.Armature


def register():
    pass


def unregister():
    pass

if __name__ == "__main__":
    register()
