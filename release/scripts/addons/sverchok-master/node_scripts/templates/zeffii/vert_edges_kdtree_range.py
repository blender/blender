def sv_main(num_verts=20, radius=5, num_rings=3, rotation=0.3, mdist=0.3):

    # in boilerplate, could be less verbose
    in_sockets = [
        ['s', 'num_verts',  num_verts],
        ['s', 'radius',  radius],
        ['s', 'num_rings', num_rings],
        ['s', 'rotation', rotation],
        ['s', 'distance', mdist]
    ]

    from math import sin, cos, pi
    import mathutils
    from mathutils import Vector

    TWO_PI = 2 * pi
    r = radius
    angle = TWO_PI / num_verts
    v = []
    e = []

    # create vertices
    for j in range(num_rings):
        radial_offset = rotation * j
        for i in range(num_verts):
            theta = (angle * i) + radial_offset
            tr = r + (0.5 * j)
            v.append([cos(theta) * tr, sin(theta) * tr, 0])

    # make kd tree
    # documentation/blender_python_api_2_70_release/mathutils.kdtree.html
    size = len(v)
    kd = mathutils.kdtree.KDTree(size)

    for i, vtx in enumerate(v):
        kd.insert(Vector(vtx), i)
    kd.balance()

    # makes edges
    for i, vtx in enumerate(v):
        num_edges = 0
        for (co, index, dist) in kd.find_range(vtx, mdist):
            if i == index or (num_edges > 2):
                continue
            e.append([i, index])
            num_edges += 1

    # out boilerplate
    out_sockets = [
        ['v', 'Vecs', [v]],
        ['s', 'Edges', e]
    ]

    return in_sockets, out_sockets
