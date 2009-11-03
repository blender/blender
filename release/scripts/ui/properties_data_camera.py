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


class DataButtonsPanel(bpy.types.Panel):
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "data"

    def poll(self, context):
        return context.camera


class DATA_PT_context_camera(DataButtonsPanel):
    bl_label = ""
    bl_show_header = False

    def draw(self, context):
        layout = self.layout

        ob = context.object
        cam = context.camera
        space = context.space_data

        split = layout.split(percentage=0.65)

        if ob:
            split.template_ID(ob, "data")
            split.itemS()
        elif cam:
            split.template_ID(space, "pin_id")
            split.itemS()


class DATA_PT_camera(DataButtonsPanel):
    bl_label = "Lens"

    def draw(self, context):
        layout = self.layout

        cam = context.camera

        layout.itemR(cam, "type", expand=True)

        row = layout.row()
        if cam.type == 'PERSP':
            if cam.lens_unit == 'MILLIMETERS':
                row.itemR(cam, "lens", text="Angle")
            elif cam.lens_unit == 'DEGREES':
                row.itemR(cam, "angle")
            row.itemR(cam, "lens_unit", text="")

        elif cam.type == 'ORTHO':
            row.itemR(cam, "ortho_scale")

        layout.itemR(cam, "panorama")

        split = layout.split()

        col = split.column(align=True)
        col.itemL(text="Shift:")
        col.itemR(cam, "shift_x", text="X")
        col.itemR(cam, "shift_y", text="Y")

        col = split.column(align=True)
        col.itemL(text="Clipping:")
        col.itemR(cam, "clip_start", text="Start")
        col.itemR(cam, "clip_end", text="End")

        layout.itemL(text="Depth of Field:")

        row = layout.row()
        row.itemR(cam, "dof_object", text="")
        row.itemR(cam, "dof_distance", text="Distance")


class DATA_PT_camera_display(DataButtonsPanel):
    bl_label = "Display"

    def draw(self, context):
        layout = self.layout

        cam = context.camera

        split = layout.split()

        col = split.column()
        col.itemR(cam, "show_limits", text="Limits")
        col.itemR(cam, "show_mist", text="Mist")
        col.itemR(cam, "show_title_safe", text="Title Safe")
        col.itemR(cam, "show_name", text="Name")

        col = split.column()
        col.itemR(cam, "draw_size", text="Size")
        col.itemS()
        col.itemR(cam, "show_passepartout", text="Passepartout")
        sub = col.column()
        sub.active = cam.show_passepartout
        sub.itemR(cam, "passepartout_alpha", text="Alpha", slider=True)

bpy.types.register(DATA_PT_context_camera)
bpy.types.register(DATA_PT_camera)
bpy.types.register(DATA_PT_camera_display)
