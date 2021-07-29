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

from itertools import cycle, islice

import bpy

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import (dataCorrect, repeat_last)


class SvVertMaskNode(bpy.types.Node, SverchCustomTreeNode):
    '''Delete verts from mesh'''
    bl_idname = 'SvVertMaskNode'
    bl_label = 'Mask Vertices'
    bl_icon = 'OUTLINER_OB_EMPTY'

    def sv_init(self, context):
        self.inputs.new('StringsSocket', 'Mask')
        self.inputs.new('VerticesSocket', 'Vertices', 'Vertices')
        self.inputs.new('StringsSocket', 'Poly Egde', 'Poly Egde')

        self.outputs.new('VerticesSocket', 'Vertices', 'Vertices')
        self.outputs.new('StringsSocket', 'Poly Egde', 'Poly Egde')

    def process(self):
        if not any(s.is_linked for s in self.outputs):
            return
        if self.inputs['Vertices'].is_linked and self.inputs['Poly Egde'].is_linked:
            verts = self.inputs['Vertices'].sv_get()
            poly = self.inputs['Poly Egde'].sv_get()
            verts = dataCorrect(verts)
            poly = dataCorrect(poly)
            self.inputs['Mask'].sv_get(default=[[1, 0]])

            verts_out = []
            poly_edge_out = []

            if self.inputs['Mask'].is_linked:
                mask = self.inputs['Mask'].sv_get()
            else:
                mask = [[1, 0]]

            for ve, pe, ma in zip(verts, poly, repeat_last(mask)):
                current_mask = islice(cycle(ma), len(ve))
                vert_index = [i for i, m in enumerate(current_mask) if m]
                if len(vert_index) < len(ve):
                    index_set = set(vert_index)
                    vert_dict = {j: i for i, j in enumerate(vert_index)}
                    new_vert = [ve[i] for i in vert_index]
                    is_ss = index_set.issuperset
                    new_pe = [[vert_dict[n] for n in fe]
                              for fe in pe if is_ss(fe)]
                    verts_out.append(new_vert)
                    poly_edge_out.append(new_pe)

                else:  # no reprocessing needed
                    verts_out.append(ve)
                    poly_edge_out.append(pe)

            self.outputs['Vertices'].sv_set(verts_out)
            self.outputs['Poly Egde'].sv_set(poly_edge_out)


def register():
    bpy.utils.register_class(SvVertMaskNode)


def unregister():
    bpy.utils.unregister_class(SvVertMaskNode)
