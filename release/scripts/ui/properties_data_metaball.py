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
        return context.meta_ball


class DATA_PT_context_metaball(DataButtonsPanel):
    bl_label = ""
    bl_show_header = False

    def draw(self, context):
        layout = self.layout

        ob = context.object
        mball = context.meta_ball
        space = context.space_data
        wide_ui = context.region.width > narrowui

        if wide_ui:
            split = layout.split(percentage=0.65)
            if ob:
                split.template_ID(ob, "data")
                split.separator()
            elif mball:
                split.template_ID(space, "pin_id")
                split.separator()
        else:
            if ob:
                layout.template_ID(ob, "data")
            elif mball:
                layout.template_ID(space, "pin_id")


class DATA_PT_custom_props_metaball(DataButtonsPanel, PropertyPanel):
    _context_path = "object.data"


class DATA_PT_metaball(DataButtonsPanel):
    bl_label = "Metaball"

    def draw(self, context):
        layout = self.layout

        mball = context.meta_ball
        wide_ui = context.region.width > narrowui

        split = layout.split()

        col = split.column()
        col.label(text="Resolution:")
        sub = col.column(align=True)
        sub.prop(mball, "wire_size", text="View")
        sub.prop(mball, "render_size", text="Render")

        if wide_ui:
            col = split.column()
        col.label(text="Settings:")
        col.prop(mball, "threshold", text="Threshold")

        layout.label(text="Update:")
        if wide_ui:
            layout.prop(mball, "flag", expand=True)
        else:
            layout.prop(mball, "flag", text="")


class DATA_PT_metaball_element(DataButtonsPanel):
    bl_label = "Active Element"

    def poll(self, context):
        return (context.meta_ball and context.meta_ball.active_element)

    def draw(self, context):
        layout = self.layout

        metaelem = context.meta_ball.active_element
        wide_ui = context.region.width > narrowui

        if wide_ui:
            layout.prop(metaelem, "type")
        else:
            layout.prop(metaelem, "type", text="")

        split = layout.split()

        col = split.column(align=True)
        col.label(text="Settings:")
        col.prop(metaelem, "stiffness", text="Stiffness")
        col.prop(metaelem, "negative", text="Negative")
        col.prop(metaelem, "hide", text="Hide")

        if wide_ui:
            col = split.column(align=True)

        if metaelem.type in ('CUBE', 'ELLIPSOID'):
            col.label(text="Size:")
            col.prop(metaelem, "size_x", text="X")
            col.prop(metaelem, "size_y", text="Y")
            col.prop(metaelem, "size_z", text="Z")

        elif metaelem.type == 'TUBE':
            col.label(text="Size:")
            col.prop(metaelem, "size_x", text="X")

        elif metaelem.type == 'PLANE':
            col.label(text="Size:")
            col.prop(metaelem, "size_x", text="X")
            col.prop(metaelem, "size_y", text="Y")


classes = [
    DATA_PT_context_metaball,
    DATA_PT_metaball,
    DATA_PT_metaball_element,

    DATA_PT_custom_props_metaball]


def register():
    register = bpy.types.register
    for cls in classes:
        register(cls)

def unregister():
    unregister = bpy.types.unregister
    for cls in classes:
        unregister(cls)

if __name__ == "__main__":
    register()

