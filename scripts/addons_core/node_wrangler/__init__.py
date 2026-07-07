# SPDX-FileCopyrightText: 2013-2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

bl_info = {
    "name": "Node Wrangler",
    # This is now displayed as the maintainer, so show the foundation.
    # "author": "Bartek Skorupa, Greg Zaal, Sebastian Koenig, Christian Brinkmann, Florian Meyer", # Original Authors
    "author": "Blender Foundation",
    "version": (4, 1, 0),
    "blender": (5, 0, 0),
    "location": "Node Editor Toolbar or Shift-W",
    "description": "Various tools to enhance and speed up node-based workflow",
    "warning": "",
    "doc_url": "{BLENDER_MANUAL_URL}/addons/node/node_wrangler.html",
    "support": 'OFFICIAL',
    "category": "Node",
}

import bpy
from bpy.props import (
    IntProperty,
    StringProperty,
)

from . import operators
from . import preferences
from . import interface


def register():
    # props
    bpy.types.Scene.NWBusyDrawing = StringProperty(
        name="Busy Drawing!",
        default="",
        description="An internal property used to store only the first mouse position")
    bpy.types.Scene.NWLazySource = StringProperty(
        name="Lazy Source!",
        default="x",
        description="An internal property used to store the first node in a Lazy Connect operation")
    bpy.types.Scene.NWLazyTarget = StringProperty(
        name="Lazy Target!",
        default="x",
        description="An internal property used to store the last node in a Lazy Connect operation")
    bpy.types.Scene.NWSourceSocket = IntProperty(
        name="Source Socket!",
        default=0,
        description="An internal property used to store the source socket in a Lazy Connect operation")

    operators.register()
    interface.register()
    preferences.register()


def unregister():
    preferences.unregister()
    interface.unregister()
    operators.unregister()

    # props
    del bpy.types.Scene.NWBusyDrawing
    del bpy.types.Scene.NWLazySource
    del bpy.types.Scene.NWLazyTarget
    del bpy.types.Scene.NWSourceSocket
