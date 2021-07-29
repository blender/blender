"""
in in_verts v
in in_edges s
out vout v
"""

#### This expects an input mesh that represents an edgenet without junctions and unlooped.

from sverchok.utils.sv_bmesh_utils import bmesh_from_pydata

for verts, edges in zip(in_verts, in_edges):
    bm = bmesh_from_pydata(verts, edges, [])
    
    idx_start = -1
    
    # find the vertex that only lies on one edge
    for v in bm.verts:
        if len(v.link_edges) == 1:
            idx_start = v.index
            break
    
    # seems this set of verts / edges is not usable
    if idx_start == -1:
        continue
    
    bm.verts.ensure_lookup_table()

    final = 20
    v1 = bm.verts[idx_start]
    e1 = v1.link_edges[0]
    v1.select = True
    e1.select = True
    
    get_verts = []
    get_verts.append(v1.co[:])
    while True:
        
        v1 = e1.other_vert(v1)
        pot_edges = v1.link_edges[:]
        get_verts.append(v1.co[:])
        
        if len(pot_edges) == 1:
            # at the end!
            break
        elif len(pot_edges) > 2:
            break
        elif len(pot_edges) == 2:
            selects = [e.select for e in pot_edges]
            if selects == [0, 1]:
                e1 = pot_edges[0]
            elif selects == [1, 0]:
                e1 = pot_edges[1]
            else:
                break
            e1.select = True
        
        v1.select = True
    vout.append(get_verts)
