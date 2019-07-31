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
from bl_ui.utils import PresetPanel
from bl_ui.properties_grease_pencil_common import (
    AnnotationDrawingToolsPanel,
    AnnotationDataPanel,
)


class CLIP_UL_tracking_objects(UIList):
    def draw_item(self, _context, layout, _data, item, _icon,
                  _active_data, _active_propname, _index):
        # assert(isinstance(item, bpy.types.MovieTrackingObject)
        tobj = item
        if self.layout_type in {'DEFAULT', 'COMPACT'}:
            layout.prop(tobj, "name", text="", emboss=False,
                        icon='CAMERA_DATA' if tobj.is_camera
                        else 'OBJECT_DATA')
        elif self.layout_type == 'GRID':
            layout.alignment = 'CENTER'
            layout.label(text="",
                         icon='CAMERA_DATA' if tobj.is_camera
                         else 'OBJECT_DATA')


class CLIP_PT_display(Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'HEADER'
    bl_label = "Clip Display"
    bl_ui_units_x = 13

    def draw(self, context):
        pass


class CLIP_PT_marker_display(Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'HEADER'
    bl_label = "Marker Display"
    bl_parent_id = 'CLIP_PT_display'
    bl_ui_units_x = 13

    def draw(self, context):
        layout = self.layout

        view = context.space_data

        row = layout.row()

        col = row.column()
        col.prop(view, "show_marker_pattern", text="Pattern")
        col.prop(view, "show_marker_search", text="Search")

        col.active = view.show_track_path
        col.prop(view, "show_track_path", text="Path")
        col.prop(view, "path_length", text="Length")

        col = row.column()
        col.prop(view, "show_disabled", text="Show Disabled")
        col.prop(view, "show_names", text="Info")

        if view.mode != 'MASK':
            col.prop(view, "show_bundles", text="3D Markers")
        col.prop(view, "show_tiny_markers", text="Display Thin")


class CLIP_PT_clip_display(Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'HEADER'
    bl_label = "Clip Display"
    bl_parent_id = 'CLIP_PT_display'
    bl_ui_units_x = 13

    def draw(self, context):
        layout = self.layout

        sc = context.space_data

        col = layout.column(align=True)

        row = layout.row(align=True)
        row.prop(sc, "show_red_channel", text="R", toggle=True)
        row.prop(sc, "show_green_channel", text="G", toggle=True)
        row.prop(sc, "show_blue_channel", text="B", toggle=True)
        row.separator()
        row.prop(sc, "use_grayscale_preview", text="B/W", toggle=True)
        row.separator()
        row.prop(sc, "use_mute_footage", text="", icon='HIDE_OFF', toggle=True)

        layout.separator()

        row = layout.row()
        col = row.column()
        col.prop(sc.clip_user, "use_render_undistorted", text="Render Undistorted")
        col.prop(sc, "lock_selection", text="Lock to Selection")
        col = row.column()
        col.prop(sc, "show_stable", text="Show Stable")
        col.prop(sc, "show_grid", text="Grid")
        col.prop(sc, "use_manual_calibration", text="Calibration")

        clip = sc.clip
        if clip:
            col = layout.column()
            col.prop(clip, "display_aspect", text="Display Aspect Ratio")


class CLIP_HT_header(Header):
    bl_space_type = 'CLIP_EDITOR'

    def _draw_tracking(self, context):
        layout = self.layout

        sc = context.space_data
        clip = sc.clip

        CLIP_MT_tracking_editor_menus.draw_collapsible(context, layout)

        layout.separator_spacer()

        row = layout.row()
        if sc.view == 'CLIP':
            row.template_ID(sc, "clip", open="clip.open")
        else:
            row = layout.row(align=True)
            props = row.operator("clip.refine_markers", text="", icon='TRACKING_REFINE_BACKWARDS')
            props.backwards = True
            row.separator()

            props = row.operator("clip.clear_track_path", text="", icon='TRACKING_CLEAR_BACKWARDS')
            props.action = 'UPTO'
            row.separator()

            props = row.operator("clip.track_markers", text="", icon='TRACKING_BACKWARDS_SINGLE')
            props.backwards = True
            props.sequence = False
            props = row.operator("clip.track_markers", text="",
                                 icon='TRACKING_BACKWARDS')
            props.backwards = True
            props.sequence = True
            props = row.operator("clip.track_markers", text="", icon='TRACKING_FORWARDS')
            props.backwards = False
            props.sequence = True
            props = row.operator("clip.track_markers", text="", icon='TRACKING_FORWARDS_SINGLE')
            props.backwards = False
            props.sequence = False
            row.separator()

            props = row.operator("clip.clear_track_path", text="", icon='TRACKING_CLEAR_FORWARDS')
            props.action = 'REMAINED'
            row.separator()

            props = row.operator("clip.refine_markers", text="", icon='TRACKING_REFINE_FORWARDS')
            props.backwards = False

        layout.separator_spacer()

        if clip:
            tracking = clip.tracking
            active_object = tracking.objects.active

            if sc.view == 'CLIP':
                layout.template_running_jobs()

                r = active_object.reconstruction

                if r.is_valid and sc.view == 'CLIP':
                    layout.label(text="Solve error: %.4f" %
                                 (r.average_error))

                row = layout.row()
                row.prop(sc, "pivot_point", text="", icon_only=True)
                row = layout.row(align=True)
                icon = 'LOCKED' if sc.lock_selection else 'UNLOCKED'
                row.prop(sc, "lock_selection", icon=icon, text="")
                row.popover(panel='CLIP_PT_display')

            elif sc.view == 'GRAPH':
                row = layout.row(align=True)
                row.prop(sc, "show_graph_only_selected", text="")
                row.prop(sc, "show_graph_hidden", text="")

                row = layout.row(align=True)

                sub = row.row(align=True)
                sub.active = clip.tracking.reconstruction.is_valid
                sub.prop(sc, "show_graph_frames", icon='SEQUENCE', text="")

                row.prop(sc, "show_graph_tracks_motion", icon='GRAPH', text="")
                row.prop(sc, "show_graph_tracks_error", icon='ANIM', text="")

            elif sc.view == 'DOPESHEET':
                dopesheet = tracking.dopesheet

                row = layout.row(align=True)
                row.prop(dopesheet, "show_only_selected", text="")
                row.prop(dopesheet, "show_hidden", text="")

                row = layout.row(align=True)
                row.prop(dopesheet, "sort_method", text="")
                row.prop(dopesheet, "use_invert_sort",
                         text="Invert", toggle=True)

    def _draw_masking(self, context):
        layout = self.layout

        tool_settings = context.tool_settings
        sc = context.space_data
        clip = sc.clip

        CLIP_MT_masking_editor_menus.draw_collapsible(context, layout)

        layout.separator_spacer()

        row = layout.row()
        row.template_ID(sc, "clip", open="clip.open")

        layout.separator_spacer()

        if clip:

            layout.prop(sc, "pivot_point", text="", icon_only=True)

            row = layout.row(align=True)
            row.prop(tool_settings, "use_proportional_edit_mask", text="", icon_only=True)
            sub = row.row(align=True)
            sub.active = tool_settings.use_proportional_edit_mask
            sub.prop(tool_settings, "proportional_edit_falloff", text="", icon_only=True)

            row = layout.row()
            row.template_ID(sc, "mask", new="mask.new")
            row.popover(panel='CLIP_PT_mask_display')
            row = layout.row(align=True)
            icon = 'LOCKED' if sc.lock_selection else 'UNLOCKED'
            row.prop(sc, "lock_selection", icon=icon, text="")
            row.popover(panel='CLIP_PT_display')

    def draw(self, context):
        layout = self.layout

        sc = context.space_data

        layout.template_header()

        layout.prop(sc, "mode", text="")
        if sc.mode == 'TRACKING':
            layout.prop(sc, "view", text="")
            self._draw_tracking(context)
        else:
            self._draw_masking(context)


class CLIP_MT_tracking_editor_menus(Menu):
    bl_idname = "CLIP_MT_tracking_editor_menus"
    bl_label = ""

    def draw(self, context):
        layout = self.layout
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
        layout = self.layout
        sc = context.space_data
        clip = sc.clip

        layout.menu("CLIP_MT_view")

        if clip:
            layout.menu("MASK_MT_select")
            layout.menu("CLIP_MT_clip")  # XXX - remove?
            layout.menu("MASK_MT_add")
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

    def draw(self, _context):
        layout = self.layout

        col = layout.column(align=True)
        col.operator("clip.set_scene_frames")
        row = col.row(align=True)
        row.operator("clip.prefetch", text="Prefetch")
        row.operator("clip.reload", text="Reload")


class CLIP_PT_tools_marker(CLIP_PT_tracking_panel, Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'TOOLS'
    bl_label = "Marker"
    bl_category = "Track"

    def draw(self, _context):
        layout = self.layout

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

    def draw_header_preset(self, _context):
        CLIP_PT_tracking_settings_presets.draw_panel_header(self.layout)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        sc = context.space_data
        clip = sc.clip
        settings = clip.tracking.settings

        col = layout.column(align=True)
        col.prop(settings, "default_pattern_size")
        col.prop(settings, "default_search_size")

        col.separator()

        col.prop(settings, "default_motion_model")
        col.prop(settings, "default_pattern_match", text="Match")

        col.prop(settings, "use_default_brute")
        col.prop(settings, "use_default_normalization")

        col = layout.column()

        row = col.row(align=True)
        row.use_property_split = False
        row.prop(settings, "use_default_red_channel",
                 text="R", toggle=True)
        row.prop(settings, "use_default_green_channel",
                 text="G", toggle=True)
        row.prop(settings, "use_default_blue_channel",
                 text="B", toggle=True)

        col.separator()
        col.operator("clip.track_settings_as_default",
                     text="Copy From Active Track")


class CLIP_PT_tracking_settings_extras(CLIP_PT_tracking_panel, Panel):
    bl_label = "Tracking Settings Extra"
    bl_parent_id = "CLIP_PT_tracking_settings"
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'TOOLS'
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        sc = context.space_data
        clip = sc.clip
        settings = clip.tracking.settings

        col = layout.column()
        col.prop(settings, "default_weight")
        col = layout.column(align=True)
        col.prop(settings, "default_correlation_min")
        col.prop(settings, "default_margin")
        col.prop(settings, "use_default_mask")


class CLIP_PT_tools_tracking(CLIP_PT_tracking_panel, Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'TOOLS'
    bl_label = "Track"
    bl_category = "Track"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, _context):
        layout = self.layout

        row = layout.row(align=True)
        row.label(text="Track:")

        props = row.operator("clip.track_markers", text="", icon='TRACKING_BACKWARDS_SINGLE')
        props.backwards = True
        props.sequence = False
        props = row.operator("clip.track_markers", text="",
                             icon='TRACKING_BACKWARDS')
        props.backwards = True
        props.sequence = True
        props = row.operator("clip.track_markers", text="", icon='TRACKING_FORWARDS')
        props.backwards = False
        props.sequence = True
        props = row.operator("clip.track_markers", text="", icon='TRACKING_FORWARDS_SINGLE')
        props.backwards = False
        props.sequence = False

        col = layout.column(align=True)
        row = col.row(align=True)
        row.label(text="Clear:")
        row.scale_x = 2.0

        props = row.operator("clip.clear_track_path", text="", icon='TRACKING_CLEAR_BACKWARDS')
        props.action = 'UPTO'

        props = row.operator("clip.clear_track_path", text="", icon='TRACKING_CLEAR_FORWARDS')
        props.action = 'REMAINED'

        col = layout.column()
        row = col.row(align=True)
        row.label(text="Refine:")
        row.scale_x = 2.0

        props = row.operator("clip.refine_markers", text="", icon='TRACKING_REFINE_BACKWARDS')
        props.backwards = True

        props = row.operator("clip.refine_markers", text="", icon='TRACKING_REFINE_FORWARDS')
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

    def draw(self, _context):
        layout = self.layout
        layout.operator("clip.create_plane_track")


class CLIP_PT_tools_solve(CLIP_PT_tracking_panel, Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'TOOLS'
    bl_label = "Solve"
    bl_category = "Solve"

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        clip = context.space_data.clip
        tracking = clip.tracking
        settings = tracking.settings
        tracking_object = tracking.objects.active

        col = layout.column()
        col.prop(settings, "use_tripod_solver", text="Tripod")
        col = layout.column()
        col.active = not settings.use_tripod_solver
        col.prop(settings, "use_keyframe_selection", text="Keyframe")

        col = layout.column(align=True)
        col.active = (not settings.use_tripod_solver and
                      not settings.use_keyframe_selection)
        col.prop(tracking_object, "keyframe_a")
        col.prop(tracking_object, "keyframe_b")

        col = layout.column()
        col.active = tracking_object.is_camera
        col.prop(settings, "refine_intrinsics", text="Refine")

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
        layout.use_property_split = True
        layout.use_property_decorate = False

        clip = context.space_data.clip
        settings = clip.tracking.settings

        col = layout.column()
        col.prop(settings, "clean_frames", text="Frames")
        col.prop(settings, "clean_error", text="Error")
        col.prop(settings, "clean_action", text="Type")
        col.separator()
        col.operator("clip.clean_tracks")
        col.operator("clip.filter_tracks")


class CLIP_PT_tools_geometry(CLIP_PT_tracking_panel, Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'TOOLS'
    bl_label = "Geometry"
    bl_options = {'DEFAULT_CLOSED'}
    bl_category = "Solve"

    def draw(self, _context):
        layout = self.layout

        layout.operator("clip.bundles_to_mesh")
        layout.operator("clip.track_to_empty")


class CLIP_PT_tools_orientation(CLIP_PT_tracking_panel, Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'TOOLS'
    bl_label = "Orientation"
    bl_category = "Solve"

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        sc = context.space_data
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
    bl_category = "Solve"

    @classmethod
    def poll(cls, context):
        sc = context.space_data
        if CLIP_PT_reconstruction_panel.poll(context) and sc.mode == 'TRACKING':
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
    bl_category = "Track"
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

        sub.operator("clip.tracking_object_new", icon='ADD', text="")
        sub.operator("clip.tracking_object_remove", icon='REMOVE', text="")


class CLIP_PT_track(CLIP_PT_tracking_panel, Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'UI'
    bl_category = "Track"
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

        sub.template_marker(sc, "clip", sc.clip_user, act_track, compact=True)

        icon = 'LOCKED' if act_track.lock else 'UNLOCKED'
        sub.prop(act_track, "lock", text="", icon=icon)

        layout.template_track(sc, "scopes")

        row = layout.row(align=True)
        sub = row.row(align=True)
        sub.prop(act_track, "use_red_channel", text="R", toggle=True)
        sub.prop(act_track, "use_green_channel", text="G", toggle=True)
        sub.prop(act_track, "use_blue_channel", text="B", toggle=True)

        row.separator()

        layout.use_property_split = True

        row.prop(act_track, "use_grayscale_preview", text="B/W", toggle=True)

        row.separator()
        row.prop(act_track, "use_alpha_preview",
                 text="", toggle=True, icon='IMAGE_ALPHA')

        layout.prop(act_track, "weight")
        layout.prop(act_track, "weight_stab")

        if act_track.has_bundle:
            label_text = "Average Error: %.4f" % (act_track.average_error)
            layout.label(text=label_text)

        layout.use_property_split = False

        row = layout.row(align=True)
        row.prop(act_track, "use_custom_color", text="")
        CLIP_PT_track_color_presets.draw_menu(row, 'Custom Color Presets')
        row.operator("clip.track_copy_color", icon='COPY_ID', text="")

        if act_track.use_custom_color:
            row = layout.row()
            row.prop(act_track, "color", text="")


class CLIP_PT_plane_track(CLIP_PT_tracking_panel, Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'UI'
    bl_category = "Track"
    bl_label = "Plane Track"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

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
    bl_category = "Track"
    bl_label = "Tracking Settings"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        clip = context.space_data.clip

        col = layout.column()

        active = clip.tracking.tracks.active
        if active:
            col.prop(active, "motion_model")
            col.prop(active, "pattern_match", text="Match")

            col.prop(active, "use_brute")
            col.prop(active, "use_normalization")


class CLIP_PT_track_settings_extras(CLIP_PT_tracking_panel, Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'UI'
    bl_category = "Track"
    bl_label = "Tracking Settings Extras"
    bl_parent_id = 'CLIP_PT_track_settings'
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        clip = context.space_data.clip
        active = clip.tracking.tracks.active
        settings = clip.tracking.settings

        col = layout.column(align=True)
        col.prop(active, "correlation_min")
        col.prop(active, "margin")

        col = layout.column()
        col.prop(active, "use_mask")
        col.prop(active, "frames_limit")
        col.prop(settings, "speed")


class CLIP_PT_tracking_camera(Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'UI'
    bl_category = "Track"
    bl_label = "Camera"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        if CLIP_PT_clip_view_panel.poll(context):
            sc = context.space_data

            return sc.mode == 'TRACKING' and sc.clip

        return False

    def draw_header_preset(self, _context):
        CLIP_PT_camera_presets.draw_panel_header(self.layout)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        sc = context.space_data
        clip = sc.clip

        col = layout.column(align=True)
        col.prop(clip.tracking.camera, "sensor_width", text="Sensor Width")
        col.prop(clip.tracking.camera, "pixel_aspect", text="Pixel Aspect")

        col = layout.column()
        col.prop(clip.tracking.camera, "principal", text="Optical Center")
        col.operator("clip.set_center_principal", text="Set Center")


class CLIP_PT_tracking_lens(Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'UI'
    bl_category = "Track"
    bl_label = "Lens"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        if CLIP_PT_clip_view_panel.poll(context):
            sc = context.space_data

            return sc.mode == 'TRACKING' and sc.clip

        return False

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        sc = context.space_data
        clip = sc.clip
        camera = clip.tracking.camera

        col = layout.column()

        if camera.units == 'MILLIMETERS':
            col.prop(camera, "focal_length")
        else:
            col.prop(camera, "focal_length_pixels")
        col.prop(camera, "units", text="Units")

        col = layout.column()
        col.prop(camera, "distortion_model", text="Lens Distortion")
        if camera.distortion_model == 'POLYNOMIAL':
            col = layout.column(align=True)
            col.prop(camera, "k1")
            col.prop(camera, "k2")
            col.prop(camera, "k3")
        elif camera.distortion_model == 'DIVISION':
            col = layout.column(align=True)
            col.prop(camera, "division_k1")
            col.prop(camera, "division_k2")


class CLIP_PT_marker(CLIP_PT_tracking_panel, Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'UI'
    bl_category = "Track"
    bl_label = "Marker"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        sc = context.space_data
        clip = context.space_data.clip
        act_track = clip.tracking.tracks.active

        if act_track:
            layout.template_marker(sc, "clip", sc.clip_user, act_track, compact=False)
        else:
            layout.active = False
            layout.label(text="No active track")


class CLIP_PT_stabilization(CLIP_PT_reconstruction_panel, Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'UI'
    bl_label = "2D Stabilization"
    bl_category = "Stabilization"

    @classmethod
    def poll(cls, context):
        if CLIP_PT_clip_view_panel.poll(context):
            sc = context.space_data

            return sc.mode == 'TRACKING' and sc.clip

        return False

    def draw_header(self, context):
        stab = context.space_data.clip.tracking.stabilization

        self.layout.prop(stab, "use_2d_stabilization", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        tracking = context.space_data.clip.tracking
        stab = tracking.stabilization

        layout.active = stab.use_2d_stabilization

        layout.prop(stab, "anchor_frame")

        row = layout.row(align=True)
        row.prop(stab, "use_stabilize_rotation", text="Rotation")
        sub = row.row(align=True)
        sub.active = stab.use_stabilize_rotation
        sub.prop(stab, "use_stabilize_scale", text="Scale")

        box = layout.box()
        row = box.row(align=True)
        row.prop(stab, "show_tracks_expanded", text="", emboss=False)

        if not stab.show_tracks_expanded:
            row.label(text="Tracks For Stabilization")
        else:
            row.label(text="Tracks For Location")
            row = box.row()
            row.template_list("UI_UL_list", "stabilization_tracks", stab, "tracks",
                              stab, "active_track_index", rows=2)

            sub = row.column(align=True)

            sub.operator("clip.stabilize_2d_add", icon='ADD', text="")
            sub.operator("clip.stabilize_2d_remove", icon='REMOVE', text="")

            sub.menu('CLIP_MT_stabilize_2d_context_menu', text="",
                     icon='DOWNARROW_HLT')

            # Usually we don't hide things from interface, but here every pixel of
            # vertical space is precious.
            if stab.use_stabilize_rotation:
                box.label(text="Tracks For Rotation / Scale")
                row = box.row()
                row.template_list("UI_UL_list", "stabilization_rotation_tracks",
                                  stab, "rotation_tracks",
                                  stab, "active_rotation_track_index", rows=2)

                sub = row.column(align=True)

                sub.operator("clip.stabilize_2d_rotation_add", icon='ADD', text="")
                sub.operator("clip.stabilize_2d_rotation_remove", icon='REMOVE', text="")

                sub.menu('CLIP_MT_stabilize_2d_rotation_context_menu', text="",
                         icon='DOWNARROW_HLT')

        col = layout.column()
        col.prop(stab, "use_autoscale")
        sub = col.row()
        sub.active = stab.use_autoscale
        sub.prop(stab, "scale_max", text="Max")

        col = layout.column(align=True)
        row = col.row(align=True)
        row.prop(stab, "target_position", text="Target")
        col.prop(stab, "target_rotation")
        row = col.row(align=True)
        row.prop(stab, "target_scale")
        row.active = not stab.use_autoscale

        col = layout.column(align=True)
        col.prop(stab, "influence_location")
        sub = col.column(align=True)
        sub.active = stab.use_stabilize_rotation
        sub.prop(stab, "influence_rotation")
        sub.prop(stab, "influence_scale")

        layout.prop(stab, "filter_type")


class CLIP_PT_proxy(CLIP_PT_clip_view_panel, Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'UI'
    bl_category = "Footage"
    bl_label = "Proxy/Timecode"
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

        layout.use_property_split = True
        layout.use_property_decorate = False
        col = layout.column()
        col.prop(clip.proxy, "quality")

        col.prop(clip, "use_proxy_custom_directory")
        if clip.use_proxy_custom_directory:
            col.prop(clip.proxy, "directory")

        col.operator(
            "clip.rebuild_proxy",
            text="Build Proxy / Timecode" if clip.source == 'MOVIE'
            else "Build Proxy"
        )

        if clip.source == 'MOVIE':
            col2 = col.column()
            col2.prop(clip.proxy, "timecode", text="Timecode Index")

        col.separator()

        col.prop(sc.clip_user, "proxy_render_size", text="Proxy Size")


# -----------------------------------------------------------------------------
# Mask (similar code in space_image.py, keep in sync)

from bl_ui.properties_mask_common import (
    MASK_PT_mask,
    MASK_PT_layers,
    MASK_PT_spline,
    MASK_PT_point,
    MASK_PT_display,
    MASK_PT_transforms,
    MASK_PT_tools
)


class CLIP_PT_mask_layers(MASK_PT_layers, Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'UI'
    bl_category = "Mask"


class CLIP_PT_mask_display(MASK_PT_display, Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'HEADER'
    bl_category = "Mask"


class CLIP_PT_active_mask_spline(MASK_PT_spline, Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'UI'
    bl_category = "Mask"


class CLIP_PT_active_mask_point(MASK_PT_point, Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'UI'
    bl_category = "Mask"


class CLIP_PT_mask(MASK_PT_mask, Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'UI'
    bl_category = "Mask"


class CLIP_PT_tools_mask_transforms(MASK_PT_transforms, Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'TOOLS'
    bl_category = "Mask"


class CLIP_PT_tools_mask_tools(MASK_PT_tools, Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'TOOLS'
    bl_category = "Mask"

# --- end mask ---


class CLIP_PT_footage(CLIP_PT_clip_view_panel, Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'UI'
    bl_category = "Footage"
    bl_label = "Footage Settings"

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        sc = context.space_data
        clip = sc.clip

        col = layout.column()
        col.template_movieclip(sc, "clip", compact=True)
        col.prop(clip, "frame_start")
        col.prop(clip, "frame_offset")
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

    def draw(self, _context):
        layout = self.layout

        layout.operator("clip.set_viewport_background")
        layout.operator("clip.setup_tracking_scene")


# Grease Pencil properties
class CLIP_PT_annotation(AnnotationDataPanel, CLIP_PT_clip_view_panel, Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'UI'
    bl_category = "Annotation"
    bl_options = set()

    # NOTE: this is just a wrapper around the generic GP Panel
    # But, this should only be visible in "clip" view


# Grease Pencil drawing tools
class CLIP_PT_tools_grease_pencil_draw(AnnotationDrawingToolsPanel, Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'TOOLS'


class CLIP_MT_view(Menu):
    bl_label = "View"

    def draw(self, context):
        layout = self.layout

        sc = context.space_data

        if sc.view == 'CLIP':
            layout.prop(sc, "show_region_ui")
            layout.prop(sc, "show_region_toolbar")
            layout.prop(sc, "show_region_hud")

            layout.separator()

            layout.operator("clip.view_selected")
            layout.operator("clip.view_all")
            layout.operator("clip.view_all", text="View Fit").fit_view = True

            layout.separator()
            layout.operator("clip.view_zoom_in")
            layout.operator("clip.view_zoom_out")

            layout.separator()
            layout.prop(sc, "show_metadata")
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
            layout.prop(sc, "show_locked_time")

        layout.separator()

        layout.menu("INFO_MT_area")


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

    def draw(self, _context):
        layout = self.layout

        layout.operator("clip.rebuild_proxy")
        layout.operator("clip.delete_proxy")


class CLIP_MT_track(Menu):
    bl_label = "Track"

    def draw(self, _context):
        layout = self.layout

        layout.operator("clip.clear_solution")
        layout.operator("clip.solve_camera")

        layout.separator()
        props = layout.operator("clip.clear_track_path", text="Clear After")
        props.clear_active = False
        props.action = 'REMAINED'

        props = layout.operator("clip.clear_track_path", text="Clear Before")
        props.clear_active = False
        props.action = 'UPTO'

        props = layout.operator("clip.clear_track_path", text="Clear Track Path")
        props.clear_active = False
        props.action = 'ALL'

        layout.separator()
        layout.operator("clip.join_tracks")

        layout.separator()
        layout.operator("clip.clean_tracks")

        layout.separator()
        layout.operator("clip.copy_tracks")
        layout.operator("clip.paste_tracks")

        layout.separator()
        props = layout.operator("clip.track_markers", text="Track Frame Backwards")
        props.backwards = True
        props.sequence = False

        props = layout.operator("clip.track_markers", text="Track Backwards")
        props.backwards = True
        props.sequence = True

        props = layout.operator("clip.track_markers", text="Track Forwards")
        props.backwards = False
        props.sequence = True

        props = layout.operator("clip.track_markers", text="Track Frame Forwards")
        props.backwards = False
        props.sequence = False

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

    def draw(self, _context):
        layout = self.layout

        layout.operator("clip.set_origin")
        layout.operator("clip.set_plane", text="Set Floor").plane = 'FLOOR'
        layout.operator("clip.set_plane", text="Set Wall").plane = 'WALL'

        layout.operator("clip.set_axis", text="Set X Axis").axis = 'X'
        layout.operator("clip.set_axis", text="Set Y Axis").axis = 'Y'

        layout.operator("clip.set_scale")

        layout.separator()

        layout.operator("clip.track_to_empty")
        layout.operator("clip.bundles_to_mesh")


class CLIP_MT_track_visibility(Menu):
    bl_label = "Show/Hide"

    def draw(self, _context):
        layout = self.layout

        layout.operator("clip.hide_tracks_clear")
        layout.operator("clip.hide_tracks", text="Hide Selected").unselected = False
        layout.operator("clip.hide_tracks", text="Hide Unselected").unselected = True


class CLIP_MT_track_transform(Menu):
    bl_label = "Transform"

    def draw(self, _context):
        layout = self.layout

        layout.operator("transform.translate")
        layout.operator("transform.resize")


class CLIP_MT_select(Menu):
    bl_label = "Select"

    def draw(self, _context):
        layout = self.layout

        layout.operator("clip.select_box")
        layout.operator("clip.select_circle")

        layout.separator()

        layout.operator("clip.select_all"
                        ).action = 'TOGGLE'
        layout.operator("clip.select_all",
                        text="Inverse").action = 'INVERT'

        layout.menu("CLIP_MT_select_grouped")


class CLIP_MT_select_grouped(Menu):
    bl_label = "Select Grouped"

    def draw(self, _context):
        layout = self.layout

        layout.operator_enum("clip.select_grouped", "group")


class CLIP_MT_mask_handle_type_menu(Menu):
    bl_label = "Set Handle Type"

    def draw(self, _context):
        layout = self.layout

        layout.operator_enum("mask.handle_type_set", "type")


class CLIP_MT_tracking_context_menu(Menu):
    bl_label = "Context Menu"

    @classmethod
    def poll(cls, context):
        return context.space_data.clip

    def draw(self, _context):
        layout = self.layout

        mode = _context.space_data.mode

        if mode == 'TRACKING':

            layout.operator("clip.track_settings_to_track")
            layout.operator("clip.track_settings_as_default")

            layout.separator()

            layout.operator("clip.track_copy_color")

            layout.separator()

            layout.operator("clip.copy_tracks", icon='COPYDOWN')
            layout.operator("clip.paste_tracks", icon='PASTEDOWN')

            layout.separator()

            layout.operator("clip.disable_markers",
                            text="Disable Markers").action = 'DISABLE'
            layout.operator("clip.disable_markers",
                            text="Enable Markers").action = 'ENABLE'

            layout.separator()

            layout.operator("clip.hide_tracks")
            layout.operator("clip.hide_tracks_clear", text="Show Tracks")

            layout.separator()

            layout.operator("clip.lock_tracks", text="Lock Tracks").action = 'LOCK'
            layout.operator("clip.lock_tracks",
                            text="Unlock Tracks").action = 'UNLOCK'

            layout.separator()

            layout.operator("clip.join_tracks")

            layout.separator()

            layout.operator("clip.delete_track")

        elif mode == 'MASK':

            layout.menu("CLIP_MT_mask_handle_type_menu")
            layout.operator("mask.switch_direction")
            layout.operator("mask.cyclic_toggle")

            layout.separator()

            layout.operator("mask.copy_splines", icon='COPYDOWN')
            layout.operator("mask.paste_splines", icon='PASTEDOWN')

            layout.separator()

            layout.operator("mask.shape_key_rekey", text="Re-key Shape Points")
            layout.operator("mask.feather_weight_clear")
            layout.operator("mask.shape_key_feather_reset", text="Reset Feather Animation")

            layout.separator()

            layout.operator("mask.parent_set")
            layout.operator("mask.parent_clear")

            layout.separator()

            layout.operator("mask.delete")


class CLIP_PT_camera_presets(PresetPanel, Panel):
    """Predefined tracking camera intrinsics"""
    bl_label = "Camera Presets"
    preset_subdir = "tracking_camera"
    preset_operator = "script.execute_preset"
    preset_add_operator = "clip.camera_preset_add"


class CLIP_PT_track_color_presets(PresetPanel, Panel):
    """Predefined track color"""
    bl_label = "Color Presets"
    preset_subdir = "tracking_track_color"
    preset_operator = "script.execute_preset"
    preset_add_operator = "clip.track_color_preset_add"


class CLIP_PT_tracking_settings_presets(PresetPanel, Panel):
    """Predefined tracking settings"""
    bl_label = "Tracking Presets"
    preset_subdir = "tracking_settings"
    preset_operator = "script.execute_preset"
    preset_add_operator = "clip.tracking_settings_preset_add"


class CLIP_MT_stabilize_2d_context_menu(Menu):
    bl_label = "Translation Track Specials"

    def draw(self, _context):
        layout = self.layout

        layout.operator("clip.stabilize_2d_select")


class CLIP_MT_stabilize_2d_rotation_context_menu(Menu):
    bl_label = "Rotation Track Specials"

    def draw(self, _context):
        layout = self.layout

        layout.operator("clip.stabilize_2d_rotation_select")


class CLIP_MT_pivot_pie(Menu):
    bl_label = "Pivot Point"

    def draw(self, context):
        layout = self.layout
        pie = layout.menu_pie()

        pie.prop_enum(context.space_data, "pivot_point", value='BOUNDING_BOX_CENTER')
        pie.prop_enum(context.space_data, "pivot_point", value='CURSOR')
        pie.prop_enum(context.space_data, "pivot_point", value='INDIVIDUAL_ORIGINS')
        pie.prop_enum(context.space_data, "pivot_point", value='MEDIAN_POINT')


class CLIP_MT_marker_pie(Menu):
    # Settings for the individual markers
    bl_label = "Marker Settings"

    @classmethod
    def poll(cls, context):
        space = context.space_data
        return space.mode == 'TRACKING' and space.clip

    def draw(self, context):
        clip = context.space_data.clip
        tracks = getattr(getattr(clip, "tracking", None), "tracks", None)
        track_active = tracks.active if tracks else None

        layout = self.layout
        pie = layout.menu_pie()
        # Use Location Tracking
        prop = pie.operator("wm.context_set_enum", text="Loc")
        prop.data_path = "space_data.clip.tracking.tracks.active.motion_model"
        prop.value = "Loc"
        # Use Affine Tracking
        prop = pie.operator("wm.context_set_enum", text="Affine")
        prop.data_path = "space_data.clip.tracking.tracks.active.motion_model"
        prop.value = "Affine"
        # Copy Settings From Active To Selected
        pie.operator("clip.track_settings_to_track", icon='COPYDOWN')
        # Make Settings Default
        pie.operator("clip.track_settings_as_default", icon='SETTINGS')
        if track_active:
            # Use Normalization
            pie.prop(track_active, "use_normalization", text="Normalization")
            # Use Brute Force
            pie.prop(track_active, "use_brute", text="Use Brute Force")
            # Match Keyframe
            prop = pie.operator("wm.context_set_enum", text="Match Previous", icon='KEYFRAME_HLT')
            prop.data_path = "space_data.clip.tracking.tracks.active.pattern_match"
            prop.value = 'KEYFRAME'
            # Match Previous Frame
            prop = pie.operator("wm.context_set_enum", text="Match Keyframe", icon='KEYFRAME')
            prop.data_path = "space_data.clip.tracking.tracks.active.pattern_match"
            prop.value = 'PREV_FRAME'


class CLIP_MT_tracking_pie(Menu):
    # Tracking Operators
    bl_label = "Tracking"

    @classmethod
    def poll(cls, context):
        space = context.space_data
        return space.mode == 'TRACKING' and space.clip

    def draw(self, _context):
        layout = self.layout

        pie = layout.menu_pie()
        # Track Backwards
        prop = pie.operator("clip.track_markers", icon='TRACKING_BACKWARDS')
        prop.backwards = True
        prop.sequence = True
        # Track Forwards
        prop = pie.operator("clip.track_markers", icon='TRACKING_FORWARDS')
        prop.backwards = False
        prop.sequence = True
        # Disable Marker
        pie.operator("clip.disable_markers", icon='HIDE_OFF').action = 'TOGGLE'
        # Detect Features
        pie.operator("clip.detect_features", icon='ZOOM_SELECTED')
        # Clear Path Backwards
        pie.operator("clip.clear_track_path", icon='TRACKING_CLEAR_BACKWARDS').action = 'UPTO'
        # Clear Path Forwards
        pie.operator("clip.clear_track_path", icon='TRACKING_CLEAR_FORWARDS').action = 'REMAINED'
        # Refine Backwards
        pie.operator("clip.refine_markers", icon='TRACKING_REFINE_BACKWARDS').backwards = True
        # Refine Forwards
        pie.operator("clip.refine_markers", icon='TRACKING_REFINE_FORWARDS').backwards = False


class CLIP_MT_solving_pie(Menu):
    # Operators to solve the scene
    bl_label = "Solving"

    @classmethod
    def poll(cls, context):
        space = context.space_data
        return space.mode == 'TRACKING' and space.clip

    def draw(self, context):
        clip = context.space_data.clip
        settings = getattr(getattr(clip, "tracking", None), "settings", None)

        layout = self.layout
        pie = layout.menu_pie()
        # Clear Solution
        pie.operator("clip.clear_solution", icon='FILE_REFRESH')
        # Solve Camera
        pie.operator("clip.solve_camera", text="Solve Camera", icon='OUTLINER_OB_CAMERA')
        # Use Tripod Solver
        if settings:
            pie.prop(settings, "use_tripod_solver", text="Tripod Solver")
        # create Plane Track
        pie.operator("clip.create_plane_track", icon='MATPLANE')
        # Set Keyframe A
        pie.operator(
            "clip.set_solver_keyframe",
            text="Set Keyframe A",
            icon='KEYFRAME',
        ).keyframe = 'KEYFRAME_A'
        # Set Keyframe B
        pie.operator(
            "clip.set_solver_keyframe",
            text="Set Keyframe B",
            icon='KEYFRAME',
        ).keyframe = 'KEYFRAME_B'
        # Clean Tracks
        prop = pie.operator("clip.clean_tracks", icon='X')
        # Filter Tracks
        pie.operator("clip.filter_tracks", icon='FILTER')
        prop.frames = 15
        prop.error = 2


class CLIP_MT_reconstruction_pie(Menu):
    # Scene Reconstruction
    bl_label = "Reconstruction"

    @classmethod
    def poll(cls, context):
        space = context.space_data
        return space.mode == 'TRACKING' and space.clip

    def draw(self, _context):
        layout = self.layout
        pie = layout.menu_pie()
        # Set Active Clip As Viewport Background
        pie.operator("clip.set_viewport_background", text="Set Viewport Background", icon='FILE_IMAGE')
        # Setup Tracking Scene
        pie.operator("clip.setup_tracking_scene", text="Setup Tracking Scene", icon='SCENE_DATA')
        # Setup Floor
        pie.operator("clip.set_plane", text="Set Floor", icon='AXIS_TOP')
        # Set Origin
        pie.operator("clip.set_origin", text="Set Origin", icon='OBJECT_ORIGIN')
        # Set X Axis
        pie.operator("clip.set_axis", text="Set X Axis", icon='AXIS_FRONT').axis = 'X'
        # Set Y Axis
        pie.operator("clip.set_axis", text="Set Y Axis", icon='AXIS_SIDE').axis = 'Y'
        # Set Scale
        pie.operator("clip.set_scale", text="Set Scale", icon='ARROW_LEFTRIGHT')
        # Apply Solution Scale
        pie.operator("clip.apply_solution_scale", icon='ARROW_LEFTRIGHT')


classes = (
    CLIP_UL_tracking_objects,
    CLIP_HT_header,
    CLIP_PT_display,
    CLIP_PT_clip_display,
    CLIP_PT_marker_display,
    CLIP_MT_track,
    CLIP_MT_tracking_editor_menus,
    CLIP_MT_masking_editor_menus,
    CLIP_PT_track,
    CLIP_PT_tools_clip,
    CLIP_PT_tools_marker,
    CLIP_PT_tracking_settings,
    CLIP_PT_tracking_settings_extras,
    CLIP_PT_tools_tracking,
    CLIP_PT_tools_plane_tracking,
    CLIP_PT_tools_solve,
    CLIP_PT_tools_cleanup,
    CLIP_PT_tools_geometry,
    CLIP_PT_tools_orientation,
    CLIP_PT_tools_object,
    CLIP_PT_objects,
    CLIP_PT_plane_track,
    CLIP_PT_track_settings,
    CLIP_PT_track_settings_extras,
    CLIP_PT_tracking_camera,
    CLIP_PT_tracking_lens,
    CLIP_PT_marker,
    CLIP_PT_proxy,
    CLIP_PT_footage,
    CLIP_PT_stabilization,
    CLIP_PT_mask,
    CLIP_PT_mask_layers,
    CLIP_PT_mask_display,
    CLIP_PT_active_mask_spline,
    CLIP_PT_active_mask_point,
    CLIP_PT_tools_mask_transforms,
    CLIP_PT_tools_mask_tools,
    CLIP_PT_tools_scenesetup,
    CLIP_PT_annotation,
    CLIP_PT_tools_grease_pencil_draw,
    CLIP_MT_view,
    CLIP_MT_clip,
    CLIP_MT_proxy,
    CLIP_MT_reconstruction,
    CLIP_MT_track_visibility,
    CLIP_MT_track_transform,
    CLIP_MT_select,
    CLIP_MT_select_grouped,
    CLIP_MT_tracking_context_menu,
    CLIP_PT_camera_presets,
    CLIP_PT_track_color_presets,
    CLIP_PT_tracking_settings_presets,
    CLIP_MT_stabilize_2d_context_menu,
    CLIP_MT_stabilize_2d_rotation_context_menu,
    CLIP_MT_pivot_pie,
    CLIP_MT_marker_pie,
    CLIP_MT_tracking_pie,
    CLIP_MT_reconstruction_pie,
    CLIP_MT_solving_pie,
    CLIP_MT_mask_handle_type_menu
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
