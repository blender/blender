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


class ObjectButtonsPanel:
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
            row.template_ID(context.view_layer.objects, "active", filter='AVAILABLE')


class OBJECT_PT_transform(ObjectButtonsPanel, Panel):
    bl_label = "Transform"

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=False)

        ob = context.object

        col = flow.column()
        row = col.row(align=True)
        row.prop(ob, "location")
        row.use_property_decorate = False
        row.prop(ob, "lock_location", text="", emboss=False)

        if ob.rotation_mode == 'QUATERNION':
            col = flow.column()
            row = col.row(align=True)
            row.prop(ob, "rotation_quaternion", text="Rotation")
            sub = row.column(align=True)
            sub.use_property_decorate = False
            sub.prop(ob, "lock_rotation_w", text="", emboss=False)
            sub.prop(ob, "lock_rotation", text="", emboss=False)
        elif ob.rotation_mode == 'AXIS_ANGLE':
            # row.column().label(text="Rotation")
            #row.column().prop(pchan, "rotation_angle", text="Angle")
            #row.column().prop(pchan, "rotation_axis", text="Axis")
            col = flow.column()
            row = col.row(align=True)
            row.prop(ob, "rotation_axis_angle", text="Rotation")

            sub = row.column(align=True)
            sub.use_property_decorate = False
            sub.prop(ob, "lock_rotation_w", text="", emboss=False)
            sub.prop(ob, "lock_rotation", text="", emboss=False)
        else:
            col = flow.column()
            row = col.row(align=True)
            row.prop(ob, "rotation_euler", text="Rotation")
            row.use_property_decorate = False
            row.prop(ob, "lock_rotation", text="", emboss=False)

        col = flow.column()
        row = col.row(align=True)
        row.prop(ob, "scale")
        row.use_property_decorate = False
        row.prop(ob, "lock_scale", text="", emboss=False)

        row = layout.row(align=True)
        row.prop(ob, "rotation_mode")
        row.label(text="", icon='BLANK1')


class OBJECT_PT_delta_transform(ObjectButtonsPanel, Panel):
    bl_label = "Delta Transform"
    bl_parent_id = "OBJECT_PT_transform"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=True, align=False)

        ob = context.object

        col = flow.column()
        col.prop(ob, "delta_location")

        col = flow.column()
        if ob.rotation_mode == 'QUATERNION':
            col.prop(ob, "delta_rotation_quaternion", text="Rotation")
        elif ob.rotation_mode == 'AXIS_ANGLE':
            # row.column().label(text="Rotation")
            #row.column().prop(pchan, "delta_rotation_angle", text="Angle")
            #row.column().prop(pchan, "delta_rotation_axis", text="Axis")
            #row.column().prop(ob, "delta_rotation_axis_angle", text="Rotation")
            col.label(text="Not for Axis-Angle")
        else:
            col.prop(ob, "delta_rotation_euler", text="Delta Rotation")

        col = flow.column()
        col.prop(ob, "delta_scale")


class OBJECT_PT_relations(ObjectButtonsPanel, Panel):
    bl_label = "Relations"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=False)

        ob = context.object

        col = flow.column()
        col.prop(ob, "parent")
        sub = col.row(align=True)
        sub.prop(ob, "parent_type")
        parent = ob.parent
        if parent and ob.parent_type == 'BONE' and parent.type == 'ARMATURE':
            sub.prop_search(ob, "parent_bone", parent.data, "bones")
        sub.active = (parent is not None)

        col = flow.column()
        col.active = (ob.parent is not None)
        col.prop(ob, "use_slow_parent")
        sub = col.row(align=True)
        sub.active = (ob.use_slow_parent)
        sub.prop(ob, "slow_parent_offset", text="Offset")

        col = flow.column()
        col.separator()

        col.prop(ob, "track_axis", text="Tracking Axis")
        col.prop(ob, "up_axis", text="Up Axis")

        col = flow.column()
        col.separator()

        col.prop(ob, "pass_index")


class COLLECTION_MT_specials(Menu):
    bl_label = "Collection Specials"

    def draw(self, context):
        layout = self.layout

        layout.operator("object.collection_unlink", icon='X')
        layout.operator("object.collection_objects_select")
        layout.operator("object.dupli_offset_from_cursor")


class OBJECT_PT_collections(ObjectButtonsPanel, Panel):
    bl_label = "Collections"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout

        obj = context.object

        row = layout.row(align=True)
        if bpy.data.collections:
            row.operator("object.collection_link", text="Add to Collection")
        else:
            row.operator("object.collection_add", text="Add to Collection")
        row.operator("object.collection_add", text="", icon='ZOOMIN')

        obj_name = obj.name
        for collection in bpy.data.collections:
            # XXX this is slow and stupid!, we need 2 checks, one thats fast
            # and another that we can be sure its not a name collision
            # from linked library data
            collection_objects = collection.objects
            if obj_name in collection.objects and obj in collection_objects[:]:
                col = layout.column(align=True)

                col.context_pointer_set("collection", collection)

                row = col.box().row()
                row.prop(collection, "name", text="")
                row.operator("object.collection_remove", text="", icon='X', emboss=False)
                row.menu("COLLECTION_MT_specials", icon='DOWNARROW_HLT', text="")

                row = col.box().row()
                row.prop(collection, "dupli_offset", text="")


class OBJECT_PT_display(ObjectButtonsPanel, Panel):
    bl_label = "Viewport Display"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=False)

        obj = context.object
        obj_type = obj.type
        is_geometry = (obj_type in {'MESH', 'CURVE', 'SURFACE', 'META', 'FONT'})
        is_wire = (obj_type in {'CAMERA', 'EMPTY'})
        is_empty_image = (obj_type == 'EMPTY' and obj.empty_draw_type == 'IMAGE')
        is_dupli = (obj.dupli_type != 'NONE')

        col = flow.column(align=True)
        col.prop(obj, "show_name", text="Name")
        col.prop(obj, "show_axis", text="Axis")

        # Makes no sense for cameras, armatures, etc.!
        # but these settings do apply to dupli instances
        col = flow.column(align=True)
        if is_geometry or is_dupli:
            col.prop(obj, "show_wire", text="Wireframe")
        if obj_type == 'MESH' or is_dupli:
            col.prop(obj, "show_all_edges")

        col = flow.column()
        col.prop(obj, "show_bounds", text="Bounds")
        sub = col.column()
        sub.active = obj.show_bounds
        sub.prop(obj, "draw_bounds_type")

        col = flow.column()
        if is_geometry:
            col.prop(obj, "show_texture_space", text="Texture Space")
            col.prop(obj.display, "show_shadows", text="Shadow")

        col.prop(obj, "show_x_ray", text="X-Ray")
        # if obj_type == 'MESH' or is_empty_image:
        #    col.prop(obj, "show_transparent", text="Transparency")

        col = flow.column()
        if is_wire:
            # wire objects only use the max. draw type for duplis
            col.active = is_dupli
        col.prop(
            obj, "draw_type",
            text="Maximum Draw Type" if is_wire else "Maximum Draw Type",
        )

        if is_geometry or is_empty_image:
            # Only useful with object having faces/materials...
            col.prop(obj, "color")


class OBJECT_PT_duplication(ObjectButtonsPanel, Panel):
    bl_label = "Duplication"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout

        ob = context.object

        row = layout.row()
        row.prop(ob, "dupli_type", expand=True)

        layout.use_property_split = True
        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=False)

        if ob.dupli_type == 'FRAMES':

            col = flow.column(align=True)
            col.prop(ob, "dupli_frames_start", text="Start")
            col.prop(ob, "dupli_frames_end", text="End")

            col = flow.column(align=True)
            col.prop(ob, "dupli_frames_on", text="On")
            col.prop(ob, "dupli_frames_off", text="Off")

            col = flow.column(align=True)
            col.prop(ob, "use_dupli_frames_speed", text="Speed")

        elif ob.dupli_type == 'VERTS':
            layout.prop(ob, "use_dupli_vertices_rotation", text="Rotation")

        elif ob.dupli_type == 'FACES':
            col = flow.column()
            col.prop(ob, "use_dupli_faces_scale", text="Scale")
            sub = col.column()
            sub.active = ob.use_dupli_faces_scale
            sub.prop(ob, "dupli_faces_scale", text="Inherit Scale")

        elif ob.dupli_type == 'COLLECTION':
            col = flow.column()
            col.prop(ob, "dupli_group", text="Collection")

        if ob.dupli_type != 'NONE' or len(ob.particle_systems):
            col = flow.column(align=True)
            col.prop(ob, "show_duplicator_for_viewport")
            col.prop(ob, "show_duplicator_for_render")


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
        # layout = self.layout

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
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}
    _context_path = "object"
    _property_type = bpy.types.Object


classes = (
    OBJECT_PT_context_object,
    OBJECT_PT_transform,
    OBJECT_PT_delta_transform,
    OBJECT_PT_relations,
    COLLECTION_MT_specials,
    OBJECT_PT_collections,
    OBJECT_PT_duplication,
    OBJECT_PT_display,
    OBJECT_PT_motion_paths,
    OBJECT_PT_custom_props,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
