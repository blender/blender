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

from itertools import chain

import bpy
from sverchok.node_tree import SverchCustomTreeNode


class SvDeleteLooseNode(bpy.types.Node, SverchCustomTreeNode):
    '''Delete vertices not used in face or edge'''
    bl_idname = 'SvDeleteLooseNode'
    bl_label = 'Delete Loose'
    bl_icon = 'OUTLINER_OB_EMPTY'

    def sv_init(self, context):
        self.inputs.new('VerticesSocket', 'Vertices', 'Vertices')
        self.inputs.new('StringsSocket', 'PolyEdge', 'PolyEdge')

        self.outputs.new('VerticesSocket', 'Vertices', 'Vertices')
        self.outputs.new('StringsSocket', 'PolyEdge', 'PolyEdge')

    def process(self):

        verts = self.inputs['Vertices'].sv_get()
        poly_edge = self.inputs['PolyEdge'].sv_get()
        verts_out = []
        poly_edge_out = []
        for ve, pe in zip(verts, poly_edge):
            """
            Okay so honestly this still upsets me, I will comment it out
            for now. If somebody wants this to work please give it a go,
            but dump broken codde into master with a comment,
            like below...

            # trying to remove indeces of polygons that more that length of
            # vertices list. But it doing wrong, ideces not mutch vertices...
            # what am i doing wrong?
            # i guess, i didn't understood this iterations at all

            delp = []
            for p in pe:
                deli = []
                for i in p:
                    if i >= len(ve):
                        deli.append(i)
                if deli and (len(p)-len(deli)) >= 2:
                    print(deli)
                    for k in deli:
                        p.remove(k)
                elif (len(p)-len(deli)) <= 1:
                    delp.append(p)
            if delp:
                for d in delp:
                    pe.remove(d)
            """
            indx = set(chain.from_iterable(pe))
            verts_out.append([v for i, v in enumerate(ve) if i in indx])
            v_index = dict([(j, i) for i, j in enumerate(sorted(indx))])
            poly_edge_out.append([list(map(lambda n:v_index[n], p)) for p in pe])

        if self.outputs['Vertices'].is_linked:
            self.outputs['Vertices'].sv_set(verts_out)
        if poly_edge_out:
            self.outputs['PolyEdge'].sv_set(poly_edge_out)


def register():
    bpy.utils.register_class(SvDeleteLooseNode)


def unregister():
    bpy.utils.unregister_class(SvDeleteLooseNode)
