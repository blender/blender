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
        return (context.object and context.object.type == 'TEXT' and context.curve)


class DATA_PT_context_text(DataButtonsPanel):
    bl_label = ""
    bl_show_header = False

    def draw(self, context):
        layout = self.layout

        ob = context.object
        curve = context.curve
        space = context.space_data

        split = layout.split(percentage=0.65)

        if ob:
            split.template_ID(ob, "data")
            split.itemS()
        elif curve:
            split.template_ID(space, "pin_id")
            split.itemS()


class DATA_PT_shape_text(DataButtonsPanel):
    bl_label = "Shape Text"

    def draw(self, context):
        layout = self.layout

        ob = context.object
        curve = context.curve
        space = context.space_data

        split = layout.split()

        col = split.column()
        col.itemL(text="Caps:")
        row = col.row()
        row .itemR(curve, "front")
        row .itemR(curve, "back")
        # col = split.column()
        col.itemL(text="Textures:")
        col.itemR(curve, "uv_orco")
        col.itemR(curve, "auto_texspace")

        col = split.column()
        col.itemL(text="Resolution:")
        sub = col.column(align=True)
        sub.itemR(curve, "resolution_u", text="Preview")
        sub.itemR(curve, "render_resolution_u", text="Render")

        # resolution_v is not used for text

        sub = col.column(align=True)
        col.itemL(text="Display:")
        col.itemR(curve, "fast", text="Fast Editing")


class DATA_PT_geometry_text(DataButtonsPanel):
    bl_label = "Geometry"

    def draw(self, context):
        layout = self.layout

        curve = context.curve

        split = layout.split()

        col = split.column()
        col.itemL(text="Modification:")
        col.itemR(curve, "width")
        col.itemR(curve, "extrude")
        col.itemL(text="Taper Object:")
        col.itemR(curve, "taper_object", text="")

        col = split.column()
        col.itemL(text="Bevel:")
        col.itemR(curve, "bevel_depth", text="Depth")
        col.itemR(curve, "bevel_resolution", text="Resolution")
        col.itemL(text="Bevel Object:")
        col.itemR(curve, "bevel_object", text="")


class DATA_PT_font(DataButtonsPanel):
    bl_label = "Font"

    def draw(self, context):
        layout = self.layout

        text = context.curve
        char = context.curve.edit_format

        layout.itemR(text, "font")

        row = layout.row()
        row.itemR(text, "text_size", text="Size")
        row.itemR(text, "shear")

        split = layout.split()

        col = split.column()
        col.itemL(text="Object Font:")
        col.itemR(text, "family", text="")

        col = split.column()
        col.itemL(text="Text on Curve:")
        col.itemR(text, "text_on_curve", text="")

        split = layout.split()

        col = split.column()
        col.itemL(text="Character:")
        col.itemR(char, "bold")
        col.itemR(char, "italic")
        col.itemR(char, "underline")
#		col.itemR(char, "style")
#		col.itemR(char, "wrap")

        col = split.column(align=True)
        col.itemL(text="Underline:")
        col.itemR(text, "ul_position", text="Position")
        col.itemR(text, "ul_height", text="Thickness")


class DATA_PT_paragraph(DataButtonsPanel):
    bl_label = "Paragraph"

    def draw(self, context):
        layout = self.layout

        text = context.curve

        layout.itemL(text="Align:")
        layout.itemR(text, "spacemode", expand=True)

        split = layout.split()

        col = split.column(align=True)
        col.itemL(text="Spacing:")
        col.itemR(text, "spacing", text="Character")
        col.itemR(text, "word_spacing", text="Word")
        col.itemR(text, "line_dist", text="Line")

        col = split.column(align=True)
        col.itemL(text="Offset:")
        col.itemR(text, "offset_x", text="X")
        col.itemR(text, "offset_y", text="Y")


class DATA_PT_textboxes(DataButtonsPanel):
    bl_label = "Text Boxes"

    def draw(self, context):
        layout = self.layout

        text = context.curve

        for box in text.textboxes:
            split = layout.box().split()

            col = split.column(align=True)
            col.itemL(text="Dimensions:")
            col.itemR(box, "width", text="Width")
            col.itemR(box, "height", text="Height")

            col = split.column(align=True)
            col.itemL(text="Offset:")
            col.itemR(box, "x", text="X")
            col.itemR(box, "y", text="Y")

bpy.types.register(DATA_PT_context_text)
bpy.types.register(DATA_PT_shape_text)
bpy.types.register(DATA_PT_geometry_text)
bpy.types.register(DATA_PT_font)
bpy.types.register(DATA_PT_paragraph)
bpy.types.register(DATA_PT_textboxes)
