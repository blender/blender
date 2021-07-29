from mathutils import Vector
import numpy as np
from sverchok.data_structure import Vector_generate, Vector_degenerate

def sv_main(powers=[],pow_str=[],points=[],lent=0.1,subs=30):

    in_sockets = [
        ['v', 'powers', powers],
        ['s', 'pow_str', pow_str],
        ['v', 'points', points],
        ['s', 'lent', lent],
        ['s', 'subs', subs]]

    verts_out = []
    edges_out = []

    def out_sockets():
        return [
            ['v', 'verts_out', verts_out],
            ['s', 'faces_out', edges_out]]
    if not all([powers, pow_str, points]):
        return in_sockets, out_sockets()


    #powers = [np.array(i) for i in powers[0]]
    #points = [np.array(i) for i in points[0]]

    points = Vector_generate(points)[0]
    powers = Vector_generate(powers)[0]

    def nextpoint(poi,powers):
        verts_ = [poi-pow for pow in powers]
        vect = Vector()
        for i in verts_:
            vect+=i*(1/i.length**2)
        vect.normalize()
        # additional power:
        #cos(x)
        #sin
        #3*exp(-(x**2+3**2)**2)
        vertnext = poi + vect*(1/lent)
        return vertnext
        
    v = []
    for poi in points:
        vers = []
        edgs = []
        for k in range(subs):
            vertnext = nextpoint(poi,powers)
            vers.append(poi)
            poi = vertnext
            if k > 0:
                edgs.append([k-1,k])
            
        edges_out.append(edgs)
        verts_out.append(vers)
    verts_out = Vector_degenerate(verts_out)

    return in_sockets, out_sockets()