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

import math
from random import random

import bpy
from bpy.props import StringProperty, BoolProperty, EnumProperty
import bmesh
from mathutils import Vector

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import updateNode
from sverchok.utils.sv_viewer_utils import matrix_sanitizer


def wipe_object(ob):
    ''' this removes all geometry '''
    bm = bmesh.new()
    bm.to_mesh(ob.data)
    bm.free()


class SvDupliInstancesMK4(bpy.types.Node, SverchCustomTreeNode):
    bl_idname = 'SvDupliInstancesMK4'
    bl_label = 'Dupli instancer mk4'
    bl_icon = 'OUTLINER_OB_EMPTY'

    def set_child_quota(self, context):
        # was used for string child property
        updateNode(self, context)

        # post update check
        if self.auto_release:
            parent = self.name_node_generated_parent
            if parent:
                for obj in bpy.data.objects[parent].children:
                    if not obj.name == self.name_child:
                        obj.parent = None

    name_node_generated_parent = StringProperty(
        description="name of the parent that this node generates",
        update=updateNode)

    scale = BoolProperty(default=False,
        description="scale children", update=updateNode)

    auto_release = BoolProperty(update=set_child_quota)

    def sv_init(self, context):
        #self.inputs.new("SvObjectSocket", "parent")
        self.inputs.new("SvObjectSocket", "child")
        self.inputs.new("MatrixSocket", "matr/vert")
        self.name_node_generated_parent = 'parant'

    def draw_buttons(self, context, layout):
        col = layout.column(align=True)
        col.prop(self, 'name_node_generated_parent', text='', icon='LOOPSEL')
        #col.prop_search(self, 'name_child', bpy.data, 'objects', text='')
        col.prop(self, 'scale', text='Scale children', toggle=True)
        col.prop(self, 'auto_release', text='One Object only', toggle=True)

    def process(self):
        #objectsP = self.inputs['parent'].sv_get(default=None)
        objectsC = self.inputs['child'].sv_get()
        transforms = self.inputs['matr/vert'].sv_get()
        objects = bpy.data.objects
        #if any([x.name == self.name_node_generated_parent for x in objects]):
        ob = objects.get(self.name_node_generated_parent)
        #self.name_node_generated_parent = ob.name

        if ob:
            wipe_object(ob)

        # minimum requirements.
        if (not transforms) and (not objectsC):
            if ob:
                ob.dupli_type = 'NONE'
            return

        if not ob:
            name = self.name_node_generated_parent
            mesh = bpy.data.meshes.new(name + '_mesh')
            ob = bpy.data.objects.new(name, mesh)
            bpy.context.scene.objects.link(ob)

        # at this point there's a reference to an ob, and the mesh is empty.
        child = self.inputs['child'].sv_get()[0]
        #print('проверка',child)


        if transforms and transforms[0]:
            # -- this mode will face duplicate --
            # i expect this can be done faster using numpy
            # please view this only as exploratory

            if self.inputs['matr/vert'].links[0].from_socket.bl_idname == 'VerticesSocket':
                transforms = transforms[0]
                # -- this mode will vertex duplicate --
                ob.data.from_pydata(transforms, [], [])
                ob.dupli_type = 'VERTS'
                child.parent = ob

            elif self.inputs['matr/vert'].links[0].from_socket.bl_idname == 'MatrixSocket':
                sin, cos = math.sin, math.cos

                theta = 2 * math.pi / 3
                thetb = theta * 2
                ofs = 0.5 * math.pi + theta

                A = Vector((cos(0 + ofs), sin(0 + ofs), 0))
                B = Vector((cos(theta + ofs), sin(theta + ofs), 0))
                C = Vector((cos(thetb + ofs), sin(thetb + ofs), 0))

                verts = []
                add_verts = verts.extend

                num_matrices = len(transforms)
                for m in transforms:
                    M = matrix_sanitizer(m)
                    add_verts([(M * A)[:], (M * B)[:], (M * C)[:]])

                strides = range(0, num_matrices * 3, 3)
                faces = [[i, i + 1, i + 2] for i in strides]

                ob.data.from_pydata(verts, [], faces)
                ob.dupli_type = 'FACES'
                ob.use_dupli_faces_scale = self.scale
                child.parent = ob


def register():
    bpy.utils.register_class(SvDupliInstancesMK4)


def unregister():
    bpy.utils.unregister_class(SvDupliInstancesMK4)

if __name__ == '__main__':
    register()