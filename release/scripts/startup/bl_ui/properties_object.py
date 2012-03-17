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
from bpy.types import Panel
from rna_prop_ui import PropertyPanel


class ObjectButtonsPanel():
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "object"


class OBJECT_PT_context_object(ObjectButtonsPanel, Panel):
    bl_label = ""
    bl_options = {'HIDE_HEADER'}

    def draw(self, context):
        layout = self.layout
        space = context.space_data

        if space.use_pin_id:
            layout.template_ID(space, "pin_id")
        else:
            row = layout.row()
            row.template_ID(context.scene.objects, "active")


class OBJECT_PT_transform(ObjectButtonsPanel, Panel):
    bl_label = "Transform"

    def draw(self, context):
        layout = self.layout

        ob = context.object

        row = layout.row()

        row.column().prop(ob, "location")
        if ob.rotation_mode == 'QUATERNION':
            row.column().prop(ob, "rotation_quaternion", text="Rotation")
        elif ob.rotation_mode == 'AXIS_ANGLE':
            #row.column().label(text="Rotation")
            #row.column().prop(pchan, "rotation_angle", text="Angle")
            #row.column().prop(pchan, "rotation_axis", text="Axis")
            row.column().prop(ob, "rotation_axis_angle", text="Rotation")
        else:
            row.column().prop(ob, "rotation_euler", text="Rotation")

        row.column().prop(ob, "scale")

        layout.prop(ob, "rotation_mode")


class OBJECT_PT_delta_transform(ObjectButtonsPanel, Panel):
    bl_label = "Delta Transform"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout

        ob = context.object

        row = layout.row()

        row.column().prop(ob, "delta_location")
        if ob.rotation_mode == 'QUATERNION':
            row.column().prop(ob, "delta_rotation_quaternion", text="Rotation")
        elif ob.rotation_mode == 'AXIS_ANGLE':
            #row.column().label(text="Rotation")
            #row.column().prop(pchan, "delta_rotation_angle", text="Angle")
            #row.column().prop(pchan, "delta_rotation_axis", text="Axis")
            #row.column().prop(ob, "delta_rotation_axis_angle", text="Rotation")
            row.column().label(text="Not for Axis-Angle")
        else:
            row.column().prop(ob, "delta_rotation_euler", text="Delat Rotation")

        row.column().prop(ob, "delta_scale")


class OBJECT_PT_transform_locks(ObjectButtonsPanel, Panel):
    bl_label = "Transform Locks"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout

        ob = context.object

        row = layout.row()

        col = row.column()
        col.prop(ob, "lock_location", text="Location")

        col = row.column()
        if ob.rotation_mode in {'QUATERNION', 'AXIS_ANGLE'}:
            col.prop(ob, "lock_rotations_4d", text="Rotation")
            if ob.lock_rotations_4d:
                col.prop(ob, "lock_rotation_w", text="W")
            col.prop(ob, "lock_rotation", text="")
        else:
            col.prop(ob, "lock_rotation", text="Rotation")

        row.column().prop(ob, "lock_scale", text="Scale")


class OBJECT_PT_relations(ObjectButtonsPanel, Panel):
    bl_label = "Relations"

    def draw(self, context):
        layout = self.layout

        ob = context.object

        split = layout.split()

        col = split.column()
        col.prop(ob, "layers")
        col.separator()
        col.prop(ob, "pass_index")

        col = split.column()
        col.label(text="Parent:")
        col.prop(ob, "parent", text="")

        sub = col.column()
        sub.prop(ob, "parent_type", text="")
        parent = ob.parent
        if parent and ob.parent_type == 'BONE' and parent.type == 'ARMATURE':
            sub.prop_search(ob, "parent_bone", parent.data, "bones", text="")
        sub.active = (parent is not None)


class OBJECT_PT_groups(ObjectButtonsPanel, Panel):
    bl_label = "Groups"

    def draw(self, context):
        layout = self.layout

        ob = context.object

        row = layout.row(align=True)
        row.operator("object.group_link", text="Add to Group")
        row.operator("object.group_add", text="", icon='ZOOMIN')

        # XXX, this is bad practice, yes, I wrote it :( - campbell
        index = 0
        value = str(tuple(context.scene.cursor_location))
        for group in bpy.data.groups:
            if ob.name in group.objects:
                col = layout.column(align=True)

                col.context_pointer_set("group", group)

                row = col.box().row()
                row.prop(group, "name", text="")
                row.operator("object.group_remove", text="", icon='X', emboss=False)

                split = col.box().split()

                col = split.column()
                col.prop(group, "layers", text="Dupli")

                col = split.column()
                col.prop(group, "dupli_offset", text="")

                props = col.operator("wm.context_set_value", text="From Cursor")
                props.data_path = "object.users_group[%d].dupli_offset" % index
                props.value = value
                index += 1


class OBJECT_PT_display(ObjectButtonsPanel, Panel):
    bl_label = "Display"

    def draw(self, context):
        layout = self.layout

        ob = context.object

        split = layout.split()
        col = split.column()
        col.prop(ob, "draw_type", text="Type")

        col = split.column()
        row = col.row()
        row.prop(ob, "show_bounds", text="Bounds")
        sub = row.row()
        sub.active = ob.show_bounds
        sub.prop(ob, "draw_bounds_type", text="")

        split = layout.split()

        col = split.column()
        col.prop(ob, "show_name", text="Name")
        col.prop(ob, "show_axis", text="Axis")
        col.prop(ob, "show_wire", text="Wire")
        col.prop(ob, "color", text="Object Color")

        col = split.column()
        col.prop(ob, "show_texture_space", text="Texture Space")
        col.prop(ob, "show_x_ray", text="X-Ray")
        if ob.type == 'MESH':
            col.prop(ob, "show_transparent", text="Transparency")


class OBJECT_PT_duplication(ObjectButtonsPanel, Panel):
    bl_label = "Duplication"

    def draw(self, context):
        layout = self.layout

        ob = context.object

        layout.prop(ob, "dupli_type", expand=True)

        if ob.dupli_type == 'FRAMES':
            split = layout.split()

            col = split.column(align=True)
            col.prop(ob, "dupli_frames_start", text="Start")
            col.prop(ob, "dupli_frames_end", text="End")

            col = split.column(align=True)
            col.prop(ob, "dupli_frames_on", text="On")
            col.prop(ob, "dupli_frames_off", text="Off")

            layout.prop(ob, "use_dupli_frames_speed", text="Speed")

        elif ob.dupli_type == 'VERTS':
            layout.prop(ob, "use_dupli_vertices_rotation", text="Rotation")

        elif ob.dupli_type == 'FACES':

            row = layout.row()
            row.prop(ob, "use_dupli_faces_scale", text="Scale")
            row.prop(ob, "dupli_faces_scale", text="Inherit Scale")

        elif ob.dupli_type == 'GROUP':
            layout.prop(ob, "dupli_group", text="Group")


class OBJECT_PT_relations_extras(ObjectButtonsPanel, Panel):
    bl_label = "Relations Extras"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout

        ob = context.object

        split = layout.split()

        col = split.column()
        col.label(text="Tracking Axes:")
        col.prop(ob, "track_axis", text="Axis")
        col.prop(ob, "up_axis", text="Up Axis")

        col = split.column()
        col.prop(ob, "use_slow_parent")
        row = col.row()
        row.active = ((ob.parent is not None) and (ob.use_slow_parent))
        row.prop(ob, "slow_parent_offset", text="Offset")


from .properties_animviz import (
    MotionPathButtonsPanel,
    OnionSkinButtonsPanel,
    )


class OBJECT_PT_motion_paths(MotionPathButtonsPanel, Panel):
    #bl_label = "Object Motion Paths"
    bl_context = "object"

    @classmethod
    def poll(cls, context):
        return (context.object)

    def draw(self, context):
        layout = self.layout

        ob = context.object

        self.draw_settings(context, ob.animation_visualisation)

        layout.separator()

        row = layout.row()
        row.operator("object.paths_calculate", text="Calculate Paths")
        row.operator("object.paths_clear", text="Clear Paths")


class OBJECT_PT_onion_skinning(OnionSkinButtonsPanel):  # , Panel): # inherit from panel when ready
    #bl_label = "Object Onion Skinning"
    bl_context = "object"

    @classmethod
    def poll(cls, context):
        return (context.object)

    def draw(self, context):
        ob = context.object

        self.draw_settings(context, ob.animation_visualisation)


class OBJECT_PT_custom_props(ObjectButtonsPanel, PropertyPanel, Panel):
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}
    _context_path = "object"
    _property_type = bpy.types.Object

if __name__ == "__main__":  # only for live edit.
    bpy.utils.register_module(__name__)
