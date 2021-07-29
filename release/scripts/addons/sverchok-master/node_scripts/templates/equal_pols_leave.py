# check if edges/polygons are containing items, 
# than either delete them or leave

from sverchok.data_structure import fullList


def sv_main(p=[],m=[]):

    in_sockets = [
        ['s', 'p', p],
        ['s', 'm', m]]
    if not m:
        out_sockets = [
        ['s', 'out', []],
        ['s', 'out_not', p],
        ]
        
        return in_sockets, out_sockets

    out = []
    out_not = []
    for opol,omas in zip(p,m):
        fullList(omas, len(opol))
        for mas, pol in zip(omas, opol):
            if set(mas).intersection(pol):
                out.append(pol)
            else:
                out_not.append(pol)
                
    
    out_sockets = [
        ['s', 'out', [out]],
        ['s', 'out_not', [out_not]],
    ]

    return in_sockets, out_sockets
