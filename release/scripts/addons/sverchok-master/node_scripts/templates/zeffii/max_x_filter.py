def sv_main(verts=[], edges=[], max_x=0.2):
    # https://github.com/zeffii/bpy_script/blob/master/geom_daily_recodes/491_orbits_001.blend

    in_sockets = [
        ['v', 'verts', verts],
        ['s', 'edges', edges],
        ['s', 'max_x', max_x]]

    out_sockets = [ ['v', 'Vecs', []], ['s', 'Edges', []] ]

    if not verts and edges:
        return in_sockets, out_sockets
    
    # - first find all vertices with x component larger than max_x
    mask_indices = []
    mask_index = mask_indices.append
    
    if verts and verts[0]:
        for idx, v in enumerate(verts[0]):
            if v[0] > max_x:
                mask_index(idx)
        
        out_sockets[0][2] = [verts]
        
        out_edges = []
        for edge in edges[0]:
            a, b = edge
            if (a or b) in mask_indices:
                continue
            out_edges.append(edge)
            
        out_sockets[1][2] = [out_edges]


    return in_sockets, out_sockets
