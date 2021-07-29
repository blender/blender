"""
in normals v .=[] .=0
in coords v .=[] .=0
in radius s .=0.4 .=2
in num_tries s .=100 .=2
out new_verts v
"""

# taken from http://stackoverflow.com/a/29354206/1243487
# translated to python and modified for sverchok.

from math import sqrt, cos, sin, pi as M_PI
from random import uniform as rand


def crossp(u, v):
    w = [0, 0, 0]
    w[0] = (u[1] * v[2]) - (u[2] * v[1])
    w[1] = (u[2] * v[0]) - (u[0] * v[2])
    w[2] = (u[0] * v[1]) - (u[1] * v[0])
    return w  # vec3

def dotp(u, v):
    return (u[0] * v[0]) + (u[1] * v[1]) + (u[2] * v[2])  # float

def norm2(u):
    return dotp(u, u)  # float

def norm(u):
    return sqrt(norm2(u))  # float

def scale(u, s):
    return [u[0] * s, u[1] * s, u[2] * s]  # vec3

def add(u, v):
    return [u[0] + v[0], u[1] + v[1], u[2] + v[2]] # vec3

def normalize(u):
    return scale(u, 1/norm(u)) # vec3


def random_on_plane(r, n, co):
    """
    generates a random point on the plane ax + by + cz = d
    """
    n = list(n)
    co = list(co)
    d = dotp(n, co)
    xorz = [1, 0, 0] if (n[0] == 0) else [0, 0, 1]
    w = crossp(n, xorz)

    theta = rand(0, 1) * 2 * M_PI
    k = normalize(n)
    w = add(scale(w, cos(theta)), 
            scale(crossp(k, w), sin(theta)))

    if r == 0:
        w = scale(w, r/norm(w))
    else:
        rand_r = rand(0, 1) * r
        w = scale(w, rand_r/norm(w))

    if d != 0:
        t = scale(n, d / norm2(n))  # vec3
        w = add(w, t)

    return w


new_verts = []
for nlist, colist in zip(normals, coords):
    sublist = []
    for n, co in zip(nlist, colist):
        sublist.append([random_on_plane(radius, n, co) for _ in range(num_tries)])
    new_verts.append(sublist)

new_verts = [new_verts]
