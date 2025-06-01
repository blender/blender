# SPDX-FileCopyrightText: 2009-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

from bpy.types import Menu


# Panel mix-in class (don't register).
class PresetPanel:
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'HEADER'
    bl_label = "Presets"
    path_menu = Menu.path_menu

    @classmethod
    def draw_panel_header(cls, layout):
        layout.emboss = 'NONE'
        layout.popover(
            panel=cls.__name__,
            icon='PRESET',
            text="",
        )

    @classmethod
    def draw_menu(cls, layout, text=None):
        if text is None:
            text = cls.bl_label

        layout.popover(
            panel=cls.__name__,
            icon='PRESET',
            text=text,
        )

    def draw(self, context):
        layout = self.layout
        layout.emboss = 'PULLDOWN_MENU'
        layout.operator_context = 'EXEC_DEFAULT'

        Menu.draw_preset(self, context)


class PlayheadSnappingPanel:
    bl_region_type = 'HEADER'
    bl_label = "Playhead"

    @classmethod
    def poll(cls, context):
        del context
        return True

    def draw(self, context):
        tool_settings = context.tool_settings
        layout = self.layout
        col = layout.column()

        col.prop(tool_settings, "use_snap_playhead")
        col.prop(tool_settings, "playhead_snap_distance")
        col.separator()
        col.label(text="Snap Target")
        col.prop(tool_settings, "snap_playhead_element", expand=True)
        col.separator()

        if 'FRAME' in tool_settings.snap_playhead_element:
            col.prop(tool_settings, "snap_playhead_frame_step")
        if 'SECOND' in tool_settings.snap_playhead_element:
            col.prop(tool_settings, "snap_playhead_second_step")
