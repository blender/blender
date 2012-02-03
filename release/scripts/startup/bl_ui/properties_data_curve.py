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
from bpy.types import Panel
from rna_prop_ui import PropertyPanel


class CurveButtonsPanel():
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "data"

    @classmethod
    def poll(cls, context):
        return (context.object and context.object.type in {'CURVE', 'SURFACE', 'FONT'} and context.curve)


class CurveButtonsPanelCurve(CurveButtonsPanel):
    '''Same as above but for curves only'''

    @classmethod
    def poll(cls, context):
        return (context.object and context.object.type == 'CURVE' and context.curve)


class CurveButtonsPanelActive(CurveButtonsPanel):
    '''Same as above but for curves only'''

    @classmethod
    def poll(cls, context):
        curve = context.curve
        return (curve and type(curve) is not bpy.types.TextCurve and curve.splines.active)


class DATA_PT_context_curve(CurveButtonsPanel, Panel):
    bl_label = ""
    bl_options = {'HIDE_HEADER'}

    def draw(self, context):
        layout = self.layout

        ob = context.object
        curve = context.curve
        space = context.space_data

        if ob:
            layout.template_ID(ob, "data")
        elif curve:
            layout.template_ID(space, "pin_id")  # XXX: broken


class DATA_PT_shape_curve(CurveButtonsPanel, Panel):
    bl_label = "Shape"

    def draw(self, context):
        layout = self.layout

        ob = context.object
        curve = context.curve
        is_surf = (ob.type == 'SURFACE')
        is_curve = (ob.type == 'CURVE')
        is_text = (ob.type == 'FONT')

        if is_curve:
            row = layout.row()
            row.prop(curve, "dimensions", expand=True)

        split = layout.split()

        col = split.column()
        col.label(text="Resolution:")
        sub = col.column(align=True)
        sub.prop(curve, "resolution_u", text="Preview U")
        sub.prop(curve, "render_resolution_u", text="Render U")
        if is_curve:
            col.label(text="Twisting:")
            col.prop(curve, "twist_mode", text="")
            col.prop(curve, "twist_smooth", text="Smooth")
        if is_text:
            col.label(text="Display:")
            col.prop(curve, "use_fast_edit", text="Fast Editing")

        col = split.column()

        if is_surf:
            sub = col.column()
            sub.label(text="")
            sub = col.column(align=True)
            sub.prop(curve, "resolution_v", text="Preview V")
            sub.prop(curve, "render_resolution_v", text="Render V")

        if (is_curve or is_text):
            col.label(text="Fill:")
            sub = col.column()
            sub.active = (curve.dimensions == '2D' or (curve.bevel_object is None and curve.dimensions == '3D'))
            sub.prop(curve, "fill_mode", text="")
            col.prop(curve, "use_fill_deform")


class DATA_PT_curve_texture_space(CurveButtonsPanel, Panel):
    bl_label = "Texture Space"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    def draw(self, context):
        layout = self.layout

        curve = context.curve

        row = layout.row()
        row.prop(curve, "use_auto_texspace")
        row.prop(curve, "use_uv_as_generated")

        row = layout.row()
        row.column().prop(curve, "texspace_location", text="Location")
        row.column().prop(curve, "texspace_size", text="Size")


class DATA_PT_geometry_curve(CurveButtonsPanel, Panel):
    bl_label = "Geometry"

    @classmethod
    def poll(cls, context):
        obj = context.object
        if obj and obj.type == 'SURFACE':
            return False

        return context.curve

    def draw(self, context):
        layout = self.layout

        curve = context.curve

        split = layout.split()

        col = split.column()
        col.label(text="Modification:")
        col.prop(curve, "offset")
        col.prop(curve, "extrude")
        col.label(text="Taper Object:")
        col.prop(curve, "taper_object", text="")

        col = split.column()
        col.label(text="Bevel:")
        col.prop(curve, "bevel_depth", text="Depth")
        col.prop(curve, "bevel_resolution", text="Resolution")
        col.label(text="Bevel Object:")
        col.prop(curve, "bevel_object", text="")

        row = col.row()
        row.active = (curve.bevel_object is not None)
        row.prop(curve, "use_fill_caps")


class DATA_PT_pathanim(CurveButtonsPanelCurve, Panel):
    bl_label = "Path Animation"

    def draw_header(self, context):
        curve = context.curve

        self.layout.prop(curve, "use_path", text="")

    def draw(self, context):
        layout = self.layout

        curve = context.curve

        layout.active = curve.use_path

        col = layout.column()
        layout.prop(curve, "path_duration", text="Frames")
        layout.prop(curve, "eval_time")

        split = layout.split()

        col = split.column()
        col.prop(curve, "use_path_follow")
        col.prop(curve, "use_stretch")
        col.prop(curve, "use_deform_bounds")

        col = split.column()
        col.prop(curve, "use_radius")
        col.prop(curve, "use_time_offset", text="Offset Children")


class DATA_PT_active_spline(CurveButtonsPanelActive, Panel):
    bl_label = "Active Spline"

    def draw(self, context):
        layout = self.layout

        ob = context.object
        curve = context.curve
        act_spline = curve.splines.active
        is_surf = (ob.type == 'SURFACE')
        is_poly = (act_spline.type == 'POLY')

        split = layout.split()

        if is_poly:
            # These settings are below but its easier to have
            # poly's set aside since they use so few settings
            row = layout.row()
            row.label(text="Cyclic:")
            row.prop(act_spline, "use_cyclic_u", text="U")

            layout.prop(act_spline, "use_smooth")
        else:
            col = split.column()
            col.label(text="Cyclic:")
            if act_spline.type == 'NURBS':
                col.label(text="Bezier:")
                col.label(text="Endpoint:")
                col.label(text="Order:")

            col.label(text="Resolution:")

            col = split.column()
            col.prop(act_spline, "use_cyclic_u", text="U")

            if act_spline.type == 'NURBS':
                sub = col.column()
                # sub.active = (not act_spline.use_cyclic_u)
                sub.prop(act_spline, "use_bezier_u", text="U")
                sub.prop(act_spline, "use_endpoint_u", text="U")

                sub = col.column()
                sub.prop(act_spline, "order_u", text="U")
            col.prop(act_spline, "resolution_u", text="U")

            if is_surf:
                col = split.column()
                col.prop(act_spline, "use_cyclic_v", text="V")

                # its a surface, assume its a nurb.
                sub = col.column()
                sub.active = (not act_spline.use_cyclic_v)
                sub.prop(act_spline, "use_bezier_v", text="V")
                sub.prop(act_spline, "use_endpoint_v", text="V")
                sub = col.column()
                sub.prop(act_spline, "order_v", text="V")
                sub.prop(act_spline, "resolution_v", text="V")

            if not is_surf:
                col = layout.column()
                col.label(text="Interpolation:")

                sub = col.column()
                sub.active = (curve.dimensions == '3D')
                sub.prop(act_spline, "tilt_interpolation", text="Tilt")

                col.prop(act_spline, "radius_interpolation", text="Radius")

            layout.prop(act_spline, "use_smooth")


class DATA_PT_font(CurveButtonsPanel, Panel):
    bl_label = "Font"

    @classmethod
    def poll(cls, context):
        return (context.object and context.object.type == 'FONT' and context.curve)

    def draw(self, context):
        layout = self.layout

        text = context.curve
        char = context.curve.edit_format

        row = layout.split(percentage=0.25)
        row.label(text="Regular")
        row.template_ID(text, "font", open="font.open", unlink="font.unlink")
        row = layout.split(percentage=0.25)
        row.label(text="Bold")
        row.template_ID(text, "font_bold", open="font.open", unlink="font.unlink")
        row = layout.split(percentage=0.25)
        row.label(text="Italic")
        row.template_ID(text, "font_italic", open="font.open", unlink="font.unlink")
        row = layout.split(percentage=0.25)
        row.label(text="Bold & Italic")
        row.template_ID(text, "font_bold_italic", open="font.open", unlink="font.unlink")

        #layout.prop(text, "font")

        split = layout.split()

        col = split.column()
        col.prop(text, "size", text="Size")
        col = split.column()
        col.prop(text, "shear")

        split = layout.split()

        col = split.column()
        col.label(text="Object Font:")
        col.prop(text, "family", text="")

        col = split.column()
        col.label(text="Text on Curve:")
        col.prop(text, "follow_curve", text="")

        split = layout.split()

        col = split.column()
        colsub = col.column(align=True)
        colsub.label(text="Underline:")
        colsub.prop(text, "underline_position", text="Position")
        colsub.prop(text, "underline_height", text="Thickness")

        col = split.column()
        col.label(text="Character:")
        col.prop(char, "use_bold")
        col.prop(char, "use_italic")
        col.prop(char, "use_underline")

        row = layout.row()
        row.prop(text, "small_caps_scale", text="Small Caps")
        row.prop(char, "use_small_caps")


class DATA_PT_paragraph(CurveButtonsPanel, Panel):
    bl_label = "Paragraph"

    @classmethod
    def poll(cls, context):
        return (context.object and context.object.type == 'FONT' and context.curve)

    def draw(self, context):
        layout = self.layout

        text = context.curve

        layout.label(text="Align:")
        layout.prop(text, "align", expand=True)

        split = layout.split()

        col = split.column(align=True)
        col.label(text="Spacing:")
        col.prop(text, "space_character", text="Character")
        col.prop(text, "space_word", text="Word")
        col.prop(text, "space_line", text="Line")

        col = split.column(align=True)
        col.label(text="Offset:")
        col.prop(text, "offset_x", text="X")
        col.prop(text, "offset_y", text="Y")


class DATA_PT_text_boxes(CurveButtonsPanel, Panel):
    bl_label = "Text Boxes"

    @classmethod
    def poll(cls, context):
        return (context.object and context.object.type == 'FONT' and context.curve)

    def draw(self, context):
        layout = self.layout

        text = context.curve

        split = layout.split()
        col = split.column()
        col.operator("font.textbox_add", icon='ZOOMIN')
        col = split.column()

        for i, box in enumerate(text.text_boxes):

            boxy = layout.box()

            row = boxy.row()

            split = row.split()

            col = split.column(align=True)

            col.label(text="Dimensions:")
            col.prop(box, "width", text="Width")
            col.prop(box, "height", text="Height")

            col = split.column(align=True)

            col.label(text="Offset:")
            col.prop(box, "x", text="X")
            col.prop(box, "y", text="Y")

            row.operator("font.textbox_remove", text='', icon='X', emboss=False).index = i


class DATA_PT_custom_props_curve(CurveButtonsPanel, PropertyPanel, Panel):
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}
    _context_path = "object.data"
    _property_type = bpy.types.Curve

if __name__ == "__main__":  # only for live edit.
    bpy.utils.register_module(__name__)
