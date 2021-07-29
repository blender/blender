def sv_main(verts=[], dist=1.1, strength=1.0):

    # combine this script with petal_sin.py

    in_sockets = [
       ['v', 'Verts', verts],
       ['s', 'Dist', dist],
       ['s', 'Strength', strength]
    ]

    from math import sin, cos, radians, pi, sqrt
    from mathutils import Vector, Euler

    out_sockets = [
        ['v', 'Verts', []]
    ]
    
    if not verts:
        return in_sockets, out_sockets
    
    verts_out = []
    vert_add = verts_out.append
    for idx, v in enumerate(verts):
        vc = Vector(v)
        fdist = vc.length
        new_z = ((2/(fdist+dist))*-1)*strength
        nvc = vc + Vector((0,0,new_z))
        vert_add(nvc.to_tuple())
        
    out_sockets[0][2] = [verts_out]
    return in_sockets, out_sockets
