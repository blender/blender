from mathutils import Vector
from collections import defaultdict


def sv_main(verts=[[]], nx=20, ny=20, nz=20):

    in_sockets = [
        ['v', 'verts', verts],
        ['s', 'nx', nx],
        ['s', 'ny', ny],
        ['s', 'nz', nz]]

    # enforce min division of 1
    nx, ny, nz = [max(i, 1) for i in [nx, ny, nz]]

    # this is a sparse 3d grid
    grid_3d = defaultdict(list)

    def downsample(verts, n=(20, 20, 20)):

        def getBBox(verts):
            rotated = list(zip(*verts[::-1]))
            return [[min(dim), max(dim)] for dim in rotated]

        def get_divs(bbox):
            divs = []
            for i in range(3):
                dim_seg = (bbox[i][1] - bbox[i][0]) / n[i]
                divs.append([bbox[i][0] + (dim_seg * k) for k in range(n[i])])
            return divs

        def sum_verts(verts):
            s = verts[0]
            for v in verts[1:]:
                s += v
            return s

        def avg_vert(verts):
            # incoming is verts per bucket
            nv = len(verts)
            return verts[0] if nv == 1 else sum_verts(verts) * (1 / nv)

        def find_slot(dim, dimdivs):
            for idx, div in reversed(list(enumerate(dimdivs))):
                if not dim >= div:
                    continue
                else:
                    return idx
            return 0

        def get_bucket_tuple(vec):
            x_idx = find_slot(vec.x, xdiv)
            y_idx = find_slot(vec.y, ydiv)
            z_idx = find_slot(vec.z, zdiv)
            return x_idx, y_idx, z_idx

        bbox = getBBox(verts)
        xdiv, ydiv, zdiv = get_divs(bbox)

        for v in verts:
            vec = Vector(v)
            bucket = get_bucket_tuple(vec)
            grid_3d[bucket].append(vec)

        return [avg_vert(v)[:] for v in grid_3d.values()]

    # out boilerplate - set your own sockets packet
    # the same principle as in in_sockets
    verts_out = []
    out_sockets = [['v', 'Downsampled', verts_out]]

    if verts and verts[0]:
        good_verts = verts[0]
        out_sockets[0][2] = [downsample(good_verts, n=(nx, ny, nz))]

    return in_sockets, out_sockets
