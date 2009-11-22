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
                split.itemS()
            elif mball:
                split.template_ID(space, "pin_id")
                split.itemS()
        else:
            if ob:
                layout.template_ID(ob, "data")
            elif mball:
                layout.template_ID(space, "pin_id")


class DATA_PT_metaball(DataButtonsPanel):
    bl_label = "Metaball"

    def draw(self, context):
        layout = self.layout

        mball = context.meta_ball
        wide_ui = context.region.width > narrowui

        split = layout.split()

        col = split.column()
        col.itemL(text="Resolution:")
        sub = col.column(align=True)
        sub.itemR(mball, "wire_size", text="View")
        sub.itemR(mball, "render_size", text="Render")

        if wide_ui:
            col = split.column()
        col.itemL(text="Settings:")
        col.itemR(mball, "threshold", text="Threshold")

        layout.itemL(text="Update:")
        if wide_ui:
            layout.itemR(mball, "flag", expand=True)
        else:
            layout.itemR(mball, "flag", text="")


class DATA_PT_metaball_element(DataButtonsPanel):
    bl_label = "Active Element"

    def poll(self, context):
        return (context.meta_ball and context.meta_ball.active_element)

    def draw(self, context):
        layout = self.layout

        metaelem = context.meta_ball.active_element
        wide_ui = context.region.width > narrowui

        if wide_ui:
            layout.itemR(metaelem, "type")
        else:
            layout.itemR(metaelem, "type", text="")

        split = layout.split()

        col = split.column(align=True)
        col.itemL(text="Settings:")
        col.itemR(metaelem, "stiffness", text="Stiffness")
        col.itemR(metaelem, "negative", text="Negative")
        col.itemR(metaelem, "hide", text="Hide")

        if wide_ui:
            col = split.column(align=True)

        if metaelem.type in ('CUBE', 'ELLIPSOID'):
            col.itemL(text="Size:")
            col.itemR(metaelem, "size_x", text="X")
            col.itemR(metaelem, "size_y", text="Y")
            col.itemR(metaelem, "size_z", text="Z")

        elif metaelem.type == 'TUBE':
            col.itemL(text="Size:")
            col.itemR(metaelem, "size_x", text="X")

        elif metaelem.type == 'PLANE':
            col.itemL(text="Size:")
            col.itemR(metaelem, "size_x", text="X")
            col.itemR(metaelem, "size_y", text="Y")

bpy.types.register(DATA_PT_context_metaball)
bpy.types.register(DATA_PT_metaball)
bpy.types.register(DATA_PT_metaball_element)
