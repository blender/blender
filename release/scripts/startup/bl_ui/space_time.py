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


# Header buttons for timeline header (play, etc.)
class TIME_HT_editor_buttons(Header):
    bl_idname = "TIME_HT_editor_buttons"
    bl_space_type = 'DOPESHEET_EDITOR'
    bl_label = ""

    def draw(self, context):
        pass

    @staticmethod
    def draw_header(context, layout):
        scene = context.scene
        toolsettings = context.tool_settings
        screen = context.screen

        layout.separator_spacer()

        layout.prop(toolsettings, "use_keyframe_insert_auto", text="", toggle=True)

        row = layout.row(align=True)
        row.operator("screen.frame_jump", text="", icon='REW').end = False
        row.operator("screen.keyframe_jump", text="", icon='PREV_KEYFRAME').next = False
        if not screen.is_animation_playing:
            # if using JACK and A/V sync:
            #   hide the play-reversed button
            #   since JACK transport doesn't support reversed playback
            if scene.sync_mode == 'AUDIO_SYNC' and context.user_preferences.system.audio_device == 'JACK':
                sub = row.row(align=True)
                sub.scale_x = 1.4
                sub.operator("screen.animation_play", text="", icon='PLAY')
            else:
                row.operator("screen.animation_play", text="", icon='PLAY_REVERSE').reverse = True
                row.operator("screen.animation_play", text="", icon='PLAY')
        else:
            sub = row.row(align=True)
            sub.scale_x = 1.4
            sub.operator("screen.animation_play", text="", icon='PAUSE')
        row.operator("screen.keyframe_jump", text="", icon='NEXT_KEYFRAME').next = True
        row.operator("screen.frame_jump", text="", icon='FF').end = True

        layout.separator_spacer()

        row = layout.row()
        row.scale_x = 0.95
        if scene.show_subframe:
            row.prop(scene, "frame_float", text="")
        else:
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


class TIME_MT_editor_menus(Menu):
    bl_idname = "TIME_MT_editor_menus"
    bl_label = ""

    def draw(self, context):
        self.draw_menus(self.layout, context, horizontal=False)

    @staticmethod
    def draw_menus(layout, context, horizontal=True):
        if horizontal:
            row = layout.row()
            sub = row.row(align=True)
        else:
            sub = layout

        sub.popover(
            panel="TIME_PT_playback",
            text="Playback",
        )
        sub.popover(
            panel="TIME_PT_keyframing_settings",
            text="Keying",
        )

        if horizontal:
            sub = row.row(align=True)

        sub.menu("TIME_MT_view")
        sub.menu("TIME_MT_marker")


class TIME_MT_marker(Menu):
    bl_label = "Marker"

    def draw(self, context):
        layout = self.layout

        marker_menu_generic(layout)


class TIME_MT_view(Menu):
    bl_label = "View"

    def draw(self, context):
        layout = self.layout

        scene = context.scene
        st = context.space_data

        layout.prop(st, "show_seconds")
        layout.prop(st, "show_locked_time")

        layout.separator()

        layout.prop(st, "show_frame_indicator")
        layout.prop(scene, "show_keys_from_selected_only")

        layout.separator()

        layout.menu("TIME_MT_cache")

        layout.separator()

        # NOTE: "action" now, since timeline is in the dopesheet editor, instead of as own editor
        layout.operator("action.view_all")
        layout.operator("action.view_frame")

        layout.separator()

        layout.menu("INFO_MT_area")


class TIME_MT_cache(Menu):
    bl_label = "Cache"

    def draw(self, context):
        layout = self.layout

        st = context.space_data

        layout.prop(st, "show_cache")

        layout.separator()

        col = layout.column()
        col.enabled = st.show_cache
        col.prop(st, "cache_softbody")
        col.prop(st, "cache_particles")
        col.prop(st, "cache_cloth")
        col.prop(st, "cache_smoke")
        col.prop(st, "cache_dynamicpaint")
        col.prop(st, "cache_rigidbody")


def marker_menu_generic(layout):
    from bpy import context

    # layout.operator_context = 'EXEC_REGION_WIN'

    layout.column()
    layout.operator("marker.add", "Add Marker")
    layout.operator("marker.duplicate", text="Duplicate Marker")

    if len(bpy.data.scenes) > 10:
        layout.operator_context = 'INVOKE_DEFAULT'
        layout.operator("marker.make_links_scene", text="Duplicate Marker to Scene...", icon='OUTLINER_OB_EMPTY')
    else:
        layout.operator_menu_enum("marker.make_links_scene", "scene", text="Duplicate Marker to Scene")

    layout.operator("marker.delete", text="Delete Marker")

    layout.separator()

    layout.operator("marker.rename", text="Rename Marker")
    layout.operator("marker.move", text="Grab/Move Marker")

    layout.separator()

    layout.operator("marker.camera_bind")

    layout.separator()

    layout.operator("screen.marker_jump", text="Jump to Next Marker").next = True
    layout.operator("screen.marker_jump", text="Jump to Previous Marker").next = False

    layout.separator()
    ts = context.tool_settings
    layout.prop(ts, "lock_markers")

###################################


class TimelinePanelButtons:
    bl_space_type = 'DOPESHEET_EDITOR'
    bl_region_type = 'UI'

    @staticmethod
    def has_timeline(context):
        return context.space_data.mode == 'TIMELINE'


class TIME_PT_playback(TimelinePanelButtons, Panel):
    bl_label = "Playback"
    bl_region_type = 'HEADER'

    def draw(self, context):
        layout = self.layout

        screen = context.screen
        scene = context.scene

        layout.prop(scene, "sync_mode", text="")
        layout.prop(scene, "use_audio_scrub")
        layout.prop(scene, "use_audio", text="Mute Audio")

        layout.prop(scene, "show_subframe", text="Subframes")

        layout.prop(scene, "lock_frame_selection_to_range", text="Limit Playhead to Frame Range")
        layout.prop(screen, "use_follow", text="Follow Playhead")

        layout.separator()

        col = layout.column()
        col.label("Play Animation In:")
        layout.prop(screen, "use_play_top_left_3d_editor", text="Active Editor Only")
        layout.prop(screen, "use_play_3d_editors")
        layout.prop(screen, "use_play_animation_editors")
        layout.prop(screen, "use_play_properties_editors")
        layout.prop(screen, "use_play_image_editors")
        layout.prop(screen, "use_play_sequence_editors")
        layout.prop(screen, "use_play_node_editors")
        layout.prop(screen, "use_play_clip_editors")

        layout.separator()

        row = layout.row(align=True)
        row.operator("anim.start_frame_set")
        row.operator("anim.end_frame_set")


class TIME_PT_keyframing_settings(TimelinePanelButtons, Panel):
    bl_label = "Keyframing Settings"
    bl_options = {'HIDE_HEADER'}
    bl_region_type = 'HEADER'

    @classmethod
    def poll(cls, context):
        # only for timeline editor
        return cls.has_timeline(context)

    def draw(self, context):
        layout = self.layout

        scene = context.scene
        toolsettings = context.tool_settings
        userprefs = context.user_preferences

        col = layout.column(align=True)
        col.label("Active Keying Set:")
        row = col.row(align=True)
        row.prop_search(scene.keying_sets_all, "active", scene, "keying_sets_all", text="")
        row.operator("anim.keyframe_insert", text="", icon='KEY_HLT')
        row.operator("anim.keyframe_delete", text="", icon='KEY_DEHLT')

        col = layout.column(align=True)
        col.label("New Keyframe Type:")
        col.prop(toolsettings, "keyframe_type", text="")

        col = layout.column(align=True)
        col.label("Auto Keyframing:")
        row = col.row()
        row.prop(toolsettings, "auto_keying_mode", text="")
        row.prop(toolsettings, "use_keyframe_insert_keyingset", text="")
        if not userprefs.edit.use_keyframe_insert_available:
            col.prop(toolsettings, "use_record_with_nla", text="Layered Recording")


###################################

classes = (
    TIME_HT_editor_buttons,
    TIME_MT_editor_menus,
    TIME_MT_marker,
    TIME_MT_view,
    TIME_MT_cache,
    TIME_PT_playback,
    TIME_PT_keyframing_settings,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
