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

import numpy as np

import bpy
import bmesh
# import mathutils
# from mathutils import Vector
from bpy.props import FloatProperty, IntProperty
from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import updateNode
from sverchok.utils.sv_bmesh_utils import bmesh_from_pydata, pydata_from_bmesh

class SvHomogenousVectorField(bpy.types.Node, SverchCustomTreeNode):
    ''' hv evenly spaced vfield '''
    bl_idname = 'SvHomogenousVectorField'
    bl_label = 'Vector P Field'

    xdim__ = IntProperty(default=2, min=1, update=updateNode)
    ydim__ = IntProperty(default=3, min=1, update=updateNode)
    zdim__ = IntProperty(default=4, min=1, update=updateNode)
    sizex__ = FloatProperty(default=1.0, min=.01, update=updateNode)
    sizey__ = FloatProperty(default=1.0, min=.01, update=updateNode)
    sizez__ = FloatProperty(default=1.0, min=.01, update=updateNode)
    seed = IntProperty(default=0, min=0, update=updateNode)

    randomize_factor = FloatProperty(name='randomize', default=0.0, min=0.0, update=updateNode)
    rm_doubles_distance = FloatProperty(name='rm distance', default=0.0, update=updateNode)

    def sv_init(self, context):
        self.inputs.new("StringsSocket", "xdim").prop_name='xdim__'
        self.inputs.new("StringsSocket", "ydim").prop_name='ydim__'
        self.inputs.new("StringsSocket", "zdim").prop_name='zdim__'
        self.inputs.new("StringsSocket", "size x").prop_name='sizex__'
        self.inputs.new("StringsSocket", "size y").prop_name='sizey__'
        self.inputs.new("StringsSocket", "size z").prop_name='sizez__'
        self.outputs.new("VerticesSocket", "verts")

    def draw_buttons(self, context, layout):
        col = layout.column(align=True)
        col.prop(self, 'randomize_factor')
        col.prop(self, 'rm_doubles_distance')
        col.prop(self, 'seed')

    def process(self):

        xdims, ydims, zdims, sizesx, sizesy, sizesz = [s.sv_get()[0] for s in self.inputs]

        verts = []
        for xdim, ydim, zdim, *size in zip(xdims, ydims, zdims, sizesx, sizesy, sizesz):
            hs0 = size[0] / 2
            hs1 = size[1] / 2
            hs2 = size[2] / 2


            x_ = np.linspace(-hs0, hs0, xdim)
            y_ = np.linspace(-hs1, hs1, ydim)
            z_ = np.linspace(-hs2, hs2, zdim)

            f = np.vstack(np.meshgrid(x_,y_,z_)).reshape(3,-1).T
            num_items = f.shape[0]* f.shape[1]

            if self.randomize_factor > 0.0:
                np.random.seed(self.seed)
                noise = (np.random.normal(0, 0.5, num_items) * self.randomize_factor).reshape(3,-1).T
                f += noise

            f = f.tolist()
            if self.rm_doubles_distance > 0.0:
                bm = bmesh_from_pydata(f, [], [])
                bmesh.ops.remove_doubles(bm, verts=bm.verts[:], dist=self.rm_doubles_distance)
                f, _, _ = pydata_from_bmesh(bm)

            verts.append(f)

        if verts:
            self.outputs['verts'].sv_set(verts)

def register():
    bpy.utils.register_class(SvHomogenousVectorField)


def unregister():
    bpy.utils.unregister_class(SvHomogenousVectorField)