# SPDX-FileCopyrightText: 2009-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
from bpy.types import (
    Header,
    Menu,
    Panel,
)

from bl_ui.properties_data_grease_pencil import (
    GreasePencil_LayerMaskPanel,
    GreasePencil_LayerTransformPanel,
    GreasePencil_LayerRelationsPanel,
    GreasePencil_LayerAdjustmentsPanel,
    GreasePencil_LayerDisplayPanel,
)
from bl_ui.space_time import playback_controls
from bl_ui.properties_data_mesh import draw_shape_key_properties

from rna_prop_ui import PropertyPanel

#######################################
# DopeSheet Filtering - Header Buttons

# used for DopeSheet, NLA, and Graph Editors


def dopesheet_filter(layout, context):
    dopesheet = context.space_data.dopesheet
    is_nla = context.area.type == 'NLA_EDITOR'
    is_action_editor = not is_nla and context.space_data.mode == 'ACTION'

    row = layout.row(align=True)
    if is_action_editor:
        row.prop(dopesheet, "show_only_slot_of_active_object", text="")
    row.prop(dopesheet, "show_only_selected", text="")
    row.prop(dopesheet, "show_hidden", text="")

    if is_nla:
        row.prop(dopesheet, "show_missing_nla", text="")
    else:  # graph and dopesheet editors - F-Curves and drivers only
        row.prop(dopesheet, "show_only_errors", text="")

#######################################
# Dope-sheet Filtering Popovers

# Generic Layout - Used as base for filtering popovers used in all animation editors
# Used for DopeSheet, NLA, and Graph Editors


class DopesheetFilterPopoverBase:
    bl_region_type = 'HEADER'
    bl_label = "Filters"

    # Generic = Affects all data-types.
    # XXX: Perhaps we want these to stay in the header instead, for easy/fast access
    @classmethod
    def draw_generic_filters(cls, context, layout):
        dopesheet = context.space_data.dopesheet
        is_nla = context.area.type == 'NLA_EDITOR'

        col = layout.column(align=True)
        col.prop(dopesheet, "show_only_selected", icon='NONE')
        col.prop(dopesheet, "show_hidden", icon='NONE')

        if is_nla:
            col.prop(dopesheet, "show_missing_nla", icon='NONE')
        else:  # Graph and dope-sheet editors - F-Curves and drivers only.
            col.prop(dopesheet, "show_only_errors", icon='NONE')

    # Name/Membership Filters
    # XXX: Perhaps these should just stay in the headers (exclusively)?
    @classmethod
    def draw_search_filters(cls, context, layout, generic_filters_only=False):
        dopesheet = context.space_data.dopesheet
        is_nla = context.area.type == 'NLA_EDITOR'

        col = layout.column(align=True)
        if not is_nla:
            row = col.row(align=True)
            row.prop(dopesheet, "filter_fcurve_name", text="")
        else:
            row = col.row(align=True)
            row.prop(dopesheet, "filter_text", text="")

        if (not generic_filters_only) and bpy.data.collections:
            col = layout.column(align=True)
            col.prop(dopesheet, "filter_collection", text="")

    # Standard = Present in all panels
    @classmethod
    def draw_standard_filters(cls, context, layout):
        dopesheet = context.space_data.dopesheet

        # datablock filters
        layout.label(text="Filter by Type:")
        flow = layout.grid_flow(row_major=True, columns=2, even_rows=False, align=False)

        flow.prop(dopesheet, "show_scenes", text="Scenes")
        flow.prop(dopesheet, "show_nodes", text="Node Trees")

        # object types
        if bpy.data.armatures:
            flow.prop(dopesheet, "show_armatures", text="Armatures")
        if bpy.data.cameras:
            flow.prop(dopesheet, "show_cameras", text="Cameras")
        if bpy.data.grease_pencils:
            flow.prop(dopesheet, "show_gpencil", text="Grease Pencil Objects")
        if bpy.data.lights:
            flow.prop(dopesheet, "show_lights", text="Lights")
        if bpy.data.meshes:
            flow.prop(dopesheet, "show_meshes", text="Meshes")
        if bpy.data.curves:
            flow.prop(dopesheet, "show_curves", text="Curves")
        if bpy.data.lattices:
            flow.prop(dopesheet, "show_lattices", text="Lattices")
        if bpy.data.metaballs:
            flow.prop(dopesheet, "show_metaballs", text="Metaballs")
        if hasattr(bpy.data, "hair_curves") and bpy.data.hair_curves:
            flow.prop(dopesheet, "show_hair_curves", text="Hair Curves")
        if hasattr(bpy.data, "pointclouds") and bpy.data.pointclouds:
            flow.prop(dopesheet, "show_pointclouds", text="Point Clouds")
        if bpy.data.volumes:
            flow.prop(dopesheet, "show_volumes", text="Volumes")
        if bpy.data.lightprobes:
            flow.prop(dopesheet, "show_lightprobes", text="Light Probes")

        # data types
        flow.prop(dopesheet, "show_worlds", text="Worlds")
        if bpy.data.particles:
            flow.prop(dopesheet, "show_particles", text="Particles")
        if bpy.data.linestyles:
            flow.prop(dopesheet, "show_linestyles", text="Line Styles")
        if bpy.data.speakers:
            flow.prop(dopesheet, "show_speakers", text="Speakers")
        if bpy.data.materials:
            flow.prop(dopesheet, "show_materials", text="Materials")
        if bpy.data.textures:
            flow.prop(dopesheet, "show_textures", text="Textures")
        if bpy.data.shape_keys:
            flow.prop(dopesheet, "show_shapekeys", text="Shape Keys")
        if bpy.data.cache_files:
            flow.prop(dopesheet, "show_cache_files", text="Cache Files")
        if bpy.data.movieclips:
            flow.prop(dopesheet, "show_movieclips", text="Movie Clips")

        layout.separator()

        # Object Data Filters

        # TODO: Add per-channel/axis convenience toggles?
        split = layout.split()

        col = split.column()
        col.prop(dopesheet, "show_transforms", text="Transforms")

        col = split.column()
        col.prop(dopesheet, "show_modifiers", text="Modifiers")

        layout.separator()

        # performance-related options (users will mostly have these enabled)
        col = layout.column(align=True)
        col.label(text="Options:")
        col.prop(dopesheet, "use_datablock_sort", icon='NONE')


# Popover for Dope-sheet Editor(s) - Dope-sheet, Action, Shape-key, GPencil, Mask, etc.
class DOPESHEET_PT_filters(DopesheetFilterPopoverBase, Panel):
    bl_space_type = 'DOPESHEET_EDITOR'
    bl_region_type = 'HEADER'
    bl_label = "Filters"

    def draw(self, context):
        layout = self.layout

        dopesheet = context.space_data.dopesheet
        ds_mode = context.space_data.mode

        layout.prop(dopesheet, "show_summary", text="Summary")

        DopesheetFilterPopoverBase.draw_generic_filters(context, layout)

        if ds_mode in {'DOPESHEET', 'ACTION', 'GPENCIL'}:
            layout.separator()
            generic_filters_only = ds_mode != 'DOPESHEET'
            DopesheetFilterPopoverBase.draw_search_filters(context, layout, generic_filters_only=generic_filters_only)

        if ds_mode == 'DOPESHEET':
            layout.separator()
            DopesheetFilterPopoverBase.draw_standard_filters(context, layout)


#######################################
# DopeSheet Editor - General/Standard UI

class DOPESHEET_HT_header(Header):
    bl_space_type = 'DOPESHEET_EDITOR'

    def draw(self, context):
        layout = self.layout

        st = context.space_data

        layout.template_header()

        if st.mode != 'TIMELINE':
            # Timeline mode is special, as it's presented as a sub-type of the
            # dope sheet editor, rather than a mode. So this shouldn't show the
            # mode selector.
            layout.prop(st, "ui_mode", text="")

        DOPESHEET_MT_editor_menus.draw_collapsible(context, layout)
        DOPESHEET_HT_editor_buttons.draw_header(context, layout)


# Header for "normal" dopesheet editor modes (e.g. Dope Sheet, Action, Shape Keys, etc.)
class DOPESHEET_HT_editor_buttons:

    @classmethod
    def draw_header(cls, context, layout):
        st = context.space_data

        if st.mode == 'TIMELINE':
            playback_controls(layout, context)
            layout.separator()
            cls._draw_overlay_selector(context, layout)
            return

        if st.mode in {'ACTION', 'SHAPEKEY'} and context.object:
            layout.separator_spacer()
            cls._draw_action_selector(context, layout)

        # Layer management
        if st.mode == 'GPENCIL':
            ob = context.active_object

            enable_but = ob is not None and ob.type == 'GREASEPENCIL'

            row = layout.row(align=True)
            row.enabled = enable_but
            row.operator("grease_pencil.layer_add", icon='ADD', text="")
            row.operator("grease_pencil.layer_remove", icon='REMOVE', text="")
            row.menu("GREASE_PENCIL_MT_grease_pencil_add_layer_extra", icon='DOWNARROW_HLT', text="")

            row = layout.row(align=True)
            row.enabled = enable_but
            row.operator("anim.channels_move", icon='TRIA_UP', text="").direction = 'UP'
            row.operator("anim.channels_move", icon='TRIA_DOWN', text="").direction = 'DOWN'

            row = layout.row(align=True)
            row.enabled = enable_but
            row.operator("grease_pencil.layer_isolate", icon='RESTRICT_VIEW_ON', text="").affect_visibility = True
            row.operator("grease_pencil.layer_isolate", icon='LOCKED', text="").affect_visibility = False

        layout.separator_spacer()

        if st.mode == 'DOPESHEET':
            dopesheet_filter(layout, context)
        elif st.mode == 'ACTION':
            dopesheet_filter(layout, context)
        elif st.mode == 'GPENCIL':
            row = layout.row(align=True)
            row.prop(st.dopesheet, "show_only_selected", text="")
            row.prop(st.dopesheet, "show_hidden", text="")

        layout.popover(
            panel="DOPESHEET_PT_filters",
            text="",
            icon='FILTER',
        )

        tool_settings = context.tool_settings

        # Grease Pencil mode doesn't need snapping, as it's frame-aligned only
        if st.mode != 'GPENCIL':
            row = layout.row(align=True)
            row.prop(tool_settings, "use_snap_anim", text="")
            sub = row.row(align=True)
            sub.popover(
                panel="DOPESHEET_PT_snapping",
                text="",
            )

        row = layout.row(align=True)
        row.prop(tool_settings, "use_proportional_action", text="", icon_only=True)
        sub = row.row(align=True)
        sub.active = tool_settings.use_proportional_action
        sub.prop_with_popover(
            tool_settings,
            "proportional_edit_falloff",
            text="",
            icon_only=True,
            panel="DOPESHEET_PT_proportional_edit",
        )

        cls._draw_overlay_selector(context, layout)

    @classmethod
    def _draw_overlay_selector(cls, context, layout):
        st = context.space_data

        overlays = st.overlays
        row = layout.row(align=True)
        row.prop(overlays, "show_overlays", text="", icon='OVERLAY')
        sub = row.row(align=True)
        sub.popover(panel="DOPESHEET_PT_overlay", text="")
        sub.active = overlays.show_overlays

    @classmethod
    def _draw_action_selector(cls, context, layout):
        animated_id = cls._get_animated_id(context)
        if not animated_id:
            return

        row = layout.row()
        if animated_id.animation_data and animated_id.animation_data.use_tweak_mode:
            row.enabled = False

        row.template_action(animated_id, new="action.new", unlink="action.unlink")

        adt = animated_id and animated_id.animation_data
        if not adt or not adt.action or not adt.action.is_action_layered:
            return

        # Store the animated ID in the context, so that the new/unlink operators
        # have access to it.
        row.context_pointer_set("animated_id", animated_id)
        row.template_search(
            adt, "action_slot",
            adt, "action_suitable_slots",
            new="anim.slot_new_for_id",
            unlink="anim.slot_unassign_from_id",
        )

    @staticmethod
    def _get_animated_id(context):
        st = context.space_data
        match st.mode:
            case 'ACTION':
                return context.object
            case 'SHAPEKEY':
                return getattr(context.object.data, "shape_keys", None)
            case _:
                print("Dope Sheet mode '{:s}' not expected to have an Action selector".format(st.mode))
                return context.object


class DOPESHEET_HT_playback_controls(Header):
    bl_space_type = 'DOPESHEET_EDITOR'
    bl_region_type = 'FOOTER'

    def draw(self, context):
        layout = self.layout

        playback_controls(layout, context)


class DOPESHEET_PT_snapping(Panel):
    bl_space_type = 'DOPESHEET_EDITOR'
    bl_region_type = 'HEADER'
    bl_label = "Snapping"

    def draw(self, context):
        layout = self.layout
        col = layout.column()
        col.label(text="Snap To")
        tool_settings = context.tool_settings
        col.prop(tool_settings, "snap_anim_element", expand=True)
        if tool_settings.snap_anim_element != 'MARKER':
            col.prop(tool_settings, "use_snap_time_absolute")


class DOPESHEET_PT_proportional_edit(Panel):
    bl_space_type = 'DOPESHEET_EDITOR'
    bl_region_type = 'HEADER'
    bl_label = "Proportional Editing"
    bl_ui_units_x = 8

    def draw(self, context):
        layout = self.layout
        tool_settings = context.tool_settings
        col = layout.column()
        col.active = tool_settings.use_proportional_action

        col.prop(tool_settings, "proportional_edit_falloff", expand=True)
        col.prop(tool_settings, "proportional_size")


class DOPESHEET_MT_editor_menus(Menu):
    bl_idname = "DOPESHEET_MT_editor_menus"
    bl_label = ""

    def draw(self, context):
        layout = self.layout
        st = context.space_data
        active_action = context.active_action

        if st.mode == 'TIMELINE':
            # Draw the 'timeline' menus, which are simpler. Most importantly, the
            # 'selected only' toggle is in the menu, and actually stored as a scene
            # flag instead of the space data.
            horizontal = (layout.direction == 'VERTICAL')
            if horizontal:
                row = layout.row()
                sub = row.row(align=True)
            else:
                sub = layout
            sub.menu("TIME_MT_view")
            if st.show_markers:
                sub.menu("DOPESHEET_MT_marker")
            return

        layout.menu("DOPESHEET_MT_view")
        layout.menu("DOPESHEET_MT_select")
        if st.show_markers:
            layout.menu("DOPESHEET_MT_marker")

        if st.mode == 'DOPESHEET' or (st.mode == 'ACTION' and active_action is not None):
            layout.menu("DOPESHEET_MT_channel")
        elif st.mode == 'GPENCIL':
            layout.menu("DOPESHEET_MT_gpencil_channel")

        layout.menu("DOPESHEET_MT_key")

        if st.mode in {'ACTION', 'SHAPEKEY'} and active_action is not None:
            layout.menu("DOPESHEET_MT_action")


class DOPESHEET_MT_view(Menu):
    bl_label = "View"

    def draw(self, context):
        layout = self.layout

        st = context.space_data

        layout.prop(st, "show_region_ui")
        layout.prop(st, "show_region_hud")
        layout.prop(st, "show_region_channels")
        layout.prop(st, "show_region_footer", text="Playback Controls")
        layout.separator()

        layout.operator("action.view_selected")
        layout.operator("action.view_all")
        if context.scene.use_preview_range:
            layout.operator("anim.scene_range_frame", text="Frame Preview Range")
        else:
            layout.operator("anim.scene_range_frame", text="Frame Scene Range")
        layout.operator("action.view_frame")
        layout.separator()

        layout.prop(st.dopesheet, "use_multi_word_filter", text="Multi-Word Match Search")
        layout.separator()

        layout.prop(st, "use_realtime_update")

        # Sliders are always shown in the Shape Key Editor regardless of this setting.
        col = layout.column()
        col.active = context.space_data.mode != 'SHAPEKEY'
        col.prop(st, "show_sliders")

        layout.prop(st, "show_interpolation")
        layout.prop(st, "show_extremes")
        layout.prop(st, "use_auto_merge_keyframes")
        layout.separator()

        layout.prop(st, "show_markers")
        layout.prop(st, "show_seconds")
        layout.prop(st, "show_locked_time")
        layout.separator()

        layout.operator("anim.previewrange_set")
        layout.operator("anim.previewrange_clear")
        layout.operator("action.previewrange_set")
        layout.separator()

        # Add this to show key-binding (reverse action in dope-sheet).
        props = layout.operator("wm.context_set_enum", text="Toggle Graph Editor", icon='GRAPH')
        props.data_path = "area.type"
        props.value = 'GRAPH_EDITOR'
        layout.separator()

        layout.menu("DOPESHEET_MT_cache")
        layout.separator()

        layout.menu("INFO_MT_area")


class DOPESHEET_MT_view_pie(Menu):
    bl_label = "View"

    def draw(self, context):
        layout = self.layout

        pie = layout.menu_pie()
        pie.operator("action.view_all")
        pie.operator("action.view_selected", icon='ZOOM_SELECTED')
        pie.operator("action.view_frame")
        if context.scene.use_preview_range:
            pie.operator("anim.scene_range_frame", text="Frame Preview Range")
        else:
            pie.operator("anim.scene_range_frame", text="Frame Scene Range")


class DOPESHEET_MT_cache(Menu):
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
        col.prop(st, "cache_simulation_nodes")
        col.prop(st, "cache_smoke")
        col.prop(st, "cache_dynamicpaint")
        col.prop(st, "cache_rigidbody")


class DOPESHEET_MT_select(Menu):
    bl_label = "Select"

    def draw(self, context):
        layout = self.layout

        layout.operator("action.select_all", text="All").action = 'SELECT'
        layout.operator("action.select_all", text="None").action = 'DESELECT'
        layout.operator("action.select_all", text="Invert").action = 'INVERT'

        layout.separator()
        layout.operator("action.select_box").axis_range = False
        layout.operator("action.select_box", text="Box Select (Axis Range)").axis_range = True
        layout.operator("action.select_circle")
        layout.operator_menu_enum("action.select_lasso", "mode")

        # FIXME: grease pencil mode isn't supported for these yet, so skip for that mode only
        if context.space_data.mode != 'GPENCIL':
            layout.separator()
            layout.operator("action.select_more", text="More")
            layout.operator("action.select_less", text="Less")

            layout.separator()
            layout.operator("action.select_linked")

        layout.separator()
        layout.operator("action.select_column", text="Columns on Selected Keys").mode = 'KEYS'
        layout.operator("action.select_column", text="Column on Current Frame").mode = 'CFRA'

        layout.operator("action.select_column", text="Columns on Selected Markers").mode = 'MARKERS_COLUMN'
        layout.operator("action.select_column", text="Between Selected Markers").mode = 'MARKERS_BETWEEN'

        layout.separator()
        props = layout.operator("action.select_leftright", text="Before Current Frame")
        props.extend = False
        props.mode = 'LEFT'
        props = layout.operator("action.select_leftright", text="After Current Frame")
        props.extend = False
        props.mode = 'RIGHT'


class DOPESHEET_MT_marker(Menu):
    bl_label = "Marker"

    def draw(self, context):
        layout = self.layout

        from bl_ui.space_time import marker_menu_generic
        marker_menu_generic(layout, context)

        st = context.space_data

        if st.mode in {'ACTION', 'SHAPEKEY'} and context.active_action:
            layout.separator()
            layout.prop(st, "show_pose_markers")

            if st.show_pose_markers is False:
                layout.operator("action.markers_make_local")

        layout.prop(st, "use_marker_sync")

#######################################
# Keyframe Editing


class DOPESHEET_MT_channel(Menu):
    bl_label = "Channel"

    def draw(self, _context):
        layout = self.layout

        layout.operator_context = 'INVOKE_REGION_CHANNELS'

        layout.operator("anim.channels_delete")
        layout.operator("action.clean", text="Clean Channels").channels = True

        layout.separator()
        layout.operator("anim.channels_group")
        layout.operator("anim.channels_ungroup")

        layout.separator()
        layout.operator_menu_enum("anim.channels_setting_toggle", "type")
        layout.operator_menu_enum("anim.channels_setting_enable", "type")
        layout.operator_menu_enum("anim.channels_setting_disable", "type")

        layout.separator()
        layout.operator("anim.channels_editable_toggle")
        layout.operator_menu_enum("action.extrapolation_type", "type", text="Extrapolation Mode")

        layout.separator()
        layout.operator("anim.channels_expand")
        layout.operator("anim.channels_collapse")

        layout.separator()
        layout.operator_menu_enum("anim.channels_move", "direction", text="Move...")

        layout.separator()
        layout.operator("anim.channels_fcurves_enable")

        layout.separator()
        layout.operator("anim.channels_bake")

        layout.separator()
        layout.operator("anim.channels_view_selected")


class DOPESHEET_MT_action(Menu):
    bl_label = "Action"

    def draw(self, context):
        layout = self.layout
        layout.operator("anim.merge_animation")
        layout.operator("anim.separate_slots")

        layout.separator()
        layout.operator("anim.slot_channels_move_to_new_action")

        layout.separator()
        layout.operator("action.push_down", text="Push Down Action", icon='NLA_PUSHDOWN')
        layout.operator("action.stash", text="Stash Action", icon='FREEZE')


class DOPESHEET_MT_key(Menu):
    bl_label = "Key"

    def draw(self, context):
        layout = self.layout
        ob = context.active_object

        layout.menu("DOPESHEET_MT_key_transform", text="Transform")

        layout.operator_menu_enum("action.snap", "type", text="Snap")
        layout.operator_menu_enum("action.mirror", "type", text="Mirror")

        layout.separator()
        layout.operator("action.keyframe_insert")

        layout.separator()
        layout.operator("action.frame_jump")

        layout.separator()
        layout.operator("action.copy")
        layout.operator("action.paste")
        layout.operator("action.paste", text="Paste Flipped").flipped = True
        layout.operator("action.duplicate_move")
        layout.operator("action.delete")
        if ob and ob.type == 'GREASEPENCIL':
            layout.operator("grease_pencil.delete_breakdown")

        layout.separator()
        layout.operator_menu_enum("action.keyframe_type", "type", text="Keyframe Type")
        layout.operator_menu_enum("action.handle_type", "type", text="Handle Type")
        layout.operator_menu_enum("action.interpolation_type", "type", text="Interpolation Mode")
        layout.operator_menu_enum("action.easing_type", "type", text="Easing Mode")

        layout.separator()
        layout.operator("action.clean").channels = False
        layout.operator("action.bake_keys")

        layout.separator()
        layout.operator("graph.euler_filter", text="Discontinuity (Euler) Filter")


class DOPESHEET_MT_key_transform(Menu):
    bl_label = "Transform"

    def draw(self, _context):
        layout = self.layout

        layout.operator("transform.transform", text="Move").mode = 'TIME_TRANSLATE'
        layout.operator("transform.transform", text="Extend").mode = 'TIME_EXTEND'
        layout.operator("transform.transform", text="Slide").mode = 'TIME_SLIDE'
        layout.operator("transform.transform", text="Scale").mode = 'TIME_SCALE'


class DopesheetActionPanelBase:
    bl_region_type = 'UI'
    bl_label = "Action"

    @classmethod
    def draw_generic_panel(cls, _context, layout, action):
        layout.label(text=action.name, icon='ACTION', translate=False)

        layout.prop(action, "use_frame_range")

        col = layout.column()
        col.active = action.use_frame_range

        row = col.row(align=True)
        row.prop(action, "frame_start", text="Start")
        row.prop(action, "frame_end", text="End")

        col.prop(action, "use_cyclic")


class DOPESHEET_PT_custom_props_action(PropertyPanel, Panel):
    bl_space_type = 'DOPESHEET_EDITOR'
    bl_category = "Action"
    bl_region_type = 'UI'
    bl_context = "data"
    _context_path = "active_action"
    _property_type = bpy.types.Action

    @classmethod
    def poll(cls, context):
        return bool(context.active_action)


class DOPESHEET_PT_action(DopesheetActionPanelBase, Panel):
    bl_space_type = 'DOPESHEET_EDITOR'
    bl_category = "Action"

    @classmethod
    def poll(cls, context):
        return bool(context.active_action)

    def draw(self, context):
        action = context.active_action
        self.draw_generic_panel(context, self.layout, action)


class DOPESHEET_PT_action_slot(Panel):
    bl_space_type = 'DOPESHEET_EDITOR'
    bl_region_type = 'UI'
    bl_category = "Action"
    bl_label = "Slot"

    @classmethod
    def poll(cls, context):
        action = context.active_action
        return bool(action and action.slots.active)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        action = context.active_action
        slot = action.slots.active

        layout.prop(slot, "name_display", text="Name")

        # Draw the ID type of the slot.
        try:
            enum_items = slot.bl_rna.properties['target_id_type'].enum_items
            idtype_label = enum_items[slot.target_id_type].name
        except (KeyError, IndexError, AttributeError) as ex:
            idtype_label = str(ex)

        split = layout.split(factor=0.4)
        split.alignment = 'RIGHT'
        split.label(text="Type")
        split.alignment = 'LEFT'

        split.label(text=idtype_label, icon_value=slot.target_id_type_icon)


#######################################
# Grease Pencil Editing

class DOPESHEET_MT_gpencil_channel(Menu):
    bl_label = "Channel"

    def draw(self, _context):
        layout = self.layout

        layout.operator_context = 'INVOKE_REGION_CHANNELS'

        layout.operator("anim.channels_delete")

        layout.separator()
        layout.operator("anim.channels_setting_toggle")
        layout.operator("anim.channels_setting_enable")
        layout.operator("anim.channels_setting_disable")

        layout.separator()
        layout.operator("anim.channels_editable_toggle")

        # XXX: to be enabled when these are ready for use!
        # layout.separator()
        # layout.operator("anim.channels_expand")
        # layout.operator("anim.channels_collapse")

        layout.separator()
        layout.operator_menu_enum("anim.channels_move", "direction", text="Move...")

        layout.separator()
        layout.operator("anim.channels_view_selected")


class DOPESHEET_MT_delete(Menu):
    bl_label = "Delete"

    def draw(self, _context):
        layout = self.layout

        layout.operator("action.delete")

        layout.separator()

        layout.operator("action.clean").channels = False
        layout.operator("action.clean", text="Clean Channels").channels = True


class DOPESHEET_MT_context_menu(Menu):
    bl_label = "Dope Sheet"

    def draw(self, context):
        layout = self.layout
        st = context.space_data

        layout.operator_context = 'INVOKE_DEFAULT'

        layout.operator("action.copy", text="Copy", icon='COPYDOWN')
        layout.operator("action.paste", text="Paste", icon='PASTEDOWN')
        layout.operator("action.paste", text="Paste Flipped", icon='PASTEFLIPDOWN').flipped = True

        layout.separator()

        layout.operator_menu_enum("action.keyframe_type", "type", text="Keyframe Type")

        if st.mode != 'GPENCIL':
            layout.operator_menu_enum("action.handle_type", "type", text="Handle Type")
            layout.operator_menu_enum("action.interpolation_type", "type", text="Interpolation Mode")
            layout.operator_menu_enum("action.easing_type", "type", text="Easing Mode")

        layout.separator()

        layout.operator("action.keyframe_insert").type = 'SEL'
        layout.operator("action.duplicate_move")

        if st.mode == 'GPENCIL':
            layout.separator()
            layout.operator("grease_pencil.delete_breakdown")

        layout.operator_context = 'EXEC_REGION_WIN'
        layout.operator("action.delete")

        layout.separator()

        layout.operator_menu_enum("action.mirror", "type", text="Mirror")
        layout.operator_menu_enum("action.snap", "type", text="Snap")


class DOPESHEET_MT_channel_context_menu(Menu):
    bl_label = "Channel"

    def draw(self, context):
        layout = self.layout

        # This menu is used from the graph editor too.
        is_graph_editor = context.area.type == 'GRAPH_EDITOR'

        layout.operator_context = 'INVOKE_REGION_CHANNELS'

        layout.separator()
        layout.operator("anim.channels_view_selected")

        layout.operator("anim.channels_setting_enable", text="Mute Channels").type = 'MUTE'
        layout.operator("anim.channels_setting_disable", text="Unmute Channels").type = 'MUTE'
        layout.separator()
        layout.operator("anim.channels_setting_enable", text="Protect Channels").type = 'PROTECT'
        layout.operator("anim.channels_setting_disable", text="Unprotect Channels").type = 'PROTECT'

        layout.separator()
        layout.operator("anim.channels_group")
        layout.operator("anim.channels_ungroup")

        layout.separator()
        layout.operator("anim.channels_editable_toggle")

        if is_graph_editor:
            operator = "graph.extrapolation_type"
        else:
            operator = "action.extrapolation_type"
        layout.operator_menu_enum(operator, "type", text="Extrapolation Mode")

        if is_graph_editor:
            layout.operator_menu_enum("graph.fmodifier_add", "type", text="Add F-Curve Modifier").only_active = False
            layout.separator()
            layout.operator("graph.hide", text="Hide Selected Curves").unselected = False
            layout.operator("graph.hide", text="Hide Unselected Curves").unselected = True
            layout.operator("graph.reveal")

        layout.separator()
        layout.operator("anim.channels_expand")
        layout.operator("anim.channels_collapse")

        layout.separator()
        layout.operator_menu_enum("anim.channels_move", "direction", text="Move...")

        layout.separator()

        layout.operator("anim.channels_delete")

        if is_graph_editor and context.space_data.mode == 'DRIVERS':
            layout.operator("graph.driver_delete_invalid")


class DOPESHEET_MT_snap_pie(Menu):
    bl_label = "Snap"

    def draw(self, _context):
        layout = self.layout
        pie = layout.menu_pie()

        pie.operator("action.snap", text="Selection to Current Frame").type = 'CFRA'
        pie.operator("action.snap", text="Selection to Nearest Frame").type = 'NEAREST_FRAME'
        pie.operator("action.snap", text="Selection to Nearest Second").type = 'NEAREST_SECOND'
        pie.operator("action.snap", text="Selection to Nearest Marker").type = 'NEAREST_MARKER'


class GreasePencilLayersDopeSheetPanel:
    bl_space_type = 'DOPESHEET_EDITOR'
    bl_region_type = 'UI'
    bl_category = "View"

    @classmethod
    def poll(cls, context):
        st = context.space_data
        ob = context.object
        if st.mode != 'GPENCIL' or ob is None or ob.type != 'GREASEPENCIL':
            return False

        grease_pencil = ob.data
        active_layer = grease_pencil.layers.active
        if active_layer:
            return True

        return False


class DOPESHEET_PT_grease_pencil_mode(GreasePencilLayersDopeSheetPanel, Panel):
    bl_label = "Layer"

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        ob = context.object
        grease_pencil = ob.data
        active_layer = grease_pencil.layers.active

        if active_layer:
            row = layout.row(align=True)
            row.prop(active_layer, "blend_mode", text="Blend")

            row = layout.row(align=True)
            row.prop(active_layer, "opacity", text="Opacity", slider=True)

            row = layout.row(align=True)
            row.prop(active_layer, "use_lights", text="Lights")


class DOPESHEET_PT_grease_pencil_layer_masks(GreasePencilLayersDopeSheetPanel, GreasePencil_LayerMaskPanel, Panel):
    bl_label = "Masks"
    bl_parent_id = "DOPESHEET_PT_grease_pencil_mode"
    bl_options = {'DEFAULT_CLOSED'}


class DOPESHEET_PT_grease_pencil_layer_transform(
        GreasePencilLayersDopeSheetPanel,
        GreasePencil_LayerTransformPanel,
        Panel,
):
    bl_label = "Transform"
    bl_parent_id = "DOPESHEET_PT_grease_pencil_mode"
    bl_options = {'DEFAULT_CLOSED'}


class DOPESHEET_PT_grease_pencil_layer_relations(
        GreasePencilLayersDopeSheetPanel,
        GreasePencil_LayerRelationsPanel,
        Panel,
):
    bl_label = "Relations"
    bl_parent_id = "DOPESHEET_PT_grease_pencil_mode"
    bl_options = {'DEFAULT_CLOSED'}


class DOPESHEET_PT_grease_pencil_layer_adjustments(
        GreasePencilLayersDopeSheetPanel,
        GreasePencil_LayerAdjustmentsPanel,
        Panel,
):
    bl_label = "Adjustments"
    bl_parent_id = "DOPESHEET_PT_grease_pencil_mode"
    bl_options = {'DEFAULT_CLOSED'}


class DOPESHEET_PT_grease_pencil_layer_display(
        GreasePencilLayersDopeSheetPanel,
        GreasePencil_LayerDisplayPanel,
        Panel,
):
    bl_label = "Display"
    bl_parent_id = "DOPESHEET_PT_grease_pencil_mode"
    bl_options = {'DEFAULT_CLOSED'}


class DOPESHEET_PT_ShapeKey(Panel):
    bl_space_type = 'DOPESHEET_EDITOR'
    bl_region_type = 'UI'
    bl_category = "Shape Key"
    bl_label = "Shape Key"

    @classmethod
    def poll(cls, context):
        st = context.space_data
        if st.mode != 'SHAPEKEY':
            return False

        ob = context.object
        if ob is None or ob.active_shape_key is None:
            return False

        if not ob.data.shape_keys.use_relative:
            return False

        return ob.active_shape_key_index > 0

    def draw(self, context):
        draw_shape_key_properties(context, self.layout)


class DOPESHEET_PT_overlay(Panel):
    bl_space_type = 'DOPESHEET_EDITOR'
    bl_region_type = 'HEADER'
    bl_label = "Overlays"
    bl_ui_units_x = 13

    def draw(self, _context):
        pass


class DOPESHEET_PT_dopesheet_overlay(Panel):
    bl_space_type = 'DOPESHEET_EDITOR'
    bl_region_type = 'HEADER'
    bl_parent_id = "DOPESHEET_PT_overlay"
    bl_label = "Dope Sheet Overlays"

    def draw(self, context):
        st = context.space_data
        overlay_settings = st.overlays
        layout = self.layout

        layout.active = overlay_settings.show_overlays
        row = layout.row()
        row.active = context.workspace.use_scene_time_sync
        row.prop(overlay_settings, "show_scene_strip_range")


classes = (
    DOPESHEET_HT_header,
    DOPESHEET_HT_playback_controls,
    DOPESHEET_PT_proportional_edit,
    DOPESHEET_MT_editor_menus,
    DOPESHEET_MT_view,
    DOPESHEET_MT_cache,
    DOPESHEET_MT_select,
    DOPESHEET_MT_marker,
    DOPESHEET_MT_channel,
    DOPESHEET_MT_action,
    DOPESHEET_MT_key,
    DOPESHEET_MT_key_transform,
    DOPESHEET_MT_gpencil_channel,
    DOPESHEET_MT_delete,
    DOPESHEET_MT_context_menu,
    DOPESHEET_MT_channel_context_menu,
    DOPESHEET_MT_snap_pie,
    DOPESHEET_MT_view_pie,
    DOPESHEET_PT_filters,
    DOPESHEET_PT_action,
    DOPESHEET_PT_action_slot,
    DOPESHEET_PT_custom_props_action,
    DOPESHEET_PT_snapping,
    DOPESHEET_PT_grease_pencil_mode,
    DOPESHEET_PT_grease_pencil_layer_masks,
    DOPESHEET_PT_grease_pencil_layer_transform,
    DOPESHEET_PT_grease_pencil_layer_adjustments,
    DOPESHEET_PT_grease_pencil_layer_relations,
    DOPESHEET_PT_grease_pencil_layer_display,
    DOPESHEET_PT_ShapeKey,

    DOPESHEET_PT_overlay,
    DOPESHEET_PT_dopesheet_overlay,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
