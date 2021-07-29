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

import collections

import bpy

from sverchok.node_tree import SverchCustomTreeNode


class SvSeparateMeshNodeMK2(bpy.types.Node, SverchCustomTreeNode):
    '''Separate Loose mesh parts'''
    bl_idname = 'SvSeparateMeshNodeMK2'
    bl_label = 'Separate Loose Parts MK2'
    bl_icon = 'OUTLINER_OB_EMPTY'

    def sv_init(self, context):
        self.inputs.new('VerticesSocket', 'Vertices')
        self.inputs.new('StringsSocket', 'Poly Egde')

        self.outputs.new('VerticesSocket', 'Vertices')
        self.outputs.new('StringsSocket', 'Poly Egde')
        self.outputs.new('StringsSocket', 'Vert idx')
        self.outputs.new('StringsSocket', 'Poly Egde idx')

    def process(self):
        if not any(s.is_linked for s in self.outputs):
            return
        verts = self.inputs['Vertices'].sv_get()
        poly = self.inputs['Poly Egde'].sv_get()
        verts_out = []
        poly_edge_out = []

        vert_index = []
        poly_edge_index = []

        for ve, pe in zip(verts, poly):
            # build links
            node_links = collections.defaultdict(set)
            for edge_face in pe:
                for i in edge_face:
                    node_links[i].update(edge_face)

            nodes = set(node_links.keys())
            n = nodes.pop()
            node_set_list = [set([n])]
            node_stack = collections.deque()
            node_stack_append = node_stack.append
            node_stack_pop = node_stack.pop
            node_set = node_set_list[-1]
            # find separate sets
            while nodes:
                for node in node_links[n]:
                    if node not in node_set:
                        node_stack_append(node)
                if not node_stack:  # new mesh part
                    n = nodes.pop()
                    node_set_list.append(set([n]))
                    node_set = node_set_list[-1]
                else:
                    while node_stack and n in node_set:
                        n = node_stack_pop()
                    nodes.discard(n)
                    node_set.add(n)
            # create new meshes from sets, new_pe is the slow line.
            if len(node_set_list) > 1:
                node_set_list.sort(key=lambda x: min(x))
                for idx, node_set in enumerate(node_set_list):
                    mesh_index = sorted(node_set)
                    vert_dict = {j: i for i, j in enumerate(mesh_index)}
                    new_vert = [ve[i] for i in mesh_index]
                    new_pe = [[vert_dict[n] for n in fe]
                              for fe in pe
                              if fe[0] in node_set]

                    verts_out.append(new_vert)
                    poly_edge_out.append(new_pe)
                    vert_index.append([idx for i in range(len(new_vert))])
                    poly_edge_index.append([idx for face in new_pe])
            elif node_set_list:  # no reprocessing needed
                verts_out.append(ve)
                poly_edge_out.append(pe)
                vert_index.append([0 for i in range(len(ve))])
                poly_edge_index.append([0 for face in pe])

        self.outputs['Vertices'].sv_set(verts_out)
        self.outputs['Poly Egde'].sv_set(poly_edge_out)
        self.outputs['Vert idx'].sv_set(vert_index)
        self.outputs['Poly Egde idx'].sv_set(poly_edge_index)


def register():
    bpy.utils.register_class(SvSeparateMeshNodeMK2)


def unregister():
    bpy.utils.unregister_class(SvSeparateMeshNodeMK2)
