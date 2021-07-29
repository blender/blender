"""

after octave code by cicciospice, numpy recode by zeffii

function [x,y,z] = hilbert3(n)
    if n <= 0
        x = 0;
        y = 0;
        z = 0;
    else
        [xo,yo,zo] = hilbert3(n-1);
        x = .5*[.5+zo .5+yo -.5+yo -.5-xo -.5-xo -.5-yo .5-yo .5+zo];
        y = .5*[.5+xo .5+zo .5+zo .5+yo -.5+yo -.5-zo -.5-zo -.5-xo];
        z = .5*[.5+yo -.5+xo -.5+xo .5-zo .5-zo -.5+xo -.5+xo .5-yo];
end 
"""

import numpy as np

def sv_main(n=3):

    in_sockets = [
        ['s', 'n', n]]

    def hilbert3(n):
        if (n <= 0):
            x, y, z = 0, 0, 0
        else:
            [xo, yo, zo] = hilbert3(n-1)
            x = .5 * np.array([.5+zo, .5+yo, -.5+yo, -.5-xo, -.5-xo, -.5-yo, .5-yo, .5+zo])
            y = .5 * np.array([.5+xo, .5+zo, .5+zo, .5+yo, -.5+yo, -.5-zo, -.5-zo, -.5-xo])
            z = .5 * np.array([.5+yo, -.5+xo, -.5+xo, .5-zo, .5-zo, -.5+xo, -.5+xo, .5-yo])
        return [x, y, z]

    vx, vy, vz = hilbert3(n)
    vx = vx.flatten().tolist()
    vy = vy.flatten().tolist()
    vz = vz.flatten().tolist()
    verts = [list(zip(vx, vy, vz))]
    
    out_sockets = [
        ['v', 'verts', verts]
    ]

    return in_sockets, out_sockets
