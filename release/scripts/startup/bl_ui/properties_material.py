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
from bpy_extras.node_utils import find_node_input


class MATERIAL_MT_context_menu(Menu):
    bl_label = "Material Specials"

    def draw(self, _context):
        layout = self.layout

        layout.operator("material.copy", icon='COPYDOWN')
        layout.operator("object.material_slot_copy")
        layout.operator("material.paste", icon='PASTEDOWN')


class MATERIAL_UL_matslots(UIList):

    def draw_item(self, _context, layout, _data, item, icon, _active_data, _active_propname, _index):
        # assert(isinstance(item, bpy.types.MaterialSlot)
        # ob = data
        slot = item
        ma = slot.material
        if self.layout_type in {'DEFAULT', 'COMPACT'}:
            if ma:
                layout.prop(ma, "name", text="", emboss=False, icon_value=icon)
            else:
                layout.label(text="", icon_value=icon)
        elif self.layout_type == 'GRID':
            layout.alignment = 'CENTER'
            layout.label(text="", icon_value=icon)


class MaterialButtonsPanel:
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "material"
    # COMPAT_ENGINES must be defined in each subclass, external engines can add themselves here

    @classmethod
    def poll(cls, context):
        mat = context.material
        return mat and (context.engine in cls.COMPAT_ENGINES) and not mat.grease_pencil


class MATERIAL_PT_preview(MaterialButtonsPanel, Panel):
    bl_label = "Preview"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_EEVEE'}

    def draw(self, context):
        self.layout.template_preview(context.material)


class MATERIAL_PT_custom_props(MaterialButtonsPanel, PropertyPanel, Panel):
    COMPAT_ENGINES = {'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}
    _context_path = "material"
    _property_type = bpy.types.Material


class EEVEE_MATERIAL_PT_context_material(MaterialButtonsPanel, Panel):
    bl_label = ""
    bl_context = "material"
    bl_options = {'HIDE_HEADER'}
    COMPAT_ENGINES = {'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

    @classmethod
    def poll(cls, context):
        ob = context.object
        mat = context.material

        if (ob and ob.type == 'GPENCIL') or (mat and mat.grease_pencil):
            return False

        return (ob or mat) and (context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout

        mat = context.material
        ob = context.object
        slot = context.material_slot
        space = context.space_data

        if ob:
            is_sortable = len(ob.material_slots) > 1
            rows = 3
            if (is_sortable):
                rows = 5

            row = layout.row()

            row.template_list("MATERIAL_UL_matslots", "", ob, "material_slots", ob, "active_material_index", rows=rows)

            col = row.column(align=True)
            col.operator("object.material_slot_add", icon='ADD', text="")
            col.operator("object.material_slot_remove", icon='REMOVE', text="")

            col.separator()

            col.menu("MATERIAL_MT_context_menu", icon='DOWNARROW_HLT', text="")

            if is_sortable:
                col.separator()

                col.operator("object.material_slot_move", icon='TRIA_UP', text="").direction = 'UP'
                col.operator("object.material_slot_move", icon='TRIA_DOWN', text="").direction = 'DOWN'

        row = layout.row()

        if ob:
            row.template_ID(ob, "active_material", new="material.new")

            if slot:
                icon_link = 'MESH_DATA' if slot.link == 'DATA' else 'OBJECT_DATA'
                row.prop(slot, "link", icon=icon_link, icon_only=True)

            if ob.mode == 'EDIT':
                row = layout.row(align=True)
                row.operator("object.material_slot_assign", text="Assign")
                row.operator("object.material_slot_select", text="Select")
                row.operator("object.material_slot_deselect", text="Deselect")

        elif mat:
            row.template_ID(space, "pin_id")


def panel_node_draw(layout, ntree, _output_type, input_name):
    node = ntree.get_output_node('EEVEE')

    if node:
        input = find_node_input(node, input_name)
        if input:
            layout.template_node_view(ntree, node, input)
        else:
            layout.label(text="Incompatible output node")
    else:
        layout.label(text="No output node")


class EEVEE_MATERIAL_PT_surface(MaterialButtonsPanel, Panel):
    bl_label = "Surface"
    bl_context = "material"
    COMPAT_ENGINES = {'BLENDER_EEVEE'}

    def draw(self, context):
        layout = self.layout

        mat = context.material

        layout.prop(mat, "use_nodes", icon='NODETREE')
        layout.separator()

        if mat.use_nodes:
            panel_node_draw(layout, mat.node_tree, 'OUTPUT_MATERIAL', "Surface")
        else:
            layout.use_property_split = True
            layout.prop(mat, "diffuse_color", text="Base Color")
            layout.prop(mat, "metallic")
            layout.prop(mat, "specular_intensity", text="Specular")
            layout.prop(mat, "roughness")


class EEVEE_MATERIAL_PT_volume(MaterialButtonsPanel, Panel):
    bl_label = "Volume"
    bl_context = "material"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_EEVEE'}

    @classmethod
    def poll(cls, context):
        engine = context.engine
        mat = context.material
        return mat and mat.use_nodes and (engine in cls.COMPAT_ENGINES) and not mat.grease_pencil

    def draw(self, context):
        layout = self.layout

        mat = context.material

        panel_node_draw(layout, mat.node_tree, 'OUTPUT_MATERIAL', "Volume")


class EEVEE_MATERIAL_PT_settings(MaterialButtonsPanel, Panel):
    bl_label = "Settings"
    bl_context = "material"
    COMPAT_ENGINES = {'BLENDER_EEVEE'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        mat = context.material

        layout.prop(mat, "use_backface_culling")
        layout.prop(mat, "blend_method")
        layout.prop(mat, "shadow_method")

        row = layout.row()
        row.active = ((mat.blend_method == 'CLIP') or (mat.shadow_method == 'CLIP'))
        row.prop(mat, "alpha_threshold")

        if mat.blend_method not in {'OPAQUE', 'CLIP', 'HASHED'}:
            layout.prop(mat, "show_transparent_back")

        layout.prop(mat, "use_screen_refraction")
        layout.prop(mat, "refraction_depth")
        layout.prop(mat, "use_sss_translucency")
        layout.prop(mat, "pass_index")


class MATERIAL_PT_viewport(MaterialButtonsPanel, Panel):
    bl_label = "Viewport Display"
    bl_context = "material"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        mat = context.material
        return mat and not mat.grease_pencil

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        mat = context.material

        col = layout.column()
        col.prop(mat, "diffuse_color", text="Color")
        col.prop(mat, "metallic")
        col.prop(mat, "roughness")


classes = (
    MATERIAL_MT_context_menu,
    MATERIAL_UL_matslots,
    MATERIAL_PT_preview,
    EEVEE_MATERIAL_PT_context_material,
    EEVEE_MATERIAL_PT_surface,
    EEVEE_MATERIAL_PT_volume,
    EEVEE_MATERIAL_PT_settings,
    MATERIAL_PT_viewport,
    MATERIAL_PT_custom_props,
)


if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
