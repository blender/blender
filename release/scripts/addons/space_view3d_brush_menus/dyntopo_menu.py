# gpl author: Ryan Inch (Imaginer)

import bpy
from bpy.types import Menu
from . import utils_core


class DynTopoMenu(Menu):
    bl_label = "Dyntopo"
    bl_idname = "VIEW3D_MT_sv3_dyntopo"

    @classmethod
    def poll(self, context):
        return utils_core.get_mode() == 'SCULPT'

    def draw(self, context):
        layout = self.layout

        if context.object.use_dynamic_topology_sculpting:
            layout.row().operator("sculpt.dynamic_topology_toggle",
                                     "Disable Dynamic Topology")

            layout.row().separator()

            layout.row().menu(DynDetailMenu.bl_idname)
            layout.row().menu(DetailMethodMenu.bl_idname)

            layout.row().separator()

            layout.row().operator("sculpt.optimize")
            if context.tool_settings.sculpt.detail_type_method == 'CONSTANT':
                layout.row().operator("sculpt.detail_flood_fill")

            layout.row().menu(SymmetrizeMenu.bl_idname)
            layout.row().prop(context.tool_settings.sculpt,
                                 "use_smooth_shading", toggle=True)

        else:
            row = layout.row()
            row.operator_context = 'INVOKE_DEFAULT'
            row.operator("sculpt.dynamic_topology_toggle",
                                        "Enable Dynamic Topology")


class DynDetailMenu(Menu):
    bl_label = "Detail Size"
    bl_idname = "VIEW3D_MT_sv3_dyn_detail"

    def init(self):
        settings = (("40", 40),
                    ("30", 30),
                    ("20", 20),
                    ("10", 10),
                    ("5", 5),
                    ("1", 1))

        if bpy.context.tool_settings.sculpt.detail_type_method == 'RELATIVE':
            datapath = "tool_settings.sculpt.detail_size"
            slider_setting = "detail_size"

        elif bpy.context.tool_settings.sculpt.detail_type_method == 'CONSTANT':
            datapath = "tool_settings.sculpt.constant_detail"
            slider_setting = "constant_detail"
        else:
            datapath = "tool_settings.sculpt.detail_percent"
            slider_setting = "detail_percent"
            settings = (("100", 100),
                        ("75", 75),
                        ("50", 50),
                        ("25", 25),
                        ("10", 10),
                        ("5", 5))

        return settings, datapath, slider_setting

    def draw(self, context):
        settings, datapath, slider_setting = self.init()
        layout = self.layout

        # add the top slider
        layout.row().prop(context.tool_settings.sculpt,
                             slider_setting, slider=True)
        layout.row().separator()

        # add the rest of the menu items
        for i in range(len(settings)):
            utils_core.menuprop(
                    layout.row(), settings[i][0], settings[i][1], datapath,
                    icon='RADIOBUT_OFF', disable=True,
                    disable_icon='RADIOBUT_ON'
                    )


class DetailMethodMenu(Menu):
    bl_label = "Detail Method"
    bl_idname = "VIEW3D_MT_sv3_detail_method_menu"

    def draw(self, context):
        layout = self.layout
        refine_path = "tool_settings.sculpt.detail_refine_method"
        type_path = "tool_settings.sculpt.detail_type_method"

        refine_items = (("Subdivide Edges", 'SUBDIVIDE'),
                        ("Collapse Edges", 'COLLAPSE'),
                        ("Subdivide Collapse", 'SUBDIVIDE_COLLAPSE'))

        type_items = (("Relative Detail", 'RELATIVE'),
                      ("Constant Detail", 'CONSTANT'),
                      ("Brush Detail", 'BRUSH'))

        layout.row().label("Refine")
        layout.row().separator()

        # add the refine menu items
        for item in refine_items:
            utils_core.menuprop(
                    layout.row(), item[0], item[1],
                    refine_path, disable=True,
                    icon='RADIOBUT_OFF',
                    disable_icon='RADIOBUT_ON'
                    )

        layout.row().label("")

        layout.row().label("Type")
        layout.row().separator()

        # add the type menu items
        for item in type_items:
            utils_core.menuprop(
                    layout.row(), item[0], item[1],
                    type_path, disable=True,
                    icon='RADIOBUT_OFF', disable_icon='RADIOBUT_ON'
                    )


class SymmetrizeMenu(Menu):
    bl_label = "Symmetrize"
    bl_idname = "VIEW3D_MT_sv3_symmetrize_menu"

    def draw(self, context):
        layout = self.layout
        path = "tool_settings.sculpt.symmetrize_direction"

        # add the the symmetrize operator to the menu
        layout.row().operator("sculpt.symmetrize")
        layout.row().separator()

        # add the rest of the menu items
        for item in context.tool_settings.sculpt. \
          bl_rna.properties['symmetrize_direction'].enum_items:
            utils_core.menuprop(
                    layout.row(), item.name, item.identifier,
                    path, disable=True,
                    icon='RADIOBUT_OFF', disable_icon='RADIOBUT_ON'
                    )
