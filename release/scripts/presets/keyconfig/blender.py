import os
import bpy
from bpy.props import (
    BoolProperty,
    EnumProperty,
)

DIRNAME, FILENAME = os.path.split(__file__)
IDNAME = os.path.splitext(FILENAME)[0]

def update_fn(_self, _context):
    load()


class Prefs(bpy.types.KeyConfigPreferences):
    bl_idname = IDNAME

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
        update=update_fn,
    )
    spacebar_action: EnumProperty(
        name="Spacebar Action",
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
    use_alt_click_leader: BoolProperty(
        name="Alt Click Tool Prompt",
        description=(
            "Tapping Alt (without pressing any other keys) shows a prompt in the status-bar\n"
            "prompting a second keystroke to activate the tool"
        ),
        default=False,
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

    gizmo_action: EnumProperty(
        name="Activate Gizmo",
        items=(
            ('PRESS', "Press", "Press causes immediate activation, preventing click being passed to the tool"),
            ('DRAG', "Drag", "Drag allows click events to pass through to the tool, adding a small delay"),
        ),
        description="Activation event for gizmos that support drag motion",
        default='DRAG',
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
    v3d_tilde_action: EnumProperty(
        name="Tilde Action",
        items=(
            ('VIEW', "Navigate",
             "View operations (useful for keyboards without a numpad)",
             0),
            ('GIZMO', "Gizmos",
             "Control transform gizmos",
             1),
        ),
        description=(
            "Action when 'Tilde' is pressed"
        ),
        default='VIEW',
        update=update_fn,
    )

    v3d_mmb_action: EnumProperty(
        name="MMB Action",
        items=(
            ('ORBIT', "Orbit",
             "",
             0),
            ('PAN', "Pan",
             "",
             1),
        ),
        description=(
            "The action when Middle-Mouse dragging in the viewport. "
            "Shift-Middle-Mouse is used for the other action. "
            "This applies to Track-Pad as well"
        ),
        update=update_fn,
    )

    v3d_alt_mmb_drag_action: EnumProperty(
        name="Alt-MMB Drag Action",
        items=(
            ('RELATIVE', "Relative",
             "Set the view axis where each mouse direction maps to an axis relative to the current orientation",
             0),
            ('ABSOLUTE', "Absolute",
             "Set the view axis where each mouse direction always maps to the same axis",
             1),
        ),
        description=(
            "Action when Alt-MMB dragging in the 3D viewport"
        ),
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
        layout.use_property_split = True
        layout.use_property_decorate = False

        is_select_left = (self.select_mouse == 'LEFT')

        # General settings.
        col = layout.column()
        col.row().prop(self, "select_mouse", text="Select with Mouse Button", expand=True)
        col.row().prop(self, "spacebar_action", text="Spacebar Action", expand=True)

        if is_select_left:
            col.row().prop(self, "gizmo_action", text="Activate Gizmo Event", expand=True)

        # Checkboxes sub-layout.
        col = layout.column()
        sub = col.column(align=True)
        row = sub.row()
        row.prop(self, "use_select_all_toggle")
        row.prop(self, "use_alt_click_leader")

        # 3DView settings.
        col = layout.column()
        col.label(text="3D View")
        col.row().prop(self, "v3d_tilde_action", text="Grave Accent / Tilde Action", expand=True)
        col.row().prop(self, "v3d_mmb_action", text="Middle Mouse Action", expand=True)
        col.row().prop(self, "v3d_alt_mmb_drag_action", text="Alt Middle Mouse Drag Action", expand=True)

        # Checkboxes sub-layout.
        col = layout.column()
        sub = col.column(align=True)
        sub.prop(self, "use_v3d_tab_menu")
        sub.prop(self, "use_pie_click_drag")
        sub.prop(self, "use_v3d_shade_ex_pie")


blender_default = bpy.utils.execfile(os.path.join(DIRNAME, "keymap_data", "blender_default.py"))


def load():
    from sys import platform
    from bpy import context
    from bl_keymap_utils.io import keyconfig_init_from_data

    prefs = context.preferences
    kc = context.window_manager.keyconfigs.new(IDNAME)
    kc_prefs = kc.preferences

    keyconfig_data = blender_default.generate_keymaps(
        blender_default.Params(
            select_mouse=kc_prefs.select_mouse,
            use_mouse_emulate_3_button=(
                prefs.inputs.use_mouse_emulate_3_button and
                prefs.inputs.mouse_emulate_3_button_modifier == 'ALT'
            ),
            spacebar_action=kc_prefs.spacebar_action,
            v3d_tilde_action=kc_prefs.v3d_tilde_action,
            use_v3d_mmb_pan=(kc_prefs.v3d_mmb_action == 'PAN'),
            v3d_alt_mmb_drag_action=kc_prefs.v3d_alt_mmb_drag_action,
            use_select_all_toggle=kc_prefs.use_select_all_toggle,
            use_v3d_tab_menu=kc_prefs.use_v3d_tab_menu,
            use_v3d_shade_ex_pie=kc_prefs.use_v3d_shade_ex_pie,
            use_gizmo_drag=(
                kc_prefs.select_mouse == 'LEFT' and
                kc_prefs.gizmo_action == 'DRAG'
            ),
            use_alt_click_leader=kc_prefs.use_alt_click_leader,
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
