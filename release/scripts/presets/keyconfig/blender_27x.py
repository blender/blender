import os
from bpy_extras.keyconfig_utils import (
    keyconfig_import_from_data,
    keyconfig_module_from_preset,
)

_mod = keyconfig_module_from_preset(os.path.join("keymap_data", "blender_default"), __file__)
keyconfig_data = _mod.generate_keymaps(_mod.KeymapParams(legacy=True))

if __name__ == "__main__":
    keyconfig_import_from_data("blender_27x", keyconfig_data)
