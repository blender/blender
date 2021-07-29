"""
in in_verts v
in in_edges s
out vout v
out eout s
"""


def dodo(verts, edges, verts_o,k):
    for i in edges:
        if k in i:
            # this is awesome !!
            k = i[int(not i.index(k))]
            verts_o.append(verts[k])
            return k, i
    return False, False

if in_verts:
    for edges, verts in zip(in_edges, in_verts):
        ed = 1
        edges_o = []
        verts_o = []
        k = 0
        while True:
            k, ed = dodo(verts, edges, verts_o,k)
            if ed:
                edges.remove(ed)
            if not ed:
                break
        edges_o = [[k,k+1] for k in range(len(verts_o)-1)]
        edges_o.append([0, len(verts_o)-1])
        eout.append(edges_o)
        vout.append(verts_o)
