# will make fractal from geometry. not sure what exactly I doing
# will remake to fully node

import mathutils
from mathutils import Vector, Matrix
from sverchok.data_structure import Vector_degenerate, Vector_generate

def sv_main(vec=[],pol=[],iter=3):

    in_sockets = [
        ['v', 'vec', vec],
        ['s', 'pol', pol],
        ['s', 'iter', iter]]
    
    if not vec: vec=[[[0,1,1],[0,2,2],[3,3,0]]]
    if not pol: pol=[[[0,1,2]]]
    vec_gen = Vector_generate(vec)
    
    def fractalize(vec_gen,pol,iter):
        out = []
        if iter:
            print(vec_gen, iter)
            vers_new = []
            for obj in vec_gen:
                for v1 in obj:
                    up = [v1/2+v2/2 for v2 in vec_gen[0]]
                    vers_new.append(up)
            #print(levelsOflist(vec_gen))
            #pols_new =[[vec[p] for p in objpol] for objpol in pol]
            out = fractalize(vers_new, pol, iter-1)
        else:
            out = dataCorrect(vec_gen, 1)
        #print(out)
        return out
    
    out_ = fractalize(vec_gen, pol, iter)
    out = Vector_degenerate(out_)
    #print(out)
    
    out_sockets = [
        ['v','vec', out]
    ]

    return in_sockets, out_sockets
