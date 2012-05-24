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


#######################################
# DopeSheet Editor - General/Standard UI

class DOPESHEET_HT_header(Header):
    bl_space_type = 'DOPESHEET_EDITOR'

    def draw(self, context):
        layout = self.layout

        st = context.space_data

        row = layout.row(align=True)
        row.template_header()

        if context.area.show_menus:
            row.menu("DOPESHEET_MT_view")
            row.menu("DOPESHEET_MT_select")
            row.menu("DOPESHEET_MT_marker")

            if st.mode == 'DOPESHEET' or (st.mode == 'ACTION' and st.action is not None):
                row.menu("DOPESHEET_MT_channel")
            elif st.mode == 'GPENCIL':
                row.menu("DOPESHEET_MT_gpencil_channel")

            if st.mode != 'GPENCIL':
                row.menu("DOPESHEET_MT_key")
            else:
                row.menu("DOPESHEET_MT_gpencil_frame")

        layout.prop(st, "mode", text="")
        layout.prop(st.dopesheet, "show_summary", text="Summary")

        if st.mode == 'DOPESHEET':
            dopesheet_filter(layout, context)
        elif st.mode == 'ACTION':
            # 'genericFiltersOnly' limits the options to only the relevant 'generic' subset of
            # filters which will work here and are useful (especially for character animation)
            dopesheet_filter(layout, context, genericFiltersOnly=True)

        if st.mode in {'ACTION', 'SHAPEKEY'}:
            layout.template_ID(st, "action", new="action.new")

        # Grease Pencil mode doesn't need snapping, as it's frame-aligned only
        if st.mode != 'GPENCIL':
            layout.prop(st, "auto_snap", text="")

        row = layout.row(align=True)
        row.operator("action.copy", text="", icon='COPYDOWN')
        row.operator("action.paste", text="", icon='PASTEDOWN')


class DOPESHEET_MT_view(Menu):
    bl_label = "View"

    def draw(self, context):
        layout = self.layout

        st = context.space_data

        layout.prop(st, "use_realtime_update")
        layout.prop(st, "show_frame_indicator")
        layout.prop(st, "show_sliders")
        layout.prop(st, "show_group_colors")
        layout.prop(st, "use_auto_merge_keyframes")
        layout.prop(st, "use_marker_sync")

        layout.prop(st, "show_seconds")

        layout.separator()
        layout.operator("anim.previewrange_set")
        layout.operator("anim.previewrange_clear")
        layout.operator("action.previewrange_set")

        layout.separator()
        layout.operator("action.frame_jump")
        layout.operator("action.view_all")
        layout.operator("action.view_selected")

        layout.separator()
        layout.operator("screen.area_dupli")
        layout.operator("screen.screen_full_area")


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

        layout.separator()
        layout.operator("action.select_column", text="Columns on Selected Keys").mode = 'KEYS'
        layout.operator("action.select_column", text="Column on Current Frame").mode = 'CFRA'

        layout.operator("action.select_column", text="Columns on Selected Markers").mode = 'MARKERS_COLUMN'
        layout.operator("action.select_column", text="Between Selected Markers").mode = 'MARKERS_BETWEEN'

        layout.separator()
        layout.operator("action.select_leftright", text="Before Current Frame").mode = 'LEFT'
        layout.operator("action.select_leftright", text="After Current Frame").mode = 'RIGHT'

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
        layout.operator("anim.channels_setting_toggle")
        layout.operator("anim.channels_setting_enable")
        layout.operator("anim.channels_setting_disable")

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
        layout.operator("action.duplicate_move")
        layout.operator("action.delete")

        layout.separator()
        layout.operator_menu_enum("action.keyframe_type", "type", text="Keyframe Type")
        layout.operator_menu_enum("action.handle_type", "type", text="Handle Type")
        layout.operator_menu_enum("action.interpolation_type", "type", text="Interpolation Mode")

        layout.separator()
        layout.operator("action.clean")
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

        #layout.operator_menu_enum("action.snap", "type", text="Snap")
        #layout.operator_menu_enum("action.mirror", "type", text="Mirror")

        layout.separator()
        layout.operator("action.duplicate")
        layout.operator("action.delete")

        #layout.separator()
        #layout.operator("action.copy")
        #layout.operator("action.paste")

if __name__ == "__main__":  # only for live edit.
    bpy.utils.register_module(__name__)
