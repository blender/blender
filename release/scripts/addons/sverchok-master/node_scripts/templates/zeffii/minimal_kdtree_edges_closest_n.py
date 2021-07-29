from math import sin, cos, pi
import mathutils
from mathutils import Vector

def sv_main(verts=[[]]):

    in_sockets = [
        ['v', 'verts', verts]]

    verts = verts[0]
    size = len(verts)
    kd = mathutils.kdtree.KDTree(size)

    for i, vtx in enumerate(verts):
        kd.insert(Vector(vtx), i)
    kd.balance()

    e = []
    e_set = set()
    for i, vtx in enumerate(verts):
        
        # 3, because the first closest vertex to each
        # vertex is that vertex. and the 2nd closest might
        # be picked already by a different edge
        print('i:', i)
        for (co, index, dist) in kd.find_n(vtx, 3):
            print(co, index, dist)
            if i==index:
                print('found self!')
                continue
            edge = tuple(sorted([i, index]))
            if not edge in e_set:
                e_set.add(edge)
                e.append(edge)

    # out boilerplate
    out_sockets = [['s', 'Edges', [e]]]

    return in_sockets, out_sockets
