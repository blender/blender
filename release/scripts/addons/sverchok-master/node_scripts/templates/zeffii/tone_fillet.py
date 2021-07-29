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

sv_bl_info = {
    'authors': '\
    original authored by zmj100,\
    later modified by Gert De Roost,\
    latest incarnation as SN by zeffii'
}

import bpy
import bmesh
from mathutils import Matrix
from math import cos, pi, degrees, sin, tan
from sverchok.utils.sv_bmesh_utils import bmesh_from_pydata


class f_buf(object):
    an = 0


class fillet_op0(object):

    adj = 0.1  # min = 0.00001, max = 100.0, step = 1, precision = 3
    n = 3  # name = '', default = 3, min = 1, max = 100, step = 1
    out = False
    flip = False
    radius = False

    def __init__(self, bm, adj, num_seg):
        self.bm = bm
        self.adj = adj
        self.n = num_seg
        self.execute()

    def execute(self):
        self.settings = self.adj, self.n, self.out, self.flip, self.radius
        bm = self.bm

        for v in bm.verts:
            v.select = True
        for e in bm.edges:
            e.select = True

        done = 1
        while done:
            tempset = set([])
            for v in bm.verts:
                if v.select:
                    tempset.add(v)
            done = 0
            for v in tempset:
                cnt = 0
                edgeset = set([])
                for e in v.link_edges:
                    if e.select:
                        edgeset.add(e)
                        cnt += 1
                if cnt == 2:
                    self.do_filletplus(edgeset)
                    done = 1
                    break

                if done:
                    break

    # def list_clear_(self, l):
    #     del l[:]
    #     return l
    def get_adj_v_(self, list_):
        tmp = {}
        for i in list_:
            try:
                tmp[i[0]].append(i[1])
            except KeyError:
                tmp[i[0]] = [i[1]]

            try:
                tmp[i[1]].append(i[0])
            except KeyError:
                tmp[i[1]] = [i[0]]
        return tmp

    def do_filletplus(self, pair):
        adj, n, out, flip, radius = self.settings
        bm = self.bm

        list_0 = [list([e.verts[0].index, e.verts[1].index]) for e in pair]

        vertset = set([])
        vertset.add(bm.verts[list_0[0][0]])
        vertset.add(bm.verts[list_0[0][1]])
        vertset.add(bm.verts[list_0[1][0]])
        vertset.add(bm.verts[list_0[1][1]])

        v1, v2, v3 = vertset

        if len(list_0) != 2:
            # self.report({'INFO'}, 'Two adjacent edges must be selected.')
            return
        else:
            vertlist = []
            found = 0
            for f in v1.link_faces:
                if v2 in f.verts and v3 in f.verts:
                    found = 1

            if not found:
                for v in [v1, v2, v3]:
                    if v.index in list_0[0] and v.index in list_0[1]:
                        startv = v
                face = None

            else:
                for f in v1.link_faces:
                    if not (v2 in f.verts and v3 in f.verts):
                        continue

                    for v in f.verts:
                        if not(v in vertset):
                            vertlist.append(v)
                        if (v in vertset and v.link_loops[0].link_loop_prev.vert in vertset
                                and v.link_loops[0].link_loop_next.vert in vertset):
                            startv = v
                    face = f

            if out:
                flip = False

            self.f_(list_0, startv, vertlist, face)

    def f_(self, list_0, startv, vertlist, face):

        adj, n, out, flip, radius = self.settings
        bm = self.bm

        dict_0 = self.get_adj_v_(list_0)
        list_1 = [[dict_0[i][0], i, dict_0[i][1]] for i in dict_0 if (len(dict_0[i]) == 2)][0]
        list_3 = []
        for elem in list_1:
            list_3.append(bm.verts[elem])
        list_2 = []

        p_ = list_3[1]
        p = (list_3[1].co).copy()
        p1 = (list_3[0].co).copy()
        p2 = (list_3[2].co).copy()
        vec1 = p - p1
        vec2 = p - p2

        ang = vec1.angle(vec2, any)
        f_buf.an = round(degrees(ang))
        if f_buf.an in {180, 0.0}:
            return

        opp = adj
        if not radius:
            h = adj * (1 / cos(ang * 0.5))
            adj_ = adj
        else:
            h = opp / sin(ang * 0.5)
            adj_ = opp / tan(ang * 0.5)

        p3 = p - (vec1.normalized() * adj_)
        p4 = p - (vec2.normalized() * adj_)
        rp = p - ((p - ((p3 + p4) * 0.5)).normalized() * h)
        vec3 = rp - p3
        vec4 = rp - p4
        axis = vec1.cross(vec2)

        if out:
            rot_ang = (2 * pi) - vec1.angle(vec2)
        else:
            rot_ang = vec1.angle(vec2) if flip else vec3.angle(vec4)

        for j in range(n + 1):
            new_angle = rot_ang * j / n
            mtrx = Matrix.Rotation(new_angle, 3, axis)
            if not out:
                if not flip:
                    tmp = p4 - rp
                    tmp1 = mtrx * tmp
                    tmp2 = tmp1 + rp
                else:
                    p3 = p - (vec1.normalized() * opp)
                    tmp = p3 - p
                    tmp1 = mtrx * tmp
                    tmp2 = tmp1 + p

            else:
                p4 = p - (vec2.normalized() * opp)
                tmp = p4 - p
                tmp1 = mtrx * tmp
                tmp2 = tmp1 + p

            v = bm.verts.new(tmp2)
            list_2.append(v)

        if not flip:
            list_2.reverse()
        list_3[1:2] = list_2

        n1 = len(list_3)
        for t in range(n1 - 1):
            bm.edges.new([list_3[t], list_3[(t + 1) % n1]])
            v = bm.verts.new(p)
            bm.edges.new([v, p_])

        if face is not None:
            self.do_face_manip(face, list_3, startv, vertlist)

        bm.verts.remove(startv)
        list_3[1].select = 1
        list_3[-2].select = 1
        bm.edges.get([list_3[0], list_3[1]]).select = 1
        bm.edges.get([list_3[-1], list_3[-2]]).select = 1
        bm.verts.index_update()
        bm.edges.index_update()
        bm.faces.index_update()

    def do_face_manip(self, face, list_3, startv, vertlist):
        for l in face.loops:
            if l.vert == list_3[0]:
                startl = l
                break

        vertlist2 = []
        if startl.link_loop_next.vert == startv:
            l = startl.link_loop_prev
            while len(vertlist) > 0:
                vertlist2.insert(0, l.vert)
                vertlist.pop(vertlist.index(l.vert))
                l = l.link_loop_prev
        else:
            l = startl.link_loop_next
            while len(vertlist) > 0:
                vertlist2.insert(0, l.vert)
                vertlist.pop(vertlist.index(l.vert))
                l = l.link_loop_next

        for v in list_3:
            vertlist2.append(v)
        self.bm.faces.new(vertlist2)


def fillet(verts, edges, adj, num_seg):
    bm = bmesh_from_pydata(verts[0], edges[0], [])
    this_fillet = fillet_op0(bm, adj, num_seg)

    verts_out = [v.co[:] for v in bm.verts]
    edges_out = [[i.index for i in p.verts] for p in bm.edges[:]]
    return verts_out, edges_out


def sv_main(verts=[[]], edges=[[]], adj=0.3, num_seg=3):

    verts_out = []
    edges_out = []
    in_sockets = [
        ['v', 'verts', verts],
        ['s', 'edges', edges],
        ['s', 'radii', adj],
        ['s', 'segments', num_seg]
    ]

    out_sockets = [
        ['v', 'verts', [verts_out]],
        ['s', 'edges', [edges_out]]
    ]

    if (verts and verts[0]) and (edges and edges[0]):
        v_out, e_out = fillet(verts, edges, adj, num_seg)
        verts_out.extend(v_out)
        edges_out.extend(e_out)

    return in_sockets, out_sockets
