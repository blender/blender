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
from bpy.types import Panel


class DataButtonsPanel:
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "data"

    @classmethod
    def poll(cls, context):
        ob = context.object
        return (ob and ob.type == 'EMPTY')


class DATA_PT_empty(DataButtonsPanel, Panel):
    bl_label = "Empty"

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        ob = context.object

        layout.prop(ob, "empty_display_type", text="Display As")
        layout.prop(ob, "empty_display_size", text="Size")

        if ob.empty_display_type == 'IMAGE':
            col = layout.column(align=True)
            col.prop(ob, "empty_image_offset", text="Offset X", index=0)
            col.prop(ob, "empty_image_offset", text="Y", index=1)

            col = layout.column()
            depth_row = col.row()
            depth_row.enabled = not ob.show_in_front
            depth_row.prop(ob, "empty_image_depth", text="Depth", expand=True)
            col.row().prop(ob, "empty_image_side", text="Side", expand=True)

            col = layout.column(heading="Show In", align=True)
            col.prop(ob, "show_empty_image_orthographic", text="Orthographic")
            col.prop(ob, "show_empty_image_perspective", text="Perspective")
            col.prop(ob, "show_empty_image_only_axis_aligned", text="Only Axis Aligned")

            col = layout.column(align=False, heading="Transparency")
            col.use_property_decorate = False
            row = col.row(align=True)
            sub = row.row(align=True)
            sub.prop(ob, "use_empty_image_alpha", text="")
            sub = sub.row(align=True)
            sub.active = ob.use_empty_image_alpha
            sub.prop(ob, "color", text="", index=3, slider=True)
            row.prop_decorator(ob, "color", index=3)


class DATA_PT_empty_image(DataButtonsPanel, Panel):
    bl_label = "Image"

    @classmethod
    def poll(cls, context):
        ob = context.object
        return (ob and ob.type == 'EMPTY' and ob.empty_display_type == 'IMAGE')

    def draw(self, context):
        layout = self.layout
        ob = context.object
        layout.template_ID(ob, "data", open="image.open", unlink="object.unlink_data")
        layout.separator()
        layout.template_image(ob, "data", ob.image_user, compact=True)


classes = (
    DATA_PT_empty,
    DATA_PT_empty_image,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
