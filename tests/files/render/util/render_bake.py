# SPDX-FileCopyrightText: 2024 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy


def bake(context):
    scene = context.scene
    cscene = scene.cycles
    image = bpy.data.images["bake_result"]
    bpy.ops.object.bake(type=cscene.bake_type)
    # TODO(sergey): This is currently corresponding to how
    # regular rendering pipeline saves images.
    image.save_render(scene.render.filepath + '0001.png', scene=scene)


if __name__ == "__main__":
    bake(bpy.context)
