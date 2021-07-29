import numpy as np

def sv_main(objects=[]):

    in_sockets = [
        ['s', 'numbers', objects],
    ]
    out_sockets = [
        ['s', 'mean', []],
        ['s', 'std', []],
        ['s', 'var', []],
    ]
    if not objects:
        return in_sockets, out_sockets

    array = np.array(objects)
    mean = array.mean(axis=1)
    std = array.std(axis=1)
    var = array.var(axis=1)

    out_sockets[0][2] = mean.tolist()
    out_sockets[1][2] = std.tolist()
    out_sockets[2][2] = var.tolist()

    return in_sockets, out_sockets