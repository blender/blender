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


class LOGIC_PT_properties(bpy.types.Panel):
    bl_space_type = 'LOGIC_EDITOR'
    bl_region_type = 'UI'
    bl_label = "Properties"

    def poll(self, context):
        ob = context.active_object
        return ob and ob.game

    def draw(self, context):
        layout = self.layout

        ob = context.active_object
        game = ob.game

        layout.itemO("object.game_property_new", text="Add Game Property")

        for i, prop in enumerate(game.properties):

            row = layout.row(align=True)
            row.itemR(prop, "name", text="")
            row.itemR(prop, "type", text="")
            row.itemR(prop, "value", text="", toggle=True) # we dont care about the type. rna will display correctly
            row.itemR(prop, "debug", text="", toggle=True, icon='ICON_INFO')
            row.item_intO("object.game_property_remove", "index", i, text="", icon='ICON_X')

bpy.types.register(LOGIC_PT_properties)
