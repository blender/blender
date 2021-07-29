import mathutils
from mathutils import Vector

def sv_main(vectors=[]):

    in_sockets = [
        ['v', 'Vertices', vectors]
    ]

    outV = []
    if vectors:
        for i in vectors:
            outV.append([Vector(i2) for i2 in i])

    out_sockets = [
        ['v', 'Mathutils variant', outV]
    ]

    return in_sockets, out_sockets
