import bpy

addon_keymaps = []

def register():
    if not canRegisterKeymaps(): return

    wm = bpy.context.window_manager
    km = wm.keyconfigs.addon.keymaps.new(name = "Node Editor", space_type = "NODE_EDITOR")

    # Open the ctrl-A search menu
    km.keymap_items.new("an.node_search", type = "A", value = "PRESS", ctrl = True)

    # Open the context sensitive pie menu
    kmi = km.keymap_items.new("wm.call_menu_pie", type = "W", value = "PRESS")
    kmi.properties.name = "an.context_pie"

    # Move view to subprogram nodes
    km.keymap_items.new("an.network_navigation", type = "TAB", value = "PRESS")

    # Selection Pie Menu
    kmi = km.keymap_items.new("wm.call_menu_pie", type = "E", value = "PRESS")
    kmi.properties.name = "an.selection_pie"

    # Floating Node Settings
    km.keymap_items.new("an.floating_node_settings_panel", type = "U", value = "PRESS")

    # Deactivate Auto Execution
    km.keymap_items.new("an.deactivate_auto_execution", type = "Q", value = "PRESS", ctrl = True, shift = True)

    addon_keymaps.append(km)

def unregister():
    if not canRegisterKeymaps(): return

    wm = bpy.context.window_manager
    for km in addon_keymaps:
        for kmi in km.keymap_items:
            km.keymap_items.remove(kmi)
        wm.keyconfigs.addon.keymaps.remove(km)
    addon_keymaps.clear()

def canRegisterKeymaps():
    return not bpy.app.background
