# SPDX-License-Identifier: GPL-2.0-or-later

import os
import bpy


# ------------------------------------------------------------------------------
# Keymap

DIRNAME, FILENAME = os.path.split(__file__)
IDNAME = os.path.splitext(FILENAME)[0]


def update_fn(_self, _context):
    load()


industry_compatible = bpy.utils.execfile(os.path.join(DIRNAME, "keymap_data", "industry_compatible_data.py"))


def load():
    from sys import platform
    from bl_keymap_utils.io import keyconfig_init_from_data

    prefs = bpy.context.preferences

    kc = bpy.context.window_manager.keyconfigs.new(IDNAME)
    params = industry_compatible.Params(use_mouse_emulate_3_button=prefs.inputs.use_mouse_emulate_3_button)
    keyconfig_data = industry_compatible.generate_keymaps(params)

    if platform == 'darwin':
        from bl_keymap_utils.platform_helpers import keyconfig_data_oskey_from_ctrl_for_macos
        keyconfig_data = keyconfig_data_oskey_from_ctrl_for_macos(keyconfig_data)

    keyconfig_init_from_data(kc, keyconfig_data)


if __name__ == "__main__":
    load()
