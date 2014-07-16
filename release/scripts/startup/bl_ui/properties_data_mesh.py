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
from bpy.types import Menu, Panel, UIList
from rna_prop_ui import PropertyPanel


class MESH_MT_vertex_group_specials(Menu):
    bl_label = "Vertex Group Specials"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    def draw(self, context):
        layout = self.layout

        layout.operator("object.vertex_group_sort", icon='SORTALPHA').sort_type = "NAME"
        layout.operator("object.vertex_group_sort", icon='ARMATURE_DATA', text="Sort by Bone Hierarchy").sort_type = "BONE_HIERARCHY"
        layout.operator("object.vertex_group_copy", icon='COPY_ID')
        layout.operator("object.vertex_group_copy_to_linked", icon='LINK_AREA')
        layout.operator("object.vertex_group_copy_to_selected", icon='LINK_AREA')
        layout.operator("object.vertex_group_mirror", icon='ARROW_LEFTRIGHT').use_topology = False
        layout.operator("object.vertex_group_mirror", text="Mirror Vertex Group (Topology)", icon='ARROW_LEFTRIGHT').use_topology = True
        layout.operator("object.vertex_group_remove_from", icon='X', text="Remove from All Groups").use_all_groups = True
        layout.operator("object.vertex_group_remove_from", icon='X', text="Clear Active Group").use_all_verts = True
        layout.operator("object.vertex_group_remove", icon='X', text="Delete All Groups").all = True
        layout.separator()
        layout.operator("object.vertex_group_lock", icon='LOCKED', text="Lock All").action = 'LOCK'
        layout.operator("object.vertex_group_lock", icon='UNLOCKED', text="UnLock All").action = 'UNLOCK'
        layout.operator("object.vertex_group_lock", icon='LOCKED', text="Lock Invert All").action = 'INVERT'


class MESH_MT_shape_key_specials(Menu):
    bl_label = "Shape Key Specials"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    def draw(self, context):
        layout = self.layout

        layout.operator("object.shape_key_transfer", icon='COPY_ID')  # icon is not ideal
        layout.operator("object.join_shapes", icon='COPY_ID')  # icon is not ideal
        layout.operator("object.shape_key_mirror", icon='ARROW_LEFTRIGHT').use_topology = False
        layout.operator("object.shape_key_mirror", text="Mirror Shape Key (Topology)", icon='ARROW_LEFTRIGHT').use_topology = True
        layout.operator("object.shape_key_add", icon='ZOOMIN', text="New Shape From Mix").from_mix = True
        layout.operator("object.shape_key_remove", icon='X', text="Delete All Shapes").all = True


class MESH_UL_vgroups(UIList):
    def draw_item(self, context, layout, data, item, icon, active_data, active_propname, index):
        # assert(isinstance(item, bpy.types.VertexGroup))
        vgroup = item
        if self.layout_type in {'DEFAULT', 'COMPACT'}:
            layout.prop(vgroup, "name", text="", emboss=False, icon_value=icon)
            icon = 'LOCKED' if vgroup.lock_weight else 'UNLOCKED'
            layout.prop(vgroup, "lock_weight", text="", icon=icon, emboss=False)
        elif self.layout_type in {'GRID'}:
            layout.alignment = 'CENTER'
            layout.label(text="", icon_value=icon)


class MESH_UL_shape_keys(UIList):
    def draw_item(self, context, layout, data, item, icon, active_data, active_propname, index):
        # assert(isinstance(item, bpy.types.ShapeKey))
        obj = active_data
        # key = data
        key_block = item
        if self.layout_type in {'DEFAULT', 'COMPACT'}:
            split = layout.split(0.66, False)
            split.prop(key_block, "name", text="", emboss=False, icon_value=icon)
            row = split.row(align=True)
            if key_block.mute or (obj.mode == 'EDIT' and not (obj.use_shape_key_edit_mode and obj.type == 'MESH')):
                row.active = False
            if not item.id_data.use_relative:
                row.prop(key_block, "frame", text="", emboss=False)
            elif index > 0:
                row.prop(key_block, "value", text="", emboss=False)
            else:
                row.label(text="")
            row.prop(key_block, "mute", text="", emboss=False)
        elif self.layout_type in {'GRID'}:
            layout.alignment = 'CENTER'
            layout.label(text="", icon_value=icon)


class MESH_UL_uvmaps_vcols(UIList):
    def draw_item(self, context, layout, data, item, icon, active_data, active_propname, index):
        # assert(isinstance(item, (bpy.types.MeshTexturePolyLayer, bpy.types.MeshLoopColorLayer)))
        if self.layout_type in {'DEFAULT', 'COMPACT'}:
            layout.prop(item, "name", text="", emboss=False, icon_value=icon)
            icon = 'RESTRICT_RENDER_OFF' if item.active_render else 'RESTRICT_RENDER_ON'
            layout.prop(item, "active_render", text="", icon=icon, emboss=False)
        elif self.layout_type in {'GRID'}:
            layout.alignment = 'CENTER'
            layout.label(text="", icon_value=icon)


class MeshButtonsPanel():
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "data"

    @classmethod
    def poll(cls, context):
        engine = context.scene.render.engine
        return context.mesh and (engine in cls.COMPAT_ENGINES)


class DATA_PT_context_mesh(MeshButtonsPanel, Panel):
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


class DATA_PT_normals(MeshButtonsPanel, Panel):
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


class DATA_PT_texture_space(MeshButtonsPanel, Panel):
    bl_label = "Texture Space"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    def draw(self, context):
        layout = self.layout

        mesh = context.mesh

        layout.prop(mesh, "texture_mesh")

        layout.separator()

        layout.prop(mesh, "use_auto_texspace")
        row = layout.row()
        row.column().prop(mesh, "texspace_location", text="Location")
        row.column().prop(mesh, "texspace_size", text="Size")


class DATA_PT_vertex_groups(MeshButtonsPanel, Panel):
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
            rows = 4

        row = layout.row()
        row.template_list("MESH_UL_vgroups", "", ob, "vertex_groups", ob.vertex_groups, "active_index", rows=rows)

        col = row.column(align=True)
        col.operator("object.vertex_group_add", icon='ZOOMIN', text="")
        col.operator("object.vertex_group_remove", icon='ZOOMOUT', text="").all = False
        col.menu("MESH_MT_vertex_group_specials", icon='DOWNARROW_HLT', text="")
        if group:
            col.separator()
            col.operator("object.vertex_group_move", icon='TRIA_UP', text="").direction = 'UP'
            col.operator("object.vertex_group_move", icon='TRIA_DOWN', text="").direction = 'DOWN'

        if ob.vertex_groups and (ob.mode == 'EDIT' or (ob.mode == 'WEIGHT_PAINT' and ob.type == 'MESH' and ob.data.use_paint_mask_vertex)):
            row = layout.row()

            sub = row.row(align=True)
            sub.operator("object.vertex_group_assign", text="Assign")
            sub.operator("object.vertex_group_remove_from", text="Remove")

            sub = row.row(align=True)
            sub.operator("object.vertex_group_select", text="Select")
            sub.operator("object.vertex_group_deselect", text="Deselect")

            layout.prop(context.tool_settings, "vertex_group_weight", text="Weight")


class DATA_PT_shape_keys(MeshButtonsPanel, Panel):
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
            rows = 4
        row.template_list("MESH_UL_shape_keys", "", key, "key_blocks", ob, "active_shape_key_index", rows=rows)

        col = row.column()

        sub = col.column(align=True)
        sub.operator("object.shape_key_add", icon='ZOOMIN', text="").from_mix = False
        sub.operator("object.shape_key_remove", icon='ZOOMOUT', text="").all = False
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
            sub.label()  # XXX, for alignment only
            subsub = sub.row(align=True)
            subsub.active = enable_edit_value
            subsub.prop(ob, "show_only_shape_key", text="")
            sub.prop(ob, "use_shape_key_edit_mode", text="")

            sub = row.row()
            if key.use_relative:
                sub.operator("object.shape_key_clear", icon='X', text="")
            else:
                sub.operator("object.shape_key_retime", icon='RECOVER_LAST', text="")

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
                layout.prop(kb, "interpolation")
                row = layout.column()
                row.active = enable_edit_value
                row.prop(key, "eval_time")
                row.prop(key, "slurph")


class DATA_PT_uv_texture(MeshButtonsPanel, Panel):
    bl_label = "UV Maps"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    def draw(self, context):
        layout = self.layout

        me = context.mesh

        row = layout.row()
        col = row.column()

        col.template_list("MESH_UL_uvmaps_vcols", "uvmaps", me, "uv_textures", me.uv_textures, "active_index", rows=1)

        col = row.column(align=True)
        col.operator("mesh.uv_texture_add", icon='ZOOMIN', text="")
        col.operator("mesh.uv_texture_remove", icon='ZOOMOUT', text="")


class DATA_PT_vertex_colors(MeshButtonsPanel, Panel):
    bl_label = "Vertex Colors"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    def draw(self, context):
        layout = self.layout

        me = context.mesh

        row = layout.row()
        col = row.column()

        col.template_list("MESH_UL_uvmaps_vcols", "vcols", me, "vertex_colors", me.vertex_colors, "active_index", rows=1)

        col = row.column(align=True)
        col.operator("mesh.vertex_color_add", icon='ZOOMIN', text="")
        col.operator("mesh.vertex_color_remove", icon='ZOOMOUT', text="")


class DATA_PT_customdata(MeshButtonsPanel, Panel):
    bl_label = "Geometry Data"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    def draw(self, context):
        layout = self.layout

        obj = context.object
        me = context.mesh
        col = layout.column()

        col.operator("mesh.customdata_clear_mask", icon='X')
        col.operator("mesh.customdata_clear_skin", icon='X')

        col = layout.column()

        col.enabled = (obj.mode != 'EDIT')
        col.prop(me, "use_customdata_vertex_bevel")
        col.prop(me, "use_customdata_edge_bevel")
        col.prop(me, "use_customdata_edge_crease")


class DATA_PT_custom_props_mesh(MeshButtonsPanel, PropertyPanel, Panel):
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}
    _context_path = "object.data"
    _property_type = bpy.types.Mesh


if __name__ == "__main__":  # only for live edit.
    bpy.utils.register_module(__name__)
