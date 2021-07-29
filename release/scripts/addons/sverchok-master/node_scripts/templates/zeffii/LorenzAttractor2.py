def lorenz(N, verts, h, a, b, c):
    add_vert = verts.append

    x0 = 0.1
    y0 = 0
    z0 = 0
    for i in range(N):
        x1 = x0 + h * a * (y0 - x0)
        y1 = y0 + h * (x0 * (b - z0) - y0)
        z1 = z0 + h * (x0 * y0 - c * z0)
        x0, y0, z0 = x1, y1, z1

        add_vert((x1,y1,z1))

def sv_main(N=1000, h=0.01, a=10.0, b=28.0, c=8.0/3.0):

    verts = []
    lorenz(N, verts, h, a, b, c)
    edges = [(i, i+1) for i in range(len(verts)-1)]

    in_sockets = [
        ['s', 'N', N],
        ['s', 'h', h],
        ['s', 'a', a],
        ['s', 'b', b],
        ['s', 'c', c]]

    out_sockets = [
        ['v','verts', [verts]],
        ['s','edges', [edges]]
    ]
    
    return in_sockets, out_sockets
