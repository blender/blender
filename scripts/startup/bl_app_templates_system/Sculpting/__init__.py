# SPDX-FileCopyrightText: 2018-2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
from bpy.app.handlers import persistent


@persistent
def load_handler(_):
    import bpy
    # Apply subdivision modifier on startup
    bpy.ops.object.mode_set(mode='OBJECT')
    if bpy.app.opensubdiv.supported:
        bpy.ops.object.modifier_apply(modifier="Subdivision")
        bpy.ops.object.mode_set(mode='EDIT')
        bpy.ops.transform.tosphere(value=1.0)
    else:
        bpy.ops.object.modifier_remove(modifier="Subdivision")
        bpy.ops.object.mode_set(mode='EDIT')
        bpy.ops.mesh.subdivide(number_cuts=6, smoothness=1.0)
    bpy.ops.object.mode_set(mode='SCULPT')


def register():
    bpy.app.handlers.load_factory_startup_post.append(load_handler)


def unregister():
    bpy.app.handlers.load_factory_startup_post.remove(load_handler)
