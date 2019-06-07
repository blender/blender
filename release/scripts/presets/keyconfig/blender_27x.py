import os
import bpy
from bpy.props import (
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
        default='RIGHT',
        update=update_fn,
    )

    def draw(self, layout):
        split = layout.split()
        col = split.column()
        col.label(text="Select With:")
        col.row().prop(self, "select_mouse", expand=True)
        split.column()


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
            spacebar_action='SEARCH',
            use_select_all_toggle=True,
            use_gizmo_drag=False,
            legacy=True,
        ),
    )

    if platform == 'darwin':
        from bl_keymap_utils.platform_helpers import keyconfig_data_oskey_from_ctrl_for_macos
        keyconfig_data = keyconfig_data_oskey_from_ctrl_for_macos(keyconfig_data)


    keyconfig_init_from_data(kc, keyconfig_data)


if __name__ == "__main__":
    bpy.utils.register_class(Prefs)
    load()
