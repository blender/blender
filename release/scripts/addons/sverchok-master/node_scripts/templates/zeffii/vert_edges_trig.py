def sv_main(num_verts=20, radius=5, num_rings=3, rotation=0.3):

    # in boilerplate, could be less verbose
    in_sockets = [
        ['s', 'num_verts',  num_verts],
        ['s', 'radius',  radius],
        ['s', 'num_rings', num_rings],
        ['s', 'rotation', rotation]
    ]

    # imports and aliases
    from math import sin, cos, pi

    TWO_PI = 2 * pi
    r = radius

    v = []
    e = []
    angle = TWO_PI / num_verts

    for j in range(num_rings):
        radial_offset = rotation * j
        for i in range(num_verts):
            theta = (angle * i) + radial_offset
            tr = r + (0.5 * j)
            v.append([cos(theta) * tr, sin(theta) * tr, 0])

    if num_rings > 1:
        for i in range(num_verts):
            for j in range(num_rings - 1):
                offset = j * num_verts
                e.append([i + offset, i + num_verts + offset])

    # out boilerplate
    out_sockets = [
        ['v', 'Vecs', [v]],
        ['s', 'Edges', e]
    ]

    return in_sockets, out_sockets
