import mathutils
from mathutils import Vector, kdtree


def sv_main(v=[[]], point=[], mdist=0.4):

    in_sockets = [
        ['v', 'verts', v],
        ['v', 'point', point],
        ['s', 'mdist', mdist]
    ]

    affected_verts = {}
    return_verts = []

    if (v and v[0]) and point:

        v = v[0]
        size = len(v)
        kd = kdtree.KDTree(size)

        for i, vtx in enumerate(v):
            kd.insert(Vector(vtx), i)
        kd.balance()

        # makes edges
        vtx = point[0][0]
        for (co, index, dist) in kd.find_range(vtx, mdist):
            affected_verts[index] = dist

        v1 = Vector(vtx)
        add_vert = return_verts.append
        for idx, vert in enumerate(v):
            rv = affected_verts.get(idx, 0)
            if rv > 0:
                amp = mdist / rv
                v2 = Vector(v[idx])
                v3 = v1.lerp(v2, amp)
                # make new vector
                rv = v3[:]
            else:
                rv = vert
            add_vert(rv)

    # out boilerplate
    out_sockets = [
        ['v', 'Modified', [return_verts]]
    ]

    return in_sockets, out_sockets
