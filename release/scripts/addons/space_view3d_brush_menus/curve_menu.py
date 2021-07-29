# gpl author: Ryan Inch (Imaginer)

from bpy.types import (
        Operator,
        Menu,
        )
from . import utils_core


class BrushCurveMenu(Menu):
    bl_label = "Curve"
    bl_idname = "VIEW3D_MT_sv3_brush_curve_menu"

    @classmethod
    def poll(self, context):
        return utils_core.get_mode() in (
                        'SCULPT', 'VERTEX_PAINT',
                        'WEIGHT_PAINT', 'TEXTURE_PAINT',
                        'PARTICLE_EDIT'
                        )

    def draw(self, context):
        layout = self.layout
        curves = (("Smooth", "SMOOTH", "SMOOTHCURVE"),
                  ("Sphere", "ROUND", "SPHERECURVE"),
                  ("Root", "ROOT", "ROOTCURVE"),
                  ("Sharp", "SHARP", "SHARPCURVE"),
                  ("Linear", "LINE", "LINCURVE"),
                  ("Constant", "MAX", "NOCURVE"))

        # add the top slider
        layout.row().operator(CurvePopup.bl_idname, icon="RNDCURVE")
        layout.row().separator()

        # add the rest of the menu items
        for curve in curves:
            item = layout.row().operator("brush.curve_preset",
                                            text=curve[0], icon=curve[2])
            item.shape = curve[1]


class CurvePopup(Operator):
    bl_label = "Adjust Curve"
    bl_idname = "view3d.sv3_curve_popup"
    bl_options = {'REGISTER'}

    @classmethod
    def poll(self, context):
        return utils_core.get_mode() in (
                        'SCULPT', 'VERTEX_PAINT',
                        'WEIGHT_PAINT', 'TEXTURE_PAINT'
                        )

    def draw(self, context):
        layout = self.layout
        has_brush = utils_core.get_brush_link(context, types="brush")

        if utils_core.get_mode() == 'SCULPT' or \
          utils_core.get_mode() == 'VERTEX_PAINT' or \
          utils_core.get_mode() == 'WEIGHT_PAINT' or \
          utils_core.get_mode() == 'TEXTURE_PAINT':
            if has_brush:
                layout.column().template_curve_mapping(has_brush,
                                                               "curve", brush=True)
            else:
                layout.row().label("No brushes available", icon="INFO")
        else:
            layout.row().label("No brushes available", icon="INFO")

    def execute(self, context):
        return context.window_manager.invoke_popup(self, width=180)
