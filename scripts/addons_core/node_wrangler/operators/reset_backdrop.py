# SPDX-FileCopyrightText: 2025 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
from bpy.types import Operator

from ..utils.nodes import (
    NWBase,
    nw_check,
    nw_check_space_type,
)


#### ------------------------------ OPERATORS ------------------------------ ####

class NODE_OT_reset_backdrop(Operator, NWBase):
    """Reset the zoom and position of the background image"""
    bl_idname = 'node.nw_bg_reset'
    bl_label = 'Reset Backdrop'
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        return nw_check(cls, context) and nw_check_space_type(cls, context, {'CompositorNodeTree'})

    def execute(self, context):
        context.space_data.backdrop_zoom = 1
        context.space_data.backdrop_offset[0] = 0
        context.space_data.backdrop_offset[1] = 0
        return {'FINISHED'}
