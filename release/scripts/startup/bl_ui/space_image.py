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

from bpy.types import (
    Header,
    Menu,
    Panel,
    UIList,
)
from bl_ui.properties_paint_common import (
    UnifiedPaintPanel,
    brush_texture_settings,
    brush_basic_texpaint_settings,
    brush_settings,
    brush_settings_advanced,
    draw_color_settings,
    ClonePanel,
    BrushSelectPanel,
    TextureMaskPanel,
    ColorPalettePanel,
    StrokePanel,
    SmoothStrokePanel,
    FalloffPanel,
    DisplayPanel,
)
from bl_ui.properties_grease_pencil_common import (
    AnnotationDataPanel,
)
from bl_ui.space_toolsystem_common import (
    ToolActivePanelHelper,
)

from bpy.app.translations import pgettext_iface as iface_


class ImagePaintPanel:
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'UI'


class BrushButtonsPanel(UnifiedPaintPanel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'UI'

    @classmethod
    def poll(cls, context):
        tool_settings = context.tool_settings.image_paint
        return tool_settings.brush


class IMAGE_PT_active_tool(Panel, ToolActivePanelHelper):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'UI'
    bl_category = "Tool"


class IMAGE_MT_view(Menu):
    bl_label = "View"

    def draw(self, context):
        layout = self.layout

        sima = context.space_data
        uv = sima.uv_editor
        tool_settings = context.tool_settings
        paint = tool_settings.image_paint

        show_uvedit = sima.show_uvedit
        show_render = sima.show_render

        layout.prop(sima, "show_region_toolbar")
        layout.prop(sima, "show_region_ui")
        layout.prop(sima, "show_region_tool_header")
        layout.prop(sima, "show_region_hud")

        layout.separator()

        layout.prop(sima, "use_realtime_update")
        layout.prop(uv, "show_metadata")

        if paint.brush and (context.image_paint_object or sima.mode == 'PAINT'):
            layout.prop(uv, "show_texpaint")
            layout.prop(tool_settings, "show_uv_local_view", text="Show Same Material")

        layout.separator()

        layout.operator("image.view_zoom_in")
        layout.operator("image.view_zoom_out")

        layout.separator()

        layout.menu("IMAGE_MT_view_zoom")

        layout.separator()

        if show_uvedit:
            layout.operator("image.view_selected", text="Frame Selected")

        layout.operator("image.view_all")
        layout.operator("image.view_all", text="Frame All Fit").fit_view = True

        layout.operator("image.view_center_cursor", text="Center View to Cursor")

        layout.separator()

        if show_render:
            layout.operator("image.render_border")
            layout.operator("image.clear_render_border")

            layout.separator()

            layout.operator("image.cycle_render_slot", text="Render Slot Cycle Next")
            layout.operator("image.cycle_render_slot", text="Render Slot Cycle Previous").reverse = True
            layout.separator()

        layout.menu("INFO_MT_area")


class IMAGE_MT_view_zoom(Menu):
    bl_label = "Fractional Zoom"

    def draw(self, _context):
        layout = self.layout

        ratios = ((1, 8), (1, 4), (1, 2), (1, 1), (2, 1), (4, 1), (8, 1))

        for i, (a, b) in enumerate(ratios):
            if i in {3, 4}:  # Draw separators around Zoom 1:1.
                layout.separator()

            layout.operator(
                "image.view_zoom_ratio",
                text=iface_("Zoom %d:%d") % (a, b),
                translate=False,
            ).ratio = a / b


class IMAGE_MT_select(Menu):
    bl_label = "Select"

    def draw(self, _context):
        layout = self.layout

        layout.operator("uv.select_all", text="All").action = 'SELECT'
        layout.operator("uv.select_all", text="None").action = 'DESELECT'
        layout.operator("uv.select_all", text="Invert").action = 'INVERT'

        layout.separator()

        layout.operator("uv.select_box").pinned = False
        layout.operator("uv.select_box", text="Box Select Pinned").pinned = True
        layout.operator("uv.select_circle")

        layout.separator()

        layout.operator("uv.select_less", text="Less")
        layout.operator("uv.select_more", text="More")

        layout.separator()

        layout.operator("uv.select_pinned")
        layout.menu("IMAGE_MT_select_linked")

        layout.separator()

        layout.operator("uv.select_split")
        layout.operator("uv.select_overlap")


class IMAGE_MT_select_linked(Menu):
    bl_label = "Select Linked"

    def draw(self, _context):
        layout = self.layout

        layout.operator("uv.select_linked", text="Linked")
        layout.operator("uv.shortest_path_select", text="Shortest Path")


class IMAGE_MT_image(Menu):
    bl_label = "Image"

    def draw(self, context):
        layout = self.layout

        sima = context.space_data
        ima = sima.image
        show_render = sima.show_render

        layout.operator("image.new", text="New")
        layout.operator("image.open", text="Open...", icon='FILE_FOLDER')

        layout.operator("image.read_viewlayers")

        if ima:
            layout.separator()

            if not show_render:
                layout.operator("image.replace", text="Replace...")
                layout.operator("image.reload", text="Reload")

            layout.operator("image.external_edit", text="Edit Externally")

        layout.separator()

        if ima:
            layout.operator("image.save", text="Save", icon='FILE_TICK')
            layout.operator("image.save_as", text="Save As...")
            layout.operator("image.save_as", text="Save a Copy...").copy = True

        if ima and ima.source == 'SEQUENCE':
            layout.operator("image.save_sequence")

        layout.operator("image.save_all_modified", text="Save All Images")

        if ima:
            layout.separator()

            layout.menu("IMAGE_MT_image_invert")
            layout.operator("image.resize", text="Resize")
            layout.menu("IMAGE_MT_image_flip")

        if ima and not show_render:
            if ima.packed_file:
                if ima.filepath:
                    layout.separator()
                    layout.operator("image.unpack", text="Unpack")
            else:
                layout.separator()
                layout.operator("image.pack", text="Pack")

        if ima:
            layout.separator()
            layout.operator("palette.extract_from_image", text="Extract Palette")
            layout.operator("gpencil.image_to_grease_pencil", text="Generate Grease Pencil")

class IMAGE_MT_image_flip(Menu):
    bl_label = "Flip"

    def draw(self, _context):
        layout = self.layout
        layout.operator("image.flip", text="Horizontally").use_flip_horizontal = True
        layout.operator("image.flip", text="Vertically").use_flip_vertical = True

class IMAGE_MT_image_invert(Menu):
    bl_label = "Invert"

    def draw(self, _context):
        layout = self.layout

        props = layout.operator("image.invert", text="Invert Image Colors", icon='IMAGE_RGB')
        props.invert_r = True
        props.invert_g = True
        props.invert_b = True

        layout.separator()

        layout.operator("image.invert", text="Invert Red Channel", icon='COLOR_RED').invert_r = True
        layout.operator("image.invert", text="Invert Green Channel", icon='COLOR_GREEN').invert_g = True
        layout.operator("image.invert", text="Invert Blue Channel", icon='COLOR_BLUE').invert_b = True
        layout.operator("image.invert", text="Invert Alpha Channel", icon='IMAGE_ALPHA').invert_a = True


class IMAGE_MT_uvs_showhide(Menu):
    bl_label = "Show/Hide Faces"

    def draw(self, _context):
        layout = self.layout

        layout.operator("uv.reveal")
        layout.operator("uv.hide", text="Hide Selected").unselected = False
        layout.operator("uv.hide", text="Hide Unselected").unselected = True


class IMAGE_MT_uvs_transform(Menu):
    bl_label = "Transform"

    def draw(self, _context):
        layout = self.layout

        layout.operator("transform.translate")
        layout.operator("transform.rotate")
        layout.operator("transform.resize")

        layout.separator()

        layout.operator("transform.shear")


class IMAGE_MT_uvs_snap(Menu):
    bl_label = "Snap"

    def draw(self, _context):
        layout = self.layout

        layout.operator_context = 'EXEC_REGION_WIN'

        layout.operator("uv.snap_selected", text="Selected to Pixels").target = 'PIXELS'
        layout.operator("uv.snap_selected", text="Selected to Cursor").target = 'CURSOR'
        layout.operator("uv.snap_selected", text="Selected to Cursor (Offset)").target = 'CURSOR_OFFSET'
        layout.operator("uv.snap_selected", text="Selected to Adjacent Unselected").target = 'ADJACENT_UNSELECTED'

        layout.separator()

        layout.operator("uv.snap_cursor", text="Cursor to Pixels").target = 'PIXELS'
        layout.operator("uv.snap_cursor", text="Cursor to Selected").target = 'SELECTED'


class IMAGE_MT_uvs_mirror(Menu):
    bl_label = "Mirror"

    def draw(self, _context):
        layout = self.layout

        layout.operator("mesh.faces_mirror_uv")

        layout.separator()

        layout.operator_context = 'EXEC_REGION_WIN'

        layout.operator("transform.mirror", text="X Axis").constraint_axis[0] = True
        layout.operator("transform.mirror", text="Y Axis").constraint_axis[1] = True


class IMAGE_MT_uvs_align(Menu):
    bl_label = "Align"

    def draw(self, _context):
        layout = self.layout

        layout.operator_enum("uv.align", "axis")


class IMAGE_MT_uvs_merge(Menu):
    bl_label = "Merge"

    def draw(self, _context):
        layout = self.layout

        layout.operator("uv.weld", text="At Center")
        # Mainly to match the mesh menu.
        layout.operator("uv.snap_selected", text="At Cursor").target = 'CURSOR'

        layout.separator()

        layout.operator("uv.remove_doubles", text="By Distance")


class IMAGE_MT_uvs_split(Menu):
    bl_label = "Split"

    def draw(self, _context):
        layout = self.layout

        layout.operator("uv.select_split", text="Selection")


class IMAGE_MT_uvs_unwrap(Menu):
    bl_label = "Unwrap"

    def draw(self, _context):
        layout = self.layout

        layout.operator("uv.unwrap")

        layout.separator()

        layout.operator_context = 'INVOKE_DEFAULT'
        layout.operator("uv.smart_project")
        layout.operator("uv.lightmap_pack")
        layout.operator("uv.follow_active_quads")

        layout.separator()

        layout.operator_context = 'EXEC_REGION_WIN'
        layout.operator("uv.cube_project")
        layout.operator("uv.cylinder_project")
        layout.operator("uv.sphere_project")


class IMAGE_MT_uvs(Menu):
    bl_label = "UV"

    def draw(self, context):
        layout = self.layout

        sima = context.space_data
        uv = sima.uv_editor

        layout.menu("IMAGE_MT_uvs_transform")
        layout.menu("IMAGE_MT_uvs_mirror")
        layout.menu("IMAGE_MT_uvs_snap")

        layout.prop_menu_enum(uv, "pixel_snap_mode")
        layout.prop(uv, "lock_bounds")

        layout.separator()

        layout.menu("IMAGE_MT_uvs_merge")
        layout.menu("IMAGE_MT_uvs_split")

        layout.separator()

        layout.prop(uv, "use_live_unwrap")
        layout.menu("IMAGE_MT_uvs_unwrap")

        layout.separator()

        layout.operator("uv.pin").clear = False
        layout.operator("uv.pin", text="Unpin").clear = True

        layout.separator()

        layout.operator("uv.mark_seam").clear = False
        layout.operator("uv.mark_seam", text="Clear Seam").clear = True
        layout.operator("uv.seams_from_islands")

        layout.separator()

        layout.operator("uv.pack_islands")
        layout.operator("uv.average_islands_scale")

        layout.separator()

        layout.operator("uv.minimize_stretch")
        layout.operator("uv.stitch")
        layout.menu("IMAGE_MT_uvs_align")

        layout.separator()

        layout.menu("IMAGE_MT_uvs_showhide")

        layout.separator()

        layout.operator("uv.reset")

        layout.separator()


class IMAGE_MT_uvs_select_mode(Menu):
    bl_label = "UV Select Mode"

    def draw(self, context):
        layout = self.layout

        layout.operator_context = 'INVOKE_REGION_WIN'
        tool_settings = context.tool_settings

        # Do smart things depending on whether uv_select_sync is on.

        if tool_settings.use_uv_select_sync:
            props = layout.operator("wm.context_set_value", text="Vertex", icon='VERTEXSEL')
            props.value = "(True, False, False)"
            props.data_path = "tool_settings.mesh_select_mode"

            props = layout.operator("wm.context_set_value", text="Edge", icon='EDGESEL')
            props.value = "(False, True, False)"
            props.data_path = "tool_settings.mesh_select_mode"

            props = layout.operator("wm.context_set_value", text="Face", icon='FACESEL')
            props.value = "(False, False, True)"
            props.data_path = "tool_settings.mesh_select_mode"

        else:
            props = layout.operator("wm.context_set_string", text="Vertex", icon='UV_VERTEXSEL')
            props.value = 'VERTEX'
            props.data_path = "tool_settings.uv_select_mode"

            props = layout.operator("wm.context_set_string", text="Edge", icon='UV_EDGESEL')
            props.value = 'EDGE'
            props.data_path = "tool_settings.uv_select_mode"

            props = layout.operator("wm.context_set_string", text="Face", icon='UV_FACESEL')
            props.value = 'FACE'
            props.data_path = "tool_settings.uv_select_mode"

            props = layout.operator("wm.context_set_string", text="Island", icon='UV_ISLANDSEL')
            props.value = 'ISLAND'
            props.data_path = "tool_settings.uv_select_mode"


class IMAGE_MT_uvs_context_menu(Menu):
    bl_label = "UV Context Menu"

    def draw(self, context):
        layout = self.layout

        sima = context.space_data

        # UV Edit Mode
        if sima.show_uvedit:
            # Add
            layout.operator("uv.unwrap")
            layout.operator("uv.follow_active_quads")

            layout.separator()

            # Modify
            layout.operator("uv.pin").clear = False
            layout.operator("uv.pin", text="Unpin").clear = True

            layout.separator()

            layout.menu("IMAGE_MT_uvs_snap")

            layout.operator("transform.mirror", text="Mirror X").constraint_axis[0] = True
            layout.operator("transform.mirror", text="Mirror Y").constraint_axis[1] = True

            layout.separator()

            layout.operator_enum("uv.align", "axis")  # W, 2/3/4.

            layout.separator()

            # Remove
            layout.menu("IMAGE_MT_uvs_merge")
            layout.operator("uv.stitch")
            layout.menu("IMAGE_MT_uvs_split")


class IMAGE_MT_pivot_pie(Menu):
    bl_label = "Pivot Point"

    def draw(self, context):
        layout = self.layout
        pie = layout.menu_pie()

        pie.prop_enum(context.space_data, "pivot_point", value='CENTER')
        pie.prop_enum(context.space_data, "pivot_point", value='CURSOR')
        pie.prop_enum(context.space_data, "pivot_point", value='INDIVIDUAL_ORIGINS')
        pie.prop_enum(context.space_data, "pivot_point", value='MEDIAN')


class IMAGE_MT_uvs_snap_pie(Menu):
    bl_label = "Snap"

    def draw(self, _context):
        layout = self.layout
        pie = layout.menu_pie()

        layout.operator_context = 'EXEC_REGION_WIN'

        pie.operator(
            "uv.snap_selected",
            text="Selected to Pixels",
            icon='RESTRICT_SELECT_OFF',
        ).target = 'PIXELS'
        pie.operator(
            "uv.snap_cursor",
            text="Cursor to Pixels",
            icon='PIVOT_CURSOR',
        ).target = 'PIXELS'
        pie.operator(
            "uv.snap_cursor",
            text="Cursor to Selected",
            icon='PIVOT_CURSOR',
        ).target = 'SELECTED'
        pie.operator(
            "uv.snap_selected",
            text="Selected to Cursor",
            icon='RESTRICT_SELECT_OFF',
        ).target = 'CURSOR'
        pie.operator(
            "uv.snap_selected",
            text="Selected to Cursor (Offset)",
            icon='RESTRICT_SELECT_OFF',
        ).target = 'CURSOR_OFFSET'
        pie.operator(
            "uv.snap_selected",
            text="Selected to Adjacent Unselected",
            icon='RESTRICT_SELECT_OFF',
        ).target = 'ADJACENT_UNSELECTED'


class IMAGE_HT_tool_header(Header):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'TOOL_HEADER'

    def draw(self, context):
        layout = self.layout

        layout.template_header()

        self.draw_tool_settings(context)

        layout.separator_spacer()

        IMAGE_HT_header.draw_xform_template(layout, context)

        layout.separator_spacer()

        self.draw_mode_settings(context)

    def draw_tool_settings(self, context):
        layout = self.layout

        # Active Tool
        # -----------
        from bl_ui.space_toolsystem_common import ToolSelectPanelHelper
        tool = ToolSelectPanelHelper.draw_active_tool_header(context, layout)
        tool_mode = context.mode if tool is None else tool.mode

        # Object Mode Options
        # -------------------

        # Example of how tool_settings can be accessed as pop-overs.

        # TODO(campbell): editing options should be after active tool options
        # (obviously separated for from the users POV)
        draw_fn = getattr(_draw_tool_settings_context_mode, tool_mode, None)
        if draw_fn is not None:
            draw_fn(context, layout, tool)

        if tool_mode == 'PAINT':
            if (tool is not None) and tool.has_datablock:
                layout.popover("IMAGE_PT_paint_settings_advanced")
                layout.popover("IMAGE_PT_paint_stroke")
                layout.popover("IMAGE_PT_paint_curve")
                layout.popover("IMAGE_PT_tools_brush_display")
                layout.popover("IMAGE_PT_tools_brush_texture")
                layout.popover("IMAGE_PT_tools_mask_texture")
        elif tool_mode == 'UV':
            if (tool is not None) and tool.has_datablock:
                layout.popover("IMAGE_PT_uv_sculpt_curve")
                layout.popover("IMAGE_PT_uv_sculpt_options")

    def draw_mode_settings(self, context):
        layout = self.layout

        # Active Tool
        # -----------
        from bl_ui.space_toolsystem_common import ToolSelectPanelHelper
        tool = ToolSelectPanelHelper.tool_active_from_context(context)
        tool_mode = context.mode if tool is None else tool.mode

        if tool_mode == 'PAINT':
            layout.popover_group(space_type='IMAGE_EDITOR', region_type='UI', context=".imagepaint_2d", category="")


class _draw_tool_settings_context_mode:
    @staticmethod
    def UV(context, layout, tool):
        if tool and tool.has_datablock:
            if context.mode == 'EDIT_MESH':
                tool_settings = context.tool_settings
                uv_sculpt = tool_settings.uv_sculpt
                brush = uv_sculpt.brush
                if brush:
                    UnifiedPaintPanel.prop_unified(
                        layout,
                        context,
                        brush,
                        "size",
                        pressure_name="use_pressure_size",
                        unified_name="use_unified_size",
                        slider=True,
                        header=True
                    )
                    UnifiedPaintPanel.prop_unified(
                        layout,
                        context,
                        brush,
                        "strength",
                        pressure_name="use_pressure_strength",
                        unified_name="use_unified_strength",
                        slider=True,
                        header=True
                    )

    @staticmethod
    def PAINT(context, layout, tool):
        if (tool is None) or (not tool.has_datablock):
            return

        paint = context.tool_settings.image_paint
        layout.template_ID_preview(paint, "brush", rows=3, cols=8, hide_buttons=True)

        brush = paint.brush
        if brush is None:
            return

        brush_basic_texpaint_settings(layout, context, brush, compact=True)


class IMAGE_HT_header(Header):
    bl_space_type = 'IMAGE_EDITOR'

    @staticmethod
    def draw_xform_template(layout, context):
        sima = context.space_data
        show_uvedit = sima.show_uvedit
        show_maskedit = sima.show_maskedit

        if show_uvedit or show_maskedit:
            layout.prop(sima, "pivot_point", icon_only=True)

        if show_uvedit:
            tool_settings = context.tool_settings

            # Snap.
            snap_uv_element = tool_settings.snap_uv_element
            act_snap_uv_element = tool_settings.bl_rna.properties['snap_uv_element'].enum_items[snap_uv_element]

            row = layout.row(align=True)
            row.prop(tool_settings, "use_snap", text="")

            sub = row.row(align=True)
            sub.popover(
                panel="IMAGE_PT_snapping",
                icon=act_snap_uv_element.icon,
                text="",
            )

            # Proportional Editing
            row = layout.row(align=True)
            row.prop(
                tool_settings,
                "use_proportional_edit",
                icon_only=True,
                icon='PROP_CON' if tool_settings.use_proportional_connected else 'PROP_ON',
            )
            sub = row.row(align=True)
            sub.active = tool_settings.use_proportional_edit
            sub.prop_with_popover(
                tool_settings,
                "proportional_edit_falloff",
                text="",
                icon_only=True,
                panel="IMAGE_PT_proportional_edit",
            )

    def draw(self, context):
        layout = self.layout

        sima = context.space_data
        overlay = sima.overlay
        ima = sima.image
        iuser = sima.image_user
        tool_settings = context.tool_settings
        show_region_tool_header = sima.show_region_tool_header

        show_render = sima.show_render
        show_uvedit = sima.show_uvedit
        show_maskedit = sima.show_maskedit

        if not show_region_tool_header:
            layout.template_header()

        if sima.mode != 'UV':
            layout.prop(sima, "ui_mode", text="")

        # UV editing.
        if show_uvedit:
            uvedit = sima.uv_editor

            layout.prop(tool_settings, "use_uv_select_sync", text="")

            if tool_settings.use_uv_select_sync:
                layout.template_edit_mode_selection()
            else:
                layout.prop(tool_settings, "uv_select_mode", text="", expand=True)
                layout.prop(uvedit, "sticky_select_mode", icon_only=True)

        IMAGE_MT_editor_menus.draw_collapsible(context, layout)

        layout.separator_spacer()

        if not show_region_tool_header:
            IMAGE_HT_header.draw_xform_template(layout, context)

        layout.template_ID(sima, "image", new="image.new", open="image.open")

        if show_maskedit:
            row = layout.row()
            row.template_ID(sima, "mask", new="mask.new")

        if not show_render:
            layout.prop(sima, "use_image_pin", text="", emboss=False)

        layout.separator_spacer()

        # Overlay toggle & popover
        row = layout.row(align=True)
        row.prop(overlay, "show_overlays", icon='OVERLAY', text="")
        sub = row.row(align=True)
        sub.active = overlay.show_overlays
        sub.popover(panel="IMAGE_PT_overlay", text="")

        if show_uvedit:
            uvedit = sima.uv_editor

            mesh = context.edit_object.data
            layout.prop_search(mesh.uv_layers, "active", mesh, "uv_layers", text="")

        if ima:
            if ima.is_stereo_3d:
                row = layout.row()
                row.prop(sima, "show_stereo_3d", text="")
            if show_maskedit:
                row = layout.row()
                row.popover(panel='CLIP_PT_mask_display')

            # layers.
            layout.template_image_layers(ima, iuser)

            # draw options.
            row = layout.row()
            row.prop(sima, "display_channels", icon_only=True)

            row = layout.row(align=True)
            if ima.type == 'COMPOSITE':
                row.operator("image.record_composite", icon='REC')
            if ima.type == 'COMPOSITE' and ima.source in {'MOVIE', 'SEQUENCE'}:
                row.operator("image.play_composite", icon='PLAY')


class IMAGE_MT_editor_menus(Menu):
    bl_idname = "IMAGE_MT_editor_menus"
    bl_label = ""

    def draw(self, context):
        layout = self.layout
        sima = context.space_data
        ima = sima.image

        show_uvedit = sima.show_uvedit
        show_maskedit = sima.show_maskedit

        layout.menu("IMAGE_MT_view")

        if show_uvedit:
            layout.menu("IMAGE_MT_select")
        if show_maskedit:
            layout.menu("MASK_MT_select")

        if ima and ima.is_dirty:
            layout.menu("IMAGE_MT_image", text="Image*")
        else:
            layout.menu("IMAGE_MT_image", text="Image")

        if show_uvedit:
            layout.menu("IMAGE_MT_uvs")
        if show_maskedit:
            layout.menu("MASK_MT_add")
            layout.menu("MASK_MT_mask")


class IMAGE_MT_mask_context_menu(Menu):
    bl_label = "Mask Context Menu"

    @classmethod
    def poll(cls, context):
        sima = context.space_data
        return sima.show_maskedit

    def draw(self, context):
        layout = self.layout
        from .properties_mask_common import draw_mask_context_menu
        draw_mask_context_menu(layout, context)


# -----------------------------------------------------------------------------
# Mask (similar code in space_clip.py, keep in sync)
# note! - panel placement does _not_ fit well with image panels... need to fix.

from bl_ui.properties_mask_common import (
    MASK_PT_mask,
    MASK_PT_layers,
    MASK_PT_spline,
    MASK_PT_point,
    MASK_PT_display,
)


class IMAGE_PT_mask(MASK_PT_mask, Panel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'UI'
    bl_category = "Mask"


class IMAGE_PT_mask_layers(MASK_PT_layers, Panel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'UI'
    bl_category = "Mask"


class IMAGE_PT_active_mask_spline(MASK_PT_spline, Panel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'UI'
    bl_category = "Mask"


class IMAGE_PT_active_mask_point(MASK_PT_point, Panel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'UI'
    bl_category = "Mask"


# --- end mask ---

class IMAGE_PT_snapping(Panel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'HEADER'
    bl_label = "Snapping"

    def draw(self, context):
        tool_settings = context.tool_settings

        layout = self.layout
        col = layout.column()
        col.label(text="Snapping")
        col.prop(tool_settings, "snap_uv_element", expand=True)

        if tool_settings.snap_uv_element != 'INCREMENT':
            col.label(text="Target")
            row = col.row(align=True)
            row.prop(tool_settings, "snap_target", expand=True)

        col.label(text="Affect")
        row = col.row(align=True)
        row.prop(tool_settings, "use_snap_translate", text="Move", toggle=True)
        row.prop(tool_settings, "use_snap_rotate", text="Rotate", toggle=True)
        row.prop(tool_settings, "use_snap_scale", text="Scale", toggle=True)


class IMAGE_PT_proportional_edit(Panel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'HEADER'
    bl_label = "Proportional Editing"
    bl_ui_units_x = 8

    def draw(self, context):
        layout = self.layout
        tool_settings = context.tool_settings
        col = layout.column()

        col.prop(tool_settings, "use_proportional_connected")
        col.separator()

        col.prop(tool_settings, "proportional_edit_falloff", expand=True)


class IMAGE_PT_image_properties(Panel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'UI'
    bl_category = "Image"
    bl_label = "Image"

    @classmethod
    def poll(cls, context):
        sima = context.space_data
        return (sima.image)

    def draw(self, context):
        layout = self.layout

        sima = context.space_data
        iuser = sima.image_user

        layout.template_image(sima, "image", iuser, multiview=True)


class IMAGE_PT_view_display(Panel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'UI'
    bl_label = "Display"
    bl_category = "View"

    @classmethod
    def poll(cls, context):
        sima = context.space_data
        return (sima and (sima.image or sima.show_uvedit))

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        sima = context.space_data
        ima = sima.image

        show_uvedit = sima.show_uvedit
        uvedit = sima.uv_editor

        col = layout.column()

        if ima:
            col.prop(ima, "display_aspect", text="Aspect Ratio")
            row = col.row()
            row.active = ima.source != 'TILED'
            row.prop(sima, "show_repeat", text="Repeat Image")

        if show_uvedit:
            col.prop(uvedit, "show_pixel_coords", text="Pixel Coordinates")


class IMAGE_UL_render_slots(UIList):
    def draw_item(self, _context, layout, _data, item, _icon, _active_data, _active_propname, _index):
        slot = item
        layout.prop(slot, "name", text="", emboss=False)


class IMAGE_PT_render_slots(Panel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'UI'
    bl_category = "Image"
    bl_label = "Render Slots"

    @classmethod
    def poll(cls, context):
        sima = context.space_data
        return (sima and sima.image and sima.show_render)

    def draw(self, context):
        layout = self.layout

        sima = context.space_data
        ima = sima.image

        row = layout.row()

        col = row.column()
        col.template_list(
            "IMAGE_UL_render_slots", "render_slots", ima,
            "render_slots", ima.render_slots, "active_index", rows=3,
        )

        col = row.column(align=True)
        col.operator("image.add_render_slot", icon='ADD', text="")
        col.operator("image.remove_render_slot", icon='REMOVE', text="")

        col.separator()

        col.operator("image.clear_render_slot", icon='X', text="")


class IMAGE_UL_udim_tiles(UIList):
    def draw_item(self, _context, layout, _data, item, _icon, _active_data, _active_propname, _index):
        tile = item
        layout.prop(tile, "label", text="", emboss=False)


class IMAGE_PT_udim_tiles(Panel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'UI'
    bl_category = "Image"
    bl_label = "UDIM Tiles"

    @classmethod
    def poll(cls, context):
        sima = context.space_data
        return (sima and sima.image and sima.image.source == 'TILED')

    def draw(self, context):
        layout = self.layout

        sima = context.space_data
        ima = sima.image

        row = layout.row()
        col = row.column()
        col.template_list("IMAGE_UL_udim_tiles", "", ima, "tiles", ima.tiles, "active_index", rows=4)

        col = row.column()
        sub = col.column(align=True)
        sub.operator("image.tile_add", icon='ADD', text="")
        sub.operator("image.tile_remove", icon='REMOVE', text="")

        tile = ima.tiles.active
        if tile:
            col = layout.column(align=True)
            col.operator("image.tile_fill")


class IMAGE_PT_paint_select(Panel, ImagePaintPanel, BrushSelectPanel):
    bl_label = "Brushes"
    bl_context = ".paint_common_2d"
    bl_category = "Tool"


class IMAGE_PT_paint_settings(Panel, ImagePaintPanel):
    bl_context = ".paint_common_2d"
    bl_category = "Tool"
    bl_label = "Brush Settings"

    def draw(self, context):
        layout = self.layout

        layout.use_property_split = True
        layout.use_property_decorate = False

        settings = context.tool_settings.image_paint
        brush = settings.brush

        if brush:
            brush_settings(layout.column(), context, brush, popover=self.is_popover)


class IMAGE_PT_paint_settings_advanced(Panel, ImagePaintPanel):
    bl_context = ".paint_common_2d"
    bl_parent_id = "IMAGE_PT_paint_settings"
    bl_category = "Tool"
    bl_label = "Advanced"
    bl_ui_units_x = 12

    def draw(self, context):
        layout = self.layout

        layout.use_property_split = True
        layout.use_property_decorate = False  # No animation.

        settings = context.tool_settings.image_paint
        brush = settings.brush

        brush_settings_advanced(layout.column(), context, brush, self.is_popover)


class IMAGE_PT_paint_color(Panel, ImagePaintPanel):
    bl_context = ".paint_common_2d"
    bl_parent_id = "IMAGE_PT_paint_settings"
    bl_category = "Tool"
    bl_label = "Color Picker"

    @classmethod
    def poll(cls, context):
        settings = context.tool_settings.image_paint
        brush = settings.brush
        capabilities = brush.image_paint_capabilities

        return capabilities.has_color

    def draw(self, context):
        layout = self.layout
        settings = context.tool_settings.image_paint
        brush = settings.brush

        draw_color_settings(context, layout, brush, color_type=True)


class IMAGE_PT_paint_swatches(Panel, ImagePaintPanel, ColorPalettePanel):
    bl_category = "Tool"
    bl_context = ".paint_common_2d"
    bl_parent_id = "IMAGE_PT_paint_settings"
    bl_label = "Color Palette"
    bl_options = {'DEFAULT_CLOSED'}


class IMAGE_PT_paint_clone(Panel, ImagePaintPanel, ClonePanel):
    bl_category = "Tool"
    bl_context = ".paint_common_2d"
    bl_parent_id = "IMAGE_PT_paint_settings"
    bl_label = "Clone from Image/UV Map"


class IMAGE_PT_tools_brush_display(Panel, BrushButtonsPanel, DisplayPanel):
    bl_context = ".paint_common_2d"
    bl_parent_id = "IMAGE_PT_paint_settings"
    bl_category = "Tool"
    bl_label = "Brush Tip"
    bl_options = {'DEFAULT_CLOSED'}
    bl_ui_units_x = 15


class IMAGE_PT_tools_brush_texture(BrushButtonsPanel, Panel):
    bl_label = "Texture"
    bl_context = ".paint_common_2d"
    bl_parent_id = "IMAGE_PT_paint_settings"
    bl_category = "Tool"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout

        tool_settings = context.tool_settings.image_paint
        brush = tool_settings.brush
        tex_slot = brush.texture_slot

        col = layout.column()
        col.template_ID_preview(tex_slot, "texture", new="texture.new", rows=3, cols=8)

        brush_texture_settings(col, brush, 0)


class IMAGE_PT_tools_mask_texture(Panel, BrushButtonsPanel, TextureMaskPanel):
    bl_context = ".paint_common_2d"
    bl_parent_id = "IMAGE_PT_paint_settings"
    bl_category = "Tool"
    bl_label = "Texture Mask"
    bl_ui_units_x = 12


class IMAGE_PT_paint_stroke(BrushButtonsPanel, Panel, StrokePanel):
    bl_label = "Stroke"
    bl_context = ".paint_common_2d"
    bl_parent_id = "IMAGE_PT_paint_settings"
    bl_category = "Tool"
    bl_options = {'DEFAULT_CLOSED'}


class IMAGE_PT_paint_stroke_smooth_stroke(Panel, BrushButtonsPanel, SmoothStrokePanel):
    bl_context = ".paint_common_2d"
    bl_label = "Stabilize Stroke"
    bl_parent_id = "IMAGE_PT_paint_stroke"
    bl_category = "Tool"
    bl_options = {'DEFAULT_CLOSED'}


class IMAGE_PT_paint_curve(BrushButtonsPanel, Panel, FalloffPanel):
    bl_label = "Falloff"
    bl_context = ".paint_common_2d"
    bl_parent_id = "IMAGE_PT_paint_settings"
    bl_category = "Tool"
    bl_options = {'DEFAULT_CLOSED'}


class IMAGE_PT_tools_imagepaint_symmetry(BrushButtonsPanel, Panel):
    bl_context = ".imagepaint_2d"
    bl_label = "Tiling"
    bl_category = "Tool"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout

        tool_settings = context.tool_settings
        ipaint = tool_settings.image_paint

        col = layout.column(align=True)
        row = col.row(align=True)
        row.prop(ipaint, "tile_x", text="X", toggle=True)
        row.prop(ipaint, "tile_y", text="Y", toggle=True)


class UVSculptPanel(UnifiedPaintPanel):
    @classmethod
    def poll(cls, context):
        return cls.get_brush_mode(context) == 'UV_SCULPT'


class IMAGE_PT_uv_sculpt_brush_select(Panel, BrushSelectPanel, ImagePaintPanel, UVSculptPanel):
    bl_context = ".uv_sculpt"
    bl_category = "Tool"
    bl_label = "Brushes"


class IMAGE_PT_uv_sculpt_brush_settings(Panel, ImagePaintPanel, UVSculptPanel):
    bl_context = ".uv_sculpt"
    bl_category = "Tool"
    bl_label = "Brush Settings"

    def draw(self, context):
        layout = self.layout

        tool_settings = context.tool_settings
        uvsculpt = tool_settings.uv_sculpt

        brush = uvsculpt.brush

        brush_settings(layout.column(), context, brush)

        if brush:
            if brush.uv_sculpt_tool == 'RELAX':
                # Although this settings is stored in the scene,
                # it is only used by a single tool,
                # so it doesn't make sense from a user perspective to move it to the Options panel.
                layout.prop(tool_settings, "uv_relax_method")


class IMAGE_PT_uv_sculpt_curve(Panel, FalloffPanel, ImagePaintPanel, UVSculptPanel):
    bl_context = ".uv_sculpt"  # dot on purpose (access from topbar)
    bl_parent_id = "IMAGE_PT_uv_sculpt_brush_settings"
    bl_category = "Tool"
    bl_label = "Falloff"
    bl_options = {'DEFAULT_CLOSED'}


class IMAGE_PT_uv_sculpt_options(Panel, ImagePaintPanel, UVSculptPanel):
    bl_context = ".uv_sculpt"  # dot on purpose (access from topbar)
    bl_category = "Tool"
    bl_label = "Options"

    def draw(self, context):
        layout = self.layout

        tool_settings = context.tool_settings
        uvsculpt = tool_settings.uv_sculpt

        col = layout.column()
        col.prop(tool_settings, "uv_sculpt_lock_borders")
        col.prop(tool_settings, "uv_sculpt_all_islands")
        col.prop(uvsculpt, "show_brush", text="Display Cursor")


class ImageScopesPanel:
    @classmethod
    def poll(cls, context):
        sima = context.space_data

        if not (sima and sima.image):
            return False

        # scopes are not updated in paint modes, hide.
        if sima.mode == 'PAINT':
            return False

        ob = context.active_object
        if ob and ob.mode in {'TEXTURE_PAINT', 'EDIT'}:
            return False

        return True


class IMAGE_PT_view_histogram(ImageScopesPanel, Panel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'UI'
    bl_category = "Scopes"
    bl_label = "Histogram"

    def draw(self, context):
        layout = self.layout

        sima = context.space_data
        hist = sima.scopes.histogram

        layout.template_histogram(sima.scopes, "histogram")

        row = layout.row(align=True)
        row.prop(hist, "mode", expand=True)
        row.prop(hist, "show_line", text="")


class IMAGE_PT_view_waveform(ImageScopesPanel, Panel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'UI'
    bl_category = "Scopes"
    bl_label = "Waveform"

    def draw(self, context):
        layout = self.layout

        sima = context.space_data

        layout.template_waveform(sima, "scopes")
        row = layout.split(factor=0.75)
        row.prop(sima.scopes, "waveform_alpha")
        row.prop(sima.scopes, "waveform_mode", text="")


class IMAGE_PT_view_vectorscope(ImageScopesPanel, Panel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'UI'
    bl_category = "Scopes"
    bl_label = "Vectorscope"

    def draw(self, context):
        layout = self.layout

        sima = context.space_data
        layout.template_vectorscope(sima, "scopes")
        layout.prop(sima.scopes, "vectorscope_alpha")


class IMAGE_PT_sample_line(ImageScopesPanel, Panel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'UI'
    bl_category = "Scopes"
    bl_label = "Sample Line"

    def draw(self, context):
        layout = self.layout

        sima = context.space_data
        hist = sima.sample_histogram

        layout.operator("image.sample_line")
        layout.template_histogram(sima, "sample_histogram")

        row = layout.row(align=True)
        row.prop(hist, "mode", expand=True)
        row.prop(hist, "show_line", text="")


class IMAGE_PT_scope_sample(ImageScopesPanel, Panel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'UI'
    bl_category = "Scopes"
    bl_label = "Samples"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=True)

        sima = context.space_data

        col = flow.column()
        col.prop(sima.scopes, "use_full_resolution")

        col = flow.column()
        col.active = not sima.scopes.use_full_resolution
        col.prop(sima.scopes, "accuracy")


class IMAGE_PT_uv_cursor(Panel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'UI'
    bl_category = "View"
    bl_label = "2D Cursor"

    @classmethod
    def poll(cls, context):
        sima = context.space_data

        return (sima and (sima.show_uvedit or sima.show_maskedit))

    def draw(self, context):
        layout = self.layout

        sima = context.space_data

        layout.use_property_split = True
        layout.use_property_decorate = False

        col = layout.column()
        col.prop(sima, "cursor_location", text="Location")


class IMAGE_PT_udim_grid(Panel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'UI'
    bl_category = "View"
    bl_label = "UDIM Grid"

    @classmethod
    def poll(cls, context):
        sima = context.space_data

        return sima.show_uvedit and sima.image is None

    def draw(self, context):
        layout = self.layout

        sima = context.space_data
        uvedit = sima.uv_editor

        layout.use_property_split = True
        layout.use_property_decorate = False

        col = layout.column()
        col.prop(uvedit, "tile_grid_shape", text="Grid Shape")


class IMAGE_PT_overlay(Panel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'HEADER'
    bl_label = "Overlays"
    bl_ui_units_x = 13

    def draw(self, context):
        pass


class IMAGE_PT_overlay_uv_edit(Panel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'HEADER'
    bl_label = "UV Editing"
    bl_parent_id = 'IMAGE_PT_overlay'

    @classmethod
    def poll(cls, context):
        sima = context.space_data
        return (sima and (sima.show_uvedit))

    def draw(self, context):
        layout = self.layout

        sima = context.space_data
        uvedit = sima.uv_editor
        overlay = sima.overlay

        layout.active = overlay.show_overlays

        # UV Stretching
        row = layout.row()
        row.prop(uvedit, "show_stretch")
        subrow = row.row(align=True)
        subrow.active = uvedit.show_stretch
        subrow.prop(uvedit, "display_stretch_type", text="")


class IMAGE_PT_overlay_uv_edit_geometry(Panel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'HEADER'
    bl_label = "Geometry"
    bl_parent_id = 'IMAGE_PT_overlay'

    @classmethod
    def poll(cls, context):
        sima = context.space_data
        return (sima and (sima.show_uvedit))

    def draw(self, context):
        layout = self.layout

        sima = context.space_data
        uvedit = sima.uv_editor
        overlay = sima.overlay

        layout.active = overlay.show_overlays

        # Edges
        col = layout.column()
        col.prop(uvedit, "uv_opacity")
        col.prop(uvedit, "edge_display_type", text="")
        col.prop(uvedit, "show_modified_edges", text="Modified Edges")

        # Faces
        row = col.row()
        row.active = not uvedit.show_stretch
        row.prop(uvedit, "show_faces", text="Faces")


class IMAGE_PT_overlay_texture_paint(Panel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'HEADER'
    bl_label = "Geometry"
    bl_parent_id = 'IMAGE_PT_overlay'

    @classmethod
    def poll(cls, context):
        sima = context.space_data
        return (sima and (sima.show_paint))

    def draw(self, context):
        layout = self.layout

        sima = context.space_data
        uvedit = sima.uv_editor
        overlay = sima.overlay

        layout.active = overlay.show_overlays
        layout.prop(uvedit, "show_texpaint")


class IMAGE_PT_overlay_image(Panel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'HEADER'
    bl_label = "Image"
    bl_parent_id = 'IMAGE_PT_overlay'

    def draw(self, context):
        layout = self.layout

        sima = context.space_data
        uvedit = sima.uv_editor
        overlay = sima.overlay

        layout.active = overlay.show_overlays
        layout.prop(uvedit, "show_metadata")


# Grease Pencil properties
class IMAGE_PT_annotation(AnnotationDataPanel, Panel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'UI'
    bl_category = "View"

    # NOTE: this is just a wrapper around the generic GP Panel.

# Grease Pencil drawing tools.


classes = (
    IMAGE_MT_view,
    IMAGE_MT_view_zoom,
    IMAGE_MT_select,
    IMAGE_MT_select_linked,
    IMAGE_MT_image,
    IMAGE_MT_image_flip,
    IMAGE_MT_image_invert,
    IMAGE_MT_uvs,
    IMAGE_MT_uvs_showhide,
    IMAGE_MT_uvs_transform,
    IMAGE_MT_uvs_snap,
    IMAGE_MT_uvs_mirror,
    IMAGE_MT_uvs_align,
    IMAGE_MT_uvs_merge,
    IMAGE_MT_uvs_split,
    IMAGE_MT_uvs_unwrap,
    IMAGE_MT_uvs_select_mode,
    IMAGE_MT_uvs_context_menu,
    IMAGE_MT_mask_context_menu,
    IMAGE_MT_pivot_pie,
    IMAGE_MT_uvs_snap_pie,
    IMAGE_HT_tool_header,
    IMAGE_HT_header,
    IMAGE_MT_editor_menus,
    IMAGE_PT_active_tool,
    IMAGE_PT_mask,
    IMAGE_PT_mask_layers,
    IMAGE_PT_active_mask_spline,
    IMAGE_PT_active_mask_point,
    IMAGE_PT_snapping,
    IMAGE_PT_proportional_edit,
    IMAGE_PT_image_properties,
    IMAGE_UL_render_slots,
    IMAGE_PT_render_slots,
    IMAGE_UL_udim_tiles,
    IMAGE_PT_udim_tiles,
    IMAGE_PT_view_display,
    IMAGE_PT_paint_select,
    IMAGE_PT_paint_settings,
    IMAGE_PT_paint_color,
    IMAGE_PT_paint_swatches,
    IMAGE_PT_paint_settings_advanced,
    IMAGE_PT_paint_clone,
    IMAGE_PT_tools_brush_texture,
    IMAGE_PT_tools_mask_texture,
    IMAGE_PT_paint_stroke,
    IMAGE_PT_paint_stroke_smooth_stroke,
    IMAGE_PT_paint_curve,
    IMAGE_PT_tools_brush_display,
    IMAGE_PT_tools_imagepaint_symmetry,
    IMAGE_PT_uv_sculpt_brush_select,
    IMAGE_PT_uv_sculpt_brush_settings,
    IMAGE_PT_uv_sculpt_options,
    IMAGE_PT_uv_sculpt_curve,
    IMAGE_PT_view_histogram,
    IMAGE_PT_view_waveform,
    IMAGE_PT_view_vectorscope,
    IMAGE_PT_sample_line,
    IMAGE_PT_scope_sample,
    IMAGE_PT_uv_cursor,
    IMAGE_PT_annotation,
    IMAGE_PT_udim_grid,
    IMAGE_PT_overlay,
    IMAGE_PT_overlay_uv_edit,
    IMAGE_PT_overlay_uv_edit_geometry,
    IMAGE_PT_overlay_texture_paint,
    IMAGE_PT_overlay_image,
)


if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
