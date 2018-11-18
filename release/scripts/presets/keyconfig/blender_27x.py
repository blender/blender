import os
import bpy
from bpy.props import (
    EnumProperty,
)

idname = os.path.splitext(os.path.basename(__file__))[0]

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

    def draw(self, layout):
        col = layout.column(align=True)
        col.label(text="Select With:")
        col.row().prop(self, "select_mouse", expand=True)

from bpy_extras.keyconfig_utils import (
    keyconfig_init_from_data,
    keyconfig_module_from_preset,
)

blender_default = keyconfig_module_from_preset(os.path.join("keymap_data", "blender_default"), __file__)

def load():
    kc = bpy.context.window_manager.keyconfigs.new(idname)
    kc_prefs = kc.preferences

    keyconfig_data = blender_default.generate_keymaps(
        blender_default.Params(
            select_mouse=kc_prefs.select_mouse,
            legacy=True,
        ),
    )
    keyconfig_init_from_data(kc, keyconfig_data)


if __name__ == "__main__":
    bpy.utils.register_class(Prefs)
    load()
