# mesh_relax.py Copyright (C) 2010, Fabian Fricke
#
# Relaxes selected vertices while retaining the shape as much as possible
#
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
    "name": "Relax",
    "author": "Fabian Fricke",
    "version": (1, 1),
    "blender": (2, 57, 0),
    "location": "View3D > Specials > Relax ",
    "description": "Relax the selected verts while retaining the shape",
    "warning": "",
    "wiki_url": "http://wiki.blender.org/index.php/Extensions:2.6/Py/"
                "Scripts/Modeling/Relax",
    "category": "Mesh",
}

"""
Usage:

Launch from "W-menu" or from "Mesh -> Vertices -> Relax"


Additional links:
    Author Site: http://frigi.designdevil.de
    e-mail: frigi.f {at} gmail {dot} com
"""


import bpy
from bpy.props import IntProperty

def relax_mesh(context):

    # deselect everything that's not related
    for obj in context.selected_objects:
        obj.select = False

    # get active object
    obj = context.active_object

    # duplicate the object so it can be used for the shrinkwrap modifier
    obj.select = True # make sure the object is selected!
    bpy.ops.object.mode_set(mode='OBJECT')
    bpy.ops.object.duplicate()
    target = context.active_object

    # remove all other modifiers from the target
    for m in range(0, len(target.modifiers)):
        target.modifiers.remove(target.modifiers[0])

    context.scene.objects.active = obj

    sw = obj.modifiers.new(type='SHRINKWRAP', name='relax_target')
    sw.target = target

    # run smooth operator to relax the mesh
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.vertices_smooth()
    bpy.ops.object.mode_set(mode='OBJECT')

    # apply the modifier
    bpy.ops.object.modifier_apply(modifier='relax_target')

    # delete the target object
    obj.select = False
    target.select = True
    bpy.ops.object.delete()

    # go back to initial state
    obj.select = True
    bpy.ops.object.mode_set(mode='EDIT')

class Relax(bpy.types.Operator):
    """Relaxes selected vertices while retaining the shape """ \
    """as much as possible"""
    bl_idname = 'mesh.relax'
    bl_label = 'Relax'
    bl_options = {'REGISTER', 'UNDO'}

    iterations = IntProperty(name="Relax iterations",
                default=1, min=0, max=100, soft_min=0, soft_max=10)

    @classmethod
    def poll(cls, context):
        obj = context.active_object
        return (obj and obj.type == 'MESH')

    def execute(self, context):
        for i in range(0,self.iterations):
            relax_mesh(context)
        return {'FINISHED'}


def menu_func(self, context):
    self.layout.operator(Relax.bl_idname, text="Relax")


def register():
    bpy.utils.register_module(__name__)

    bpy.types.VIEW3D_MT_edit_mesh_specials.append(menu_func)
    bpy.types.VIEW3D_MT_edit_mesh_vertices.append(menu_func)

def unregister():
    bpy.utils.unregister_module(__name__)

    bpy.types.VIEW3D_MT_edit_mesh_specials.remove(menu_func)
    bpy.types.VIEW3D_MT_edit_mesh_vertices.remove(menu_func)

if __name__ == "__main__":
    register()
