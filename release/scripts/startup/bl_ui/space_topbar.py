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
from bpy.types import Header, Menu, Panel
from .properties_grease_pencil_common import (
    GPENCIL_UL_layer,
)


class TOPBAR_HT_upper_bar(Header):
    bl_space_type = 'TOPBAR'

    def draw(self, context):
        region = context.region

        if region.alignment == 'RIGHT':
            self.draw_right(context)
        else:
            self.draw_left(context)

    def draw_left(self, context):
        layout = self.layout

        window = context.window
        screen = context.screen

        layout.operator("wm.splash", text="", icon='BLENDER', emboss=False)

        TOPBAR_MT_editor_menus.draw_collapsible(context, layout)

        layout.separator()

        if not screen.show_fullscreen:
            layout.template_ID_tabs(
                window, "workspace",
                new="workspace.add",
                menu="TOPBAR_MT_workspace_menu",
            )
        else:
            layout.operator(
                "screen.back_to_previous",
                icon='SCREEN_BACK',
                text="Back to Previous",
            )

    def draw_right(self, context):
        layout = self.layout

        window = context.window
        screen = context.screen
        scene = window.scene

        # If statusbar is hidden, still show messages at the top
        if not screen.show_statusbar:
            layout.template_reports_banner()
            layout.template_running_jobs()

        # Active workspace view-layer is retrieved through window, not through workspace.
        layout.template_ID(window, "scene", new="scene.new", unlink="scene.delete")

        row = layout.row(align=True)
        row.template_search(
            window, "view_layer",
            scene, "view_layers",
            new="scene.view_layer_add",
            unlink="scene.view_layer_remove")


class TOPBAR_HT_lower_bar(Header):
    bl_space_type = 'TOPBAR'
    bl_region_type = 'WINDOW'

    def draw(self, context):
        region = context.region

        if region.alignment == 'LEFT':
            self.draw_left(context)
        elif region.alignment == 'RIGHT':
            self.draw_right(context)
        else:
            self.draw_center(context)

    def draw_left(self, context):
        layout = self.layout

        # Active Tool
        # -----------
        from .space_toolsystem_common import ToolSelectPanelHelper
        tool = ToolSelectPanelHelper.draw_active_tool_header(context, layout)
        tool_space_type = 'VIEW_3D' if tool is None else tool.space_type
        tool_mode = context.mode if tool is None else tool.mode

        # Object Mode Options
        # -------------------

        # Example of how toolsettings can be accessed as pop-overs.

        # TODO(campbell): editing options should be after active tool options
        # (obviously separated for from the users POV)
        draw_fn = getattr(getattr(_draw_left_context_mode, tool_space_type, None), tool_mode, None)
        if draw_fn is not None:
            draw_fn(context, layout, tool)

        if tool_space_type == 'VIEW_3D':
            # Note: general mode options should be added to 'draw_right'.
            if tool_mode == 'SCULPT':
                if (tool is not None) and tool.has_datablock:
                    layout.popover_group(space_type='PROPERTIES', region_type='WINDOW', context=".paint_common", category="")
            elif tool_mode == 'PAINT_VERTEX':
                if (tool is not None) and tool.has_datablock:
                    layout.popover_group(space_type='PROPERTIES', region_type='WINDOW', context=".paint_common", category="")
            elif tool_mode == 'PAINT_WEIGHT':
                if (tool is not None) and tool.has_datablock:
                    layout.popover_group(space_type='PROPERTIES', region_type='WINDOW', context=".paint_common", category="")
            elif tool_mode == 'PAINT_TEXTURE':
                if (tool is not None) and tool.has_datablock:
                    layout.popover_group(space_type='PROPERTIES', region_type='WINDOW', context=".paint_common", category="")
            elif tool_mode == 'EDIT_ARMATURE':
                pass
            elif tool_mode == 'EDIT_CURVE':
                pass
            elif tool_mode == 'EDIT_MESH':
                pass
            elif tool_mode == 'POSE':
                pass
            elif tool_mode == 'PARTICLE':
                # Disable, only shows "Brush" panel, which is already in the top-bar.
                # if tool.has_datablock:
                #     layout.popover_group(space_type='PROPERTIES', region_type='WINDOW', context=".paint_common", category="")
                pass
            elif tool_mode == 'GPENCIL_PAINT':
                if (tool is not None) and tool.has_datablock:
                    layout.popover_group(space_type='PROPERTIES', region_type='WINDOW', context=".greasepencil_paint", category="")
            elif tool_mode == 'GPENCIL_SCULPT':
                layout.popover_group(space_type='PROPERTIES', region_type='WINDOW', context=".greasepencil_sculpt", category="")
            elif tool_mode == 'GPENCIL_WEIGHT':
                layout.popover_group(space_type='PROPERTIES', region_type='WINDOW', context=".greasepencil_weight", category="")
        elif tool_space_type == 'IMAGE_EDITOR':
            if tool_mode == 'PAINT':
                if (tool is not None) and tool.has_datablock:
                    layout.popover_group(space_type='PROPERTIES', region_type='WINDOW', context=".paint_common_2d", category="")
            elif context.uv_sculpt_object is not None:
                layout.popover_group(space_type='PROPERTIES', region_type='WINDOW', context=".uv_sculpt", category="")

    def draw_center(self, context):
        pass

    def draw_right(self, context):
        layout = self.layout

        # Active Tool
        # -----------
        from .space_toolsystem_common import ToolSelectPanelHelper
        tool = ToolSelectPanelHelper.tool_active_from_context(context)
        tool_space_type = 'VIEW_3D' if tool is None else tool.space_type
        tool_mode = context.mode if tool is None else tool.mode

        if tool_space_type == 'VIEW_3D':
            if tool_mode == 'SCULPT':
                layout.popover_group(space_type='PROPERTIES', region_type='WINDOW', context=".sculpt_mode", category="")
            elif tool_mode == 'PAINT_VERTEX':
                layout.popover_group(space_type='PROPERTIES', region_type='WINDOW', context=".vertexpaint", category="")
            elif tool_mode == 'PAINT_WEIGHT':
                layout.popover_group(space_type='PROPERTIES', region_type='WINDOW', context=".weightpaint", category="")
            elif tool_mode == 'PAINT_TEXTURE':
                layout.popover_group(space_type='PROPERTIES', region_type='WINDOW', context=".imagepaint", category="")
            elif tool_mode == 'EDIT_TEXT':
                layout.popover_group(space_type='PROPERTIES', region_type='WINDOW', context=".text_edit", category="")
            elif tool_mode == 'EDIT_ARMATURE':
                layout.popover_group(space_type='PROPERTIES', region_type='WINDOW', context=".armature_edit", category="")
            elif tool_mode == 'EDIT_METABALL':
                layout.popover_group(space_type='PROPERTIES', region_type='WINDOW', context=".mball_edit", category="")
            elif tool_mode == 'EDIT_LATTICE':
                layout.popover_group(space_type='PROPERTIES', region_type='WINDOW', context=".lattice_edit", category="")
            elif tool_mode == 'EDIT_CURVE':
                layout.popover_group(space_type='PROPERTIES', region_type='WINDOW', context=".curve_edit", category="")
            elif tool_mode == 'EDIT_MESH':
                layout.popover_group(space_type='PROPERTIES', region_type='WINDOW', context=".mesh_edit", category="")
            elif tool_mode == 'POSE':
                layout.popover_group(space_type='PROPERTIES', region_type='WINDOW', context=".posemode", category="")
            elif tool_mode == 'PARTICLE':
                layout.popover_group(space_type='PROPERTIES', region_type='WINDOW', context=".particlemode", category="")
            elif tool_mode == 'OBJECT':
                layout.popover_group(space_type='PROPERTIES', region_type='WINDOW', context=".objectmode", category="")
            elif tool_mode in {'GPENCIL_PAINT', 'GPENCIL_EDIT', 'GPENCIL_SCULPT', 'GPENCIL_WEIGHT'}:
                # Grease pencil layer.
                gpl = context.active_gpencil_layer
                if gpl and gpl.info is not None:
                    text = gpl.info
                    maxw = 25
                    if len(text) > maxw:
                        text = text[:maxw - 5] + '..' + text[-3:]
                else:
                    text = ""

                layout.label(text="Layer:")
                sub = layout.row()
                sub.ui_units_x = 8
                sub.popover(
                    panel="TOPBAR_PT_gpencil_layers",
                    text=text,
                )
        elif tool_space_type == 'IMAGE_EDITOR':
            if tool_mode == 'PAINT':
                layout.popover_group(space_type='PROPERTIES', region_type='WINDOW', context=".imagepaint_2d", category="")


class _draw_left_context_mode:
    class VIEW_3D:
        @staticmethod
        def SCULPT(context, layout, tool):
            if (tool is None) or (not tool.has_datablock):
                return

            paint = context.tool_settings.sculpt
            layout.template_ID_preview(paint, "brush", rows=3, cols=8, hide_buttons=True)

            brush = paint.brush
            if brush is None:
                return

            from .properties_paint_common import UnifiedPaintPanel

            UnifiedPaintPanel.prop_unified_size(layout, context, brush, "size", slider=True, text="Radius")
            UnifiedPaintPanel.prop_unified_strength(layout, context, brush, "strength", slider=True, text="Strength")
            layout.prop(brush, "direction", text="", expand=True)

        @staticmethod
        def PAINT_TEXTURE(context, layout, tool):
            if (tool is None) or (not tool.has_datablock):
                return

            paint = context.tool_settings.image_paint
            layout.template_ID_preview(paint, "brush", rows=3, cols=8, hide_buttons=True)

            brush = paint.brush
            if brush is None:
                return

            from .properties_paint_common import UnifiedPaintPanel

            UnifiedPaintPanel.prop_unified_color(layout, context, brush, "color", text="")
            UnifiedPaintPanel.prop_unified_size(layout, context, brush, "size", slider=True, text="Radius")
            UnifiedPaintPanel.prop_unified_strength(layout, context, brush, "strength", slider=True, text="Strength")

        @staticmethod
        def PAINT_VERTEX(context, layout, tool):
            if (tool is None) or (not tool.has_datablock):
                return

            paint = context.tool_settings.vertex_paint
            layout.template_ID_preview(paint, "brush", rows=3, cols=8, hide_buttons=True)

            brush = paint.brush
            if brush is None:
                return

            from .properties_paint_common import UnifiedPaintPanel

            UnifiedPaintPanel.prop_unified_color(layout, context, brush, "color", text="")
            UnifiedPaintPanel.prop_unified_size(layout, context, brush, "size", slider=True, text="Radius")
            UnifiedPaintPanel.prop_unified_strength(layout, context, brush, "strength", slider=True, text="Strength")

        @staticmethod
        def PAINT_WEIGHT(context, layout, tool):
            if (tool is None) or (not tool.has_datablock):
                return

            paint = context.tool_settings.weight_paint
            layout.template_ID_preview(paint, "brush", rows=3, cols=8, hide_buttons=True)
            brush = paint.brush
            if brush is None:
                return

            from .properties_paint_common import UnifiedPaintPanel

            UnifiedPaintPanel.prop_unified_weight(layout, context, brush, "weight", slider=True, text="Weight")
            UnifiedPaintPanel.prop_unified_size(layout, context, brush, "size", slider=True, text="Radius")
            UnifiedPaintPanel.prop_unified_strength(layout, context, brush, "strength", slider=True, text="Strength")

        @staticmethod
        def GPENCIL_PAINT(context, layout, tool):
            if tool is None:
                return

            is_paint = True
            if (tool.name in {"Line", "Box", "Circle"}):
                is_paint = False
            elif (not tool.has_datablock):
                return

            paint = context.tool_settings.gpencil_paint
            brush = paint.brush
            if brush is None:
                return

            gp_settings = brush.gpencil_settings

            def draw_color_selector():
                ma = gp_settings.material
                row = layout.row(align=True)

                icon_id = 0
                if ma:
                    icon_id = ma.id_data.preview.icon_id
                    txt_ma = ma.name
                    maxw = 25
                    if len(txt_ma) > maxw:
                        txt_ma = txt_ma[:maxw - 5] + '..' + txt_ma[-3:]
                else:
                    txt_ma = ""

                row.label(text="Material:")
                sub = row.row()
                sub.ui_units_x = 8
                sub.popover(
                    panel="TOPBAR_PT_gpencil_materials",
                    text=txt_ma,
                    icon_value=icon_id,
                )

                row.prop(gp_settings, "use_material_pin", text="")

            row = layout.row(align=True)
            ts = context.scene.tool_settings
            settings = ts.gpencil_paint
            row.template_ID_preview(settings, "brush", rows=3, cols=8, hide_buttons=True)

            if brush.gpencil_tool == 'ERASE':
                row = layout.row(align=True)
                row.prop(brush, "size", text="Radius")
                row.prop(gp_settings, "use_pressure", text="", icon='STYLUS_PRESSURE')
                if gp_settings.eraser_mode == 'SOFT':
                    row = layout.row(align=True)
                    row.prop(gp_settings, "pen_strength", slider=True)
                    row.prop(gp_settings, "use_strength_pressure", text="", icon='STYLUS_PRESSURE')
            elif brush.gpencil_tool == 'FILL':
                row = layout.row()
                row.prop(gp_settings, "fill_leak", text="Leak Size")
                row.prop(brush, "size", text="Thickness")
                row.prop(gp_settings, "fill_simplify_level", text="Simplify")

                draw_color_selector()

                row = layout.row(align=True)
                row.prop(gp_settings, "fill_draw_mode", text="")
                row.prop(gp_settings, "show_fill_boundary", text="", icon='GRID')

            else:  # brush.gpencil_tool == 'DRAW':
                row = layout.row(align=True)
                row.prop(brush, "size", text="Radius")
                if is_paint:
                    row.prop(gp_settings, "use_pressure", text="", icon='STYLUS_PRESSURE')
                row = layout.row(align=True)
                row.prop(gp_settings, "pen_strength", slider=True)
                if is_paint:
                    row.prop(gp_settings, "use_strength_pressure", text="", icon='STYLUS_PRESSURE')

                draw_color_selector()

        @staticmethod
        def GPENCIL_SCULPT(context, layout, tool):
            if (tool is None) or (not tool.has_datablock):
                return
            tool_settings = context.tool_settings
            settings = tool_settings.gpencil_sculpt
            tool = settings.sculpt_tool
            brush = settings.brush

            layout.prop(brush, "size", slider=True)

            row = layout.row(align=True)
            row.prop(brush, "strength", slider=True)
            row.prop(brush, "use_pressure_strength", text="")

            if tool in {'THICKNESS', 'STRENGTH', 'PINCH', 'TWIST'}:
                row.separator()
                row.prop(brush, "direction", expand=True, text="")

        @staticmethod
        def GPENCIL_WEIGHT(context, layout, tool):
            if (tool is None) or (not tool.has_datablock):
                return
            tool_settings = context.tool_settings
            settings = tool_settings.gpencil_sculpt
            brush = settings.brush

            layout.prop(brush, "size", slider=True)

            row = layout.row(align=True)
            row.prop(brush, "strength", slider=True)
            row.prop(brush, "use_pressure_strength", text="")

            layout.prop(brush, "target_weight", slider=True)

        @staticmethod
        def PARTICLE(context, layout, tool):
            # See: 'VIEW3D_PT_tools_brush', basically a duplicate
            settings = context.tool_settings.particle_edit
            brush = settings.brush
            tool = settings.tool
            if tool != 'NONE':
                layout.prop(brush, "size", slider=True)
                if tool == 'ADD':
                    layout.prop(brush, "count")

                    layout.prop(settings, "use_default_interpolate")
                    layout.prop(brush, "steps", slider=True)
                    layout.prop(settings, "default_key_count", slider=True)
                else:
                    layout.prop(brush, "strength", slider=True)

                    if tool == 'LENGTH':
                        layout.row().prop(brush, "length_mode", expand=True)
                    elif tool == 'PUFF':
                        layout.row().prop(brush, "puff_mode", expand=True)
                        layout.prop(brush, "use_puff_volume")
                    elif tool == 'COMB':
                        # Note: actually in 'Options' panel,
                        # disabled when used in popover.
                        row = layout.row()
                        row.active = settings.is_editable
                        row.prop(settings, "use_emitter_deflect", text="Deflect Emitter")
                        sub = row.row(align=True)
                        sub.active = settings.use_emitter_deflect
                        sub.prop(settings, "emitter_distance", text="Distance")

    class IMAGE_EDITOR:
        @staticmethod
        def VIEW(context, layout, tool):
            tool_settings = context.tool_settings
            if tool_settings.use_uv_sculpt:
                if context.mode == 'EDIT_MESH':
                    uv_sculpt = tool_settings.uv_sculpt
                    brush = uv_sculpt.brush
                    if brush:
                        from .properties_paint_common import UnifiedPaintPanel

                        row = layout.row(align=True)
                        UnifiedPaintPanel.prop_unified_size(row, context, brush, "size", slider=True, text="Radius")
                        UnifiedPaintPanel.prop_unified_size(row, context, brush, "use_pressure_size")

                        row = layout.row(align=True)
                        UnifiedPaintPanel.prop_unified_strength(row, context, brush, "strength", slider=True, text="Strength")
                        UnifiedPaintPanel.prop_unified_strength(row, context, brush, "use_pressure_strength")

        @staticmethod
        def PAINT(context, layout, tool):
            if (tool is None) or (not tool.has_datablock):
                return

            paint = context.tool_settings.image_paint
            layout.template_ID_preview(paint, "brush", rows=3, cols=8, hide_buttons=True)

            brush = paint.brush
            if brush is None:
                return

            from .properties_paint_common import UnifiedPaintPanel

            UnifiedPaintPanel.prop_unified_color(layout, context, brush, "color", text="")
            UnifiedPaintPanel.prop_unified_size(layout, context, brush, "size", slider=True, text="Radius")
            UnifiedPaintPanel.prop_unified_strength(layout, context, brush, "strength", slider=True, text="Strength")


class TOPBAR_PT_gpencil_layers(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'HEADER'
    bl_label = "Layers"
    bl_ui_units_x = 14

    @classmethod
    def poll(cls, context):
        if context.gpencil_data is None:
            return False

        ob = context.object
        if ob is not None and ob.type == 'GPENCIL':
            return True

        return False

    @staticmethod
    def draw(self, context):
        layout = self.layout
        gpd = context.gpencil_data

        # Grease Pencil data...
        if (gpd is None) or (not gpd.layers):
            layout.operator("gpencil.layer_add", text="New Layer")
        else:
            self.draw_layers(context, layout, gpd)

    def draw_layers(self, context, layout, gpd):
        row = layout.row()

        col = row.column()
        layer_rows = 10
        col.template_list("GPENCIL_UL_layer", "", gpd, "layers", gpd.layers, "active_index",
                          rows=layer_rows, reverse=True)

        gpl = context.active_gpencil_layer
        if gpl:
            srow = col.row(align=True)
            srow.prop(gpl, "blend_mode", text="Blend")

            srow = col.row(align=True)
            srow.prop(gpl, "opacity", text="Opacity", slider=True)
            srow.prop(gpl, "clamp_layer", text="",
                     icon='MOD_MASK' if gpl.clamp_layer else 'ONIONSKIN_OFF')

        col = row.column()

        sub = col.column(align=True)
        sub.operator("gpencil.layer_add", icon='ADD', text="")
        sub.operator("gpencil.layer_remove", icon='REMOVE', text="")

        gpl = context.active_gpencil_layer
        if gpl:
            sub.menu("GPENCIL_MT_layer_specials", icon='DOWNARROW_HLT', text="")

            if len(gpd.layers) > 1:
                col.separator()

                sub = col.column(align=True)
                sub.operator("gpencil.layer_move", icon='TRIA_UP', text="").type = 'UP'
                sub.operator("gpencil.layer_move", icon='TRIA_DOWN', text="").type = 'DOWN'

                col.separator()

                sub = col.column(align=True)
                sub.operator("gpencil.layer_isolate", icon='LOCKED', text="").affect_visibility = False
                sub.operator("gpencil.layer_isolate", icon='HIDE_OFF', text="").affect_visibility = True


class TOPBAR_MT_editor_menus(Menu):
    bl_idname = "TOPBAR_MT_editor_menus"
    bl_label = ""

    def draw(self, context):
        self.draw_menus(self.layout, context)

    @staticmethod
    def draw_menus(layout, context):
        layout.menu("TOPBAR_MT_file")
        layout.menu("TOPBAR_MT_edit")

        layout.menu("TOPBAR_MT_render")

        layout.menu("TOPBAR_MT_window")
        layout.menu("TOPBAR_MT_help")


class TOPBAR_MT_file(Menu):
    bl_label = "File"

    def draw(self, context):
        layout = self.layout

        layout.operator_context = 'INVOKE_AREA'
        layout.menu("TOPBAR_MT_file_new", text="New", icon='FILE_NEW')
        layout.operator("wm.open_mainfile", text="Open...", icon='FILE_FOLDER')
        layout.menu("TOPBAR_MT_file_open_recent")
        layout.operator("wm.revert_mainfile")
        layout.operator("wm.recover_last_session")
        layout.operator("wm.recover_auto_save", text="Recover Auto Save...")

        layout.separator()

        layout.operator_context = 'EXEC_AREA' if context.blend_data.is_saved else 'INVOKE_AREA'
        layout.operator("wm.save_mainfile", text="Save", icon='FILE_TICK')

        layout.operator_context = 'INVOKE_AREA'
        layout.operator("wm.save_as_mainfile", text="Save As...")
        layout.operator_context = 'INVOKE_AREA'
        layout.operator("wm.save_as_mainfile", text="Save Copy...").copy = True

        layout.separator()
        layout.operator_context = 'INVOKE_AREA'

        if any(bpy.utils.app_template_paths()):
            app_template = context.user_preferences.app_template
        else:
            app_template = None

        if app_template:
            layout.label(text=bpy.path.display_name(app_template))
            layout.operator("wm.save_homefile")
            layout.operator(
                "wm.read_factory_settings",
                text="Load Factory Settings",
            ).app_template = app_template
        else:
            layout.operator("wm.save_homefile")
            layout.operator("wm.read_factory_settings")

        layout.separator()

        layout.operator("wm.app_template_install", text="Install Application Template...")

        layout.separator()

        layout.operator_context = 'INVOKE_AREA'
        layout.operator("wm.link", text="Link...", icon='LINK_BLEND')
        layout.operator("wm.append", text="Append...", icon='APPEND_BLEND')
        layout.menu("TOPBAR_MT_file_previews")

        layout.separator()

        layout.menu("TOPBAR_MT_file_import", icon='IMPORT')
        layout.menu("TOPBAR_MT_file_export", icon='EXPORT')

        layout.separator()

        layout.menu("TOPBAR_MT_file_external_data")

        layout.separator()

        layout.operator_context = 'EXEC_AREA'
        if bpy.data.is_dirty and context.user_preferences.view.use_quit_dialog:
            layout.operator_context = 'INVOKE_SCREEN'  # quit dialog
        layout.operator("wm.quit_blender", text="Quit", icon='QUIT')


class TOPBAR_MT_file_new(Menu):
    bl_label = "New File"

    @staticmethod
    def app_template_paths():
        import os

        template_paths = bpy.utils.app_template_paths()

        # expand template paths
        app_templates = []
        for path in template_paths:
            for d in os.listdir(path):
                if d.startswith(("__", ".")):
                    continue
                template = os.path.join(path, d)
                if os.path.isdir(template):
                    # template_paths_expand.append(template)
                    app_templates.append(d)

        return sorted(app_templates)

    def draw_ex(layout, context, *, use_splash=False, use_more=False):
        layout.operator_context = 'EXEC_DEFAULT'

        # Limit number of templates in splash screen, spill over into more menu.
        paths = TOPBAR_MT_file_new.app_template_paths()
        splash_limit = 5

        if use_splash:
            icon = 'FILE_NEW'
            show_more = len(paths) > (splash_limit - 1)
            if show_more:
                paths = paths[:splash_limit - 2]
        elif use_more:
            icon = 'FILE_NEW'
            paths = paths[splash_limit - 2:]
            show_more = False
        else:
            icon = 'NONE'
            show_more = False

        # Draw application templates.
        if not use_more:
            props = layout.operator("wm.read_homefile", text="General", icon=icon)
            props.app_template = ""

        for d in paths:
            props = layout.operator(
                "wm.read_homefile",
                text=bpy.path.display_name(d),
                icon=icon,
            )
            props.app_template = d

        if show_more:
            layout.menu("TOPBAR_MT_templates_more", text="...")

    def draw(self, context):
        TOPBAR_MT_file_new.draw_ex(self.layout, context)


class TOPBAR_MT_templates_more(Menu):
    bl_label = "Templates"

    def draw(self, context):
        bpy.types.TOPBAR_MT_file_new.draw_ex(self.layout, context, use_more=True)


class TOPBAR_MT_file_import(Menu):
    bl_idname = "TOPBAR_MT_file_import"
    bl_label = "Import"

    def draw(self, context):
        if bpy.app.build_options.collada:
            self.layout.operator("wm.collada_import", text="Collada (Default) (.dae)")
        if bpy.app.build_options.alembic:
            self.layout.operator("wm.alembic_import", text="Alembic (.abc)")


class TOPBAR_MT_file_export(Menu):
    bl_idname = "TOPBAR_MT_file_export"
    bl_label = "Export"

    def draw(self, context):
        if bpy.app.build_options.collada:
            self.layout.operator("wm.collada_export", text="Collada (Default) (.dae)")
        if bpy.app.build_options.alembic:
            self.layout.operator("wm.alembic_export", text="Alembic (.abc)")


class TOPBAR_MT_file_external_data(Menu):
    bl_label = "External Data"

    def draw(self, context):
        layout = self.layout

        icon = 'CHECKBOX_HLT' if bpy.data.use_autopack else 'CHECKBOX_DEHLT'
        layout.operator("file.autopack_toggle", icon=icon)

        layout.separator()

        pack_all = layout.row()
        pack_all.operator("file.pack_all")
        pack_all.active = not bpy.data.use_autopack

        unpack_all = layout.row()
        unpack_all.operator("file.unpack_all")
        unpack_all.active = not bpy.data.use_autopack

        layout.separator()

        layout.operator("file.make_paths_relative")
        layout.operator("file.make_paths_absolute")
        layout.operator("file.report_missing_files")
        layout.operator("file.find_missing_files")


class TOPBAR_MT_file_previews(Menu):
    bl_label = "Data Previews"

    def draw(self, context):
        layout = self.layout

        layout.operator("wm.previews_ensure")
        layout.operator("wm.previews_batch_generate")

        layout.separator()

        layout.operator("wm.previews_clear")
        layout.operator("wm.previews_batch_clear")


class TOPBAR_MT_render(Menu):
    bl_label = "Render"

    def draw(self, context):
        layout = self.layout

        rd = context.scene.render

        layout.operator("render.render", text="Render Image", icon='RENDER_STILL').use_viewport = True
        props = layout.operator("render.render", text="Render Animation", icon='RENDER_ANIMATION')
        props.animation = True
        props.use_viewport = True

        layout.separator()

        layout.operator("sound.mixdown", text="Render Audio...")

        layout.separator()

        layout.operator("render.view_show", text="View Render")
        layout.operator("render.play_rendered_anim", text="View Animation")
        layout.prop_menu_enum(rd, "display_mode", text="Display Mode")

        layout.separator()

        layout.prop(rd, "use_lock_interface", text="Lock Interface")


class TOPBAR_MT_edit(Menu):
    bl_label = "Edit"

    def draw(self, context):
        layout = self.layout

        layout.operator("ed.undo")
        layout.operator("ed.redo")

        layout.separator()

        layout.operator("ed.undo_history", text="Undo History...")

        layout.separator()

        layout.operator("screen.repeat_last")
        layout.operator("screen.repeat_history", text="Repeat History...")

        layout.separator()

        layout.operator("screen.redo_last", text="Adjust Last Operation...")

        layout.separator()

        layout.operator("wm.search_menu", text="Operator Search...", icon='VIEWZOOM')

        layout.separator()

        # Should move elsewhere (impacts outliner & 3D view).
        tool_settings = context.tool_settings
        layout.prop(tool_settings, "lock_object_mode")

        layout.separator()

        layout.operator("screen.settings_show", text="Settings...", icon='PREFERENCES')


class TOPBAR_MT_window(Menu):
    bl_label = "Window"

    def draw(self, context):
        import sys

        layout = self.layout

        layout.operator("wm.window_new")
        layout.operator("wm.window_new_main")

        layout.separator()

        layout.operator("wm.window_fullscreen_toggle", icon='FULLSCREEN_ENTER')

        layout.separator()

        layout.operator("screen.workspace_cycle", text="Next Workspace").direction = 'NEXT'
        layout.operator("screen.workspace_cycle", text="Previous Workspace").direction = 'PREV'

        layout.separator()

        layout.prop(context.screen, "show_topbar")
        layout.prop(context.screen, "show_statusbar")

        layout.separator()

        layout.operator("screen.screenshot")

        if sys.platform[:3] == "win":
            layout.separator()
            layout.operator("wm.console_toggle", icon='CONSOLE')

        if context.scene.render.use_multiview:
            layout.separator()
            layout.operator("wm.set_stereo_3d")


class TOPBAR_MT_help(Menu):
    bl_label = "Help"

    def draw(self, context):
        layout = self.layout

        show_developer = context.user_preferences.view.show_developer_ui

        layout.operator(
            "wm.url_open", text="Manual", icon='HELP',
        ).url = "https://docs.blender.org/manual/en/dev/"

        layout.operator(
            "wm.url_open", text="Report a Bug", icon='URL',
        ).url = "https://developer.blender.org/maniphest/task/edit/form/1"

        layout.separator()

        layout.operator(
            "wm.url_open", text="User Communities", icon='URL',
        ).url = "https://www.blender.org/community/"
        layout.operator(
            "wm.url_open", text="Developer Community", icon='URL',
        ).url = "https://www.blender.org/get-involved/developers/"

        layout.separator()

        layout.operator(
            "wm.url_open", text="Blender Website", icon='URL',
        ).url = "https://www.blender.org"
        layout.operator(
            "wm.url_open", text="Release Notes", icon='URL',
        ).url = "https://www.blender.org/download/releases/%d-%d/" % bpy.app.version[:2]
        layout.operator(
            "wm.url_open", text="Credits", icon='URL',
        ).url = "https://www.blender.org/about/credits/"

        layout.separator()

        layout.operator(
            "wm.url_open", text="Blender Store", icon='URL',
        ).url = "https://store.blender.org"
        layout.operator(
            "wm.url_open", text="Development Fund", icon='URL'
        ).url = "https://fund.blender.org"
        layout.operator(
            "wm.url_open", text="Donate", icon='URL',
        ).url = "https://www.blender.org/foundation/donation-payment/"

        layout.separator()

        if show_developer:
            layout.operator(
                "wm.url_open", text="Python API Reference", icon='URL',
            ).url = bpy.types.WM_OT_doc_view._prefix

            layout.operator("wm.operator_cheat_sheet", icon='TEXT')

        layout.operator("wm.sysinfo")

        layout.separator()

        layout.operator("wm.splash", icon='BLENDER')


class TOPBAR_MT_file_specials(Menu):
    bl_label = "File Context Menu"

    def draw(self, context):
        layout = self.layout

        layout.operator_context = 'INVOKE_AREA'
        layout.operator("wm.read_homefile", text="New", icon='FILE_NEW')
        layout.operator("wm.open_mainfile", text="Open...", icon='FILE_FOLDER')

        layout.separator()

        layout.operator("wm.link", text="Link...", icon='LINK_BLEND')
        layout.operator("wm.append", text="Append...", icon='APPEND_BLEND')

        layout.separator()

        layout.menu("TOPBAR_MT_file_import", icon='IMPORT')
        layout.menu("TOPBAR_MT_file_export", icon='EXPORT')


class TOPBAR_MT_window_specials(Menu):
    bl_label = "Window Context Menu"

    def draw(self, context):
        layout = self.layout

        layout.operator_context = 'EXEC_AREA'

        layout.operator("wm.window_new")
        layout.operator("wm.window_new_main")

        layout.operator_context = 'INVOKE_AREA'

        layout.operator("screen.area_dupli", icon='DUPLICATE')

        layout.separator()

        layout.operator("screen.area_split", text="Horizontal Split").direction = 'HORIZONTAL'
        layout.operator("screen.area_split", text="Vertical Split").direction = 'VERTICAL'

        layout.separator()

        layout.operator("wm.window_fullscreen_toggle", icon='FULLSCREEN_ENTER')

        layout.separator()

        layout.operator("screen.settings_show", text="Settings...", icon='PREFERENCES')


class TOPBAR_MT_workspace_menu(Menu):
    bl_label = "Workspace"

    def draw(self, context):
        layout = self.layout

        layout.operator("workspace.duplicate", text="Duplicate", icon='DUPLICATE')
        if len(bpy.data.workspaces) > 1:
            layout.operator("workspace.delete", text="Delete", icon='REMOVE')

        layout.separator()

        layout.operator("workspace.reorder_to_front", text="Reorder to Front", icon='TRIA_LEFT_BAR')
        layout.operator("workspace.reorder_to_back", text="Reorder to Back", icon='TRIA_RIGHT_BAR')


class TOPBAR_PT_active_tool(Panel):
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_category = ""
    bl_context = ".active_tool"  # dot on purpose (access from tool settings)
    bl_label = "Active Tool"
    bl_options = {'HIDE_HEADER'}

    def draw(self, context):
        layout = self.layout

        # Panel display of topbar tool settings.
        # currently displays in tool settings, keep here since the same functionality is used for the topbar.

        layout.use_property_split = True
        layout.use_property_decorate = False

        from .space_toolsystem_common import ToolSelectPanelHelper
        ToolSelectPanelHelper.draw_active_tool_header(context, layout, show_tool_name=True)


classes = (
    TOPBAR_HT_upper_bar,
    TOPBAR_HT_lower_bar,
    TOPBAR_MT_file_specials,
    TOPBAR_MT_window_specials,
    TOPBAR_MT_workspace_menu,
    TOPBAR_MT_editor_menus,
    TOPBAR_MT_file,
    TOPBAR_MT_file_new,
    TOPBAR_MT_templates_more,
    TOPBAR_MT_file_import,
    TOPBAR_MT_file_export,
    TOPBAR_MT_file_external_data,
    TOPBAR_MT_file_previews,
    TOPBAR_MT_edit,
    TOPBAR_MT_render,
    TOPBAR_MT_window,
    TOPBAR_MT_help,
    TOPBAR_PT_active_tool,
    TOPBAR_PT_gpencil_layers,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
