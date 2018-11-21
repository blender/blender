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
            ('LEFT', "Left", "Use left Mouse Button for selection"),
            ('RIGHT', "Right", "Use Right Mouse Button for selection"),
        ),
        description=(
            "Mouse button used for selection"
        ),
        default='RIGHT',
        update=update_fn,
    )
    spacebar_action: EnumProperty(
        name="Spacebar",
        items=(
            ('TOOL', "Tool-Bar",
             "Open the popup tool-bar\n"
             "When 'Space' is held and used as a modifier:\n"
             "\u2022 Pressing the tools binding key switches to it immediately.\n"
             "\u2022 Dragging the cursor over a tool and releasing activates it (like a pie menu).\n"
            ),
            ('PLAY', "Playback",
             "Toggle animation playback"
            ),
        ),
        description=(
            "Action when 'Space' is pressed ('Shift-Space' is used for the other action)"
        ),
        default='TOOL',
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

    use_v3d_tab_menu: BoolProperty(
        name="Tab for Pie Menu",
        description=(
            "Causes tab to open pie menu (swaps 'Tab' / 'Ctrl-Tab')"
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

        split = layout.split()
        col = split.column()
        col.label(text="3D View:")
        col.prop(self, "use_v3d_tab_menu")
        split.column()


blender_default = bpy.utils.execfile(os.path.join(dirname, "keymap_data", "blender_default.py"))


def load():
    from bl_keymap_utils.io import keyconfig_init_from_data

    kc = bpy.context.window_manager.keyconfigs.new(idname)
    kc_prefs = kc.preferences

    keyconfig_data = blender_default.generate_keymaps(
        blender_default.Params(
            select_mouse=kc_prefs.select_mouse,
            spacebar_action=kc_prefs.spacebar_action,
            use_select_all_toggle=kc_prefs.use_select_all_toggle,
            use_v3d_tab_menu=kc_prefs.use_v3d_tab_menu,
        ),
    )
    keyconfig_init_from_data(kc, keyconfig_data)


if __name__ == "__main__":
    bpy.utils.register_class(Prefs)
    load()
