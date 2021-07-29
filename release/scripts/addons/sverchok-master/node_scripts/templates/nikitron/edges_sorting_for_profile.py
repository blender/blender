
def dodo(verts,edges,verts_o,k):
    for i in edges:
        if k in i:
            # this is awesome !!
            k = i[int(not i.index(k))]
            verts_o.append(verts[k])
            return k, i
    return False, False

def sv_main(v=[],e=[]):

    in_sockets = [
        ['v', 'v', v],
        ['s', 'e', e]]
    vout = []
    eout = []
    if v:
        for edges,verts in zip(e,v):
            ed = 1
            edges_o = []
            verts_o = []
            k = 0
            while True:
                k, ed = dodo(verts,edges,verts_o,k)
                if ed:
                    edges.remove(ed)
                if not ed:
                    break
            edges_o = [[k,k+1] for k in range(len(verts_o)-1)]
            edges_o.append([0,len(verts_o)-1])
            eout.append(edges_o)
            vout.append(verts_o)

    out_sockets = [
        ['v', 'v', vout],
        ['s', 'e', eout]]

    return in_sockets, out_sockets
