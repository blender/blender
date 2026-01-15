# SPDX-FileCopyrightText: 2024 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

bl_info = {
    "name": "Blender MCP Chat",
    "author": "Blender Foundation",
    "version": (1, 0, 0),
    "blender": (4, 0, 0),
    "location": "3D View > Sidebar > Chat",
    "description": "Connect Blender to AI assistants via MCP protocol with chat sidebar",
    "support": "OFFICIAL",
    "category": "3D View",
}

if "bpy" in locals():
    import importlib
    importlib.reload(properties)
    importlib.reload(server)
    importlib.reload(handlers)
    importlib.reload(integrations)
    importlib.reload(gui)
    importlib.reload(operators)
else:
    from . import properties, server, handlers, integrations, gui, operators

import bpy


def register():
    properties.register()
    handlers.register()
    integrations.register()
    operators.register()
    gui.register()
    server.register()


def unregister():
    server.unregister()
    gui.unregister()
    operators.unregister()
    integrations.unregister()
    handlers.unregister()
    properties.unregister()


if __name__ == "__main__":
    register()
