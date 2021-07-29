import numpy as np
import inspect

def sv_main(b=89, n_items=1500, q_factor_x=2, q_factor_y=2, seed=6, a=90):

    in_sockets = [
        ['s', 'n_items', n_items],
        ['s', 'q_factor_x', q_factor_x],
        ['s', 'q_factor_y', q_factor_y],
        ['s', 'seed', seed]]

    np.random.seed(seed)
    points=np.random.uniform(0.0,1.0,size = (n_items,2))
    points *= (1000, 200)

    a = points[:,0]
    b = points[:,1]
    a = np.floor(a / q_factor_x) * q_factor_x
    b = np.floor(b / q_factor_y) * q_factor_y
    c = np.array([0.0 for i in b])
    d = np.column_stack((a,b,c))

    # consumables
    Verts = d.tolist()

    def func1():
        print(Verts[:3])


    out_sockets = [
        ['v', 'Verts', [Verts]]
    ]

    ui_operators = [
        ['button_name', func1]
    ]

    return in_sockets, out_sockets, ui_operators

f = sv_main
a, b, ops = sv_main()
f.button1 = ops[0]

print(f.button1[0])
f.button1[1]()

