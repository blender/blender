"""
in verts v .=[[]] n=1
in radius s .=0.3 n=2
out edges s
"""

import mathutils

# just use first list?
size = len(verts)
kd = mathutils.kdtree.KDTree(size)
for i, xyz in enumerate(verts):
    kd.insert(xyz, i)
kd.balance()

edges = [[]]
edge_set = set()
r = radius
for idx, vtx in enumerate(verts):
    n_list = kd.find_range(vtx, r)
    for co, index, dist in n_list:
        if index == idx:
            continue
        edge_set.add(tuple(sorted([idx, index])))

edges[0] = list(edge_set)
