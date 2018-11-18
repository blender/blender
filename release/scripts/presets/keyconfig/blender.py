import os
import bpy
from bpy.props import (
    BoolProperty,
)

userpref = bpy.context.user_preferences

idname = os.path.splitext(os.path.basename(__file__))[0]

def update(_self, _context):
    _load()


class Prefs(bpy.types.KeyConfigPreferences):
    bl_idname = idname

    use_select_all_toggle: BoolProperty(
        name="Select All Toggles",
        description=(
            "Causes select-all (A-key) to de-select in the case a selection exists"
        ),
        default=False,
        update=update,
    )

    def draw(self, layout):
        row = layout.row()
        row.prop(self, "use_select_all_toggle")


from bpy_extras.keyconfig_utils import (
    keyconfig_init_from_data,
    keyconfig_module_from_preset,
)

mod = keyconfig_module_from_preset(os.path.join("keymap_data", "blender_default"), __file__)

def _load():
    kc = bpy.context.window_manager.keyconfigs.new(idname)
    kc_prefs = kc.preferences

    keyconfig_data = mod.generate_keymaps(
        mod.KeymapParams(
            select_mouse=userpref.inputs.select_mouse,
            use_select_all_toggle=kc_prefs.use_select_all_toggle,
        ),
    )
    keyconfig_init_from_data(kc, keyconfig_data)
    kc.has_select_mouse = True  # Support switching select mouse


if __name__ == "__main__":
    bpy.utils.register_class(Prefs)
    _load()
