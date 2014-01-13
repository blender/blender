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
            row.column().prop(ob, "delta_rotation_euler", text="Delta Rotation")

        row.column().prop(ob, "delta_scale")


class OBJECT_PT_transform_locks(ObjectButtonsPanel, Panel):
    bl_label = "Transform Locks"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout

        ob = context.object

        split = layout.split(percentage=0.1)

        col = split.column(align=True)
        col.label(text="")
        col.label(text="X:")
        col.label(text="Y:")
        col.label(text="Z:")

        split.column().prop(ob, "lock_location", text="Location")
        split.column().prop(ob, "lock_rotation", text="Rotation")
        split.column().prop(ob, "lock_scale", text="Scale")

        if ob.rotation_mode in {'QUATERNION', 'AXIS_ANGLE'}:
            row = layout.row()
            row.prop(ob, "lock_rotations_4d", text="Lock Rotation")

            sub = row.row()
            sub.active = ob.lock_rotations_4d
            sub.prop(ob, "lock_rotation_w", text="W")


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

        obj = context.object

        row = layout.row(align=True)
        if bpy.data.groups:
            row.operator("object.group_link", text="Add to Group")
        else:
            row.operator("object.group_add", text="Add to Group")
        row.operator("object.group_add", text="", icon='ZOOMIN')

        # XXX, this is bad practice, yes, I wrote it :( - campbell
        index = 0
        obj_name = obj.name
        for group in bpy.data.groups:
            # XXX this is slow and stupid!, we need 2 checks, one thats fast
            # and another that we can be sure its not a name collision
            # from linked library data
            group_objects = group.objects
            if obj_name in group.objects and obj in group_objects[:]:
                col = layout.column(align=True)

                col.context_pointer_set("group", group)

                row = col.box().row()
                row.prop(group, "name", text="")
                row.operator("object.group_remove", text="", icon='X', emboss=False)

                split = col.box().split()

                col = split.column()
                col.prop(group, "layers", text="Dupli Visibility")

                col = split.column()
                col.prop(group, "dupli_offset", text="")

                props = col.operator("object.dupli_offset_from_cursor", text="From Cursor")
                props.group = index
                index += 1


class OBJECT_PT_display(ObjectButtonsPanel, Panel):
    bl_label = "Display"

    def draw(self, context):
        layout = self.layout

        obj = context.object
        obj_type = obj.type
        is_geometry = (obj_type in {'MESH', 'CURVE', 'SURFACE', 'META', 'FONT'})
        is_empty_image = (obj_type == 'EMPTY' and obj.empty_draw_type == 'IMAGE')

        split = layout.split()

        col = split.column()
        col.prop(obj, "show_name", text="Name")
        col.prop(obj, "show_axis", text="Axis")
        if is_geometry:
            # Makes no sense for cameras, armatures, etc.!
            col.prop(obj, "show_wire", text="Wire")
        if obj_type == 'MESH':
            col.prop(obj, "show_all_edges")

        col = split.column()
        row = col.row()
        row.prop(obj, "show_bounds", text="Bounds")
        sub = row.row()
        sub.active = obj.show_bounds
        sub.prop(obj, "draw_bounds_type", text="")

        if is_geometry:
            col.prop(obj, "show_texture_space", text="Texture Space")
        col.prop(obj, "show_x_ray", text="X-Ray")
        if obj_type == 'MESH' or is_empty_image:
            col.prop(obj, "show_transparent", text="Transparency")

        split = layout.split()

        col = split.column()
        if obj_type not in {'CAMERA', 'EMPTY'}:
            col.label(text="Maximum Draw Type:")
            col.prop(obj, "draw_type", text="")

        col = split.column()
        if is_geometry or is_empty_image:
            # Only useful with object having faces/materials...
            col.label(text="Object Color:")
            col.prop(obj, "color", text="")


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
            sub = row.row()
            sub.active = ob.use_dupli_faces_scale
            sub.prop(ob, "dupli_faces_scale", text="Inherit Scale")

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

        layout.prop(ob, "use_extra_recalc_object")
        layout.prop(ob, "use_extra_recalc_data")


from bl_ui.properties_animviz import (MotionPathButtonsPanel,
                                      OnionSkinButtonsPanel)


class OBJECT_PT_motion_paths(MotionPathButtonsPanel, Panel):
    #bl_label = "Object Motion Paths"
    bl_context = "object"

    @classmethod
    def poll(cls, context):
        return (context.object)

    def draw(self, context):
        layout = self.layout

        ob = context.object
        avs = ob.animation_visualization
        mpath = ob.motion_path

        self.draw_settings(context, avs, mpath)


class OBJECT_PT_onion_skinning(OnionSkinButtonsPanel):  # , Panel): # inherit from panel when ready
    #bl_label = "Object Onion Skinning"
    bl_context = "object"

    @classmethod
    def poll(cls, context):
        return (context.object)

    def draw(self, context):
        ob = context.object

        self.draw_settings(context, ob.animation_visualization)


class OBJECT_PT_custom_props(ObjectButtonsPanel, PropertyPanel, Panel):
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}
    _context_path = "object"
    _property_type = bpy.types.Object

if __name__ == "__main__":  # only for live edit.
    bpy.utils.register_module(__name__)
