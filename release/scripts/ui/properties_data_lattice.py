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

narrowui = 180


class DataButtonsPanel(bpy.types.Panel):
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "data"

    def poll(self, context):
        return context.lattice


class DATA_PT_context_lattice(DataButtonsPanel):
    bl_label = ""
    bl_show_header = False

    def draw(self, context):
        layout = self.layout

        ob = context.object
        lat = context.lattice
        space = context.space_data
        wide_ui = context.region.width > narrowui

        if wide_ui:
            split = layout.split(percentage=0.65)
            if ob:
                split.template_ID(ob, "data")
                split.separator()
            elif lat:
                split.template_ID(space, "pin_id")
                split.separator()
        else:
            if ob:
                layout.template_ID(ob, "data")
            elif lat:
                layout.template_ID(space, "pin_id")


class DATA_PT_custom_props_lattice(DataButtonsPanel, PropertyPanel):
    _context_path = "object.data"


class DATA_PT_lattice(DataButtonsPanel):
    bl_label = "Lattice"

    def draw(self, context):
        layout = self.layout

        lat = context.lattice
        wide_ui = context.region.width > narrowui

        split = layout.split()
        col = split.column()
        col.prop(lat, "points_u")
        if wide_ui:
            col = split.column()
        col.prop(lat, "interpolation_type_u", text="")

        split = layout.split()
        col = split.column()
        col.prop(lat, "points_v")
        if wide_ui:
            col = split.column()
        col.prop(lat, "interpolation_type_v", text="")

        split = layout.split()
        col = split.column()
        col.prop(lat, "points_w")
        if wide_ui:
            col = split.column()
        col.prop(lat, "interpolation_type_w", text="")

        layout.prop(lat, "outside")


classes = [
    DATA_PT_context_lattice,
    DATA_PT_lattice,

    DATA_PT_custom_props_lattice]


def register():
    register = bpy.types.register
    for cls in classes:
        register(cls)

def unregister():
    unregister = bpy.types.unregister
    for cls in classes:
        unregister(cls)
