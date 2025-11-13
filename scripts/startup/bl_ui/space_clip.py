# SPDX-FileCopyrightText: 2011-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

from bpy.types import Panel, Header, Menu, UIList
from bpy.app.translations import (
    pgettext_iface as iface_,
    pgettext_rpt as rpt_,
    contexts as i18n_contexts,
)
from bl_ui.utils import PresetPanel
from bl_ui.properties_grease_pencil_common import (
    AnnotationDrawingToolsPanel,
    AnnotationDataPanel,
)
from bl_ui import anim


class CLIP_UL_tracking_objects(UIList):
    def draw_item(self, _context, layout, _data, item, _icon, _active_data, _active_propname, _index):
        # assert(isinstance(item, bpy.types.MovieTrackingObject)
        tobj = item
        layout.prop(
            tobj, "name", text="", emboss=False,
            icon='CAMERA_DATA' if tobj.is_camera else 'OBJECT_DATA',
        )


class CLIP_PT_display(Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'HEADER'
    bl_label = "Clip Display"
    bl_ui_units_x = 13

    def draw(self, context):
        pass


class CLIP_PT_clip_display(Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'HEADER'
    bl_label = "Clip Display"
    bl_parent_id = "CLIP_PT_display"
    bl_ui_units_x = 13

    def draw(self, context):
        layout = self.layout

        sc = context.space_data

        col = layout.column(align=True)

        row = layout.row(align=True)
        row.prop(sc, "show_red_channel", text="R", text_ctxt=i18n_contexts.color, toggle=True)
        row.prop(sc, "show_green_channel", text="G", text_ctxt=i18n_contexts.color, toggle=True)
        row.prop(sc, "show_blue_channel", text="B", text_ctxt=i18n_contexts.color, toggle=True)
        row.separator()
        row.prop(sc, "use_grayscale_preview", text="B/W", toggle=True)
        row.separator()
        row.prop(sc, "use_mute_footage", text="", icon='HIDE_OFF', toggle=True)

        layout.separator()

        row = layout.row()
        col = row.column()
        col.prop(sc.clip_user, "use_render_undistorted", text="Render Undistorted")
        col = row.column()
        col.prop(sc, "show_stable", text="Show Stable")
        col.prop(sc, "use_manual_calibration", text="Calibration")

        clip = sc.clip
        if clip:
            col = layout.column()
            col.use_property_split = True
            col.use_property_decorate = False
            col.separator()
            col.prop(clip, "display_aspect", text="Aspect Ratio")


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

            row = layout.row()
            row.prop(sc, "pivot_point", text="", icon_only=True)
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
            props = row.operator("clip.track_markers", text="", icon='TRACKING_BACKWARDS')
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
                r = active_object.reconstruction

                if r.is_valid and sc.view == 'CLIP':
                    layout.label(text=rpt_("Solve error: {:.2f} px").format(r.average_error), translate=False)

                row = layout.row(align=True)
                icon = 'LOCKED' if sc.lock_selection else 'UNLOCKED'
                row.operator("clip.lock_selection_toggle", icon=icon, text="", depress=sc.lock_selection)
                row.popover(panel="CLIP_PT_display")

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
                row.prop(
                    dopesheet, "use_invert_sort",
                    text="", toggle=True,
                    icon='SORT_DESC' if dopesheet.use_invert_sort else 'SORT_ASC',
                )

    def _draw_masking(self, context):
        layout = self.layout

        tool_settings = context.tool_settings
        sc = context.space_data
        clip = sc.clip

        CLIP_MT_masking_editor_menus.draw_collapsible(context, layout)

        layout.separator_spacer()

        row = layout.row()
        row.template_ID(sc, "clip", open="clip.open")

        if clip:
            row = layout.row()
            row.template_ID(sc, "mask", new="mask.new")

            layout.prop(sc, "pivot_point", text="", icon_only=True)

            row = layout.row(align=True)
            row.prop(tool_settings, "use_proportional_edit_mask", text="", icon_only=True)
            sub = row.row(align=True)
            sub.active = tool_settings.use_proportional_edit_mask
            sub.prop_with_popover(
                tool_settings,
                "proportional_edit_falloff",
                text="",
                icon_only=True,
                panel="CLIP_PT_proportional_edit",
            )

            layout.separator_spacer()

            row = layout.row(align=True)
            icon = 'LOCKED' if sc.lock_selection else 'UNLOCKED'
            row.operator("clip.lock_selection_toggle", icon=icon, text="", depress=sc.lock_selection)
            row.popover(panel="CLIP_PT_display")
        else:
            layout.separator_spacer()

    def draw(self, context):
        layout = self.layout

        sc = context.space_data
        overlay = sc.overlay

        layout.template_header()

        layout.prop(sc, "mode", text="")
        if sc.mode == 'TRACKING':
            layout.prop(sc, "view", text="")
            self._draw_tracking(context)
        else:
            self._draw_masking(context)

        # Gizmo toggle & popover.
        row = layout.row(align=True)
        row.prop(sc, "show_gizmo", icon='GIZMO', text="")
        sub = row.row(align=True)
        sub.active = sc.show_gizmo
        sub.popover(panel="CLIP_PT_gizmo_display", text="")

        # Overlay toggle & popover.
        if sc.view == 'CLIP':
            row = layout.row(align=True)
            row.prop(overlay, "show_overlays", icon='OVERLAY', text="")
            sub = row.row(align=True)
            sub.active = overlay.show_overlays
            sub.popover(panel="CLIP_PT_overlay", text="")


class CLIP_PT_proportional_edit(Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'HEADER'
    bl_label = "Proportional Editing"
    bl_ui_units_x = 8

    def draw(self, context):
        layout = self.layout
        tool_settings = context.tool_settings
        col = layout.column()
        col.active = tool_settings.use_proportional_edit_mask

        col.prop(tool_settings, "proportional_edit_falloff", expand=True)
        col.prop(tool_settings, "proportional_size")


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
        elif sc.view == 'GRAPH':
            layout.menu("CLIP_MT_select_graph")


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
            layout.menu("CLIP_MT_clip")
            layout.menu("MASK_MT_add")
            layout.menu("MASK_MT_mask")
        else:
            layout.menu("CLIP_MT_clip")


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
    bl_translation_context = i18n_contexts.id_movieclip
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
    bl_translation_context = i18n_contexts.id_movieclip
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
        row.prop(settings, "use_default_red_channel", text="R", text_ctxt=i18n_contexts.color, toggle=True)
        row.prop(settings, "use_default_green_channel", text="G", text_ctxt=i18n_contexts.color, toggle=True)
        row.prop(settings, "use_default_blue_channel", text="B", text_ctxt=i18n_contexts.color, toggle=True)

        col.separator()
        col.operator("clip.track_settings_as_default", text="Copy from Active Track")


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
    bl_translation_context = i18n_contexts.id_movieclip
    bl_category = "Track"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, _context):
        layout = self.layout

        row = layout.row(align=True)
        row.label(text="Track:")

        props = row.operator("clip.track_markers", text="", icon='TRACKING_BACKWARDS_SINGLE')
        props.backwards = True
        props.sequence = False
        props = row.operator("clip.track_markers", text="", icon='TRACKING_BACKWARDS')
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
        sub = row.column()
        sub.operator("clip.join_tracks", text="Join Tracks")
        sub.operator("clip.average_tracks", text="Average Tracks")


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
        camera = clip.tracking.camera

        col = layout.column()
        col.prop(settings, "use_tripod_solver", text="Tripod")
        col = layout.column()
        col.active = not settings.use_tripod_solver
        col.prop(settings, "use_keyframe_selection", text="Keyframe")

        col = layout.column(align=True)
        col.active = (
            not settings.use_tripod_solver and
            not settings.use_keyframe_selection
        )
        col.prop(tracking_object, "keyframe_a")
        col.prop(tracking_object, "keyframe_b")

        col = layout.column(heading="Refine", align=True)
        col.active = tracking_object.is_camera
        col.prop(settings, "refine_intrinsics_focal_length", text="Focal Length")
        col.prop(settings, "refine_intrinsics_principal_point", text="Optical Center")

        col.prop(settings, "refine_intrinsics_radial_distortion", text="Radial Distortion")

        row = col.row()
        row.active = (camera.distortion_model in ('BROWN', 'NUKE'))
        row.prop(settings, "refine_intrinsics_tangential_distortion", text="Tangential Distortion")

        col = layout.column(align=True)
        col.scale_y = 2.0

        col.operator(
            "clip.solve_camera",
            text=(
                "Solve Camera Motion" if tracking_object.is_camera else
                "Solve Object Motion"
            ),
        )


class CLIP_PT_tools_cleanup(CLIP_PT_tracking_panel, Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'TOOLS'
    bl_label = "Clean Up"
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
        row.template_list("CLIP_UL_tracking_objects", "", tracking, "objects", tracking, "active_object_index", rows=1)

        sub = row.column(align=True)

        sub.operator("clip.tracking_object_new", icon='ADD', text="")
        sub.operator("clip.tracking_object_remove", icon='REMOVE', text="")


class CLIP_PT_track(CLIP_PT_tracking_panel, Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'UI'
    bl_category = "Track"
    bl_label = "Track"
    bl_translation_context = i18n_contexts.id_movieclip

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
        sub.prop(act_track, "use_red_channel", text="R", text_ctxt=i18n_contexts.color, toggle=True)
        sub.prop(act_track, "use_green_channel", text="G", text_ctxt=i18n_contexts.color, toggle=True)
        sub.prop(act_track, "use_blue_channel", text="B", text_ctxt=i18n_contexts.color, toggle=True)

        row.separator()

        layout.use_property_split = True

        row.prop(act_track, "use_grayscale_preview", text="B/W", toggle=True)

        row.separator()
        row.prop(act_track, "use_alpha_preview", text="", toggle=True, icon='IMAGE_ALPHA')

        layout.prop(act_track, "weight")
        layout.prop(act_track, "weight_stab")

        if act_track.has_bundle:
            label_text = rpt_("Average Error: {:.2f} px").format(act_track.average_error)
            layout.label(text=label_text, translate=False)

        layout.use_property_split = False

        row = layout.row(align=True)
        row.prop(act_track, "use_custom_color", text="")
        CLIP_PT_track_color_presets.draw_menu(row, iface_("Custom Color Presets"))
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
        layout.use_property_decorate = False  # No animation.

        clip = context.space_data.clip
        active_track = clip.tracking.plane_tracks.active

        if not active_track:
            layout.active = False
            layout.label(text="No active plane track")
            return

        layout.prop(active_track, "name")
        layout.prop(active_track, "use_auto_keying")
        row = layout.row()
        row.template_ID(active_track, "image", new="image.new", open="image.open")
        row.menu("CLIP_MT_plane_track_image_context_menu", icon='DOWNARROW_HLT', text="")

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
        active = clip.tracking.tracks.active

        if not active:
            layout.active = False
            layout.label(text="No active track")
            return

        col = layout.column()
        col.prop(active, "motion_model")
        col.prop(active, "pattern_match", text="Match")

        col.prop(active, "use_brute")
        col.prop(active, "use_normalization")


class CLIP_PT_track_settings_extras(CLIP_PT_tracking_panel, Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'UI'
    bl_category = "Track"
    bl_label = "Tracking Settings Extras"
    bl_parent_id = "CLIP_PT_track_settings"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        clip = context.space_data.clip

        return clip.tracking.tracks.active

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


class CLIP_PT_tracking_lens(Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'UI'
    bl_category = "Track"
    bl_label = "Lens"
    bl_translation_context = i18n_contexts.id_camera
    bl_parent_id = "CLIP_PT_tracking_camera"
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
        col.prop(clip.tracking.camera, "principal_point", text="Optical Center")

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
        elif camera.distortion_model == 'NUKE':
            col = layout.column(align=True)
            col.prop(camera, "nuke_k1")
            col.prop(camera, "nuke_k2")
            col.separator()
            col.prop(camera, "nuke_p1")
            col.prop(camera, "nuke_p2")
        elif camera.distortion_model == 'BROWN':
            col = layout.column(align=True)
            col.prop(camera, "brown_k1")
            col.prop(camera, "brown_k2")
            col.prop(camera, "brown_k3")
            col.prop(camera, "brown_k4")
            col.separator()
            col.prop(camera, "brown_p1")
            col.prop(camera, "brown_p2")


class CLIP_PT_marker(CLIP_PT_tracking_panel, Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'UI'
    bl_category = "Track"
    bl_label = "Marker"
    bl_translation_context = i18n_contexts.id_movieclip
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
            row.label(text="Tracks for Stabilization")
        else:
            row.label(text="Tracks for Location")
            row = box.row()
            row.template_list("UI_UL_list", "stabilization_tracks", stab, "tracks", stab, "active_track_index", rows=2)

            sub = row.column(align=True)

            sub.operator("clip.stabilize_2d_add", icon='ADD', text="")
            sub.operator("clip.stabilize_2d_remove", icon='REMOVE', text="")

            sub.menu("CLIP_MT_stabilize_2d_context_menu", text="", icon='DOWNARROW_HLT')

            # Usually we don't hide things from interface, but here every pixel of
            # vertical space is precious.
            if stab.use_stabilize_rotation:
                box.label(text="Tracks for Rotation/Scale")
                row = box.row()
                row.template_list(
                    "UI_UL_list", "stabilization_rotation_tracks",
                    stab, "rotation_tracks",
                    stab, "active_rotation_track_index", rows=2,
                )

                sub = row.column(align=True)

                sub.operator("clip.stabilize_2d_rotation_add", icon='ADD', text="")
                sub.operator("clip.stabilize_2d_rotation_remove", icon='REMOVE', text="")

                sub.menu("CLIP_MT_stabilize_2d_rotation_context_menu", text="", icon='DOWNARROW_HLT')

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


class CLIP_PT_2d_cursor(Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'UI'
    bl_category = "View"
    bl_label = "2D Cursor"

    @classmethod
    def poll(cls, context):
        if not CLIP_PT_clip_view_panel.poll(context):
            return False

        sc = context.space_data
        return sc.pivot_point == 'CURSOR' or sc.mode == 'MASK'

    def draw(self, context):
        layout = self.layout

        sc = context.space_data

        layout.use_property_split = True
        layout.use_property_decorate = False

        col = layout.column()
        col.prop(sc, "cursor_location", text="Location")


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
    MASK_PT_animation,
    MASK_PT_display,
    MASK_PT_transforms,
    MASK_PT_tools,
)


class CLIP_PT_mask_layers(MASK_PT_layers, Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'UI'
    bl_category = "Mask"


class CLIP_PT_active_mask_spline(MASK_PT_spline, Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'UI'
    bl_category = "Mask"


class CLIP_PT_active_mask_point(MASK_PT_point, Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'UI'
    bl_category = "Mask"


class CLIP_PT_mask_animation(MASK_PT_animation, Panel):
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

        col = layout.column()
        col.template_movieclip(sc, "clip", compact=True)
        col.template_movieclip_information(sc, "clip", sc.clip_user)


class CLIP_PT_animation(CLIP_PT_clip_view_panel, Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'UI'
    bl_category = "Footage"
    bl_label = "Animation"

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        sc = context.space_data
        clip = sc.clip

        col = layout.column(align=True)
        anim.draw_action_and_slot_selector_for_id(col, clip)


class CLIP_PT_tools_scenesetup(Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'TOOLS'
    bl_label = "Scene Setup"
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
    bl_category = "View"
    bl_options = set()

    # NOTE: this is just a wrapper around the generic GP Panel
    # But, this should only be visible in "clip" view


# Grease Pencil drawing tools
class CLIP_PT_tools_grease_pencil_draw(AnnotationDrawingToolsPanel, Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'TOOLS'


class CLIP_MT_view_zoom(Menu):
    bl_label = "Zoom"

    def draw(self, context):
        layout = self.layout
        from math import isclose

        current_zoom = context.space_data.zoom_percentage
        ratios = ((1, 8), (1, 4), (1, 2), (1, 1), (2, 1), (4, 1), (8, 1))

        for (a, b) in ratios:
            ratio = a / b
            percent = ratio * 100.0
            layout.operator(
                "clip.view_zoom_ratio",
                text="{:g}% ({:d}:{:d})".format(percent, a, b),
                translate=False,
                icon='LAYER_ACTIVE' if isclose(percent, current_zoom, abs_tol=0.5) else 'NONE',
            ).ratio = ratio

        layout.separator()
        layout.operator("clip.view_zoom_in")
        layout.operator("clip.view_zoom_out")
        layout.operator("clip.view_all", text="Zoom to Fit").fit_view = True


class CLIP_MT_view(Menu):
    bl_label = "View"

    def draw(self, context):
        layout = self.layout

        sc = context.space_data

        if sc.view == 'CLIP':
            layout.prop(sc, "show_region_toolbar")
            layout.prop(sc, "show_region_ui")
            layout.prop(sc, "show_region_hud")
            layout.separator()

            layout.prop(sc, "show_metadata")
            layout.separator()

            layout.operator("clip.view_all")
            layout.operator("clip.view_selected")
            layout.operator("clip.view_center_cursor")

            layout.menu("CLIP_MT_view_zoom")
        else:
            layout.operator_context = 'INVOKE_REGION_PREVIEW'
            layout.operator("clip.graph_view_all")
            if sc.view == 'GRAPH':
                layout.operator("clip.graph_center_current_frame")

            layout.operator_context = 'INVOKE_DEFAULT'

            layout.separator()
            layout.prop(sc, "show_seconds")
            layout.prop(sc, "show_locked_time")

        layout.separator()
        layout.menu("INFO_MT_area")


class CLIP_MT_clip(Menu):
    bl_label = "Clip"
    bl_translation_context = i18n_contexts.id_movieclip

    def draw(self, context):
        layout = self.layout

        sc = context.space_data
        clip = sc.clip

        layout.operator("clip.open", text="Open...", icon='FILE_FOLDER')

        if clip:
            layout.operator("clip.set_scene_frames")
            layout.operator("clip.prefetch")
            layout.operator("clip.reload")
            layout.menu("CLIP_MT_proxy")

            layout.separator()

            layout.operator("clip.set_viewport_background")
            layout.operator("clip.setup_tracking_scene")


class CLIP_MT_proxy(Menu):
    bl_label = "Proxy"

    def draw(self, _context):
        layout = self.layout

        layout.operator("clip.rebuild_proxy")
        layout.operator("clip.delete_proxy")


class CLIP_MT_track_transform(Menu):
    bl_label = "Transform"

    def draw(self, _context):
        layout = self.layout

        layout.operator("transform.translate")
        layout.operator("transform.rotate")
        layout.operator("transform.resize")


class CLIP_MT_track_motion(Menu):
    bl_label = "Track Motion"

    def draw(self, _context):
        layout = self.layout

        props = layout.operator("clip.track_markers", text="Backwards")
        props.backwards = True
        props.sequence = True

        props = layout.operator("clip.track_markers", text="Frame Backwards")
        props.backwards = True
        props.sequence = False

        props = layout.operator("clip.track_markers", text="Forwards")
        props.backwards = False
        props.sequence = True

        props = layout.operator("clip.track_markers", text="Frame Forwards")
        props.backwards = False
        props.sequence = False


class CLIP_MT_track_clear(Menu):
    bl_label = "Clear"

    def draw(self, _context):
        layout = self.layout

        props = layout.operator("clip.clear_track_path", text="Before")
        props.clear_active = False
        props.action = 'UPTO'

        props = layout.operator("clip.clear_track_path", text="After")
        props.clear_active = False
        props.action = 'REMAINED'

        props = layout.operator("clip.clear_track_path", text="Track Path")
        props.clear_active = False
        props.action = 'ALL'

        layout.separator()

        layout.operator("clip.clear_solution", text="Solution")


class CLIP_MT_track_refine(Menu):
    bl_label = "Refine"

    def draw(self, _context):
        layout = self.layout

        props = layout.operator("clip.refine_markers", text="Backwards")
        props.backwards = True

        props = layout.operator("clip.refine_markers", text="Forwards")
        props.backwards = False


class CLIP_MT_track_animation(Menu):
    bl_label = "Animation"

    def draw(self, _context):
        layout = self.layout

        layout.operator("clip.keyframe_insert")
        layout.operator("clip.keyframe_delete")


class CLIP_MT_track_visibility(Menu):
    bl_label = "Show/Hide"

    def draw(self, _context):
        layout = self.layout

        layout.operator("clip.hide_tracks_clear")
        layout.operator("clip.hide_tracks", text="Hide Selected").unselected = False
        layout.operator("clip.hide_tracks", text="Hide Unselected").unselected = True


class CLIP_MT_track_cleanup(Menu):
    bl_label = "Clean Up"

    def draw(self, _context):
        layout = self.layout

        layout.operator("clip.clean_tracks")
        layout.operator("clip.filter_tracks")


class CLIP_MT_track(Menu):
    bl_label = "Track"

    def draw(self, context):
        layout = self.layout

        clip = context.space_data.clip
        tracking_object = clip.tracking.objects.active

        layout.menu("CLIP_MT_track_transform")
        layout.menu("CLIP_MT_track_motion")
        layout.menu("CLIP_MT_track_clear")
        layout.menu("CLIP_MT_track_refine")

        layout.separator()

        layout.operator("clip.add_marker_move", text="Add Marker")
        layout.operator("clip.detect_features")
        layout.operator("clip.create_plane_track")

        layout.separator()
        layout.operator("clip.new_image_from_plane_marker")
        layout.operator("clip.update_image_from_plane_marker")

        layout.separator()

        layout.operator(
            "clip.solve_camera",
            text=(
                "Solve Camera Motion" if tracking_object.is_camera else
                "Solve Object Motion"
            ),
        )

        layout.separator()

        layout.operator("clip.join_tracks")
        layout.operator("clip.average_tracks")

        layout.separator()

        layout.operator("clip.copy_tracks", icon='COPYDOWN')
        layout.operator("clip.paste_tracks", icon='PASTEDOWN')

        layout.separator()

        layout.operator("clip.track_settings_as_default", text="Copy Settings to Defaults")
        layout.operator("clip.track_settings_to_track", text="Apply Default Settings")

        layout.separator()

        layout.menu("CLIP_MT_track_animation")

        layout.separator()

        layout.menu("CLIP_MT_track_visibility")
        layout.menu("CLIP_MT_track_cleanup")

        layout.separator()

        layout.operator("clip.delete_track")
        layout.operator("clip.delete_marker")


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
        layout.operator("clip.apply_solution_scale")

        layout.separator()

        layout.operator("clip.track_to_empty")
        layout.operator("clip.bundles_to_mesh")


class CLIP_MT_select_grouped(Menu):
    bl_label = "Select Grouped"

    def draw(self, _context):
        layout = self.layout

        layout.operator_enum("clip.select_grouped", "group")


class CLIP_MT_select(Menu):
    bl_label = "Select"

    def draw(self, _context):
        layout = self.layout

        layout.operator("clip.select_all", text="All").action = 'SELECT'
        layout.operator("clip.select_all", text="None").action = 'DESELECT'
        layout.operator("clip.select_all", text="Inverse").action = 'INVERT'

        layout.separator()

        layout.operator("clip.select_box")
        layout.operator("clip.select_circle")
        layout.operator_menu_enum("clip.select_lasso", "mode")

        layout.separator()

        layout.menu("CLIP_MT_select_grouped")

        layout.separator()

        layout.operator("clip.stabilize_2d_select", text="Stabilization Tracks")
        layout.operator("clip.stabilize_2d_rotation_select", text="Stabilization Rotation Tracks")


class CLIP_MT_select_graph(Menu):
    bl_label = "Select"

    def draw(self, _context):
        layout = self.layout

        layout.operator("clip.graph_select_all_markers", text="All").action = 'SELECT'
        layout.operator("clip.graph_select_all_markers", text="None").action = 'DESELECT'
        layout.operator("clip.graph_select_all_markers", text="Invert").action = 'INVERT'


class CLIP_MT_tracking_context_menu(Menu):
    bl_label = "Context Menu"

    @classmethod
    def poll(cls, context):
        return context.space_data.clip

    def draw(self, context):
        layout = self.layout

        mode = context.space_data.mode

        if mode == 'TRACKING':

            layout.operator("clip.track_settings_to_track")
            layout.operator("clip.track_settings_as_default")

            layout.separator()

            layout.operator("clip.track_copy_color")

            layout.separator()

            layout.operator("clip.copy_tracks", icon='COPYDOWN')
            layout.operator("clip.paste_tracks", icon='PASTEDOWN')

            layout.separator()

            layout.operator("clip.disable_markers", text="Disable Markers").action = 'DISABLE'
            layout.operator("clip.disable_markers", text="Enable Markers").action = 'ENABLE'

            layout.separator()

            layout.operator("clip.hide_tracks")
            layout.operator("clip.hide_tracks_clear", text="Show Tracks")

            layout.separator()

            layout.operator("clip.lock_tracks", text="Lock Tracks").action = 'LOCK'
            layout.operator("clip.lock_tracks", text="Unlock Tracks").action = 'UNLOCK'

            layout.separator()

            layout.operator("clip.join_tracks")
            layout.operator("clip.average_tracks")

            layout.separator()

            layout.operator("clip.delete_track")

        elif mode == 'MASK':
            from .properties_mask_common import draw_mask_context_menu
            draw_mask_context_menu(layout, context)


class CLIP_MT_plane_track_image_context_menu(Menu):
    bl_label = "Plane Track Image Specials"

    def draw(self, _context):
        layout = self.layout

        layout.operator("clip.new_image_from_plane_marker")
        layout.operator("clip.update_image_from_plane_marker")


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
        props = pie.operator("wm.context_set_enum", text="Location")
        props.data_path = "space_data.clip.tracking.tracks.active.motion_model"
        props.value = "Loc"
        # Use Affine Tracking
        props = pie.operator("wm.context_set_enum", text="Affine")
        props.data_path = "space_data.clip.tracking.tracks.active.motion_model"
        props.value = "Affine"
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
            props = pie.operator("wm.context_set_enum", text="Match Previous", icon='KEYFRAME_HLT')
            props.data_path = "space_data.clip.tracking.tracks.active.pattern_match"
            props.value = 'PREV_FRAME'
            # Match Previous Frame
            props = pie.operator("wm.context_set_enum", text="Match Keyframe", icon='KEYFRAME')
            props.data_path = "space_data.clip.tracking.tracks.active.pattern_match"
            props.value = 'KEYFRAME'


class CLIP_MT_tracking_pie(Menu):
    # Tracking Operators
    bl_label = "Tracking"
    bl_translation_context = i18n_contexts.id_movieclip

    @classmethod
    def poll(cls, context):
        space = context.space_data
        return space.mode == 'TRACKING' and space.clip

    def draw(self, _context):
        layout = self.layout

        pie = layout.menu_pie()
        # Track Backwards
        props = pie.operator("clip.track_markers", icon='TRACKING_BACKWARDS')
        props.backwards = True
        props.sequence = True
        # Track Forwards
        props = pie.operator("clip.track_markers", icon='TRACKING_FORWARDS')
        props.backwards = False
        props.sequence = True
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
        props = pie.operator("clip.clean_tracks", icon='X')
        props.frames = 15
        props.error = 2
        # Filter Tracks
        pie.operator("clip.filter_tracks", icon='FILTER')


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


class CLIP_MT_view_pie(Menu):
    bl_label = "View"

    @classmethod
    def poll(cls, context):
        space = context.space_data

        # View operators are not yet implemented in Dope-sheet mode.
        return space.view != 'DOPESHEET'

    def draw(self, context):
        layout = self.layout
        sc = context.space_data

        pie = layout.menu_pie()

        if sc.view == 'CLIP':
            pie.operator("clip.view_all")
            pie.operator("clip.view_selected", icon='ZOOM_SELECTED')

            if sc.mode == 'MASK':
                pie.operator("clip.view_center_cursor")
                pie.separator()
            else:
                # Add spaces so items stay in the same position through all modes.
                pie.separator()
                pie.separator()

            pie.operator("clip.view_all", text="Frame All Fit").fit_view = True

        if sc.view == 'GRAPH':
            pie.operator_context = 'INVOKE_REGION_PREVIEW'
            pie.operator("clip.graph_view_all")
            pie.separator()
            pie.operator("clip.graph_center_current_frame")


class CLIP_PT_overlay_display(Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'HEADER'
    bl_label = "Marker Display"
    bl_parent_id = "CLIP_PT_overlay"

    def draw(self, context):
        layout = self.layout

        view = context.space_data

        row = layout.row()

        col = row.column()
        col.prop(view, "show_marker_pattern", text="Pattern")
        col.prop(view, "show_marker_search", text="Search")

        col.prop(view, "show_track_path", text="Path")
        col = col.column()
        col.active = view.show_track_path
        col.prop(view, "path_length", text="Length")

        col = row.column()
        col.prop(view, "show_disabled", text="Show Disabled")
        col.prop(view, "show_names", text="Info")

        if view.mode != 'MASK':
            col.prop(view, "show_bundles", text="3D Markers")
        col.prop(view, "show_tiny_markers", text="Display Thin")


class CLIP_PT_overlay_mask(MASK_PT_display, Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'HEADER'
    bl_parent_id = "CLIP_PT_overlay"

    @classmethod
    def poll(cls, context):
        sc = context.space_data

        return sc.mode == 'MASK'


class CLIP_PT_overlay_guides(Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'HEADER'
    bl_parent_id = "CLIP_PT_overlay"
    bl_label = "Guides"

    def draw(self, context):
        layout = self.layout
        sc = context.space_data
        overlay = sc.overlay

        col = layout.column()
        col.prop(sc, "show_grid", text="Distortion Grid")
        col.prop(overlay, "show_cursor", text="Cursor")
        col.prop(sc, "show_annotation", text="Annotations")


class CLIP_PT_overlay(Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'HEADER'
    bl_label = "Overlays"
    bl_ui_units_x = 13

    def draw(self, _context):
        layout = self.layout
        layout.label(text="Viewport Overlays")


class CLIP_PT_gizmo_display(Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'HEADER'
    bl_label = "Gizmos"
    bl_ui_units_x = 8

    def draw(self, context):
        layout = self.layout

        view = context.space_data

        col = layout.column()
        col.label(text="Viewport Gizmos")
        col.separator()

        col.active = view.show_gizmo
        colsub = col.column()
        colsub.prop(view, "show_gizmo_navigate", text="Navigate")


classes = (
    CLIP_UL_tracking_objects,
    CLIP_PT_proportional_edit,
    CLIP_HT_header,
    CLIP_PT_display,
    CLIP_PT_clip_display,
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
    CLIP_PT_animation,
    CLIP_PT_stabilization,
    CLIP_PT_2d_cursor,
    CLIP_PT_mask,
    CLIP_PT_mask_layers,
    CLIP_PT_active_mask_spline,
    CLIP_PT_active_mask_point,
    CLIP_PT_mask_animation,
    CLIP_PT_tools_mask_transforms,
    CLIP_PT_tools_mask_tools,
    CLIP_PT_tools_scenesetup,
    CLIP_PT_annotation,
    CLIP_PT_tools_grease_pencil_draw,
    CLIP_MT_view_zoom,
    CLIP_MT_view,
    CLIP_MT_clip,
    CLIP_MT_proxy,
    CLIP_MT_reconstruction,
    CLIP_MT_track,
    CLIP_MT_track_transform,
    CLIP_MT_track_motion,
    CLIP_MT_track_clear,
    CLIP_MT_track_refine,
    CLIP_MT_track_animation,
    CLIP_MT_track_visibility,
    CLIP_MT_track_cleanup,
    CLIP_MT_select,
    CLIP_MT_select_graph,
    CLIP_MT_select_grouped,
    CLIP_MT_tracking_context_menu,
    CLIP_MT_plane_track_image_context_menu,
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
    CLIP_MT_view_pie,
    CLIP_PT_overlay,
    CLIP_PT_overlay_guides,
    CLIP_PT_overlay_mask,
    CLIP_PT_overlay_display,
    CLIP_PT_gizmo_display,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
