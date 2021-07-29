import numpy as np

def sv_main(indexes=[],vertices=[]):

    in_sockets = [
        ['s', 'indexes', indexes],
        ['v', 'vertics', vertices],
    ]
    out_sockets = [
        ['s', 'influence', []],
    ]
    if not indexes or not vertices:
        return in_sockets, out_sockets
    inds = []
    for i in indexes[0]:
        inds.extend(i)
    influence = []
    for i, v in enumerate(vertices[0]):
        influence.append(inds.count(i))
    outnp = np.array(influence)
    max = outnp.max()
    influence = outnp / max

    out_sockets[0][2] = [influence.tolist()]

    return in_sockets, out_sockets