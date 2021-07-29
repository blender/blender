"""
in   quads   v   .=[]   n=0
in   points  s   .=4    n=2
out  verts   v
out  edges   s
"""

print('---')

from mathutils.geometry import interpolate_bezier as bezlerp
from mathutils import Vector, Euler

Verts = []
Edges = []

def mid(A, B):
    return (A[0]+B[0])/2, (A[1]+B[1])/2, (A[2]+B[2])/2

for quad in quads:
    if len(quad) == 4:
        sverts, sedges = [], []
        for s in range(4):

            knot1 = mid(quad[s%4], quad[(s+1)%4])
            ctrl_1 = mid(knot1, quad[(s+1)%4])
            knot2 =  mid(quad[(s+1)%4], quad[(s+2)%4])       
            ctrl_2 = mid(knot2, quad[(s+1)%4])

            arc_verts = bezlerp(knot1, ctrl_1, ctrl_2, knot2, max(3, points))
            sverts.extend([v[:] for v in arc_verts[:-1]])

        arc_edges = [(n, n+1) for n in range(len(sverts)-1)]
        arc_edges.append([len(sverts)-1, 0])
        Edges.append([arc_edges])
        Verts.append([sverts])

verts = [Verts]
edges = [Edges]
