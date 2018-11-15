import os
import bpy

userpref = bpy.context.user_preferences

from bpy_extras.keyconfig_utils import (
    keyconfig_import_from_data,
    keyconfig_module_from_preset,
)

_mod = keyconfig_module_from_preset(os.path.join("keymap_data", "blender_default"), __file__)
keyconfig_data = _mod.generate_keymaps(
    _mod.KeymapParams(
        select_mouse=userpref.inputs.select_mouse,
    ),
)

if __name__ == "__main__":
    kc = keyconfig_import_from_data(
        os.path.splitext(os.path.basename(__file__))[0],
        keyconfig_data,
    )
    kc.has_select_mouse = True  # Support switching select mouse
