# gpl author: Ryan Inch (Imaginer)

import bpy
from bpy.types import Menu
from . import utils_core
from .brushes import brush_datapath

# stroke methods: 'AIRBRUSH' 'ANCHORED' 'SPACE' 'DRAG_DOT' 'DOTS' 'LINE' 'CURVE'

class PaintCurvesMenu(Menu):
    bl_label = "Paint Curves"
    bl_idname = "VIEW3D_MT_sv3_paint_curves_menu"

    def draw(self, context):
        mode = utils_core.get_mode()
        layout = self.layout
        colum_n = utils_core.addon_settings(lists=False)

        layout.row().label(text="Paint Curves")
        layout.row().separator()

        has_brush = utils_core.get_brush_link(context, types="brush")

        has_current_curve = has_brush.paint_curve if has_brush else None
        current_curve = has_current_curve.name if has_current_curve else ''

        column_flow = layout.column_flow(columns=colum_n)

        if len(bpy.data.paint_curves) != 0:
            for x, item in enumerate(bpy.data.paint_curves):
                utils_core.menuprop(
                        column_flow.row(),
                        item.name,
                        'bpy.data.paint_curves["%s"]' % item.name,
                        brush_datapath[mode] + ".paint_curve",
                        icon='RADIOBUT_OFF',
                        disable=True,
                        disable_icon='RADIOBUT_ON',
                        custom_disable_exp=(item.name, current_curve),
                        path=True
                        )

        else:
            layout.row().label("No Paint Curves Available", icon="INFO")

class StrokeOptionsMenu(Menu):
    bl_label = "Stroke Options"
    bl_idname = "VIEW3D_MT_sv3_stroke_options"

    @classmethod
    def poll(self, context):
        return utils_core.get_mode() in (
                    'SCULPT', 'VERTEX_PAINT',
                    'WEIGHT_PAINT', 'TEXTURE_PAINT',
                    'PARTICLE_EDIT'
                    )

    def init(self):
        has_brush = utils_core.get_brush_link(bpy.context, types="brush")
        if utils_core.get_mode() == 'SCULPT':
            settings = bpy.context.tool_settings.sculpt

        elif utils_core.get_mode() == 'VERTEX_PAINT':
            settings = bpy.context.tool_settings.vertex_paint

        elif utils_core.get_mode() == 'WEIGHT_PAINT':
            settings = bpy.context.tool_settings.weight_paint

        elif utils_core.get_mode() == 'TEXTURE_PAINT':
            settings = bpy.context.tool_settings.image_paint

        else:
            settings = None

        stroke_method = has_brush.stroke_method if has_brush else None

        return settings, has_brush, stroke_method

    def draw(self, context):
        settings, brush, stroke_method = self.init()
        layout = self.layout

        layout.row().menu(StrokeMethodMenu.bl_idname)
        layout.row().separator()

        if stroke_method:

            if stroke_method in ('SPACE', 'LINE') and brush:
                layout.row().prop(brush, "spacing",
                                     text=utils_core.PIW + "Spacing", slider=True)

            elif stroke_method == 'AIRBRUSH' and brush:
                layout.row().prop(brush, "rate",
                                    text=utils_core.PIW + "Rate", slider=True)

            elif stroke_method == 'ANCHORED' and brush:
                layout.row().prop(brush, "use_edge_to_edge")

            elif stroke_method == 'CURVE' and brush:
                has_current_curve = brush.paint_curve if brush else None
                current_curve = has_current_curve.name if has_current_curve else 'No Curve Selected'

                layout.row().menu(PaintCurvesMenu.bl_idname, text=current_curve,
                                  icon='CURVE_BEZCURVE')
                layout.row().operator("paintcurve.new", icon='ZOOMIN')
                layout.row().operator("paintcurve.draw")

                layout.row().separator()

                layout.row().prop(brush, "spacing",
                                  text=utils_core.PIW + "Spacing",
                                  slider=True)

            else:
                pass

            if utils_core.get_mode() == 'SCULPT' and stroke_method in ('DRAG_DOT', 'ANCHORED'):
                pass
            else:
                if brush:
                    layout.row().prop(brush, "jitter",
                                      text=utils_core.PIW + "Jitter", slider=True)

            layout.row().prop(settings, "input_samples",
                              text=utils_core.PIW + "Input Samples", slider=True)

            if stroke_method in ('DOTS', 'SPACE', 'AIRBRUSH') and brush:
                layout.row().separator()

                layout.row().prop(brush, "use_smooth_stroke", toggle=True)

                if brush.use_smooth_stroke:
                    layout.row().prop(brush, "smooth_stroke_radius",
                                      text=utils_core.PIW + "Radius", slider=True)
                    layout.row().prop(brush, "smooth_stroke_factor",
                                      text=utils_core.PIW + "Factor", slider=True)
        else:
            layout.row().label("No Stroke Options available", icon="INFO")


class StrokeMethodMenu(Menu):
    bl_label = "Stroke Method"
    bl_idname = "VIEW3D_MT_sv3_stroke_method"

    def init(self):
        has_brush = utils_core.get_brush_link(bpy.context, types="brush")
        if utils_core.get_mode() == 'SCULPT':
            path = "tool_settings.sculpt.brush.stroke_method"

        elif utils_core.get_mode() == 'VERTEX_PAINT':
            path = "tool_settings.vertex_paint.brush.stroke_method"

        elif utils_core.get_mode() == 'WEIGHT_PAINT':
            path = "tool_settings.weight_paint.brush.stroke_method"

        elif utils_core.get_mode() == 'TEXTURE_PAINT':
            path = "tool_settings.image_paint.brush.stroke_method"

        else:
            path = ""

        return has_brush, path

    def draw(self, context):
        brush, path = self.init()
        layout = self.layout

        layout.row().label(text="Stroke Method")
        layout.row().separator()

        if brush:
            # add the menu items dynamicaly based on values in enum property
            for tool in brush.bl_rna.properties['stroke_method'].enum_items:
                if tool.identifier in ('ANCHORED', 'DRAG_DOT') and \
                   utils_core.get_mode() in ('VERTEX_PAINT',
                                             'WEIGHT_PAINT'):
                    continue

                utils_core.menuprop(
                        layout.row(), tool.name, tool.identifier, path,
                        icon='RADIOBUT_OFF', disable=True,
                        disable_icon='RADIOBUT_ON'
                        )
        else:
            layout.row().label("No Stroke Method available", icon="INFO")
