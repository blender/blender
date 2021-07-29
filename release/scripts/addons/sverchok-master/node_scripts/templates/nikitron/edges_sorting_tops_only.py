import mathutils 
from mathutils import Vector
 
def sv_main(verts=[[]],edges=[[]],count=1):

    in_sockets = [
        ['v', 'Vertices',  verts],
        ['s', 'Edges',  edges],
        ['s', 'Count', count]
    ]

    Edges_out = []
    count = min(len(edges[0]), count)

    def out_sockets():
        return [['s','Edges',Edges_out]]

    if not verts[0]:
        return in_sockets, out_sockets()

    edges[0].sort(key=lambda ed:(verts[0][ed[0]][2] + verts[0][ed[1]][2]) / 2)
    Edges_out = [edges[0][-count:]]
 
    return in_sockets, out_sockets()
