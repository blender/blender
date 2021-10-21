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
from bpy.types import (
    Header,
    Menu,
    Panel,
)
from bpy.app.translations import (
    contexts as i18n_contexts,
    pgettext_iface as iface_,
)
from bl_ui.properties_grease_pencil_common import (
    AnnotationDataPanel,
    AnnotationOnionSkin,
)
from bl_ui.space_toolsystem_common import (
    ToolActivePanelHelper,
)
from rna_prop_ui import PropertyPanel


def selected_sequences_len(context):
    selected_sequences = getattr(context, "selected_sequences", None)
    if selected_sequences is None:
        return 0
    return len(selected_sequences)


def draw_color_balance(layout, color_balance):

    layout.prop(color_balance, "correction_method")

    layout.use_property_split = False

    flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=False)

    if color_balance.correction_method == 'LIFT_GAMMA_GAIN':
        col = flow.column()

        box = col.box()
        split = box.split(factor=0.35)
        col = split.column(align=True)
        col.label(text="Lift:")
        col.separator()
        col.separator()
        col.prop(color_balance, "lift", text="")
        col.prop(color_balance, "invert_lift", text="Invert", icon='ARROW_LEFTRIGHT')
        split.template_color_picker(color_balance, "lift", value_slider=True, cubic=True)

        col = flow.column()

        box = col.box()
        split = box.split(factor=0.35)
        col = split.column(align=True)
        col.label(text="Gamma:")
        col.separator()
        col.separator()
        col.prop(color_balance, "gamma", text="")
        col.prop(color_balance, "invert_gamma", text="Invert", icon='ARROW_LEFTRIGHT')
        split.template_color_picker(color_balance, "gamma", value_slider=True, lock_luminosity=True, cubic=True)

        col = flow.column()

        box = col.box()
        split = box.split(factor=0.35)
        col = split.column(align=True)
        col.label(text="Gain:")
        col.separator()
        col.separator()
        col.prop(color_balance, "gain", text="")
        col.prop(color_balance, "invert_gain", text="Invert", icon='ARROW_LEFTRIGHT')
        split.template_color_picker(color_balance, "gain", value_slider=True, lock_luminosity=True, cubic=True)

    elif color_balance.correction_method == 'OFFSET_POWER_SLOPE':
        col = flow.column()

        box = col.box()
        split = box.split(factor=0.35)
        col = split.column(align=True)
        col.label(text="Offset:")
        col.separator()
        col.separator()
        col.prop(color_balance, "offset", text="")
        col.prop(color_balance, "invert_offset", text="Invert", icon='ARROW_LEFTRIGHT')
        split.template_color_picker(color_balance, "offset", value_slider=True, cubic=True)

        col = flow.column()

        box = col.box()
        split = box.split(factor=0.35)
        col = split.column(align=True)
        col.label(text="Power:")
        col.separator()
        col.separator()
        col.prop(color_balance, "power", text="")
        col.prop(color_balance, "invert_power", text="Invert", icon='ARROW_LEFTRIGHT')
        split.template_color_picker(color_balance, "power", value_slider=True, cubic=True)

        col = flow.column()

        box = col.box()
        split = box.split(factor=0.35)
        col = split.column(align=True)
        col.label(text="Slope:")
        col.separator()
        col.separator()
        col.prop(color_balance, "slope", text="")
        col.prop(color_balance, "invert_slope", text="Invert", icon='ARROW_LEFTRIGHT')
        split.template_color_picker(color_balance, "slope", value_slider=True, cubic=True)


class SEQUENCER_PT_active_tool(ToolActivePanelHelper, Panel):
    bl_space_type = 'SEQUENCE_EDITOR'
    bl_region_type = 'UI'
    bl_category = "Tool"


class SEQUENCER_HT_tool_header(Header):
    bl_space_type = 'SEQUENCE_EDITOR'
    bl_region_type = 'TOOL_HEADER'

    def draw(self, context):
        layout = self.layout

        self.draw_tool_settings(context)

        # TODO: options popover.

    def draw_tool_settings(self, context):
        pass

        layout = self.layout

        # Active Tool
        # -----------
        from bl_ui.space_toolsystem_common import ToolSelectPanelHelper
        # Most callers assign the `tool` & `tool_mode`, currently the result is not used.
        """
        tool = ToolSelectPanelHelper.draw_active_tool_header(context, layout)
        tool_mode = context.mode if tool is None else tool.mode
        """
        # Only draw the header.
        ToolSelectPanelHelper.draw_active_tool_header(context, layout)


class SEQUENCER_HT_header(Header):
    bl_space_type = 'SEQUENCE_EDITOR'

    def draw(self, context):
        layout = self.layout

        st = context.space_data

        layout.template_header()

        layout.prop(st, "view_type", text="")

        SEQUENCER_MT_editor_menus.draw_collapsible(context, layout)

        layout.separator_spacer()

        tool_settings = context.tool_settings
        sequencer_tool_settings = tool_settings.sequencer_tool_settings

        if st.view_type == 'PREVIEW':
            layout.prop(sequencer_tool_settings, "pivot_point", text="", icon_only=True)
            layout.separator_spacer()

        if st.view_type in {'SEQUENCER', 'SEQUENCER_PREVIEW'}:
            row = layout.row(align=True)
            row.prop(sequencer_tool_settings, "overlap_mode", text="")

        if st.view_type == 'SEQUENCER_PREVIEW':
            row = layout.row(align=True)
            row.prop(sequencer_tool_settings, "pivot_point", text="", icon_only=True)

        if st.view_type in {'SEQUENCER', 'SEQUENCER_PREVIEW'}:
            row = layout.row(align=True)
            row.prop(tool_settings, "use_snap_sequencer", text="")
            sub = row.row(align=True)
            sub.popover(panel="SEQUENCER_PT_snapping")
            layout.separator_spacer()

        if st.view_type in {'PREVIEW', 'SEQUENCER_PREVIEW'}:
            layout.prop(st, "display_mode", text="", icon_only=True)
            layout.prop(st, "preview_channels", text="", icon_only=True)

            # Gizmo toggle & popover.
            row = layout.row(align=True)
            # FIXME: place-holder icon.
            row.prop(st, "show_gizmo", text="", toggle=True, icon='GIZMO')
            sub = row.row(align=True)
            sub.active = st.show_gizmo
            sub.popover(
                panel="SEQUENCER_PT_gizmo_display",
                text="",
            )

        row = layout.row(align=True)
        row.prop(st, "show_overlays", text="", icon='OVERLAY')
        sub = row.row(align=True)
        sub.popover(panel="SEQUENCER_PT_overlay", text="")
        sub.active = st.show_overlays


class SEQUENCER_MT_editor_menus(Menu):
    bl_idname = "SEQUENCER_MT_editor_menus"
    bl_label = ""

    def draw(self, context):
        layout = self.layout
        st = context.space_data

        layout.menu("SEQUENCER_MT_view")

        if st.view_type in {'SEQUENCER', 'SEQUENCER_PREVIEW'}:
            layout.menu("SEQUENCER_MT_select")
            if st.show_markers:
                layout.menu("SEQUENCER_MT_marker")
            layout.menu("SEQUENCER_MT_add")
            layout.menu("SEQUENCER_MT_strip")


class SEQUENCER_PT_gizmo_display(Panel):
    bl_space_type = 'SEQUENCE_EDITOR'
    bl_region_type = 'HEADER'
    bl_label = "Gizmo"
    bl_ui_units_x = 8

    def draw(self, context):
        layout = self.layout

        scene = context.scene
        st = context.space_data

        col = layout.column()
        col.label(text="Viewport Gizmos")
        col.separator()

        col.active = st.show_gizmo
        colsub = col.column()
        colsub.prop(st, "show_gizmo_navigate", text="Navigate")
        colsub.prop(st, "show_gizmo_tool", text="Active Tools")
        # colsub.prop(st, "show_gizmo_context", text="Active Object")  # Currently unused.


class SEQUENCER_PT_overlay(Panel):
    bl_space_type = 'SEQUENCE_EDITOR'
    bl_region_type = 'HEADER'
    bl_label = "Overlays"
    bl_ui_units_x = 7

    def draw(self, _context):
        pass


class SEQUENCER_PT_preview_overlay(Panel):
    bl_space_type = 'SEQUENCE_EDITOR'
    bl_region_type = 'HEADER'
    bl_parent_id = 'SEQUENCER_PT_overlay'
    bl_label = "Preview Overlays"

    @classmethod
    def poll(cls, context):
        st = context.space_data
        return st.view_type in {'PREVIEW', 'SEQUENCER_PREVIEW'} and st.display_mode == 'IMAGE'

    def draw(self, context):
        ed = context.scene.sequence_editor
        st = context.space_data
        overlay_settings = st.preview_overlay
        layout = self.layout

        layout.active = st.show_overlays
        layout.prop(overlay_settings, "show_image_outline")
        layout.prop(overlay_settings, "show_cursor")
        layout.prop(ed, "show_overlay_frame", text="Frame Overlay")
        layout.prop(overlay_settings, "show_safe_areas", text="Safe Areas")
        layout.prop(overlay_settings, "show_metadata", text="Metadata")
        layout.prop(overlay_settings, "show_annotation", text="Annotations")


class SEQUENCER_PT_sequencer_overlay(Panel):
    bl_space_type = 'SEQUENCE_EDITOR'
    bl_region_type = 'HEADER'
    bl_parent_id = 'SEQUENCER_PT_overlay'
    bl_label = "Sequencer Overlays"

    @classmethod
    def poll(cls, context):
        st = context.space_data
        return st.view_type in {'SEQUENCER', 'SEQUENCER_PREVIEW'}

    def draw(self, context):
        st = context.space_data
        overlay_settings = st.timeline_overlay
        layout = self.layout

        layout.active = st.show_overlays

        layout.prop(overlay_settings, "show_strip_name", text="Name")
        layout.prop(overlay_settings, "show_strip_source", text="Source")
        layout.prop(overlay_settings, "show_strip_duration", text="Duration")
        layout.prop(overlay_settings, "show_strip_tag_color", text="Color Tags")

        layout.separator()

        layout.prop(overlay_settings, "show_strip_offset", text="Offsets")
        layout.prop(overlay_settings, "show_fcurves", text="F-Curves")
        layout.prop(overlay_settings, "show_thumbnails", text="Thumbnails")
        layout.prop(overlay_settings, "show_grid", text="Grid")

        layout.separator()

        layout.prop_menu_enum(overlay_settings, "waveform_display_type")


class SEQUENCER_MT_view_cache(Menu):
    bl_label = "Cache"

    def draw(self, context):
        layout = self.layout

        ed = context.scene.sequence_editor
        layout.prop(ed, "show_cache")
        layout.separator()

        col = layout.column()
        col.enabled = ed.show_cache

        col.prop(ed, "show_cache_final_out")
        col.prop(ed, "show_cache_raw")
        col.prop(ed, "show_cache_preprocessed")
        col.prop(ed, "show_cache_composite")


class SEQUENCER_MT_range(Menu):
    bl_label = "Range"

    def draw(self, _context):
        layout = self.layout

        layout.operator("anim.previewrange_set", text="Set Preview Range")
        layout.operator("sequencer.set_range_to_strips", text="Set Preview Range to Strips").preview = True
        layout.operator("anim.previewrange_clear", text="Clear Preview Range")

        layout.separator()

        layout.operator("anim.start_frame_set", text="Set Start Frame")
        layout.operator("anim.end_frame_set", text="Set End Frame")
        layout.operator("sequencer.set_range_to_strips", text="Set Frame Range to Strips")


class SEQUENCER_MT_preview_zoom(Menu):
    bl_label = "Fractional Zoom"

    def draw(self, _context):
        layout = self.layout
        layout.operator_context = 'INVOKE_REGION_PREVIEW'

        ratios = ((1, 8), (1, 4), (1, 2), (1, 1), (2, 1), (4, 1), (8, 1))

        for i, (a, b) in enumerate(ratios):
            if i in {3, 4}:  # Draw separators around Zoom 1:1.
                layout.separator()

            layout.operator(
                "sequencer.view_zoom_ratio",
                text=iface_("Zoom %d:%d") % (a, b),
                translate=False,
            ).ratio = a / b
        layout.operator_context = 'INVOKE_DEFAULT'


class SEQUENCER_MT_proxy(Menu):
    bl_label = "Proxy"

    def draw(self, context):
        layout = self.layout

        st = context.space_data
        col = layout.column()
        col.operator("sequencer.enable_proxies", text="Setup")
        col.operator("sequencer.rebuild_proxy", text="Rebuild")
        col.enabled = selected_sequences_len(context) >= 1
        layout.prop(st, "proxy_render_size", text="")


class SEQUENCER_MT_view(Menu):
    bl_label = "View"

    def draw(self, context):
        layout = self.layout

        st = context.space_data
        is_preview = st.view_type in {'PREVIEW', 'SEQUENCER_PREVIEW'}
        is_sequencer_view = st.view_type in {'SEQUENCER', 'SEQUENCER_PREVIEW'}

        if st.view_type == 'PREVIEW':
            # Specifying the REGION_PREVIEW context is needed in preview-only
            # mode, else the lookup for the shortcut will fail in
            # wm_keymap_item_find_props() (see T32595).
            layout.operator_context = 'INVOKE_REGION_PREVIEW'
        layout.prop(st, "show_region_ui")
        layout.prop(st, "show_region_tool_header")
        layout.prop(st, "show_region_toolbar")
        layout.operator_context = 'INVOKE_DEFAULT'

        if is_sequencer_view:
            layout.prop(st, "show_region_hud")

        layout.separator()

        if st.view_type == 'SEQUENCER':
            layout.prop(st, "show_backdrop", text="Preview as Backdrop")
        if is_preview or st.show_backdrop:
            layout.prop(st, "show_transform_preview", text="Preview During Transform")

        layout.separator()

        if is_sequencer_view:
            layout.operator_context = 'INVOKE_REGION_WIN'
            layout.operator("sequencer.view_selected", text="Frame Selected")
            layout.operator("sequencer.view_all")
            layout.operator("view2d.zoom_border", text="Zoom")

        if is_preview:
            layout.operator_context = 'INVOKE_REGION_PREVIEW'
            layout.separator()

            layout.operator("sequencer.view_all_preview", text="Fit Preview in Window")

            if is_sequencer_view:
                layout.menu("SEQUENCER_MT_preview_zoom", text="Fractional Preview Zoom")
            else:
                layout.operator("view2d.zoom_border", text="Zoom")
                layout.menu("SEQUENCER_MT_preview_zoom")

            layout.prop(st, "use_zoom_to_fit")

            if st.display_mode == 'WAVEFORM':
                layout.separator()
                layout.prop(st, "show_separate_color", text="Show Separate Color Channels")

            layout.separator()

            layout.menu("SEQUENCER_MT_proxy")

            layout.operator_context = 'INVOKE_DEFAULT'

        layout.separator()
        layout.operator_context = 'INVOKE_REGION_WIN'
        layout.operator("sequencer.refresh_all", icon='FILE_REFRESH', text="Refresh All")
        layout.operator_context = 'INVOKE_DEFAULT'

        if is_sequencer_view:
            layout.separator()

            layout.operator_context = 'INVOKE_DEFAULT'
            layout.menu("SEQUENCER_MT_navigation")
            layout.menu("SEQUENCER_MT_range")

            layout.separator()
            layout.prop(st, "show_locked_time")

            layout.separator()
            layout.prop(st, "show_seconds")
            layout.prop(st, "show_markers")
            if context.preferences.view.show_developer_ui:
                layout.menu("SEQUENCER_MT_view_cache", text="Show Cache")

        layout.separator()

        layout.operator("render.opengl", text="Sequence Render Image", icon='RENDER_STILL').sequencer = True
        props = layout.operator("render.opengl", text="Sequence Render Animation", icon='RENDER_ANIMATION')
        props.animation = True
        props.sequencer = True

        layout.separator()
        layout.operator("sequencer.export_subtitles", text="Export Subtitles", icon='EXPORT')

        layout.separator()

        # Note that the context is needed for the shortcut to display properly.
        layout.operator_context = 'INVOKE_REGION_PREVIEW' if is_preview else 'INVOKE_REGION_WIN'
        props = layout.operator(
            "wm.context_toggle_enum",
            text="Toggle Sequencer/Preview",
            icon='SEQ_SEQUENCER' if is_preview else 'SEQ_PREVIEW',
        )
        props.data_path = "space_data.view_type"
        props.value_1 = 'SEQUENCER'
        props.value_2 = 'PREVIEW'
        layout.operator_context = 'INVOKE_DEFAULT'

        layout.separator()

        layout.menu("INFO_MT_area")


class SEQUENCER_MT_select_handle(Menu):
    bl_label = "Select Handle"

    def draw(self, _context):
        layout = self.layout

        layout.operator("sequencer.select_handles", text="Both").side = 'BOTH'
        layout.operator("sequencer.select_handles", text="Left").side = 'LEFT'
        layout.operator("sequencer.select_handles", text="Right").side = 'RIGHT'

        layout.separator()

        layout.operator("sequencer.select_handles", text="Both Neighbors").side = 'BOTH_NEIGHBORS'
        layout.operator("sequencer.select_handles", text="Left Neighbor").side = 'LEFT_NEIGHBOR'
        layout.operator("sequencer.select_handles", text="Right Neighbor").side = 'RIGHT_NEIGHBOR'


class SEQUENCER_MT_select_channel(Menu):
    bl_label = "Select Channel"

    def draw(self, _context):
        layout = self.layout

        layout.operator("sequencer.select_side", text="Left").side = 'LEFT'
        layout.operator("sequencer.select_side", text="Right").side = 'RIGHT'
        layout.separator()
        layout.operator("sequencer.select_side", text="Both Sides").side = 'BOTH'


class SEQUENCER_MT_select_linked(Menu):
    bl_label = "Select Linked"

    def draw(self, _context):
        layout = self.layout

        layout.operator("sequencer.select_linked", text="All")
        layout.operator("sequencer.select_less", text="Less")
        layout.operator("sequencer.select_more", text="More")


class SEQUENCER_MT_select(Menu):
    bl_label = "Select"

    def draw(self, _context):
        layout = self.layout

        layout.operator("sequencer.select_all", text="All").action = 'SELECT'
        layout.operator("sequencer.select_all", text="None").action = 'DESELECT'
        layout.operator("sequencer.select_all", text="Invert").action = 'INVERT'

        layout.separator()

        layout.operator("sequencer.select_box", text="Box Select")
        props = layout.operator("sequencer.select_box", text="Box Select (Include Handles)")
        props.include_handles = True

        layout.separator()

        layout.operator_menu_enum("sequencer.select_side_of_frame", "side", text="Side of Frame...")
        layout.menu("SEQUENCER_MT_select_handle", text="Handle")
        layout.menu("SEQUENCER_MT_select_channel", text="Channel")
        layout.menu("SEQUENCER_MT_select_linked", text="Linked")

        layout.separator()
        layout.operator_menu_enum("sequencer.select_grouped", "type", text="Grouped")


class SEQUENCER_MT_marker(Menu):
    bl_label = "Marker"

    def draw(self, context):
        layout = self.layout

        st = context.space_data
        is_sequencer_view = st.view_type in {'SEQUENCER', 'SEQUENCER_PREVIEW'}

        from bl_ui.space_time import marker_menu_generic
        marker_menu_generic(layout, context)

        if is_sequencer_view:
            layout.prop(st, "use_marker_sync")


class SEQUENCER_MT_change(Menu):
    bl_label = "Change"

    def draw(self, context):
        layout = self.layout
        strip = context.active_sequence_strip

        layout.operator_context = 'INVOKE_REGION_WIN'

        layout.operator_menu_enum("sequencer.change_effect_input", "swap")
        layout.operator_menu_enum("sequencer.change_effect_type", "type")
        prop = layout.operator("sequencer.change_path", text="Path/Files")

        if strip:
            strip_type = strip.type

            if strip_type == 'IMAGE':
                prop.filter_image = True
            elif strip_type == 'MOVIE':
                prop.filter_movie = True
            elif strip_type == 'SOUND':
                prop.filter_sound = True


class SEQUENCER_MT_navigation(Menu):
    bl_label = "Navigation"

    def draw(self, _context):
        layout = self.layout

        layout.operator("screen.animation_play")

        layout.separator()

        layout.operator("sequencer.view_frame")

        layout.separator()

        props = layout.operator("sequencer.strip_jump", text="Jump to Previous Strip")
        props.next = False
        props.center = False
        props = layout.operator("sequencer.strip_jump", text="Jump to Next Strip")
        props.next = True
        props.center = False

        layout.separator()

        props = layout.operator("sequencer.strip_jump", text="Jump to Previous Strip (Center)")
        props.next = False
        props.center = True
        props = layout.operator("sequencer.strip_jump", text="Jump to Next Strip (Center)")
        props.next = True
        props.center = True


class SEQUENCER_MT_add(Menu):
    bl_label = "Add"
    bl_translation_context = i18n_contexts.operator_default

    def draw(self, context):

        layout = self.layout
        layout.operator_context = 'INVOKE_REGION_WIN'

        bpy_data_scenes_len = len(bpy.data.scenes)
        if bpy_data_scenes_len > 10:
            layout.operator_context = 'INVOKE_DEFAULT'
            layout.operator("sequencer.scene_strip_add", text="Scene...", icon='SCENE_DATA')
        elif bpy_data_scenes_len > 1:
            layout.operator_menu_enum("sequencer.scene_strip_add", "scene", text="Scene", icon='SCENE_DATA')
        else:
            layout.menu("SEQUENCER_MT_add_empty", text="Scene", icon='SCENE_DATA')
        del bpy_data_scenes_len

        bpy_data_movieclips_len = len(bpy.data.movieclips)
        if bpy_data_movieclips_len > 10:
            layout.operator_context = 'INVOKE_DEFAULT'
            layout.operator("sequencer.movieclip_strip_add", text="Clip...", icon='TRACKER')
        elif bpy_data_movieclips_len > 0:
            layout.operator_menu_enum("sequencer.movieclip_strip_add", "clip", text="Clip", icon='TRACKER')
        else:
            layout.menu("SEQUENCER_MT_add_empty", text="Clip", icon='TRACKER')
        del bpy_data_movieclips_len

        bpy_data_masks_len = len(bpy.data.masks)
        if bpy_data_masks_len > 10:
            layout.operator_context = 'INVOKE_DEFAULT'
            layout.operator("sequencer.mask_strip_add", text="Mask...", icon='MOD_MASK')
        elif bpy_data_masks_len > 0:
            layout.operator_menu_enum("sequencer.mask_strip_add", "mask", text="Mask", icon='MOD_MASK')
        else:
            layout.menu("SEQUENCER_MT_add_empty", text="Mask", icon='MOD_MASK')
        del bpy_data_masks_len

        layout.separator()

        layout.operator("sequencer.movie_strip_add", text="Movie", icon='FILE_MOVIE')
        layout.operator("sequencer.sound_strip_add", text="Sound", icon='FILE_SOUND')
        layout.operator("sequencer.image_strip_add", text="Image/Sequence", icon='FILE_IMAGE')

        layout.separator()

        layout.operator_context = 'INVOKE_REGION_WIN'
        layout.operator("sequencer.effect_strip_add", text="Color", icon='COLOR').type = 'COLOR'
        layout.operator("sequencer.effect_strip_add", text="Text", icon='FONT_DATA').type = 'TEXT'

        layout.separator()

        layout.operator("sequencer.effect_strip_add", text="Adjustment Layer", icon='COLOR').type = 'ADJUSTMENT'

        layout.operator_context = 'INVOKE_DEFAULT'
        layout.menu("SEQUENCER_MT_add_effect", icon='SHADERFX')

        col = layout.column()
        col.menu("SEQUENCER_MT_add_transitions", icon='ARROW_LEFTRIGHT')
        col.enabled = selected_sequences_len(context) >= 2

        col = layout.column()
        col.operator_menu_enum("sequencer.fades_add", "type", text="Fade", icon='IPO_EASE_IN_OUT')
        col.enabled = selected_sequences_len(context) >= 1


class SEQUENCER_MT_add_empty(Menu):
    bl_label = "Empty"

    def draw(self, _context):
        layout = self.layout

        layout.label(text="No Items Available")


class SEQUENCER_MT_add_transitions(Menu):
    bl_label = "Transition"

    def draw(self, context):

        layout = self.layout

        col = layout.column()

        col.operator("sequencer.crossfade_sounds", text="Sound Crossfade")

        col.separator()

        col.operator("sequencer.effect_strip_add", text="Cross").type = 'CROSS'
        col.operator("sequencer.effect_strip_add", text="Gamma Cross").type = 'GAMMA_CROSS'

        col.separator()

        col.operator("sequencer.effect_strip_add", text="Wipe").type = 'WIPE'
        col.enabled = selected_sequences_len(context) >= 2


class SEQUENCER_MT_add_effect(Menu):
    bl_label = "Effect Strip"

    def draw(self, context):

        layout = self.layout
        layout.operator_context = 'INVOKE_REGION_WIN'

        col = layout.column()
        col.operator("sequencer.effect_strip_add", text="Add").type = 'ADD'
        col.operator("sequencer.effect_strip_add", text="Subtract").type = 'SUBTRACT'
        col.operator("sequencer.effect_strip_add", text="Multiply").type = 'MULTIPLY'
        col.operator("sequencer.effect_strip_add", text="Over Drop").type = 'OVER_DROP'
        col.operator("sequencer.effect_strip_add", text="Alpha Over").type = 'ALPHA_OVER'
        col.operator("sequencer.effect_strip_add", text="Alpha Under").type = 'ALPHA_UNDER'
        col.operator("sequencer.effect_strip_add", text="Color Mix").type = 'COLORMIX'
        col.enabled = selected_sequences_len(context) >= 2

        layout.separator()

        layout.operator("sequencer.effect_strip_add", text="Multicam Selector").type = 'MULTICAM'

        layout.separator()

        col = layout.column()
        col.operator("sequencer.effect_strip_add", text="Transform").type = 'TRANSFORM'
        col.operator("sequencer.effect_strip_add", text="Speed Control").type = 'SPEED'

        col.separator()

        col.operator("sequencer.effect_strip_add", text="Glow").type = 'GLOW'
        col.operator("sequencer.effect_strip_add", text="Gaussian Blur").type = 'GAUSSIAN_BLUR'
        col.enabled = selected_sequences_len(context) != 0


class SEQUENCER_MT_strip_image_transform(Menu):
    bl_label = "Image Transform"

    def draw(self, _context):
        layout = self.layout

        layout.operator("sequencer.strip_transform_fit", text="Scale To Fit").fit_method = 'FIT'
        layout.operator("sequencer.strip_transform_fit", text="Scale to Fill").fit_method = 'FILL'
        layout.operator("sequencer.strip_transform_fit", text="Stretch To Fill").fit_method = 'STRETCH'
        layout.separator()

        layout.operator("sequencer.strip_transform_clear", text="Clear Position").property = 'POSITION'
        layout.operator("sequencer.strip_transform_clear", text="Clear Scale").property = 'SCALE'
        layout.operator("sequencer.strip_transform_clear", text="Clear Rotation").property = 'ROTATION'
        layout.operator("sequencer.strip_transform_clear", text="Clear All").property = 'ALL'


class SEQUENCER_MT_strip_transform(Menu):
    bl_label = "Transform"

    def draw(self, _context):
        layout = self.layout

        layout.operator("transform.seq_slide", text="Move")
        layout.operator("transform.transform", text="Move/Extend from Current Frame").mode = 'TIME_EXTEND'
        layout.operator("sequencer.slip", text="Slip Strip Contents")

        layout.separator()
        layout.operator("sequencer.snap")
        layout.operator("sequencer.offset_clear")

        layout.separator()
        layout.operator_menu_enum("sequencer.swap", "side")

        layout.separator()
        layout.operator("sequencer.gap_remove").all = False
        layout.operator("sequencer.gap_insert")


class SEQUENCER_MT_strip_input(Menu):
    bl_label = "Inputs"

    def draw(self, context):
        layout = self.layout
        strip = context.active_sequence_strip

        layout.operator("sequencer.reload", text="Reload Strips")
        layout.operator("sequencer.reload", text="Reload Strips and Adjust Length").adjust_length = True
        prop = layout.operator("sequencer.change_path", text="Change Path/Files")
        layout.operator("sequencer.swap_data", text="Swap Data")

        if strip:
            strip_type = strip.type

            if strip_type == 'IMAGE':
                prop.filter_image = True
            elif strip_type == 'MOVIE':
                prop.filter_movie = True
            elif strip_type == 'SOUND':
                prop.filter_sound = True


class SEQUENCER_MT_strip_lock_mute(Menu):
    bl_label = "Lock/Mute"

    def draw(self, _context):
        layout = self.layout

        layout.operator("sequencer.lock")
        layout.operator("sequencer.unlock")

        layout.separator()

        layout.operator("sequencer.mute").unselected = False
        layout.operator("sequencer.unmute").unselected = False
        layout.operator("sequencer.mute", text="Mute Unselected Strips").unselected = True
        layout.operator("sequencer.unmute", text="Unmute Deselected Strips").unselected = True


class SEQUENCER_MT_strip_effect(Menu):
    bl_label = "Effect Strip"

    def draw(self, _context):
        layout = self.layout

        layout.operator_menu_enum("sequencer.change_effect_input", "swap")
        layout.operator_menu_enum("sequencer.change_effect_type", "type")
        layout.operator("sequencer.reassign_inputs")
        layout.operator("sequencer.swap_inputs")


class SEQUENCER_MT_strip_movie(Menu):
    bl_label = "Movie Strip"

    def draw(self, _context):
        layout = self.layout

        layout.operator("sequencer.rendersize")
        layout.operator("sequencer.deinterlace_selected_movies")


class SEQUENCER_MT_strip(Menu):
    bl_label = "Strip"

    def draw(self, context):
        layout = self.layout

        layout.operator_context = 'INVOKE_REGION_WIN'

        layout.separator()
        layout.menu("SEQUENCER_MT_strip_transform")
        layout.menu("SEQUENCER_MT_strip_image_transform")

        layout.separator()
        layout.operator("sequencer.split", text="Split").type = 'SOFT'
        layout.operator("sequencer.split", text="Hold Split").type = 'HARD'

        layout.separator()
        layout.operator("sequencer.copy", text="Copy")
        layout.operator("sequencer.paste", text="Paste")
        layout.operator("sequencer.duplicate_move")
        layout.operator("sequencer.delete", text="Delete")

        strip = context.active_sequence_strip

        if strip:
            strip_type = strip.type

            if strip_type != 'SOUND':
                layout.separator()
                layout.operator_menu_enum("sequencer.strip_modifier_add", "type", text="Add Modifier")
                layout.operator("sequencer.strip_modifier_copy", text="Copy Modifiers to Selection")

            if strip_type in {
                    'CROSS', 'ADD', 'SUBTRACT', 'ALPHA_OVER', 'ALPHA_UNDER',
                    'GAMMA_CROSS', 'MULTIPLY', 'OVER_DROP', 'WIPE', 'GLOW',
                    'TRANSFORM', 'COLOR', 'SPEED', 'MULTICAM', 'ADJUSTMENT',
                    'GAUSSIAN_BLUR',
            }:
                layout.separator()
                layout.menu("SEQUENCER_MT_strip_effect")
            elif strip_type == 'MOVIE':
                layout.separator()
                layout.menu("SEQUENCER_MT_strip_movie")
            elif strip_type == 'IMAGE':
                layout.separator()
                layout.operator("sequencer.rendersize")
                layout.operator("sequencer.images_separate")
            elif strip_type == 'TEXT':
                layout.separator()
                layout.menu("SEQUENCER_MT_strip_effect")
            elif strip_type == 'META':
                layout.separator()
                layout.operator("sequencer.meta_make")
                layout.operator("sequencer.meta_separate")
                layout.operator("sequencer.meta_toggle", text="Toggle Meta")
            if strip_type != 'META':
                layout.separator()
                layout.operator("sequencer.meta_make")
                layout.operator("sequencer.meta_toggle", text="Toggle Meta")

        layout.separator()
        layout.menu("SEQUENCER_MT_color_tag_picker")

        layout.separator()
        layout.menu("SEQUENCER_MT_strip_lock_mute")

        layout.separator()
        layout.menu("SEQUENCER_MT_strip_input")


class SEQUENCER_MT_context_menu(Menu):
    bl_label = "Sequencer Context Menu"

    def draw(self, context):
        layout = self.layout

        layout.operator_context = 'INVOKE_REGION_WIN'

        layout.operator("sequencer.split", text="Split").type = 'SOFT'

        layout.separator()

        layout.operator("sequencer.copy", text="Copy", icon='COPYDOWN')
        layout.operator("sequencer.paste", text="Paste", icon='PASTEDOWN')
        layout.operator("sequencer.duplicate_move")
        props = layout.operator("wm.call_panel", text="Rename...")
        props.name = "TOPBAR_PT_name"
        props.keep_open = False
        layout.operator("sequencer.delete", text="Delete")

        layout.separator()

        layout.operator("sequencer.slip", text="Slip Strip Contents")
        layout.operator("sequencer.snap")

        layout.separator()

        layout.operator("sequencer.set_range_to_strips", text="Set Preview Range to Strips").preview = True

        layout.separator()

        layout.operator("sequencer.gap_remove").all = False
        layout.operator("sequencer.gap_insert")

        layout.separator()

        strip = context.active_sequence_strip

        if strip:
            strip_type = strip.type
            selected_sequences_count = selected_sequences_len(context)

            if strip_type != 'SOUND':
                layout.separator()
                layout.operator_menu_enum("sequencer.strip_modifier_add", "type", text="Add Modifier")
                layout.operator("sequencer.strip_modifier_copy", text="Copy Modifiers to Selection")

                if selected_sequences_count >= 2:
                    layout.separator()
                    col = layout.column()
                    col.menu("SEQUENCER_MT_add_transitions", text="Add Transition")

            elif selected_sequences_count >= 2:
                layout.separator()
                layout.operator("sequencer.crossfade_sounds", text="Crossfade Sounds")

            if selected_sequences_count >= 1:
                col = layout.column()
                col.operator_menu_enum("sequencer.fades_add", "type", text="Fade")
                layout.operator("sequencer.fades_clear", text="Clear Fade")

            if strip_type in {
                    'CROSS', 'ADD', 'SUBTRACT', 'ALPHA_OVER', 'ALPHA_UNDER',
                    'GAMMA_CROSS', 'MULTIPLY', 'OVER_DROP', 'WIPE', 'GLOW',
                    'TRANSFORM', 'COLOR', 'SPEED', 'MULTICAM', 'ADJUSTMENT',
                    'GAUSSIAN_BLUR',
            }:
                layout.separator()
                layout.menu("SEQUENCER_MT_strip_effect")
            elif strip_type == 'MOVIE':
                layout.separator()
                layout.menu("SEQUENCER_MT_strip_movie")
            elif strip_type == 'IMAGE':
                layout.separator()
                layout.operator("sequencer.rendersize")
                layout.operator("sequencer.images_separate")
            elif strip_type == 'TEXT':
                layout.separator()
                layout.menu("SEQUENCER_MT_strip_effect")
            elif strip_type == 'META':
                layout.separator()
                layout.operator("sequencer.meta_make")
                layout.operator("sequencer.meta_separate")
                layout.operator("sequencer.meta_toggle", text="Toggle Meta")
            if strip_type != 'META':
                layout.separator()
                layout.operator("sequencer.meta_make")
                layout.operator("sequencer.meta_toggle", text="Toggle Meta")

        layout.separator()
        layout.menu("SEQUENCER_MT_color_tag_picker")

        layout.separator()

        layout.menu("SEQUENCER_MT_strip_lock_mute")


class SEQUENCER_MT_preview_context_menu(Menu):
    bl_label = "Sequencer Preview Context Menu"

    def draw(self, context):
        layout = self.layout

        layout.operator_context = 'INVOKE_REGION_WIN'

        props = layout.operator("wm.call_panel", text="Rename...")
        props.name = "TOPBAR_PT_name"
        props.keep_open = False

        # TODO: support in preview.
        # layout.operator("sequencer.delete", text="Delete")

        strip = context.active_sequence_strip

        if strip:
            pass


class SEQUENCER_MT_pivot_pie(Menu):
    bl_label = "Pivot Point"

    def draw(self, context):
        layout = self.layout
        pie = layout.menu_pie()

        tool_settings = context.tool_settings
        sequencer_tool_settings = context.tool_settings.sequencer_tool_settings

        pie.prop_enum(sequencer_tool_settings, "pivot_point", value='CENTER')
        pie.prop_enum(sequencer_tool_settings, "pivot_point", value='CURSOR')
        pie.prop_enum(sequencer_tool_settings, "pivot_point", value='INDIVIDUAL_ORIGINS')
        pie.prop_enum(sequencer_tool_settings, "pivot_point", value='MEDIAN')


class SequencerButtonsPanel:
    bl_space_type = 'SEQUENCE_EDITOR'
    bl_region_type = 'UI'

    @staticmethod
    def has_sequencer(context):
        return (context.space_data.view_type in {'SEQUENCER', 'SEQUENCER_PREVIEW'})

    @classmethod
    def poll(cls, context):
        return cls.has_sequencer(context) and (context.active_sequence_strip is not None)


class SequencerButtonsPanel_Output:
    bl_space_type = 'SEQUENCE_EDITOR'
    bl_region_type = 'UI'

    @staticmethod
    def has_preview(context):
        st = context.space_data
        return (st.view_type in {'PREVIEW', 'SEQUENCER_PREVIEW'}) or st.show_backdrop

    @classmethod
    def poll(cls, context):
        return cls.has_preview(context)


class SequencerColorTagPicker:
    bl_space_type = 'SEQUENCE_EDITOR'
    bl_region_type = 'UI'

    @staticmethod
    def has_sequencer(context):
        return (context.space_data.view_type in {'SEQUENCER', 'SEQUENCER_PREVIEW'})

    @classmethod
    def poll(cls, context):
        return cls.has_sequencer(context) and context.active_sequence_strip is not None


class SEQUENCER_PT_color_tag_picker(SequencerColorTagPicker, Panel):
    bl_label = "Color Tag"
    bl_category = "Strip"
    bl_options = {'HIDE_HEADER', 'INSTANCED'}

    def draw(self, context):
        layout = self.layout

        row = layout.row(align=True)
        row.operator("sequencer.strip_color_tag_set", icon='X').color = 'NONE'
        for i in range(1, 10):
            icon = 'SEQUENCE_COLOR_%02d' % i
            row.operator("sequencer.strip_color_tag_set", icon=icon).color = 'COLOR_%02d' % i


class SEQUENCER_MT_color_tag_picker(SequencerColorTagPicker, Menu):
    bl_label = "Set Color Tag"

    def draw(self, context):
        layout = self.layout

        row = layout.row(align=True)
        row.operator_enum("sequencer.strip_color_tag_set", "color", icon_only=True)


class SEQUENCER_PT_strip(SequencerButtonsPanel, Panel):
    bl_label = ""
    bl_options = {'HIDE_HEADER'}
    bl_category = "Strip"

    def draw(self, context):
        layout = self.layout
        strip = context.active_sequence_strip
        strip_type = strip.type

        if strip_type in {
                'ADD', 'SUBTRACT', 'ALPHA_OVER', 'ALPHA_UNDER', 'MULTIPLY',
                'OVER_DROP', 'GLOW', 'TRANSFORM', 'SPEED', 'MULTICAM',
                'GAUSSIAN_BLUR', 'COLORMIX',
        }:
            icon_header = 'SHADERFX'
        elif strip_type in {
                'CROSS', 'GAMMA_CROSS', 'WIPE',
        }:
            icon_header = 'ARROW_LEFTRIGHT'
        elif strip_type == 'SCENE':
            icon_header = 'SCENE_DATA'
        elif strip_type == 'MOVIECLIP':
            icon_header = 'TRACKER'
        elif strip_type == 'MASK':
            icon_header = 'MOD_MASK'
        elif strip_type == 'MOVIE':
            icon_header = 'FILE_MOVIE'
        elif strip_type == 'SOUND':
            icon_header = 'FILE_SOUND'
        elif strip_type == 'IMAGE':
            icon_header = 'FILE_IMAGE'
        elif strip_type == 'COLOR':
            icon_header = 'COLOR'
        elif strip_type == 'TEXT':
            icon_header = 'FONT_DATA'
        elif strip_type == 'ADJUSTMENT':
            icon_header = 'COLOR'
        elif strip_type == 'META':
            icon_header = 'SEQ_STRIP_META'
        else:
            icon_header = 'SEQ_SEQUENCER'

        row = layout.row(align=True)
        row.use_property_decorate = False
        row.label(text="", icon=icon_header)
        row.separator()
        row.prop(strip, "name", text="")

        sub = row.row(align=True)
        if strip.color_tag == 'NONE':
            sub.popover(panel="SEQUENCER_PT_color_tag_picker", text="", icon='COLOR')
        else:
            icon = 'SEQUENCE_' + strip.color_tag
            sub.popover(panel="SEQUENCER_PT_color_tag_picker", text="", icon=icon)

        row.separator()
        row.prop(strip, "mute", toggle=True, icon_only=True, emboss=False)


class SEQUENCER_PT_adjust_crop(SequencerButtonsPanel, Panel):
    bl_label = "Crop"
    bl_options = {'DEFAULT_CLOSED'}
    bl_category = "Strip"

    @classmethod
    def poll(cls, context):
        if not cls.has_sequencer(context):
            return False

        strip = context.active_sequence_strip
        if not strip:
            return False

        return strip.type != 'SOUND'

    def draw(self, context):
        strip = context.active_sequence_strip
        layout = self.layout
        layout.use_property_split = True
        layout.active = not strip.mute

        col = layout.column(align=True)
        col.prop(strip.crop, "min_x")
        col.prop(strip.crop, "max_x")
        col.prop(strip.crop, "max_y")
        col.prop(strip.crop, "min_y")


class SEQUENCER_PT_effect(SequencerButtonsPanel, Panel):
    bl_label = "Effect Strip"
    bl_category = "Strip"

    @classmethod
    def poll(cls, context):
        if not cls.has_sequencer(context):
            return False

        strip = context.active_sequence_strip
        if not strip:
            return False

        return strip.type in {
            'ADD', 'SUBTRACT', 'ALPHA_OVER', 'ALPHA_UNDER',
            'CROSS', 'GAMMA_CROSS', 'MULTIPLY', 'OVER_DROP',
            'WIPE', 'GLOW', 'TRANSFORM', 'COLOR', 'SPEED',
            'MULTICAM', 'GAUSSIAN_BLUR', 'TEXT', 'COLORMIX'
        }

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        strip = context.active_sequence_strip

        layout.active = not strip.mute

        if strip.input_count > 0:
            col = layout.column()
            row = col.row()
            row.prop(strip, "input_1")

            if strip.input_count > 1:
                row.operator("sequencer.swap_inputs", text="", icon='SORT_ASC')
                row = col.row()
                row.prop(strip, "input_2")
                row.operator("sequencer.swap_inputs", text="", icon='SORT_DESC')

        strip_type = strip.type

        if strip_type == 'COLOR':
            layout.template_color_picker(strip, "color", value_slider=True, cubic=True)
            layout.prop(strip, "color", text="")

        elif strip_type == 'WIPE':
            col = layout.column()
            col.prop(strip, "transition_type")
            col.alignment = 'RIGHT'
            col.row().prop(strip, "direction", expand=True)

            col = layout.column()
            col.prop(strip, "blur_width", slider=True)
            if strip.transition_type in {'SINGLE', 'DOUBLE'}:
                col.prop(strip, "angle")

        elif strip_type == 'GLOW':
            flow = layout.column_flow()
            flow.prop(strip, "threshold", slider=True)
            flow.prop(strip, "clamp", slider=True)
            flow.prop(strip, "boost_factor")
            flow.prop(strip, "blur_radius")
            flow.prop(strip, "quality", slider=True)
            flow.prop(strip, "use_only_boost")

        elif strip_type == 'SPEED':
            col = layout.column(align=True)
            col.prop(strip, "speed_control", text="Speed Control")
            if strip.speed_control == 'MULTIPLY':
                col.prop(strip, "speed_factor", text=" ")
            elif strip.speed_control == 'LENGTH':
                col.prop(strip, "speed_length", text=" ")
            elif strip.speed_control == 'FRAME_NUMBER':
                col.prop(strip, "speed_frame_number", text=" ")

            row = layout.row(align=True)
            row.enabled = strip.speed_control != 'STRETCH'
            row = layout.row(align=True, heading="Interpolation")
            row.prop(strip, "use_frame_interpolate", text="")

        elif strip_type == 'TRANSFORM':
            col = layout.column()

            col.prop(strip, "interpolation")
            col.prop(strip, "translation_unit")
            col = layout.column(align=True)
            col.prop(strip, "translate_start_x", text="Position X")
            col.prop(strip, "translate_start_y", text="Y")

            col.separator()

            colsub = col.column(align=True)
            colsub.prop(strip, "use_uniform_scale")
            if strip.use_uniform_scale:
                colsub = col.column(align=True)
                colsub.prop(strip, "scale_start_x", text="Scale")
            else:
                col.prop(strip, "scale_start_x", text="Scale X")
                col.prop(strip, "scale_start_y", text="Y")

            col.separator()

            col.prop(strip, "rotation_start", text="Rotation")

        elif strip_type == 'MULTICAM':
            col = layout.column(align=True)
            strip_channel = strip.channel

            col.prop(strip, "multicam_source", text="Source Channel")

            # The multicam strip needs at least 2 strips to be useful
            if strip_channel > 2:
                BT_ROW = 4
                col.label(text="Cut To")
                row = col.row()

                for i in range(1, strip_channel):
                    if (i % BT_ROW) == 1:
                        row = col.row(align=True)

                    # Workaround - .enabled has to have a separate UI block to work
                    if i == strip.multicam_source:
                        sub = row.row(align=True)
                        sub.enabled = False
                        sub.operator("sequencer.split_multicam", text="%d" % i).camera = i
                    else:
                        sub_1 = row.row(align=True)
                        sub_1.enabled = True
                        sub_1.operator("sequencer.split_multicam", text="%d" % i).camera = i

                if strip.channel > BT_ROW and (strip_channel - 1) % BT_ROW:
                    for i in range(strip.channel, strip_channel + ((BT_ROW + 1 - strip_channel) % BT_ROW)):
                        row.label(text="")
            else:
                col.separator()
                col.label(text="Two or more channels are needed below this strip", icon='INFO')

        elif strip_type == 'TEXT':
            layout = self.layout
            col = layout.column()
            col.scale_x = 1.3
            col.scale_y = 1.3
            col.use_property_split = False
            col.prop(strip, "text", text="")
            col.use_property_split = True
            layout.prop(strip, "wrap_width", text="Wrap Width")

        col = layout.column(align=True)
        if strip_type in {'CROSS', 'GAMMA_CROSS', 'WIPE', 'ALPHA_OVER', 'ALPHA_UNDER', 'OVER_DROP'}:
            col.prop(strip, "use_default_fade", text="Default Fade")
            if not strip.use_default_fade:
                col.prop(strip, "effect_fader", text="Effect Fader")
        elif strip_type == 'GAUSSIAN_BLUR':
            col = layout.column(align=True)
            col.prop(strip, "size_x", text="Size X")
            col.prop(strip, "size_y", text="Y")
        elif strip_type == 'COLORMIX':
            layout.prop(strip, "blend_effect", text="Blend Mode")
            row = layout.row(align=True)
            row.prop(strip, "factor", slider=True)


class SEQUENCER_PT_effect_text_layout(SequencerButtonsPanel, Panel):
    bl_label = "Layout"
    bl_parent_id = "SEQUENCER_PT_effect"
    bl_category = "Strip"

    @classmethod
    def poll(cls, context):
        strip = context.active_sequence_strip
        return strip.type == 'TEXT'

    def draw(self, context):
        strip = context.active_sequence_strip
        layout = self.layout
        layout.use_property_split = True
        col = layout.column()
        col.prop(strip, "location", text="Location")
        col.prop(strip, "align_x", text="Anchor X")
        col.prop(strip, "align_y", text="Y")


class SEQUENCER_PT_effect_text_style(SequencerButtonsPanel, Panel):
    bl_label = "Style"
    bl_parent_id = "SEQUENCER_PT_effect"
    bl_category = "Strip"

    @classmethod
    def poll(cls, context):
        strip = context.active_sequence_strip
        return strip.type == 'TEXT'

    def draw(self, context):
        strip = context.active_sequence_strip
        layout = self.layout
        layout.use_property_split = True
        col = layout.column()

        row = col.row(align=True)
        row.use_property_decorate = False
        row.template_ID(strip, "font", open="font.open", unlink="font.unlink")
        row.prop(strip, "use_bold", text="", icon='BOLD')
        row.prop(strip, "use_italic", text="", icon='ITALIC')

        col.prop(strip, "font_size")
        col.prop(strip, "color")

        row = layout.row(align=True, heading="Shadow")
        row.use_property_decorate = False
        sub = row.row(align=True)
        sub.prop(strip, "use_shadow", text="")
        subsub = sub.row(align=True)
        subsub.active = strip.use_shadow and (not strip.mute)
        subsub.prop(strip, "shadow_color", text="")
        row.prop_decorator(strip, "shadow_color")

        row = layout.row(align=True, heading="Box")
        row.use_property_decorate = False
        sub = row.row(align=True)
        sub.prop(strip, "use_box", text="")
        subsub = sub.row(align=True)
        subsub.active = strip.use_box and (not strip.mute)
        subsub.prop(strip, "box_color", text="")
        row.prop_decorator(strip, "box_color")

        row = layout.row(align=True, heading="Box Margin")
        sub = row.row(align=True)
        sub.prop(strip, "box_margin")
        sub.active = strip.use_box and (not strip.mute)


class SEQUENCER_PT_source(SequencerButtonsPanel, Panel):
    bl_label = "Source"
    bl_options = {'DEFAULT_CLOSED'}
    bl_category = "Strip"

    @classmethod
    def poll(cls, context):
        if not cls.has_sequencer(context):
            return False

        strip = context.active_sequence_strip
        if not strip:
            return False

        return strip.type in {'MOVIE', 'IMAGE', 'SOUND'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        scene = context.scene
        strip = context.active_sequence_strip
        strip_type = strip.type

        layout.active = not strip.mute

        # Draw a filename if we have one.
        if strip_type == 'SOUND':
            sound = strip.sound
            layout.template_ID(strip, "sound", open="sound.open")
            if sound is not None:

                col = layout.column()
                col.prop(sound, "filepath", text="")

                col.alignment = 'RIGHT'
                sub = col.column(align=True)
                split = sub.split(factor=0.5, align=True)
                split.alignment = 'RIGHT'
                if sound.packed_file:
                    split.label(text="Unpack")
                    split.operator("sound.unpack", icon='PACKAGE', text="")
                else:
                    split.label(text="Pack")
                    split.operator("sound.pack", icon='UGLYPACKAGE', text="")

                layout.prop(sound, "use_memory_cache")
        else:
            if strip_type == 'IMAGE':
                col = layout.column()
                col.prop(strip, "directory", text="")

                # Current element for the filename.
                elem = strip.strip_elem_from_frame(scene.frame_current)
                if elem:
                    col.prop(elem, "filename", text="")  # strip.elements[0] could be a fallback

                col.prop(strip.colorspace_settings, "name", text="Color Space")

                col.prop(strip, "alpha_mode", text="Alpha")
                sub = col.column(align=True)
                sub.operator("sequencer.change_path", text="Change Data/Files", icon='FILEBROWSER').filter_image = True
            else:  # elif strip_type == 'MOVIE':
                elem = strip.elements[0]

                col = layout.column()
                col.prop(strip, "filepath", text="")
                col.prop(strip.colorspace_settings, "name", text="Color Space")
                col.prop(strip, "stream_index")
                col.prop(strip, "use_deinterlace")

            if scene.render.use_multiview:
                layout.prop(strip, "use_multiview")

                col = layout.column()
                col.active = strip.use_multiview

                col.row().prop(strip, "views_format", expand=True)

                box = col.box()
                box.active = strip.views_format == 'STEREO_3D'
                box.template_image_stereo_3d(strip.stereo_3d_format)

            # Resolution.
            col = layout.box()
            col = col.column(align=True)
            split = col.split(factor=0.5, align=False)
            split.alignment = 'RIGHT'
            split.label(text="Resolution")
            size = (elem.orig_width, elem.orig_height) if elem else (0, 0)
            if size[0] and size[1]:
                split.alignment = 'LEFT'
                split.label(text="%dx%d" % size, translate=False)
            else:
                split.label(text="None")
            # FPS
            if elem.orig_fps:
                split = col.split(factor=0.5, align=False)
                split.alignment = 'RIGHT'
                split.label(text="FPS")
                split.alignment = 'LEFT'
                split.label(text="%.2f" % elem.orig_fps, translate=False)


class SEQUENCER_PT_scene(SequencerButtonsPanel, Panel):
    bl_label = "Scene"
    bl_category = "Strip"

    @classmethod
    def poll(cls, context):
        if not cls.has_sequencer(context):
            return False

        strip = context.active_sequence_strip
        if not strip:
            return False

        return (strip.type == 'SCENE')

    def draw(self, context):
        strip = context.active_sequence_strip
        scene = strip.scene

        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False
        layout.active = not strip.mute

        layout.template_ID(strip, "scene", text="Scene")
        layout.prop(strip, "scene_input", text="Input")

        if strip.scene_input == 'CAMERA':
            layout.template_ID(strip, "scene_camera", text="Camera")

        if scene:
            # Build a manual split layout as a hack to get proper alignment with the rest of the buttons.
            sub = layout.row(align=True)
            sub.use_property_decorate = True
            split = sub.split(factor=0.4, align=True)
            split.alignment = 'RIGHT'
            split.label(text="Volume")
            split.prop(scene, "audio_volume", text="")
            sub.use_property_decorate = False

        if strip.scene_input == 'CAMERA':
            layout = layout.column(heading="Show")
            layout.prop(strip, "use_annotations", text="Annotations")
            if scene:
                # Warning, this is not a good convention to follow.
                # Expose here because setting the alpha from the 'Render' menu is very inconvenient.
                layout.prop(scene.render, "film_transparent")


class SEQUENCER_PT_mask(SequencerButtonsPanel, Panel):
    bl_label = "Mask"
    bl_category = "Strip"

    @classmethod
    def poll(cls, context):
        if not cls.has_sequencer(context):
            return False

        strip = context.active_sequence_strip
        if not strip:
            return False

        return (strip.type == 'MASK')

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        strip = context.active_sequence_strip

        layout.active = not strip.mute

        layout.template_ID(strip, "mask")

        mask = strip.mask

        if mask:
            sta = mask.frame_start
            end = mask.frame_end
            layout.label(text=iface_("Original frame range: %d-%d (%d)") % (sta, end, end - sta + 1), translate=False)


class SEQUENCER_PT_time(SequencerButtonsPanel, Panel):
    bl_label = "Time"
    bl_options = {'DEFAULT_CLOSED'}
    bl_category = "Strip"

    @classmethod
    def poll(cls, context):
        if not cls.has_sequencer(context):
            return False

        strip = context.active_sequence_strip
        if not strip:
            return False

        return strip.type

    def draw_header_preset(self, context):
        layout = self.layout
        layout.alignment = 'RIGHT'
        strip = context.active_sequence_strip

        layout.prop(strip, "lock", text="", icon_only=True, emboss=False)

    def draw(self, context):
        from bpy.utils import smpte_from_frame

        layout = self.layout
        layout.use_property_split = False
        layout.use_property_decorate = False

        scene = context.scene
        frame_current = scene.frame_current
        strip = context.active_sequence_strip

        is_effect = isinstance(strip, bpy.types.EffectSequence)

        # Get once.
        frame_start = strip.frame_start
        frame_final_start = strip.frame_final_start
        frame_final_end = strip.frame_final_end
        frame_final_duration = strip.frame_final_duration
        frame_offset_start = strip.frame_offset_start
        frame_offset_end = strip.frame_offset_end

        length_list = (
            str(frame_start),
            str(frame_final_end),
            str(frame_final_duration),
            str(frame_offset_start),
            str(frame_offset_end),
        )

        if not is_effect:
            length_list = length_list + (
                str(strip.animation_offset_start),
                str(strip.animation_offset_end),
            )

        max_length = max(len(x) for x in length_list)
        max_factor = (1.9 - max_length) / 30

        layout.enabled = not strip.lock
        layout.active = not strip.mute

        sub = layout.row(align=True)
        split = sub.split(factor=0.5 + max_factor)
        split.alignment = 'RIGHT'
        split.label(text="Channel")
        split.prop(strip, "channel", text="")

        sub = layout.column(align=True)
        split = sub.split(factor=0.5 + max_factor, align=True)
        split.alignment = 'RIGHT'
        split.label(text="Start")
        split.prop(strip, "frame_start", text=smpte_from_frame(frame_start))

        split = sub.split(factor=0.5 + max_factor, align=True)
        split.alignment = 'RIGHT'
        split.label(text="Duration")
        split.prop(strip, "frame_final_duration", text=smpte_from_frame(frame_final_duration))

        # Use label, editing this value from the UI allows negative values,
        # users can adjust duration.
        split = sub.split(factor=0.5 + max_factor, align=True)
        split.alignment = 'RIGHT'
        split.label(text="End")
        split = split.split(factor=0.8 + max_factor, align=True)
        split.label(text="%14s" % smpte_from_frame(frame_final_end))
        split.alignment = 'RIGHT'
        split.label(text=str(frame_final_end) + " ")

        if not is_effect:

            layout.alignment = 'RIGHT'
            sub = layout.column(align=True)

            split = sub.split(factor=0.5 + max_factor, align=True)
            split.alignment = 'RIGHT'
            split.label(text="Strip Offset Start")
            split.prop(strip, "frame_offset_start", text=smpte_from_frame(frame_offset_start))

            split = sub.split(factor=0.5 + max_factor, align=True)
            split.alignment = 'RIGHT'
            split.label(text="End")
            split.prop(strip, "frame_offset_end", text=smpte_from_frame(frame_offset_end))

            layout.alignment = 'RIGHT'
            sub = layout.column(align=True)

            split = sub.split(factor=0.5 + max_factor, align=True)
            split.alignment = 'RIGHT'
            split.label(text="Hold Offset Start")
            split.prop(strip, "animation_offset_start", text=smpte_from_frame(strip.animation_offset_start))

            split = sub.split(factor=0.5 + max_factor, align=True)
            split.alignment = 'RIGHT'
            split.label(text="End")
            split.prop(strip, "animation_offset_end", text=smpte_from_frame(strip.animation_offset_end))

        col = layout.column(align=True)
        col = col.box()
        col.active = (
            (frame_current >= frame_final_start) and
            (frame_current <= frame_final_start + frame_final_duration)
        )

        split = col.split(factor=0.5 + max_factor, align=True)
        split.alignment = 'RIGHT'
        split.label(text="Current Frame")
        split = split.split(factor=0.8 + max_factor, align=True)
        frame_display = frame_current - frame_final_start
        split.label(text="%14s" % smpte_from_frame(frame_display))
        split.alignment = 'RIGHT'
        split.label(text=str(frame_display) + " ")

        if strip.type == 'SCENE':
            scene = strip.scene

            if scene:
                sta = scene.frame_start
                end = scene.frame_end
                split = col.split(factor=0.5 + max_factor)
                split.alignment = 'RIGHT'
                split.label(text="Original Frame Range")
                split.alignment = 'LEFT'
                split.label(text="%d-%d (%d)" % (sta, end, end - sta + 1), translate=False)


class SEQUENCER_PT_adjust_sound(SequencerButtonsPanel, Panel):
    bl_label = "Sound"
    bl_category = "Strip"

    @classmethod
    def poll(cls, context):
        if not cls.has_sequencer(context):
            return False

        strip = context.active_sequence_strip
        if not strip:
            return False

        return strip.type == 'SOUND'

    def draw(self, context):
        layout = self.layout

        st = context.space_data
        overlay_settings = st.timeline_overlay
        strip = context.active_sequence_strip
        sound = strip.sound

        layout.active = not strip.mute

        if sound is not None:
            layout.use_property_split = True
            col = layout.column()

            split = col.split(factor=0.4)
            split.alignment = 'RIGHT'
            split.label(text="Volume")
            split.prop(strip, "volume", text="")

            split = col.split(factor=0.4)
            split.alignment = 'RIGHT'
            split.label(text="Pitch")
            split.prop(strip, "pitch", text="")

            audio_channels = context.scene.render.ffmpeg.audio_channels
            pan_enabled = sound.use_mono and audio_channels != 'MONO'
            pan_text = "%.2f°" % (strip.pan * 90)

            split = col.split(factor=0.4)
            split.alignment = 'RIGHT'
            split.label(text="Pan")
            split.prop(strip, "pan", text="")
            split.enabled = pan_enabled

            if audio_channels != 'MONO' and audio_channels != 'STEREO':
                split = col.split(factor=0.4)
                split.alignment = 'RIGHT'
                split.label(text="Pan Angle")
                split.enabled = pan_enabled
                subsplit = split.row()
                subsplit.alignment = 'CENTER'
                subsplit.label(text=pan_text)
                subsplit.label(text=" ")  # Compensate for no decorate.
                subsplit.enabled = pan_enabled

            layout.use_property_split = False
            col = layout.column()

            split = col.split(factor=0.4)
            split.label(text="")
            split.prop(sound, "use_mono")
            if overlay_settings.waveform_display_type == 'DEFAULT_WAVEFORMS':
                split = col.split(factor=0.4)
                split.label(text="")
                split.prop(strip, "show_waveform")



class SEQUENCER_PT_adjust_comp(SequencerButtonsPanel, Panel):
    bl_label = "Compositing"
    bl_category = "Strip"

    @classmethod
    def poll(cls, context):
        if not cls.has_sequencer(context):
            return False

        strip = context.active_sequence_strip
        if not strip:
            return False

        return strip.type != 'SOUND'

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        strip = context.active_sequence_strip

        layout.active = not strip.mute

        col = layout.column()
        col.prop(strip, "blend_type", text="Blend")
        col.prop(strip, "blend_alpha", text="Opacity", slider=True)


class SEQUENCER_PT_adjust_transform(SequencerButtonsPanel, Panel):
    bl_label = "Transform"
    bl_category = "Strip"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        if not cls.has_sequencer(context):
            return False

        strip = context.active_sequence_strip
        if not strip:
            return False

        return strip.type != 'SOUND'

    def draw(self, context):
        strip = context.active_sequence_strip
        layout = self.layout
        layout.use_property_split = True
        layout.active = not strip.mute

        col = layout.column(align=True)
        col.prop(strip.transform, "offset_x", text="Position X")
        col.prop(strip.transform, "offset_y", text="Y")

        col = layout.column(align=True)
        col.prop(strip.transform, "scale_x", text="Scale X")
        col.prop(strip.transform, "scale_y", text="Y")

        col = layout.column(align=True)
        col.prop(strip.transform, "rotation", text="Rotation")

        col = layout.column(align=True)
        col.prop(strip.transform, "origin")

        row = layout.row(heading="Mirror")
        sub = row.row(align=True)
        sub.prop(strip, "use_flip_x", text="X", toggle=True)
        sub.prop(strip, "use_flip_y", text="Y", toggle=True)


class SEQUENCER_PT_adjust_video(SequencerButtonsPanel, Panel):
    bl_label = "Video"
    bl_options = {'DEFAULT_CLOSED'}
    bl_category = "Strip"

    @classmethod
    def poll(cls, context):
        if not cls.has_sequencer(context):
            return False

        strip = context.active_sequence_strip
        if not strip:
            return False

        return strip.type in {
            'MOVIE', 'IMAGE', 'SCENE', 'MOVIECLIP', 'MASK',
            'META', 'ADD', 'SUBTRACT', 'ALPHA_OVER',
            'ALPHA_UNDER', 'CROSS', 'GAMMA_CROSS', 'MULTIPLY',
            'OVER_DROP', 'WIPE', 'GLOW', 'TRANSFORM', 'COLOR',
            'MULTICAM', 'SPEED', 'ADJUSTMENT', 'COLORMIX'
        }

    def draw(self, context):
        layout = self.layout

        layout.use_property_split = True

        col = layout.column()

        strip = context.active_sequence_strip

        layout.active = not strip.mute

        col.prop(strip, "strobe")

        if strip.type == 'MOVIECLIP':
            col = layout.column()
            col.label(text="Tracker")
            col.prop(strip, "stabilize2d")

            col = layout.column()
            col.label(text="Distortion")
            col.prop(strip, "undistort")
            col.separator()

        col.prop(strip, "use_reverse_frames")


class SEQUENCER_PT_adjust_color(SequencerButtonsPanel, Panel):
    bl_label = "Color"
    bl_options = {'DEFAULT_CLOSED'}
    bl_category = "Strip"

    @classmethod
    def poll(cls, context):
        if not cls.has_sequencer(context):
            return False

        strip = context.active_sequence_strip
        if not strip:
            return False

        return strip.type in {
            'MOVIE', 'IMAGE', 'SCENE', 'MOVIECLIP', 'MASK',
            'META', 'ADD', 'SUBTRACT', 'ALPHA_OVER',
            'ALPHA_UNDER', 'CROSS', 'GAMMA_CROSS', 'MULTIPLY',
            'OVER_DROP', 'WIPE', 'GLOW', 'TRANSFORM', 'COLOR',
            'MULTICAM', 'SPEED', 'ADJUSTMENT', 'COLORMIX'
        }

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        strip = context.active_sequence_strip

        layout.active = not strip.mute

        col = layout.column()
        col.prop(strip, "color_saturation", text="Saturation")
        col.prop(strip, "color_multiply", text="Multiply")
        col.prop(strip, "use_float", text="Convert to Float")


class SEQUENCER_PT_cache_settings(SequencerButtonsPanel, Panel):
    bl_label = "Cache Settings"
    bl_category = "Cache"

    @classmethod
    def poll(cls, context):
        show_developer_ui = context.preferences.view.show_developer_ui
        return cls.has_sequencer(context) and context.scene.sequence_editor and show_developer_ui

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        ed = context.scene.sequence_editor

        col = layout.column(heading="Cache", align=True)

        col.prop(ed, "use_cache_raw", text="Raw")
        col.prop(ed, "use_cache_preprocessed", text="Pre-Processed")
        col.prop(ed, "use_cache_composite", text="Composite")
        col.prop(ed, "use_cache_final", text="Final")


class SEQUENCER_PT_proxy_settings(SequencerButtonsPanel, Panel):
    bl_label = "Proxy Settings"
    bl_category = "Proxy"

    @classmethod
    def poll(cls, context):
        return cls.has_sequencer(context) and context.scene.sequence_editor

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        ed = context.scene.sequence_editor
        flow = layout.column_flow()
        flow.prop(ed, "proxy_storage", text="Storage")

        if ed.proxy_storage == 'PROJECT':
            flow.prop(ed, "proxy_dir", text="Directory")

        col = layout.column()
        col.operator("sequencer.enable_proxies")
        col.operator("sequencer.rebuild_proxy")


class SEQUENCER_PT_strip_proxy(SequencerButtonsPanel, Panel):
    bl_label = "Strip Proxy & Timecode"
    bl_category = "Proxy"

    @classmethod
    def poll(cls, context):
        if not cls.has_sequencer(context) and context.scene.sequence_editor:
            return False

        strip = context.active_sequence_strip
        if not strip:
            return False

        return strip.type in {'MOVIE', 'IMAGE'}

    def draw_header(self, context):
        strip = context.active_sequence_strip

        self.layout.prop(strip, "use_proxy", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        ed = context.scene.sequence_editor

        strip = context.active_sequence_strip

        if strip.proxy:
            proxy = strip.proxy

            if ed.proxy_storage == 'PER_STRIP':
                col = layout.column(heading="Custom Proxy")
                col.prop(proxy, "use_proxy_custom_directory", text="Directory")
                if proxy.use_proxy_custom_directory and not proxy.use_proxy_custom_file:
                    col.prop(proxy, "directory")
                col.prop(proxy, "use_proxy_custom_file", text="File")
                if proxy.use_proxy_custom_file:
                    col.prop(proxy, "filepath")

            row = layout.row(heading="Resolutions", align=True)
            row.prop(strip.proxy, "build_25", toggle=True)
            row.prop(strip.proxy, "build_50", toggle=True)
            row.prop(strip.proxy, "build_75", toggle=True)
            row.prop(strip.proxy, "build_100", toggle=True)

            layout.use_property_split = True
            layout.use_property_decorate = False

            layout.prop(proxy, "use_overwrite")

            col = layout.column()
            col.prop(proxy, "quality", text="Quality")

            if strip.type == 'MOVIE':
                col = layout.column()

                col.prop(proxy, "timecode", text="Timecode Index")


class SEQUENCER_PT_strip_cache(SequencerButtonsPanel, Panel):
    bl_label = "Strip Cache"
    bl_category = "Cache"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        show_developer_ui = context.preferences.view.show_developer_ui
        if not cls.has_sequencer(context):
            return False
        if context.active_sequence_strip is not None and show_developer_ui:
            return True
        return False

    def draw_header(self, context):
        strip = context.active_sequence_strip
        self.layout.prop(strip, "override_cache_settings", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        strip = context.active_sequence_strip
        layout.active = strip.override_cache_settings

        col = layout.column(heading="Cache")
        col.prop(strip, "use_cache_raw", text="Raw")
        col.prop(strip, "use_cache_preprocessed", text="Pre-Processed")
        col.prop(strip, "use_cache_composite", text="Composite")


class SEQUENCER_PT_preview(SequencerButtonsPanel_Output, Panel):
    bl_label = "Scene Strip Display"
    bl_space_type = 'SEQUENCE_EDITOR'
    bl_region_type = 'UI'
    bl_options = {'DEFAULT_CLOSED'}
    bl_category = "View"

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        render = context.scene.render

        col = layout.column()
        col.prop(render, "sequencer_gl_preview", text="Shading")

        if render.sequencer_gl_preview in {'SOLID', 'WIREFRAME'}:
            col.prop(render, "use_sequencer_override_scene_strip")


class SEQUENCER_PT_view(SequencerButtonsPanel_Output, Panel):
    bl_label = "View Settings"
    bl_category = "View"

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        st = context.space_data
        ed = context.scene.sequence_editor

        col = layout.column()
        col.prop(st, "proxy_render_size")

        col = layout.column()
        col.prop(st, "use_proxies")
        if st.proxy_render_size in {'NONE', 'SCENE'}:
            col.enabled = False

        col = layout.column()
        if ed:
            col.prop(ed, "use_prefetch")

        col.prop(st, "display_channel", text="Channel")

        if st.display_mode == 'IMAGE':
            col.prop(st, "show_overexposed")

        elif st.display_mode == 'WAVEFORM':
            col.prop(st, "show_separate_color")


class SEQUENCER_PT_view_cursor(SequencerButtonsPanel_Output, Panel):
    bl_category = "View"
    bl_label = "2D Cursor"

    def draw(self, context):
        layout = self.layout

        st = context.space_data

        layout.use_property_split = True
        layout.use_property_decorate = False

        col = layout.column()
        col.prop(st, "cursor_location", text="Location")


class SEQUENCER_PT_frame_overlay(SequencerButtonsPanel_Output, Panel):
    bl_label = "Frame Overlay"
    bl_category = "View"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        if not context.scene.sequence_editor:
            return False
        return SequencerButtonsPanel_Output.poll(context)

    def draw_header(self, context):
        scene = context.scene
        ed = scene.sequence_editor

        self.layout.prop(ed, "show_overlay_frame", text="")

    def draw(self, context):
        layout = self.layout

        layout.operator_context = 'INVOKE_REGION_PREVIEW'
        layout.operator("sequencer.view_ghost_border", text="Set Overlay Region")
        layout.operator_context = 'INVOKE_DEFAULT'

        layout.use_property_split = True
        layout.use_property_decorate = False

        st = context.space_data
        scene = context.scene
        ed = scene.sequence_editor

        layout.active = ed.show_overlay_frame

        col = layout.column()
        col.prop(ed, "overlay_frame", text="Frame Offset")
        col.prop(st, "overlay_frame_type")
        col.prop(ed, "use_overlay_frame_lock")


class SEQUENCER_PT_view_safe_areas(SequencerButtonsPanel_Output, Panel):
    bl_label = "Safe Areas"
    bl_options = {'DEFAULT_CLOSED'}
    bl_category = "View"

    @classmethod
    def poll(cls, context):
        st = context.space_data
        is_preview = st.view_type in {'PREVIEW', 'SEQUENCER_PREVIEW'}
        return is_preview and (st.display_mode == 'IMAGE')

    def draw_header(self, context):
        overlay_settings = context.space_data.preview_overlay
        self.layout.prop(overlay_settings, "show_safe_areas", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        overlay_settings = context.space_data.preview_overlay
        safe_data = context.scene.safe_areas

        layout.active = overlay_settings.show_safe_areas

        col = layout.column()

        sub = col.column()
        sub.prop(safe_data, "title", slider=True)
        sub.prop(safe_data, "action", slider=True)


class SEQUENCER_PT_view_safe_areas_center_cut(SequencerButtonsPanel_Output, Panel):
    bl_label = "Center-Cut Safe Areas"
    bl_parent_id = "SEQUENCER_PT_view_safe_areas"
    bl_options = {'DEFAULT_CLOSED'}
    bl_category = "View"

    def draw_header(self, context):
        layout = self.layout
        overlay_settings = context.space_data.preview_overlay
        layout.active = overlay_settings.show_safe_areas
        layout.prop(overlay_settings, "show_safe_center", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        safe_data = context.scene.safe_areas
        overlay_settings = context.space_data.preview_overlay

        layout.active = overlay_settings.show_safe_areas and overlay_settings.show_safe_center

        col = layout.column()
        col.prop(safe_data, "title_center", slider=True)


class SEQUENCER_PT_modifiers(SequencerButtonsPanel, Panel):
    bl_label = "Modifiers"
    bl_category = "Modifiers"

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        strip = context.active_sequence_strip
        ed = context.scene.sequence_editor

        layout.prop(strip, "use_linear_modifiers")

        layout.operator_menu_enum("sequencer.strip_modifier_add", "type")
        layout.operator("sequencer.strip_modifier_copy")

        for mod in strip.modifiers:
            box = layout.box()

            row = box.row()
            row.use_property_decorate = False
            row.prop(mod, "show_expanded", text="", emboss=False)
            row.prop(mod, "name", text="")

            row.prop(mod, "mute", text="")
            row.use_property_decorate = True

            sub = row.row(align=True)
            props = sub.operator("sequencer.strip_modifier_move", text="", icon='TRIA_UP')
            props.name = mod.name
            props.direction = 'UP'
            props = sub.operator("sequencer.strip_modifier_move", text="", icon='TRIA_DOWN')
            props.name = mod.name
            props.direction = 'DOWN'

            row.operator("sequencer.strip_modifier_remove", text="", icon='X', emboss=False).name = mod.name

            if mod.show_expanded:
                row = box.row()
                row.prop(mod, "input_mask_type", expand=True)

                if mod.input_mask_type == 'STRIP':
                    sequences_object = ed
                    if ed.meta_stack:
                        sequences_object = ed.meta_stack[-1]
                    box.prop_search(mod, "input_mask_strip", sequences_object, "sequences", text="Mask")
                else:
                    box.prop(mod, "input_mask_id")
                    row = box.row()
                    row.prop(mod, "mask_time", expand=True)

                if mod.type == 'COLOR_BALANCE':
                    box.prop(mod, "color_multiply")
                    draw_color_balance(box, mod.color_balance)
                elif mod.type == 'CURVES':
                    box.template_curve_mapping(mod, "curve_mapping", type='COLOR', show_tone=True)
                elif mod.type == 'HUE_CORRECT':
                    box.template_curve_mapping(mod, "curve_mapping", type='HUE')
                elif mod.type == 'BRIGHT_CONTRAST':
                    col = box.column()
                    col.prop(mod, "bright")
                    col.prop(mod, "contrast")
                elif mod.type == 'WHITE_BALANCE':
                    col = box.column()
                    col.prop(mod, "white_value")
                elif mod.type == 'TONEMAP':
                    col = box.column()
                    col.prop(mod, "tonemap_type")
                    if mod.tonemap_type == 'RD_PHOTORECEPTOR':
                        col.prop(mod, "intensity")
                        col.prop(mod, "contrast")
                        col.prop(mod, "adaptation")
                        col.prop(mod, "correction")
                    elif mod.tonemap_type == 'RH_SIMPLE':
                        col.prop(mod, "key")
                        col.prop(mod, "offset")
                        col.prop(mod, "gamma")


class SEQUENCER_PT_annotation(AnnotationDataPanel, SequencerButtonsPanel_Output, Panel):
    bl_space_type = 'SEQUENCE_EDITOR'
    bl_region_type = 'UI'
    bl_category = "View"

    @staticmethod
    def has_preview(context):
        st = context.space_data
        return st.view_type in {'PREVIEW', 'SEQUENCER_PREVIEW'}

    @classmethod
    def poll(cls, context):
        return cls.has_preview(context)

    # NOTE: this is just a wrapper around the generic GP Panel
    # But, it should only show up when there are images in the preview region


class SEQUENCER_PT_annotation_onion(AnnotationOnionSkin, SequencerButtonsPanel_Output, Panel):
    bl_space_type = 'SEQUENCE_EDITOR'
    bl_region_type = 'UI'
    bl_category = "View"

    @staticmethod
    def has_preview(context):
        st = context.space_data
        return st.view_type in {'PREVIEW', 'SEQUENCER_PREVIEW'}

    @classmethod
    def poll(cls, context):
        if context.annotation_data_owner is None:
            return False
        elif type(context.annotation_data_owner) is bpy.types.Object:
            return False
        else:
            gpl = context.active_annotation_layer
            if gpl is None:
                return False

        return cls.has_preview(context)

    # NOTE: this is just a wrapper around the generic GP Panel
    # But, it should only show up when there are images in the preview region


class SEQUENCER_PT_custom_props(SequencerButtonsPanel, PropertyPanel, Panel):
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}
    _context_path = "active_sequence_strip"
    _property_type = (bpy.types.Sequence,)
    bl_category = "Strip"


class SEQUENCER_PT_snapping(Panel):
    bl_space_type = 'SEQUENCE_EDITOR'
    bl_region_type = 'HEADER'
    bl_label = ""

    def draw(self, context):
        tool_settings = context.tool_settings
        sequencer_tool_settings = tool_settings.sequencer_tool_settings

        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        col = layout.column(heading="Snap to", align=True)
        col.prop(sequencer_tool_settings, "snap_to_current_frame")
        col.prop(sequencer_tool_settings, "snap_to_hold_offset")

        col = layout.column(heading="Ignore", align=True)
        col.prop(sequencer_tool_settings, "snap_ignore_muted", text="Muted Strips")
        col.prop(sequencer_tool_settings, "snap_ignore_sound", text="Sound Strips")

        col = layout.column(heading="Current Frame", align=True)
        col.prop(sequencer_tool_settings, "use_snap_current_frame_to_strips", text="Snap to Strips")


classes = (
    SEQUENCER_MT_change,
    SEQUENCER_HT_tool_header,
    SEQUENCER_HT_header,
    SEQUENCER_MT_editor_menus,
    SEQUENCER_MT_range,
    SEQUENCER_MT_view,
    SEQUENCER_MT_view_cache,
    SEQUENCER_MT_preview_zoom,
    SEQUENCER_MT_proxy,
    SEQUENCER_MT_select_handle,
    SEQUENCER_MT_select_channel,
    SEQUENCER_MT_select_linked,
    SEQUENCER_MT_select,
    SEQUENCER_MT_marker,
    SEQUENCER_MT_navigation,
    SEQUENCER_MT_add,
    SEQUENCER_MT_add_effect,
    SEQUENCER_MT_add_transitions,
    SEQUENCER_MT_add_empty,
    SEQUENCER_MT_strip_effect,
    SEQUENCER_MT_strip_movie,
    SEQUENCER_MT_strip,
    SEQUENCER_MT_strip_image_transform,
    SEQUENCER_MT_strip_transform,
    SEQUENCER_MT_strip_input,
    SEQUENCER_MT_strip_lock_mute,
    SEQUENCER_MT_color_tag_picker,
    SEQUENCER_MT_context_menu,
    SEQUENCER_MT_preview_context_menu,
    SEQUENCER_MT_pivot_pie,

    SEQUENCER_PT_color_tag_picker,

    SEQUENCER_PT_active_tool,
    SEQUENCER_PT_strip,

    SEQUENCER_PT_gizmo_display,
    SEQUENCER_PT_overlay,
    SEQUENCER_PT_preview_overlay,
    SEQUENCER_PT_sequencer_overlay,

    SEQUENCER_PT_effect,
    SEQUENCER_PT_scene,
    SEQUENCER_PT_mask,
    SEQUENCER_PT_effect_text_style,
    SEQUENCER_PT_effect_text_layout,

    SEQUENCER_PT_adjust_comp,
    SEQUENCER_PT_adjust_transform,
    SEQUENCER_PT_adjust_crop,
    SEQUENCER_PT_adjust_video,
    SEQUENCER_PT_adjust_color,
    SEQUENCER_PT_adjust_sound,

    SEQUENCER_PT_time,
    SEQUENCER_PT_source,

    SEQUENCER_PT_modifiers,

    SEQUENCER_PT_cache_settings,
    SEQUENCER_PT_strip_cache,
    SEQUENCER_PT_proxy_settings,
    SEQUENCER_PT_strip_proxy,

    SEQUENCER_PT_custom_props,

    SEQUENCER_PT_view,
    SEQUENCER_PT_view_cursor,
    SEQUENCER_PT_frame_overlay,
    SEQUENCER_PT_view_safe_areas,
    SEQUENCER_PT_view_safe_areas_center_cut,
    SEQUENCER_PT_preview,

    SEQUENCER_PT_annotation,
    SEQUENCER_PT_annotation_onion,

    SEQUENCER_PT_snapping,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
