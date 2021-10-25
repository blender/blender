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
from bpy.types import Header, Menu


#######################################
# DopeSheet Filtering

# used for DopeSheet, NLA, and Graph Editors
def dopesheet_filter(layout, context, genericFiltersOnly=False):
    dopesheet = context.space_data.dopesheet
    is_nla = context.area.type == 'NLA_EDITOR'

    row = layout.row(align=True)
    row.prop(dopesheet, "show_only_selected", text="")
    row.prop(dopesheet, "show_hidden", text="")

    if is_nla:
        row.prop(dopesheet, "show_missing_nla", text="")
    else:  # graph and dopesheet editors - F-Curves and drivers only
        row.prop(dopesheet, "show_only_errors", text="")

    if not genericFiltersOnly:
        if bpy.data.groups:
            row = layout.row(align=True)
            row.prop(dopesheet, "show_only_group_objects", text="")
            if dopesheet.show_only_group_objects:
                row.prop(dopesheet, "filter_group", text="")

    if not is_nla:
        row = layout.row(align=True)
        row.prop(dopesheet, "show_only_matching_fcurves", text="")
        if dopesheet.show_only_matching_fcurves:
            row.prop(dopesheet, "filter_fcurve_name", text="")
            row.prop(dopesheet, "use_multi_word_filter", text="")
    else:
        row = layout.row(align=True)
        row.prop(dopesheet, "use_filter_text", text="")
        if dopesheet.use_filter_text:
            row.prop(dopesheet, "filter_text", text="")
            row.prop(dopesheet, "use_multi_word_filter", text="")

    if not genericFiltersOnly:
        row = layout.row(align=True)
        row.prop(dopesheet, "show_datablock_filters", text="Filters")

        if dopesheet.show_datablock_filters:
            row.prop(dopesheet, "show_scenes", text="")
            row.prop(dopesheet, "show_worlds", text="")
            row.prop(dopesheet, "show_nodes", text="")

            row.prop(dopesheet, "show_transforms", text="")

            if bpy.data.meshes:
                row.prop(dopesheet, "show_meshes", text="")
            if bpy.data.shape_keys:
                row.prop(dopesheet, "show_shapekeys", text="")
            if bpy.data.meshes:
                row.prop(dopesheet, "show_modifiers", text="")
            if bpy.data.materials:
                row.prop(dopesheet, "show_materials", text="")
            if bpy.data.lamps:
                row.prop(dopesheet, "show_lamps", text="")
            if bpy.data.textures:
                row.prop(dopesheet, "show_textures", text="")
            if bpy.data.cameras:
                row.prop(dopesheet, "show_cameras", text="")
            if bpy.data.curves:
                row.prop(dopesheet, "show_curves", text="")
            if bpy.data.metaballs:
                row.prop(dopesheet, "show_metaballs", text="")
            if bpy.data.lattices:
                row.prop(dopesheet, "show_lattices", text="")
            if bpy.data.armatures:
                row.prop(dopesheet, "show_armatures", text="")
            if bpy.data.particles:
                row.prop(dopesheet, "show_particles", text="")
            if bpy.data.speakers:
                row.prop(dopesheet, "show_speakers", text="")
            if bpy.data.linestyles:
                row.prop(dopesheet, "show_linestyles", text="")
            if bpy.data.grease_pencil:
                row.prop(dopesheet, "show_gpencil", text="")

            layout.prop(dopesheet, "use_datablock_sort", text="")


#######################################
# DopeSheet Editor - General/Standard UI

class DOPESHEET_HT_header(Header):
    bl_space_type = 'DOPESHEET_EDITOR'

    def draw(self, context):
        layout = self.layout

        st = context.space_data
        toolsettings = context.tool_settings

        row = layout.row(align=True)
        row.template_header()

        DOPESHEET_MT_editor_menus.draw_collapsible(context, layout)

        layout.prop(st, "mode", text="")

        if st.mode in {'ACTION', 'SHAPEKEY'}:
            row = layout.row(align=True)
            row.operator("action.layer_prev", text="", icon='TRIA_DOWN')
            row.operator("action.layer_next", text="", icon='TRIA_UP')

            layout.template_ID(st, "action", new="action.new", unlink="action.unlink")

            row = layout.row(align=True)
            row.operator("action.push_down", text="Push Down", icon='NLA_PUSHDOWN')
            row.operator("action.stash", text="Stash", icon='FREEZE')

        layout.prop(st.dopesheet, "show_summary", text="Summary")

        if st.mode == 'DOPESHEET':
            dopesheet_filter(layout, context)
        elif st.mode == 'ACTION':
            # 'genericFiltersOnly' limits the options to only the relevant 'generic' subset of
            # filters which will work here and are useful (especially for character animation)
            dopesheet_filter(layout, context, genericFiltersOnly=True)
        elif st.mode == 'GPENCIL':
            row = layout.row(align=True)
            row.prop(st.dopesheet, "show_gpencil_3d_only", text="Active Only")

            if st.dopesheet.show_gpencil_3d_only:
                row = layout.row(align=True)
                row.prop(st.dopesheet, "show_only_selected", text="")
                row.prop(st.dopesheet, "show_hidden", text="")

            row = layout.row(align=True)
            row.prop(st.dopesheet, "use_filter_text", text="")
            if st.dopesheet.use_filter_text:
                row.prop(st.dopesheet, "filter_text", text="")
                row.prop(st.dopesheet, "use_multi_word_filter", text="")

        row = layout.row(align=True)
        row.prop(toolsettings, "use_proportional_action",
                 text="", icon_only=True)
        if toolsettings.use_proportional_action:
            row.prop(toolsettings, "proportional_edit_falloff",
                     text="", icon_only=True)

        # Grease Pencil mode doesn't need snapping, as it's frame-aligned only
        if st.mode != 'GPENCIL':
            layout.prop(st, "auto_snap", text="")

        row = layout.row(align=True)
        row.operator("action.copy", text="", icon='COPYDOWN')
        row.operator("action.paste", text="", icon='PASTEDOWN')
        if st.mode not in ('GPENCIL', 'MASK'):
            row.operator("action.paste", text="", icon='PASTEFLIPDOWN').flipped = True


class DOPESHEET_MT_editor_menus(Menu):
    bl_idname = "DOPESHEET_MT_editor_menus"
    bl_label = ""

    def draw(self, context):
        self.draw_menus(self.layout, context)

    @staticmethod
    def draw_menus(layout, context):
        st = context.space_data

        layout.menu("DOPESHEET_MT_view")
        layout.menu("DOPESHEET_MT_select")
        layout.menu("DOPESHEET_MT_marker")

        if st.mode == 'DOPESHEET' or (st.mode == 'ACTION' and st.action is not None):
            layout.menu("DOPESHEET_MT_channel")
        elif st.mode == 'GPENCIL':
            layout.menu("DOPESHEET_MT_gpencil_channel")

        if st.mode != 'GPENCIL':
            layout.menu("DOPESHEET_MT_key")
        else:
            layout.menu("DOPESHEET_MT_gpencil_frame")


class DOPESHEET_MT_view(Menu):
    bl_label = "View"

    def draw(self, context):
        layout = self.layout

        st = context.space_data

        layout.operator("action.properties", icon='MENU_PANEL')
        layout.separator()

        layout.prop(st, "use_realtime_update")
        layout.prop(st, "show_frame_indicator")
        layout.prop(st, "show_sliders")
        layout.prop(st, "show_group_colors")
        layout.prop(st, "use_auto_merge_keyframes")
        layout.prop(st, "use_marker_sync")

        layout.prop(st, "show_seconds")
        layout.prop(st, "show_locked_time")

        layout.separator()
        layout.operator("anim.previewrange_set")
        layout.operator("anim.previewrange_clear")
        layout.operator("action.previewrange_set")

        layout.separator()
        layout.operator("action.view_all")
        layout.operator("action.view_selected")
        layout.operator("action.view_frame")

        layout.separator()
        layout.operator("screen.area_dupli")
        layout.operator("screen.screen_full_area")
        layout.operator("screen.screen_full_area", text="Toggle Fullscreen Area").use_hide_panels = True


class DOPESHEET_MT_select(Menu):
    bl_label = "Select"

    def draw(self, context):
        layout = self.layout

        # This is a bit misleading as the operator's default text is "Select All" while it actually *toggles* All/None
        layout.operator("action.select_all_toggle").invert = False
        layout.operator("action.select_all_toggle", text="Invert Selection").invert = True

        layout.separator()
        layout.operator("action.select_border").axis_range = False
        layout.operator("action.select_border", text="Border Axis Range").axis_range = True

        layout.operator("action.select_circle")

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

        # FIXME: grease pencil mode isn't supported for these yet, so skip for that mode only
        if context.space_data.mode != 'GPENCIL':
            layout.separator()
            layout.operator("action.select_more")
            layout.operator("action.select_less")

            layout.separator()
            layout.operator("action.select_linked")


class DOPESHEET_MT_marker(Menu):
    bl_label = "Marker"

    def draw(self, context):
        layout = self.layout

        from bl_ui.space_time import marker_menu_generic
        marker_menu_generic(layout)

        st = context.space_data

        if st.mode in {'ACTION', 'SHAPEKEY'} and st.action:
            layout.separator()
            layout.prop(st, "show_pose_markers")

            if st.show_pose_markers is False:
                layout.operator("action.markers_make_local")


#######################################
# Keyframe Editing

class DOPESHEET_MT_channel(Menu):
    bl_label = "Channel"

    def draw(self, context):
        layout = self.layout

        layout.operator_context = 'INVOKE_REGION_CHANNELS'

        layout.operator("anim.channels_delete")

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


class DOPESHEET_MT_key(Menu):
    bl_label = "Key"

    def draw(self, context):
        layout = self.layout

        layout.menu("DOPESHEET_MT_key_transform", text="Transform")

        layout.operator_menu_enum("action.snap", "type", text="Snap")
        layout.operator_menu_enum("action.mirror", "type", text="Mirror")

        layout.separator()
        layout.operator("action.keyframe_insert")

        layout.separator()
        layout.operator("action.frame_jump")

        layout.separator()
        layout.operator("action.duplicate_move")
        layout.operator("action.delete")

        layout.separator()
        layout.operator_menu_enum("action.keyframe_type", "type", text="Keyframe Type")
        layout.operator_menu_enum("action.handle_type", "type", text="Handle Type")
        layout.operator_menu_enum("action.interpolation_type", "type", text="Interpolation Mode")

        layout.separator()
        layout.operator("action.clean").channels = False
        layout.operator("action.clean", text="Clean Channels").channels = True
        layout.operator("action.sample")

        layout.separator()
        layout.operator("action.copy")
        layout.operator("action.paste")


class DOPESHEET_MT_key_transform(Menu):
    bl_label = "Transform"

    def draw(self, context):
        layout = self.layout

        layout.operator("transform.transform", text="Grab/Move").mode = 'TIME_TRANSLATE'
        layout.operator("transform.transform", text="Extend").mode = 'TIME_EXTEND'
        layout.operator("transform.transform", text="Slide").mode = 'TIME_SLIDE'
        layout.operator("transform.transform", text="Scale").mode = 'TIME_SCALE'


#######################################
# Grease Pencil Editing

class DOPESHEET_MT_gpencil_channel(Menu):
    bl_label = "Channel"

    def draw(self, context):
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
        #layout.separator()
        #layout.operator("anim.channels_expand")
        #layout.operator("anim.channels_collapse")

        #layout.separator()
        #layout.operator_menu_enum("anim.channels_move", "direction", text="Move...")


class DOPESHEET_MT_gpencil_frame(Menu):
    bl_label = "Frame"

    def draw(self, context):
        layout = self.layout

        layout.menu("DOPESHEET_MT_key_transform", text="Transform")
        layout.operator_menu_enum("action.snap", "type", text="Snap")
        layout.operator_menu_enum("action.mirror", "type", text="Mirror")

        layout.separator()
        layout.operator("action.duplicate")
        layout.operator("action.delete")

        layout.separator()
        layout.operator("action.keyframe_type")

        #layout.separator()
        #layout.operator("action.copy")
        #layout.operator("action.paste")


class DOPESHEET_MT_delete(Menu):
    bl_label = "Delete"

    def draw(self, context):
        layout = self.layout

        layout.operator("action.delete")

        layout.separator()

        layout.operator("action.clean").channels = False
        layout.operator("action.clean", text="Clean Channels").channels = True


classes = (
    DOPESHEET_HT_header,
    DOPESHEET_MT_editor_menus,
    DOPESHEET_MT_view,
    DOPESHEET_MT_select,
    DOPESHEET_MT_marker,
    DOPESHEET_MT_channel,
    DOPESHEET_MT_key,
    DOPESHEET_MT_key_transform,
    DOPESHEET_MT_gpencil_channel,
    DOPESHEET_MT_gpencil_frame,
    DOPESHEET_MT_delete,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
