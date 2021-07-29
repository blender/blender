# gpl author: Ryan Inch (Imaginer)

import bpy
from bpy.types import (
        Operator,
        Menu,
        )
from bpy.props import BoolProperty
from . import utils_core
from . import brushes
from bl_ui.properties_paint_common import UnifiedPaintPanel

class BrushOptionsMenu(Menu):
    bl_label = "Brush Options"
    bl_idname = "VIEW3D_MT_sv3_brush_options"

    @classmethod
    def poll(self, context):
        return utils_core.get_mode() in (
                    'SCULPT', 'VERTEX_PAINT',
                    'WEIGHT_PAINT', 'TEXTURE_PAINT',
                    'PARTICLE_EDIT'
                    )

    def draw_brushes(self, layout, h_brush, ico, context):
        if utils_core.addon_settings(lists=True) == 'popup' or not h_brush:
            layout.row().operator(
                    "view3d.sv3_brush_menu_popup", text="Brush",
                    icon=ico
                    )
        else:
            layout.row().menu(
                    "VIEW3D_MT_sv3_brushes_menu", text="Brush",
                    icon=ico
                    )

    def draw(self, context):
        mode = utils_core.get_mode()
        layout = self.layout

        if mode == 'SCULPT':
            self.sculpt(mode, layout, context)

        elif mode in ('VERTEX_PAINT', 'WEIGHT_PAINT'):
            self.vw_paint(mode, layout, context)

        elif mode == 'TEXTURE_PAINT':
            self.texpaint(mode, layout, context)

        else:
            self.particle(layout, context)

    def sculpt(self, mode, layout, context):
        has_brush = utils_core.get_brush_link(context, types="brush")
        icons = brushes.brush_icon[mode][has_brush.sculpt_tool] if \
                has_brush else "BRUSH_DATA"

        self.draw_brushes(layout, has_brush, icons, context)

        layout.row().menu(BrushRadiusMenu.bl_idname)

        if has_brush:
            # if the active brush is unlinked these menus don't do anything
            layout.row().menu(BrushStrengthMenu.bl_idname)
            layout.row().menu(BrushAutosmoothMenu.bl_idname)
            layout.row().menu(BrushModeMenu.bl_idname)
            layout.row().menu("VIEW3D_MT_sv3_texture_menu")
            layout.row().menu("VIEW3D_MT_sv3_stroke_options")
            layout.row().menu("VIEW3D_MT_sv3_brush_curve_menu")

        layout.row().menu("VIEW3D_MT_sv3_dyntopo")
        layout.row().menu("VIEW3D_MT_sv3_master_symmetry_menu")

    def vw_paint(self, mode, layout, context):
        has_brush = utils_core.get_brush_link(context, types="brush")
        icons = brushes.brush_icon[mode][has_brush.vertex_tool] if \
                has_brush else "BRUSH_DATA"

        if mode == 'VERTEX_PAINT':
            layout.row().operator(ColorPickerPopup.bl_idname, icon="COLOR")
            layout.row().separator()

        self.draw_brushes(layout, has_brush, icons, context)

        if mode == 'VERTEX_PAINT':
            layout.row().menu(BrushRadiusMenu.bl_idname)

            if has_brush:
                # if the active brush is unlinked these menus don't do anything
                layout.row().menu(BrushStrengthMenu.bl_idname)
                layout.row().menu(BrushModeMenu.bl_idname)
                layout.row().menu("VIEW3D_MT_sv3_texture_menu")
                layout.row().menu("VIEW3D_MT_sv3_stroke_options")
                layout.row().menu("VIEW3D_MT_sv3_brush_curve_menu")

        if mode == 'WEIGHT_PAINT':
            layout.row().menu(BrushWeightMenu.bl_idname)
            layout.row().menu(BrushRadiusMenu.bl_idname)

            if has_brush:
                # if the active brush is unlinked these menus don't do anything
                layout.row().menu(BrushStrengthMenu.bl_idname)
                layout.row().menu(BrushModeMenu.bl_idname)
                layout.row().menu("VIEW3D_MT_sv3_stroke_options")
                layout.row().menu("VIEW3D_MT_sv3_brush_curve_menu")

    def texpaint(self, mode, layout, context):
        toolsettings = context.tool_settings.image_paint

        if context.image_paint_object and not toolsettings.detect_data():
            layout.row().label("Missing Data", icon="INFO")
            layout.row().label("See Tool Shelf", icon="BACK")
        else:
            has_brush = utils_core.get_brush_link(context, types="brush")
            if has_brush and has_brush.image_tool in {'DRAW', 'FILL'} and \
               has_brush.blend not in {'ERASE_ALPHA', 'ADD_ALPHA'}:
                layout.row().operator(ColorPickerPopup.bl_idname, icon="COLOR")
                layout.row().separator()

            icons = brushes.brush_icon[mode][has_brush.image_tool] if \
                    has_brush else "BRUSH_DATA"

            self.draw_brushes(layout, has_brush, icons, context)

            if has_brush:
                # if the active brush is unlinked these menus don't do anything
                if has_brush and has_brush.image_tool in {'MASK'}:
                    layout.row().menu(BrushWeightMenu.bl_idname, text="Mask Value")

                if has_brush and has_brush.image_tool not in {'FILL'}:
                    layout.row().menu(BrushRadiusMenu.bl_idname)

                layout.row().menu(BrushStrengthMenu.bl_idname)

                if has_brush and has_brush.image_tool in {'DRAW'}:
                    layout.row().menu(BrushModeMenu.bl_idname)

                layout.row().menu("VIEW3D_MT_sv3_texture_menu")
                layout.row().menu("VIEW3D_MT_sv3_stroke_options")
                layout.row().menu("VIEW3D_MT_sv3_brush_curve_menu")

            layout.row().menu("VIEW3D_MT_sv3_master_symmetry_menu")

    def particle(self, layout, context):
        particle_edit = context.tool_settings.particle_edit

        if particle_edit.tool == 'NONE':
            layout.row().label("No Brush Selected", icon="INFO")
            layout.row().separator()
            layout.row().menu("VIEW3D_MT_sv3_brushes_menu",
                                text="Select Brush", icon="BRUSH_DATA")
        else:
            layout.row().menu("VIEW3D_MT_sv3_brushes_menu",
                                icon="BRUSH_DATA")
            layout.row().menu(BrushRadiusMenu.bl_idname)

            if particle_edit.tool != 'ADD':
                layout.row().menu(BrushStrengthMenu.bl_idname)
            else:
                layout.row().menu(ParticleCountMenu.bl_idname)
                layout.row().separator()
                layout.row().prop(particle_edit, "use_default_interpolate", toggle=True)

                layout.row().prop(particle_edit.brush, "steps", slider=True)
                layout.row().prop(particle_edit, "default_key_count", slider=True)

            if particle_edit.tool == 'LENGTH':
                layout.row().separator()
                layout.row().menu(ParticleLengthMenu.bl_idname)

            if particle_edit.tool == 'PUFF':
                layout.row().separator()
                layout.row().menu(ParticlePuffMenu.bl_idname)
                layout.row().prop(particle_edit.brush, "use_puff_volume", toggle=True)


class BrushRadiusMenu(Menu):
    bl_label = "Radius"
    bl_idname = "VIEW3D_MT_sv3_brush_radius_menu"
    bl_description = "Change the size of the brushes"

    def init(self):
        if utils_core.get_mode() == 'PARTICLE_EDIT':
            settings = (("100", 100),
                        ("70", 70),
                        ("50", 50),
                        ("30", 30),
                        ("20", 20),
                        ("10", 10))

            datapath = "tool_settings.particle_edit.brush.size"
            proppath = bpy.context.tool_settings.particle_edit.brush

        else:
            settings = (("200", 200),
                        ("150", 150),
                        ("100", 100),
                        ("50", 50),
                        ("35", 35),
                        ("10", 10))

            datapath = "tool_settings.unified_paint_settings.size"
            proppath = bpy.context.tool_settings.unified_paint_settings

        return settings, datapath, proppath

    def draw(self, context):
        settings, datapath, proppath = self.init()
        layout = self.layout

        # add the top slider
        layout.row().prop(proppath, "size", slider=True)
        layout.row().separator()

        # add the rest of the menu items
        for i in range(len(settings)):
            utils_core.menuprop(
                    layout.row(), settings[i][0], settings[i][1],
                    datapath, icon='RADIOBUT_OFF', disable=True,
                    disable_icon='RADIOBUT_ON'
                    )


class BrushStrengthMenu(Menu):
    bl_label = "Strength"
    bl_idname = "VIEW3D_MT_sv3_brush_strength_menu"

    def init(self):
        mode = utils_core.get_mode()
        settings = (("1.0", 1.0),
                    ("0.7", 0.7),
                    ("0.5", 0.5),
                    ("0.3", 0.3),
                    ("0.2", 0.2),
                    ("0.1", 0.1))

        proppath = utils_core.get_brush_link(bpy.context, types="brush")

        if mode == 'SCULPT':
            datapath = "tool_settings.sculpt.brush.strength"

        elif mode == 'VERTEX_PAINT':
            datapath = "tool_settings.vertex_paint.brush.strength"

        elif mode == 'WEIGHT_PAINT':
            datapath = "tool_settings.weight_paint.brush.strength"

        elif mode == 'TEXTURE_PAINT':
            datapath = "tool_settings.image_paint.brush.strength"

        else:
            datapath = "tool_settings.particle_edit.brush.strength"
            proppath = bpy.context.tool_settings.particle_edit.brush

        return settings, datapath, proppath

    def draw(self, context):
        settings, datapath, proppath = self.init()
        layout = self.layout

        # add the top slider
        if proppath:
            layout.row().prop(proppath, "strength", slider=True)
            layout.row().separator()

            # add the rest of the menu items
            for i in range(len(settings)):
                utils_core.menuprop(
                        layout.row(), settings[i][0], settings[i][1],
                        datapath, icon='RADIOBUT_OFF', disable=True,
                        disable_icon='RADIOBUT_ON'
                        )
        else:
            layout.row().label("No brushes available", icon="INFO")


class BrushModeMenu(Menu):
    bl_label = "Brush Mode"
    bl_idname = "VIEW3D_MT_sv3_brush_mode_menu"

    def init(self):
        mode = utils_core.get_mode()
        has_brush = utils_core.get_brush_link(bpy.context, types="brush")

        if mode == 'SCULPT':
            enum = has_brush.bl_rna.properties['sculpt_plane'].enum_items if \
                   has_brush else None
            path = "tool_settings.sculpt.brush.sculpt_plane"

        elif mode == 'VERTEX_PAINT':
            enum = has_brush.bl_rna.properties['vertex_tool'].enum_items if \
                   has_brush else None
            path = "tool_settings.vertex_paint.brush.vertex_tool"

        elif mode == 'WEIGHT_PAINT':
            enum = has_brush.bl_rna.properties['vertex_tool'].enum_items if \
                   has_brush else None
            path = "tool_settings.weight_paint.brush.vertex_tool"

        elif mode == 'TEXTURE_PAINT':
            enum = has_brush.bl_rna.properties['blend'].enum_items if \
                   has_brush else None
            path = "tool_settings.image_paint.brush.blend"

        else:
            enum = None
            path = ""

        return enum, path

    def draw(self, context):
        enum, path = self.init()
        layout = self.layout
        colum_n = utils_core.addon_settings(lists=False)

        layout.row().label(text="Brush Mode")
        layout.row().separator()

        if enum:
            if utils_core.get_mode() == 'TEXTURE_PAINT':
                column_flow = layout.column_flow(columns=colum_n)

                # add all the brush modes to the menu
                for brush in enum:
                    utils_core.menuprop(
                            column_flow.row(), brush.name,
                            brush.identifier, path, icon='RADIOBUT_OFF',
                            disable=True, disable_icon='RADIOBUT_ON'
                            )
            else:
                # add all the brush modes to the menu
                for brush in enum:
                    utils_core.menuprop(
                            layout.row(), brush.name,
                            brush.identifier, path, icon='RADIOBUT_OFF',
                            disable=True, disable_icon='RADIOBUT_ON'
                            )
        else:
            layout.row().label("No brushes available", icon="INFO")


class BrushAutosmoothMenu(Menu):
    bl_label = "Autosmooth"
    bl_idname = "VIEW3D_MT_sv3_brush_autosmooth_menu"

    def init(self):
        settings = (("1.0", 1.0),
                    ("0.7", 0.7),
                    ("0.5", 0.5),
                    ("0.3", 0.3),
                    ("0.2", 0.2),
                    ("0.1", 0.1))

        return settings

    def draw(self, context):
        settings = self.init()
        layout = self.layout
        has_brush = utils_core.get_brush_link(context, types="brush")

        if has_brush:
            # add the top slider
            layout.row().prop(has_brush, "auto_smooth_factor", slider=True)
            layout.row().separator()

            # add the rest of the menu items
            for i in range(len(settings)):
                utils_core.menuprop(
                        layout.row(), settings[i][0], settings[i][1],
                        "tool_settings.sculpt.brush.auto_smooth_factor",
                        icon='RADIOBUT_OFF', disable=True,
                        disable_icon='RADIOBUT_ON'
                        )
        else:
            layout.row().label("No Smooth options available", icon="INFO")


class BrushWeightMenu(Menu):
    bl_label = "Weight"
    bl_idname = "VIEW3D_MT_sv3_brush_weight_menu"

    def init(self):
        settings = (("1.0", 1.0),
                    ("0.7", 0.7),
                    ("0.5", 0.5),
                    ("0.3", 0.3),
                    ("0.2", 0.2),
                    ("0.1", 0.1))

        if utils_core.get_mode() == 'WEIGHT_PAINT':
            brush = bpy.context.tool_settings.unified_paint_settings
            brushstr = "tool_settings.unified_paint_settings.weight"
            name = "Weight"

        else:
            brush = bpy.context.tool_settings.image_paint.brush
            brushstr = "tool_settings.image_paint.brush.weight"
            name = "Mask Value"

        return settings, brush, brushstr, name

    def draw(self, context):
        settings, brush, brushstr, name = self.init()
        layout = self.layout

        if brush:
            # add the top slider
            layout.row().prop(brush, "weight", text=name, slider=True)
            layout.row().separator()

            # add the rest of the menu items
            for i in range(len(settings)):
                utils_core.menuprop(
                        layout.row(), settings[i][0], settings[i][1],
                        brushstr,
                        icon='RADIOBUT_OFF', disable=True,
                        disable_icon='RADIOBUT_ON'
                        )
        else:
            layout.row().label("No brush available", icon="INFO")


class ParticleCountMenu(Menu):
    bl_label = "Count"
    bl_idname = "VIEW3D_MT_sv3_particle_count_menu"

    def init(self):
        settings = (("50", 50),
                    ("25", 25),
                    ("10", 10),
                    ("5", 5),
                    ("3", 3),
                    ("1", 1))

        return settings

    def draw(self, context):
        settings = self.init()
        layout = self.layout

        # add the top slider
        layout.row().prop(context.tool_settings.particle_edit.brush,
                             "count", slider=True)
        layout.row().separator()

        # add the rest of the menu items
        for i in range(len(settings)):
            utils_core.menuprop(
                    layout.row(), settings[i][0], settings[i][1],
                    "tool_settings.particle_edit.brush.count",
                    icon='RADIOBUT_OFF', disable=True,
                    disable_icon='RADIOBUT_ON'
                    )


class ParticleLengthMenu(Menu):
    bl_label = "Length Mode"
    bl_idname = "VIEW3D_MT_sv3_particle_length_menu"

    def draw(self, context):
        layout = self.layout
        path = "tool_settings.particle_edit.brush.length_mode"

        # add the menu items
        for item in context.tool_settings.particle_edit.brush. \
          bl_rna.properties['length_mode'].enum_items:
            utils_core.menuprop(
                    layout.row(), item.name, item.identifier, path,
                    icon='RADIOBUT_OFF',
                    disable=True,
                    disable_icon='RADIOBUT_ON'
                    )


class ParticlePuffMenu(Menu):
    bl_label = "Puff Mode"
    bl_idname = "VIEW3D_MT_sv3_particle_puff_menu"

    def draw(self, context):
        layout = self.layout
        path = "tool_settings.particle_edit.brush.puff_mode"

        # add the menu items
        for item in context.tool_settings.particle_edit.brush. \
          bl_rna.properties['puff_mode'].enum_items:
            utils_core.menuprop(
                    layout.row(), item.name, item.identifier, path,
                    icon='RADIOBUT_OFF',
                    disable=True,
                    disable_icon='RADIOBUT_ON'
                    )


class FlipColorsAll(Operator):
    bl_label = "Flip Colors"
    bl_idname = "view3d.sv3_flip_colors_all"
    bl_description = "Switch between Foreground and Background colors"

    is_tex = BoolProperty(
                default=False,
                options={'HIDDEN'}
                )

    def execute(self, context):
        try:
            if self.is_tex is False:
                color = context.tool_settings.vertex_paint.brush.color
                secondary_color = context.tool_settings.vertex_paint.brush.secondary_color

                orig_prim = color.hsv
                orig_sec = secondary_color.hsv

                color.hsv = orig_sec
                secondary_color.hsv = orig_prim
            else:
                color = context.tool_settings.image_paint.brush.color
                secondary_color = context.tool_settings.image_paint.brush.secondary_color

                orig_prim = color.hsv
                orig_sec = secondary_color.hsv

                color.hsv = orig_sec
                secondary_color.hsv = orig_prim

            return {'FINISHED'}

        except Exception as e:
            utils_core.error_handlers(self, "view3d.sv3_flip_colors_all", e,
                                     "Flip Colors could not be completed")

            return {'CANCELLED'}


class ColorPickerPopup(Operator):
    bl_label = "Color"
    bl_idname = "view3d.sv3_color_picker_popup"
    bl_options = {'REGISTER'}

    @classmethod
    def poll(self, context):
        return utils_core.get_mode() in (
                        'VERTEX_PAINT',
                        'TEXTURE_PAINT'
                        )

    def check(self, context):
        return True

    def init(self):
        if utils_core.get_mode() == 'TEXTURE_PAINT':
            settings = bpy.context.tool_settings.image_paint
            brush = getattr(settings, "brush", None)
        else:
            settings = bpy.context.tool_settings.vertex_paint
            brush = settings.brush
            brush = getattr(settings, "brush", None)

        return settings, brush

    def draw(self, context):
        layout = self.layout
        settings, brush = self.init()


        if brush:
            layout.row().template_color_picker(brush, "color", value_slider=True)
            prim_sec_row = layout.row(align=True)
            prim_sec_row.prop(brush, "color", text="")
            prim_sec_row.prop(brush, "secondary_color", text="")

            if utils_core.get_mode() == 'VERTEX_PAINT':
                prim_sec_row.operator(
                                FlipColorsAll.bl_idname,
                                icon='FILE_REFRESH', text=""
                                ).is_tex = False
            else:
                prim_sec_row.operator(
                                FlipColorsAll.bl_idname,
                                icon='FILE_REFRESH', text=""
                                ).is_tex = True

            if settings.palette:
                layout.column().template_palette(settings, "palette", color=True)

            layout.row().template_ID(settings, "palette", new="palette.new")
        else:
            layout.row().label("No brushes currently available", icon="INFO")

            return

    def execute(self, context):
        return context.window_manager.invoke_popup(self, width=180)


class BrushMenuPopup(Operator):
    bl_label = "Color"
    bl_idname = "view3d.sv3_brush_menu_popup"
    bl_options = {'REGISTER'}

    @classmethod
    def poll(self, context):
        return utils_core.get_mode() in (
                        'VERTEX_PAINT',
                        'TEXTURE_PAINT',
                        'SCULPT',
                        'WEIGHT_PAINT'
                        )

    def check(self, context):
        return True

    def draw(self, context):
        layout = self.layout
        settings = UnifiedPaintPanel.paint_settings(context)
        colum_n = utils_core.addon_settings(lists=False)

        if utils_core.addon_settings(lists=True) != 'popup':
            layout.label(text="Seems no active brush", icon="INFO")
            layout.label(text="in the Tool Shelf", icon="BACK")

        layout.template_ID_preview(settings, "brush",
                                   new="brush.add", rows=3, cols=colum_n)

    def execute(self, context):
        return context.window_manager.invoke_popup(self, width=180)
