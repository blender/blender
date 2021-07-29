import mathutils
import random
import collections
from itertools import islice
from mathutils import Vector

def sv_main(objs=[], seed=1, start=-1, step=5, switch=.9):

    # in boilerplate, could be less verbose
    in_sockets = [
        ['v', 'verts', objs],
        ['s', 'seed', seed],
        ['s', 'start', start],
    ]
    
    random.seed(seed)
    edge_out = []
    
    for verts in objs:
        if not verts:
            break
    
        size = len(verts)
        kd = mathutils.kdtree.KDTree(size)

        for i, v in enumerate(verts):
            kd.insert(v, i)
        kd.balance()
        if 0 < start and start < size:
            index = start
        else:
            index = random.randrange(size)
        vecs = None
        out = collections.OrderedDict({index:0})
        while len(out) < size:
            #print(round(len(out)/size,3))
            if (len(out) / size) < .9: 
                found_next = False
                n = 0
                step = 10
                while not found_next:
                    count = min(size, n+step)
                    for pt, n_i, dist in islice(kd.find_n(verts[index], count), n, count):
                        if not n_i in out:
                            out[n_i] = index
                            index = n_i
                            found_next = True
                            break
                    if n > size:
                        break
                    n += step
            else: #now we reduced the number of verts and the above gets very slow
                if not vecs:
                    del kd
                    total_set = set(range(size))
                    indx = total_set - set(out.keys())
                    vecs = {i:Vector(verts[i]) for i in indx}
                    
                c_v = Vector(verts[index])
                dists = [((vecs[i]-c_v).length, i) for i in indx]
                pt, n_i = min(dists, key=lambda x:x[0])
                out[n_i] = index
                index = n_i
                indx.discard(index)
                
        out.popitem(last=False)
        edge_out.append([(j,k) for j,k in out.items()])

    out_sockets = [
        ['s', 'Edges', edge_out]
    ]

    return in_sockets, out_sockets
