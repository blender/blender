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
from bpy.types import Panel, Header, Menu


class CLIP_HT_header(Header):
    bl_space_type = 'CLIP_EDITOR'

    def draw(self, context):
        layout = self.layout

        sc = context.space_data
        clip = sc.clip

        row = layout.row(align=True)
        row.template_header()

        if context.area.show_menus:
            sub = row.row(align=True)
            sub.menu("CLIP_MT_view")
            sub.menu("CLIP_MT_clip")

            if clip:
                sub.menu("CLIP_MT_select")
                sub.menu("CLIP_MT_track")
                sub.menu("CLIP_MT_reconstruction")

        if clip:
            layout.prop(sc, "mode", text="")

        row = layout.row()
        row.template_ID(sc, "clip", open='clip.open')

        if clip:
            r = clip.tracking.reconstruction

            if r.is_reconstructed:
                layout.label(text="Average solve error: %.4f" % \
                    (r.average_error))

        layout.template_running_jobs()


class CLIP_PT_tools(Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'TOOLS'
    bl_label = "Tools"

    @classmethod
    def poll(cls, context):
        sc = context.space_data
        clip = sc.clip

        return not clip and sc.mode == 'TRACKING'

    def draw(self, context):
        layout = self.layout
        layout.operator('clip.open')


class CLIP_PT_tools_marker(Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'TOOLS'
    bl_label = "Marker"

    @classmethod
    def poll(cls, context):
        sc = context.space_data
        clip = sc.clip

        return clip and sc.mode == 'TRACKING'

    def draw(self, context):
        layout = self.layout

        col = layout.column(align=True)
        col.operator("clip.add_marker_move")
        col.operator("clip.detect_features")
        col.operator("clip.delete_track")


class CLIP_PT_tools_tracking(Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'TOOLS'
    bl_label = "Track"

    @classmethod
    def poll(cls, context):
        sc = context.space_data
        clip = sc.clip

        return clip and sc.mode == 'TRACKING'

    def draw(self, context):
        layout = self.layout
        clip = context.space_data.clip
        settings = clip.tracking.settings

        row = layout.row(align=True)

        op = row.operator("clip.track_markers", text="", icon='FRAME_PREV')
        op.backwards = True
        op = row.operator("clip.track_markers", text="", \
             icon='PLAY_REVERSE')
        op.backwards = True
        op.sequence = True
        op = row.operator("clip.track_markers", text="", icon='PLAY')
        op.sequence = True
        row.operator("clip.track_markers", text="", icon='FRAME_NEXT')

        col = layout.column(align=True)
        op = col.operator("clip.clear_track_path", text="Clear After")
        op.action = 'REMAINED'

        op = col.operator("clip.clear_track_path", text="Clear Before")
        op.action = 'UPTO'

        op = col.operator("clip.clear_track_path", text="Clear Track Path")
        op.action = 'ALL'

        layout.operator("clip.join_tracks")


class CLIP_PT_tools_solving(Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'TOOLS'
    bl_label = "Solving"

    @classmethod
    def poll(cls, context):
        sc = context.space_data
        clip = sc.clip

        return clip and sc.mode == 'TRACKING'

    def draw(self, context):
        layout = self.layout
        clip = context.space_data.clip
        settings = clip.tracking.settings

        col = layout.column(align=True)
        col.prop(settings, "keyframe_a")
        col.prop(settings, "keyframe_b")

        col = layout.column(align=True)
        col.operator("clip.solve_camera")
        col.operator("clip.clear_solution")


class CLIP_PT_tools_cleanup(Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'TOOLS'
    bl_label = "Clean up"

    @classmethod
    def poll(cls, context):
        sc = context.space_data
        clip = sc.clip

        return clip and sc.mode == 'TRACKING'

    def draw(self, context):
        layout = self.layout
        clip = context.space_data.clip
        settings = clip.tracking.settings

        layout.prop(settings, 'clean_frames', text="Frames")
        layout.prop(settings, 'clean_error', text="Error")
        layout.prop(settings, 'clean_action', text="")

        layout.operator("clip.clean_tracks")


class CLIP_PT_tools_geometry(Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'TOOLS'
    bl_label = "Geometry"

    @classmethod
    def poll(cls, context):
        sc = context.space_data
        clip = sc.clip

        return clip and sc.mode == 'RECONSTRUCTION'

    def draw(self, context):
        layout = self.layout

        layout.operator("clip.bundles_to_mesh")
        layout.operator("clip.track_to_empty")


class CLIP_PT_tools_orientation(Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'TOOLS'
    bl_label = "Orientation"

    @classmethod
    def poll(cls, context):
        sc = context.space_data
        clip = sc.clip

        return clip and sc.mode == 'RECONSTRUCTION'

    def draw(self, context):
        sc = context.space_data
        layout = self.layout
        settings = sc.clip.tracking.settings

        col = layout.column(align=True)
        col.label(text="Scene Orientation:")
        col.operator("clip.set_floor")
        col.operator("clip.set_origin")

        row = col.row()
        row.operator("clip.set_axis", text="Set X Axis").axis = 'X'
        row.operator("clip.set_axis", text="Set Y Axis").axis = 'Y'

        col = layout.column()
        col.prop(settings, "distance")
        col.operator("clip.set_scale")


class CLIP_PT_tools_grease_pencil(Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'TOOLS'
    bl_label = "Grease Pencil"

    @classmethod
    def poll(cls, context):
        sc = context.space_data
        clip = sc.clip

        return clip and sc.mode == 'DISTORTION'

    def draw(self, context):
        layout = self.layout

        col = layout.column(align=True)

        row = col.row(align=True)
        row.operator("gpencil.draw", text="Draw").mode = 'DRAW'
        row.operator("gpencil.draw", text="Line").mode = 'DRAW_STRAIGHT'

        row = col.row(align=True)
        row.operator("gpencil.draw", text="Poly").mode = 'DRAW_POLY'
        row.operator("gpencil.draw", text="Erase").mode = 'ERASER'

        row = col.row()
        row.prop(context.tool_settings, "use_grease_pencil_sessions")


class CLIP_PT_track(Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'UI'
    bl_label = "Track"

    @classmethod
    def poll(cls, context):
        sc = context.space_data
        clip = sc.clip

        return sc.mode == 'TRACKING' and clip

    def draw(self, context):
        layout = self.layout
        sc = context.space_data
        clip = context.space_data.clip
        act_track = clip.tracking.active_track

        if not act_track:
            layout.active = False
            layout.label(text="No active track")
            return

        row = layout.row()
        row.prop(act_track, "name", text="")

        sub = row.row(align=True)

        sub.template_marker(sc, "clip", sc.clip_user, act_track, True)

        icon = 'LOCKED' if act_track.locked else 'UNLOCKED'
        sub.prop(act_track, "locked", text="", icon=icon)

        layout.template_track(sc, "scopes")

        row = layout.row()
        row.prop(act_track, "use_red_channel", text="Red")
        row.prop(act_track, "use_green_channel", text="Green")
        row.prop(act_track, "use_blue_channel", text="Blue")

        layout.separator()

        row = layout.row(align=True)
        label = bpy.types.CLIP_MT_track_color_presets.bl_label
        row.menu('CLIP_MT_track_color_presets', text=label)
        row.menu('CLIP_MT_track_color_specials', text="", icon="DOWNARROW_HLT")
        row.operator("clip.track_color_preset_add", text="", icon="ZOOMIN")
        op = row.operator("clip.track_color_preset_add", \
            text="", icon="ZOOMOUT")
        op.remove_active = True

        row = layout.row()
        row.prop(act_track, "use_custom_color")
        if act_track.use_custom_color:
            row.prop(act_track, "color", text="")

        if act_track.has_bundle:
            label_text = "Average Error: %.4f" % (act_track.average_error)
            layout.label(text=label_text)


class CLIP_PT_tracking_camera(Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'UI'
    bl_label = "Camera Data"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        sc = context.space_data

        return sc.mode in ['TRACKING', 'DISTORTION'] and sc.clip

    def draw(self, context):
        layout = self.layout

        sc = context.space_data
        clip = sc.clip

        row = layout.row(align=True)
        label = bpy.types.CLIP_MT_camera_presets.bl_label
        row.menu('CLIP_MT_camera_presets', text=label)
        row.operator("clip.camera_preset_add", text="", icon="ZOOMIN")
        op = row.operator("clip.camera_preset_add", text="", icon="ZOOMOUT")
        op.remove_active = True

        row = layout.row(align=True)
        sub = row.split(percentage=0.65)
        sub.prop(clip.tracking.camera, "focal_length")
        sub.prop(clip.tracking.camera, "units", text="")

        col = layout.column(align=True)
        col.label(text="Sensor:")
        col.prop(clip.tracking.camera, "sensor_width", text="Size")
        col.prop(clip.tracking.camera, "pixel_aspect")

        col = layout.column()
        col.label(text="Principal Point")
        row = col.row()
        row.prop(clip.tracking.camera, "principal", text="")
        col.operator("clip.set_center_principal", text="Center")

        col = layout.column(align=True)
        col.label(text="Undistortion:")
        col.prop(clip.tracking.camera, "k1")
        col.prop(clip.tracking.camera, "k2")
        col.prop(clip.tracking.camera, "k3")


class CLIP_PT_display(Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'UI'
    bl_label = "Display"

    def draw(self, context):
        layout = self.layout
        sc = context.space_data

        row = layout.row()
        row.prop(sc, "show_marker_pattern", text="Pattern")
        row.prop(sc, "show_marker_search", text="Search")

        row = layout.row()
        row.prop(sc, "show_track_path", text="Path")
        sub = row.column()
        sub.active = sc.show_track_path
        sub.prop(sc, "path_length", text="Length")

        row = layout.row()
        row.prop(sc, "show_disabled", text="Disabled")
        row.prop(sc, "show_bundles", text="Bundles")

        row = layout.row()
        row.prop(sc, "show_names", text="Names")
        row.prop(sc, "show_tiny_markers", text="Tiny Markers")

        row = layout.row()
        row.prop(sc, "show_grease_pencil", text="Grease Pencil")
        row.prop(sc, "use_mute_footage", text="Mute")

        if sc.mode == 'DISTORTION':
            layout.prop(sc, "show_grid", text="Grid")
            layout.prop(sc, "use_manual_calibration")
        elif sc.mode == 'RECONSTRUCTION':
            layout.prop(sc, "show_stable", text="Stable")

        layout.prop(sc, "lock_selection")

        clip = sc.clip
        if clip:
            layout.label(text="Display Aspect:")
            layout.prop(clip, "display_aspect", text="")


class CLIP_PT_track_settings(Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'UI'
    bl_label = "Tracking Settings"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        sc = context.space_data

        return sc.mode == 'TRACKING' and sc.clip

    def draw(self, context):
        layout = self.layout
        clip = context.space_data.clip
        settings = clip.tracking.settings

        layout.prop(settings, "tracker")

        layout.prop(settings, "adjust_frames")

        if settings.tracker == "SAD":
            layout.prop(settings, "min_correlation")

        layout.prop(settings, "speed")
        layout.prop(settings, "frames_limit")
        layout.prop(settings, "margin")


class CLIP_PT_stabilization(Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'UI'
    bl_label = "2D Stabilization"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        sc = context.space_data

        return sc.mode == 'RECONSTRUCTION' and sc.clip

    def draw_header(self, context):
        sc = context.space_data
        tracking = sc.clip.tracking
        stab = tracking.stabilization

        self.layout.prop(stab, "use_2d_stabilization", text="")

    def draw(self, context):
        layout = self.layout
        sc = context.space_data
        tracking = sc.clip.tracking
        stab = tracking.stabilization

        layout.active = stab.use_2d_stabilization

        row = layout.row()
        row.template_list(stab, "tracks", stab, "active_track_index", rows=3)

        sub = row.column(align=True)

        sub.operator("clip.stabilize_2d_add", icon='ZOOMIN', text="")
        sub.operator("clip.stabilize_2d_remove", icon='ZOOMOUT', text="")

        sub.menu('CLIP_MT_stabilize_2d_specials', text="", \
            icon="DOWNARROW_HLT")

        layout.prop(stab, "influence_location")

        layout.separator()

        layout.prop(stab, "use_autoscale")
        col = layout.column()
        col.active = stab.use_autoscale
        col.prop(stab, "max_scale")
        col.prop(stab, "influence_scale")

        layout.separator()

        layout.label(text="Rotation:")

        row = layout.row(align=True)
        row.prop_search(stab, "rotation_track", tracking, "tracks", text="")
        row.operator("clip.stabilize_2d_set_rotation", text="", icon='ZOOMIN')

        row = layout.row()
        row.active = stab.rotation_track is not None
        row.prop(stab, "influence_rotation")


class CLIP_PT_marker(Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'UI'
    bl_label = "Marker"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        sc = context.space_data
        clip = sc.clip

        return sc.mode == 'TRACKING' and clip

    def draw(self, context):
        layout = self.layout
        sc = context.space_data
        clip = context.space_data.clip
        act_track = clip.tracking.active_track

        if act_track:
            layout.template_marker(sc, "clip", sc.clip_user, act_track, False)
        else:
            layout.active = False
            layout.label(text="No active track")


class CLIP_PT_proxy(Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'UI'
    bl_label = "Proxy / Timecode"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        sc = context.space_data

        return sc.clip

    def draw_header(self, context):
        sc = context.space_data

        self.layout.prop(sc.clip, "use_proxy", text="")

    def draw(self, context):
        layout = self.layout
        sc = context.space_data
        clip = sc.clip

        layout.active = clip.use_proxy

        layout.label(text="Build Sizes:")

        row = layout.row()
        row.prop(clip.proxy, "build_25")
        row.prop(clip.proxy, "build_50")

        row = layout.row()
        row.prop(clip.proxy, "build_75")
        row.prop(clip.proxy, "build_100")

        layout.prop(clip.proxy, "build_undistorted")

        layout.prop(clip.proxy, "quality")

        layout.prop(clip, 'use_proxy_custom_directory')
        if clip.use_proxy_custom_directory:
            layout.prop(clip.proxy, "directory")

        layout.operator("clip.rebuild_proxy", text="Rebuild Proxy")

        if clip.source == 'MOVIE':
            col = layout.column()

            col.label(text="Use timecode index:")
            col.prop(clip.proxy, "timecode", text="")

        col = layout.column()
        col.label(text="Proxy render size:")

        col.prop(sc.clip_user, "proxy_render_size", text="")
        col.prop(sc.clip_user, "use_render_undistorted")


class CLIP_PT_footage(Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'UI'
    bl_label = "Footage Settings"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        sc = context.space_data

        return sc.clip

    def draw(self, context):
        layout = self.layout

        sc = context.space_data
        clip = sc.clip

        if clip:
            layout.template_movieclip(sc, "clip", compact=True)
        else:
            layout.operator("clip.open", icon='FILESEL')


class CLIP_PT_tools_clip(Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'TOOLS'
    bl_label = "Clip"

    def draw(self, context):
        layout = self.layout
        clip = context.space_data.clip

        layout.operator("clip.set_viewport_background")


class CLIP_MT_view(Menu):
    bl_label = "View"

    def draw(self, context):
        layout = self.layout

        layout.operator("clip.properties", icon='MENU_PANEL')
        layout.operator("clip.tools", icon='MENU_PANEL')
        layout.separator()

        layout.operator("clip.view_selected")
        layout.operator("clip.view_all")

        layout.separator()
        layout.operator("clip.view_zoom_in")
        layout.operator("clip.view_zoom_out")

        layout.separator()

        ratios = [[1, 8], [1, 4], [1, 2], [1, 1], [2, 1], [4, 1], [8, 1]]

        for a, b in ratios:
            text = "Zoom %d:%d" % (a, b)
            layout.operator("clip.view_zoom_ratio", text=text).ratio = a / b

        layout.separator()
        layout.operator("screen.area_dupli")
        layout.operator("screen.screen_full_area")


class CLIP_MT_clip(Menu):
    bl_label = "Clip"

    def draw(self, context):
        layout = self.layout

        sc = context.space_data
        clip = sc.clip

        layout.menu("CLIP_MT_proxy")

        if clip:
            layout.operator("clip.reload")

        layout.operator("clip.open")


class CLIP_MT_proxy(Menu):
    bl_label = "Proxy"

    def draw(self, context):
        layout = self.layout

        sc = context.space_data
        clip = sc.clip

        layout.operator("clip.rebuild_proxy")
        layout.operator("clip.delete_proxy")


class CLIP_MT_track(Menu):
    bl_label = "Track"

    def draw(self, context):
        layout = self.layout

        layout.operator("clip.clear_solution")
        layout.operator("clip.solve_camera")

        layout.separator()
        op = layout.operator("clip.clear_track_path", text="Clear After")
        op.action = 'REMAINED'

        op = layout.operator("clip.clear_track_path", text="Clear Before")
        op.action = 'UPTO'

        op = layout.operator("clip.clear_track_path", text="Clear Track Path")
        op.action = 'ALL'

        layout.separator()
        layout.operator("clip.join_tracks")

        layout.separator()
        layout.operator("clip.clean_tracks")

        layout.separator()
        op = layout.operator("clip.track_markers", \
            text="Track Frame Backwards")
        op.backwards = True

        op = layout.operator("clip.track_markers", text="Track Backwards")
        op.backwards = True
        op.sequence = True

        op = layout.operator("clip.track_markers", text="Track Forwards")
        op.sequence = True
        layout.operator("clip.track_markers", text="Track Frame Forwards")

        layout.separator()
        layout.operator("clip.delete_track")
        layout.operator("clip.delete_marker")

        layout.separator()
        layout.operator("clip.add_marker_move")

        layout.separator()
        layout.menu("CLIP_MT_track_visibility")
        layout.menu("CLIP_MT_track_transform")


class CLIP_MT_reconstruction(Menu):
    bl_label = "Reconstruction"

    def draw(self, context):
        layout = self.layout

        layout.operator("clip.set_origin")
        layout.operator("clip.set_floor")

        layout.operator("clip.set_axis", text="Set X Asix").axis = "X"
        layout.operator("clip.set_axis", text="Set Y Asix").axis = "Y"

        layout.operator("clip.set_scale")

        layout.separator()

        layout.operator("clip.track_to_empty")
        layout.operator("clip.bundles_to_mesh")


class CLIP_MT_track_visibility(Menu):
    bl_label = "Show/Hide"

    def draw(self, context):
        layout = self.layout

        layout.operator("clip.hide_tracks_clear", text="Show Hidden")
        layout.operator("clip.hide_tracks", text="Hide Selected")

        op = layout.operator("clip.hide_tracks", text="Hide Unselected")
        op.unselected = True


class CLIP_MT_track_transform(Menu):
    bl_label = "Transform"

    def draw(self, context):
        layout = self.layout

        layout.operator("transform.translate")
        layout.operator("transform.resize")


class CLIP_MT_select(Menu):
    bl_label = "Select"

    def draw(self, context):
        layout = self.layout

        sc = context.space_data

        layout.menu("CLIP_MT_select_grouped")
        layout.operator("clip.select_border")
        layout.operator("clip.select_circle")
        layout.operator("clip.select_all", text="Select/Deselect all")
        layout.operator("clip.select_all", text="Inverse").action = 'INVERT'


class CLIP_MT_select_grouped(Menu):
    bl_label = "Select Grouped"

    def draw(self, context):
        layout = self.layout

        sc = context.space_data

        op = layout.operator("clip.select_grouped", text="Select Keyframed")
        op.group = 'KEYFRAMED'

        op = layout.operator("clip.select_grouped", text="Select Estimated")
        op.group = 'ESTIMATED'

        op = layout.operator("clip.select_grouped", text="Select Tracked")
        op.group = 'TRACKED'

        op = layout.operator("clip.select_grouped", text="Select Locked")
        op.group = 'LOCKED'

        op = layout.operator("clip.select_grouped", text="Select Disabled")
        op.group = 'DISABLED'

        op = layout.operator("clip.select_grouped", text="Select Failed")
        op.group = 'FAILED'

        op = layout.operator("clip.select_grouped", text="Select by Color")
        op.group = 'COLOR'


class CLIP_MT_tracking_specials(Menu):
    bl_label = "Specials"

    @classmethod
    def poll(cls, context):
        return context.space_data.clip

    def draw(self, context):
        layout = self.layout

        op = layout.operator("clip.disable_markers", text="Enable Markers")
        op.action = 'ENABLE'

        op = layout.operator("clip.disable_markers", text="Disable markers")
        op.action = 'DISABLE'

        layout.separator()
        layout.operator("clip.set_origin")

        layout.separator()
        layout.operator("clip.hide_tracks")
        layout.operator("clip.hide_tracks_clear", text="Show Tracks")

        layout.separator()
        op = layout.operator("clip.lock_tracks", text="Lock Tracks")
        op.action = 'LOCK'

        op = layout.operator("clip.lock_tracks", text="Unlock Tracks")
        op.action = 'UNLOCK'


class CLIP_MT_camera_presets(Menu):
    bl_label = "Camera Presets"
    preset_subdir = "tracking_camera"
    preset_operator = "script.execute_preset"
    draw = bpy.types.Menu.draw_preset


class CLIP_MT_track_color_presets(Menu):
    bl_label = "Color Presets"
    preset_subdir = "tracking_track_color"
    preset_operator = "script.execute_preset"
    draw = bpy.types.Menu.draw_preset


class CLIP_MT_track_color_specials(Menu):
    bl_label = "Track Color Specials"

    def draw(self, context):
        layout = self.layout

        layout.operator('clip.track_copy_color', icon='COPY_ID')


class CLIP_MT_stabilize_2d_specials(Menu):
    bl_label = "Track Color Specials"

    def draw(self, context):
        layout = self.layout

        layout.operator('clip.stabilize_2d_select')


if __name__ == "__main__":  # only for live edit.
    bpy.utils.register_module(__name__)
