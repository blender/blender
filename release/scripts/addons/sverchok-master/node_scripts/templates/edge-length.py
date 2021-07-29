import mathutils 
from mathutils import Vector

#get length of all edges
 
def sv_main(verts=[[]],edges=[[]]):
 
    # in boilerplate - make your own sockets
    in_sockets = [
        ['v', 'Vertices',  verts],
        ['s', 'Edges',  edges],
    ]
    lengths = []
    for v,es in zip(verts,edges):
        lens=[]
        for e0,e1 in es:
            lens.append((Vector(v[e0])-Vector(v[e1])).length) 
        lengths.append(lens)    
    
    out_sockets = [
        ['s', 'Lengths', lengths],
    ]
 
    return in_sockets, out_sockets
