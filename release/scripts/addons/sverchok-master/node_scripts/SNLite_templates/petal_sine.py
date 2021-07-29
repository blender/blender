"""
in   n_petals s        default=8       nested=2
in   vp_petal s        default=10      nested=2
in   profile_radius s  ;=2.3           n=2
in   amp s             default=1.0     nested=2
in   origin v          defautt=(0,0,0) n=2
out  verts v
out  edges s
"""

from math import sin, cos, radians, pi
from mathutils import Vector, Euler

# create flower
n_verts = n_petals * vp_petal
section_angle = 360.0 / n_verts
position = (2 * (pi / (n_verts / n_petals)))
x, y, z = origin[:3]

Verts = []
Edges = []

for i in range(n_verts):
    difference = amp * cos(i * position)
    arm = profile_radius + difference
    ampline = Vector((arm, 0.0, 0.0))

    rad_angle = radians(section_angle * i)
    myEuler = Euler((0.0, 0.0, rad_angle), 'XYZ')
    ampline.rotate(myEuler)
    Verts.append((ampline.x+x, ampline.y+y, 0.0+z))

# makes edge keys, ensure cyclic
if Verts:
    i = 0
    Edges.extend([[i, i + 1] for i in range(n_verts - 1)])
    Edges.append([len(Edges), 0])

verts = [Verts]
edges = [Edges]
