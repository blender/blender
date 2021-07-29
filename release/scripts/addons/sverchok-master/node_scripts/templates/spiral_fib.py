def sv_main(n=1000, c=0.1):

    in_sockets = [
        ['s', 'Number of points',  n],
        ['s', 'Scale Factor',  c]
    ]

    from math import sin, cos, radians, pi, sqrt
    from mathutils import Vector, Euler

    Verts = []
    verts_new = Verts.append

    for i in range(0, n):
        theta = i * radians(137.5)
        r = c * sqrt(i)
        verts_new((cos(theta) * r, sin(theta) * r, 0.0))

    out_sockets = [
        ['v', 'Verts', [Verts]]
    ]

    return in_sockets, out_sockets
