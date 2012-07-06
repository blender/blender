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
from bl_ui.properties_paint_common import UnifiedPaintPanel


class ImagePaintPanel(UnifiedPaintPanel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'UI'


class BrushButtonsPanel():
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'UI'

    @classmethod
    def poll(cls, context):
        sima = context.space_data
        toolsettings = context.tool_settings.image_paint
        return sima.show_paint and toolsettings.brush


class IMAGE_MT_view(Menu):
    bl_label = "View"

    def draw(self, context):
        layout = self.layout

        sima = context.space_data
        uv = sima.uv_editor
        toolsettings = context.tool_settings

        show_uvedit = sima.show_uvedit

        layout.operator("image.properties", icon='MENU_PANEL')
        layout.operator("image.scopes", icon='MENU_PANEL')

        layout.separator()

        layout.prop(sima, "use_realtime_update")
        if show_uvedit:
            layout.prop(toolsettings, "show_uv_local_view")

        layout.prop(uv, "show_other_objects")

        layout.separator()

        layout.operator("image.view_zoom_in")
        layout.operator("image.view_zoom_out")

        layout.separator()

        ratios = ((1, 8), (1, 4), (1, 2), (1, 1), (2, 1), (4, 1), (8, 1))

        for a, b in ratios:
            layout.operator("image.view_zoom_ratio", text="Zoom" + " %d:%d" % (a, b)).ratio = a / b

        layout.separator()

        if show_uvedit:
            layout.operator("image.view_selected")

        layout.operator("image.view_all")

        layout.separator()

        layout.operator("screen.area_dupli")
        layout.operator("screen.screen_full_area")


class IMAGE_MT_select(Menu):
    bl_label = "Select"

    def draw(self, context):
        layout = self.layout

        layout.operator("uv.select_border").pinned = False
        layout.operator("uv.select_border", text="Border Select Pinned").pinned = True

        layout.separator()

        layout.operator("uv.select_all").action = 'TOGGLE'
        layout.operator("uv.select_all", text="Inverse").action = 'INVERT'
        layout.operator("uv.unlink_selected")

        layout.separator()

        layout.operator("uv.select_pinned")
        layout.operator("uv.select_linked")


class IMAGE_MT_image(Menu):
    bl_label = "Image"

    def draw(self, context):
        layout = self.layout

        sima = context.space_data
        ima = sima.image

        layout.operator("image.new")
        layout.operator("image.open")

        show_render = sima.show_render

        if ima:
            if not show_render:
                layout.operator("image.replace")
                layout.operator("image.reload")

            layout.operator("image.save")
            layout.operator("image.save_as")
            layout.operator("image.save_as", text="Save a Copy").copy = True

            if ima.source == 'SEQUENCE':
                layout.operator("image.save_sequence")

            layout.operator("image.external_edit", "Edit Externally")

            layout.separator()

            layout.menu("IMAGE_MT_image_invert")

            if not show_render:
                layout.separator()

                if ima.packed_file:
                    layout.operator("image.unpack")
                else:
                    layout.operator("image.pack")

                # only for dirty && specific image types, perhaps
                # this could be done in operator poll too
                if ima.is_dirty:
                    if ima.source in {'FILE', 'GENERATED'} and ima.type != 'OPEN_EXR_MULTILAYER':
                        layout.operator("image.pack", text="Pack As PNG").as_png = True

            if not context.tool_settings.use_uv_sculpt:
                layout.separator()
                layout.prop(sima, "use_image_paint")

            layout.separator()


class IMAGE_MT_image_invert(Menu):
    bl_label = "Invert"

    def draw(self, context):
        layout = self.layout

        props = layout.operator("image.invert", text="Invert Image Colors")
        props.invert_r = True
        props.invert_g = True
        props.invert_b = True

        layout.separator()

        props = layout.operator("image.invert", text="Invert Red Channel")
        props.invert_r = True

        props = layout.operator("image.invert", text="Invert Green Channel")
        props.invert_g = True

        props = layout.operator("image.invert", text="Invert Blue Channel")
        props.invert_b = True

        props = layout.operator("image.invert", text="Invert Alpha Channel")
        props.invert_a = True


class IMAGE_MT_uvs_showhide(Menu):
    bl_label = "Show/Hide Faces"

    def draw(self, context):
        layout = self.layout

        layout.operator("uv.reveal")
        layout.operator("uv.hide", text="Hide Selected").unselected = False
        layout.operator("uv.hide", text="Hide Unselected").unselected = True


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
        layout.operator("uv.pin")

        layout.separator()

        layout.operator("uv.pack_islands")
        layout.operator("uv.average_islands_scale")
        layout.operator("uv.minimize_stretch")
        layout.operator("uv.stitch")
        layout.operator("uv.mark_seam")
        layout.operator("uv.seams_from_islands")
        layout.operator("mesh.faces_mirror_uv")

        layout.separator()

        layout.menu("IMAGE_MT_uvs_transform")
        layout.menu("IMAGE_MT_uvs_mirror")
        layout.menu("IMAGE_MT_uvs_snap")
        layout.menu("IMAGE_MT_uvs_weldalign")

        layout.separator()

        layout.prop_menu_enum(toolsettings, "proportional_edit")
        layout.prop_menu_enum(toolsettings, "proportional_edit_falloff")

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


class IMAGE_HT_header(Header):
    bl_space_type = 'IMAGE_EDITOR'

    def draw(self, context):
        layout = self.layout

        sima = context.space_data
        ima = sima.image
        iuser = sima.image_user
        toolsettings = context.tool_settings

        show_render = sima.show_render
        # show_paint = sima.show_paint
        show_uvedit = sima.show_uvedit

        row = layout.row(align=True)
        row.template_header()

        # menus
        if context.area.show_menus:
            sub = row.row(align=True)
            sub.menu("IMAGE_MT_view")

            if show_uvedit:
                sub.menu("IMAGE_MT_select")

            if ima and ima.is_dirty:
                sub.menu("IMAGE_MT_image", text="Image*")
            else:
                sub.menu("IMAGE_MT_image", text="Image")

            if show_uvedit:
                sub.menu("IMAGE_MT_uvs")

        layout.template_ID(sima, "image", new="image.new")
        if not show_render:
            layout.prop(sima, "use_image_pin", text="")

        # uv editing
        if show_uvedit:
            uvedit = sima.uv_editor

            layout.prop(uvedit, "pivot_point", text="", icon_only=True)
            layout.prop(toolsettings, "use_uv_select_sync", text="")

            if toolsettings.use_uv_select_sync:
                layout.template_edit_mode_selection()
            else:
                layout.prop(toolsettings, "uv_select_mode", text="", expand=True)
                layout.prop(uvedit, "sticky_select_mode", text="", icon_only=True)

            row = layout.row(align=True)
            row.prop(toolsettings, "proportional_edit", text="", icon_only=True)
            if toolsettings.proportional_edit != 'DISABLED':
                row.prop(toolsettings, "proportional_edit_falloff", text="", icon_only=True)

            row = layout.row(align=True)
            row.prop(toolsettings, "use_snap", text="")
            row.prop(toolsettings, "snap_target", text="")

            mesh = context.edit_object.data
            layout.prop_search(mesh.uv_textures, "active", mesh, "uv_textures", text="")

        if ima:
            # layers
            layout.template_image_layers(ima, iuser)

            # painting
            layout.prop(sima, "use_image_paint", text="")

            # draw options
            row = layout.row(align=True)
            row.prop(sima, "draw_channels", text="", expand=True)

            row = layout.row(align=True)
            if ima.type == 'COMPOSITE':
                row.operator("image.record_composite", icon='REC')
            if ima.type == 'COMPOSITE' and ima.source in {'MOVIE', 'SEQUENCE'}:
                row.operator("image.play_composite", icon='PLAY')

        if show_uvedit or sima.use_image_paint:
            layout.prop(sima, "use_realtime_update", text="", icon_only=True, icon='LOCKED')


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

        layout.template_image(sima, "image", iuser)


class IMAGE_PT_game_properties(Panel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'UI'
    bl_label = "Game Properties"

    @classmethod
    def poll(cls, context):
        sima = context.space_data
        # display even when not in game mode because these settings effect the 3d view
        return (sima and sima.image)  # and (rd.engine == 'BLENDER_GAME')

    def draw(self, context):
        layout = self.layout

        sima = context.space_data
        ima = sima.image

        split = layout.split()

        col = split.column()

        col.prop(ima, "use_animation")
        sub = col.column(align=True)
        sub.active = ima.use_animation
        sub.prop(ima, "frame_start", text="Start")
        sub.prop(ima, "frame_end", text="End")
        sub.prop(ima, "fps", text="Speed")

        col.prop(ima, "use_tiles")
        sub = col.column(align=True)
        sub.active = ima.use_tiles or ima.use_animation
        sub.prop(ima, "tiles_x", text="X")
        sub.prop(ima, "tiles_y", text="Y")

        col = split.column()
        col.label(text="Clamp:")
        col.prop(ima, "use_clamp_x", text="X")
        col.prop(ima, "use_clamp_y", text="Y")
        col.separator()
        col.prop(ima, "mapping", expand=True)


class IMAGE_PT_view_histogram(Panel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'PREVIEW'
    bl_label = "Histogram"

    @classmethod
    def poll(cls, context):
        sima = context.space_data
        return (sima and sima.image)

    def draw(self, context):
        layout = self.layout

        sima = context.space_data
        hist = sima.scopes.histogram

        layout.template_histogram(sima.scopes, "histogram")
        row = layout.row(align=True)
        row.prop(hist, "mode", icon_only=True, expand=True)
        row.prop(hist, "show_line", text="")


class IMAGE_PT_view_waveform(Panel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'PREVIEW'
    bl_label = "Waveform"

    @classmethod
    def poll(cls, context):
        sima = context.space_data
        return (sima and sima.image)

    def draw(self, context):
        layout = self.layout

        sima = context.space_data

        layout.template_waveform(sima, "scopes")
        row = layout.split(percentage=0.75)
        row.prop(sima.scopes, "waveform_alpha")
        row.prop(sima.scopes, "waveform_mode", text="", icon_only=True)


class IMAGE_PT_view_vectorscope(Panel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'PREVIEW'
    bl_label = "Vectorscope"

    @classmethod
    def poll(cls, context):
        sima = context.space_data
        return (sima and sima.image)

    def draw(self, context):
        layout = self.layout

        sima = context.space_data
        layout.template_vectorscope(sima, "scopes")
        layout.prop(sima.scopes, "vectorscope_alpha")


class IMAGE_PT_sample_line(Panel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'PREVIEW'
    bl_label = "Sample Line"

    @classmethod
    def poll(cls, context):
        sima = context.space_data
        return (sima and sima.image)

    def draw(self, context):
        layout = self.layout

        sima = context.space_data
        hist = sima.sample_histogram

        layout.operator("image.sample_line")
        layout.template_histogram(sima, "sample_histogram")
        row = layout.row(align=True)
        row.prop(hist, "mode", expand=True)
        row.prop(hist, "show_line", text="")


class IMAGE_PT_scope_sample(Panel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'PREVIEW'
    bl_label = "Scope Samples"

    @classmethod
    def poll(cls, context):
        sima = context.space_data
        return sima

    def draw(self, context):
        layout = self.layout

        sima = context.space_data

        row = layout.row()
        row.prop(sima.scopes, "use_full_resolution")
        sub = row.row()
        sub.active = not sima.scopes.use_full_resolution
        sub.prop(sima.scopes, "accuracy")


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
        show_uvedit = sima.show_uvedit
        uvedit = sima.uv_editor

        split = layout.split()

        col = split.column()
        if ima:
            col.prop(ima, "display_aspect", text="Aspect Ratio")

            col = split.column()
            col.label(text="Coordinates:")
            col.prop(sima, "show_repeat", text="Repeat")
            if show_uvedit:
                col.prop(uvedit, "show_normalized_coords", text="Normalized")

        elif show_uvedit:
            col.label(text="Coordinates:")
            col.prop(uvedit, "show_normalized_coords", text="Normalized")

        if show_uvedit:

            col = layout.column()
            col.label("Cursor Location:")
            col.row().prop(uvedit, "cursor_location", text="")

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


class IMAGE_PT_paint(Panel, ImagePaintPanel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'UI'
    bl_label = "Paint"

    @classmethod
    def poll(cls, context):
        sima = context.space_data
        return sima.show_paint

    def draw(self, context):
        layout = self.layout

        toolsettings = context.tool_settings.image_paint
        brush = toolsettings.brush

        col = layout.column()
        col.template_ID_preview(toolsettings, "brush", new="brush.add", rows=2, cols=6)

        if brush:
            col = layout.column()
            col.template_color_wheel(brush, "color", value_slider=True)
            col.prop(brush, "color", text="")

            row = col.row(align=True)
            self.prop_unified_size(row, context, brush, "size", slider=True, text="Radius")
            self.prop_unified_size(row, context, brush, "use_pressure_size")

            row = col.row(align=True)
            self.prop_unified_strength(row, context, brush, "strength", slider=True, text="Strength")
            self.prop_unified_strength(row, context, brush, "use_pressure_strength")

            row = col.row(align=True)
            row.prop(brush, "jitter", slider=True)
            row.prop(brush, "use_pressure_jitter", toggle=True, text="")

            col.prop(brush, "blend", text="Blend")

            if brush.image_tool == 'CLONE':
                col.separator()
                col.prop(brush, "clone_image", text="Image")
                col.prop(brush, "clone_alpha", text="Alpha")


class IMAGE_PT_tools_brush_texture(BrushButtonsPanel, Panel):
    bl_label = "Texture"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout

        toolsettings = context.tool_settings.image_paint
        brush = toolsettings.brush

        col = layout.column()
        col.template_ID_preview(brush, "texture", new="texture.new", rows=3, cols=8)
        col.prop(brush, "use_fixed_texture")


class IMAGE_PT_tools_brush_tool(BrushButtonsPanel, Panel):
    bl_label = "Tool"
    bl_options = {'DEFAULT_CLOSED'}

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

    def draw(self, context):
        layout = self.layout

        toolsettings = context.tool_settings.image_paint
        brush = toolsettings.brush

        layout.prop(brush, "use_airbrush")
        row = layout.row()
        row.active = brush.use_airbrush
        row.prop(brush, "rate", slider=True)

        layout.prop(brush, "use_space")
        row = layout.row(align=True)
        row.active = brush.use_space
        row.prop(brush, "spacing", text="Distance", slider=True)
        row.prop(brush, "use_pressure_spacing", toggle=True, text="")

        layout.prop(brush, "use_wrap")


class IMAGE_PT_paint_curve(BrushButtonsPanel, Panel):
    bl_label = "Paint Curve"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout

        toolsettings = context.tool_settings.image_paint
        brush = toolsettings.brush

        layout.template_curve_mapping(brush, "curve")

        row = layout.row(align=True)
        row.operator("brush.curve_preset", icon='SMOOTHCURVE', text="").shape = 'SMOOTH'
        row.operator("brush.curve_preset", icon='SPHERECURVE', text="").shape = 'ROUND'
        row.operator("brush.curve_preset", icon='ROOTCURVE', text="").shape = 'ROOT'
        row.operator("brush.curve_preset", icon='SHARPCURVE', text="").shape = 'SHARP'
        row.operator("brush.curve_preset", icon='LINCURVE', text="").shape = 'LINE'
        row.operator("brush.curve_preset", icon='NOCURVE', text="").shape = 'MAX'


class IMAGE_UV_sculpt_curve(Panel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'UI'
    bl_label = "UV Sculpt Curve"
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


class IMAGE_UV_sculpt(Panel, ImagePaintPanel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'UI'
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

        split = layout.split()
        col = split.column()

        col.prop(toolsettings, "uv_sculpt_lock_borders")
        col.prop(toolsettings, "uv_sculpt_all_islands")

        split = layout.split()
        col = split.column()

        col.prop(toolsettings, "uv_sculpt_tool")

        if toolsettings.uv_sculpt_tool == 'RELAX':
            col.prop(toolsettings, "uv_relax_method")


if __name__ == "__main__":  # only for live edit.
    bpy.utils.register_module(__name__)
