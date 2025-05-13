# SPDX-FileCopyrightText: 2024 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy


def bake(context):
    scene = context.scene
    cscene = scene.cycles
    image = bpy.data.images["bake_result"]

    if scene.render.bake.target == 'VERTEX_COLORS':
        # Bake to the vertex group first, then bake to a image
        #
        # TODO: The code currently works under the assumption that
        # a color attribute node with the baked attribute is connected
        # to the material output node. This limits the type of passes
        # that can be baked to a vertex group to specific data passes.
        # Look at https://projects.blender.org/blender/blender-test-data/pulls/73
        # for ideas on how to improve this.
        bpy.ops.object.bake(type=cscene.bake_type)
        scene.render.bake.target = 'IMAGE_TEXTURES'
        cscene.bake_type = 'COMBINED'

    if scene.render.use_bake_multires:
        # Multires baking calls a different function to bake images.
        bpy.ops.object.bake_image()
    else:
        bpy.ops.object.bake(type=cscene.bake_type)
    # TODO(sergey): This is currently corresponding to how
    # regular rendering pipeline saves images.
    image.save_render(scene.render.filepath + '0001.png', scene=scene)


if __name__ == "__main__":
    bake(bpy.context)
