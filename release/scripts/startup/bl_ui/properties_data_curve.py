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

from bpy.types import Curve, SurfaceCurve, TextCurve


class CurveButtonsPanel:
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "data"

    @classmethod
    def poll(cls, context):
        return (context.curve is not None)


class CurveButtonsPanelCurve(CurveButtonsPanel):
    @classmethod
    def poll(cls, context):
        return (type(context.curve) is Curve)


class CurveButtonsPanelSurface(CurveButtonsPanel):
    @classmethod
    def poll(cls, context):
        return (type(context.curve) is SurfaceCurve)


class CurveButtonsPanelText(CurveButtonsPanel):
    @classmethod
    def poll(cls, context):
        return (type(context.curve) is TextCurve)


class CurveButtonsPanelActive(CurveButtonsPanel):
    """Same as above but for curves only"""

    @classmethod
    def poll(cls, context):
        curve = context.curve
        return (curve and type(curve) is not TextCurve and curve.splines.active)


class DATA_PT_context_curve(CurveButtonsPanel, Panel):
    bl_label = ""
    bl_options = {'HIDE_HEADER'}

    def draw(self, context):
        layout = self.layout

        obj = context.object
        curve = context.curve
        space = context.space_data

        if obj:
            layout.template_ID(obj, "data")
        elif curve:
            layout.template_ID(space, "pin_id")


class DATA_PT_shape_curve(CurveButtonsPanel, Panel):
    bl_label = "Shape"

    def draw(self, context):
        layout = self.layout

        curve = context.curve
        is_surf = type(curve) is SurfaceCurve
        is_curve = type(curve) is Curve
        is_text = type(curve) is TextCurve

        if is_curve:
            row = layout.row()
            row.prop(curve, "dimensions", expand=True)

        layout.use_property_split = True

        col = layout.column()
        sub = col.column(align=True)
        sub.prop(curve, "resolution_u", text="Resolution Preview U")
        if is_surf:
            sub.prop(curve, "resolution_v", text="V")

        sub = col.column(align=True)
        sub.prop(curve, "render_resolution_u", text="Render U")
        if is_surf:
            sub.prop(curve, "render_resolution_v", text="V")
        col.separator()

        if is_curve:
            col.prop(curve, "twist_mode")
            col.prop(curve, "twist_smooth", text="Smooth")
        elif is_text:
            col.prop(curve, "use_fast_edit", text="Fast Editing")

        if is_curve or is_text:
            col = layout.column()
            col.separator()

            sub = col.column()
            sub.active = (curve.dimensions == '2D' or (curve.bevel_object is None and curve.dimensions == '3D'))
            sub.prop(curve, "fill_mode")
            col.prop(curve, "use_fill_deform")

        if is_curve:
            col = layout.column()
            col.separator()

            sub = col.column()
            sub.prop(curve, "use_radius")
            sub.prop(curve, "use_stretch")
            sub.prop(curve, "use_deform_bounds")


class DATA_PT_curve_texture_space(CurveButtonsPanel, Panel):
    bl_label = "Texture Space"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        curve = context.curve

        col = layout.column()
        col.prop(curve, "use_uv_as_generated")
        col.prop(curve, "use_auto_texspace")

        col = layout.column()
        col.prop(curve, "texspace_location")
        col.prop(curve, "texspace_size")

        layout.operator("curve.match_texture_space")


class DATA_PT_geometry_curve(CurveButtonsPanelCurve, Panel):
    bl_label = "Geometry"

    @classmethod
    def poll(cls, context):
        return (type(context.curve) in {Curve, TextCurve})

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        curve = context.curve

        col = layout.column()
        col.prop(curve, "offset")

        sub = col.column()
        sub.active = (curve.bevel_object is None)
        sub.prop(curve, "extrude")

        col.prop(curve, "taper_object")

        sub = col.column()
        sub.active = curve.taper_object is not None
        sub.prop(curve, "use_map_taper")


class DATA_PT_geometry_curve_bevel(CurveButtonsPanelCurve, Panel):
    bl_label = "Bevel"
    bl_parent_id = "DATA_PT_geometry_curve"

    @classmethod
    def poll(cls, context):
        return (type(context.curve) in {Curve, TextCurve})

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        curve = context.curve

        col = layout.column()
        sub = col.column()
        sub.active = (curve.bevel_object is None)
        sub.prop(curve, "bevel_depth", text="Depth")
        sub.prop(curve, "bevel_resolution", text="Resolution")

        col.prop(curve, "bevel_object", text="Object")

        sub = col.column()
        sub.active = curve.bevel_object is not None
        sub.prop(curve, "use_fill_caps")

        if type(curve) is not TextCurve:

            col = layout.column()
            col.active = (
                (curve.bevel_depth > 0.0) or
                (curve.extrude > 0.0) or
                (curve.bevel_object is not None)
            )
            sub = col.column(align=True)
            sub.prop(curve, "bevel_factor_start", text="Bevel Start")
            sub.prop(curve, "bevel_factor_end", text="End")

            sub = col.column(align=True)
            sub.prop(curve, "bevel_factor_mapping_start", text="Bevel Mapping Start")
            sub.prop(curve, "bevel_factor_mapping_end", text="End")


class DATA_PT_pathanim(CurveButtonsPanelCurve, Panel):
    bl_label = "Path Animation"

    def draw_header(self, context):
        curve = context.curve

        self.layout.prop(curve, "use_path", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        curve = context.curve

        layout.active = curve.use_path

        col = layout.column()
        col.prop(curve, "path_duration", text="Frames")
        col.prop(curve, "eval_time")

        # these are for paths only
        col.separator()

        col.prop(curve, "use_path_follow")


class DATA_PT_active_spline(CurveButtonsPanelActive, Panel):
    bl_label = "Active Spline"

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        curve = context.curve
        act_spline = curve.splines.active
        is_surf = type(curve) is SurfaceCurve
        is_poly = (act_spline.type == 'POLY')

        col = layout.column()

        if is_poly:
            # These settings are below but its easier to have
            # polys set aside since they use so few settings

            col.prop(act_spline, "use_cyclic_u")
            col.prop(act_spline, "use_smooth")
        else:

            sub = col.column(align=True)
            sub.prop(act_spline, "use_cyclic_u")
            if is_surf:
                sub.prop(act_spline, "use_cyclic_v", text="V")

            if act_spline.type == 'NURBS':
                sub = col.column(align=True)
                # sub.active = (not act_spline.use_cyclic_u)
                sub.prop(act_spline, "use_bezier_u", text="Bezier U")

                if is_surf:
                    subsub = sub.column()
                    subsub.active = (not act_spline.use_cyclic_v)
                    subsub.prop(act_spline, "use_bezier_v", text="V")

                sub = col.column(align=True)
                sub.prop(act_spline, "use_endpoint_u", text="Endpoint U")

                if is_surf:
                    subsub = sub.column()
                    subsub.active = (not act_spline.use_cyclic_v)
                    subsub.prop(act_spline, "use_endpoint_v", text="V")

                sub = col.column(align=True)
                sub.prop(act_spline, "order_u", text="Order U")

                if is_surf:
                    sub.prop(act_spline, "order_v", text="V")

            sub = col.column(align=True)
            sub.prop(act_spline, "resolution_u", text="Resolution U")
            if is_surf:
                sub.prop(act_spline, "resolution_v", text="V")

            if act_spline.type == 'BEZIER':

                col.separator()

                sub = col.column()
                sub.active = (curve.dimensions == '3D')
                sub.prop(act_spline, "tilt_interpolation", text="Interpolation Tilt")

                col.prop(act_spline, "radius_interpolation", text="Radius")

            layout.prop(act_spline, "use_smooth")


class DATA_PT_font(CurveButtonsPanelText, Panel):
    bl_label = "Font"

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

        layout.separator()

        row = layout.row(align=True)
        row.prop(char, "use_bold", toggle=True)
        row.prop(char, "use_italic", toggle=True)
        row.prop(char, "use_underline", toggle=True)
        row.prop(char, "use_small_caps", toggle=True)


class DATA_PT_font_transform(CurveButtonsPanelText, Panel):
    bl_label = "Transform"
    bl_parent_id = "DATA_PT_font"

    def draw(self, context):
        layout = self.layout

        text = context.curve
        char = context.curve.edit_format

        layout.use_property_split = True

        col = layout.column()

        col.separator()

        col.prop(text, "size", text="Size")
        col.prop(text, "shear")

        col.separator()

        col.prop(text, "family")
        col.prop(text, "follow_curve")

        col.separator()

        sub = col.column(align=True)
        sub.prop(text, "underline_position", text="Underline Position")
        sub.prop(text, "underline_height", text="Underline Thickness")

        col.prop(text, "small_caps_scale", text="Small Caps Scale")


class DATA_PT_paragraph(CurveButtonsPanelText, Panel):
    bl_label = "Paragraph"

    def draw(self, context):
        layout = self.layout

        text = context.curve


class DATA_PT_paragraph_alignment(CurveButtonsPanelText, Panel):
    bl_parent_id = "DATA_PT_paragraph"
    bl_label = "Alignment"

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = False

        text = context.curve

        layout.row().prop(text, "align_x", expand=True)
        layout.row().prop(text, "align_y", expand=True)


class DATA_PT_paragraph_spacing(CurveButtonsPanelText, Panel):
    bl_parent_id = "DATA_PT_paragraph"
    bl_label = "Spacing"

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        text = context.curve

        col = layout.column(align=True)
        col.prop(text, "space_character", text="Character Spacing")
        col.prop(text, "space_word", text="Word Spacing")
        col.prop(text, "space_line", text="Line Spacing")

        layout.separator()

        col = layout.column(align=True)
        col.prop(text, "offset_x", text="Offset X")
        col.prop(text, "offset_y", text="Y")


class DATA_PT_text_boxes(CurveButtonsPanelText, Panel):
    bl_label = "Text Boxes"

    def draw(self, context):
        layout = self.layout

        text = context.curve

        layout.operator("font.textbox_add", icon='ZOOMIN')

        for i, box in enumerate(text.text_boxes):

            boxy = layout.box()

            row = boxy.row()

            col = row.column()
            col.use_property_split = True

            sub = col.column(align=True)
            sub.prop(box, "width", text="Size X")
            sub.prop(box, "height", text="Y")

            sub = col.column(align=True)
            sub.prop(box, "x", text="Offset X")
            sub.prop(box, "y", text="Y")

            row.operator("font.textbox_remove", text="", icon='X', emboss=False).index = i


class DATA_PT_custom_props_curve(CurveButtonsPanel, PropertyPanel, Panel):
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}
    _context_path = "object.data"
    _property_type = bpy.types.Curve


classes = (
    DATA_PT_context_curve,
    DATA_PT_shape_curve,
    DATA_PT_curve_texture_space,
    DATA_PT_geometry_curve,
    DATA_PT_geometry_curve_bevel,
    DATA_PT_pathanim,
    DATA_PT_active_spline,
    DATA_PT_font,
    DATA_PT_font_transform,
    DATA_PT_paragraph,
    DATA_PT_paragraph_alignment,
    DATA_PT_paragraph_spacing,
    DATA_PT_text_boxes,
    DATA_PT_custom_props_curve,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
