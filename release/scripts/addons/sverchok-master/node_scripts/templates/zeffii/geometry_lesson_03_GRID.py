def make_geometry(ny, nx, verts, faces):
    
    # ------------
    j = ny * nx
    verts.extend([(i%nx, i//nx, 0) for i in range(j)])
    
    # ------------
    add_face = faces.extend

    total_range = ((ny-1) * (nx))
    for i in range(total_range):
        if not ((i+1) % nx == 0):  # +1 is the shift
            add_face([[i, i+nx, i+nx+1, i+1]])  # clockwise
    
    print(verts, faces)

def sv_main(ny=3, nx=4):
    verts = []
    faces = []

    in_sockets = [
        ['s', 'ny', ny],
        ['s', 'nx', nx]
    ]

    out_sockets = [
        ['v', 'verts', [verts]],
        ['s', 'faces', [faces]]
    ]

    make_geometry(ny, nx, verts, faces)

    return in_sockets, out_sockets
