"""
in verts_in v
in edges_in s
in matrices_in m
out combo_len s
"""

import math
from mathutils import Matrix
from sverchok.utils.sv_bmesh_utils import bmesh_from_pydata


matrices_linked = self.inputs['matrices_in'].is_linked
verts_linked = self.inputs['verts_in'].is_linked
edges_linked = self.inputs['edges_in'].is_linked

def get_scale_factor(idx):
    if matrices_linked:
        good_index = min(len(matrices_in), idx)
        matrix = matrices_in[good_index]
        Matrix(matrix).to_scale().length / 1.7320508
        return scale
    return 1.0


if verts_linked and edges_linked:

    for idx, (verts, edges) in enumerate(zip(verts_in, edges_in)):

        bm = bmesh_from_pydata(verts, edges, [])
        calced_len = 0.0
        for e in bm.edges:
            calced_len += e.calc_length()

        scale = get_scale_factor(idx)
        calced_len *= scale
        
        combo_len.append(calced_len)

elif verts_linked:
    for idx, verts in enumerate(verts_in):
        calced_len = 0.0
        for v_idx in range(len(verts)-1):
            vert_a, vert_b = verts[v_idx], verts[v_idx+1]
            v = vert_a[0]-vert_b[0], vert_a[1]-vert_b[1], vert_a[2]-vert_b[2]
            distance = math.sqrt((v[0] * v[0]) + (v[1] * v[1]) + (v[2] * v[2]))
            calced_len += distance

        scale = get_scale_factor(idx)
        calced_len *= scale
        
        combo_len.append(calced_len)

combo_len = [combo_len]
