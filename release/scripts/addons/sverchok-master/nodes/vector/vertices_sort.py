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

from operator import itemgetter

import bpy
from bpy.props import EnumProperty
from mathutils import Matrix, Vector
from mathutils.geometry import intersect_point_line

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import (repeat_last, Matrix_generate, Vector_generate,
                                     updateNode)


# distance between two points without sqrt, for comp only
def distK(v1, v2):
    return sum((i[0]-i[1])**2 for i in zip(v1, v2))


def sortVerticesByConnexions(verts_in, edges_in):
    vertsOut = []
    edgesOut = []
    index = []
    edgeLegth = len(edges_in)
    edgesIndex = [j for j in range(edgeLegth)]
    edges0 = [j[0] for j in edges_in]
    edges1 = [j[1] for j in edges_in]
    edIndex = 0
    orSide = 0
    edgesCopy = [edges0,edges1, edgesIndex]

    for co in edgesCopy:
        co.pop(0)

    for j in range(edgeLegth):
        e = edges_in[edIndex]
        ed = []
        if orSide == 1:
            e = [e[1], e[0]]

        for side in e:
            if verts_in[side] in vertsOut:
                ed.append(vertsOut.index(verts_in[side]))
            else:
                vertsOut.append(verts_in[side])
                ed.append(vertsOut.index(verts_in[side]))
                index.append(side)

        edgesOut.append(ed)

        edIndexOld = edIndex
        vIndex = e[1]
        if vIndex in edgesCopy[0]:
            k = edgesCopy[0].index(vIndex)
            edIndex = edgesCopy[2][k]
            orSide = 0
            
            for co in edgesCopy:
                co.pop(k) 

        elif vIndex in edgesCopy[1]:
            k = edgesCopy[1].index(vIndex)
            edIndex = edgesCopy[2][k]
            orSide = 1
            for co in edgesCopy:
                co.pop(k) 
        
        if edIndex == edIndexOld and len(edgesCopy[0]) > 0:
            edIndex = edgesCopy[2][0]
            orSide = 0
            for co in edgesCopy:
                co.pop(0) 

    # add unconnected vertices
    if len(vertsOut) != len(verts_in):
        for verts, i in zip(verts_in, range(len(verts_in))):
            if verts not in vertsOut:
                vertsOut.append(verts)
                index.append(i)

    return vertsOut, edgesOut, index
    
# function taken from poligons_to_edges.py
def pols_edges(obj, unique_edges=False):
    out = []
    for faces in obj:
        out_edges = []
        seen = set()
        for face in faces:
            for edge in zip(face, list(face[1:]) + list([face[0]])):
                if unique_edges and tuple(sorted(edge)) in seen:
                    continue
                if unique_edges:
                    seen.add(tuple(sorted(edge)))
                out_edges.append(edge)
        out.append(out_edges)
    return out


class SvVertSortNode(bpy.types.Node, SverchCustomTreeNode):
    '''Vector sort'''
    bl_idname = 'SvVertSortNode'
    bl_label = 'Vector Sort'
    bl_icon = 'SORTSIZE'

    def mode_change(self, context):
        if self.mode == 'XYZ':
            while len(self.inputs) > 2:
                self.inputs.remove(self.inputs[-1])
        if self.mode == 'DIST':
            while len(self.inputs) > 2:
                self.inputs.remove(self.inputs[-1])
            self.inputs.new('VerticesSocket', 'Base Point', 'Base Point')

        if self.mode == 'AXIS':
            while len(self.inputs) > 2:
                self.inputs.remove(self.inputs[-1])
            self.inputs.new('MatrixSocket', 'Mat')
            
        if self.mode == 'CONNEX':
            while len(self.inputs) > 2:
                self.inputs.remove(self.inputs[-1])
                        
        if self.mode == 'USER':
            while len(self.inputs) > 2:
                self.inputs.remove(self.inputs[-1])
            self.inputs.new('StringsSocket', 'Index Data', 'Index Data')

        updateNode(self, [])

    modes = [("XYZ",    "XYZ", "X Y Z Sort",    1),
             ("DIST",   "Dist", "Distance",     2),
             ("AXIS",   "Axis", "Axial sort",   3),
             ("CONNEX", "Connect", "Sort by connections",   4),
             ("USER",   "User", "User defined", 10)]

    mode = EnumProperty(default='XYZ', items=modes,
                        name='Mode', description='Sort Mode',
                        update=mode_change)

    def draw_buttons(self, context, layout):
        layout.prop(self, "mode", expand=False)
        if self.mode == "XYZ":
            pass

    def sv_init(self, context):
        self.inputs.new('VerticesSocket', 'Vertices', 'Vertices')
        self.inputs.new('StringsSocket', 'PolyEdge', 'PolyEdge')

        self.outputs.new('VerticesSocket', 'Vertices')
        self.outputs.new('StringsSocket', 'PolyEdge')
        self.outputs.new('StringsSocket', 'Item order')

    def process(self):
        verts = self.inputs['Vertices'].sv_get()

        if self.inputs['PolyEdge'].is_linked:
            poly_edge = self.inputs['PolyEdge'].sv_get()
            polyIn = True
        else:
            polyIn = False
            poly_edge = repeat_last([[]])

        verts_out = []
        poly_edge_out = []
        item_order = []

        polyOutput = polyIn and self.outputs['PolyEdge'].is_linked
        orderOutput = self.outputs['Item order'].is_linked
        vertOutput = self.outputs['Vertices'].is_linked

        if not any((vertOutput, orderOutput, polyOutput)):
            return

        if self.mode == 'XYZ':
            # should be user settable
            op_order = [(0, False),
                        (1, False),
                        (2, False)]

            for v, p in zip(verts, poly_edge):
                s_v = ((e[0], e[1], e[2], i) for i, e in enumerate(v))

                for item_index, rev in op_order:
                    s_v = sorted(s_v, key=itemgetter(item_index), reverse=rev)

                verts_out.append([v[:3] for v in s_v])

                if polyOutput:
                    v_index = {item[-1]: j for j, item in enumerate(s_v)}
                    poly_edge_out.append([list(map(lambda n:v_index[n], pe)) for pe in p])
                if orderOutput:
                    item_order.append([i[-1] for i in s_v])

        if self.mode == 'DIST':
            if self.inputs['Base Point'].is_linked:
                base_points = self.inputs['Base Point'].sv_get()
                bp_iter = repeat_last(base_points[0])
            else:
                bp = [(0, 0, 0)]
                bp_iter = repeat_last(bp)

            for v, p, v_base in zip(verts, poly_edge, bp_iter):
                s_v = sorted(((v_c, i) for i, v_c in enumerate(v)), key=lambda v: distK(v[0], v_base))
                verts_out.append([vert[0] for vert in s_v])

                if polyOutput:
                    v_index = {item[-1]: j for j, item in enumerate(s_v)}
                    poly_edge_out.append([list(map(lambda n:v_index[n], pe)) for pe in p])
                if orderOutput:
                    item_order.append([i[-1] for i in s_v])

        if self.mode == 'AXIS':
            if self.inputs['Mat'].is_linked:
                mat = Matrix_generate(self.inputs['Mat'].sv_get())
            else:
                mat = [Matrix. Identity(4)]
            mat_iter = repeat_last(mat)

            def f(axis, q):
                if axis.dot(q.axis) > 0:
                    return q.angle
                else:
                    return -q.angle

            for v, p, m in zip(Vector_generate(verts), poly_edge, mat_iter):
                axis = m * Vector((0, 0, 1))
                axis_norm = m * Vector((1, 0, 0))
                base_point = m * Vector((0, 0, 0))
                intersect_d = [intersect_point_line(v_c, base_point, axis) for v_c in v]
                rotate_d = [f(axis, (axis_norm + v_l[0]).rotation_difference(v_c)) for v_c, v_l in zip(v, intersect_d)]
                s_v = ((data[0][1], data[1], i) for i, data in enumerate(zip(intersect_d, rotate_d)))
                s_v = sorted(s_v, key=itemgetter(0, 1))

                verts_out.append([v[i[-1]].to_tuple() for i in s_v])

                if polyOutput:
                    v_index = {item[-1]: j for j, item in enumerate(s_v)}
                    poly_edge_out.append([list(map(lambda n:v_index[n], pe)) for pe in p])
                if orderOutput:
                    item_order.append([i[-1] for i in s_v])

        if self.mode == 'USER':
            if self.inputs['Index Data'].is_linked:
                index = self.inputs['Index Data'].sv_get()
            else:
                return

            for v, p, i in zip(verts, poly_edge, index):

                s_v = sorted([(data[0], data[1], i) for i, data in enumerate(zip(i, v))], key=itemgetter(0))

                verts_out.append([obj[1] for obj in s_v])

                if polyOutput:
                    v_index = {item[-1]: j for j, item in enumerate(s_v)}
                    poly_edge_out.append([[v_index[k] for k in pe] for pe in p])
                if orderOutput:
                    item_order.append([i[-1] for i in s_v])
 
        if self.mode == 'CONNEX':
            if self.inputs['PolyEdge'].is_linked:
                edges = self.inputs['PolyEdge'].sv_get()
                for v, p in zip(verts, edges):
                    pols = []
                    if len(p[0])>2:
                        pols = [p[:]]
                        p = pols_edges([p], True)[0]

                    vN, pN, iN = sortVerticesByConnexions(v, p)
                    if len(pols) > 0:
                        newPols = []
                        for pol in pols[0]:
                            newPol = []
                            for i in pol:
                                newPol.append(iN.index(i))
                            newPols.append(newPol)
                        pN = [newPols]
                        
                    verts_out.append(vN)
                    poly_edge_out.append(pN)
                    item_order.append(iN)
                        
                    
                
        if vertOutput:
            self.outputs['Vertices'].sv_set(verts_out)
        if polyOutput:
            self.outputs['PolyEdge'].sv_set(poly_edge_out)
        if orderOutput:
            self.outputs['Item order'].sv_set(item_order)


def register():
    bpy.utils.register_class(SvVertSortNode)


def unregister():
    bpy.utils.unregister_class(SvVertSortNode)
