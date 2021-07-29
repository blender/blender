import mathutils
from sverchok.data_structure import (match_long_cycle)


def sv_main(V=[], P=[[]], S=[], T=[]):

    in_sockets = [
        ['v', 'Points', V],
        ['s', 'Polys', P],
        ['v', 'SourceTri', S],
        ['v', 'TargetTri', T]

        ]

    outV,outP = [],[]
    if V and S and T:
        v,p,s,t = match_long_cycle([V,P,S,T])
        for v,p,s,t in zip(v,p,s,t):
            outV.append([mathutils.geometry.barycentric_transform(i, s[0], s[1], s[2], t[0], t[1], t[2])[:] for i in v])
            outP.append(p)

    out_sockets = [
        ['v', 'TransformedPoints', outV],
        ['s', 'Polygons', outP]
    ]

    return in_sockets, out_sockets
