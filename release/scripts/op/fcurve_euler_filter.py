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

# <pep8 compliant>

import bpy


def main(context):
    from math import pi

    def cleanupEulCurve(fcv):
        keys = []

        for k in fcv.keyframe_points:
            keys.append([k.handle_left.copy(), k.co.copy(), k.handle_right.copy()])

        for i in range(len(keys)):
            cur = keys[i]
            prev = keys[i - 1] if i > 0 else None
            next = keys[i + 1] if i < len(keys) - 1 else None

            if prev is None:
                continue

            th = pi
            if abs(prev[1][1] - cur[1][1]) >= th:  # more than 180 degree jump
                fac = pi * 2.0
                if prev[1][1] > cur[1][1]:
                    while abs(cur[1][1] - prev[1][1]) >= th:  # < prev[1][1]:
                        cur[0][1] += fac
                        cur[1][1] += fac
                        cur[2][1] += fac
                elif prev[1][1] < cur[1][1]:
                    while abs(cur[1][1] - prev[1][1]) >= th:
                        cur[0][1] -= fac
                        cur[1][1] -= fac
                        cur[2][1] -= fac

        for i in range(len(keys)):
            for x in range(2):
                fcv.keyframe_points[i].handle_left[x] = keys[i][0][x]
                fcv.keyframe_points[i].co[x] = keys[i][1][x]
                fcv.keyframe_points[i].handle_right[x] = keys[i][2][x]

    flist = bpy.context.active_object.animation_data.action.fcurves
    for f in flist:
        if f.select and f.data_path.endswith("rotation_euler"):
            cleanupEulCurve(f)


class DiscontFilterOp(bpy.types.Operator):
    """Fixes the most common causes of gimbal lock in the fcurves of the active bone"""
    bl_idname = "graph.euler_filter"
    bl_label = "Filter out discontinuities in the active fcurves"

    @classmethod
    def poll(cls, context):
        return context.active_object != None

    def execute(self, context):
        main(context)
        return {'FINISHED'}


def register():
    bpy.utils.register_module(__name__)


def unregister():
    bpy.utils.unregister_module(__name__)

if __name__ == "__main__":
    register()
