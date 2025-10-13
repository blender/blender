# SPDX-FileCopyrightText: 2009-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

from __future__ import annotations

# support reloading sub-modules
if "bpy" in locals():
    from importlib import reload
    _modules_loaded[:] = [reload(val) for val in _modules_loaded]
    del reload

_modules = [
    "add_mesh_torus",
    "anim",
    "assets",
    "bone_selection_sets",
    "clip",
    "connect_to_output",
    "console",
    "constraint",
    "copy_global_transform",
    "file",
    "geometry_nodes",
    "grease_pencil",
    "image",
    "image_as_planes",
    "mesh",
    "node",
    "object",
    "object_align",
    "object_quick_effects",
    "object_randomize_transform",
    "presets",
    "rigidbody",
    "screen_play_rendered_anim",
    "sequencer",
    "spreadsheet",
    "userpref",
    "uvcalc_follow_active",
    "uvcalc_lightmap",
    "uvcalc_transform",
    "vertexpaint_dirt",
    "view3d",
    "world",
    "wm",
]

import bpy

if bpy.app.build_options.freestyle:
    _modules.append("freestyle")

__import__(name=__name__, fromlist=_modules)
_namespace = globals()
_modules_loaded = [_namespace[name] for name in _modules]
del _namespace


def register():
    from bpy.utils import register_class
    from . import (
        bone_selection_sets,
        copy_global_transform,
    )

    for mod in _modules_loaded:
        for cls in mod.classes:
            register_class(cls)

    bone_selection_sets.register()
    copy_global_transform.register()


def unregister():
    from bpy.utils import unregister_class
    from . import (
        bone_selection_sets,
        copy_global_transform,
    )

    bone_selection_sets.unregister()
    copy_global_transform.unregister()

    for mod in reversed(_modules_loaded):
        for cls in reversed(mod.classes):
            if cls.is_registered:
                unregister_class(cls)
