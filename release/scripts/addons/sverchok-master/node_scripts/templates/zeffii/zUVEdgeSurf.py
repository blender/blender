import numpy as np

def sv_main(verts=[], num_u=6, num_v=10, cyclic_u=0, cyclic_v=0):

    in_sockets = [
        ['v', 'verts', verts],
        ['s', 'num_u', num_u],
        ['s', 'num_v', num_v],
        ['s', 'cyclic_u', cyclic_u],
        ['s', 'cyclic_v', cyclic_v]]

    # arbitrary user code
    if not (cyclic_u in {0, 1}):
        cyclic_u = 0

    if not (cyclic_v in {0, 1}):
        cyclic_v = 0
        
    # adjust_to_index
    num_u -= 1
    num_v -= 1    
    num_u = max(2, num_u)
    num_v = max(2, num_v)

    # perform U polygon make
    _Up = np.array([(i, i+1, i+num_u+2, i+num_u+1) for i in range(num_u)])
    if cyclic_u:
        _UpClose = np.array([[num_u, 0, num_u+1, 2*(num_u+1)-1]])
        p = np.concatenate((_Up, _UpClose), 0)
    else:
        p = _Up

    # perform V polygon make
    if True:
        tp = p
        for j in range(1, num_v):
            offset = j*(num_u+1)
            next_level = p+offset
            tp = np.concatenate((tp, next_level), 0)
            
        if cyclic_v:
            offset = num_u+1
            m = next_level+offset
            mod = m[0][0]
            # print(m.T)
            mT = m.T
            mT0 = mT[0]
            mT1 = np.roll(mT[1] % mod, 1)
            mT2 = mT[1] % mod
            mT3 = np.roll(mT[0], -1)
            d = np.array([mT0, mT3, mT2, mT1])
            dT = d.T
            tp = np.concatenate((tp, dT), 0)
            
    else:
        tp = p

    print('num_u: {u}, num_v: {v}'.format(u=num_u, v=num_v))
    print('num_u * num_v =', (num_u+cyclic_u)*(num_v+cyclic_v))
    print(len(tp))        
    p = tp.tolist()
    # out boilerplate
    out_sockets = [
        ['s', 'Polygons', p]
    ]

    return in_sockets, out_sockets
