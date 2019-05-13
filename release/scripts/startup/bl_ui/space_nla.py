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

from bpy.types import Header, Menu, Panel
from bpy.app.translations import contexts as i18n_contexts
from .space_dopesheet import (
    DopesheetFilterPopoverBase,
    dopesheet_filter,
)


class NLA_HT_header(Header):
    bl_space_type = 'NLA_EDITOR'

    def draw(self, context):
        layout = self.layout

        st = context.space_data

        layout.template_header()

        NLA_MT_editor_menus.draw_collapsible(context, layout)

        layout.separator_spacer()

        dopesheet_filter(layout, context)

        layout.popover(
            panel="NLA_PT_filters",
            text="",
            icon='FILTER',
        )

        layout.prop(st, "auto_snap", text="")


class NLA_PT_filters(DopesheetFilterPopoverBase, Panel):
    bl_space_type = 'NLA_EDITOR'
    bl_region_type = 'HEADER'
    bl_label = "Filters"

    def draw(self, context):
        layout = self.layout

        DopesheetFilterPopoverBase.draw_generic_filters(context, layout)
        layout.separator()
        DopesheetFilterPopoverBase.draw_search_filters(context, layout)
        layout.separator()
        DopesheetFilterPopoverBase.draw_standard_filters(context, layout)


class NLA_MT_editor_menus(Menu):
    bl_idname = "NLA_MT_editor_menus"
    bl_label = ""

    def draw(self, _context):
        layout = self.layout
        layout.menu("NLA_MT_view")
        layout.menu("NLA_MT_select")
        layout.menu("NLA_MT_marker")
        layout.menu("NLA_MT_edit")
        layout.menu("NLA_MT_add")


class NLA_MT_view(Menu):
    bl_label = "View"

    def draw(self, context):
        layout = self.layout

        st = context.space_data

        layout.prop(st, "show_region_ui")
        layout.separator()

        layout.prop(st, "use_realtime_update")
        layout.prop(st, "show_frame_indicator")

        layout.prop(st, "show_seconds")
        layout.prop(st, "show_locked_time")

        layout.prop(st, "show_strip_curves")
        layout.prop(st, "show_local_markers")
        layout.prop(st, "show_marker_lines")

        layout.separator()
        layout.operator("anim.previewrange_set")
        layout.operator("anim.previewrange_clear")
        layout.operator("nla.previewrange_set")

        layout.separator()
        layout.operator("nla.view_all")
        layout.operator("nla.view_selected")
        layout.operator("nla.view_frame")

        layout.separator()
        layout.menu("INFO_MT_area")


class NLA_MT_select(Menu):
    bl_label = "Select"

    def draw(self, _context):
        layout = self.layout

        layout.operator("nla.select_all", text="All").action = 'SELECT'
        layout.operator("nla.select_all", text="None").action = 'DESELECT'
        layout.operator("nla.select_all", text="Invert").action = 'INVERT'

        layout.separator()
        layout.operator("nla.select_box").axis_range = False
        layout.operator("nla.select_box", text="Border Axis Range").axis_range = True

        layout.separator()
        props = layout.operator("nla.select_leftright", text="Before Current Frame")
        props.extend = False
        props.mode = 'LEFT'
        props = layout.operator("nla.select_leftright", text="After Current Frame")
        props.extend = False
        props.mode = 'RIGHT'


class NLA_MT_marker(Menu):
    bl_label = "Marker"

    def draw(self, context):
        layout = self.layout

        from .space_time import marker_menu_generic
        marker_menu_generic(layout, context)


class NLA_MT_edit(Menu):
    bl_label = "Edit"

    def draw(self, context):
        layout = self.layout

        scene = context.scene

        layout.menu("NLA_MT_edit_transform", text="Transform")

        layout.operator_menu_enum("nla.snap", "type", text="Snap")

        layout.separator()
        layout.operator("nla.duplicate", text="Duplicate").linked = False
        layout.operator("nla.duplicate", text="Linked Duplicate").linked = True
        layout.operator("nla.split")
        layout.operator("nla.delete")

        layout.separator()
        layout.operator("nla.mute_toggle")

        layout.separator()
        layout.operator("nla.apply_scale")
        layout.operator("nla.clear_scale")
        layout.operator("nla.action_sync_length").active = False

        layout.separator()
        layout.operator("nla.make_single_user")

        layout.separator()
        layout.operator("nla.swap")
        layout.operator("nla.move_up")
        layout.operator("nla.move_down")

        # TODO: this really belongs more in a "channel" (or better, "track") menu
        layout.separator()
        layout.operator_menu_enum("anim.channels_move", "direction", text="Track Ordering...")
        layout.operator("anim.channels_clean_empty")

        layout.separator()
        # TODO: names of these tools for 'tweak-mode' need changing?
        if scene.is_nla_tweakmode:
            layout.operator("nla.tweakmode_exit", text="Stop Editing Stashed Action").isolate_action = True
            layout.operator("nla.tweakmode_exit", text="Stop Tweaking Strip Actions")
        else:
            layout.operator("nla.tweakmode_enter", text="Start Editing Stashed Action").isolate_action = True
            layout.operator("nla.tweakmode_enter", text="Start Tweaking Strip Actions")


class NLA_MT_add(Menu):
    bl_label = "Add"
    bl_translation_context = i18n_contexts.operator_default

    def draw(self, _context):
        layout = self.layout

        layout.operator("nla.actionclip_add")
        layout.operator("nla.transition_add")
        layout.operator("nla.soundclip_add")

        layout.separator()
        layout.operator("nla.meta_add")
        layout.operator("nla.meta_remove")

        layout.separator()
        layout.operator("nla.tracks_add").above_selected = False
        layout.operator("nla.tracks_add", text="Add Tracks Above Selected").above_selected = True

        layout.separator()
        layout.operator("nla.selected_objects_add")


class NLA_MT_edit_transform(Menu):
    bl_label = "Transform"

    def draw(self, _context):
        layout = self.layout

        layout.operator("transform.translate", text="Move")
        layout.operator("transform.transform", text="Extend").mode = 'TIME_EXTEND'
        layout.operator("transform.transform", text="Scale").mode = 'TIME_SCALE'


class NLA_MT_snap_pie(Menu):
    bl_label = "Snap"

    def draw(self, _context):
        layout = self.layout
        pie = layout.menu_pie()

        pie.operator("nla.snap", text="Current Frame").type = 'CFRA'
        pie.operator("nla.snap", text="Nearest Frame").type = 'NEAREST_FRAME'
        pie.operator("nla.snap", text="Nearest Second").type = 'NEAREST_SECOND'
        pie.operator("nla.snap", text="Nearest Marker").type = 'NEAREST_MARKER'

class NLA_MT_context_menu(Menu):
    bl_label = "NLA Context Menu"

    def draw(self, context):
        layout = self.layout
        scene = context.scene

        if scene.is_nla_tweakmode:
            layout.operator("nla.tweakmode_exit", text="Stop Editing Stashed Action").isolate_action = True
            layout.operator("nla.tweakmode_exit", text="Stop Tweaking Strip Actions")
        else:
            layout.operator("nla.tweakmode_enter", text="Start Editing Stashed Action").isolate_action = True
            layout.operator("nla.tweakmode_enter", text="Start Tweaking Strip Actions")

        layout.separator()

        layout.operator("nla.duplicate", text="Duplicate").linked = False
        layout.operator("nla.duplicate", text="Linked Duplicate").linked = True

        layout.separator()

        layout.operator("nla.split")
        layout.operator("nla.delete")

        layout.separator()

        layout.operator("nla.swap")

        layout.separator()

        layout.operator_menu_enum("nla.snap", "type", text="Snap")


class NLA_MT_channel_context_menu(Menu):
    bl_label = "NLA Channel Context Menu"

    def draw(self, context):
        layout = self.layout

        layout.operator_menu_enum("anim.channels_move", "direction", text="Track Ordering...")
        layout.operator("anim.channels_clean_empty")



classes = (
    NLA_HT_header,
    NLA_MT_edit,
    NLA_MT_editor_menus,
    NLA_MT_view,
    NLA_MT_select,
    NLA_MT_marker,
    NLA_MT_add,
    NLA_MT_edit_transform,
    NLA_MT_snap_pie,
    NLA_MT_context_menu,
    NLA_MT_channel_context_menu,
    NLA_PT_filters,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
