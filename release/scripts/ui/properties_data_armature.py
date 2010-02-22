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

narrowui = 180


class DataButtonsPanel(bpy.types.Panel):
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "data"

    def poll(self, context):
        return context.armature


class DATA_PT_context_arm(DataButtonsPanel):
    bl_label = ""
    bl_show_header = False

    def draw(self, context):
        layout = self.layout

        ob = context.object
        arm = context.armature
        space = context.space_data
        wide_ui = context.region.width > narrowui

        if wide_ui:
            split = layout.split(percentage=0.65)
            if ob:
                split.template_ID(ob, "data")
                split.separator()
            elif arm:
                split.template_ID(space, "pin_id")
                split.separator()
        else:
            layout.template_ID(ob, "data")


class DATA_PT_custom_props_arm(DataButtonsPanel, PropertyPanel):
    _context_path = "object.data"


class DATA_PT_skeleton(DataButtonsPanel):
    bl_label = "Skeleton"

    def draw(self, context):
        layout = self.layout

        arm = context.armature
        wide_ui = context.region.width > narrowui

        layout.prop(arm, "pose_position", expand=True)

        split = layout.split()

        col = split.column()
        col.label(text="Layers:")
        col.prop(arm, "layer", text="")
        col.label(text="Protected Layers:")
        col.prop(arm, "layer_protection", text="")

        if wide_ui:
            col = split.column()
        col.label(text="Deform:")
        col.prop(arm, "deform_vertexgroups", text="Vertex Groups")
        col.prop(arm, "deform_envelope", text="Envelopes")
        col.prop(arm, "deform_quaternion", text="Quaternion")
        col.prop(arm, "deform_bbone_rest", text="B-Bones Rest")


class DATA_PT_display(DataButtonsPanel):
    bl_label = "Display"

    def draw(self, context):
        layout = self.layout

        ob = context.object
        arm = context.armature
        wide_ui = context.region.width > narrowui

        if wide_ui:
            layout.row().prop(arm, "drawtype", expand=True)
        else:
            layout.row().prop(arm, "drawtype", text="")

        split = layout.split()

        col = split.column()
        col.prop(arm, "draw_names", text="Names")
        col.prop(arm, "draw_axes", text="Axes")
        col.prop(arm, "draw_custom_bone_shapes", text="Shapes")

        if wide_ui:
            col = split.column()
        col.prop(arm, "draw_group_colors", text="Colors")
        col.prop(arm, "delay_deform", text="Delay Refresh")
        col.prop(ob, "x_ray", text="X-Ray (Object)")


class DATA_PT_bone_groups(DataButtonsPanel):
    bl_label = "Bone Groups"

    def poll(self, context):
        return (context.object and context.object.type == 'ARMATURE' and context.object.pose)

    def draw(self, context):
        layout = self.layout

        ob = context.object
        pose = ob.pose
        wide_ui = context.region.width > narrowui

        row = layout.row()
        row.template_list(pose, "bone_groups", pose, "active_bone_group_index", rows=2)

        col = row.column(align=True)
        col.active = (ob.proxy is None)
        col.operator("pose.group_add", icon='ZOOMIN', text="")
        col.operator("pose.group_remove", icon='ZOOMOUT', text="")

        group = pose.active_bone_group
        if group:
            col = layout.column()
            col.active = (ob.proxy is None)
            col.prop(group, "name")

            split = layout.split()
            split.active = (ob.proxy is None)

            col = split.column()
            col.prop(group, "color_set")
            if group.color_set:
                if wide_ui:
                    col = split.column()
                col.template_triColorSet(group, "colors")

        row = layout.row(align=True)
        row.active = (ob.proxy is None)

        row.operator("pose.group_assign", text="Assign")
        row.operator("pose.group_unassign", text="Remove") #row.operator("pose.bone_group_remove_from", text="Remove")
        #row.operator("object.bone_group_select", text="Select")
        #row.operator("object.bone_group_deselect", text="Deselect")


# TODO: this panel will soon be depreceated too


class DATA_PT_ghost(DataButtonsPanel):
    bl_label = "Ghost"

    def draw(self, context):
        layout = self.layout

        arm = context.armature
        wide_ui = context.region.width > narrowui

        if wide_ui:
            layout.prop(arm, "ghost_type", expand=True)
        else:
            layout.prop(arm, "ghost_type", text="")

        split = layout.split()

        col = split.column()

        sub = col.column(align=True)
        if arm.ghost_type == 'RANGE':
            sub.prop(arm, "ghost_start_frame", text="Start")
            sub.prop(arm, "ghost_end_frame", text="End")
            sub.prop(arm, "ghost_size", text="Step")
        elif arm.ghost_type == 'CURRENT_FRAME':
            sub.prop(arm, "ghost_step", text="Range")
            sub.prop(arm, "ghost_size", text="Step")

        if wide_ui:
            col = split.column()
        col.label(text="Display:")
        col.prop(arm, "ghost_only_selected", text="Selected Only")


class DATA_PT_iksolver_itasc(DataButtonsPanel):
    bl_label = "iTaSC parameters"
    bl_default_closed = True

    def poll(self, context):
        ob = context.object
        return (ob and ob.pose)

    def draw(self, context):
        layout = self.layout

        ob = context.object

        itasc = ob.pose.ik_param
        wide_ui = (context.region.width > narrowui)

        row = layout.row()
        row.prop(ob.pose, "ik_solver")

        if itasc:
            layout.prop(itasc, "mode", expand=True)
            simulation = (itasc.mode == 'SIMULATION')
            if simulation:
                layout.label(text="Reiteration:")
                layout.prop(itasc, "reiteration", expand=True)

            split = layout.split()
            split.active = not simulation or itasc.reiteration != 'NEVER'
            col = split.column()
            col.prop(itasc, "precision")

            if wide_ui:
                col = split.column()
            col.prop(itasc, "num_iter")


            if simulation:
                layout.prop(itasc, "auto_step")
                row = layout.row()
                if itasc.auto_step:
                    row.prop(itasc, "min_step", text="Min")
                    row.prop(itasc, "max_step", text="Max")
                else:
                    row.prop(itasc, "num_step")

            layout.prop(itasc, "solver")
            if simulation:
                layout.prop(itasc, "feedback")
                layout.prop(itasc, "max_velocity")
            if itasc.solver == 'DLS':
                row = layout.row()
                row.prop(itasc, "dampmax", text="Damp", slider=True)
                row.prop(itasc, "dampeps", text="Eps", slider=True)

classes = [
    DATA_PT_context_arm,
    DATA_PT_skeleton,
    DATA_PT_display,
    DATA_PT_bone_groups,
    DATA_PT_ghost,
    DATA_PT_iksolver_itasc,

    DATA_PT_custom_props_arm]


def register():
    register = bpy.types.register
    for cls in classes:
        register(cls)


def unregister():
    unregister = bpy.types.unregister
    for cls in classes:
        unregister(cls)

if __name__ == "__main__":
    register()
