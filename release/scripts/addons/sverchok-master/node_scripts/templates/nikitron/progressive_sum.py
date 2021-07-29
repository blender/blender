import mathutils 
from mathutils import Vector

#get length of all edges
 
def sv_main(data=[],divider=1.0,adding=0.0,zerostart=False):
 
    # in boilerplate - make your own sockets
    in_sockets = [
        ['s', 'data', data],
        ['s', 'multiplier', divider],
        ['s', 'adding', adding],
        ['s', 'zerostart', zerostart],
    ]
    def obj_sum(data):
        if zerostart:
            out = [0]
            data = data[:-1]
        else:
            out = []
        i = 0
        for d in data:
            i += (d+adding)*divider
            out.append(i)
        return out
    out = [obj_sum(i[0]) for i in data]
    out_sockets = [
        ['s', 'data', out],
    ]
 
    return in_sockets, out_sockets
