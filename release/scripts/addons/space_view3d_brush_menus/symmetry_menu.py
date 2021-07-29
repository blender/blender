# gpl author: Ryan Inch (Imaginer)

from bpy.types import Menu
from . import utils_core


class MasterSymmetryMenu(Menu):
    bl_label = "Symmetry Options"
    bl_idname = "VIEW3D_MT_sv3_master_symmetry_menu"

    @classmethod
    def poll(self, context):
        return utils_core.get_mode() in (
                        'SCULPT',
                        'TEXTURE_PAINT'
                        )

    def draw(self, context):
        layout = self.layout

        if utils_core.get_mode() == 'TEXTURE_PAINT':
            layout.row().prop(context.tool_settings.image_paint,
                              "use_symmetry_x", toggle=True)
            layout.row().prop(context.tool_settings.image_paint,
                              "use_symmetry_y", toggle=True)
            layout.row().prop(context.tool_settings.image_paint,
                              "use_symmetry_z", toggle=True)
        else:
            layout.row().menu(SymmetryMenu.bl_idname)
            layout.row().menu(SymmetryRadialMenu.bl_idname)
            layout.row().prop(context.tool_settings.sculpt,
                              "use_symmetry_feather", toggle=True)


class SymmetryMenu(Menu):
    bl_label = "Symmetry"
    bl_idname = "VIEW3D_MT_sv3_symmetry_menu"

    def draw(self, context):
        layout = self.layout

        layout.row().label(text="Symmetry")
        layout.row().separator()

        layout.row().prop(context.tool_settings.sculpt,
                          "use_symmetry_x", toggle=True)
        layout.row().prop(context.tool_settings.sculpt,
                          "use_symmetry_y", toggle=True)
        layout.row().prop(context.tool_settings.sculpt,
                          "use_symmetry_z", toggle=True)


class SymmetryRadialMenu(Menu):
    bl_label = "Radial"
    bl_idname = "VIEW3D_MT_sv3_symmetry_radial_menu"

    def draw(self, context):
        layout = self.layout

        layout.row().label(text="Radial")
        layout.row().separator()

        layout.column().prop(context.tool_settings.sculpt,
                             "radial_symmetry", text="", slider=True)
