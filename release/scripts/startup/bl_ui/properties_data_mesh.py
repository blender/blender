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


class MESH_MT_vertex_group_context_menu(Menu):
    bl_label = "Vertex Group Specials"

    def draw(self, _context):
        layout = self.layout

        layout.operator(
            "object.vertex_group_sort",
            icon='SORTALPHA',
            text="Sort by Name",
        ).sort_type = 'NAME'
        layout.operator(
            "object.vertex_group_sort",
            icon='BONE_DATA',
            text="Sort by Bone Hierarchy",
        ).sort_type = 'BONE_HIERARCHY'
        layout.separator()
        layout.operator("object.vertex_group_copy", icon='DUPLICATE')
        layout.operator("object.vertex_group_copy_to_linked")
        layout.operator("object.vertex_group_copy_to_selected")
        layout.separator()
        layout.operator("object.vertex_group_mirror", icon='ARROW_LEFTRIGHT').use_topology = False
        layout.operator("object.vertex_group_mirror", text="Mirror Vertex Group (Topology)").use_topology = True
        layout.separator()
        layout.operator("object.vertex_group_remove_from", icon='X', text="Remove from All Groups").use_all_groups = True
        layout.operator("object.vertex_group_remove_from", text="Clear Active Group").use_all_verts = True
        layout.operator("object.vertex_group_remove", text="Delete All Unlocked Groups").all_unlocked = True
        layout.operator("object.vertex_group_remove", text="Delete All Groups").all = True
        layout.separator()
        layout.operator("object.vertex_group_lock", icon='LOCKED', text="Lock All").action = 'LOCK'
        layout.operator("object.vertex_group_lock", icon='UNLOCKED', text="UnLock All").action = 'UNLOCK'
        layout.operator("object.vertex_group_lock", text="Lock Invert All").action = 'INVERT'


class MESH_MT_shape_key_context_menu(Menu):
    bl_label = "Shape Key Specials"

    def draw(self, _context):
        layout = self.layout

        layout.operator("object.shape_key_add", icon='ADD', text="New Shape From Mix").from_mix = True
        layout.separator()
        layout.operator("object.shape_key_mirror", icon='ARROW_LEFTRIGHT').use_topology = False
        layout.operator("object.shape_key_mirror", text="Mirror Shape Key (Topology)").use_topology = True
        layout.separator()
        layout.operator("object.join_shapes")
        layout.operator("object.shape_key_transfer")
        layout.separator()
        layout.operator("object.shape_key_remove", icon='X', text="Delete All Shape Keys").all = True
        layout.separator()
        layout.operator("object.shape_key_move", icon='TRIA_UP_BAR', text="Move To Top").type = 'TOP'
        layout.operator("object.shape_key_move", icon='TRIA_DOWN_BAR', text="Move To Bottom").type = 'BOTTOM'


class MESH_UL_vgroups(UIList):
    def draw_item(self, _context, layout, _data, item, icon, _active_data_, _active_propname, _index):
        # assert(isinstance(item, bpy.types.VertexGroup))
        vgroup = item
        if self.layout_type in {'DEFAULT', 'COMPACT'}:
            layout.prop(vgroup, "name", text="", emboss=False, icon_value=icon)
            icon = 'LOCKED' if vgroup.lock_weight else 'UNLOCKED'
            layout.prop(vgroup, "lock_weight", text="", icon=icon, emboss=False)
        elif self.layout_type == 'GRID':
            layout.alignment = 'CENTER'
            layout.label(text="", icon_value=icon)


class MESH_UL_fmaps(UIList):
    def draw_item(self, _context, layout, _data, item, icon, _active_data, _active_propname, _index):
        # assert(isinstance(item, bpy.types.FaceMap))
        fmap = item
        if self.layout_type in {'DEFAULT', 'COMPACT'}:
            layout.prop(fmap, "name", text="", emboss=False, icon='FACE_MAPS')
        elif self.layout_type == 'GRID':
            layout.alignment = 'CENTER'
            layout.label(text="", icon_value=icon)


class MESH_UL_shape_keys(UIList):
    def draw_item(self, _context, layout, _data, item, icon, active_data, _active_propname, index):
        # assert(isinstance(item, bpy.types.ShapeKey))
        obj = active_data
        # key = data
        key_block = item
        if self.layout_type in {'DEFAULT', 'COMPACT'}:
            split = layout.split(factor=0.66, align=False)
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
        elif self.layout_type == 'GRID':
            layout.alignment = 'CENTER'
            layout.label(text="", icon_value=icon)


class MESH_UL_uvmaps(UIList):
    def draw_item(self, _context, layout, _data, item, icon, _active_data, _active_propname, _index):
        # assert(isinstance(item, (bpy.types.MeshTexturePolyLayer, bpy.types.MeshLoopColorLayer)))
        if self.layout_type in {'DEFAULT', 'COMPACT'}:
            layout.prop(item, "name", text="", emboss=False, icon='GROUP_UVS')
            icon = 'RESTRICT_RENDER_OFF' if item.active_render else 'RESTRICT_RENDER_ON'
            layout.prop(item, "active_render", text="", icon=icon, emboss=False)
        elif self.layout_type == 'GRID':
            layout.alignment = 'CENTER'
            layout.label(text="", icon_value=icon)


class MESH_UL_vcols(UIList):
    def draw_item(self, _context, layout, _data, item, icon, _active_data, _active_propname, _index):
        # assert(isinstance(item, (bpy.types.MeshTexturePolyLayer, bpy.types.MeshLoopColorLayer)))
        if self.layout_type in {'DEFAULT', 'COMPACT'}:
            layout.prop(item, "name", text="", emboss=False, icon='GROUP_VCOL')
            icon = 'RESTRICT_RENDER_OFF' if item.active_render else 'RESTRICT_RENDER_ON'
            layout.prop(item, "active_render", text="", icon=icon, emboss=False)
        elif self.layout_type == 'GRID':
            layout.alignment = 'CENTER'
            layout.label(text="", icon_value=icon)


class MeshButtonsPanel:
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "data"

    @classmethod
    def poll(cls, context):
        engine = context.engine
        return context.mesh and (engine in cls.COMPAT_ENGINES)


class DATA_PT_context_mesh(MeshButtonsPanel, Panel):
    bl_label = ""
    bl_options = {'HIDE_HEADER'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

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
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

    def draw(self, context):
        pass


class DATA_PT_normals_auto_smooth(MeshButtonsPanel, Panel):
    bl_label = "Auto Smooth"
    bl_parent_id = "DATA_PT_normals"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

    def draw_header(self, context):
        mesh = context.mesh

        self.layout.prop(mesh, "use_auto_smooth", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        mesh = context.mesh

        layout.active = mesh.use_auto_smooth and not mesh.has_custom_normals
        layout.prop(mesh, "auto_smooth_angle", text="Angle")


class DATA_PT_texture_space(MeshButtonsPanel, Panel):
    bl_label = "Texture Space"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        mesh = context.mesh

        layout.prop(mesh, "texture_mesh")

        layout.separator()

        layout.prop(mesh, "use_auto_texspace")

        layout.prop(mesh, "texspace_location", text="Location")
        layout.prop(mesh, "texspace_size", text="Size")


class DATA_PT_vertex_groups(MeshButtonsPanel, Panel):
    bl_label = "Vertex Groups"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

    @classmethod
    def poll(cls, context):
        engine = context.engine
        obj = context.object
        return (obj and obj.type in {'MESH', 'LATTICE'} and (engine in cls.COMPAT_ENGINES))

    def draw(self, context):
        layout = self.layout

        ob = context.object
        group = ob.vertex_groups.active

        rows = 3
        if group:
            rows = 5

        row = layout.row()
        row.template_list("MESH_UL_vgroups", "", ob, "vertex_groups", ob.vertex_groups, "active_index", rows=rows)

        col = row.column(align=True)

        col.operator("object.vertex_group_add", icon='ADD', text="")
        props = col.operator("object.vertex_group_remove", icon='REMOVE', text="")
        props.all_unlocked = props.all = False

        col.separator()

        col.menu("MESH_MT_vertex_group_context_menu", icon='DOWNARROW_HLT', text="")

        if group:
            col.separator()
            col.operator("object.vertex_group_move", icon='TRIA_UP', text="").direction = 'UP'
            col.operator("object.vertex_group_move", icon='TRIA_DOWN', text="").direction = 'DOWN'

        if (
                ob.vertex_groups and
                (ob.mode == 'EDIT' or
                 (ob.mode == 'WEIGHT_PAINT' and ob.type == 'MESH' and ob.data.use_paint_mask_vertex))
        ):
            row = layout.row()

            sub = row.row(align=True)
            sub.operator("object.vertex_group_assign", text="Assign")
            sub.operator("object.vertex_group_remove_from", text="Remove")

            sub = row.row(align=True)
            sub.operator("object.vertex_group_select", text="Select")
            sub.operator("object.vertex_group_deselect", text="Deselect")

            layout.prop(context.tool_settings, "vertex_group_weight", text="Weight")


class DATA_PT_face_maps(MeshButtonsPanel, Panel):
    bl_label = "Face Maps"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

    @classmethod
    def poll(cls, context):
        obj = context.object
        return (obj and obj.type == 'MESH')

    def draw(self, context):
        layout = self.layout

        ob = context.object
        facemap = ob.face_maps.active

        rows = 2
        if facemap:
            rows = 4

        row = layout.row()
        row.template_list("MESH_UL_fmaps", "", ob, "face_maps", ob.face_maps, "active_index", rows=rows)

        col = row.column(align=True)
        col.operator("object.face_map_add", icon='ADD', text="")
        col.operator("object.face_map_remove", icon='REMOVE', text="")

        if facemap:
            col.separator()
            col.operator("object.face_map_move", icon='TRIA_UP', text="").direction = 'UP'
            col.operator("object.face_map_move", icon='TRIA_DOWN', text="").direction = 'DOWN'

        if ob.face_maps and (ob.mode == 'EDIT' and ob.type == 'MESH'):
            row = layout.row()

            sub = row.row(align=True)
            sub.operator("object.face_map_assign", text="Assign")
            sub.operator("object.face_map_remove_from", text="Remove")

            sub = row.row(align=True)
            sub.operator("object.face_map_select", text="Select")
            sub.operator("object.face_map_deselect", text="Deselect")


class DATA_PT_shape_keys(MeshButtonsPanel, Panel):
    bl_label = "Shape Keys"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

    @classmethod
    def poll(cls, context):
        engine = context.engine
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

        rows = 3
        if kb:
            rows = 5

        row.template_list("MESH_UL_shape_keys", "", key, "key_blocks", ob, "active_shape_key_index", rows=rows)

        col = row.column(align=True)

        col.operator("object.shape_key_add", icon='ADD', text="").from_mix = False
        col.operator("object.shape_key_remove", icon='REMOVE', text="").all = False

        col.separator()

        col.menu("MESH_MT_shape_key_context_menu", icon='DOWNARROW_HLT', text="")

        if kb:
            col.separator()

            sub = col.column(align=True)
            sub.operator("object.shape_key_move", icon='TRIA_UP', text="").type = 'UP'
            sub.operator("object.shape_key_move", icon='TRIA_DOWN', text="").type = 'DOWN'

            split = layout.split(factor=0.4)
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
                    layout.use_property_split = True

                    row = layout.row()
                    row.active = enable_edit_value
                    row.prop(kb, "value")

                    col = layout.column()
                    sub.active = enable_edit_value
                    sub = col.column(align=True)
                    sub.prop(kb, "slider_min", text="Range Min")
                    sub.prop(kb, "slider_max", text="Max")

                    col.prop_search(kb, "vertex_group", ob, "vertex_groups", text="Vertex Group")
                    col.prop_search(kb, "relative_key", key, "key_blocks", text="Relative To")

            else:
                layout.prop(kb, "interpolation")
                row = layout.column()
                row.active = enable_edit_value
                row.prop(key, "eval_time")


class DATA_PT_uv_texture(MeshButtonsPanel, Panel):
    bl_label = "UV Maps"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

    def draw(self, context):
        layout = self.layout

        me = context.mesh

        row = layout.row()
        col = row.column()

        col.template_list("MESH_UL_uvmaps", "uvmaps", me, "uv_layers", me.uv_layers, "active_index", rows=2)

        col = row.column(align=True)
        col.operator("mesh.uv_texture_add", icon='ADD', text="")
        col.operator("mesh.uv_texture_remove", icon='REMOVE', text="")


class DATA_PT_vertex_colors(MeshButtonsPanel, Panel):
    bl_label = "Vertex Colors"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

    def draw(self, context):
        layout = self.layout

        me = context.mesh

        row = layout.row()
        col = row.column()

        col.template_list("MESH_UL_vcols", "vcols", me, "vertex_colors", me.vertex_colors, "active_index", rows=2)

        col = row.column(align=True)
        col.operator("mesh.vertex_color_add", icon='ADD', text="")
        col.operator("mesh.vertex_color_remove", icon='REMOVE', text="")


class DATA_PT_customdata(MeshButtonsPanel, Panel):
    bl_label = "Geometry Data"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        obj = context.object
        me = context.mesh
        col = layout.column()

        col.operator("mesh.customdata_mask_clear", icon='X')
        col.operator("mesh.customdata_skin_clear", icon='X')

        if me.has_custom_normals:
            col.operator("mesh.customdata_custom_splitnormals_clear", icon='X')
        else:
            col.operator("mesh.customdata_custom_splitnormals_add", icon='ADD')

        col = layout.column()

        col.enabled = (obj.mode != 'EDIT')
        col.prop(me, "use_customdata_vertex_bevel")
        col.prop(me, "use_customdata_edge_bevel")
        col.prop(me, "use_customdata_edge_crease")


class DATA_PT_custom_props_mesh(MeshButtonsPanel, PropertyPanel, Panel):
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}
    _context_path = "object.data"
    _property_type = bpy.types.Mesh


classes = (
    MESH_MT_vertex_group_context_menu,
    MESH_MT_shape_key_context_menu,
    MESH_UL_vgroups,
    MESH_UL_fmaps,
    MESH_UL_shape_keys,
    MESH_UL_uvmaps,
    MESH_UL_vcols,
    DATA_PT_context_mesh,
    DATA_PT_vertex_groups,
    DATA_PT_shape_keys,
    DATA_PT_uv_texture,
    DATA_PT_vertex_colors,
    DATA_PT_face_maps,
    DATA_PT_normals,
    DATA_PT_normals_auto_smooth,
    DATA_PT_texture_space,
    DATA_PT_customdata,
    DATA_PT_custom_props_mesh,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
