from math import sin
from mathutils import Vector
def sv_main(x=1,n=[]):

    in_sockets = [
        ['s', 'x', x],
        ['s', 'n', n]]
    locs = []
    size = []
    names = []
    if n:
        tree = bpy.data.node_groups[n[0][0][0]].nodes
        for i in tree:
            i.location[1] = sin(i.location[0]/200+x)*100
            locs.append((i.location[0]/100,0,i.location[1]/100))
            size.append((i.width/100,i.height/100,i.height/100))
            names.append([i.name])
    
    out_sockets = [
        ['v','x',[locs]],
        ['v','y',[size]],
        ['s','z',names]]
    return in_sockets, out_sockets
