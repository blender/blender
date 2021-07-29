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

import parser

import bpy
from bpy.props import StringProperty

from sverchok.node_tree import SverchCustomTreeNode, StringsSocket
from sverchok.data_structure import updateNode, SvSetSocketAnyType, SvGetSocketAnyType
from math import cos, sin, pi, tan


class FormulaNode(bpy.types.Node, SverchCustomTreeNode):
    ''' Formula '''
    bl_idname = 'FormulaNode'
    bl_label = 'Formula'
    bl_icon = 'OUTLINER_OB_EMPTY'

    formula = StringProperty(name='formula',
                             default='x*n[0]',
                             update=updateNode)

    def draw_buttons(self, context, layout):
        layout.prop(self, "formula", text="formula")

    def sv_init(self, context):
        self.inputs.new('StringsSocket', "X", "X")
        self.inputs.new('StringsSocket', "n[.]", "n[.]")
        self.outputs.new('StringsSocket', "Result", "Result")


    def update(self):
        # inputs
        if not self.inputs:
            return
        if self.inputs[-1].links:
            self.inputs.new('StringsSocket', 'n[.]')
        else:
            while len(node.inputs) > min and not node.inputs[-2].links:
                node.inputs.remove(node.inputs[-1])

    def process(self):
        vecs = self.inputs['X'].sv_get(default=[[0]])
        
        list_mult = []
        for idx, socket in enumerate(self.inputs[1:-1]):
            if socket.is_linked and \
               type(socket.links[0].from_socket) == StringsSocket:

                mult = socket.sv_get()
                list_mult.extend(mult)
            else:
                list_mult.extend([[0]])
                
        if len(list_mult) == 0:
            list_mult = [[0.0]]

        # outputs
        if self.outputs['Result'].is_linked:

            code_formula = parser.expr(self.formula).compile()
            r_ = []
            result = []
            max_l = 0
            for list_m in list_mult:
                l1 = len(list_m)
                max_l = max(max_l, l1)
            max_l = max(max_l, len(vecs[0]))

            for list_m in list_mult:
                d = max_l - len(list_m)
                if d > 0:
                    for d_ in range(d):
                        list_m.append(list_m[-1])

            lres = []
            for l in range(max_l):
                ltmp = []
                for list_m in list_mult:
                    ltmp.append(list_m[l])
                lres.append(ltmp)

            r = self.inte(vecs, code_formula, lres)

            result.extend(r)
            SvSetSocketAnyType(self, 'Result', result)

    def inte(self, l, formula, list_n, indx=0):
        if type(l) in [int, float]:
            x = X = l

            n = list_n[indx]
            N = n

            t = eval(formula)
        else:
            t = []
            for idx, i in enumerate(l):
                j = self.inte(i, formula, list_n, idx)
                t.append(j)
            if type(l) == tuple:
                t = tuple(t)
        return t


def register():
    bpy.utils.register_class(FormulaNode)


def unregister():
    bpy.utils.unregister_class(FormulaNode)

if __name__ == '__main__':
    register()
