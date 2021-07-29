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

# <pep8 compliant>


import bpy
import bmesh
import sys

from . import cad_module as cm

messages = {
    'SHARED_VERTEX': 'Shared Vertex, no intersection possible',
    'PARALLEL_EDGES': 'Edges Parallel, no intersection possible',
    'NON_PLANAR_EDGES': 'Non Planar Edges, no clean intersection point'
}


def add_edges(bm, pt, idxs, fdp):
    '''
    this function is a disaster --
    index updates and ensure_lookup_table() are called before this function
    and after, and i've tried doing this less verbose but results tend to be
    less predictable. I'm obviously a terrible coder, but can only spend so
    much time figuring out this stuff.
    '''

    v1 = bm.verts.new(pt)

    bm.verts.ensure_lookup_table()
    bm.edges.ensure_lookup_table()
    bm.verts.index_update()

    try:
        for e in idxs:
            bm.edges.index_update()
            v2 = bm.verts[e]
            bm.edges.new((v1, v2))

        bm.edges.index_update()
        bm.verts.ensure_lookup_table()
        bm.edges.ensure_lookup_table()

    except Exception as err:
        print('some failure: details')
        for l in fdp:
            print(l)

        sys.stderr.write('ERROR: %s\n' % str(err))
        print(sys.exc_info()[-1].tb_frame.f_code)
        print('Error on line {}'.format(sys.exc_info()[-1].tb_lineno))


def remove_earmarked_edges(bm, earmarked):
    edges_select = [e for e in bm.edges if e.index in earmarked]
    bmesh.ops.delete(bm, geom=edges_select, context=2)


def perform_vtx(bm, pt, edges, pts, vertex_indices):
    idx1, idx2 = edges[0].index, edges[1].index
    fdp = pt, edges, pts, vertex_indices

    # this list will hold those edges that pt lies on
    edges_indices = cm.find_intersecting_edges(bm, pt, idx1, idx2)
    mode = 'VTX'[len(edges_indices)]

    if mode == 'V':
        cl_vert1 = cm.closest_idx(pt, edges[0])
        cl_vert2 = cm.closest_idx(pt, edges[1])
        add_edges(bm, pt, [cl_vert1, cl_vert2], fdp)

    elif mode == 'T':
        to_edge_idx = edges_indices[0]
        from_edge_idx = idx1 if to_edge_idx == idx2 else idx2

        cl_vert = cm.closest_idx(pt, bm.edges[from_edge_idx])
        to_vert1, to_vert2 = cm.vert_idxs_from_edge_idx(bm, to_edge_idx)
        add_edges(bm, pt, [cl_vert, to_vert1, to_vert2], fdp)

    elif mode == 'X':
        add_edges(bm, pt, vertex_indices, fdp)

    # final refresh before returning to user.
    if edges_indices:
        remove_earmarked_edges(bm, edges_indices)

    bm.edges.index_update()
    return bm


def do_vtx_if_appropriate(bm, edges):
    vertex_indices = cm.get_vert_indices_from_bmedges(edges)

    # test 1, are there shared vers? if so return non-viable
    if not len(set(vertex_indices)) == 4:
        return {'SHARED_VERTEX'}

    # test 2, is parallel?
    p1, p2, p3, p4 = [bm.verts[i].co for i in vertex_indices]
    point = cm.get_intersection([p1, p2], [p3, p4])
    if not point:
        return {'PARALLEL_EDGES'}

    # test 3, coplanar edges?
    coplanar = cm.test_coplanar([p1, p2], [p3, p4])
    if not coplanar:
        return {'NON_PLANAR_EDGES'}

    # point must lie on an edge or the virtual extention of an edge
    bm = perform_vtx(bm, point, edges, (p1, p2, p3, p4), vertex_indices)
    return bm


class TCAutoVTX(bpy.types.Operator):
    '''Weld intersecting edges, project converging edges towards their intersection'''
    bl_idname = 'tinycad.autovtx'
    bl_label = 'VTX autoVTX'

    @classmethod
    def poll(cls, context):
        obj = context.active_object
        return bool(obj) and obj.type == 'MESH'

    def cancel_message(self, msg):
        print(msg)
        self.report({"WARNING"}, msg)
        return {'CANCELLED'}

    def execute(self, context):

        # final attempt to enter unfragmented bm/mesh
        # ghastly, but what can I do? it works with these
        # fails without.
        bpy.ops.object.mode_set(mode='OBJECT')
        bpy.ops.object.mode_set(mode='EDIT')

        obj = context.active_object
        me = obj.data

        bm = bmesh.from_edit_mesh(me)
        bm.verts.ensure_lookup_table()
        bm.edges.ensure_lookup_table()

        edges = [e for e in bm.edges if e.select and not e.hide]

        if len(edges) == 2:
            message = do_vtx_if_appropriate(bm, edges)
            if isinstance(message, set):
                msg = messages.get(message.pop())
                return self.cancel_message(msg)
            bm = message
        else:
            return self.cancel_message('select two edges!')

        bm.verts.index_update()
        bm.edges.index_update()
        bmesh.update_edit_mesh(me, True)

        return {'FINISHED'}


def register():
    bpy.utils.register_module(__name__)


def unregister():
    bpy.utils.unregister_module(__name__)
