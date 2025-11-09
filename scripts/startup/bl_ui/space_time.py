# SPDX-FileCopyrightText: 2009-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
from bpy.types import Menu, Panel
from bpy.app.translations import (
    pgettext_n as n_,
    contexts as i18n_contexts,
)


class TIME_PT_playhead_snapping(Panel):
    bl_space_type = 'DOPESHEET_EDITOR'
    bl_region_type = 'HEADER'
    bl_label = "Playhead"

    @classmethod
    def poll(cls, context):
        del context
        return True

    def draw(self, context):
        tool_settings = context.tool_settings
        layout = self.layout
        col = layout.column()

        col.prop(tool_settings, "playhead_snap_distance")
        col.separator()
        col.label(text="Snap Target")
        col.prop(tool_settings, "snap_playhead_element", expand=True)
        col.separator()

        if 'FRAME' in tool_settings.snap_playhead_element:
            col.prop(tool_settings, "snap_playhead_frame_step")
        if 'SECOND' in tool_settings.snap_playhead_element:
            col.prop(tool_settings, "snap_playhead_second_step")


def playback_controls(layout, context):
    st = context.space_data
    is_sequencer = st.type == 'SEQUENCE_EDITOR' and st.view_type == 'SEQUENCER'
    is_timeline = st.type == 'DOPESHEET_EDITOR' and st.mode == 'TIMELINE'

    scene = context.scene if not is_sequencer else context.sequencer_scene
    tool_settings = scene.tool_settings if scene else None
    screen = context.screen

    if not scene:
        return

    layout.popover(
        panel="TIME_PT_playback",
        text="Playback",
    )

    if tool_settings and not is_timeline:
        # The Keyframe settings are not exposed in the Timeline view.
        icon_keytype = 'KEYTYPE_{:s}_VEC'.format(tool_settings.keyframe_type)
        layout.popover(
            panel="TIME_PT_keyframing_settings",
            text_ctxt=i18n_contexts.id_windowmanager,
            icon=icon_keytype,
        )

    if is_sequencer:
        layout.prop(context.workspace, "use_scene_time_sync", text="Sync Scene Time")

    layout.separator_spacer()

    if tool_settings:
        row = layout.row(align=True)
        row.prop(tool_settings, "use_keyframe_insert_auto", text="", toggle=True)
        sub = row.row(align=True)
        sub.active = tool_settings.use_keyframe_insert_auto
        sub.popover(
            panel="TIME_PT_auto_keyframing",
            text="",
        )

    row = layout.row(align=True)
    row.operator("screen.frame_jump", text="", icon='REW').end = False
    row.operator("screen.keyframe_jump", text="", icon='PREV_KEYFRAME').next = False

    if not screen.is_animation_playing:
        # if using JACK and A/V sync:
        #   hide the play-reversed button
        #   since JACK transport doesn't support reversed playback
        if scene and scene.sync_mode == 'AUDIO_SYNC' and context.preferences.system.audio_device == 'JACK':
            row.scale_x = 2
            row.operator("screen.animation_play", text="", icon='PLAY')
            row.scale_x = 1
        else:
            row.operator("screen.animation_play", text="", icon='PLAY_REVERSE').reverse = True
            row.operator("screen.animation_play", text="", icon='PLAY')
    else:
        row.scale_x = 2
        row.operator("screen.animation_play", text="", icon='PAUSE')
        row.scale_x = 1

    row.operator("screen.keyframe_jump", text="", icon='NEXT_KEYFRAME').next = True
    row.operator("screen.frame_jump", text="", icon='FF').end = True

    # Time jump
    row = layout.row(align=True)
    row.operator("screen.time_jump", text="", icon='FRAME_PREV').backward = True
    row.operator("screen.time_jump", text="", icon='FRAME_NEXT').backward = False
    row.popover(panel="TIME_PT_jump", text="")

    if tool_settings:
        row = layout.row(align=True)
        row.prop(tool_settings, "use_snap_playhead", text="")
        sub = row.row(align=True)
        sub.popover(panel="TIME_PT_playhead_snapping", text="")

    layout.separator_spacer()

    if scene:
        row = layout.row()
        if scene.show_subframe:
            row.scale_x = 1.15
            row.prop(scene, "frame_float", text="")
        else:
            row.scale_x = 0.95
            row.prop(scene, "frame_current", text="")

        row = layout.row(align=True)
        row.prop(scene, "use_preview_range", text="", toggle=True)
        sub = row.row(align=True)
        sub.scale_x = 0.8
        if not scene.use_preview_range:
            sub.prop(scene, "frame_start", text="Start")
            sub.prop(scene, "frame_end", text="End")
        else:
            sub.prop(scene, "frame_preview_start", text="Start")
            sub.prop(scene, "frame_preview_end", text="End")


class TIME_MT_view(Menu):
    bl_label = "View"

    def draw(self, context):
        layout = self.layout
        scene = context.scene
        st = context.space_data
        layout.prop(st, "show_region_hud")
        layout.prop(st, "show_region_channels")
        layout.separator()
        layout.operator("action.view_all")
        if context.scene.use_preview_range:
            layout.operator("anim.scene_range_frame", text="Frame Preview Range")
        else:
            layout.operator("anim.scene_range_frame", text="Frame Scene Range")
        layout.operator("action.view_frame")
        layout.separator()
        layout.prop(st, "show_markers")
        layout.prop(st, "show_seconds")
        layout.prop(st, "show_locked_time")
        layout.separator()
        layout.prop(scene, "show_keys_from_selected_only")
        layout.prop(st.dopesheet, "show_only_errors")
        layout.separator()
        layout.menu("DOPESHEET_MT_cache")
        layout.separator()
        layout.menu("INFO_MT_area")


def marker_menu_generic(layout, context):

    # layout.operator_context = 'EXEC_REGION_WIN'

    layout.column()

    tool_settings = context.tool_settings
    layout.prop(tool_settings, "lock_markers")

    layout.separator()

    layout.operator("screen.marker_jump", text="Jump to Previous Marker").next = False
    layout.operator("screen.marker_jump", text="Jump to Next Marker").next = True

    layout.separator()

    layout.operator("marker.camera_bind")

    layout.separator()

    layout.menu("NLA_MT_marker_select")

    layout.separator()

    layout.operator("marker.move", text="Move Marker")
    props = layout.operator("wm.call_panel", text="Rename Marker")
    props.name = "TOPBAR_PT_name_marker"
    props.keep_open = False

    layout.separator()

    layout.operator("marker.delete", text="Delete Marker")

    if len(bpy.data.scenes) > 10:
        layout.operator_context = 'INVOKE_DEFAULT'
        layout.operator("marker.make_links_scene", text="Duplicate Marker to Scene...", icon='OUTLINER_OB_EMPTY')
    else:
        layout.operator_menu_enum("marker.make_links_scene", "scene", text="Duplicate Marker to Scene")

    layout.operator("marker.duplicate", text="Duplicate Marker")
    layout.operator("marker.add", text="Add Marker")


###################################


class TimelinePanelButtons:
    bl_space_type = 'DOPESHEET_EDITOR'
    bl_region_type = 'UI'


class TIME_PT_playback(TimelinePanelButtons, Panel):
    bl_label = "Playback"
    bl_region_type = 'HEADER'
    bl_ui_units_x = 13

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        screen = context.screen
        st = context.space_data
        is_sequencer = st.type == 'SEQUENCE_EDITOR' and st.view_type == 'SEQUENCER'
        scene = context.scene if not is_sequencer else context.sequencer_scene

        layout.prop(scene, "sync_mode", text="Sync")
        col = layout.column(heading="Audio")
        col.prop(scene, "use_audio_scrub", text="Scrubbing")
        col.prop(scene, "use_audio")

        col = layout.column(heading="Playback")
        col.prop(scene, "lock_frame_selection_to_range", text="Limit to Frame Range")
        col.prop(screen, "use_follow", text="Follow Current Frame")

        col = layout.column(heading="Play In")
        col.prop(screen, "use_play_top_left_3d_editor", text="Active Editor")
        col.prop(screen, "use_play_3d_editors", text="3D Viewport")
        col.prop(screen, "use_play_animation_editors", text="Animation Editors")
        col.prop(screen, "use_play_image_editors", text="Image Editor")
        col.prop(screen, "use_play_properties_editors", text="Properties and Sidebars")
        col.prop(screen, "use_play_clip_editors", text="Movie Clip Editor")
        col.prop(screen, "use_play_node_editors", text="Node Editors")
        col.prop(screen, "use_play_sequence_editors", text="Video Sequencer")
        col.prop(screen, "use_play_spreadsheet_editors", text="Spreadsheet")

        col = layout.column(heading="Show")
        col.prop(scene, "show_subframe", text="Subframes")

        layout.separator()

        row = layout.row(align=True)
        row.operator("anim.start_frame_set")
        row.operator("anim.end_frame_set")


class TIME_PT_keyframing_settings(TimelinePanelButtons, Panel):
    bl_label = "Keyframing Settings"
    bl_options = {'HIDE_HEADER'}
    bl_region_type = 'HEADER'
    bl_description = "Active keying set and keyframing settings"

    def draw_header(self, context):
        st = context.space_data
        is_sequencer = st.type == 'SEQUENCE_EDITOR' and st.view_type == 'SEQUENCER'
        scene = context.scene if not is_sequencer else context.sequencer_scene
        if scene.keying_sets_all.active:
            self.bl_label = scene.keying_sets_all.active.bl_label
            if scene.keying_sets_all.active.bl_label in scene.keying_sets:
                # Do not translate, this keying set is user-defined.
                self.bl_translation_context = i18n_contexts.no_translation
            else:
                # Use the keying set's translation context (default).
                self.bl_translation_context = scene.keying_sets_all.active.bl_rna.translation_context
        else:
            # Use a custom translation context to differentiate from compositing keying.
            self.bl_label = n_("Keying", i18n_contexts.id_windowmanager)
            self.bl_translation_context = i18n_contexts.id_windowmanager

    def draw(self, context):
        layout = self.layout

        st = context.space_data
        is_sequencer = st.type == 'SEQUENCE_EDITOR' and st.view_type == 'SEQUENCER'
        scene = context.scene if not is_sequencer else context.sequencer_scene
        tool_settings = context.tool_settings

        col = layout.column(align=True)
        col.label(text="Active Keying Set")
        row = col.row(align=True)
        row.prop_search(scene.keying_sets_all, "active", scene, "keying_sets_all", text="")
        row.operator("anim.keyframe_insert", text="", icon='KEY_HLT')
        row.operator("anim.keyframe_delete", text="", icon='KEY_DEHLT')

        col = layout.column(align=True)
        col.label(text="New Keyframe Type")
        col.prop(tool_settings, "keyframe_type", text="")

        layout.prop(tool_settings, "use_keyframe_cycle_aware")


class TIME_PT_auto_keyframing(TimelinePanelButtons, Panel):
    bl_label = "Auto Keyframing"
    bl_options = {'HIDE_HEADER'}
    bl_region_type = 'HEADER'
    bl_ui_units_x = 9

    def draw(self, context):
        layout = self.layout

        tool_settings = context.tool_settings
        prefs = context.preferences

        layout.active = tool_settings.use_keyframe_insert_auto

        layout.prop(tool_settings, "auto_keying_mode", expand=True)

        col = layout.column(align=True)
        col.prop(tool_settings, "use_keyframe_insert_keyingset", text="Only Active Keying Set", toggle=False)
        if not prefs.edit.use_keyframe_insert_available:
            col.prop(tool_settings, "use_record_with_nla", text="Layered Recording")


class TIME_PT_jump(TimelinePanelButtons, Panel):
    bl_label = "Time Jump"
    bl_options = {'HIDE_HEADER'}
    bl_region_type = 'HEADER'
    bl_ui_units_x = 10

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        st = context.space_data
        is_sequencer = st.type == 'SEQUENCE_EDITOR' and st.view_type == 'SEQUENCER'
        scene = context.scene if not is_sequencer else context.sequencer_scene

        layout.prop(scene, "time_jump_unit", expand=True, text="Jump Unit")
        layout.prop(scene, "time_jump_delta", text="Delta")


###################################

classes = (
    TIME_MT_view,
    TIME_PT_playback,
    TIME_PT_keyframing_settings,
    TIME_PT_auto_keyframing,
    TIME_PT_jump,
    TIME_PT_playhead_snapping,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
