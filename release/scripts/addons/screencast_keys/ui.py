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

from .ops import SK_OT_ScreencastKeys, show_mouse_hold_status
from .utils.bl_class_registry import BlClassRegistry
from .utils import compatibility as compat
from .utils import c_structures


@BlClassRegistry()
class SK_PT_ScreencastKeys(bpy.types.Panel):
    bl_idname = "SK_PT_ScreencastKeys"
    bl_label = "Screencast Keys"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = "Screencast Keys"

    def draw_header(self, context):
        layout = self.layout
        layout.prop(context.window_manager, "enable_screencast_keys", text="")

    def draw(self, context):
        layout = self.layout
        prefs = compat.get_user_preferences(bpy.context).addons[__package__].preferences

        column = layout.column()

        column.prop(prefs, "color")
        column.prop(prefs, "shadow")
        if prefs.shadow:
            column.prop(prefs, "shadow_color", text="")
        column.prop(prefs, "background")
        if prefs.background:
            sp = compat.layout_split(column, factor=0.5)
            sp.prop(prefs, "background_mode", text="")
            sp = compat.layout_split(sp, factor=1.0)
            sp.prop(prefs, "background_color", text="")
        column.prop(prefs, "font_size")
        if show_mouse_hold_status(prefs):
            column.prop(prefs, "mouse_size")

        column.separator()

        column.prop(prefs, "origin")
        column.operator("wm.sk_set_origin", text="Set Origin")
        column.prop(prefs, "align")
        row = column.row()
        row.prop(prefs, "offset")
        column.prop(prefs, "display_time")

        column.separator()

        column.prop(prefs, "max_event_history")
        column.prop(prefs, "show_mouse_events")
        if prefs.show_mouse_events:
            column.prop(prefs, "mouse_events_show_mode")
        column.prop(prefs, "show_last_operator")

        column.separator()

        column.label(text="Experimental:")
        col = column.column()
        col.prop(prefs, "get_event_aggressively")
        col.enabled = False if c_structures.NOT_SUPPORTED else True

    @classmethod
    def register(cls):
        def get_func(self):
            return SK_OT_ScreencastKeys.is_running()

        def set_func(self, value):
            pass

        def update_func(self, context):
            bpy.ops.wm.sk_screencast_keys('INVOKE_REGION_WIN')

        bpy.types.WindowManager.enable_screencast_keys = \
            bpy.props.BoolProperty(
                name="Screencast Keys",
                get=get_func,
                set=set_func,
                update=update_func,
            )

    @classmethod
    def unregister(cls):
        del bpy.types.WindowManager.enable_screencast_keys
