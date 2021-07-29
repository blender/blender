def sv_main(N=1000):

    in_sockets = [
        ['s', 'N', N]]

    verts = []
    out_sockets = [
        ['v','verts', [verts]]
    ]

    i = 0
    h = 0.01
    a = 10.0
    b = 28.0
    c = 8.0 / 3.0

    x0 = 0.1
    y0 = 0
    z0 = 0
    add_vert = verts.append
    for i in range(N):
        x1 = x0 + h * a * (y0 - x0)
        y1 = y0 + h * (x0 * (b - z0) - y0)
        z1 = z0 + h * (x0 * y0 - c * z0)
        x0, y0, z0 = x1, y1, z1

        add_vert((x1,y1,z1))

    return in_sockets, out_sockets
