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

# <pep8-80 compliant>

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

            if clip:
                sub.menu("CLIP_MT_select")

            sub.menu("CLIP_MT_clip")

            if clip:
                sub.menu("CLIP_MT_track")
                sub.menu("CLIP_MT_reconstruction")

        if clip:
            layout.prop(sc, "mode", text="")
            layout.prop(sc, "view", text="", expand=True)

            if sc.view == 'GRAPH':
                row = layout.row(align=True)

                if sc.show_filters:
                    row.prop(sc, "show_filters", icon='DISCLOSURE_TRI_DOWN',
                        text="Filters")

                    sub = row.column()
                    sub.active = clip.tracking.reconstruction.is_valid
                    sub.prop(sc, "show_graph_frames", icon='SEQUENCE', text="")

                    row.prop(sc, "show_graph_tracks", icon='ANIM', text="")
                else:
                    row.prop(sc, "show_filters", icon='DISCLOSURE_TRI_RIGHT',
                        text="Filters")

        row = layout.row()
        row.template_ID(sc, "clip", open='clip.open')

        if clip:
            tracking = clip.tracking
            active = tracking.objects.active

            if active and not active.is_camera:
                r = active.reconstruction
            else:
                r = tracking.reconstruction

            if r.is_valid:
                layout.label(text="Average solve error: %.4f" %
                    (r.average_error))

        layout.template_running_jobs()


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
        sc = context.space_data
        clip = sc.clip
        settings = clip.tracking.settings
        layout = self.layout

        col = layout.column(align=True)
        col.operator("clip.add_marker_move")
        col.operator("clip.detect_features")
        col.operator("clip.delete_track")

        box = layout.box()
        row = box.row(align=True)
        row.prop(settings, "show_default_expanded", text="", emboss=False)
        row.label(text="Tracking Settings")

        if settings.show_default_expanded:
            col = box.column()
            row = col.row(align=True)
            label = bpy.types.CLIP_MT_tracking_settings_presets.bl_label
            row.menu('CLIP_MT_tracking_settings_presets', text=label)
            row.operator("clip.tracking_settings_preset_add",
                         text="", icon='ZOOMIN')
            props = row.operator("clip.tracking_settings_preset_add",
                                 text="", icon='ZOOMOUT')
            props.remove_active = True

            col.separator()

            sub = col.column(align=True)
            sub.prop(settings, "default_pattern_size")
            sub.prop(settings, "default_search_size")

            col.label(text="Tracker:")
            col.prop(settings, "default_tracker", text="")

            if settings.default_tracker == 'KLT':
                col.prop(settings, "default_pyramid_levels")
            col.prop(settings, "default_correlation_min")

            col.separator()

            sub = col.column(align=True)
            sub.prop(settings, "default_frames_limit")
            sub.prop(settings, "default_margin")

            col.label(text="Match:")
            col.prop(settings, "default_pattern_match", text="")


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
        # clip = context.space_data.clip  # UNUSED

        row = layout.row(align=True)

        props = row.operator("clip.track_markers", text="", icon='FRAME_PREV')
        props.backwards = True
        props = row.operator("clip.track_markers", text="",
             icon='PLAY_REVERSE')
        props.backwards = True
        props.sequence = True
        props = row.operator("clip.track_markers", text="", icon='PLAY')
        props.sequence = True
        row.operator("clip.track_markers", text="", icon='FRAME_NEXT')

        col = layout.column(align=True)
        props = col.operator("clip.clear_track_path", text="Clear After")
        props.action = 'REMAINED'

        props = col.operator("clip.clear_track_path", text="Clear Before")
        props.action = 'UPTO'

        props = col.operator("clip.clear_track_path", text="Clear")
        props.action = 'ALL'

        layout.operator("clip.join_tracks", text="Join")


class CLIP_PT_tools_solve(Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'TOOLS'
    bl_label = "Solve"

    @classmethod
    def poll(cls, context):
        sc = context.space_data
        clip = sc.clip

        return clip and sc.mode == 'TRACKING'

    def draw(self, context):
        layout = self.layout
        clip = context.space_data.clip
        tracking = clip.tracking
        settings = tracking.settings
        tracking_object = tracking.objects.active

        col = layout.column(align=True)

        col.operator("clip.solve_camera",
                     text="Camera Motion" if tracking_object.is_camera
                     else "Object Motion")
        col.operator("clip.clear_solution")

        col = layout.column(align=True)
        col.prop(settings, "keyframe_a")
        col.prop(settings, "keyframe_b")

        col = layout.column(align=True)
        col.active = tracking_object.is_camera
        col.label(text="Refine:")
        col.prop(settings, "refine_intrinsics", text="")


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

        layout.operator("clip.clean_tracks")

        layout.prop(settings, 'clean_frames', text="Frames")
        layout.prop(settings, 'clean_error', text="Error")
        layout.prop(settings, 'clean_action', text="")


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
        col.operator("clip.set_floor")
        col.operator("clip.set_origin")

        row = col.row()
        row.operator("clip.set_axis", text="Set X Axis").axis = 'X'
        row.operator("clip.set_axis", text="Set Y Axis").axis = 'Y'

        layout.separator()

        col = layout.column()
        col.operator("clip.set_scale")
        col.prop(settings, "distance")


class CLIP_PT_tools_object(Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'TOOLS'
    bl_label = "Object"

    @classmethod
    def poll(cls, context):
        sc = context.space_data
        clip = sc.clip

        if clip and sc.mode == 'RECONSTRUCTION':
            tracking_object = clip.tracking.objects.active
            return not tracking_object.is_camera

        return False

    def draw(self, context):
        layout = self.layout

        sc = context.space_data
        clip = sc.clip
        tracking_object = clip.tracking.objects.active
        settings = sc.clip.tracking.settings

        col = layout.column()

        col.prop(tracking_object, "scale")

        col.separator()

        col.operator("clip.set_solution_scale", text="Set Scale")
        col.prop(settings, "object_distance")


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


class CLIP_PT_objects(Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'UI'
    bl_label = "Objects"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        sc = context.space_data

        return sc.clip

    def draw(self, context):
        layout = self.layout
        
        sc = context.space_data
        tracking = sc.clip.tracking

        row = layout.row()
        row.template_list(tracking, "objects",
                          tracking, "active_object_index", rows=3)

        sub = row.column(align=True)

        sub.operator("clip.tracking_object_new", icon='ZOOMIN', text="")
        sub.operator("clip.tracking_object_remove", icon='ZOOMOUT', text="")

        active = tracking.objects.active
        if active:
            layout.prop(active, "name")


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
        act_track = clip.tracking.tracks.active

        if not act_track:
            layout.active = False
            layout.label(text="No active track")
            return

        row = layout.row()
        row.prop(act_track, "name", text="")

        sub = row.row(align=True)

        sub.template_marker(sc, "clip", sc.clip_user, act_track, True)

        icon = 'LOCKED' if act_track.lock else 'UNLOCKED'
        sub.prop(act_track, "lock", text="", icon=icon)

        layout.template_track(sc, "scopes")

        row = layout.row(align=True)
        sub = row.row()
        sub.prop(act_track, "use_red_channel", text="R", toggle=True)
        sub.prop(act_track, "use_green_channel", text="G", toggle=True)
        sub.prop(act_track, "use_blue_channel", text="B", toggle=True)

        row.separator()

        sub = row.row()
        sub.prop(act_track, "use_grayscale_preview", text="B/W", toggle=True)

        layout.separator()

        row = layout.row(align=True)
        label = bpy.types.CLIP_MT_track_color_presets.bl_label
        row.menu('CLIP_MT_track_color_presets', text=label)
        row.menu('CLIP_MT_track_color_specials', text="", icon='DOWNARROW_HLT')
        row.operator("clip.track_color_preset_add", text="", icon='ZOOMIN')
        props = row.operator("clip.track_color_preset_add",
                             text="", icon='ZOOMOUT')
        props.remove_active = True

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

        return sc.mode in {'TRACKING', 'DISTORTION'} and sc.clip

    def draw(self, context):
        layout = self.layout

        sc = context.space_data
        clip = sc.clip

        row = layout.row(align=True)
        label = bpy.types.CLIP_MT_camera_presets.bl_label
        row.menu('CLIP_MT_camera_presets', text=label)
        row.operator("clip.camera_preset_add", text="", icon='ZOOMIN')
        props = row.operator("clip.camera_preset_add", text="", icon='ZOOMOUT')
        props.remove_active = True

        row = layout.row(align=True)
        sub = row.split(percentage=0.65)
        if clip.tracking.camera.units == 'MILLIMETERS':
            sub.prop(clip.tracking.camera, "focal_length")
        else:
            sub.prop(clip.tracking.camera, "focal_length_pixels")
        sub.prop(clip.tracking.camera, "units", text="")

        col = layout.column(align=True)
        col.label(text="Sensor:")
        col.prop(clip.tracking.camera, "sensor_width", text="Width")
        col.prop(clip.tracking.camera, "pixel_aspect")

        col = layout.column()
        col.label(text="Optical Center:")
        row = col.row()
        row.prop(clip.tracking.camera, "principal", text="")
        col.operator("clip.set_center_principal", text="Center")

        col = layout.column(align=True)
        col.label(text="Lens Distortion:")
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

        col = layout.column(align=True)

        col.prop(sc, "show_marker_pattern", text="Pattern")
        col.prop(sc, "show_marker_search", text="Search")
        col.prop(sc, "show_pyramid_levels", text="Pyramid")

        col.prop(sc, "show_track_path", text="Path")
        row = col.row()
        row.active = sc.show_track_path
        row.prop(sc, "path_length", text="Length")

        col.prop(sc, "show_disabled", "Disabled Tracks")
        col.prop(sc, "show_bundles", text="3D Markers")

        col.prop(sc, "show_names", text="Names and Status")
        col.prop(sc, "show_tiny_markers", text="Compact Markers")

        col.prop(sc, "show_grease_pencil", text="Grease Pencil")
        col.prop(sc, "use_mute_footage", text="Mute")

        if sc.mode == 'DISTORTION':
            col.prop(sc, "show_grid", text="Grid")
            col.prop(sc, "use_manual_calibration")
        elif sc.mode == 'RECONSTRUCTION':
            col.prop(sc, "show_stable", text="Stable")

        col.prop(sc, "lock_selection")

        clip = sc.clip
        if clip:
            col.label(text="Display Aspect Ratio:")
            col.prop(clip, "display_aspect", text="")


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

        col = layout.column()

        active = clip.tracking.tracks.active
        if active:
            col.prop(active, "tracker")

            if active.tracker == 'KLT':
                col.prop(active, "pyramid_levels")
            col.prop(active, "correlation_min")

            col.separator()
            col.prop(active, "frames_limit")
            col.prop(active, "margin")
            col.prop(active, "pattern_match", text="Match")

        col.prop(settings, "speed")


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
        stab = context.space_data.clip.tracking.stabilization

        self.layout.prop(stab, "use_2d_stabilization", text="")

    def draw(self, context):
        layout = self.layout

        tracking = context.space_data.clip.tracking
        stab = tracking.stabilization

        layout.active = stab.use_2d_stabilization

        row = layout.row()
        row.template_list(stab, "tracks", stab, "active_track_index", rows=3)

        sub = row.column(align=True)

        sub.operator("clip.stabilize_2d_add", icon='ZOOMIN', text="")
        sub.operator("clip.stabilize_2d_remove", icon='ZOOMOUT', text="")

        sub.menu('CLIP_MT_stabilize_2d_specials', text="",
                 icon='DOWNARROW_HLT')

        layout.prop(stab, "influence_location")

        layout.prop(stab, "use_autoscale")
        col = layout.column()
        col.active = stab.use_autoscale
        col.prop(stab, "scale_max")
        col.prop(stab, "influence_scale")

        layout.prop(stab, "use_stabilize_rotation")
        col = layout.column()
        col.active = stab.use_stabilize_rotation

        row = col.row(align=True)
        row.prop_search(stab, "rotation_track", tracking, "tracks", text="")
        row.operator("clip.stabilize_2d_set_rotation", text="", icon='ZOOMIN')

        row = col.row()
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
        act_track = clip.tracking.tracks.active

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

    @classmethod
    def poll(cls, context):
        sc = context.space_data

        return sc.clip

    def draw(self, context):
        layout = self.layout

        layout.operator("clip.set_viewport_background")
        layout.operator("clip.setup_tracking_scene")


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

        ratios = ((1, 8), (1, 4), (1, 2), (1, 1), (2, 1), (4, 1), (8, 1))

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

        layout.operator("clip.open")

        if clip:
            layout.operator("clip.reload")
            layout.menu("CLIP_MT_proxy")


class CLIP_MT_proxy(Menu):
    bl_label = "Proxy"

    def draw(self, context):
        layout = self.layout

        layout.operator("clip.rebuild_proxy")
        layout.operator("clip.delete_proxy")


class CLIP_MT_track(Menu):
    bl_label = "Track"

    def draw(self, context):
        layout = self.layout

        layout.operator("clip.clear_solution")
        layout.operator("clip.solve_camera")

        layout.separator()
        props = layout.operator("clip.clear_track_path", text="Clear After")
        props.action = 'REMAINED'

        props = layout.operator("clip.clear_track_path", text="Clear Before")
        props.action = 'UPTO'

        props = layout.operator("clip.clear_track_path",
            text="Clear Track Path")
        props.action = 'ALL'

        layout.separator()
        layout.operator("clip.join_tracks")

        layout.separator()
        layout.operator("clip.clean_tracks")

        layout.separator()
        layout.operator("clip.copy_tracks")
        layout.operator("clip.paste_tracks")

        layout.separator()
        props = layout.operator("clip.track_markers",
            text="Track Frame Backwards")
        props.backwards = True

        props = layout.operator("clip.track_markers", text="Track Backwards")
        props.backwards = True
        props.sequence = True

        props = layout.operator("clip.track_markers", text="Track Forwards")
        props.sequence = True
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

        layout.operator("clip.set_axis", text="Set X Axis").axis = "X"
        layout.operator("clip.set_axis", text="Set Y Axis").axis = "Y"

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

        props = layout.operator("clip.hide_tracks", text="Hide Unselected")
        props.unselected = True


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

        layout.operator("clip.select_border")
        layout.operator("clip.select_circle")

        layout.separator()

        layout.operator("clip.select_all", text="Select/Deselect all")
        layout.operator("clip.select_all", text="Inverse").action = 'INVERT'

        layout.menu("CLIP_MT_select_grouped")


class CLIP_MT_select_grouped(Menu):
    bl_label = "Select Grouped"

    def draw(self, context):
        layout = self.layout

        layout.operator_enum("clip.select_grouped", "group")


class CLIP_MT_tracking_specials(Menu):
    bl_label = "Specials"

    @classmethod
    def poll(cls, context):
        return context.space_data.clip

    def draw(self, context):
        layout = self.layout

        props = layout.operator("clip.disable_markers", text="Enable Markers")
        props.action = 'ENABLE'

        props = layout.operator("clip.disable_markers", text="Disable markers")
        props.action = 'DISABLE'

        layout.separator()
        layout.operator("clip.set_origin")

        layout.separator()
        layout.operator("clip.hide_tracks")
        layout.operator("clip.hide_tracks_clear", text="Show Tracks")

        layout.separator()
        props = layout.operator("clip.lock_tracks", text="Lock Tracks")
        props.action = 'LOCK'

        props = layout.operator("clip.lock_tracks", text="Unlock Tracks")
        props.action = 'UNLOCK'


class CLIP_MT_camera_presets(Menu):
    """Predefined tracking camera intrinsics"""
    bl_label = "Camera Presets"
    preset_subdir = "tracking_camera"
    preset_operator = "script.execute_preset"
    draw = bpy.types.Menu.draw_preset


class CLIP_MT_track_color_presets(Menu):
    """Predefined track color"""
    bl_label = "Color Presets"
    preset_subdir = "tracking_track_color"
    preset_operator = "script.execute_preset"
    draw = bpy.types.Menu.draw_preset


class CLIP_MT_tracking_settings_presets(Menu):
    """Predefined tracking settings"""
    bl_label = "Tracking Presets"
    preset_subdir = "tracking_settings"
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
