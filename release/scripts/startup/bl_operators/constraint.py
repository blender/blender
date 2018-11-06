# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8-80 compliant>

import bpy
from bpy.types import (
    Operator,
)
from bpy.props import (
    IntProperty,
)


class CONSTRAINT_OT_add_target(Operator):
    """Add a target to the constraint"""
    bl_idname = "constraint.add_target"
    bl_label = "Add Target"
    bl_options = {'UNDO', 'INTERNAL'}

    def execute(self, context):
        context.constraint.targets.new()
        return {'FINISHED'}


class CONSTRAINT_OT_remove_target(Operator):
    """Remove the target from the constraint"""
    bl_idname = "constraint.remove_target"
    bl_label = "Remove Target"
    bl_options = {'UNDO', 'INTERNAL'}

    index: IntProperty()

    def execute(self, context):
        tgts = context.constraint.targets
        tgts.remove(tgts[self.index])
        return {'FINISHED'}


class CONSTRAINT_OT_normalize_target_weights(Operator):
    """Normalize weights of all target bones"""
    bl_idname = "constraint.normalize_target_weights"
    bl_label = "Normalize Weights"
    bl_options = {'UNDO', 'INTERNAL'}

    def execute(self, context):
        tgts = context.constraint.targets
        total = sum(t.weight for t in tgts)

        if total > 0:
            for t in tgts:
                t.weight = t.weight / total

        return {'FINISHED'}


classes = (
    CONSTRAINT_OT_add_target,
    CONSTRAINT_OT_remove_target,
    CONSTRAINT_OT_normalize_target_weights,
)
