# collect edge data and calulate 
# number of connections for each index
# by Linus Yng

import collections

def sv_main(edges=[[]]):

    in_sockets = [
        ['s', 'Edges', edges],
    ]

    count =[]    
    for pe in edges:
        # build links
        if not pe:
            continue
        node_links = collections.defaultdict(set)
        for edge in pe:
            for i in edge:
                node_links[i].update(edge)
                node_links[i].discard(i)
        count.append([len(links) for i,links in node_links.items()]) 
    
    out_sockets = [
        ['s', 'Connections', count],
    ]

    return in_sockets, out_sockets
