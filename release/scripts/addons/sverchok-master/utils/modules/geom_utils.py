# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

import math

def interp_v3_v3v3(a, b, t=0.5):
    if t == 0.0: return a
    elif t == 1.0: return b
    else:
        s = 1.0 - t
        return (s * a[0] + t * b[0], s * a[1] + t * b[1], s * a[2] + t * b[2])

def length(v):
    return math.sqrt((v[0] * v[0]) + (v[1] * v[1]) + (v[2] * v[2]))

def length_v2(v):
    return math.sqrt((v[0] * v[0]) + (v[1] * v[1]))

def normalize(v):
    l = math.sqrt((v[0] * v[0]) + (v[1] * v[1]) + (v[2] * v[2]))
    return [v[0]/l, v[1]/l, v[2]/l]

def sub_v3_v3v3(a, b):
    return a[0]-b[0], a[1]-b[1], a[2]-b[2]

def madd_v3_v3v3fl(a, b, f=1.0):
    return a[0]+b[0]*f, a[1]+b[1]*f, a[2]+b[2]*f

def dot_v3v3(a, b):
    return a[0]*b[0]+a[1]*b[1]+a[2]*b[2]

def isect_line_plane(l1, l2, plane_co, plane_no):
    u = l2[0]-l1[0], l2[1]-l1[1], l2[2]-l1[2]
    h = l1[0]-plane_co[0], l1[1]-plane_co[1], l1[2]-plane_co[2]
    dot = plane_no[0]*u[0] + plane_no[1]*u[1] + plane_no[2]*u[2]

    if abs(dot) > 1.0e-5:
        f = -(plane_no[0]*h[0] + plane_no[1]*h[1] + plane_no[2]*h[2]) / dot
        return l1[0]+u[0]*f, l1[1]+u[1]*f, l1[2]+u[2]*f
    else:
        # parallel to plane
        return False

def obtain_normal3(p1, p2, p3):
    # http://stackoverflow.com/a/8135330/1243487
    return [
        ((p2[1]-p1[1])*(p3[2]-p1[2]))-((p2[2]-p1[2])*(p3[1]-p1[1])),
        ((p2[2]-p1[2])*(p3[0]-p1[0]))-((p2[0]-p1[0])*(p3[2]-p1[2])),
        ((p2[0]-p1[0])*(p3[1]-p1[1]))-((p2[1]-p1[1])*(p3[0]-p1[0]))
    ]

def mean(verts):
    # expects a list.. something that has len()
    vx, vy, vz = 0, 0, 0
    for v in verts:
        vx += v[0]
        vy += v[1]
        vz += v[2]
    numverts = len(verts)
    return vx/numverts, vy/numverts, vz/numverts


def is_reasonably_opposite(n, normal_one):
    return dot_v3v3(normalized(n), normalized(normal_one)) < 0.0


def pt_in_triangle(p_test, p0, p1, p2):
    # Function taken from Ramiro R.C https://stackoverflow.com/a/46409704
    dX = p_test[0] - p0[0]
    dY = p_test[1] - p0[1]
    dX20 = p2[0] - p0[0]
    dY20 = p2[1] - p0[1]
    dX10 = p1[0] - p0[0]
    dY10 = p1[1] - p0[1]

    s_p = (dY20*dX) - (dX20*dY)
    t_p = (dX10*dY) - (dY10*dX)
    D = (dX10*dY20) - (dY10*dX20)

    if D > 0:
        return (  (s_p >= 0) and (t_p >= 0) and (s_p + t_p) <= D  )
    else:
        return (  (s_p <= 0) and (t_p <= 0) and (s_p + t_p) >= D  )
