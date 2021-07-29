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

class LorenzAttractor(SvScriptSimpleGenerator):
    
    @staticmethod
    def make_verts(N, h, a, b, c):

        verts = []
        lorenz(N, verts, h, a, b, c)
        return verts

    @staticmethod
    def make_edges(N=1000, h=0.01, a=10.0, b=28.0, c=8.0/3.0):
        edges = [(i, i+1) for i in range(N-1)]
        return edges

    inputs = [
        ['s', 'N', 1000],
        ['s', 'h', 0.01],
        ['s', 'a', 10.0],
        ['s', 'b', 28.0],
        ['s', 'c', 8.0/3.0]]

    outputs = [
        ['v','verts', "make_verts"],
        ['s','edges', "make_edges"]
    ]
