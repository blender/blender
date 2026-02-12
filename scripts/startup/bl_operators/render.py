# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

from bpy.types import Operator


class RENDER_OT_swap_dimensions(Operator):
    bl_label = "Swap Dimensions"
    bl_idname = 'render.swap_dimensions'
    bl_description = "Flip X and Y resolutions"
    bl_options = {'INTERNAL'}

    def execute(self, context):
        rd = context.scene.render
        rd.resolution_x, rd.resolution_y = (rd.resolution_y, rd.resolution_x)

        return {'FINISHED'}


classes = (RENDER_OT_swap_dimensions,)
