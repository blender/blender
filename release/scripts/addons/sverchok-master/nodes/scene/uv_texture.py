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

import bpy
from bpy.props import StringProperty, EnumProperty, BoolProperty
import bmesh
from sverchok.utils.sv_bmesh_utils import *
import numpy as np

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import updateNode

class SvUVtextureNode(bpy.types.Node, SverchCustomTreeNode):
    ''' UV texture node '''
    bl_idname = 'SvUVtextureNode'
    bl_label = 'UVtextures'
    bl_icon = 'MATERIAL'

    def sv_init(self, context):
        self.inputs.new('SvObjectSocket', "Object", "Object")
        self.outputs.new('VerticesSocket', "Verts", "Verts")
        self.outputs.new('StringsSocket', "Pols", "Pols")

    def avail_objects(self, context):
        items = [('','','')]
        if self.inputs and self.inputs[0].is_linked:
            objects = self.inputs[0].sv_get()
            items = [(obj.name, obj.name, '') for obj in objects]
        return items

    def avail_uvs(self, context):
        items = [('','','')]
        if self.inputs and self.inputs[0].is_linked:
            obj = bpy.data.objects[self.objects]
            if obj.data.uv_layers:
                items = [(p.name, p.name, "") for p in obj.data.uv_layers]
        return items

    objects = EnumProperty(items=avail_objects, name="Objects",
        description="Choose Objects", update=updateNode)
    uv = EnumProperty(items=avail_uvs, name="UV",
        description="Choose UV to load", update=updateNode)

    def draw_buttons(self, context, layout):
        layout.prop(self, 'uv', 'uv')


    def update(self):
        pass

    def UV(self, object, uv):
        # makes UV from layout texture area to sverchok vertices and polygons.
        mesh = object.data
        bm = bmesh.new()
        bm.from_mesh(mesh)
        uv_layer = bm.loops.layers.uv[uv]

        nFaces = len(bm.faces)
        bm.verts.ensure_lookup_table()
        bm.faces.ensure_lookup_table()

        vertices_dict = {}
        polygons_new = []
        areas = []
        for fi in range(nFaces):
            polygons_new_pol = []
            areas.append(bm.faces[fi].calc_area())
            
            for loop in bm.faces[fi].loops:
                li = loop.index
                polygons_new_pol.append(li)
                vertices_dict[li] = list(loop[uv_layer].uv[:])+[0]
            polygons_new.append(polygons_new_pol)

        vertices_new = [i for i in vertices_dict.values()]

        bm_roll = bmesh_from_pydata(verts=vertices_new,edges=[],faces=polygons_new)
        bm_roll.verts.ensure_lookup_table()
        bm_roll.faces.ensure_lookup_table()
        areas_roll = []
        for fi in range(nFaces):
            areas_roll.append(bm_roll.faces[fi].calc_area())

        np_area_origin = np.array(areas).mean()
        np_area_roll = np.array(areas_roll).mean()
        mult = np.sqrt(np_area_origin/np_area_roll)

        np_ver = np.array(vertices_new)
        #(print(np_area_origin,np_area_roll,mult,'плориг, плразв, множитель'))
        vertices_new = (np_ver*mult).tolist()
        bm.clear()
        del bm
        bm_roll.clear()
        del bm_roll
        return [vertices_new], [polygons_new]

    def process(self):
        if self.inputs and self.inputs[0].is_linked:
            obj = bpy.data.objects[self.objects]
            if not self.uv:
                print ('!!! for node:',self.name,'!!! object',self.objects,'have no UV')
                if self.outputs and self.outputs[0].is_linked:
                    self.outputs[0].sv_set([[]])
                return
            uv = self.uv
            v,p = self.UV(obj,uv)

            if self.outputs and self.outputs[0].is_linked:
                self.outputs[0].sv_set(v)
            if self.outputs and self.outputs[1].is_linked:
                self.outputs[1].sv_set(p)


def register():
    bpy.utils.register_class(SvUVtextureNode)


def unregister():
    bpy.utils.unregister_class(SvUVtextureNode)

if __name__ == '__main__':
    register()
