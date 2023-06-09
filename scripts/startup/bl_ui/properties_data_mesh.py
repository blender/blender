# SPDX-License-Identifier: GPL-2.0-or-later
import bpy
from bpy.types import Menu, Panel, UIList
from rna_prop_ui import PropertyPanel

from bpy.app.translations import (
    pgettext_iface as iface_,
    pgettext_tip as tip_,
)


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
        layout.operator("object.vertex_group_copy_to_selected")
        layout.separator()
        layout.operator("object.vertex_group_mirror", icon='ARROW_LEFTRIGHT').use_topology = False
        layout.operator("object.vertex_group_mirror", text="Mirror Vertex Group (Topology)").use_topology = True
        layout.separator()
        layout.operator(
            "object.vertex_group_remove_from",
            icon='X',
            text="Remove from All Groups",
        ).use_all_groups = True
        layout.operator("object.vertex_group_remove_from", text="Clear Active Group").use_all_verts = True
        layout.operator("object.vertex_group_remove", text="Delete All Unlocked Groups").all_unlocked = True
        layout.operator("object.vertex_group_remove", text="Delete All Groups").all = True
        layout.separator()
        props = layout.operator("object.vertex_group_lock", icon='LOCKED', text="Lock All")
        props.action, props.mask = 'LOCK', 'ALL'
        props = layout.operator("object.vertex_group_lock", icon='UNLOCKED', text="Unlock All")
        props.action, props.mask = 'UNLOCK', 'ALL'
        props = layout.operator("object.vertex_group_lock", text="Lock Invert All")
        props.action, props.mask = 'INVERT', 'ALL'


class MESH_MT_shape_key_context_menu(Menu):
    bl_label = "Shape Key Specials"

    def draw(self, _context):
        layout = self.layout

        layout.operator("object.shape_key_add", icon='ADD', text="New Shape from Mix").from_mix = True
        layout.separator()
        layout.operator("object.shape_key_mirror", icon='ARROW_LEFTRIGHT').use_topology = False
        layout.operator("object.shape_key_mirror", text="Mirror Shape Key (Topology)").use_topology = True
        layout.separator()
        layout.operator("object.join_shapes")
        layout.operator("object.shape_key_transfer")
        layout.separator()
        props = layout.operator("object.shape_key_remove", icon='X', text="Delete All Shape Keys")
        props.all = True
        props.apply_mix = False
        props = layout.operator("object.shape_key_remove", text="Apply All Shape Keys")
        props.all = True
        props.apply_mix = True
        layout.separator()
        layout.operator("object.shape_key_move", icon='TRIA_UP_BAR', text="Move to Top").type = 'TOP'
        layout.operator("object.shape_key_move", icon='TRIA_DOWN_BAR', text="Move to Bottom").type = 'BOTTOM'


class MESH_MT_color_attribute_context_menu(Menu):
    bl_label = "Color Attribute Specials"

    def draw(self, _context):
        layout = self.layout

        layout.operator(
            "geometry.color_attribute_duplicate",
            icon='DUPLICATE',
        )
        layout.operator("geometry.color_attribute_convert")


class MESH_MT_attribute_context_menu(Menu):
    bl_label = "Attribute Specials"

    def draw(self, _context):
        layout = self.layout

        layout.operator("geometry.attribute_convert")


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
            row.emboss = 'NONE_OR_STATUS'
            if key_block.mute or (obj.mode == 'EDIT' and not (obj.use_shape_key_edit_mode and obj.type == 'MESH')):
                row.active = False
            if not item.id_data.use_relative:
                row.prop(key_block, "frame", text="")
            elif index > 0:
                row.prop(key_block, "value", text="")
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
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH', 'BLENDER_WORKBENCH_NEXT'}

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
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH', 'BLENDER_WORKBENCH_NEXT'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        mesh = context.mesh

        col = layout.column(align=False, heading="Auto Smooth")
        col.use_property_decorate = False
        row = col.row(align=True)
        sub = row.row(align=True)
        sub.prop(mesh, "use_auto_smooth", text="")
        sub = sub.row(align=True)
        sub.active = mesh.use_auto_smooth and not mesh.has_custom_normals
        sub.prop(mesh, "auto_smooth_angle", text="")
        row.prop_decorator(mesh, "auto_smooth_angle")


class DATA_PT_texture_space(MeshButtonsPanel, Panel):
    bl_label = "Texture Space"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH', 'BLENDER_WORKBENCH_NEXT'}

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
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH', 'BLENDER_WORKBENCH_NEXT'}

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


class DATA_PT_shape_keys(MeshButtonsPanel, Panel):
    bl_label = "Shape Keys"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH', 'BLENDER_WORKBENCH_NEXT'}

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
        enable_pin = False

        if enable_edit or (ob.use_shape_key_edit_mode and ob.type == 'MESH'):
            enable_pin = True
            if ob.show_only_shape_key is False:
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
            subsub.active = enable_pin
            subsub.prop(ob, "show_only_shape_key", text="")
            sub.prop(ob, "use_shape_key_edit_mode", text="")

            sub = row.row()
            if key.use_relative:
                sub.operator("object.shape_key_clear", icon='X', text="")
            else:
                sub.operator("object.shape_key_retime", icon='RECOVER_LAST', text="")

            layout.use_property_split = True
            if key.use_relative:
                if ob.active_shape_key_index != 0:
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

        layout.prop(ob, "add_rest_position_attribute")


class DATA_PT_uv_texture(MeshButtonsPanel, Panel):
    bl_label = "UV Maps"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH', 'BLENDER_WORKBENCH_NEXT'}

    def draw(self, context):
        layout = self.layout

        me = context.mesh

        row = layout.row()
        col = row.column()

        col.template_list("MESH_UL_uvmaps", "uvmaps", me, "uv_layers", me.uv_layers, "active_index", rows=2)

        col = row.column(align=True)
        col.operator("mesh.uv_texture_add", icon='ADD', text="")
        col.operator("mesh.uv_texture_remove", icon='REMOVE', text="")


class DATA_PT_remesh(MeshButtonsPanel, Panel):
    bl_label = "Remesh"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH', 'BLENDER_WORKBENCH_NEXT'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False
        row = layout.row()

        mesh = context.mesh
        row.prop(mesh, "remesh_mode", text="Mode", expand=True)
        col = layout.column()
        if mesh.remesh_mode == 'VOXEL':
            col.prop(mesh, "remesh_voxel_size")
            col.prop(mesh, "remesh_voxel_adaptivity")
            col.prop(mesh, "use_remesh_fix_poles")

            col = layout.column(heading="Preserve")
            col.prop(mesh, "use_remesh_preserve_volume", text="Volume")
            col.prop(mesh, "use_remesh_preserve_paint_mask", text="Paint Mask")
            col.prop(mesh, "use_remesh_preserve_sculpt_face_sets", text="Face Sets")
            col.prop(mesh, "use_remesh_preserve_vertex_colors", text="Color Attributes")

            col.operator("object.voxel_remesh", text="Voxel Remesh")
        else:
            col.operator("object.quadriflow_remesh", text="QuadriFlow Remesh")


class DATA_PT_customdata(MeshButtonsPanel, Panel):
    bl_label = "Geometry Data"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH', 'BLENDER_WORKBENCH_NEXT'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        me = context.mesh
        col = layout.column()

        col.operator("mesh.customdata_mask_clear", icon='X')
        col.operator("mesh.customdata_skin_clear", icon='X')

        if me.has_custom_normals:
            col.operator("mesh.customdata_custom_splitnormals_clear", icon='X')
        else:
            col.operator("mesh.customdata_custom_splitnormals_add", icon='ADD')

        if me.has_crease_edge:
            col.operator("mesh.customdata_crease_edge_clear", icon='X')
        else:
            col.operator("mesh.customdata_crease_edge_add", icon='ADD')

        if me.has_crease_vertex:
            col.operator("mesh.customdata_crease_vertex_clear", icon='X')
        else:
            col.operator("mesh.customdata_crease_vertex_add", icon='ADD')


class DATA_PT_custom_props_mesh(MeshButtonsPanel, PropertyPanel, Panel):
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH', 'BLENDER_WORKBENCH_NEXT'}
    _context_path = "object.data"
    _property_type = bpy.types.Mesh


class MESH_UL_attributes(UIList):
    display_domain_names = {
        'POINT': "Vertex",
        'EDGE': "Edge",
        'FACE': "Face",
        'CORNER': "Face Corner",
    }

    def filter_items(self, _context, data, property):
        attributes = getattr(data, property)
        flags = []
        indices = [i for i in range(len(attributes))]

        # Filtering by name
        if self.filter_name:
            flags = bpy.types.UI_UL_list.filter_items_by_name(
                self.filter_name, self.bitflag_filter_item, attributes, "name", reverse=self.use_filter_invert)
        if not flags:
            flags = [self.bitflag_filter_item] * len(attributes)

        # Filtering internal attributes
        for idx, item in enumerate(attributes):
            flags[idx] = 0 if item.is_internal else flags[idx]

        return flags, indices

    def draw_item(self, _context, layout, _data, attribute, _icon, _active_data, _active_propname, _index):
        data_type = attribute.bl_rna.properties["data_type"].enum_items[attribute.data_type]

        domain_name = self.display_domain_names.get(attribute.domain, "")

        split = layout.split(factor=0.50)
        split.emboss = 'NONE'
        split.prop(attribute, "name", text="")
        sub = split.row()
        sub.alignment = 'RIGHT'
        sub.active = False
        sub.label(text="%s ▶ %s" % (iface_(domain_name), iface_(data_type.name)),
                  translate=False)


class DATA_PT_mesh_attributes(MeshButtonsPanel, Panel):
    bl_label = "Attributes"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH', 'BLENDER_WORKBENCH_NEXT'}

    def draw(self, context):
        mesh = context.mesh

        layout = self.layout
        row = layout.row()

        col = row.column()
        col.template_list(
            "MESH_UL_attributes",
            "attributes",
            mesh,
            "attributes",
            mesh.attributes,
            "active_index",
            rows=3,
        )

        col = row.column(align=True)
        col.operator("geometry.attribute_add", icon='ADD', text="")
        col.operator("geometry.attribute_remove", icon='REMOVE', text="")

        col.separator()

        col.menu("MESH_MT_attribute_context_menu", icon='DOWNARROW_HLT', text="")

        self.draw_attribute_warnings(context, layout)

    def draw_attribute_warnings(self, context, layout):
        ob = context.object
        mesh = context.mesh

        unique_names = set()
        colliding_names = []
        for collection in (
                # Built-in names.
                {"crease": None},
                mesh.attributes,
                None if ob is None else ob.vertex_groups,
        ):
            if collection is None:
                colliding_names.append("Cannot check for object vertex groups when pinning mesh")
                continue
            for name in collection.keys():
                unique_names_len = len(unique_names)
                unique_names.add(name)
                if len(unique_names) == unique_names_len:
                    colliding_names.append(name)

        if not colliding_names:
            return

        layout.label(text=tip_("Name collisions: ") + ", ".join(set(colliding_names)), icon='ERROR')


class ColorAttributesListBase():
    display_domain_names = {
        'POINT': "Vertex",
        'EDGE': "Edge",
        'FACE': "Face",
        'CORNER': "Face Corner",
    }

    def filter_items(self, _context, data, property):
        attributes = getattr(data, property)
        flags = []
        indices = [i for i in range(len(attributes))]

        # Filtering by name
        if self.filter_name:
            flags = bpy.types.UI_UL_list.filter_items_by_name(
                self.filter_name, self.bitflag_filter_item, attributes, "name", reverse=self.use_filter_invert)
        if not flags:
            flags = [self.bitflag_filter_item] * len(attributes)

        for idx, item in enumerate(attributes):
            skip = (
                (item.domain not in {"POINT", "CORNER"}) or
                (item.data_type not in {"FLOAT_COLOR", "BYTE_COLOR"}) or
                item.is_internal
            )
            flags[idx] = 0 if skip else flags[idx]

        return flags, indices


class MESH_UL_color_attributes(UIList, ColorAttributesListBase):
    def draw_item(self, _context, layout, data, attribute, _icon, _active_data, _active_propname, _index):
        data_type = attribute.bl_rna.properties["data_type"].enum_items[attribute.data_type]

        domain_name = self.display_domain_names.get(attribute.domain, "")

        split = layout.split(factor=0.50)
        split.emboss = 'NONE'
        split.prop(attribute, "name", text="", icon='GROUP_VCOL')

        sub = split.row()
        sub.alignment = 'RIGHT'
        sub.active = False
        sub.label(text="%s ▶ %s" % (iface_(domain_name), iface_(data_type.name)),
                  translate=False)

        active_render = _index == data.color_attributes.render_color_index

        row = layout.row()
        row.emboss = 'NONE'
        props = row.operator(
            "geometry.color_attribute_render_set",
            text="",
            icon='RESTRICT_RENDER_OFF' if active_render else 'RESTRICT_RENDER_ON',
        )
        props.name = attribute.name


class MESH_UL_color_attributes_selector(UIList, ColorAttributesListBase):
    def draw_item(self, _context, layout, _data, attribute, _icon, _active_data, _active_propname, _index):
        layout.emboss = 'NONE'
        layout.prop(attribute, "name", text="", icon='GROUP_VCOL')


class DATA_PT_vertex_colors(DATA_PT_mesh_attributes, Panel):
    bl_label = "Color Attributes"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH', 'BLENDER_WORKBENCH_NEXT'}

    def draw(self, context):
        mesh = context.mesh

        layout = self.layout
        row = layout.row()

        col = row.column()
        col.template_list(
            "MESH_UL_color_attributes",
            "color_attributes",
            mesh,
            "color_attributes",
            mesh.color_attributes,
            "active_color_index",
            rows=3,
        )

        col = row.column(align=True)
        col.operator("geometry.color_attribute_add", icon='ADD', text="")
        col.operator("geometry.color_attribute_remove", icon='REMOVE', text="")

        col.separator()

        col.menu("MESH_MT_color_attribute_context_menu", icon='DOWNARROW_HLT', text="")

        self.draw_attribute_warnings(context, layout)


classes = (
    MESH_MT_vertex_group_context_menu,
    MESH_MT_shape_key_context_menu,
    MESH_MT_color_attribute_context_menu,
    MESH_MT_attribute_context_menu,
    MESH_UL_vgroups,
    MESH_UL_shape_keys,
    MESH_UL_uvmaps,
    MESH_UL_attributes,
    DATA_PT_context_mesh,
    DATA_PT_vertex_groups,
    DATA_PT_shape_keys,
    DATA_PT_uv_texture,
    DATA_PT_vertex_colors,
    DATA_PT_mesh_attributes,
    DATA_PT_normals,
    DATA_PT_texture_space,
    DATA_PT_remesh,
    DATA_PT_customdata,
    DATA_PT_custom_props_mesh,
    MESH_UL_color_attributes,
    MESH_UL_color_attributes_selector,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
