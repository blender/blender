# SPDX-FileCopyrightText: 2011-2022 Blender Foundation
#
# SPDX-License-Identifier: Apache-2.0

bl_info = {
    "name": "Hydra Storm render engine",
    "author": "AMD",
    "version": (1, 0, 0),
    "blender": (4, 0, 0),
    "description": "USD's high performance rasterizing renderer",
    "tracker_url": "",
    "doc_url": "",
    "community": "",
    "downloads": "",
    "main_web": "",
    "support": 'OFFICIAL',
    "category": "Render"
}


from . import engine, properties, ui


def register():
    engine.register()
    properties.register()
    ui.register()


def unregister():
    ui.unregister()
    properties.unregister()
    engine.unregister()
