# SPDX-FileCopyrightText: 2024 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

bl_info = {
    "name": "AI Assistant",
    "author": "Blender Foundation",
    "version": (1, 0, 0),
    "blender": (4, 0, 0),
    "location": "3D View > Sidebar > AI",
    "description": "Chat with an AI that can control Blender",
    "support": "OFFICIAL",
    "category": "3D View",
}

if "bpy" in locals():
    import importlib
    importlib.reload(properties)
    importlib.reload(ai_backend)
    importlib.reload(operators)
    importlib.reload(gui)
else:
    from . import properties, ai_backend, operators, gui

import bpy


def register():
    properties.register()
    ai_backend.register()
    operators.register()
    gui.register()


def unregister():
    gui.unregister()
    operators.unregister()
    ai_backend.unregister()
    properties.unregister()


if __name__ == "__main__":
    register()
