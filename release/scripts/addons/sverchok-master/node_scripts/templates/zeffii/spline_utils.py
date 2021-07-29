import mathutils
from mathutils import Vector
import math

# this file would be placed directly into the sverchokmaster directory when used.

def get_length(verts):
    summed = 0
    lengths = []
    lengths_add = lengths.append
    for idx in range(len(verts)-1):
        segment = (verts[idx]-verts[idx+1]).length
        summed += segment
        lengths_add(segment)
    return summed, lengths


def get_verts_n_edges(verts, lengths, seg_width):

    K = seg_width
    eps = 0.00001
    new_points = []
    add_point = new_points.append

    def consume(K, A, idx, v1):

        if idx > len(lengths)-2:
            return

        R = K - A

        # close enough to start fresh segment
        if (-eps <= R <= eps):
            K = seg_width
            idx += 1
            add_point(verts[idx])
            A = lengths[idx]
            consume(K, A, idx, None)

        # must divide segment, same idx
        elif (R < -eps):
            # rate = R / A
            rate = K / A
            if not v1:
                v1 = verts[idx]
            v2 = verts[idx+1]
            vmid = v1.lerp(v2, rate)
            add_point(vmid)
            A = (vmid-v2).length
            consume(seg_width, A, idx, v1)

        # consume segment, update k, update idx
        elif (R > eps):
            A = lengths[idx+1]
            consume(R, A, idx+1, None)

    add_point(verts[0])
    consume(K, lengths[0], 0, None)
    add_point(verts[-1])
    return new_points
