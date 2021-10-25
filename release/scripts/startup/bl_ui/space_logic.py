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
        is_font = (ob.type == 'FONT')

        if is_font:
            prop_index = game.properties.find("Text")
            if prop_index != -1:
                layout.operator("object.game_property_remove", text="Remove Text Game Property", icon='X').index = prop_index
                row = layout.row()
                sub = row.row()
                sub.enabled = 0
                prop = game.properties[prop_index]
                sub.prop(prop, "name", text="")
                row.prop(prop, "type", text="")
                # get the property from the body, not the game property
                # note, don't do this - it's too slow and body can potentially be a really long string.
                #~ row.prop(ob.data, "body", text="")
                row.label("See Text Object")
            else:
                props = layout.operator("object.game_property_new", text="Add Text Game Property", icon='ZOOMIN')
                props.name = "Text"
                props.type = 'STRING'

        props = layout.operator("object.game_property_new", text="Add Game Property", icon='ZOOMIN')
        props.name = ""

        for i, prop in enumerate(game.properties):

            if is_font and i == prop_index:
                continue

            box = layout.box()
            row = box.row()
            row.prop(prop, "name", text="")
            row.prop(prop, "type", text="")
            row.prop(prop, "value", text="")
            row.prop(prop, "show_debug", text="", toggle=True, icon='INFO')
            sub = row.row(align=True)
            props = sub.operator("object.game_property_move", text="", icon='TRIA_UP')
            props.index = i
            props.direction = 'UP'
            props = sub.operator("object.game_property_move", text="", icon='TRIA_DOWN')
            props.index = i
            props.direction = 'DOWN'
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

        LOGIC_MT_editor_menus.draw_collapsible(context, layout)


class LOGIC_MT_editor_menus(Menu):
    bl_idname = "LOGIC_MT_editor_menus"
    bl_label = ""

    def draw(self, context):
        self.draw_menus(self.layout, context)

    @staticmethod
    def draw_menus(layout, context):
        layout.menu("LOGIC_MT_view")
        layout.menu("LOGIC_MT_logicbricks_add")


class LOGIC_MT_view(Menu):
    bl_label = "View"

    def draw(self, context):
        layout = self.layout

        layout.operator("logic.properties", icon='MENU_PANEL')

        layout.separator()

        layout.operator("screen.area_dupli")
        layout.operator("screen.screen_full_area")
        layout.operator("screen.screen_full_area", text="Toggle Fullscreen Area").use_hide_panels = True


classes = (
    LOGIC_PT_properties,
    LOGIC_MT_logicbricks_add,
    LOGIC_HT_header,
    LOGIC_MT_editor_menus,
    LOGIC_MT_view,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
