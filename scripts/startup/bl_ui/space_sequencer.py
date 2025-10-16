# SPDX-FileCopyrightText: 2009-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

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
from bl_ui.space_time import playback_controls


def _space_view_types(st):
    view_type = st.view_type
    return (
        view_type in {'SEQUENCER', 'SEQUENCER_PREVIEW'},
        view_type == 'PREVIEW',
    )


def selected_strips_count(context):
    selected_strips = getattr(context, "selected_strips", None)
    if selected_strips is None:
        return 0, 0

    total_count = len(selected_strips)
    nonsound_count = sum(1 for strip in selected_strips if strip.type != 'SOUND')

    return total_count, nonsound_count


class SEQUENCER_PT_active_tool(ToolActivePanelHelper, Panel):
    bl_space_type = 'SEQUENCE_EDITOR'
    bl_region_type = 'UI'
    bl_category = "Tool"


class SEQUENCER_HT_tool_header(Header):
    bl_space_type = 'SEQUENCE_EDITOR'
    bl_region_type = 'TOOL_HEADER'

    def draw(self, context):
        # layout = self.layout

        self.draw_tool_settings(context)

        # TODO: options popover.

    def draw_tool_settings(self, context):
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

        scene = context.sequencer_scene
        tool_settings = scene.tool_settings if scene else None
        sequencer_tool_settings = tool_settings.sequencer_tool_settings if tool_settings else None

        if st.view_type in {'SEQUENCER', 'SEQUENCER_PREVIEW'}:
            row = layout.row(align=True)
            row.template_ID(context.workspace, "sequencer_scene", new="scene.new_sequencer_scene")

        if sequencer_tool_settings and st.view_type == 'PREVIEW':
            layout.prop(sequencer_tool_settings, "pivot_point", text="", icon_only=True)

        if sequencer_tool_settings and st.view_type in {'SEQUENCER', 'SEQUENCER_PREVIEW'}:
            row = layout.row(align=True)
            row.prop(sequencer_tool_settings, "overlap_mode", text="")

        if tool_settings:
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


class SEQUENCER_HT_playback_controls(Header):
    bl_space_type = 'SEQUENCE_EDITOR'
    bl_region_type = 'FOOTER'

    def draw(self, context):
        layout = self.layout

        playback_controls(layout, context)


class SEQUENCER_MT_editor_menus(Menu):
    bl_idname = "SEQUENCER_MT_editor_menus"
    bl_label = ""

    def draw(self, context):
        layout = self.layout
        st = context.space_data
        has_sequencer, _has_preview = _space_view_types(st)

        layout.menu("SEQUENCER_MT_view")
        layout.menu("SEQUENCER_MT_select")

        if has_sequencer and context.sequencer_scene:
            if st.show_markers:
                layout.menu("SEQUENCER_MT_marker")
            layout.menu("SEQUENCER_MT_add")

        layout.menu("SEQUENCER_MT_strip")

        if st.view_type in {'SEQUENCER', 'PREVIEW'}:
            layout.menu("SEQUENCER_MT_image")


class SEQUENCER_PT_gizmo_display(Panel):
    bl_space_type = 'SEQUENCE_EDITOR'
    bl_region_type = 'HEADER'
    bl_label = "Gizmos"
    bl_ui_units_x = 8

    def draw(self, context):
        layout = self.layout

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
    bl_ui_units_x = 13

    def draw(self, _context):
        pass


class SEQUENCER_PT_preview_overlay(Panel):
    bl_space_type = 'SEQUENCE_EDITOR'
    bl_region_type = 'HEADER'
    bl_parent_id = "SEQUENCER_PT_overlay"
    bl_label = "Preview Overlays"

    @classmethod
    def poll(cls, context):
        st = context.space_data
        return st.view_type in {'PREVIEW', 'SEQUENCER_PREVIEW'} and context.sequencer_scene

    def draw(self, context):
        ed = context.sequencer_scene.sequence_editor
        st = context.space_data
        overlay_settings = st.preview_overlay
        layout = self.layout

        layout.active = st.show_overlays and st.display_mode == 'IMAGE'

        split = layout.column().split()
        col = split.column()
        col.prop(overlay_settings, "show_image_outline")
        col.prop(ed, "show_overlay_frame", text="Frame Overlay")
        col.prop(overlay_settings, "show_metadata", text="Metadata")

        col = split.column()
        col.prop(overlay_settings, "show_cursor")
        col.prop(overlay_settings, "show_safe_areas", text="Safe Areas")
        col.prop(overlay_settings, "show_annotation", text="Annotations")


class SEQUENCER_PT_sequencer_overlay(Panel):
    bl_space_type = 'SEQUENCE_EDITOR'
    bl_region_type = 'HEADER'
    bl_parent_id = "SEQUENCER_PT_overlay"
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
        split = layout.column().split()

        col = split.column()
        col.prop(overlay_settings, "show_grid", text="Grid")

        col = split.column()
        col.prop(st.cache_overlay, "show_cache", text="Cache")


class SEQUENCER_PT_sequencer_overlay_strips(Panel):
    bl_space_type = 'SEQUENCE_EDITOR'
    bl_region_type = 'HEADER'
    bl_parent_id = "SEQUENCER_PT_overlay"
    bl_label = "Strips"

    @classmethod
    def poll(cls, context):
        st = context.space_data
        return st.view_type in {'SEQUENCER', 'SEQUENCER_PREVIEW'}

    def draw(self, context):
        st = context.space_data
        overlay_settings = st.timeline_overlay
        layout = self.layout

        layout.active = st.show_overlays
        split = layout.column().split()

        col = split.column()
        col.prop(overlay_settings, "show_strip_name", text="Name")
        col.prop(overlay_settings, "show_strip_source", text="Source")
        col.prop(overlay_settings, "show_strip_duration", text="Duration")
        col.prop(overlay_settings, "show_fcurves", text="Animation Curves")

        col = split.column()
        col.prop(overlay_settings, "show_thumbnails", text="Thumbnails")
        col.prop(overlay_settings, "show_strip_tag_color", text="Color Tags")
        col.prop(overlay_settings, "show_strip_offset", text="Offsets")
        col.prop(overlay_settings, "show_strip_retiming", text="Retiming")


class SEQUENCER_PT_sequencer_overlay_waveforms(Panel):
    bl_space_type = 'SEQUENCE_EDITOR'
    bl_region_type = 'HEADER'
    bl_parent_id = "SEQUENCER_PT_overlay"
    bl_label = "Waveforms"

    @classmethod
    def poll(cls, context):
        st = context.space_data
        return st.view_type in {'SEQUENCER', 'SEQUENCER_PREVIEW'}

    def draw(self, context):
        st = context.space_data
        overlay_settings = st.timeline_overlay
        layout = self.layout

        layout.active = st.show_overlays

        layout.row().prop(overlay_settings, "waveform_display_type", expand=True)

        row = layout.row()
        row.prop(overlay_settings, "waveform_display_style", expand=True)
        row.active = overlay_settings.waveform_display_type != 'NO_WAVEFORMS'


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
    bl_label = "Zoom"

    def draw(self, context):
        layout = self.layout
        layout.operator_context = 'INVOKE_REGION_PREVIEW'
        from math import isclose

        current_zoom = context.space_data.zoom_percentage
        ratios = ((1, 8), (1, 4), (1, 2), (1, 1), (2, 1), (4, 1), (8, 1))

        for (a, b) in ratios:
            ratio = a / b
            percent = ratio * 100.0

            layout.operator(
                "sequencer.view_zoom_ratio",
                text="{:g}% ({:d}:{:d})".format(percent, a, b),
                translate=False,
                icon='LAYER_ACTIVE' if isclose(percent, current_zoom, abs_tol=0.5) else 'NONE',
            ).ratio = ratio

        layout.separator()
        layout.operator("view2d.zoom_in")
        layout.operator("view2d.zoom_out")
        layout.operator("view2d.zoom_border", text="Zoom Region...")


class SEQUENCER_MT_proxy(Menu):
    bl_label = "Proxy"

    def draw(self, context):
        layout = self.layout
        st = context.space_data
        _, nonsound = selected_strips_count(context)

        col = layout.column()
        col.operator("sequencer.enable_proxies", text="Setup")
        col.operator("sequencer.rebuild_proxy", text="Rebuild")
        col.enabled = nonsound >= 1
        layout.prop(st, "proxy_render_size", text="")


class SEQUENCER_MT_view_render(Menu):
    bl_label = "Render Preview"

    def draw(self, _context):
        layout = self.layout
        layout.operator("render.opengl", text="Render Sequencer Image", icon='RENDER_STILL').sequencer = True
        props = layout.operator("render.opengl", text="Render Sequencer Animation", icon='RENDER_ANIMATION')
        props.animation = True
        props.sequencer = True


class SEQUENCER_MT_view(Menu):
    bl_label = "View"

    def draw(self, context):
        layout = self.layout

        st = context.space_data
        is_preview = st.view_type in {'PREVIEW', 'SEQUENCER_PREVIEW'}
        is_sequencer_view = st.view_type in {'SEQUENCER', 'SEQUENCER_PREVIEW'}
        is_sequencer_only = st.view_type == 'SEQUENCER'

        if st.view_type == 'PREVIEW':
            # Specifying the REGION_PREVIEW context is needed in preview-only
            # mode, else the lookup for the shortcut will fail in
            # wm_keymap_item_find_props() (see #32595).
            layout.operator_context = 'INVOKE_REGION_PREVIEW'
        layout.prop(st, "show_region_toolbar")
        layout.prop(st, "show_region_ui")
        layout.prop(st, "show_region_tool_header")
        layout.operator_context = 'INVOKE_DEFAULT'
        if is_sequencer_view:
            layout.prop(st, "show_region_hud")
        if is_sequencer_only:
            layout.prop(st, "show_region_channels")
        layout.prop(st, "show_region_footer", text="Playback Controls")
        layout.separator()

        if is_preview:
            layout.prop(st, "show_transform_preview", text="Preview During Transform")
        layout.separator()

        layout.operator_context = 'INVOKE_REGION_WIN'
        layout.operator("sequencer.refresh_all", icon='FILE_REFRESH', text="Refresh All")
        layout.operator_context = 'INVOKE_DEFAULT'
        layout.separator()

        layout.operator_context = 'INVOKE_REGION_WIN'
        if st.view_type == 'PREVIEW':
            # See above (#32595)
            layout.operator_context = 'INVOKE_REGION_PREVIEW'
        layout.operator("sequencer.view_selected", text="Frame Selected")
        if is_sequencer_view and context.sequencer_scene:
            layout.operator_context = 'INVOKE_REGION_WIN'
            layout.operator("sequencer.view_all")
            layout.operator(
                "anim.scene_range_frame",
                text="Frame Preview Range" if context.sequencer_scene.use_preview_range else "Frame Scene Range",
            )
            layout.operator("sequencer.view_frame")
            layout.prop(st, "use_clamp_view")

        if is_preview:
            if is_sequencer_view:
                layout.separator()
            layout.operator_context = 'INVOKE_REGION_PREVIEW'
            layout.operator("sequencer.view_all_preview", text="Fit Preview in Window")
            if is_sequencer_view:
                layout.menu("SEQUENCER_MT_preview_zoom", text="Preview Zoom")
            else:
                layout.menu("SEQUENCER_MT_preview_zoom")
            layout.prop(st, "use_zoom_to_fit", text="Auto Zoom")
            layout.separator()
            layout.menu("SEQUENCER_MT_proxy")
            layout.operator_context = 'INVOKE_DEFAULT'
            layout.separator()

        if is_sequencer_view:
            layout.separator()

            layout.prop(st, "show_markers")
            layout.prop(st, "show_seconds")
            layout.prop(st, "show_locked_time")
            layout.separator()

            layout.operator_context = 'INVOKE_DEFAULT'
            layout.menu("SEQUENCER_MT_navigation")
            layout.menu("SEQUENCER_MT_range")
            layout.separator()

        layout.operator("render.opengl", text="Render Still Preview", icon='RENDER_STILL').sequencer = True
        props = layout.operator("render.opengl", text="Render Sequence Preview", icon='RENDER_ANIMATION')
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


class SEQUENCER_MT_select(Menu):
    bl_label = "Select"

    def draw(self, context):
        layout = self.layout

        st = context.space_data
        has_sequencer, has_preview = _space_view_types(st)
        is_retiming = (
            context.sequencer_scene is not None and
            context.sequencer_scene.sequence_editor is not None and
            context.sequencer_scene.sequence_editor.selected_retiming_keys
        )
        if has_preview:
            layout.operator_context = 'INVOKE_REGION_PREVIEW'
        else:
            layout.operator_context = 'INVOKE_REGION_WIN'
        layout.operator("sequencer.select_all", text="All").action = 'SELECT'
        layout.operator("sequencer.select_all", text="None").action = 'DESELECT'
        layout.operator("sequencer.select_all", text="Invert").action = 'INVERT'

        layout.separator()

        col = layout.column()
        if has_sequencer:
            col.operator("sequencer.select_box", text="Box Select")
            props = col.operator("sequencer.select_box", text="Box Select (Include Handles)")
            props.include_handles = True
        elif has_preview:
            col.operator_context = 'INVOKE_REGION_PREVIEW'
            col.operator("sequencer.select_box", text="Box Select")

        col.separator()

        if has_sequencer:
            col.operator("sequencer.select_more", text="More")
            col.operator("sequencer.select_less", text="Less")
            col.separator()

        col.operator_menu_enum("sequencer.select_grouped", "type", text="Select Grouped")
        col.enabled = not is_retiming
        if has_sequencer:
            col.operator("sequencer.select_linked", text="Select Linked")
            col.separator()

        if has_sequencer:
            col.operator_menu_enum("sequencer.select_side_of_frame", "side", text="Side of Frame...")
            col.menu("SEQUENCER_MT_select_handle", text="Handle")
            col.menu("SEQUENCER_MT_select_channel", text="Channel")


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
        strip = context.active_strip

        layout.operator_context = 'INVOKE_REGION_WIN'
        if strip and strip.type == 'SCENE':
            bpy_data_scenes_len = len(bpy.data.scenes)
            if bpy_data_scenes_len > 10:
                layout.operator_context = 'INVOKE_DEFAULT'
                layout.operator("sequencer.change_scene", text="Change Scene...")
            elif bpy_data_scenes_len > 1:
                layout.operator_menu_enum("sequencer.change_scene", "scene", text="Change Scene")
            del bpy_data_scenes_len

        layout.operator_context = 'INVOKE_DEFAULT'
        if strip and strip.type in {
            'CROSS', 'ADD', 'SUBTRACT', 'ALPHA_OVER', 'ALPHA_UNDER',
            'GAMMA_CROSS', 'MULTIPLY', 'WIPE', 'GLOW',
            'SPEED', 'MULTICAM', 'ADJUSTMENT', 'GAUSSIAN_BLUR',
        }:
            layout.menu("SEQUENCER_MT_strip_effect_change")
            layout.operator("sequencer.swap_inputs")
        props = layout.operator("sequencer.change_path", text="Path/Files")

        if strip:
            strip_type = strip.type

            if strip_type == 'IMAGE':
                props.filter_image = True
            elif strip_type == 'MOVIE':
                props.filter_movie = True
            elif strip_type == 'SOUND':
                props.filter_sound = True


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
    bl_options = {'SEARCH_ON_KEY_PRESS'}

    def draw(self, context):

        layout = self.layout
        layout.operator_context = 'INVOKE_REGION_WIN'

        layout.menu("SEQUENCER_MT_add_scene", text="Scene", icon='SCENE_DATA')

        bpy_data_movieclips_len = len(bpy.data.movieclips)
        if bpy_data_movieclips_len > 10:
            layout.operator_context = 'INVOKE_DEFAULT'
            layout.operator("sequencer.movieclip_strip_add", text="Clip...", icon='TRACKER')
        elif bpy_data_movieclips_len > 0:
            layout.operator_menu_enum("sequencer.movieclip_strip_add", "clip", text="Clip", icon='TRACKER')
        else:
            layout.menu("SEQUENCER_MT_add_empty", text="Clip", text_ctxt=i18n_contexts.id_movieclip, icon='TRACKER')
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

        total, nonsound = selected_strips_count(context)

        col = layout.column()
        col.menu("SEQUENCER_MT_add_transitions", icon='ARROW_LEFTRIGHT')
        # Enable for video transitions or sound cross-fade.
        col.enabled = nonsound == 2 or (nonsound == 0 and total == 2)

        col = layout.column()
        col.operator_menu_enum("sequencer.fades_add", "type", text="Fade", icon='IPO_EASE_IN_OUT')
        col.enabled = total >= 1


class SEQUENCER_MT_add_empty(Menu):
    bl_label = "Empty"

    def draw(self, _context):
        layout = self.layout

        layout.label(text="No Items Available")


class SEQUENCER_MT_add_transitions(Menu):
    bl_label = "Transition"

    def draw(self, context):
        total, nonsound = selected_strips_count(context)

        layout = self.layout

        col = layout.column()
        col.operator("sequencer.crossfade_sounds", text="Sound Crossfade")
        col.enabled = (nonsound == 0 and total == 2)

        layout.separator()

        col = layout.column()
        col.operator("sequencer.effect_strip_add", text="Crossfade").type = 'CROSS'
        col.operator("sequencer.effect_strip_add", text="Gamma Crossfade").type = 'GAMMA_CROSS'

        col.separator()

        col.operator("sequencer.effect_strip_add", text="Wipe").type = 'WIPE'
        col.enabled = nonsound == 2


class SEQUENCER_MT_add_effect(Menu):
    bl_label = "Effect Strip"

    def draw(self, context):

        layout = self.layout
        layout.operator_context = 'INVOKE_REGION_WIN'
        _, nonsound = selected_strips_count(context)

        layout.operator("sequencer.effect_strip_add", text="Multicam Selector").type = 'MULTICAM'

        layout.separator()

        col = layout.column()
        col.operator("sequencer.effect_strip_add", text="Speed Control").type = 'SPEED'
        col.operator("sequencer.effect_strip_add", text="Glow").type = 'GLOW'
        col.operator("sequencer.effect_strip_add", text="Gaussian Blur").type = 'GAUSSIAN_BLUR'
        col.enabled = nonsound == 1

        layout.separator()

        col = layout.column()
        col.operator(
            "sequencer.effect_strip_add",
            text="Add",
            text_ctxt=i18n_contexts.id_sequence,
        ).type = 'ADD'
        col.operator(
            "sequencer.effect_strip_add",
            text="Subtract",
            text_ctxt=i18n_contexts.id_sequence,
        ).type = 'SUBTRACT'
        col.operator(
            "sequencer.effect_strip_add",
            text="Multiply",
            text_ctxt=i18n_contexts.id_sequence,
        ).type = 'MULTIPLY'
        col.operator(
            "sequencer.effect_strip_add",
            text="Alpha Over",
            text_ctxt=i18n_contexts.id_sequence,
        ).type = 'ALPHA_OVER'
        col.operator(
            "sequencer.effect_strip_add",
            text="Alpha Under",
            text_ctxt=i18n_contexts.id_sequence,
        ).type = 'ALPHA_UNDER'
        col.operator(
            "sequencer.effect_strip_add",
            text="Color Mix",
            text_ctxt=i18n_contexts.id_sequence,
        ).type = 'COLORMIX'
        col.enabled = nonsound == 2


class SEQUENCER_MT_strip_transform(Menu):
    bl_label = "Transform"

    def draw(self, context):
        layout = self.layout
        st = context.space_data
        has_sequencer, has_preview = _space_view_types(st)

        if has_preview:
            layout.operator_context = 'INVOKE_REGION_PREVIEW'
        else:
            layout.operator_context = 'INVOKE_REGION_WIN'

        col = layout.column()
        if has_preview:
            col.operator("transform.translate", text="Move")
            col.operator("transform.rotate", text="Rotate")
            col.operator("transform.resize", text="Scale")
        else:
            col.operator("transform.seq_slide", text="Move").view2d_edge_pan = True
            col.operator("transform.transform", text="Move/Extend from Current Frame").mode = 'TIME_EXTEND'
            col.operator("sequencer.slip", text="Slip Strip Contents")

        # TODO (for preview)
        if has_sequencer:
            col.separator()
            col.operator("sequencer.snap")
            col.operator("sequencer.offset_clear")

            col.separator()

        if has_sequencer:
            col.operator_menu_enum("sequencer.swap", "side")

            col.separator()
            col.operator("sequencer.gap_remove").all = False
            col.operator("sequencer.gap_remove", text="Remove Gaps (All)").all = True
            col.operator("sequencer.gap_insert")

        col.enabled = bool(context.sequencer_scene)


class SEQUENCER_MT_strip_text(Menu):
    bl_label = "Text"

    def draw(self, context):
        layout = self.layout
        layout.operator_context = 'INVOKE_REGION_PREVIEW'
        layout.operator("sequencer.text_edit_mode_toggle")
        layout.separator()
        layout.operator("sequencer.text_edit_copy", icon='COPYDOWN')
        layout.operator("sequencer.text_edit_paste", icon='PASTEDOWN')
        layout.operator("sequencer.text_edit_cut")
        layout.separator()
        props = layout.operator("sequencer.text_delete")
        props.type = 'PREVIOUS_OR_SELECTION'
        layout.operator("sequencer.text_line_break")
        layout.separator()
        layout.operator("sequencer.text_select_all")
        layout.operator("sequencer.text_deselect_all")


class SEQUENCER_MT_strip_show_hide(Menu):
    bl_label = "Show/Hide"

    def draw(self, _context):
        layout = self.layout
        layout.operator_context = 'INVOKE_REGION_PREVIEW'
        layout.operator("sequencer.unmute", text="Show Hidden Strips").unselected = False
        layout.separator()
        layout.operator("sequencer.mute", text="Hide Selected").unselected = False
        layout.operator("sequencer.mute", text="Hide Unselected").unselected = True


class SEQUENCER_MT_strip_animation(Menu):
    bl_label = "Animation"

    def draw(self, context):
        layout = self.layout
        layout.operator_context = 'INVOKE_REGION_PREVIEW'

        col = layout.column()
        col.operator("anim.keyframe_insert", text="Insert Keyframe")
        col.operator("anim.keyframe_insert_menu", text="Insert Keyframe with Keying Set").always_prompt = True
        col.operator("anim.keying_set_active_set", text="Change Keying Set...")
        col.operator("anim.keyframe_delete_vse", text="Delete Keyframes...")
        col.operator("anim.keyframe_clear_vse", text="Clear Keyframes...")
        col.enabled = bool(context.sequencer_scene)


class SEQUENCER_MT_strip_mirror(Menu):
    bl_label = "Mirror"

    def draw(self, context):
        layout = self.layout

        col = layout.column()
        col.operator_context = 'INVOKE_REGION_PREVIEW'
        col.operator("transform.mirror", text="Interactive Mirror")

        col.separator()

        # Only interactive mirror should invoke the modal, all others should immediately run.
        col.operator_context = 'EXEC_REGION_PREVIEW'

        for (space_name, space_id) in (("Global", 'GLOBAL'), ("Local", 'LOCAL')):
            for axis_index, axis_name in enumerate("XY"):
                props = col.operator(
                    "transform.mirror",
                    text="{:s} {:s}".format(axis_name, iface_(space_name)),
                    translate=False,
                )
                props.constraint_axis[axis_index] = True
                props.orient_type = space_id

            if space_id == 'GLOBAL':
                col.separator()
        col.enabled = bool(context.sequencer_scene)


class SEQUENCER_MT_strip_input(Menu):
    bl_label = "Inputs"

    def draw(self, context):
        layout = self.layout
        strip = context.active_strip

        layout.operator("sequencer.reload", text="Reload Strips")
        layout.operator("sequencer.reload", text="Reload Strips and Adjust Length").adjust_length = True
        props = layout.operator("sequencer.change_path", text="Change Path/Files")
        layout.operator("sequencer.swap_data", text="Swap Data")

        if strip:
            strip_type = strip.type

            if strip_type == 'IMAGE':
                props.filter_image = True
            elif strip_type == 'MOVIE':
                props.filter_movie = True
            elif strip_type == 'SOUND':
                props.filter_sound = True


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


class SEQUENCER_MT_strip_modifiers(Menu):
    bl_label = "Modifiers"

    def draw(self, _context):
        layout = self.layout

        layout.menu("SEQUENCER_MT_modifier_add", text="Add Modifier")

        layout.operator("sequencer.strip_modifier_copy", text="Copy to Selected Strips...")


class SEQUENCER_MT_strip_effect(Menu):
    bl_label = "Effect Strip"

    def draw(self, _context):
        layout = self.layout

        layout.menu("SEQUENCER_MT_strip_effect_change")
        layout.operator("sequencer.reassign_inputs")
        layout.operator("sequencer.swap_inputs")


class SEQUENCER_MT_strip_effect_change(Menu):
    bl_label = "Change Effect Type"

    def draw(self, context):
        layout = self.layout

        strip = context.active_strip

        col = layout.column()
        col.operator("sequencer.change_effect_type", text="Adjustment Layer").type = 'ADJUSTMENT'
        col.operator("sequencer.change_effect_type", text="Multicam Selector").type = 'MULTICAM'
        col.enabled = strip.input_count == 0

        layout.separator()

        col = layout.column()
        col.operator("sequencer.change_effect_type", text="Speed Control").type = 'SPEED'
        col.operator("sequencer.change_effect_type", text="Glow").type = 'GLOW'
        col.operator("sequencer.change_effect_type", text="Gaussian Blur").type = 'GAUSSIAN_BLUR'
        col.enabled = strip.input_count == 1

        layout.separator()

        col = layout.column()
        col.operator("sequencer.change_effect_type", text="Add").type = 'ADD'
        col.operator("sequencer.change_effect_type", text="Subtract").type = 'SUBTRACT'
        col.operator("sequencer.change_effect_type", text="Multiply").type = 'MULTIPLY'
        col.operator("sequencer.change_effect_type", text="Alpha Over").type = 'ALPHA_OVER'
        col.operator("sequencer.change_effect_type", text="Alpha Under").type = 'ALPHA_UNDER'
        col.operator("sequencer.change_effect_type", text="Color Mix").type = 'COLORMIX'
        col.operator("sequencer.change_effect_type", text="Crossfade").type = 'CROSS'
        col.operator("sequencer.change_effect_type", text="Gamma Crossfade").type = 'GAMMA_CROSS'
        col.operator("sequencer.change_effect_type", text="Wipe").type = 'WIPE'
        col.enabled = strip.input_count == 2


class SEQUENCER_MT_strip_movie(Menu):
    bl_label = "Movie Strip"

    def draw(self, _context):
        layout = self.layout

        layout.operator("sequencer.rendersize")
        layout.operator("sequencer.deinterlace_selected_movies")


class SEQUENCER_MT_strip_retiming(Menu):
    bl_label = "Retiming"

    def draw(self, context):
        layout = self.layout

        is_retiming = (
            context.sequencer_scene is not None and
            context.sequencer_scene.sequence_editor is not None and
            context.sequencer_scene.sequence_editor.selected_retiming_keys
        )
        strip = context.active_strip

        layout.operator("sequencer.retiming_key_add")
        layout.operator("sequencer.retiming_add_freeze_frame_slide")
        col = layout.column()
        col.operator("sequencer.retiming_add_transition_slide")
        col.enabled = is_retiming

        layout.separator()

        layout.operator("sequencer.retiming_key_delete")
        col = layout.column()
        col.operator("sequencer.retiming_reset")
        col.enabled = not is_retiming

        layout.separator()

        layout.operator("sequencer.retiming_segment_speed_set")
        layout.operator(
            "sequencer.retiming_show",
            icon='CHECKBOX_HLT' if (strip and strip.show_retiming_keys) else 'CHECKBOX_DEHLT',
            text="Toggle Retiming Keys",
        )


class SEQUENCER_MT_strip(Menu):
    bl_label = "Strip"

    def draw(self, context):
        from _bl_ui_utils.layout import operator_context

        layout = self.layout
        st = context.space_data
        has_sequencer, has_preview = _space_view_types(st)

        layout.menu("SEQUENCER_MT_strip_transform")

        if has_preview:
            layout.operator_context = 'INVOKE_REGION_PREVIEW'
        else:
            layout.operator_context = 'INVOKE_REGION_WIN'

        strip = context.active_strip

        if has_preview:
            layout.menu("SEQUENCER_MT_strip_mirror")
            layout.separator()
            layout.operator("sequencer.preview_duplicate_move", text="Duplicate")
            layout.operator("sequencer.copy", text="Copy")
            layout.operator("sequencer.paste", text="Paste")
            layout.separator()
            layout.menu("SEQUENCER_MT_strip_animation")
            layout.separator()
            layout.menu("SEQUENCER_MT_strip_show_hide")
            layout.separator()
            if strip and strip.type == 'TEXT':
                layout.menu("SEQUENCER_MT_strip_text")

        if has_sequencer:
            layout.menu("SEQUENCER_MT_strip_retiming")
            layout.separator()

            with operator_context(layout, 'EXEC_REGION_WIN'):
                props = layout.operator("sequencer.split", text="Split", text_ctxt=i18n_contexts.id_sequence)
                props.type = 'SOFT'

                props = layout.operator("sequencer.split", text="Hold Split", text_ctxt=i18n_contexts.id_sequence)
                props.type = 'HARD'

            layout.separator()

            layout.operator("sequencer.copy", text="Copy")
            layout.operator("sequencer.paste", text="Paste")
            layout.operator("sequencer.duplicate_move", text="Duplicate")
            layout.operator("sequencer.duplicate_move_linked", text="Duplicate Linked")

        layout.separator()
        layout.operator("sequencer.delete", text="Delete")

        if strip and strip.type == 'SCENE':
            layout.operator("sequencer.delete", text="Delete Strip & Data").delete_data = True
            layout.operator("sequencer.scene_frame_range_update")

        if has_sequencer:
            if strip:
                strip_type = strip.type
                layout.separator()
                layout.menu("SEQUENCER_MT_strip_modifiers", icon='MODIFIER')

                if strip_type in {
                        'CROSS', 'ADD', 'SUBTRACT', 'ALPHA_OVER', 'ALPHA_UNDER',
                        'GAMMA_CROSS', 'MULTIPLY', 'WIPE', 'GLOW',
                        'SPEED', 'MULTICAM', 'ADJUSTMENT', 'GAUSSIAN_BLUR',
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
                elif strip_type == 'META':
                    layout.separator()
                    layout.operator("sequencer.meta_make")
                    layout.operator("sequencer.meta_separate")
                    layout.operator("sequencer.meta_toggle", text="Toggle Meta")
                if strip_type != 'META':
                    layout.separator()
                    layout.operator("sequencer.meta_make")
                    layout.operator("sequencer.meta_toggle", text="Toggle Meta")

        if has_sequencer:
            layout.separator()
            layout.menu("SEQUENCER_MT_color_tag_picker")

            layout.separator()
            layout.menu("SEQUENCER_MT_strip_lock_mute")

            layout.separator()
            layout.operator("sequencer.connect", icon='LINKED').toggle = True
            layout.operator("sequencer.disconnect")

            layout.separator()
            layout.menu("SEQUENCER_MT_strip_input")


class SEQUENCER_MT_image(Menu):
    bl_label = "Image"

    def draw(self, context):
        layout = self.layout
        st = context.space_data

        if st.view_type in {'PREVIEW', 'SEQUENCER_PREVIEW'}:
            layout.menu("SEQUENCER_MT_image_transform")

        layout.menu("SEQUENCER_MT_image_clear")
        layout.menu("SEQUENCER_MT_image_apply")


class SEQUENCER_MT_image_transform(Menu):
    bl_label = "Transform"

    def draw(self, context):
        layout = self.layout

        layout.operator_context = 'INVOKE_REGION_PREVIEW'

        col = layout.column()
        col.operator("transform.translate")
        col.operator("transform.rotate")
        col.operator("transform.resize", text="Scale")
        col.separator()
        col.operator("transform.translate", text="Move Origin").translate_origin = True
        col.enabled = bool(context.sequencer_scene)


class SEQUENCER_MT_image_clear(Menu):
    bl_label = "Clear"

    def draw(self, _context):
        layout = self.layout

        layout.operator(
            "sequencer.strip_transform_clear",
            text="Position",
            text_ctxt=i18n_contexts.default,
        ).property = 'POSITION'
        layout.operator(
            "sequencer.strip_transform_clear",
            text="Scale",
            text_ctxt=i18n_contexts.default,
        ).property = 'SCALE'
        layout.operator(
            "sequencer.strip_transform_clear",
            text="Rotation",
            text_ctxt=i18n_contexts.default,
        ).property = 'ROTATION'
        layout.operator(
            "sequencer.strip_transform_clear",
            text="All Transforms",
        ).property = 'ALL'


class SEQUENCER_MT_image_apply(Menu):
    bl_label = "Apply"

    def draw(self, _context):
        layout = self.layout

        layout.operator("sequencer.strip_transform_fit", text="Scale To Fit").fit_method = 'FIT'
        layout.operator("sequencer.strip_transform_fit", text="Scale to Fill").fit_method = 'FILL'
        layout.operator("sequencer.strip_transform_fit", text="Stretch To Fill").fit_method = 'STRETCH'


class SEQUENCER_MT_retiming(Menu):
    bl_label = "Retiming"
    bl_translation_context = i18n_contexts.operator_default

    def draw(self, context):

        layout = self.layout
        layout.operator_context = 'INVOKE_REGION_WIN'

        layout.operator("sequencer.retiming_key_add")
        layout.operator("sequencer.retiming_add_freeze_frame_slide")


class SEQUENCER_MT_context_menu(Menu):
    bl_label = "Sequencer"

    def draw_generic(self, context):
        layout = self.layout

        layout.operator_context = 'INVOKE_REGION_WIN'

        layout.operator("sequencer.split", text="Split", text_ctxt=i18n_contexts.id_sequence).type = 'SOFT'

        layout.separator()

        layout.operator("sequencer.copy", text="Copy", icon='COPYDOWN')
        layout.operator("sequencer.paste", text="Paste", icon='PASTEDOWN')
        layout.operator("sequencer.duplicate_move")
        props = layout.operator("wm.call_panel", text="Rename...")
        props.name = "TOPBAR_PT_name"
        props.keep_open = False
        layout.operator("sequencer.delete", text="Delete")

        strip = context.active_strip
        if strip and strip.type == 'SCENE':
            layout.operator("sequencer.delete", text="Delete Strip & Data").delete_data = True
            layout.operator("sequencer.scene_frame_range_update")

        layout.separator()

        layout.operator("sequencer.slip", text="Slip Strip Contents")
        layout.operator("sequencer.snap")

        layout.separator()

        layout.operator("sequencer.set_range_to_strips", text="Set Preview Range to Strips").preview = True

        layout.separator()

        layout.operator("sequencer.gap_remove").all = False
        layout.operator("sequencer.gap_insert")

        layout.separator()

        if strip:
            strip_type = strip.type
            total, nonsound = selected_strips_count(context)

            layout.separator()
            layout.menu("SEQUENCER_MT_strip_modifiers", icon='MODIFIER')

            if total == 2:
                if nonsound == 2:
                    layout.separator()
                    col = layout.column()
                    col.menu("SEQUENCER_MT_add_transitions", text="Add Transition")
                elif nonsound == 0:
                    layout.separator()
                    layout.operator("sequencer.crossfade_sounds", text="Crossfade Sounds")

            if total >= 1:
                col = layout.column()
                col.operator_menu_enum("sequencer.fades_add", "type", text="Fade")
                layout.operator("sequencer.fades_clear", text="Clear Fade")

            if strip_type in {
                    'CROSS', 'ADD', 'SUBTRACT', 'ALPHA_OVER', 'ALPHA_UNDER',
                    'GAMMA_CROSS', 'MULTIPLY', 'WIPE', 'GLOW',
                    'SPEED', 'MULTICAM', 'ADJUSTMENT', 'GAUSSIAN_BLUR',
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
        layout.operator("sequencer.connect", icon='LINKED').toggle = True
        layout.operator("sequencer.disconnect")

    def draw_retime(self, context):
        layout = self.layout
        layout.operator_context = 'INVOKE_REGION_WIN'

        if context.sequencer_scene.sequence_editor.selected_retiming_keys:
            layout.operator("sequencer.retiming_add_freeze_frame_slide")
            layout.operator("sequencer.retiming_add_transition_slide")
            layout.separator()

            layout.operator("sequencer.retiming_segment_speed_set")
            layout.separator()

            layout.operator("sequencer.retiming_key_delete", text="Delete Retiming Keys")

    def draw(self, context):
        ed = context.sequencer_scene.sequence_editor
        if ed.selected_retiming_keys:

            self.draw_retime(context)
        else:
            self.draw_generic(context)


class SEQUENCER_MT_preview_context_menu(Menu):
    bl_label = "Sequencer Preview"

    def draw(self, context):
        layout = self.layout

        layout.operator_context = 'INVOKE_REGION_WIN'

        props = layout.operator("wm.call_panel", text="Rename...")
        props.name = "TOPBAR_PT_name"
        props.keep_open = False

        # TODO: support in preview.
        # layout.operator("sequencer.delete", text="Delete")


class SEQUENCER_MT_pivot_pie(Menu):
    bl_label = "Pivot Point"

    def draw(self, context):
        layout = self.layout
        pie = layout.menu_pie()

        if context.sequencer_scene:
            sequencer_tool_settings = context.sequencer_scene.tool_settings.sequencer_tool_settings

            pie.prop_enum(sequencer_tool_settings, "pivot_point", value='CENTER')
            pie.prop_enum(sequencer_tool_settings, "pivot_point", value='CURSOR')
            pie.prop_enum(sequencer_tool_settings, "pivot_point", value='INDIVIDUAL_ORIGINS')
            pie.prop_enum(sequencer_tool_settings, "pivot_point", value='MEDIAN')


class SEQUENCER_MT_view_pie(Menu):
    bl_label = "View"

    def draw(self, context):
        layout = self.layout

        pie = layout.menu_pie()
        pie.operator("sequencer.view_all")
        pie.operator("sequencer.view_selected", text="Frame Selected", icon='ZOOM_SELECTED')
        pie.separator()
        if context.sequencer_scene.use_preview_range:
            pie.operator("anim.scene_range_frame", text="Frame Preview Range")
        else:
            pie.operator("anim.scene_range_frame", text="Frame Scene Range")


class SEQUENCER_MT_preview_view_pie(Menu):
    bl_label = "View"

    def draw(self, _context):
        layout = self.layout

        pie = layout.menu_pie()
        pie.operator_context = 'INVOKE_REGION_PREVIEW'
        pie.operator("sequencer.view_all_preview")
        pie.operator("sequencer.view_selected", text="Frame Selected", icon='ZOOM_SELECTED')
        pie.separator()
        pie.operator("sequencer.view_zoom_ratio", text="Zoom 1:1").ratio = 1


class SEQUENCER_MT_modifier_add(Menu):
    bl_label = "Add Modifier"
    bl_options = {'SEARCH_ON_KEY_PRESS'}

    MODIFIER_TYPES_TO_ICONS = {
        enum_it.identifier: enum_it.icon
        for enum_it in bpy.types.StripModifier.bl_rna.properties["type"].enum_items_static
    }
    MODIFIER_TYPES_TO_LABELS = {
        enum_it.identifier: enum_it.name
        for enum_it in bpy.types.StripModifier.bl_rna.properties["type"].enum_items_static
    }
    MODIFIER_TYPES_I18N_CONTEXT = bpy.types.StripModifier.bl_rna.properties["type"].translation_context

    @classmethod
    def operator_modifier_add(cls, layout, mod_type):
        layout.operator(
            "sequencer.strip_modifier_add",
            text=cls.MODIFIER_TYPES_TO_LABELS[mod_type],
            # Although these are operators, the label actually comes from an (enum) property,
            # so the property's translation context must be used here.
            text_ctxt=cls.MODIFIER_TYPES_I18N_CONTEXT,
            icon=cls.MODIFIER_TYPES_TO_ICONS[mod_type],
        ).type = mod_type

    def draw(self, context):
        layout = self.layout
        strip = context.active_strip
        if not strip:
            return

        if layout.operator_context == 'EXEC_REGION_WIN':
            layout.operator_context = 'INVOKE_REGION_WIN'
            layout.operator(
                "WM_OT_search_single_menu",
                text="Search...",
                icon='VIEWZOOM',
            ).menu_idname = "SEQUENCER_MT_modifier_add"
            layout.separator()

        layout.operator_context = 'INVOKE_REGION_WIN'

        if strip.type == 'SOUND':
            self.operator_modifier_add(layout, 'SOUND_EQUALIZER')
        else:
            self.operator_modifier_add(layout, 'BRIGHT_CONTRAST')
            self.operator_modifier_add(layout, 'COLOR_BALANCE')
            self.operator_modifier_add(layout, 'COMPOSITOR')
            self.operator_modifier_add(layout, 'CURVES')
            self.operator_modifier_add(layout, 'HUE_CORRECT')
            self.operator_modifier_add(layout, 'MASK')
            self.operator_modifier_add(layout, 'TONEMAP')
            self.operator_modifier_add(layout, 'WHITE_BALANCE')


class SequencerButtonsPanel:
    bl_space_type = 'SEQUENCE_EDITOR'
    bl_region_type = 'UI'

    @staticmethod
    def has_sequencer(context):
        return (context.space_data.view_type in {'SEQUENCER', 'SEQUENCER_PREVIEW'})

    @classmethod
    def poll(cls, context):
        return cls.has_sequencer(context) and (context.active_strip is not None)


class SequencerButtonsPanel_Output:
    bl_space_type = 'SEQUENCE_EDITOR'
    bl_region_type = 'UI'

    @staticmethod
    def has_preview(context):
        st = context.space_data
        return (st.view_type in {'PREVIEW', 'SEQUENCER_PREVIEW'})

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
        return cls.has_sequencer(context) and context.active_strip is not None


class SEQUENCER_PT_color_tag_picker(SequencerColorTagPicker, Panel):
    bl_label = "Color Tag"
    bl_options = {'HIDE_HEADER', 'INSTANCED'}

    def draw(self, _context):
        layout = self.layout

        row = layout.row(align=True)
        row.operator("sequencer.strip_color_tag_set", icon='X').color = 'NONE'
        for i in range(1, 10):
            icon = 'STRIP_COLOR_{:02d}'.format(i)
            row.operator("sequencer.strip_color_tag_set", icon=icon).color = 'COLOR_{:02d}'.format(i)


class SEQUENCER_MT_color_tag_picker(SequencerColorTagPicker, Menu):
    bl_label = "Set Color Tag"

    def draw(self, _context):
        layout = self.layout

        row = layout.row(align=True)
        row.operator_enum("sequencer.strip_color_tag_set", "color", icon_only=True)


class SEQUENCER_PT_cache_settings(SequencerButtonsPanel, Panel):
    bl_label = "Cache Settings"
    bl_category = "Cache"

    @classmethod
    def poll(cls, context):
        return cls.has_sequencer(context) and context.sequencer_scene and context.sequencer_scene.sequence_editor

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        ed = context.sequencer_scene.sequence_editor

        col = layout.column()
        if ed:
            col.prop(ed, "use_prefetch")

        col = layout.column(heading="Cache", align=True)

        col.prop(ed, "use_cache_raw", text="Raw")
        col.prop(ed, "use_cache_final", text="Final")


class SEQUENCER_PT_cache_view_settings(SequencerButtonsPanel, Panel):
    bl_label = "Display"
    bl_category = "Cache"
    bl_parent_id = "SEQUENCER_PT_cache_settings"

    @classmethod
    def poll(cls, context):
        return cls.has_sequencer(context) and context.sequencer_scene and context.sequencer_scene.sequence_editor

    def draw_header(self, context):
        cache_settings = context.space_data.cache_overlay

        self.layout.prop(cache_settings, "show_cache", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        cache_settings = context.space_data.cache_overlay
        ed = context.sequencer_scene.sequence_editor
        layout.active = cache_settings.show_cache

        col = layout.column(heading="Cache", align=True)

        show_developer_ui = context.preferences.view.show_developer_ui
        if show_developer_ui:
            col.prop(cache_settings, "show_cache_raw", text="Raw")
        col.prop(cache_settings, "show_cache_final_out", text="Final")

        show_cache_size = show_developer_ui and (ed.use_cache_raw or ed.use_cache_final)
        if show_cache_size:
            cache_raw_size = ed.cache_raw_size
            cache_final_size = ed.cache_final_size

            col = layout.box()
            col = col.column(align=True)

            split = col.split(factor=0.4, align=True)
            split.alignment = 'RIGHT'
            split.label(text="Current Cache Size")
            split.alignment = 'LEFT'
            split.label(text=iface_("{:d} MB").format(cache_raw_size + cache_final_size), translate=False)

            split = col.split(factor=0.4, align=True)
            split.alignment = 'RIGHT'
            split.label(text="Raw")
            split.alignment = 'LEFT'
            split.label(text=iface_("{:d} MB").format(cache_raw_size), translate=False)

            split = col.split(factor=0.4, align=True)
            split.alignment = 'RIGHT'
            split.label(text="Final")
            split.alignment = 'LEFT'
            split.label(text=iface_("{:d} MB").format(cache_final_size), translate=False)


class SEQUENCER_PT_proxy_settings(SequencerButtonsPanel, Panel):
    bl_label = "Proxy Settings"
    bl_category = "Proxy"

    @classmethod
    def poll(cls, context):
        return cls.has_sequencer(context) and context.sequencer_scene and context.sequencer_scene.sequence_editor

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        ed = context.sequencer_scene.sequence_editor
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
        if not cls.has_sequencer(context) or not context.sequencer_scene or not context.sequencer_scene.sequence_editor:
            return False

        strip = context.active_strip
        if not strip:
            return False

        return strip.type in {'MOVIE', 'IMAGE'}

    def draw_header(self, context):
        strip = context.active_strip

        self.layout.prop(strip, "use_proxy", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        ed = context.sequencer_scene.sequence_editor

        strip = context.active_strip

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


class SEQUENCER_PT_preview(SequencerButtonsPanel_Output, Panel):
    bl_label = "Scene Strip Display"
    bl_space_type = 'SEQUENCE_EDITOR'
    bl_region_type = 'UI'
    bl_options = {'DEFAULT_CLOSED'}
    bl_category = "View"

    @classmethod
    def poll(cls, context):
        return SequencerButtonsPanel_Output.poll(context) and context.sequencer_scene

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        render = context.sequencer_scene.render

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
        if st.proxy_render_size in {'NONE', 'SCENE'}:
            col.enabled = False
        col.prop(st, "use_proxies")

        col = layout.column()
        col.prop(st, "display_channel", text="Channel")

        if st.display_mode == 'IMAGE':
            col.prop(st, "show_overexposed")

        if ed:
            col.prop(ed, "show_missing_media")


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
        if not context.sequencer_scene or not context.sequencer_scene.sequence_editor:
            return False
        return SequencerButtonsPanel_Output.poll(context)

    def draw_header(self, context):
        scene = context.sequencer_scene
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
        scene = context.sequencer_scene
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
        return is_preview and (st.display_mode == 'IMAGE') and context.sequencer_scene

    def draw_header(self, context):
        overlay_settings = context.space_data.preview_overlay
        self.layout.prop(overlay_settings, "show_safe_areas", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        overlay_settings = context.space_data.preview_overlay
        safe_data = context.sequencer_scene.safe_areas

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

    @classmethod
    def poll(cls, context):
        return SequencerButtonsPanel_Output.poll(context) and context.sequencer_scene

    def draw_header(self, context):
        layout = self.layout
        overlay_settings = context.space_data.preview_overlay
        layout.active = overlay_settings.show_safe_areas
        layout.prop(overlay_settings, "show_safe_center", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        safe_data = context.sequencer_scene.safe_areas
        overlay_settings = context.space_data.preview_overlay

        layout.active = overlay_settings.show_safe_areas and overlay_settings.show_safe_center

        col = layout.column()
        col.prop(safe_data, "title_center", slider=True)
        col.prop(safe_data, "action_center", slider=True)


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
    bl_parent_id = "SEQUENCER_PT_annotation"
    bl_options = {'DEFAULT_CLOSED'}

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
    COMPAT_ENGINES = {
        'BLENDER_RENDER',
        'BLENDER_WORKBENCH',
    }
    _context_path = "active_strip"
    _property_type = (bpy.types.Strip,)
    bl_category = "Strip"


class SEQUENCER_PT_snapping(Panel):
    bl_space_type = 'SEQUENCE_EDITOR'
    bl_region_type = 'HEADER'
    bl_label = ""
    bl_ui_units_x = 11

    def draw(self, _context):
        pass


class SEQUENCER_PT_preview_snapping(Panel):
    bl_space_type = 'SEQUENCE_EDITOR'
    bl_region_type = 'HEADER'
    bl_parent_id = "SEQUENCER_PT_snapping"
    bl_label = "Preview Snapping"

    @classmethod
    def poll(cls, context):
        st = context.space_data
        return st.view_type in {'PREVIEW', 'SEQUENCER_PREVIEW'} and context.sequencer_scene

    def draw(self, context):
        tool_settings = context.tool_settings
        sequencer_tool_settings = tool_settings.sequencer_tool_settings

        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        col = layout.column(heading="Snap to", align=True)
        col.prop(sequencer_tool_settings, "snap_to_borders")
        col.prop(sequencer_tool_settings, "snap_to_center")
        col.prop(sequencer_tool_settings, "snap_to_strips_preview")


class SEQUENCER_PT_sequencer_snapping(Panel):
    bl_space_type = 'SEQUENCE_EDITOR'
    bl_region_type = 'HEADER'
    bl_parent_id = "SEQUENCER_PT_snapping"
    bl_label = "Sequencer Snapping"

    @classmethod
    def poll(cls, context):
        st = context.space_data
        return st.view_type in {'SEQUENCER', 'SEQUENCER_PREVIEW'} and context.sequencer_scene

    def draw(self, context):
        tool_settings = context.tool_settings
        sequencer_tool_settings = tool_settings.sequencer_tool_settings

        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        col = layout.column(heading="Snap to", align=True)
        col.prop(sequencer_tool_settings, "snap_to_frame_range")
        col.prop(sequencer_tool_settings, "snap_to_current_frame")
        col.prop(sequencer_tool_settings, "snap_to_hold_offset")
        col.prop(sequencer_tool_settings, "snap_to_markers")
        col.prop(sequencer_tool_settings, "snap_to_retiming_keys")

        col = layout.column(heading="Ignore", align=True)
        col.prop(sequencer_tool_settings, "snap_ignore_muted", text="Muted Strips")
        col.prop(sequencer_tool_settings, "snap_ignore_sound", text="Sound Strips")


classes = (
    SEQUENCER_MT_change,
    SEQUENCER_HT_tool_header,
    SEQUENCER_HT_header,
    SEQUENCER_HT_playback_controls,
    SEQUENCER_MT_editor_menus,
    SEQUENCER_MT_range,
    SEQUENCER_MT_view,
    SEQUENCER_MT_preview_zoom,
    SEQUENCER_MT_proxy,
    SEQUENCER_MT_select_handle,
    SEQUENCER_MT_select_channel,
    SEQUENCER_MT_select,
    SEQUENCER_MT_marker,
    SEQUENCER_MT_navigation,
    SEQUENCER_MT_add,
    SEQUENCER_MT_add_effect,
    SEQUENCER_MT_add_transitions,
    SEQUENCER_MT_add_empty,
    SEQUENCER_MT_strip_effect,
    SEQUENCER_MT_strip_effect_change,
    SEQUENCER_MT_strip_movie,
    SEQUENCER_MT_strip,
    SEQUENCER_MT_strip_transform,
    SEQUENCER_MT_strip_retiming,
    SEQUENCER_MT_strip_text,
    SEQUENCER_MT_strip_show_hide,
    SEQUENCER_MT_strip_animation,
    SEQUENCER_MT_strip_mirror,
    SEQUENCER_MT_strip_input,
    SEQUENCER_MT_strip_lock_mute,
    SEQUENCER_MT_strip_modifiers,
    SEQUENCER_MT_image,
    SEQUENCER_MT_image_transform,
    SEQUENCER_MT_image_clear,
    SEQUENCER_MT_image_apply,
    SEQUENCER_MT_color_tag_picker,
    SEQUENCER_MT_context_menu,
    SEQUENCER_MT_preview_context_menu,
    SEQUENCER_MT_pivot_pie,
    SEQUENCER_MT_retiming,
    SEQUENCER_MT_view_pie,
    SEQUENCER_MT_preview_view_pie,
    SEQUENCER_MT_modifier_add,

    SEQUENCER_PT_active_tool,

    SEQUENCER_PT_gizmo_display,
    SEQUENCER_PT_overlay,
    SEQUENCER_PT_preview_overlay,
    SEQUENCER_PT_sequencer_overlay,
    SEQUENCER_PT_sequencer_overlay_strips,
    SEQUENCER_PT_sequencer_overlay_waveforms,


    SEQUENCER_PT_cache_settings,
    SEQUENCER_PT_cache_view_settings,
    SEQUENCER_PT_proxy_settings,
    SEQUENCER_PT_strip_proxy,

    SEQUENCER_PT_view,
    SEQUENCER_PT_view_cursor,
    SEQUENCER_PT_frame_overlay,
    SEQUENCER_PT_view_safe_areas,
    SEQUENCER_PT_view_safe_areas_center_cut,
    SEQUENCER_PT_preview,

    SEQUENCER_PT_annotation,
    SEQUENCER_PT_annotation_onion,

    SEQUENCER_PT_snapping,
    SEQUENCER_PT_preview_snapping,
    SEQUENCER_PT_sequencer_snapping,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
