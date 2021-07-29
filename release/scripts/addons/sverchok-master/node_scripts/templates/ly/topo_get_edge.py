# sort topology to make one edge
# for testing

import collections

def sv_main(verts=[[]],edges=[[]]):

    in_sockets = [
        ['v', 'Verts', verts],
        ['s', 'Edges', edges],
    ]

    verts_out = []
    edge_out = [] 
        
    for ve,pe in zip(verts,edges):
        # build links
        if not ve:
            continue
        node_links = collections.defaultdict(set)
        for edge in pe:
            for i in edge:
                node_links[i].update(edge)
                node_links[i].discard(i)

        start = None
        for i,links in node_links.items():
            if len(links) == 1:
                start = i
                        
        out_list = collections.OrderedDict()
        if start:
            out_list[start]=True
        else:
            out_list[0]=True

        nodes = node_links.get(start)    
        while nodes:
            indx = nodes.pop()
            if not indx in out_list:
                nodes = node_links.get(indx)
                out_list[indx]=True
        
        new_vert = [ve[i] for i in out_list.keys()]
        new_pe = [[i,i+1] for i in range(len(out_list)-1)]

        verts_out.append(new_vert)
        edge_out.append(new_pe)
    
    out_sockets = [
        ['v', 'Verts', verts_out],
        ['s', 'Edges', edge_out],
    ]

    return in_sockets, out_sockets
