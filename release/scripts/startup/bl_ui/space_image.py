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
import math
from bpy.types import Header, Menu, Panel, UIList
from .properties_paint_common import (
    UnifiedPaintPanel,
    brush_texture_settings,
    brush_texpaint_common,
    brush_mask_texture_settings,
)
from .properties_grease_pencil_common import (
    GreasePencilDrawingToolsPanel,
    GreasePencilStrokeEditPanel,
    GreasePencilStrokeSculptPanel,
    GreasePencilBrushPanel,
    GreasePencilBrushCurvesPanel,
    GreasePencilDataPanel,
    GreasePencilPaletteColorPanel,
)
from bpy.app.translations import pgettext_iface as iface_


class ImagePaintPanel(UnifiedPaintPanel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'TOOLS'


class BrushButtonsPanel(UnifiedPaintPanel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'TOOLS'

    @classmethod
    def poll(cls, context):
        sima = context.space_data
        toolsettings = context.tool_settings.image_paint
        return sima.show_paint and toolsettings.brush


class UVToolsPanel:
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'TOOLS'
    bl_category = "Tools"

    @classmethod
    def poll(cls, context):
        sima = context.space_data
        return sima.show_uvedit and not context.tool_settings.use_uv_sculpt


class IMAGE_MT_view(Menu):
    bl_label = "View"

    def draw(self, context):
        layout = self.layout

        sima = context.space_data
        uv = sima.uv_editor
        toolsettings = context.tool_settings
        paint = toolsettings.image_paint

        show_uvedit = sima.show_uvedit
        show_render = sima.show_render

        layout.operator("image.properties", icon='MENU_PANEL')
        layout.operator("image.toolshelf", icon='MENU_PANEL')

        layout.separator()

        layout.prop(sima, "use_realtime_update")
        if show_uvedit:
            layout.prop(toolsettings, "show_uv_local_view")

        layout.prop(uv, "show_other_objects")
        layout.prop(uv, "show_metadata")
        if paint.brush and (context.image_paint_object or sima.mode == 'PAINT'):
            layout.prop(uv, "show_texpaint")
            layout.prop(toolsettings, "show_uv_local_view", text="Show Same Material")

        layout.separator()

        layout.operator("image.view_zoom_in")
        layout.operator("image.view_zoom_out")

        layout.separator()

        ratios = ((1, 8), (1, 4), (1, 2), (1, 1), (2, 1), (4, 1), (8, 1))

        for a, b in ratios:
            layout.operator(
                "image.view_zoom_ratio",
                text=iface_(f"Zoom {a:d}:{b:d}"),
                translate=False,
            ).ratio = a / b

        layout.separator()

        if show_uvedit:
            layout.operator("image.view_selected")

        layout.operator("image.view_all")
        layout.operator("image.view_all", text="View Fit").fit_view = True

        layout.separator()

        if show_render:
            layout.operator("image.render_border")
            layout.operator("image.clear_render_border")

            layout.separator()

            layout.operator("image.cycle_render_slot", text="Render Slot Cycle Next")
            layout.operator("image.cycle_render_slot", text="Render Slot Cycle Previous").reverse = True
            layout.separator()

        layout.menu("INFO_MT_area")


class IMAGE_MT_select(Menu):
    bl_label = "Select"

    def draw(self, context):
        layout = self.layout

        layout.operator("uv.select_all", text="All").action = 'SELECT'
        layout.operator("uv.select_all", text="None").action = 'DESELECT'
        layout.operator("uv.select_all", text="Invert").action = 'INVERT'

        layout.separator()

        layout.operator("uv.select_border").pinned = False
        layout.operator("uv.select_border", text="Border Select Pinned").pinned = True
        layout.operator("uv.circle_select")

        layout.separator()

        layout.operator("uv.select_less", text="Less")
        layout.operator("uv.select_more", text="More")

        layout.separator()

        layout.operator("uv.select_pinned")
        layout.operator("uv.select_linked").extend = False

        layout.separator()

        layout.operator("uv.select_split")


class IMAGE_MT_brush(Menu):
    bl_label = "Brush"

    def draw(self, context):
        layout = self.layout
        toolsettings = context.tool_settings
        settings = toolsettings.image_paint
        brush = settings.brush

        ups = context.tool_settings.unified_paint_settings
        layout.prop(ups, "use_unified_size", text="Unified Size")
        layout.prop(ups, "use_unified_strength", text="Unified Strength")
        layout.prop(ups, "use_unified_color", text="Unified Color")
        layout.separator()

        # brush tool
        layout.prop_menu_enum(brush, "image_tool")


class IMAGE_MT_image(Menu):
    bl_label = "Image"

    def draw(self, context):
        layout = self.layout

        sima = context.space_data
        ima = sima.image
        show_render = sima.show_render

        layout.operator("image.new", text="New")
        layout.operator("image.open", text="Open...")

        layout.operator("image.read_viewlayers")

        if ima:
            if not show_render:
                layout.operator("image.replace", text="Replace...")
                layout.operator("image.reload", text="Reload")

            layout.operator("image.external_edit", "Edit Externally")

        layout.separator()

        if ima:
            layout.operator("image.save", text="Save")
            layout.operator("image.save_as", text="Save As...")
            layout.operator("image.save_as", text="Save a Copy...").copy = True

        if ima and ima.source == 'SEQUENCE':
            layout.operator("image.save_sequence")

        layout.operator("image.save_dirty", text="Save All Images")

        if ima:
            layout.separator()

            layout.menu("IMAGE_MT_image_invert")

            if not show_render:
                if not ima.packed_file:
                    layout.separator()
                    layout.operator("image.pack", text="Pack")

                # only for dirty && specific image types, perhaps
                # this could be done in operator poll too
                if ima.is_dirty:
                    if ima.source in {'FILE', 'GENERATED'} and ima.type != 'OPEN_EXR_MULTILAYER':
                        if ima.packed_file:
                            layout.separator()
                        layout.operator("image.pack", text="Pack As PNG").as_png = True


class IMAGE_MT_image_invert(Menu):
    bl_label = "Invert"

    def draw(self, context):
        layout = self.layout

        props = layout.operator("image.invert", text="Invert Image Colors")
        props.invert_r = True
        props.invert_g = True
        props.invert_b = True

        layout.separator()

        layout.operator("image.invert", text="Invert Red Channel").invert_r = True
        layout.operator("image.invert", text="Invert Green Channel").invert_g = True
        layout.operator("image.invert", text="Invert Blue Channel").invert_b = True
        layout.operator("image.invert", text="Invert Alpha Channel").invert_a = True


class IMAGE_MT_uvs_showhide(Menu):
    bl_label = "Show/Hide Faces"

    def draw(self, context):
        layout = self.layout

        layout.operator("uv.reveal")
        layout.operator("uv.hide", text="Hide Selected").unselected = False
        layout.operator("uv.hide", text="Hide Unselected").unselected = True


class IMAGE_MT_uvs_proportional(Menu):
    bl_label = "Proportional Editing"

    def draw(self, context):
        layout = self.layout

        layout.props_enum(context.tool_settings, "proportional_edit")

        layout.separator()

        layout.label("Falloff:")
        layout.props_enum(context.tool_settings, "proportional_edit_falloff")


class IMAGE_MT_uvs_transform(Menu):
    bl_label = "Transform"

    def draw(self, context):
        layout = self.layout

        layout.operator("transform.translate")
        layout.operator("transform.rotate")
        layout.operator("transform.resize")

        layout.separator()

        layout.operator("transform.shear")


class IMAGE_MT_uvs_snap(Menu):
    bl_label = "Snap"

    def draw(self, context):
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

    def draw(self, context):
        layout = self.layout

        layout.operator_context = 'EXEC_REGION_WIN'

        layout.operator("transform.mirror", text="X Axis").constraint_axis[0] = True
        layout.operator("transform.mirror", text="Y Axis").constraint_axis[1] = True


class IMAGE_MT_uvs_weldalign(Menu):
    bl_label = "Weld/Align"

    def draw(self, context):
        layout = self.layout

        layout.operator("uv.weld")  # W, 1
        layout.operator("uv.remove_doubles")
        layout.operator_enum("uv.align", "axis")  # W, 2/3/4


class IMAGE_MT_uvs(Menu):
    bl_label = "UVs"

    def draw(self, context):
        layout = self.layout

        sima = context.space_data
        uv = sima.uv_editor
        toolsettings = context.tool_settings

        layout.prop(uv, "use_snap_to_pixels")
        layout.prop(uv, "lock_bounds")

        layout.separator()

        layout.prop(toolsettings, "use_uv_sculpt")

        layout.separator()

        layout.prop(uv, "use_live_unwrap")
        layout.operator("uv.unwrap")
        layout.operator("uv.pin", text="Unpin").clear = True
        layout.operator("uv.pin").clear = False

        layout.separator()

        layout.operator("uv.pack_islands")
        layout.operator("uv.average_islands_scale")
        layout.operator("uv.minimize_stretch")
        layout.operator("uv.stitch")
        layout.operator("uv.mark_seam").clear = False
        layout.operator("uv.mark_seam", text="Clear Seam").clear = True
        layout.operator("uv.seams_from_islands")
        layout.operator("mesh.faces_mirror_uv")

        layout.separator()

        layout.menu("IMAGE_MT_uvs_transform")
        layout.menu("IMAGE_MT_uvs_mirror")
        layout.menu("IMAGE_MT_uvs_snap")
        layout.menu("IMAGE_MT_uvs_weldalign")

        layout.separator()

        layout.menu("IMAGE_MT_uvs_proportional")

        layout.separator()

        layout.menu("IMAGE_MT_uvs_showhide")


class IMAGE_MT_uvs_select_mode(Menu):
    bl_label = "UV Select Mode"

    def draw(self, context):
        layout = self.layout

        layout.operator_context = 'INVOKE_REGION_WIN'
        toolsettings = context.tool_settings

        # do smart things depending on whether uv_select_sync is on

        if toolsettings.use_uv_select_sync:
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


class IMAGE_MT_specials(Menu):
    bl_label = "UV Context Menu"

    def draw(self, context):
        layout = self.layout

        sima = context.space_data

        # UV Edit Mode
        if sima.show_uvedit:
            layout.operator("uv.unwrap")
            layout.operator("uv.follow_active_quads")

            layout.separator()

            layout.operator("uv.pin").clear = False
            layout.operator("uv.pin", text="Unpin").clear = True

            layout.separator()

            layout.operator("uv.weld")
            layout.operator("uv.stitch")

            layout.separator()

            layout.operator_enum("uv.align", "axis")  # W, 2/3/4

            layout.separator()

            layout.operator("transform.mirror", text="Mirror X").constraint_axis[0] = True
            layout.operator("transform.mirror", text="Mirror Y").constraint_axis[1] = True

            layout.separator()

            layout.menu("IMAGE_MT_uvs_snap")


class IMAGE_HT_header(Header):
    bl_space_type = 'IMAGE_EDITOR'

    def draw(self, context):
        layout = self.layout

        sima = context.space_data
        ima = sima.image
        iuser = sima.image_user
        toolsettings = context.tool_settings
        mode = sima.mode

        show_render = sima.show_render
        show_uvedit = sima.show_uvedit
        show_maskedit = sima.show_maskedit

        row = layout.row(align=True)
        row.template_header()

        layout.prop(sima, "mode", text="")

        # uv editing
        if show_uvedit:
            uvedit = sima.uv_editor

            layout.prop(toolsettings, "use_uv_select_sync", text="")

            if toolsettings.use_uv_select_sync:
                layout.template_edit_mode_selection()
            else:
                layout.prop(toolsettings, "uv_select_mode", text="", expand=True)
                layout.prop(uvedit, "sticky_select_mode", icon_only=True)

        MASK_MT_editor_menus.draw_collapsible(context, layout)

        layout.separator_spacer()

        layout.template_ID(sima, "image", new="image.new", open="image.open")

        if show_maskedit:
            row = layout.row()
            row.template_ID(sima, "mask", new="mask.new")

        layout.separator_spacer()

        if show_uvedit or show_maskedit or mode == 'PAINT':
            layout.prop(sima, "use_realtime_update", icon_only=True, icon='FILE_REFRESH')

        if not show_render:
            layout.prop(sima, "use_image_pin", text="")

        if show_uvedit:
            uvedit = sima.uv_editor

            mesh = context.edit_object.data
            layout.prop_search(mesh.uv_layers, "active", mesh, "uv_layers", text="")

            # Snap
            row = layout.row(align=True)
            row.prop(toolsettings, "use_snap", text="")
            row.prop(toolsettings, "snap_uv_element", icon_only=True)
            if toolsettings.snap_uv_element != 'INCREMENT':
                row.prop(toolsettings, "snap_target", text="")

            row = layout.row(align=True)
            row.prop(toolsettings, "proportional_edit", icon_only=True)
            # if toolsettings.proportional_edit != 'DISABLED':
            sub = row.row(align=True)
            sub.active = toolsettings.proportional_edit != 'DISABLED'
            sub.prop(toolsettings, "proportional_edit_falloff", icon_only=True)

        layout.prop(sima, "pivot_point", icon_only=True)

        if ima:
            if ima.is_stereo_3d:
                row = layout.row()
                row.prop(sima, "show_stereo_3d", text="")

            # layers
            layout.template_image_layers(ima, iuser)

            # draw options
            row = layout.row(align=True)
            row.prop(sima, "draw_channels", text="", expand=True)

            row = layout.row(align=True)
            if ima.type == 'COMPOSITE':
                row.operator("image.record_composite", icon='REC')
            if ima.type == 'COMPOSITE' and ima.source in {'MOVIE', 'SEQUENCE'}:
                row.operator("image.play_composite", icon='PLAY')


class MASK_MT_editor_menus(Menu):
    bl_idname = "MASK_MT_editor_menus"
    bl_label = ""

    def draw(self, context):
        self.draw_menus(self.layout, context)

    @staticmethod
    def draw_menus(layout, context):
        sima = context.space_data
        ima = sima.image

        show_uvedit = sima.show_uvedit
        show_maskedit = sima.show_maskedit
        show_paint = sima.show_paint

        layout.menu("IMAGE_MT_view")

        if show_uvedit:
            layout.menu("IMAGE_MT_select")
        if show_maskedit:
            layout.menu("MASK_MT_select")
        if show_paint:
            layout.menu("IMAGE_MT_brush")

        if ima and ima.is_dirty:
            layout.menu("IMAGE_MT_image", text="Image*")
        else:
            layout.menu("IMAGE_MT_image", text="Image")

        if show_uvedit:
            layout.menu("IMAGE_MT_uvs")
        if show_maskedit:
            layout.menu("MASK_MT_mask")


# -----------------------------------------------------------------------------
# Mask (similar code in space_clip.py, keep in sync)
# note! - panel placement does _not_ fit well with image panels... need to fix

from .properties_mask_common import (
    MASK_PT_mask,
    MASK_PT_layers,
    MASK_PT_spline,
    MASK_PT_point,
    MASK_PT_display,
    MASK_PT_tools,
    MASK_PT_add,
)


class IMAGE_PT_mask(MASK_PT_mask, Panel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'UI'


class IMAGE_PT_mask_layers(MASK_PT_layers, Panel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'UI'


class IMAGE_PT_mask_display(MASK_PT_display, Panel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'UI'


class IMAGE_PT_active_mask_spline(MASK_PT_spline, Panel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'UI'


class IMAGE_PT_active_mask_point(MASK_PT_point, Panel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'UI'


class IMAGE_PT_tools_mask(MASK_PT_tools, Panel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'TOOLS'
    bl_category = 'Mask'


class IMAGE_PT_tools_mask_add(MASK_PT_add, Panel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'TOOLS'
    bl_category = 'Mask'


# --- end mask ---


class IMAGE_PT_image_properties(Panel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'UI'
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


class IMAGE_PT_view_properties(Panel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'UI'
    bl_label = "Display"

    @classmethod
    def poll(cls, context):
        sima = context.space_data
        return (sima and (sima.image or sima.show_uvedit))

    def draw(self, context):
        layout = self.layout

        sima = context.space_data
        ima = sima.image

        show_render = sima.show_render
        show_uvedit = sima.show_uvedit
        show_maskedit = sima.show_maskedit
        uvedit = sima.uv_editor

        split = layout.split()

        col = split.column()
        if ima:
            col.prop(ima, "display_aspect", text="Aspect Ratio")

            col = split.column()
            col.label(text="Coordinates:")
            col.prop(sima, "show_repeat", text="Repeat")
            if show_uvedit:
                col.prop(uvedit, "show_pixel_coords", text="Pixel")

        elif show_uvedit:
            col.label(text="Coordinates:")
            col.prop(uvedit, "show_pixel_coords", text="Pixel")

        if show_uvedit or show_maskedit:
            col = layout.column()
            col.label("Cursor Location:")
            col.row().prop(sima, "cursor_location", text="")

        if show_uvedit:
            col.separator()

            col.label(text="UVs:")
            col.row().prop(uvedit, "edge_draw_type", expand=True)

            split = layout.split()

            col = split.column()
            col.prop(uvedit, "show_faces")
            col.prop(uvedit, "show_smooth_edges", text="Smooth")
            col.prop(uvedit, "show_modified_edges", text="Modified")

            col = split.column()
            col.prop(uvedit, "show_stretch", text="Stretch")
            sub = col.column()
            sub.active = uvedit.show_stretch
            sub.row().prop(uvedit, "draw_stretch_type", expand=True)

            col = layout.column()
            col.prop(uvedit, "show_other_objects")
            row = col.row()
            row.active = uvedit.show_other_objects
            row.prop(uvedit, "other_uv_filter", text="Filter")


class IMAGE_UL_render_slots(UIList):
    def draw_item(self, context, layout, data, item, icon, active_data, active_propname, index):
        slot = item
        layout.prop(slot, "name", text="", emboss=False)


class IMAGE_PT_render_slots(Panel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'UI'
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
        col.template_list("IMAGE_UL_render_slots", "render_slots", ima, "render_slots", ima.render_slots, "active_index", rows=3)

        col = row.column(align=True)
        col.operator("image.add_render_slot", icon='ZOOMIN', text="")
        col.operator("image.remove_render_slot", icon='ZOOMOUT', text="")

        col.separator()

        col.operator("image.clear_render_slot", icon='X', text="")


class IMAGE_PT_tools_transform_uvs(Panel, UVToolsPanel):
    bl_label = "Transform"

    @classmethod
    def poll(cls, context):
        sima = context.space_data
        return sima.show_uvedit and not context.tool_settings.use_uv_sculpt

    def draw(self, context):
        layout = self.layout

        col = layout.column(align=True)
        col.operator("transform.translate")
        col.operator("transform.rotate")
        col.operator("transform.resize", text="Scale")
        col.operator("transform.shear")


class IMAGE_PT_tools_align_uvs(Panel, UVToolsPanel):
    bl_label = "UV Align"

    @classmethod
    def poll(cls, context):
        sima = context.space_data
        return sima.show_uvedit and not context.tool_settings.use_uv_sculpt

    def draw(self, context):
        layout = self.layout
        layout.operator_context = 'EXEC_REGION_WIN'

        split = layout.split()
        col = split.column(align=True)
        col.operator("transform.mirror", text="Mirror X").constraint_axis[0] = True
        col.operator("transform.mirror", text="Mirror Y").constraint_axis[1] = True
        col = split.column(align=True)
        col.operator("transform.rotate", text="Rotate +90°").value = math.pi / 2
        col.operator("transform.rotate", text="Rotate  - 90°").value = math.pi / -2

        split = layout.split()
        col = split.column(align=True)
        col.operator("uv.align", text="Straighten").axis = 'ALIGN_S'
        col.operator("uv.align", text="Straighten X").axis = 'ALIGN_T'
        col.operator("uv.align", text="Straighten Y").axis = 'ALIGN_U'
        col = split.column(align=True)
        col.operator("uv.align", text="Align Auto").axis = 'ALIGN_AUTO'
        col.operator("uv.align", text="Align X").axis = 'ALIGN_X'
        col.operator("uv.align", text="Align Y").axis = 'ALIGN_Y'


class IMAGE_PT_tools_uvs(Panel, UVToolsPanel):
    bl_label = "UV Tools"

    @classmethod
    def poll(cls, context):
        sima = context.space_data
        return sima.show_uvedit and not context.tool_settings.use_uv_sculpt

    def draw(self, context):
        layout = self.layout

        col = layout.column(align=True)
        row = col.row(align=True)
        row.operator("uv.weld")
        row.operator("uv.stitch")
        col.operator("uv.remove_doubles")
        col.operator("uv.average_islands_scale")
        col.operator("uv.pack_islands")
        col.operator("mesh.faces_mirror_uv")
        col.operator("uv.minimize_stretch")

        layout.label(text="UV Unwrap:")
        row = layout.row(align=True)
        row.operator("uv.pin").clear = False
        row.operator("uv.pin", text="Unpin").clear = True
        col = layout.column(align=True)
        row = col.row(align=True)
        row.operator("uv.mark_seam", text="Mark Seam").clear = False
        row.operator("uv.mark_seam", text="Clear Seam").clear = True
        col.operator("uv.seams_from_islands", text="Mark Seams from Islands")
        col.operator("uv.unwrap")


class IMAGE_PT_paint(Panel, ImagePaintPanel):
    bl_label = "Paint"
    bl_category = "Tools"

    def draw(self, context):
        layout = self.layout

        settings = context.tool_settings.image_paint
        brush = settings.brush

        col = layout.column()
        col.template_ID_preview(settings, "brush", new="brush.add", rows=2, cols=6)

        if brush:
            brush_texpaint_common(self, context, layout, brush, settings)

    @classmethod
    def poll(cls, context):
        sima = context.space_data
        return sima.show_paint


class IMAGE_PT_tools_brush_overlay(BrushButtonsPanel, Panel):
    bl_label = "Overlay"
    bl_options = {'DEFAULT_CLOSED'}
    bl_category = "Options"

    def draw(self, context):
        layout = self.layout

        toolsettings = context.tool_settings.image_paint
        brush = toolsettings.brush
        tex_slot = brush.texture_slot
        tex_slot_mask = brush.mask_texture_slot

        col = layout.column()

        col.label(text="Curve:")

        row = col.row(align=True)
        if brush.use_cursor_overlay:
            row.prop(brush, "use_cursor_overlay", toggle=True, text="", icon='RESTRICT_VIEW_OFF')
        else:
            row.prop(brush, "use_cursor_overlay", toggle=True, text="", icon='RESTRICT_VIEW_ON')

        sub = row.row(align=True)
        sub.prop(brush, "cursor_overlay_alpha", text="Alpha")
        sub.prop(brush, "use_cursor_overlay_override", toggle=True, text="", icon='BRUSH_DATA')

        col.active = brush.brush_capabilities.has_overlay
        col.label(text="Texture:")
        row = col.row(align=True)
        if tex_slot.map_mode != 'STENCIL':
            if brush.use_primary_overlay:
                row.prop(brush, "use_primary_overlay", toggle=True, text="", icon='RESTRICT_VIEW_OFF')
            else:
                row.prop(brush, "use_primary_overlay", toggle=True, text="", icon='RESTRICT_VIEW_ON')

        sub = row.row(align=True)
        sub.prop(brush, "texture_overlay_alpha", text="Alpha")
        sub.prop(brush, "use_primary_overlay_override", toggle=True, text="", icon='BRUSH_DATA')

        col.label(text="Mask Texture:")

        row = col.row(align=True)
        if tex_slot_mask.map_mode != 'STENCIL':
            if brush.use_secondary_overlay:
                row.prop(brush, "use_secondary_overlay", toggle=True, text="", icon='RESTRICT_VIEW_OFF')
            else:
                row.prop(brush, "use_secondary_overlay", toggle=True, text="", icon='RESTRICT_VIEW_ON')

        sub = row.row(align=True)
        sub.prop(brush, "mask_overlay_alpha", text="Alpha")
        sub.prop(brush, "use_secondary_overlay_override", toggle=True, text="", icon='BRUSH_DATA')


class IMAGE_PT_tools_brush_texture(BrushButtonsPanel, Panel):
    bl_label = "Texture"
    bl_options = {'DEFAULT_CLOSED'}
    bl_category = "Tools"

    def draw(self, context):
        layout = self.layout

        toolsettings = context.tool_settings.image_paint
        brush = toolsettings.brush

        col = layout.column()
        col.template_ID_preview(brush, "texture", new="texture.new", rows=3, cols=8)

        brush_texture_settings(col, brush, 0)


class IMAGE_PT_tools_mask_texture(BrushButtonsPanel, Panel):
    bl_label = "Texture Mask"
    bl_options = {'DEFAULT_CLOSED'}
    bl_category = "Tools"

    def draw(self, context):
        layout = self.layout

        brush = context.tool_settings.image_paint.brush

        col = layout.column()

        col.template_ID_preview(brush, "mask_texture", new="texture.new", rows=3, cols=8)

        brush_mask_texture_settings(col, brush)


class IMAGE_PT_tools_brush_tool(BrushButtonsPanel, Panel):
    bl_label = "Tool"
    bl_options = {'DEFAULT_CLOSED'}
    bl_category = "Options"

    def draw(self, context):
        layout = self.layout
        toolsettings = context.tool_settings.image_paint
        brush = toolsettings.brush

        layout.prop(brush, "image_tool", text="")

        row = layout.row(align=True)
        row.prop(brush, "use_paint_sculpt", text="", icon='SCULPTMODE_HLT')
        row.prop(brush, "use_paint_vertex", text="", icon='VPAINT_HLT')
        row.prop(brush, "use_paint_weight", text="", icon='WPAINT_HLT')
        row.prop(brush, "use_paint_image", text="", icon='TPAINT_HLT')


class IMAGE_PT_paint_stroke(BrushButtonsPanel, Panel):
    bl_label = "Paint Stroke"
    bl_options = {'DEFAULT_CLOSED'}
    bl_category = "Tools"

    def draw(self, context):
        layout = self.layout

        toolsettings = context.tool_settings.image_paint
        brush = toolsettings.brush

        col = layout.column()

        col.label(text="Stroke Method:")

        col.prop(brush, "stroke_method", text="")

        if brush.use_anchor:
            col.separator()
            col.prop(brush, "use_edge_to_edge", "Edge To Edge")

        if brush.use_airbrush:
            col.separator()
            col.prop(brush, "rate", text="Rate", slider=True)

        if brush.use_space:
            col.separator()
            row = col.row(align=True)
            row.prop(brush, "spacing", text="Spacing")
            row.prop(brush, "use_pressure_spacing", toggle=True, text="")

        if brush.use_line or brush.use_curve:
            col.separator()
            row = col.row(align=True)
            row.prop(brush, "spacing", text="Spacing")

        if brush.use_curve:
            col.separator()
            col.template_ID(brush, "paint_curve", new="paintcurve.new")
            col.operator("paintcurve.draw")

        col = layout.column()
        col.separator()

        row = col.row(align=True)
        row.prop(brush, "use_relative_jitter", icon_only=True)
        if brush.use_relative_jitter:
            row.prop(brush, "jitter", slider=True)
        else:
            row.prop(brush, "jitter_absolute")
        row.prop(brush, "use_pressure_jitter", toggle=True, text="")

        col = layout.column()
        col.separator()

        if brush.brush_capabilities.has_smooth_stroke:
            col.prop(brush, "use_smooth_stroke")

            sub = col.column()
            sub.active = brush.use_smooth_stroke
            sub.prop(brush, "smooth_stroke_radius", text="Radius", slider=True)
            sub.prop(brush, "smooth_stroke_factor", text="Factor", slider=True)

            col.separator()

        col.prop(toolsettings, "input_samples")


class IMAGE_PT_paint_curve(BrushButtonsPanel, Panel):
    bl_label = "Paint Curve"
    bl_options = {'DEFAULT_CLOSED'}
    bl_category = "Tools"

    def draw(self, context):
        layout = self.layout

        toolsettings = context.tool_settings.image_paint
        brush = toolsettings.brush

        layout.template_curve_mapping(brush, "curve")

        col = layout.column(align=True)
        row = col.row(align=True)
        row.operator("brush.curve_preset", icon='SMOOTHCURVE', text="").shape = 'SMOOTH'
        row.operator("brush.curve_preset", icon='SPHERECURVE', text="").shape = 'ROUND'
        row.operator("brush.curve_preset", icon='ROOTCURVE', text="").shape = 'ROOT'
        row.operator("brush.curve_preset", icon='SHARPCURVE', text="").shape = 'SHARP'
        row.operator("brush.curve_preset", icon='LINCURVE', text="").shape = 'LINE'
        row.operator("brush.curve_preset", icon='NOCURVE', text="").shape = 'MAX'


class IMAGE_PT_tools_imagepaint_symmetry(BrushButtonsPanel, Panel):
    bl_category = "Tools"
    bl_context = "imagepaint"
    bl_label = "Tiling"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout

        toolsettings = context.tool_settings
        ipaint = toolsettings.image_paint

        col = layout.column(align=True)
        row = col.row(align=True)
        row.prop(ipaint, "tile_x", text="X", toggle=True)
        row.prop(ipaint, "tile_y", text="Y", toggle=True)


class IMAGE_PT_tools_brush_appearance(BrushButtonsPanel, Panel):
    bl_label = "Appearance"
    bl_options = {'DEFAULT_CLOSED'}
    bl_category = "Options"

    def draw(self, context):
        layout = self.layout

        toolsettings = context.tool_settings.image_paint
        brush = toolsettings.brush

        if brush is None:  # unlikely but can happen
            layout.label(text="Brush Unset")
            return

        col = layout.column(align=True)

        col.prop(toolsettings, "show_brush")
        sub = col.column()
        sub.active = toolsettings.show_brush
        sub.prop(brush, "cursor_color_add", text="")

        col.separator()

        col.prop(brush, "use_custom_icon")
        sub = col.column()
        sub.active = brush.use_custom_icon
        sub.prop(brush, "icon_filepath", text="")


class IMAGE_PT_tools_paint_options(BrushButtonsPanel, Panel):
    bl_label = "Image Paint"
    bl_category = "Options"

    def draw(self, context):
        layout = self.layout

        toolsettings = context.tool_settings
        # brush = toolsettings.image_paint.brush

        ups = toolsettings.unified_paint_settings

        col = layout.column(align=True)
        col.label(text="Unified Settings:")
        row = col.row()
        row.prop(ups, "use_unified_size", text="Size")
        row.prop(ups, "use_unified_strength", text="Strength")
        col.prop(ups, "use_unified_color", text="Color")


class IMAGE_PT_uv_sculpt_curve(Panel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'TOOLS'
    bl_label = "UV Sculpt Curve"
    bl_category = "Tools"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        sima = context.space_data
        toolsettings = context.tool_settings.image_paint
        return sima.show_uvedit and context.tool_settings.use_uv_sculpt and not (sima.show_paint and toolsettings.brush)

    def draw(self, context):
        layout = self.layout

        toolsettings = context.tool_settings
        uvsculpt = toolsettings.uv_sculpt
        brush = uvsculpt.brush

        layout.template_curve_mapping(brush, "curve")

        row = layout.row(align=True)
        row.operator("brush.curve_preset", icon='SMOOTHCURVE', text="").shape = 'SMOOTH'
        row.operator("brush.curve_preset", icon='SPHERECURVE', text="").shape = 'ROUND'
        row.operator("brush.curve_preset", icon='ROOTCURVE', text="").shape = 'ROOT'
        row.operator("brush.curve_preset", icon='SHARPCURVE', text="").shape = 'SHARP'
        row.operator("brush.curve_preset", icon='LINCURVE', text="").shape = 'LINE'
        row.operator("brush.curve_preset", icon='NOCURVE', text="").shape = 'MAX'


class IMAGE_PT_uv_sculpt(Panel, ImagePaintPanel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'TOOLS'
    bl_category = "Tools"
    bl_label = "UV Sculpt"

    @classmethod
    def poll(cls, context):
        sima = context.space_data
        toolsettings = context.tool_settings.image_paint
        return sima.show_uvedit and context.tool_settings.use_uv_sculpt and not (sima.show_paint and toolsettings.brush)

    def draw(self, context):
        layout = self.layout

        toolsettings = context.tool_settings
        uvsculpt = toolsettings.uv_sculpt
        brush = uvsculpt.brush

        if brush:
            col = layout.column()

            row = col.row(align=True)
            self.prop_unified_size(row, context, brush, "size", slider=True, text="Radius")
            self.prop_unified_size(row, context, brush, "use_pressure_size")

            row = col.row(align=True)
            self.prop_unified_strength(row, context, brush, "strength", slider=True, text="Strength")
            self.prop_unified_strength(row, context, brush, "use_pressure_strength")

        col = layout.column()
        col.prop(toolsettings, "uv_sculpt_lock_borders")
        col.prop(toolsettings, "uv_sculpt_all_islands")

        col.prop(toolsettings, "uv_sculpt_tool")
        if toolsettings.uv_sculpt_tool == 'RELAX':
            col.prop(toolsettings, "uv_relax_method")

        col.prop(uvsculpt, "show_brush")


class IMAGE_PT_options_uvs(Panel, UVToolsPanel):
    bl_label = "UV Options"
    bl_category = "Options"

    @classmethod
    def poll(cls, context):
        sima = context.space_data
        return sima.show_uvedit

    def draw(self, context):
        layout = self.layout

        sima = context.space_data
        uv = sima.uv_editor
        toolsettings = context.tool_settings

        col = layout.column(align=True)
        col.prop(toolsettings, "use_uv_sculpt")
        col.prop(uv, "use_live_unwrap")
        col.prop(uv, "use_snap_to_pixels")
        col.prop(uv, "lock_bounds")


class ImageScopesPanel:
    @classmethod
    def poll(cls, context):
        sima = context.space_data
        if not (sima and sima.image):
            return False
        # scopes are not updated in paint modes, hide
        if sima.mode == 'PAINT':
            return False
        ob = context.active_object
        if ob and ob.mode in {'TEXTURE_PAINT', 'EDIT'}:
            return False
        return True


class IMAGE_PT_view_histogram(ImageScopesPanel, Panel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'TOOLS'
    bl_label = "Histogram"
    bl_category = "Scopes"

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
    bl_region_type = 'TOOLS'
    bl_label = "Waveform"
    bl_category = "Scopes"

    def draw(self, context):
        layout = self.layout

        sima = context.space_data

        layout.template_waveform(sima, "scopes")
        row = layout.split(percentage=0.75)
        row.prop(sima.scopes, "waveform_alpha")
        row.prop(sima.scopes, "waveform_mode", text="")


class IMAGE_PT_view_vectorscope(ImageScopesPanel, Panel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'TOOLS'
    bl_label = "Vectorscope"
    bl_category = "Scopes"

    def draw(self, context):
        layout = self.layout

        sima = context.space_data
        layout.template_vectorscope(sima, "scopes")
        layout.prop(sima.scopes, "vectorscope_alpha")


class IMAGE_PT_sample_line(ImageScopesPanel, Panel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'TOOLS'
    bl_label = "Sample Line"
    bl_category = "Scopes"

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
    bl_region_type = 'TOOLS'
    bl_label = "Scope Samples"
    bl_category = "Scopes"

    def draw(self, context):
        layout = self.layout

        sima = context.space_data

        row = layout.row()
        row.prop(sima.scopes, "use_full_resolution")
        sub = row.row()
        sub.active = not sima.scopes.use_full_resolution
        sub.prop(sima.scopes, "accuracy")


# Grease Pencil properties
class IMAGE_PT_grease_pencil(GreasePencilDataPanel, Panel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'UI'

    # NOTE: this is just a wrapper around the generic GP Panel


# Grease Pencil palette colors
class IMAGE_PT_grease_pencil_palettecolor(GreasePencilPaletteColorPanel, Panel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'UI'

    # NOTE: this is just a wrapper around the generic GP Panel


# Grease Pencil drawing tools
class IMAGE_PT_tools_grease_pencil_draw(GreasePencilDrawingToolsPanel, Panel):
    bl_space_type = 'IMAGE_EDITOR'


# Grease Pencil stroke editing tools
class IMAGE_PT_tools_grease_pencil_edit(GreasePencilStrokeEditPanel, Panel):
    bl_space_type = 'IMAGE_EDITOR'


# Grease Pencil stroke sculpting tools
class IMAGE_PT_tools_grease_pencil_sculpt(GreasePencilStrokeSculptPanel, Panel):
    bl_space_type = 'IMAGE_EDITOR'


# Grease Pencil drawing brushes
class IMAGE_PT_tools_grease_pencil_brush(GreasePencilBrushPanel, Panel):
    bl_space_type = 'IMAGE_EDITOR'


# Grease Pencil drawing curves
class IMAGE_PT_tools_grease_pencil_brushcurves(GreasePencilBrushCurvesPanel, Panel):
    bl_space_type = 'IMAGE_EDITOR'


classes = (
    IMAGE_MT_view,
    IMAGE_MT_select,
    IMAGE_MT_brush,
    IMAGE_MT_image,
    IMAGE_MT_image_invert,
    IMAGE_MT_uvs,
    IMAGE_MT_uvs_showhide,
    IMAGE_MT_uvs_proportional,
    IMAGE_MT_uvs_transform,
    IMAGE_MT_uvs_snap,
    IMAGE_MT_uvs_mirror,
    IMAGE_MT_uvs_weldalign,
    IMAGE_MT_uvs_select_mode,
    IMAGE_MT_specials,
    IMAGE_HT_header,
    MASK_MT_editor_menus,
    IMAGE_PT_mask,
    IMAGE_PT_tools_mask_add,
    IMAGE_PT_mask_layers,
    IMAGE_PT_mask_display,
    IMAGE_PT_active_mask_spline,
    IMAGE_PT_active_mask_point,
    IMAGE_PT_image_properties,
    IMAGE_UL_render_slots,
    IMAGE_PT_render_slots,
    IMAGE_PT_view_properties,
    IMAGE_PT_tools_transform_uvs,
    IMAGE_PT_tools_align_uvs,
    IMAGE_PT_tools_uvs,
    IMAGE_PT_options_uvs,
    IMAGE_PT_paint,
    IMAGE_PT_tools_brush_overlay,
    IMAGE_PT_tools_brush_texture,
    IMAGE_PT_tools_mask,
    IMAGE_PT_tools_mask_texture,
    IMAGE_PT_tools_brush_tool,
    IMAGE_PT_paint_stroke,
    IMAGE_PT_paint_curve,
    IMAGE_PT_tools_imagepaint_symmetry,
    IMAGE_PT_tools_brush_appearance,
    IMAGE_PT_tools_paint_options,
    IMAGE_PT_uv_sculpt,
    IMAGE_PT_uv_sculpt_curve,
    IMAGE_PT_view_histogram,
    IMAGE_PT_view_waveform,
    IMAGE_PT_view_vectorscope,
    IMAGE_PT_sample_line,
    IMAGE_PT_scope_sample,
    IMAGE_PT_grease_pencil,
    IMAGE_PT_grease_pencil_palettecolor,
    IMAGE_PT_tools_grease_pencil_draw,
    IMAGE_PT_tools_grease_pencil_edit,
    IMAGE_PT_tools_grease_pencil_sculpt,
    IMAGE_PT_tools_grease_pencil_brush,
    IMAGE_PT_tools_grease_pencil_brushcurves,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
