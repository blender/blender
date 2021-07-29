def sv_main(verts=[[]], edges=[[]]):
    verts_out = []
    polys_out = []
    masked_items_out = []

    in_sockets = [
        ['v', 'verts', verts],
        ['s', 'edges', edges]
    ]
    lenob = len(verts)
    if verts[0] and edges[0]:
        i = 0
        for ve, ed in zip(verts,edges):
            verts_out.extend(ve)
            if i == lenob-1: continue
            lenve = len(ve)
            for e in ed:
                polys_out.append([e[0]+lenve*(i+1),e[1]+lenve*(i+1),e[1]+lenve*i,e[0]+lenve*i])
            i+=1
    out_sockets = [
        ['v', 'verts', [verts_out]],
        ['s', 'polys', [polys_out]],
    ]

    return in_sockets, out_sockets
