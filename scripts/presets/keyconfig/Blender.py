# SPDX-FileCopyrightText: 2018-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

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
    tool_key_mode: EnumProperty(
        name="Tool Keys",
        description=(
            "The method of keys to activate tools such as move, rotate & scale (G, R, S)"
        ),
        items=(
            ('IMMEDIATE', "Immediate",
             "Activate actions immediately"),
            ('TOOL', "Active Tool",
             "Activate the tool for editors that support tools"),
        ),
        default='IMMEDIATE',
        update=update_fn,
    )

    rmb_action: EnumProperty(
        name="Right Mouse Select Action",
        items=(
            ('TWEAK', "Select & Tweak",
             "Right mouse always tweaks"),
            ('FALLBACK_TOOL', "Selection Tool",
             "Right mouse uses the selection tool"),
        ),
        description=(
            "Default action for the right mouse button"
        ),
        update=update_fn,
    )

    # Experimental: only show with developer extras, see: #107785.
    use_region_toggle_pie: BoolProperty(
        name="Region Toggle Pie",
        description=(
            "N-key opens a pie menu to toggle regions"
        ),
        default=False,
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
    # NOTE: expose `use_alt_tool` and `use_alt_cursor` as two options in the UI
    # as the tool-tips and titles are different enough depending on RMB/LMB select.
    use_alt_tool: BoolProperty(
        name="Alt Tool Access",
        description=(
            "Hold Alt to use the active tool when the gizmo would normally be required\n"
            "Incompatible with the input preference \"Emulate 3 Button Mouse\" when the \"Alt\" key is used"
        ),
        default=False,
        update=update_fn,
    )
    use_alt_cursor: BoolProperty(
        name="Alt Cursor Access",
        description=(
            "Hold Alt-LMB to place the Cursor (instead of LMB), allows tools to activate on press instead of drag.\n"
            "Incompatible with the input preference \"Emulate 3 Button Mouse\" when the \"Alt\" key is used"
        ),
        default=False,
        update=update_fn,
    )
    # end note.

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
            "This applies to trackpad as well"
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

    # Developer note, this is an experimental option.
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

    use_file_single_click: BoolProperty(
        name="Open Folders on Single Click",
        description=(
            "Navigate into folders by clicking on them once instead of twice"
        ),
        default=False,
        update=update_fn,
    )

    use_alt_navigation: BoolProperty(
        name="Transform Navigation with Alt",
        description=(
            "During transformations, use Alt to navigate in the 3D View. "
            "Note that if disabled, hotkeys for Proportional Editing, Automatic Constraints, and Auto IK Chain Length will require holding Alt"),
        default=True,
        update=update_fn,
    )

    def draw(self, layout):
        from bpy import context

        layout.use_property_split = True
        layout.use_property_decorate = False

        prefs = context.preferences

        show_developer_ui = prefs.view.show_developer_ui
        is_select_left = (self.select_mouse == 'LEFT')
        use_mouse_emulate_3_button = (
            prefs.inputs.use_mouse_emulate_3_button and
            prefs.inputs.mouse_emulate_3_button_modifier == 'ALT'
        )

        # General settings.
        col = layout.column()
        col.row().prop(self, "select_mouse", text="Select with Mouse Button", expand=True)
        col.row().prop(self, "spacebar_action", text="Spacebar Action", expand=True)

        if is_select_left:
            col.row().prop(self, "gizmo_action", text="Activate Gizmo Event", expand=True)
        else:
            col.row().prop(self, "rmb_action", text="Right Mouse Select Action", expand=True)

        col.row().prop(self, "tool_key_mode", expand=True)

        # Check-box sub-layout.
        col = layout.column()
        sub = col.column(align=True)
        row = sub.row()
        row.prop(self, "use_alt_click_leader")

        rowsub = row.row()
        if is_select_left:
            rowsub.prop(self, "use_alt_tool")
        else:
            rowsub.prop(self, "use_alt_cursor")
        rowsub.active = not use_mouse_emulate_3_button

        row = sub.row()
        row.prop(self, "use_select_all_toggle")

        if show_developer_ui:
            row = sub.row()
            row.prop(self, "use_region_toggle_pie")

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
        sub.prop(self, "use_alt_navigation")

        # File Browser settings.
        col = layout.column()
        col.label(text="File Browser")
        col.row().prop(self, "use_file_single_click")


blender_default = bpy.utils.execfile(os.path.join(DIRNAME, "keymap_data", "blender_default.py"))


def load():
    from sys import platform
    from bpy import context
    from bl_keymap_utils.io import keyconfig_init_from_data

    prefs = context.preferences
    kc = context.window_manager.keyconfigs.new(IDNAME)
    kc_prefs = kc.preferences

    show_developer_ui = prefs.view.show_developer_ui
    is_select_left = (kc_prefs.select_mouse == 'LEFT')
    use_mouse_emulate_3_button = (
        prefs.inputs.use_mouse_emulate_3_button and
        prefs.inputs.mouse_emulate_3_button_modifier == 'ALT'
    )

    keyconfig_data = blender_default.generate_keymaps(
        blender_default.Params(
            select_mouse=kc_prefs.select_mouse,
            use_mouse_emulate_3_button=use_mouse_emulate_3_button,
            spacebar_action=kc_prefs.spacebar_action,
            use_key_activate_tools=(kc_prefs.tool_key_mode == 'TOOL'),
            use_region_toggle_pie=(show_developer_ui and kc_prefs.use_region_toggle_pie),
            v3d_tilde_action=kc_prefs.v3d_tilde_action,
            use_v3d_mmb_pan=(kc_prefs.v3d_mmb_action == 'PAN'),
            v3d_alt_mmb_drag_action=kc_prefs.v3d_alt_mmb_drag_action,
            use_select_all_toggle=kc_prefs.use_select_all_toggle,
            use_v3d_tab_menu=kc_prefs.use_v3d_tab_menu,
            use_v3d_shade_ex_pie=kc_prefs.use_v3d_shade_ex_pie,
            use_gizmo_drag=(is_select_left and kc_prefs.gizmo_action == 'DRAG'),
            use_fallback_tool=True,
            use_fallback_tool_select_handled=(
                # LMB doesn't need additional selection fallback key-map items.
                False if is_select_left else
                # RMB is select and RMB must trigger the fallback tool.
                # Otherwise LMB activates the fallback tool and RMB always tweak-selects.
                (kc_prefs.rmb_action != 'FALLBACK_TOOL')
            ),
            use_alt_tool_or_cursor=(
                (not use_mouse_emulate_3_button) and
                (kc_prefs.use_alt_tool if is_select_left else kc_prefs.use_alt_cursor)
            ),
            use_alt_click_leader=kc_prefs.use_alt_click_leader,
            use_pie_click_drag=kc_prefs.use_pie_click_drag,
            use_file_single_click=kc_prefs.use_file_single_click,
            use_alt_navigation=kc_prefs.use_alt_navigation,
            # Experimental features.
            use_experimental_grease_pencil_version3=prefs.experimental.use_grease_pencil_version3,
        ),
    )

    if platform == 'darwin':
        from bl_keymap_utils.platform_helpers import keyconfig_data_oskey_from_ctrl_for_macos
        keyconfig_data = keyconfig_data_oskey_from_ctrl_for_macos(keyconfig_data)

    keyconfig_init_from_data(kc, keyconfig_data)


if __name__ == "__main__":
    bpy.utils.register_class(Prefs)
    load()
