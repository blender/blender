# it separates vertical and horisontal edges
from sverchok.data_structure import sv_zip
from math import sin


def sv_main(v=[],e=[],tres=1):
    
    in_sockets = [
        ['v', 'Vertices',  v],
        ['s', 'edges', e],
        ['s', 'Threshold', tres],
    ]
    

    edgV=[]
    edgH=[]
    edgX=[]
    if v:
        v = v[0]
        if tres<1:
            tre=1
        if tres>2:
            tre=2
        else:
            tre = tres
        for edg in e[0]:
            if round(v[edg[0]][2],tre) == round(v[edg[1]][2],tre):
                edgH.append(edg)
            elif round(v[edg[0]][0],tre) == round(v[edg[1]][0],tre) and round(v[edg[0]][1],tre) == round(v[edg[1]][1],tre):
                edgV.append(edg)
            else:
                edgX.append(edg)
    
    out_sockets = [
        ['v', 'Vers', v],
        ['s', 'edgV', [edgV]],
        ['s', 'edgH', [edgH]],
        ['s', 'edgX', [edgX]],
    ]

    return in_sockets, out_sockets

