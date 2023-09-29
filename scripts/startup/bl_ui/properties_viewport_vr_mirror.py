# SPDX-License-Identifier: GPL-2.0-or-later
import bpy
from bpy.types import Panel

class VIEW3D_PT_vr_session_blender_ui_mirror(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = 'VR'
    bl_label = 'Blender UI Mirror'

    def draw(self, context):
        layout = self.layout
        session_settings = context.window_manager.xr_session_settings

        layout.use_property_split = True
        layout.use_property_decorate = False  # No animation.

        col = layout.column(align=True)
        col.prop(session_settings, "show_blender_ui_mirror")
        col.prop(session_settings, "show_xr_controllers")
        col.prop(session_settings, "use_mirror_ui_xray")
        col.prop(session_settings, "hide_mirror_ui_on_mouse_over_v3d")
        col.prop(session_settings, "viewer_offset")
        col.prop(session_settings, "viewer_angle_offset")
        col.prop(session_settings, "mirrored_ui_rads_span")
        col.prop(session_settings, "mirrored_ui_offset")
        col.prop(session_settings, "mirrored_ui_scale_y")
        col.prop(session_settings, "mirrored_ui_distance_factor")
        col.prop(session_settings, "cursor_raycast_distance0")
        col.prop(session_settings, "cursor_raycast_distance1")
        col.prop(session_settings, "cursor_raycast_distance2")

classes = [
    VIEW3D_PT_vr_session_blender_ui_mirror
]

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
