#split a mesh two lists with edge points
# by Linus Yng 
def sv_main(verts=[[]],edges=[[]]):
 
    in_sockets = [
        ['v', 'Vertices',  verts],
        ['s', 'Edges',  edges],
    ]
    pt0, pt1 = [], []
    for v,es in zip(verts,edges):
        pt0_i, pt1_i = [], []
        for e in edge:
            pt0_i.append(v[e[0]])
            pt1_i.append(v[e[1]])
        pt0.append(pt0_i)
        pt1.append(pt1_i)
        
    out_sockets = [
        ['v', 'Pt0', pt0],
        ['v', 'Pt1', pt1],
    ]
 
    return in_sockets, out_sockets
