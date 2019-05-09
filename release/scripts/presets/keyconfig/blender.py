import os
import bpy
from bpy.props import (
    BoolProperty,
    EnumProperty,
)

dirname, filename = os.path.split(__file__)
idname = os.path.splitext(filename)[0]

def update_fn(_self, _context):
    load()


class Prefs(bpy.types.KeyConfigPreferences):
    bl_idname = idname

    select_mouse: EnumProperty(
        name="Select Mouse",
        items=(
            ('LEFT', "Left",
             "Use left mouse button for selection. "
             "The standard behavior that works well for mouse, trackpad and tablet devices"),
            ('RIGHT', "Right",
             "Use right mouse button for selection, and left mouse button for actions. "
             "This works well primarily for keyboard and mouse devices"),
        ),
        description=(
            "Mouse button used for selection"
        ),
        default='LEFT',
        update=update_fn,
    )
    spacebar_action: EnumProperty(
        name="Spacebar",
        items=(
            ('PLAY', "Play",
             "Toggle animation playback "
             "('Shift-Space' for Tools)",
             1),
            ('TOOL', "Tools",
             "Open the popup tool-bar\n"
             "When 'Space' is held and used as a modifier:\n"
             "\u2022 Pressing the tools binding key switches to it immediately.\n"
             "\u2022 Dragging the cursor over a tool and releasing activates it (like a pie menu).\n"
             "For Play use 'Shift-Space'",
             0),
            ('SEARCH', "Search",
             "Open the operator search popup",
             2),
        ),
        description=(
            "Action when 'Space' is pressed"
        ),
        default='PLAY',
        update=update_fn,
    )
    use_select_all_toggle: BoolProperty(
        name="Select All Toggles",
        description=(
            "Causes select-all ('A' key) to de-select in the case a selection exists"
        ),
        default=False,
        update=update_fn,
    )

    # 3D View
    use_v3d_tab_menu: BoolProperty(
        name="Tab for Pie Menu",
        description=(
            "Causes tab to open pie menu (swaps 'Tab' / 'Ctrl-Tab')"
        ),
        default=False,
        update=update_fn,
    )
    use_v3d_shade_ex_pie: BoolProperty(
        name="Extra Shading Pie Menu Items",
        description=(
            "Show additional options in the shading menu ('Z')"
        ),
        default=False,
        update=update_fn,
    )
    # Developer note, this is an experemental option.
    use_pie_click_drag: BoolProperty(
        name="Pie Menu on Drag",
        description=(
            "Activate some pie menus on drag,\n"
            "allowing the tapping the same key to have a secondary action.\n"
            "\n"
             "\u2022 Tapping Tab in the 3D view toggles edit-mode, drag for mode menu.\n"
             "\u2022 Tapping Z in the 3D view toggles wireframe, drag for draw modes.\n"
             "\u2022 Tapping Tilde in the 3D view for first person navigation, drag for view axes"
        ),
        default=False,
        update=update_fn,
    )

    def draw(self, layout):
        split = layout.split()
        col = split.column(align=True)
        col.label(text="Select With:")
        col.row().prop(self, "select_mouse", expand=True)
        col.prop(self, "use_select_all_toggle")

        col = split.column(align=True)
        col.label(text="Spacebar Action:")
        col.row().prop(self, "spacebar_action", expand=True)

        layout.label(text="3D View:")
        split = layout.split()
        col = split.column()
        col.prop(self, "use_v3d_tab_menu")
        col.prop(self, "use_pie_click_drag")
        col = split.column()
        col.prop(self, "use_v3d_shade_ex_pie")


blender_default = bpy.utils.execfile(os.path.join(dirname, "keymap_data", "blender_default.py"))


def load():
    from sys import platform
    from bpy import context
    from bl_keymap_utils.io import keyconfig_init_from_data

    prefs = context.preferences
    kc = context.window_manager.keyconfigs.new(idname)
    kc_prefs = kc.preferences

    keyconfig_data = blender_default.generate_keymaps(
        blender_default.Params(
            select_mouse=kc_prefs.select_mouse,
            use_mouse_emulate_3_button=prefs.inputs.use_mouse_emulate_3_button,
            spacebar_action=kc_prefs.spacebar_action,
            use_select_all_toggle=kc_prefs.use_select_all_toggle,
            use_v3d_tab_menu=kc_prefs.use_v3d_tab_menu,
            use_v3d_shade_ex_pie=kc_prefs.use_v3d_shade_ex_pie,
            use_pie_click_drag=kc_prefs.use_pie_click_drag,
        ),
    )

    if platform == 'darwin':
        from bl_keymap_utils.platform_helpers import keyconfig_data_oskey_from_ctrl_for_macos
        keyconfig_data = keyconfig_data_oskey_from_ctrl_for_macos(keyconfig_data)

    keyconfig_init_from_data(kc, keyconfig_data)


if __name__ == "__main__":
    bpy.utils.register_class(Prefs)
    load()
