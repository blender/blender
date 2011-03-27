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


class MESH_MT_vertex_group_specials(bpy.types.Menu):
    bl_label = "Vertex Group Specials"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    def draw(self, context):
        layout = self.layout

        layout.operator("object.vertex_group_sort", icon='SORTALPHA')
        layout.operator("object.vertex_group_copy", icon='COPY_ID')
        layout.operator("object.vertex_group_copy_to_linked", icon='LINK_AREA')
        layout.operator("object.vertex_group_copy_to_selected", icon='LINK_AREA')
        layout.operator("object.vertex_group_mirror", icon='ARROW_LEFTRIGHT')
        layout.operator("object.vertex_group_remove", icon='X', text="Delete All").all = True


class MESH_MT_shape_key_specials(bpy.types.Menu):
    bl_label = "Shape Key Specials"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    def draw(self, context):
        layout = self.layout

        layout.operator("object.shape_key_transfer", icon='COPY_ID')  # icon is not ideal
        layout.operator("object.join_shapes", icon='COPY_ID')  # icon is not ideal
        layout.operator("object.shape_key_mirror", icon='ARROW_LEFTRIGHT')


class MeshButtonsPanel():
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "data"

    @classmethod
    def poll(cls, context):
        engine = context.scene.render.engine
        return context.mesh and (engine in cls.COMPAT_ENGINES)


class DATA_PT_context_mesh(MeshButtonsPanel, bpy.types.Panel):
    bl_label = ""
    bl_options = {'HIDE_HEADER'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    def draw(self, context):
        layout = self.layout

        ob = context.object
        mesh = context.mesh
        space = context.space_data

        if ob:
            layout.template_ID(ob, "data")
        elif mesh:
            layout.template_ID(space, "pin_id")


class DATA_PT_normals(MeshButtonsPanel, bpy.types.Panel):
    bl_label = "Normals"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    def draw(self, context):
        layout = self.layout

        mesh = context.mesh

        split = layout.split()

        col = split.column()
        col.prop(mesh, "use_auto_smooth")
        sub = col.column()
        sub.active = mesh.use_auto_smooth
        sub.prop(mesh, "auto_smooth_angle", text="Angle")

        split.prop(mesh, "show_double_sided")


class DATA_PT_settings(MeshButtonsPanel, bpy.types.Panel):
    bl_label = "Settings"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    def draw(self, context):
        layout = self.layout

        mesh = context.mesh

        layout.prop(mesh, "texture_mesh")
        layout.prop(mesh, "use_auto_texspace")


class DATA_PT_vertex_groups(MeshButtonsPanel, bpy.types.Panel):
    bl_label = "Vertex Groups"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    @classmethod
    def poll(cls, context):
        engine = context.scene.render.engine
        obj = context.object
        return (obj and obj.type in {'MESH', 'LATTICE'} and (engine in cls.COMPAT_ENGINES))

    def draw(self, context):
        layout = self.layout

        ob = context.object
        group = ob.vertex_groups.active

        rows = 2
        if group:
            rows = 5

        row = layout.row()
        row.template_list(ob, "vertex_groups", ob.vertex_groups, "active_index", rows=rows)

        col = row.column(align=True)
        col.operator("object.vertex_group_add", icon='ZOOMIN', text="")
        col.operator("object.vertex_group_remove", icon='ZOOMOUT', text="")
        col.menu("MESH_MT_vertex_group_specials", icon='DOWNARROW_HLT', text="")
        if group:
            col.operator("object.vertex_group_move", icon='TRIA_UP', text="").direction = 'UP'
            col.operator("object.vertex_group_move", icon='TRIA_DOWN', text="").direction = 'DOWN'

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


class DATA_PT_shape_keys(MeshButtonsPanel, bpy.types.Panel):
    bl_label = "Shape Keys"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    @classmethod
    def poll(cls, context):
        engine = context.scene.render.engine
        obj = context.object
        return (obj and obj.type in {'MESH', 'LATTICE', 'CURVE', 'SURFACE'} and (engine in cls.COMPAT_ENGINES))

    def draw(self, context):
        layout = self.layout

        ob = context.object
        key = ob.data.shape_keys
        kb = ob.active_shape_key

        enable_edit = ob.mode != 'EDIT'
        enable_edit_value = False

        if ob.show_only_shape_key is False:
            if enable_edit or (ob.type == 'MESH' and ob.use_shape_key_edit_mode):
                enable_edit_value = True

        row = layout.row()

        rows = 2
        if kb:
            rows = 5
        row.template_list(key, "key_blocks", ob, "active_shape_key_index", rows=rows)

        col = row.column()

        sub = col.column(align=True)
        sub.operator("object.shape_key_add", icon='ZOOMIN', text="")
        sub.operator("object.shape_key_remove", icon='ZOOMOUT', text="")
        sub.menu("MESH_MT_shape_key_specials", icon='DOWNARROW_HLT', text="")

        if kb:
            col.separator()

            sub = col.column(align=True)
            sub.operator("object.shape_key_move", icon='TRIA_UP', text="").type = 'UP'
            sub.operator("object.shape_key_move", icon='TRIA_DOWN', text="").type = 'DOWN'

            split = layout.split(percentage=0.4)
            row = split.row()
            row.enabled = enable_edit
            row.prop(key, "use_relative")

            row = split.row()
            row.alignment = 'RIGHT'

            sub = row.row(align=True)
            subsub = sub.row(align=True)
            subsub.active = enable_edit_value
            subsub.prop(ob, "show_only_shape_key", text="")
            subsub.prop(kb, "mute", text="")
            sub.prop(ob, "use_shape_key_edit_mode", text="")

            sub = row.row()
            sub.operator("object.shape_key_clear", icon='X', text="")

            row = layout.row()
            row.prop(kb, "name")

            if key.use_relative:
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

                    col = split.column(align=True)
                    col.active = enable_edit_value
                    col.label(text="Blend:")
                    col.prop_search(kb, "vertex_group", ob, "vertex_groups", text="")
                    col.prop_search(kb, "relative_key", key, "key_blocks", text="")

            else:
                row = layout.row()
                row.active = enable_edit_value
                row.prop(key, "slurph")


class DATA_PT_uv_texture(MeshButtonsPanel, bpy.types.Panel):
    bl_label = "UV Texture"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    def draw(self, context):
        layout = self.layout

        me = context.mesh

        row = layout.row()
        col = row.column()

        col.template_list(me, "uv_textures", me.uv_textures, "active_index", rows=2)

        col = row.column(align=True)
        col.operator("mesh.uv_texture_add", icon='ZOOMIN', text="")
        col.operator("mesh.uv_texture_remove", icon='ZOOMOUT', text="")

        lay = me.uv_textures.active
        if lay:
            layout.prop(lay, "name")


class DATA_PT_texface(MeshButtonsPanel, bpy.types.Panel):
    bl_label = "Texture Face"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    @classmethod
    def poll(cls, context):
        ob = context.active_object

        return (context.mode == 'EDIT_MESH') and ob and ob.type == 'MESH'

    def draw(self, context):
        layout = self.layout
        col = layout.column()

        me = context.mesh

        tf = me.faces.active_tface

        if tf:
            if context.scene.render.engine != 'BLENDER_GAME':
                col.label(text="Options only supported in Game Engine")

            split = layout.split()
            col = split.column()

            col.prop(tf, "use_image")
            col.prop(tf, "use_light")
            col.prop(tf, "hide")
            col.prop(tf, "use_collision")

            col.prop(tf, "use_blend_shared")
            col.prop(tf, "use_twoside")
            col.prop(tf, "use_object_color")

            col = split.column()

            col.prop(tf, "use_halo")
            col.prop(tf, "use_billboard")
            col.prop(tf, "use_shadow_cast")
            col.prop(tf, "use_bitmap_text")
            col.prop(tf, "use_alpha_sort")

            col = layout.column()
            col.prop(tf, "blend_type")
        else:
            col.label(text="No UV Texture")


class DATA_PT_vertex_colors(MeshButtonsPanel, bpy.types.Panel):
    bl_label = "Vertex Colors"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    def draw(self, context):
        layout = self.layout

        me = context.mesh

        row = layout.row()
        col = row.column()

        col.template_list(me, "vertex_colors", me.vertex_colors, "active_index", rows=2)

        col = row.column(align=True)
        col.operator("mesh.vertex_color_add", icon='ZOOMIN', text="")
        col.operator("mesh.vertex_color_remove", icon='ZOOMOUT', text="")

        lay = me.vertex_colors.active
        if lay:
            layout.prop(lay, "name")


class DATA_PT_custom_props_mesh(MeshButtonsPanel, PropertyPanel, bpy.types.Panel):
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}
    _context_path = "object.data"
    _property_type = bpy.types.Mesh
