import mathutils
from mathutils import Vector, kdtree


def sv_main(v=[[]], mdist=0.4):

    in_sockets = [
        ['v', 'verts', v],
        ['s', 'mdist', mdist]
    ]

    e = []

    if v and v[0]:

        v = v[0]
        # make kd tree
        # documentation mathutils.kdtree.html
        size = len(v)
        kd = kdtree.KDTree(size)

        for i, vtx in enumerate(v):
            kd.insert(Vector(vtx), i)
        kd.balance()

        # makes edges
        for i, vtx in enumerate(v):
            num_edges = 0
            for (co, index, dist) in kd.find_range(vtx, mdist):
                if i == index or (num_edges > 2):
                    continue
                e.append([i, index])
                num_edges += 1

    # out boilerplate
    out_sockets = [
        ['s', 'Edges', [e]]
    ]

    return in_sockets, out_sockets
