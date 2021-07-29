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
from bpy.props import FloatProperty
from sverchok.node_tree import SverchCustomTreeNode, StringsSocket, VerticesSocket
from sverchok.data_structure import updateNode, Vector_generate


class VectorMoveNode(bpy.types.Node, SverchCustomTreeNode):
    ''' Vector Move vectors '''
    bl_idname = 'VectorMoveNode'
    bl_label = 'Move'
    bl_icon = 'MAN_TRANS'

    replacement_nodes = [('SvMoveNodeMK2', None, None)]

    mult_ = FloatProperty(name='multiplier',
                          default=1.0,
                          options={'ANIMATABLE'}, update=updateNode)

    def sv_init(self, context):
        self.inputs.new('VerticesSocket', "vertices", "vertices")
        self.inputs.new('VerticesSocket', "vectors", "vectors")
        self.inputs.new('StringsSocket', "multiplier", "multiplier").prop_name = 'mult_'
        self.outputs.new('VerticesSocket', "vertices", "vertices")

    def process(self):
        # inputs
        vers_ = self.inputs['vertices'].sv_get()
        vers = Vector_generate(vers_)
        vecs_ = self.inputs['vectors'].sv_get()
        vecs = Vector_generate(vecs_)
        mult = self.inputs['multiplier'].sv_get()

        mov = self.moved(vers, vecs, mult)
        self.outputs['vertices'].sv_set(mov)

    def moved(self, vers, vecs, mult):
        r = len(vers) - len(vecs)
        rm = len(vers) - len(mult)
        moved = []
        if r > 0:
            vecs.extend([vecs[-1] for a in range(r)])
        if rm > 0:
            mult.extend([mult[-1] for a in range(rm)])
        for i, ob in enumerate(vers):       # object
            d = len(ob) - len(vecs[i])
            dm = len(ob) - len(mult[i])
            if d > 0:
                vecs[i].extend([vecs[i][-1] for a in range(d)])
            if dm > 0:
                mult[i].extend([mult[i][-1] for a in range(dm)])
            temp = []
            for k, vr in enumerate(ob):     # vectors
                v = ((vr + vecs[i][k]*mult[i][k]))[:]
                temp.append(v)   # [0]*mult[0], v[1]*mult[0], v[2]*mult[0]))
            moved.append(temp)
        return moved


def register():
    bpy.utils.register_class(VectorMoveNode)

def unregister():
    bpy.utils.unregister_class(VectorMoveNode)
