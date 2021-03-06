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


class DataButtonsPanel:
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "data"

    @classmethod
    def poll(cls, context):
        engine = context.scene.render.engine
        return hasattr(context, 'hair') and context.hair and (engine in cls.COMPAT_ENGINES)


class DATA_PT_context_hair(DataButtonsPanel, Panel):
    bl_label = ""
    bl_options = {'HIDE_HEADER'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

    def draw(self, context):
        layout = self.layout

        ob = context.object
        hair = context.hair
        space = context.space_data

        if ob:
            layout.template_ID(ob, "data")
        elif hair:
            layout.template_ID(space, "pin_id")


class HAIR_MT_add_attribute(Menu):
    bl_label = "Add Attribute"

    @staticmethod
    def add_standard_attribute(layout, hair, name, data_type, domain):
        exists = hair.attributes.get(name) is not None

        col = layout.column()
        col.enabled = not exists
        col.operator_context = 'EXEC_DEFAULT'

        props = col.operator("geometry.attribute_add", text=name)
        props.name = name
        props.data_type = data_type
        props.domain = domain

    def draw(self, context):
        layout = self.layout
        hair = context.hair

        self.add_standard_attribute(layout, hair, 'Radius', 'FLOAT', 'POINT')
        self.add_standard_attribute(layout, hair, 'Color', 'FLOAT_COLOR', 'POINT')

        layout.separator()

        layout.operator_context = 'INVOKE_DEFAULT'
        layout.operator("geometry.attribute_add", text="Custom...")


class HAIR_UL_attributes(UIList):
    def draw_item(self, _context, layout, _data, attribute, _icon, _active_data, _active_propname, _index):
        data_type = attribute.bl_rna.properties['data_type'].enum_items[attribute.data_type]
        domain = attribute.bl_rna.properties['domain'].enum_items[attribute.domain]

        split = layout.split(factor=0.5)
        split.emboss = 'NONE'
        row = split.row()
        row.prop(attribute, "name", text="")
        sub = split.split()
        sub.alignment = 'RIGHT'
        sub.active = False
        sub.label(text=domain.name)
        sub.label(text=data_type.name)


class DATA_PT_hair_attributes(DataButtonsPanel, Panel):
    bl_label = "Attributes"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

    def draw(self, context):
        hair = context.hair

        layout = self.layout
        row = layout.row()

        col = row.column()
        col.template_list(
            "HAIR_UL_attributes",
            "attributes",
            hair,
            "attributes",
            hair.attributes,
            "active_index",
            rows=3,
        )

        col = row.column(align=True)
        col.menu("HAIR_MT_add_attribute", icon='ADD', text="")
        col.operator("geometry.attribute_remove", icon='REMOVE', text="")


class DATA_PT_custom_props_hair(DataButtonsPanel, PropertyPanel, Panel):
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}
    _context_path = "object.data"
    _property_type = bpy.types.Hair if hasattr(bpy.types, "Hair") else None


classes = (
    DATA_PT_context_hair,
    DATA_PT_hair_attributes,
    DATA_PT_custom_props_hair,
    HAIR_MT_add_attribute,
    HAIR_UL_attributes,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
