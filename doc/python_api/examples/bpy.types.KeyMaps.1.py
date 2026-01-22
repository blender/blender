"""
Add-on Keymap Registration
++++++++++++++++++++++++++

This example shows how an add-on can register custom keyboard shortcuts.
Keymaps are added to ``keyconfigs.addon`` and removed when unregistered.

Store ``(keymap, keymap_item)`` tuples for safe cleanup, as multiple add-ons may use the same keymap.

.. note::

   Users can customize add-on shortcuts in the Keymap Preferences.
   Add-on keymaps appear under their respective editors and can be
   modified or disabled without editing the add-on code.

   Add-ons should only manipulate keymaps in ``keyconfigs.addon`` and not manipulate the user's keymaps
   because add-on keymaps serve as a default which users may customize.
   Modifying user keymaps directly interferes with users' own preferences.

.. warning::

   Add-ons can add items to existing modal keymaps but cannot create
   new modal keymaps via Python. Use ``modal=True`` when targeting
   an existing modal keymap such as "Knife Tool Modal Map".
"""

# In this example keymap registration functions are only split out for clarity,
# so skipping keymap registration in background mode doesn't interfere with other registration logic.

import bpy

# Store (keymap, keymap_item) for cleanup on unregister.
addon_keymaps = []


def register_keymaps():
    wm = bpy.context.window_manager
    kc = wm.keyconfigs.addon
    if kc is None:
        return  # Can be None in background mode.

    # Target the 3D View; name must match Blender's built-in keymap exactly.
    km = kc.keymaps.new(name="3D View", space_type='VIEW_3D')

    # Bind Shift+Alt+K to frame selected objects.
    kmi = km.keymap_items.new(
        idname="view3d.view_selected",
        type='K',
        value='PRESS',
        shift=True,
        alt=True,
    )
    kmi.properties.use_all_regions = True

    addon_keymaps.append((km, kmi))


def unregister_keymaps():
    for km, kmi in addon_keymaps:
        km.keymap_items.remove(kmi)
    addon_keymaps.clear()


def register():
    register_keymaps()


def unregister():
    unregister_keymaps()


if __name__ == "__main__":
    register()
