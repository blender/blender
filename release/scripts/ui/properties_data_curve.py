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
        return (context.object and context.object.type in ('CURVE', 'SURFACE', 'TEXT') and context.curve)


class DataButtonsPanelCurve(DataButtonsPanel):
    '''Same as above but for curves only'''

    def poll(self, context):
        return (context.object and context.object.type == 'CURVE' and context.curve)


class DataButtonsPanelActive(DataButtonsPanel):
    '''Same as above but for curves only'''

    def poll(self, context):
        curve = context.curve
        return (curve and curve.active_spline)


class DATA_PT_context_curve(DataButtonsPanel):
    bl_label = ""
    bl_show_header = False

    def draw(self, context):
        layout = self.layout

        ob = context.object
        curve = context.curve
        space = context.space_data
        wide_ui = context.region.width > narrowui


        if wide_ui:
            split = layout.split(percentage=0.65)

            if ob:
                split.template_ID(ob, "data")
                split.separator()
            elif curve:
                split.template_ID(space, "pin_id")
                split.separator()
        else:
            layout.template_ID(ob, "data")


class DATA_PT_custom_props_curve(DataButtonsPanel, PropertyPanel):
    _context_path = "object.data"


class DATA_PT_shape_curve(DataButtonsPanel):
    bl_label = "Shape"

    def draw(self, context):
        layout = self.layout

        ob = context.object
        curve = context.curve
        wide_ui = context.region.width > narrowui
        is_surf = (ob.type == 'SURFACE')
        is_curve = (ob.type == 'CURVE')
        is_text = (ob.type == 'TEXT')

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
            col.prop(curve, "fast", text="Fast Editing")

        if wide_ui:
            col = split.column()

        if is_surf:
            sub = col.column(align=True)
            sub.label(text="")
            sub.prop(curve, "resolution_v", text="Preview V")
            sub.prop(curve, "render_resolution_v", text="Render V")

        if is_curve or is_text:
            sub = col.column()
            sub.active = (curve.dimensions == '2D')
            sub.label(text="Caps:")
            sub.prop(curve, "front")
            sub.prop(curve, "back")

        col.label(text="Textures:")
#       col.prop(curve, "uv_orco")
        col.prop(curve, "auto_texspace")


class DATA_PT_geometry_curve(DataButtonsPanel):
    bl_label = "Geometry"

    def poll(self, context):
        obj = context.object
        if obj and obj.type == 'SURFACE':
            return False

        return context.curve

    def draw(self, context):
        layout = self.layout

        curve = context.curve
        wide_ui = context.region.width > narrowui

        split = layout.split()

        col = split.column()
        col.label(text="Modification:")
        col.prop(curve, "width")
        col.prop(curve, "extrude")
        col.label(text="Taper Object:")
        col.prop(curve, "taper_object", text="")

        if wide_ui:
            col = split.column()
        col.label(text="Bevel:")
        col.prop(curve, "bevel_depth", text="Depth")
        col.prop(curve, "bevel_resolution", text="Resolution")
        col.label(text="Bevel Object:")
        col.prop(curve, "bevel_object", text="")


class DATA_PT_pathanim(DataButtonsPanelCurve):
    bl_label = "Path Animation"

    def draw_header(self, context):
        curve = context.curve

        self.layout.prop(curve, "use_path", text="")

    def draw(self, context):
        layout = self.layout

        curve = context.curve
        wide_ui = context.region.width > narrowui

        layout.active = curve.use_path

        col = layout.column()
        layout.prop(curve, "path_length", text="Frames")
        layout.prop(curve, "eval_time")

        split = layout.split()

        col = split.column()
        col.prop(curve, "use_path_follow")
        col.prop(curve, "use_stretch")

        if wide_ui:
            col = split.column()
        col.prop(curve, "use_radius")
        col.prop(curve, "use_time_offset", text="Offset Children")


class DATA_PT_active_spline(DataButtonsPanelActive):
    bl_label = "Active Spline"

    def draw(self, context):
        layout = self.layout

        ob = context.object
        curve = context.curve
        act_spline = curve.active_spline
        is_surf = (ob.type == 'SURFACE')
        is_poly = (act_spline.type == 'POLY')

        split = layout.split()

        if is_poly:
            # These settings are below but its easier to have
            # poly's set aside since they use so few settings
            col = split.column()
            col.label(text="Cyclic:")
            col.prop(act_spline, "smooth")
            col = split.column()
            col.prop(act_spline, "cyclic_u", text="U")

        else:
            col = split.column()
            col.label(text="Cyclic:")
            if act_spline.type == 'NURBS':
                col.label(text="Bezier:")
                col.label(text="Endpoint:")
                col.label(text="Order:")

            col.label(text="Resolution:")

            col = split.column()
            col.prop(act_spline, "cyclic_u", text="U")

            if act_spline.type == 'NURBS':
                sub = col.column()
                # sub.active = (not act_spline.cyclic_u)
                sub.prop(act_spline, "bezier_u", text="U")
                sub.prop(act_spline, "endpoint_u", text="U")

                sub = col.column()
                sub.prop(act_spline, "order_u", text="U")
            col.prop(act_spline, "resolution_u", text="U")

            if is_surf:
                col = split.column()
                col.prop(act_spline, "cyclic_v", text="V")

                # its a surface, assume its a nurb.
                sub = col.column()
                sub.active = (not act_spline.cyclic_v)
                sub.prop(act_spline, "bezier_v", text="V")
                sub.prop(act_spline, "endpoint_v", text="V")
                sub = col.column()
                sub.prop(act_spline, "order_v", text="V")
                sub.prop(act_spline, "resolution_v", text="V")

            if not is_surf:
                split = layout.split()
                col = split.column()
                col.active = (curve.dimensions == '3D')

                col.label(text="Interpolation:")
                col.prop(act_spline, "tilt_interpolation", text="Tilt")
                col.prop(act_spline, "radius_interpolation", text="Radius")

            layout.prop(act_spline, "smooth")


class DATA_PT_font(DataButtonsPanel):
    bl_label = "Font"

    def poll(self, context):
        return (context.object and context.object.type == 'TEXT' and context.curve)

    def draw(self, context):
        layout = self.layout

        text = context.curve
        char = context.curve.edit_format
        wide_ui = context.region.width > narrowui

        layout.template_ID(text, "font", open="font.open", unlink="font.unlink")

        #if wide_ui:
        #    layout.prop(text, "font")
        #else:
        #    layout.prop(text, "font", text="")

        split = layout.split()

        col = split.column()
        col.prop(text, "text_size", text="Size")
        if wide_ui:
            col = split.column()
        col.prop(text, "shear")

        split = layout.split()

        col = split.column()
        col.label(text="Object Font:")
        col.prop(text, "family", text="")

        if wide_ui:
            col = split.column()
        col.label(text="Text on Curve:")
        col.prop(text, "text_on_curve", text="")

        split = layout.split()

        col = split.column(align=True)
        col.label(text="Underline:")
        col.prop(text, "ul_position", text="Position")
        col.prop(text, "ul_height", text="Thickness")

        if wide_ui:
            col = split.column()
        col.label(text="Character:")
        col.prop(char, "bold")
        col.prop(char, "italic")
        col.prop(char, "underline")
#       col.prop(char, "style")
#       col.prop(char, "wrap")


class DATA_PT_paragraph(DataButtonsPanel):
    bl_label = "Paragraph"

    def poll(self, context):
        return (context.object and context.object.type == 'TEXT' and context.curve)

    def draw(self, context):
        layout = self.layout

        text = context.curve
        wide_ui = context.region.width > narrowui

        layout.label(text="Align:")
        if wide_ui:
            layout.prop(text, "spacemode", expand=True)
        else:
            layout.prop(text, "spacemode", text="")

        split = layout.split()

        col = split.column(align=True)
        col.label(text="Spacing:")
        col.prop(text, "spacing", text="Character")
        col.prop(text, "word_spacing", text="Word")
        col.prop(text, "line_dist", text="Line")

        if wide_ui:
            col = split.column(align=True)
        col.label(text="Offset:")
        col.prop(text, "offset_x", text="X")
        col.prop(text, "offset_y", text="Y")


class DATA_PT_textboxes(DataButtonsPanel):
    bl_label = "Text Boxes"

    def poll(self, context):
        return (context.object and context.object.type == 'TEXT' and context.curve)

    def draw(self, context):
        layout = self.layout

        text = context.curve
        wide_ui = context.region.width > narrowui

        for box in text.textboxes:
            split = layout.box().split()

            col = split.column(align=True)
            col.label(text="Dimensions:")
            col.prop(box, "width", text="Width")
            col.prop(box, "height", text="Height")

            if wide_ui:
                col = split.column(align=True)
            col.label(text="Offset:")
            col.prop(box, "x", text="X")
            col.prop(box, "y", text="Y")


classes = [
    DATA_PT_context_curve,
    DATA_PT_shape_curve,
    DATA_PT_geometry_curve,
    DATA_PT_pathanim,
    DATA_PT_active_spline,
    DATA_PT_font,
    DATA_PT_paragraph,
    DATA_PT_textboxes,

    DATA_PT_custom_props_curve]


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
