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
#  Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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
        return context.mesh


class DATA_PT_context_mesh(DataButtonsPanel):
    bl_label = ""
    bl_show_header = False

    def draw(self, context):
        layout = self.layout

        ob = context.object
        mesh = context.mesh
        space = context.space_data
        wide_ui = context.region.width > narrowui

        if wide_ui:
            split = layout.split(percentage=0.65)
            if ob:
                split.template_ID(ob, "data")
                split.separator()
            elif mesh:
                split.template_ID(space, "pin_id")
                split.separator()
        else:
            if ob:
                layout.template_ID(ob, "data")
            elif mesh:
                layout.template_ID(space, "pin_id")


class DATA_PT_custom_props_mesh(DataButtonsPanel, PropertyPanel):
    _context_path = "object.data"


class DATA_PT_normals(DataButtonsPanel):
    bl_label = "Normals"

    def draw(self, context):
        layout = self.layout

        mesh = context.mesh
        wide_ui = context.region.width > narrowui

        split = layout.split()

        col = split.column()
        col.prop(mesh, "autosmooth")
        sub = col.column()
        sub.active = mesh.autosmooth
        sub.prop(mesh, "autosmooth_angle", text="Angle")

        if wide_ui:
            col = split.column()
        else:
            col.separator()
        col.prop(mesh, "vertex_normal_flip")
        col.prop(mesh, "double_sided")


class DATA_PT_settings(DataButtonsPanel):
    bl_label = "Settings"

    def draw(self, context):
        layout = self.layout

        mesh = context.mesh

        layout.prop(mesh, "texture_mesh")


class DATA_PT_vertex_groups(DataButtonsPanel):
    bl_label = "Vertex Groups"

    def poll(self, context):
        return (context.object and context.object.type in ('MESH', 'LATTICE'))

    def draw(self, context):
        layout = self.layout

        ob = context.object
        group = ob.active_vertex_group

        rows = 2
        if group:
            rows = 5

        row = layout.row()
        row.template_list(ob, "vertex_groups", ob, "active_vertex_group_index", rows=rows)

        col = row.column(align=True)
        col.operator("object.vertex_group_add", icon='ZOOMIN', text="")
        col.operator("object.vertex_group_remove", icon='ZOOMOUT', text="")

        col.operator("object.vertex_group_copy", icon='COPY_ID', text="")
        if ob.data.users > 1:
            col.operator("object.vertex_group_copy_to_linked", icon='LINK_AREA', text="")

        if group:
            row = layout.row()
            row.prop(group, "name")

        if ob.mode == 'EDIT' and len(ob.vertex_groups) > 0:
            row = layout.row()

            sub = row.row(align=True)
            sub.operator("object.vertex_group_assign", text="Assign")
            sub.operator("object.vertex_group_remove_from", text="Remove")

            sub = row.row(align=True)
            sub.operator("object.vertex_group_select", text="Select")
            sub.operator("object.vertex_group_deselect", text="Deselect")

            layout.prop(context.tool_settings, "vertex_group_weight", text="Weight")


class DATA_PT_shape_keys(DataButtonsPanel):
    bl_label = "Shape Keys"

    def poll(self, context):
        return (context.object and context.object.type in ('MESH', 'LATTICE', 'CURVE', 'SURFACE'))

    def draw(self, context):
        layout = self.layout

        ob = context.object
        key = ob.data.shape_keys
        if key and len(key.keys):
            # this is so that we get the active shapekey from the
            # shapekeys block, not from object data
            kb = key.keys[ob.active_shape_key.name]
        else:
            kb = None
        wide_ui = context.region.width > narrowui

        enable_edit = ob.mode != 'EDIT'
        enable_edit_value = False

        if ob.shape_key_lock is False:
            if enable_edit or (ob.type == 'MESH' and ob.shape_key_edit_mode):
                enable_edit_value = True

        row = layout.row()

        rows = 2
        if kb:
            rows = 5
        row.template_list(key, "keys", ob, "active_shape_key_index", rows=rows)

        col = row.column()

        sub = col.column(align=True)
        sub.operator("object.shape_key_add", icon='ZOOMIN', text="")
        sub.operator("object.shape_key_remove", icon='ZOOMOUT', text="")

        if kb:
            col.separator()

            sub = col.column(align=True)
            sub.operator("object.shape_key_move", icon='TRIA_UP', text="").type = 'UP'
            sub.operator("object.shape_key_move", icon='TRIA_DOWN', text="").type = 'DOWN'

            split = layout.split(percentage=0.4)
            row = split.row()
            row.enabled = enable_edit
            if wide_ui:
                row.prop(key, "relative")

            row = split.row()
            row.alignment = 'RIGHT'

            if not wide_ui:
                layout.prop(key, "relative")
                row = layout.row()


            sub = row.row(align=True)
            subsub = sub.row(align=True)
            subsub.active = enable_edit_value
            subsub.prop(ob, "shape_key_lock", text="")
            subsub.prop(kb, "mute", text="")
            sub.prop(ob, "shape_key_edit_mode", text="")

            sub = row.row(align=True)
            sub.operator("object.shape_key_transfer", icon='COPY_ID', text="") # icon is not ideal
            sub.operator("object.shape_key_mirror", icon='ARROW_LEFTRIGHT', text="")
            sub.operator("object.shape_key_clear", icon='X', text="")


            row = layout.row()
            row.prop(kb, "name")

            if key.relative:
                if ob.active_shape_key_index != 0:
                    row = layout.row()
                    row.active = enable_edit_value
                    row.prop(kb, "value")

                    split = layout.split()

                    col = split.column(align=True)
                    col.active = enable_edit_value
                    col.label(text="Range:")
                    col.prop(kb, "slider_min", text="Min")
                    col.prop(kb, "slider_max", text="Max")

                    if wide_ui:
                        col = split.column(align=True)
                    col.active = enable_edit_value
                    col.label(text="Blend:")
                    col.prop_object(kb, "vertex_group", ob, "vertex_groups", text="")
                    col.prop_object(kb, "relative_key", key, "keys", text="")

            else:
                row = layout.row()
                row.active = enable_edit_value
                row.prop(key, "slurph")


class DATA_PT_uv_texture(DataButtonsPanel):
    bl_label = "UV Texture"

    def draw(self, context):
        layout = self.layout

        me = context.mesh

        row = layout.row()
        col = row.column()

        col.template_list(me, "uv_textures", me, "active_uv_texture_index", rows=2)

        col = row.column(align=True)
        col.operator("mesh.uv_texture_add", icon='ZOOMIN', text="")
        col.operator("mesh.uv_texture_remove", icon='ZOOMOUT', text="")

        lay = me.active_uv_texture
        if lay:
            layout.prop(lay, "name")


class DATA_PT_vertex_colors(DataButtonsPanel):
    bl_label = "Vertex Colors"

    def draw(self, context):
        layout = self.layout

        me = context.mesh

        row = layout.row()
        col = row.column()

        col.template_list(me, "vertex_colors", me, "active_vertex_color_index", rows=2)

        col = row.column(align=True)
        col.operator("mesh.vertex_color_add", icon='ZOOMIN', text="")
        col.operator("mesh.vertex_color_remove", icon='ZOOMOUT', text="")

        lay = me.active_vertex_color
        if lay:
            layout.prop(lay, "name")

bpy.types.register(DATA_PT_context_mesh)
bpy.types.register(DATA_PT_normals)
bpy.types.register(DATA_PT_settings)
bpy.types.register(DATA_PT_vertex_groups)
bpy.types.register(DATA_PT_shape_keys)
bpy.types.register(DATA_PT_uv_texture)
bpy.types.register(DATA_PT_vertex_colors)

bpy.types.register(DATA_PT_custom_props_mesh)
