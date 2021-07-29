def sv_main(num_poly=20, skip=5, num_verts=10, num_rings=15):

    in_sockets = [
        ['s', 'num_poly', num_poly],
        ['s', 'skip', skip],
        ['s', 'num_verts', num_verts],
        ['s', 'num_rings', num_rings]]


    # arbitrary user code
    p = [(i, i+1, i+num_verts, i+num_verts-1) for i in range(num_poly) if (i-skip) % (skip+1)]
    p += [((i*(num_verts-1)),((i+1)*(num_verts-1)), ((i+2)*(num_verts-1)-1),((i+1)*(num_verts-1))-1) for i in range(num_rings)]

    # out boilerplate
    out_sockets = [
        ['s', 'Polygons', p]
    ]

    return in_sockets, out_sockets
