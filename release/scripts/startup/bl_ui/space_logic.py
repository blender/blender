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
from bpy.types import Header, Menu, Panel

def get_id_by_name(properties, name):
    """returns ID"""
    for i, prop in enumerate(properties):
            if prop.name == name:
                return i
    return -1

class LOGIC_PT_properties(Panel):
    bl_space_type = 'LOGIC_EDITOR'
    bl_region_type = 'UI'
    bl_label = "Properties"

    @classmethod
    def poll(cls, context):
        ob = context.active_object
        return ob and ob.game

    def draw(self, context):
        layout = self.layout

        ob = context.active_object
        game = ob.game

        if ob.type == 'FONT':
            prop = game.properties.get("Text")
            if prop:
                layout.operator("object.game_property_remove", text="Text Game Property", icon='X').index = get_id_by_name(game.properties, "Text")
                row = layout.row()
                sub=row.row()
                sub.enabled=0
                sub.prop(prop, "name", text="")
                row.prop(prop, "type", text="")
                # get the property from the body, not the game property
                row.prop(ob.data, "body", text="")
            else:
                props=layout.operator("object.game_property_new", text="Text Game Property", icon='ZOOMIN')
                props.name='Text'
                props.type='STRING'

        layout.operator("object.game_property_new", text="Add Game Property", icon='ZOOMIN')

        for i, prop in enumerate(game.properties):

            if ob.type == 'FONT' and prop.name == "Text":
                continue

            box = layout.box()
            row = box.row()
            row.prop(prop, "name", text="")
            row.prop(prop, "type", text="")
            row.prop(prop, "value", text="", toggle=True)  # we don't care about the type. rna will display correctly
            row.prop(prop, "show_debug", text="", toggle=True, icon='INFO')
            row.operator("object.game_property_remove", text="", icon='X', emboss=False).index = i


class LOGIC_MT_logicbricks_add(Menu):
    bl_label = "Add"

    def draw(self, context):
        layout = self.layout

        layout.operator_menu_enum("logic.sensor_add", "type", text="Sensor")
        layout.operator_menu_enum("logic.controller_add", "type", text="Controller")
        layout.operator_menu_enum("logic.actuator_add", "type", text="Actuator")


class LOGIC_HT_header(Header):
    bl_space_type = 'LOGIC_EDITOR'

    def draw(self, context):
        layout = self.layout.row(align=True)

        layout.template_header()

        if context.area.show_menus:
            row = layout.row(align=True)
            row.menu("LOGIC_MT_view")
            row.menu("LOGIC_MT_logicbricks_add")


class LOGIC_MT_view(Menu):
    bl_label = "View"

    def draw(self, context):
        layout = self.layout

        layout.operator("logic.properties", icon='MENU_PANEL')

if __name__ == "__main__":  # only for live edit.
    bpy.utils.register_module(__name__)
