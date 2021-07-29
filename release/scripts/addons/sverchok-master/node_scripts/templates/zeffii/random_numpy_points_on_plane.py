import numpy as np

def sv_main(n_items=1500):

    in_sockets = [
        ['s', 'n_items', n_items]]

    points=np.random.uniform(0.0,1.0,size = (n_items,2))
    points *= (2, 10)

    a = points[:,0]
    b = points[:,1]
    c = np.array([0.0 for i in b])
    d = np.column_stack((a,b,c))

    # consumables
    Verts = [d.tolist()]

    out_sockets = [
        ['v', 'Verts', Verts]
    ]

    return in_sockets, out_sockets
