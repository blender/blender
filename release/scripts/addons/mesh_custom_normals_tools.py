# ***** BEGIN GPL LICENSE BLOCK *****
#
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ***** END GPL LICENCE BLOCK *****

bl_info = {
    "name": "Custom Normals Tools",
    "author": "Bastien Montagne (mont29)",
    "version": (0, 0, 1),
    "blender": (2, 75, 0),
    "location": "3DView > Tools",
    "description": "Various tools/helpers for custom normals",
    "warning": "",
    "support": 'OFFICIAL',
    "category": "Mesh",
}


import bpy


class MESH_OT_flip_custom_normals(bpy.types.Operator):
    """Flip active mesh's normals, including custom ones (only in Object mode)"""
    bl_idname = "mesh.flip_custom_normals"
    bl_label = "Flip Custom Normals"
    bl_options = {'UNDO'}

    @classmethod
    def poll(cls, context):
        return context.object and context.object.type == 'MESH' and context.object.mode == 'OBJECT'

    def execute(self, context):
        me = context.object.data

        if me.has_custom_normals:
            me.calc_normals_split()
            clnors = [0.0] * 3 * len(me.loops)
            me.loops.foreach_get("normal", clnors)

        bpy.ops.object.mode_set(mode='EDIT')
        bpy.ops.mesh.select_all(action='SELECT')
        bpy.ops.mesh.flip_normals()
        bpy.ops.object.mode_set(mode='OBJECT')

        me = context.object.data
        if me.has_custom_normals:
            clnors[:] = list(zip(*[(-n for n in clnors)] * 3))
            # We also have to take in account that the winding was reverted...
            for p in me.polygons:
                ls = p.loop_start + 1
                le = ls + p.loop_total - 1
                clnors[ls:le] = reversed(clnors[ls:le])
            me.normals_split_custom_set(clnors)

        context.scene.update()
        return {'FINISHED'}


def flip_custom_normals_draw_func(self, context):
    if isinstance(self, bpy.types.Panel):
        self.layout.label("Custom Normal Tools:")
    self.layout.operator(MESH_OT_flip_custom_normals.bl_idname)


def register():
    bpy.utils.register_module(__name__)
    bpy.types.VIEW3D_PT_tools_object.append(flip_custom_normals_draw_func)


def unregister():
    bpy.types.VIEW3D_PT_tools_object.remove(flip_custom_normals_draw_func)
    bpy.utils.unregister_module(__name__)


if __name__ == "__main__":
    register()
