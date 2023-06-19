# SPDX-FileCopyrightText: 2009-2023 Blender Foundation
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


# -----------------------------------------------------------------------------
# Mix-In Helpers

# Panel mix-in.
class CenterAlignMixIn:
    """
    Base class for panels to center align contents with some horizontal margin.
    Deriving classes need to implement a ``draw_centered(context, layout)`` function.

    Used by Preferences and Project Settings, and optimized for their display. May not work that
    well in other cases.
    """

    def draw(self, context):
        layout = self.layout
        width = context.region.width
        ui_scale = context.preferences.system.ui_scale
        # No horizontal margin if region is rather small.
        is_wide = width > (350 * ui_scale)

        layout.use_property_split = True
        layout.use_property_decorate = False  # No animation.

        row = layout.row()
        if is_wide:
            row.label()  # Needed so col below is centered.

        col = row.column()
        col.ui_units_x = 50

        # Implemented by sub-classes.
        self.draw_centered(context, col)

        if is_wide:
            row.label()  # Needed so col above is centered.
