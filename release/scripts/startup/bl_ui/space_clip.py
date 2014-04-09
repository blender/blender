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
from bpy.types import Panel, Header, Menu, UIList
from bpy.app.translations import pgettext_iface as iface_
from bl_ui.properties_grease_pencil_common import GreasePencilPanel


class CLIP_UL_tracking_objects(UIList):
    def draw_item(self, context, layout, data, item, icon,
                  active_data, active_propname, index):
        # assert(isinstance(item, bpy.types.MovieTrackingObject)
        tobj = item
        if self.layout_type in {'DEFAULT', 'COMPACT'}:
            layout.prop(tobj, "name", text="", emboss=False,
                        icon='CAMERA_DATA' if tobj.is_camera
                        else 'OBJECT_DATA')
        elif self.layout_type in {'GRID'}:
            layout.alignment = 'CENTER'
            layout.label(text="",
                         icon='CAMERA_DATA' if tobj.is_camera
                         else 'OBJECT_DATA')


class CLIP_HT_header(Header):
    bl_space_type = 'CLIP_EDITOR'

    def _draw_tracking(self, context):
        layout = self.layout

        sc = context.space_data
        clip = sc.clip

        row = layout.row(align=True)
        row.template_header()

        CLIP_MT_tracking_editor_menus.draw_collapsible(context, layout)

        row = layout.row()
        row.template_ID(sc, "clip", open="clip.open")

        if clip:
            tracking = clip.tracking
            active_object = tracking.objects.active

            if sc.view == 'CLIP':
                layout.prop(sc, "mode", text="")
                layout.prop(sc, "view", text="", expand=True)
                layout.prop(sc, "pivot_point", text="", icon_only=True)

                r = active_object.reconstruction

                if r.is_valid and sc.view == 'CLIP':
                    layout.label(text="Solve error: %.4f" %
                                 (r.average_error))
            elif sc.view == 'GRAPH':
                layout.prop(sc, "view", text="", expand=True)

                row = layout.row(align=True)
                row.prop(sc, "show_graph_only_selected", text="")
                row.prop(sc, "show_graph_hidden", text="")

                row = layout.row(align=True)

                if sc.show_filters:
                    row.prop(sc, "show_filters", icon='DISCLOSURE_TRI_DOWN',
                             text="Filters")

                    sub = row.row(align=True)
                    sub.active = clip.tracking.reconstruction.is_valid
                    sub.prop(sc, "show_graph_frames", icon='SEQUENCE', text="")

                    row.prop(sc, "show_graph_tracks_motion", icon='IPO', text="")
                    row.prop(sc, "show_graph_tracks_error", icon='ANIM', text="")
                else:
                    row.prop(sc, "show_filters", icon='DISCLOSURE_TRI_RIGHT',
                             text="Filters")
            elif sc.view == 'DOPESHEET':
                dopesheet = tracking.dopesheet
                layout.prop(sc, "view", text="", expand=True)

                row = layout.row(align=True)
                row.prop(dopesheet, "show_only_selected", text="")
                row.prop(dopesheet, "show_hidden", text="")

                row = layout.row(align=True)
                row.prop(dopesheet, "sort_method", text="")
                row.prop(dopesheet, "use_invert_sort",
                         text="Invert", toggle=True)
        else:
            layout.prop(sc, "view", text="", expand=True)

    def _draw_masking(self, context):
        layout = self.layout

        toolsettings = context.tool_settings
        sc = context.space_data
        clip = sc.clip

        row = layout.row(align=True)
        row.template_header()

        CLIP_MT_masking_editor_menus.draw_collapsible(context, layout)

        row = layout.row()
        row.template_ID(sc, "clip", open="clip.open")

        if clip:
            layout.prop(sc, "mode", text="")

            row = layout.row()
            row.template_ID(sc, "mask", new="mask.new")

            layout.prop(sc, "pivot_point", text="", icon_only=True)

            row = layout.row(align=True)
            row.prop(toolsettings, "use_proportional_edit_mask",
                     text="", icon_only=True)
            if toolsettings.use_proportional_edit_mask:
                row.prop(toolsettings, "proportional_edit_falloff",
                         text="", icon_only=True)

    def draw(self, context):
        layout = self.layout

        sc = context.space_data

        if sc.mode in {'TRACKING'}:
            self._draw_tracking(context)
        else:
            self._draw_masking(context)

        layout.template_running_jobs()


class CLIP_MT_tracking_editor_menus(Menu):
    bl_idname = "CLIP_MT_tracking_editor_menus"
    bl_label = ""

    def draw(self, context):
        self.draw_menus(self.layout, context)

    @staticmethod
    def draw_menus(layout, context):
        sc = context.space_data
        clip = sc.clip

        layout.menu("CLIP_MT_view")

        if sc.view == 'CLIP':
            if clip:
                layout.menu("CLIP_MT_select")
                layout.menu("CLIP_MT_clip")
                layout.menu("CLIP_MT_track")
                layout.menu("CLIP_MT_reconstruction")
            else:
                layout.menu("CLIP_MT_clip")


class CLIP_MT_masking_editor_menus(Menu):

    bl_idname = "CLIP_MT_masking_editor_menus"
    bl_label = ""

    def draw(self, context):
        self.draw_menus(self.layout, context)

    @staticmethod
    def draw_menus(layout, context):
        sc = context.space_data
        clip = sc.clip

        layout.menu("CLIP_MT_view")

        if clip:
            layout.menu("MASK_MT_select")
            layout.menu("CLIP_MT_clip")  # XXX - remove?
            layout.menu("MASK_MT_mask")
        else:
            layout.menu("CLIP_MT_clip")  # XXX - remove?


class CLIP_PT_clip_view_panel:

    @classmethod
    def poll(cls, context):
        sc = context.space_data
        clip = sc.clip

        return clip and sc.view == 'CLIP'


class CLIP_PT_tracking_panel:

    @classmethod
    def poll(cls, context):
        sc = context.space_data
        clip = sc.clip

        return clip and sc.mode == 'TRACKING' and sc.view == 'CLIP'


class CLIP_PT_reconstruction_panel:

    @classmethod
    def poll(cls, context):
        sc = context.space_data
        clip = sc.clip

        return clip and sc.view == 'CLIP'


class CLIP_PT_tools_clip(Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'TOOLS'
    bl_label = "Clip"
    bl_translation_context = bpy.app.translations.contexts.id_movieclip
    bl_category = "Track"

    @classmethod
    def poll(cls, context):
        sc = context.space_data
        clip = sc.clip

        return clip and sc.view == 'CLIP' and sc.mode != 'MASK'

    def draw(self, context):
        layout = self.layout
        col = layout.column(align=True)
        row = col.row(align=True)
        row.operator("clip.prefetch", text="Prefetch")
        row.operator("clip.reload", text="Reload")
        col.operator("clip.set_scene_frames")


class CLIP_PT_tools_marker(CLIP_PT_tracking_panel, Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'TOOLS'
    bl_label = "Marker"
    bl_category = "Track"

    def draw(self, context):
        layout = self.layout

        sc = context.space_data
        clip = sc.clip
        settings = clip.tracking.settings

        col = layout.column(align=True)
        row = col.row(align=True)
        row.operator("clip.add_marker_at_click", text="Add")
        row.operator("clip.delete_track", text="Delete")
        col.operator("clip.detect_features")


class CLIP_PT_tracking_settings(CLIP_PT_tracking_panel, Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'TOOLS'
    bl_label = "Tracking Settings"
    bl_category = "Track"

    def draw(self, context):

        sc = context.space_data
        clip = sc.clip
        settings = clip.tracking.settings
        layout = self.layout
        col = layout.column()

        row = col.row(align=True)
        label = CLIP_MT_tracking_settings_presets.bl_label
        row.menu('CLIP_MT_tracking_settings_presets', text=label)
        row.operator("clip.tracking_settings_preset_add",
                     text="", icon='ZOOMIN')
        row.operator("clip.tracking_settings_preset_add",
                     text="", icon='ZOOMOUT').remove_active = True

        row = col.row(align=True)
        row.prop(settings, "use_default_red_channel",
                 text="R", toggle=True)
        row.prop(settings, "use_default_green_channel",
                 text="G", toggle=True)
        row.prop(settings, "use_default_blue_channel",
                 text="B", toggle=True)

        col.separator()

        sub = col.column(align=True)
        sub.prop(settings, "default_pattern_size")
        sub.prop(settings, "default_search_size")

        col.prop(settings, "default_motion_model")

        row = col.row(align=True)
        row.label(text="Match:")
        row.prop(settings, "default_pattern_match", text="")

        row = col.row(align=True)
        row.prop(settings, "use_default_brute")
        row.prop(settings, "use_default_normalization")

        col.separator()
        col.operator("clip.track_settings_as_default",
                     text="Copy From Active Track")

        box = layout.box()
        row = box.row(align=True)
        row.prop(settings, "show_default_expanded", text="", emboss=False)
        row.label(text="Extra Settings")

        if settings.show_default_expanded:
            col = box.column()
            row = col.row()
            row.prop(settings, "use_default_mask")

            sub = col.column(align=True)
            sub.prop(settings, "default_correlation_min")
            sub.prop(settings, "default_frames_limit")
            sub.prop(settings, "default_margin")

            col = box.column()
            col.prop(settings, "default_weight")


class CLIP_PT_tools_tracking(CLIP_PT_tracking_panel, Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'TOOLS'
    bl_label = "Track"
    bl_category = "Track"

    def draw(self, context):
        layout = self.layout

        row = layout.row(align=True)
        row.label(text="Track:")

        props = row.operator("clip.track_markers", text="", icon='FRAME_PREV')
        props.backwards = True
        props.sequence = False
        props = row.operator("clip.track_markers", text="",
                             icon='PLAY_REVERSE')
        props.backwards = True
        props.sequence = True
        props = row.operator("clip.track_markers", text="", icon='PLAY')
        props.backwards = False
        props.sequence = True
        props = row.operator("clip.track_markers", text="", icon='FRAME_NEXT')
        props.backwards = False
        props.sequence = False

        col = layout.column(align=True)
        row = col.row(align=True)
        row.label(text="Clear:")
        row.scale_x = 2.0

        props = row.operator("clip.clear_track_path", icon="BACK", text="")
        props.action = 'UPTO'

        props = row.operator("clip.clear_track_path", icon="FORWARD", text="")
        props.action = 'REMAINED'

        col = layout.column()
        row = col.row(align=True)
        row.label(text="Refine:")
        row.scale_x = 2.0

        props = row.operator("clip.refine_markers", icon='LOOP_BACK', text="")
        props.backwards = True

        props = row.operator("clip.refine_markers", icon='LOOP_FORWARDS', text="")
        props.backwards = False

        col = layout.column(align=True)
        row = col.row(align=True)
        row.label(text="Merge:")
        row.operator("clip.join_tracks", text="Join Tracks")


class CLIP_PT_tools_plane_tracking(CLIP_PT_tracking_panel, Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'TOOLS'
    bl_label = "Plane Track"
    bl_options = {'DEFAULT_CLOSED'}
    bl_category = "Solve"

    def draw(self, context):
        layout = self.layout
        layout.operator("clip.create_plane_track")


class CLIP_PT_tools_solve(CLIP_PT_tracking_panel, Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'TOOLS'
    bl_label = "Solve"
    bl_category = "Solve"

    def draw(self, context):
        layout = self.layout

        clip = context.space_data.clip
        tracking = clip.tracking
        settings = tracking.settings
        tracking_object = tracking.objects.active

        col = layout.column()
        row = col.row()
        row.prop(settings, "use_tripod_solver", text="Tripod")
        row.prop(settings, "use_keyframe_selection", text="Keyframe")

        col = layout.column(align=True)
        col.active = (not settings.use_tripod_solver and
                      not settings.use_keyframe_selection)
        col.prop(tracking_object, "keyframe_a")
        col.prop(tracking_object, "keyframe_b")

        col = layout.column(align=True)
        col.active = tracking_object.is_camera
        row = col.row(align=True)
        row.label(text="Refine:")
        row.prop(settings, "refine_intrinsics", text="")

        col = layout.column(align=True)
        col.scale_y = 2.0

        col.operator("clip.solve_camera",
                     text="Solve Camera Motion" if tracking_object.is_camera
                     else "Solve Object Motion")


class CLIP_PT_tools_cleanup(CLIP_PT_tracking_panel, Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'TOOLS'
    bl_label = "Clean up"
    bl_options = {'DEFAULT_CLOSED'}
    bl_category = "Solve"

    def draw(self, context):
        layout = self.layout
        clip = context.space_data.clip
        settings = clip.tracking.settings

        layout.operator("clip.clean_tracks")

        layout.prop(settings, "clean_frames", text="Frames")
        layout.prop(settings, "clean_error", text="Error")
        layout.prop(settings, "clean_action", text="")


class CLIP_PT_tools_geometry(CLIP_PT_tracking_panel, Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'TOOLS'
    bl_label = "Geometry"
    bl_options = {'DEFAULT_CLOSED'}
    bl_category = "Solve"

    def draw(self, context):
        layout = self.layout

        layout.operator("clip.bundles_to_mesh")
        layout.operator("clip.track_to_empty")


class CLIP_PT_tools_orientation(CLIP_PT_tracking_panel, Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'TOOLS'
    bl_label = "Orientation"
    bl_category = "Solve"

    def draw(self, context):
        sc = context.space_data
        layout = self.layout
        settings = sc.clip.tracking.settings

        col = layout.column(align=True)
        row = col.row(align=True)
        row.operator("clip.set_plane", text="Floor").plane = 'FLOOR'
        row.operator("clip.set_plane", text="Wall").plane = 'WALL'

        col.operator("clip.set_origin")

        row = col.row(align=True)
        row.operator("clip.set_axis", text="Set X Axis").axis = 'X'
        row.operator("clip.set_axis", text="Set Y Axis").axis = 'Y'

        layout.separator()

        col = layout.column()
        row = col.row(align=True)
        row.operator("clip.set_scale")
        row.operator("clip.apply_solution_scale", text="Apply Scale")

        col.prop(settings, "distance")


class CLIP_PT_tools_object(CLIP_PT_reconstruction_panel, Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'TOOLS'
    bl_label = "Object"

    @classmethod
    def poll(cls, context):
        if CLIP_PT_reconstruction_panel.poll(context):
            sc = context.space_data
            clip = sc.clip

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


class CLIP_PT_objects(CLIP_PT_clip_view_panel, Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'UI'
    bl_label = "Objects"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout

        sc = context.space_data
        tracking = sc.clip.tracking

        row = layout.row()
        row.template_list("CLIP_UL_tracking_objects", "", tracking, "objects",
                          tracking, "active_object_index", rows=1)

        sub = row.column(align=True)

        sub.operator("clip.tracking_object_new", icon='ZOOMIN', text="")
        sub.operator("clip.tracking_object_remove", icon='ZOOMOUT', text="")


class CLIP_PT_track(CLIP_PT_tracking_panel, Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'UI'
    bl_label = "Track"

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
        sub = row.row(align=True)
        sub.prop(act_track, "use_red_channel", text="R", toggle=True)
        sub.prop(act_track, "use_green_channel", text="G", toggle=True)
        sub.prop(act_track, "use_blue_channel", text="B", toggle=True)

        row.separator()

        row.prop(act_track, "use_grayscale_preview", text="B/W", toggle=True)

        row.separator()
        row.prop(act_track, "use_alpha_preview",
                 text="", toggle=True, icon='IMAGE_ALPHA')

        layout.prop(act_track, "weight")

        if act_track.has_bundle:
            label_text = "Average Error: %.4f" % (act_track.average_error)
            layout.label(text=label_text)

        layout.separator()

        row = layout.row(align=True)
        label = bpy.types.CLIP_MT_track_color_presets.bl_label
        row.menu('CLIP_MT_track_color_presets', text=label)
        row.menu('CLIP_MT_track_color_specials', text="", icon='DOWNARROW_HLT')
        row.operator("clip.track_color_preset_add", text="", icon='ZOOMIN')
        row.operator("clip.track_color_preset_add",
                     text="", icon='ZOOMOUT').remove_active = True

        row = layout.row()
        row.prop(act_track, "use_custom_color")
        if act_track.use_custom_color:
            row.prop(act_track, "color", text="")


class CLIP_PT_plane_track(CLIP_PT_tracking_panel, Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'UI'
    bl_label = "Plane Track"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout

        sc = context.space_data
        clip = context.space_data.clip
        active_track = clip.tracking.plane_tracks.active

        if not active_track:
            layout.active = False
            layout.label(text="No active plane track")
            return

        layout.prop(active_track, "name")
        layout.prop(active_track, "use_auto_keying")
        layout.prop(active_track, "image")

        row = layout.row()
        row.active = active_track.image is not None
        row.prop(active_track, "image_opacity", text="Opacity")


class CLIP_PT_track_settings(CLIP_PT_tracking_panel, Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'UI'
    bl_label = "Tracking Settings"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout

        clip = context.space_data.clip
        settings = clip.tracking.settings

        col = layout.column()

        active = clip.tracking.tracks.active
        if active:
            col.prop(active, "motion_model")
            col.prop(active, "pattern_match", text="Match")
            col = layout.column()
            row = col.row(align=True)
            row.prop(active, "use_brute")
            row.prop(active, "use_normalization")

            box = layout.box()
            row = box.row(align=True)
            row.prop(settings, "show_extra_expanded", text="", emboss=False)
            row.label(text="Extra Settings")

            if settings.show_extra_expanded:
                col = box.column()
                row = col.row()
                row.prop(active, "use_mask")

                sub = col.column(align=True)
                sub.prop(active, "correlation_min")
                sub.prop(active, "frames_limit")
                sub.prop(active, "margin")
                sub.separator()
                sub.prop(settings, "speed")


class CLIP_PT_tracking_camera(Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'UI'
    bl_label = "Camera"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        if CLIP_PT_clip_view_panel.poll(context):
            sc = context.space_data

            return sc.mode in {'TRACKING'} and sc.clip

        return False

    def draw(self, context):
        layout = self.layout

        sc = context.space_data
        clip = sc.clip

        row = layout.row(align=True)
        label = bpy.types.CLIP_MT_camera_presets.bl_label
        row.menu('CLIP_MT_camera_presets', text=label)
        row.operator("clip.camera_preset_add", text="", icon='ZOOMIN')
        row.operator("clip.camera_preset_add", text="",
                     icon='ZOOMOUT').remove_active = True

        col = layout.column(align=True)
        col.label(text="Sensor:")
        col.prop(clip.tracking.camera, "sensor_width", text="Width")
        col.prop(clip.tracking.camera, "pixel_aspect")

        col = layout.column()
        col.label(text="Optical Center:")
        row = col.row()
        row.prop(clip.tracking.camera, "principal", text="")
        col.operator("clip.set_center_principal", text="Center")


class CLIP_PT_tracking_lens(Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'UI'
    bl_label = "Lens"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        if CLIP_PT_clip_view_panel.poll(context):
            sc = context.space_data

            return sc.mode in {'TRACKING'} and sc.clip

        return False

    def draw(self, context):
        layout = self.layout

        sc = context.space_data
        clip = sc.clip

        row = layout.row(align=True)
        sub = row.split(percentage=0.65, align=True)
        if clip.tracking.camera.units == 'MILLIMETERS':
            sub.prop(clip.tracking.camera, "focal_length")
        else:
            sub.prop(clip.tracking.camera, "focal_length_pixels")
        sub.prop(clip.tracking.camera, "units", text="")

        col = layout.column(align=True)
        col.label(text="Lens Distortion:")
        col.prop(clip.tracking.camera, "k1")
        col.prop(clip.tracking.camera, "k2")
        col.prop(clip.tracking.camera, "k3")


class CLIP_PT_display(CLIP_PT_clip_view_panel, Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'UI'
    bl_label = "Display"

    def draw(self, context):
        layout = self.layout
        sc = context.space_data

        row = layout.row(align=True)

        sub = row.row(align=True)
        sub.prop(sc, "show_red_channel", text="R", toggle=True)
        sub.prop(sc, "show_green_channel", text="G", toggle=True)
        sub.prop(sc, "show_blue_channel", text="B", toggle=True)
        row.separator()
        row.prop(sc, "use_grayscale_preview", text="B/W", toggle=True)
        row.separator()
        row.prop(sc, "use_mute_footage", text="", icon="VISIBLE_IPO_ON", toggle=True)

        col = layout.column(align=True)
        col.prop(sc.clip_user, "use_render_undistorted", text="Render Undistorted")
        col.prop(sc, "lock_selection", text="Lock to Selection")
        col.prop(sc, "show_stable", text="Display Stabilization")
        if sc.view == 'GRAPH':
            col.prop(sc, "lock_time_cursor")
        row = col.row(align=True)
        row.prop(sc, "show_grid", text="Grid")
        row.prop(sc, "use_manual_calibration", text="Calibration")

        clip = sc.clip
        if clip:
            col.label(text="Display Aspect Ratio:")
            row = col.row()
            row.prop(clip, "display_aspect", text="")


class CLIP_PT_marker_display(CLIP_PT_clip_view_panel, Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'UI'
    bl_label = "Marker Display"

    @classmethod
    def poll(cls, context):
        sc = context.space_data

        return sc.mode != 'MASK'

    def draw(self, context):
        layout = self.layout
        sc = context.space_data

        col = layout.column(align=True)
        row = col.row(align=True)
        row.prop(sc, "show_marker_pattern", text="Pattern")
        row.prop(sc, "show_marker_search", text="Search")

        row = col.row(align=True)
        row.active = sc.show_track_path
        row.prop(sc, "show_track_path", text="Path")
        row.prop(sc, "path_length", text="Length")

        col = layout.column(align=True)
        row = col.row(align=True)
        row.prop(sc, "show_disabled", "Disabled")
        row.prop(sc, "show_names", text="Info")

        row = col.row(align=True)
        if sc.mode != 'MASK':
            row.prop(sc, "show_bundles", text="3D Markers")
        row.prop(sc, "show_tiny_markers", text="Thin")


class CLIP_PT_marker(CLIP_PT_tracking_panel, Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'UI'
    bl_label = "Marker"
    bl_options = {'DEFAULT_CLOSED'}

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


class CLIP_PT_stabilization(CLIP_PT_reconstruction_panel, Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'UI'
    bl_label = "2D Stabilization"
    bl_category = "Stabilization"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        if CLIP_PT_clip_view_panel.poll(context):
            sc = context.space_data

            return sc.mode in {'TRACKING'} and sc.clip

        return False

    def draw_header(self, context):
        stab = context.space_data.clip.tracking.stabilization

        self.layout.prop(stab, "use_2d_stabilization", text="")

    def draw(self, context):
        layout = self.layout

        tracking = context.space_data.clip.tracking
        stab = tracking.stabilization

        layout.active = stab.use_2d_stabilization

        row = layout.row()
        row.template_list("UI_UL_list", "stabilization_tracks", stab, "tracks",
                          stab, "active_track_index", rows=2)

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

        layout.prop(stab, "filter_type")


class CLIP_PT_proxy(CLIP_PT_clip_view_panel, Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'UI'
    bl_label = "Proxy / Timecode"
    bl_options = {'DEFAULT_CLOSED'}

    def draw_header(self, context):
        sc = context.space_data

        self.layout.prop(sc.clip, "use_proxy", text="")

    def draw(self, context):
        layout = self.layout

        sc = context.space_data
        clip = sc.clip

        col = layout.column()
        col.active = clip.use_proxy

        col.label(text="Build Original:")

        row = col.row(align=True)
        row.prop(clip.proxy, "build_25", toggle=True)
        row.prop(clip.proxy, "build_50", toggle=True)
        row.prop(clip.proxy, "build_75", toggle=True)
        row.prop(clip.proxy, "build_100", toggle=True)

        col.label(text="Build Undistorted:")

        row = col.row(align=True)
        row.prop(clip.proxy, "build_undistorted_25", toggle=True)
        row.prop(clip.proxy, "build_undistorted_50", toggle=True)
        row.prop(clip.proxy, "build_undistorted_75", toggle=True)
        row.prop(clip.proxy, "build_undistorted_100", toggle=True)

        col.prop(clip.proxy, "quality")

        col.prop(clip, "use_proxy_custom_directory")
        if clip.use_proxy_custom_directory:
            col.prop(clip.proxy, "directory")

        col.operator("clip.rebuild_proxy", text="Build Proxy")

        if clip.source == 'MOVIE':
            col2 = col.column()

            col2.label(text="Use timecode index:")
            col2.prop(clip.proxy, "timecode", text="")

        col2 = col.column()
        col2.label(text="Proxy render size:")

        col.prop(sc.clip_user, "proxy_render_size", text="")


# -----------------------------------------------------------------------------
# Mask (similar code in space_image.py, keep in sync)


from bl_ui.properties_mask_common import (MASK_PT_mask,
                                          MASK_PT_layers,
                                          MASK_PT_spline,
                                          MASK_PT_point,
                                          MASK_PT_display,
                                          MASK_PT_tools,
                                          MASK_PT_transforms,
                                          MASK_PT_add)


class CLIP_PT_mask_layers(MASK_PT_layers, Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'UI'


class CLIP_PT_mask_display(MASK_PT_display, Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'UI'


class CLIP_PT_active_mask_spline(MASK_PT_spline, Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'UI'


class CLIP_PT_active_mask_point(MASK_PT_point, Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'UI'


class CLIP_PT_mask(MASK_PT_mask, Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'UI'


class CLIP_PT_tools_mask_add(MASK_PT_add, Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'TOOLS'


class CLIP_PT_tools_mask_transforms(MASK_PT_transforms, Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'TOOLS'


class CLIP_PT_tools_mask(MASK_PT_tools, Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'TOOLS'

# --- end mask ---


class CLIP_PT_tools_grease_pencil(GreasePencilPanel, Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'TOOLS'
    bl_category = "Grease Pencil"


class CLIP_PT_footage(CLIP_PT_clip_view_panel, Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'UI'
    bl_label = "Footage Settings"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout

        sc = context.space_data
        clip = sc.clip

        col = layout.column()
        col.template_movieclip(sc, "clip", compact=True)
        col.prop(clip, "frame_start")
        col.prop(clip, "frame_offset")


class CLIP_PT_footage_info(CLIP_PT_clip_view_panel, Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'UI'
    bl_label = "Footage Information"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout

        sc = context.space_data
        clip = sc.clip

        col = layout.column()
        col.template_movieclip_information(sc, "clip", sc.clip_user)


class CLIP_PT_tools_scenesetup(Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'TOOLS'
    bl_label = "Scene Setup"
    bl_translation_context = bpy.app.translations.contexts.id_movieclip
    bl_category = "Solve"

    @classmethod
    def poll(cls, context):
        sc = context.space_data
        clip = sc.clip

        return clip and sc.view == 'CLIP' and sc.mode != 'MASK'

    def draw(self, context):
        layout = self.layout

        layout.operator("clip.set_viewport_background")
        layout.operator("clip.setup_tracking_scene")


class CLIP_MT_view(Menu):
    bl_label = "View"

    def draw(self, context):
        layout = self.layout

        sc = context.space_data

        if sc.view == 'CLIP':
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

            text = iface_("Zoom %d:%d")
            for a, b in ratios:
                layout.operator("clip.view_zoom_ratio",
                                text=text % (a, b),
                                translate=False).ratio = a / b
        else:
            if sc.view == 'GRAPH':
                layout.operator_context = 'INVOKE_REGION_PREVIEW'
                layout.operator("clip.graph_center_current_frame")
                layout.operator("clip.graph_view_all")
                layout.operator_context = 'INVOKE_DEFAULT'

            layout.prop(sc, "show_seconds")
            layout.separator()

        layout.separator()
        layout.operator("screen.area_dupli")
        layout.operator("screen.screen_full_area")


class CLIP_MT_clip(Menu):
    bl_label = "Clip"
    bl_translation_context = bpy.app.translations.contexts.id_movieclip

    def draw(self, context):
        layout = self.layout

        sc = context.space_data
        clip = sc.clip

        layout.operator("clip.open")

        if clip:
            layout.operator("clip.prefetch")
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
        layout.operator("clip.clear_track_path",
                        text="Clear After").action = 'REMAINED'

        layout.operator("clip.clear_track_path",
                        text="Clear Before").action = 'UPTO'

        layout.operator("clip.clear_track_path",
                        text="Clear Track Path").action = 'ALL'

        layout.separator()
        layout.operator("clip.join_tracks")

        layout.separator()
        layout.operator("clip.clean_tracks")

        layout.separator()
        layout.operator("clip.copy_tracks")
        layout.operator("clip.paste_tracks")

        layout.separator()
        layout.operator("clip.track_markers",
                        text="Track Frame Backwards").backwards = True

        props = layout.operator("clip.track_markers", text="Track Backwards")
        props.backwards = True
        props.sequence = True

        layout.operator("clip.track_markers",
                        text="Track Forwards").sequence = True
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
        layout.operator("clip.set_plane", text="Set Floor").plane = 'FLOOR'
        layout.operator("clip.set_plane", text="Set Wall").plane = 'WALL'

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

        layout.operator("clip.hide_tracks",
                        text="Hide Unselected").unselected = True


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

        layout.operator("clip.select_all"
                        ).action = 'TOGGLE'
        layout.operator("clip.select_all",
                        text="Inverse").action = 'INVERT'

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

        layout.operator("clip.disable_markers",
                        text="Enable Markers").action = 'ENABLE'

        layout.operator("clip.disable_markers",
                        text="Disable markers").action = 'DISABLE'

        layout.separator()
        layout.operator("clip.set_origin")

        layout.separator()
        layout.operator("clip.hide_tracks")
        layout.operator("clip.hide_tracks_clear", text="Show Tracks")

        layout.separator()
        layout.operator("clip.lock_tracks", text="Lock Tracks").action = 'LOCK'

        layout.operator("clip.lock_tracks",
                        text="Unlock Tracks").action = 'UNLOCK'


class CLIP_MT_camera_presets(Menu):
    """Predefined tracking camera intrinsics"""
    bl_label = "Camera Presets"
    preset_subdir = "tracking_camera"
    preset_operator = "script.execute_preset"
    draw = Menu.draw_preset


class CLIP_MT_track_color_presets(Menu):
    """Predefined track color"""
    bl_label = "Color Presets"
    preset_subdir = "tracking_track_color"
    preset_operator = "script.execute_preset"
    draw = Menu.draw_preset


class CLIP_MT_tracking_settings_presets(Menu):
    """Predefined tracking settings"""
    bl_label = "Tracking Presets"
    preset_subdir = "tracking_settings"
    preset_operator = "script.execute_preset"
    draw = Menu.draw_preset


class CLIP_MT_track_color_specials(Menu):
    bl_label = "Track Color Specials"

    def draw(self, context):
        layout = self.layout

        layout.operator("clip.track_copy_color", icon='COPY_ID')


class CLIP_MT_stabilize_2d_specials(Menu):
    bl_label = "Track Color Specials"

    def draw(self, context):
        layout = self.layout

        layout.operator("clip.stabilize_2d_select")


if __name__ == "__main__":  # only for live edit.
    bpy.utils.register_module(__name__)
