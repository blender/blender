"""Vector tools

	Various vector tools, basing on vect.py"""

from vect import *

EPSILON = 0.0001

def vecarea(v, w):
	"Computes area of the span of vector 'v' and 'w' in 2D (not regarding z coordinate)"
	return v[0]*w[1] - v[1]*w[0]

def intersect(a1, b1, a2, b2):
	"""Computes 2D intersection of edges ('a1' -> 'b1') and ('a2' -> 'b2'), 
returning normalized intersection parameter 's' of edge (a1 -> b1). 
If 0.0 < 's' <= 1.0,
the two edges intersect at the point::

	v = a1 + s * (b1 - a1)
"""
	v = (b1[0] - a1[0], b1[1] - a1[1])
	w = (b2[0] - a2[0], b2[1] - a2[1])
	d0 = (a2[0] - a1[0])
	d1 = (a2[1] - a1[1])

	det = w[0]*v[1] - w[1]*v[0]
	if det == 0: return 0.0
	t = v[0]*d1 - v[1]*d0
	s = w[0]*d1 - w[1]*d0
	s /= det
	t /= det
	if s > 1.0 or s < 0.0: return 0.0
	if t > 1.0 or t < 0.0: return 0.0
	return s

def insidetri(a, b, c, x):
	"Returns 1 if 'x' is inside the 2D triangle ('a' -> 'b' -> 'c'), 0 otherwise"
	v1 = norm3(sub3(b, a))
	v2 = norm3(sub3(c, a))
	v3 = norm3(sub3(x, a))
	
	a1 = (vecarea(v1, v2))
	a2 = (vecarea(v1, v3))
	lo = min(0.0, a1)
	hi = max(0.0, a1)

	if a2 < lo or a2 > hi: return 0

	v2 = norm3(sub3(b, c))
	v3 = norm3(sub3(b, x))

	a1 = (vecarea(v1, v2))
	a2 = (vecarea(v1, v3))

	lo = min(0.0, a1)
	hi = max(0.0, a1)
	
	if a2 < lo or a2 > hi: return 0

	return 1

def plane_fromface(v1, v2, v3):
	"Returns plane (normal, point) from 3 vertices 'v1', 'v2', 'v3'"
	v = sub3(v2, v1)
	w = sub3(v3, v1)
	n = norm3(cross(v, w))
	return n, v1

def inside_halfspace(vec, plane):
	"Returns 1 if point 'vec' inside halfspace defined by 'plane'"
	n, t = plane
 	n = norm3(n)
	v = sub3(vec, t)
	if dot(n, v) < 0.0:
		return 1
	else:
		return 0

def half_space(vec, plane, tol = EPSILON):
	"""Determine whether point 'vec' is inside (return value -1), outside (+1)
, or lying in the plane 'plane' (return 0) of a numerical thickness 
'tol' = 'EPSILON' (default)."""
	n, t = plane
	v = sub3(vec, t)
	fac = len3(n)

	d = dot(n, v)
	if d < -fac * tol:
		return -1
	elif d > fac * tol:
		return 1
	else:
		return 0
	

def plane_edge_intersect(plane, edge):
	"""Returns normalized factor 's' of the intersection of 'edge' with 'plane'.
The point of intersection on the plane is::

	p = edge[0] + s * (edge[1] - edge[0])

"""
	n, t = plane # normal, translation
	mat = matfromnormal(n)
	mat = transmat(mat) # inverse
	v = matxvec(mat, sub3(edge[0], t)) #transformed edge points
	w = matxvec(mat, sub3(edge[1], t)) 
	w = sub3(w, v)
	if w[2] != 0.0:
		s = -v[2] / w[2]
		return s
	else:
		return None

def insidecube(v):
	"Returns 1 if point 'v' inside normalized cube, 0 otherwise"
	if v[0] > 1.0 or v[0] < 0.0:
		return 0
	if v[1] > 1.0 or v[1] < 0.0:
		return 0
	if v[2] > 1.0 or v[2] < 0.0:
		return 0
	return 1


def flatproject(verts, up):
	"""Projects a 3D set (list of vertices) 'verts' into a 2D set according to
an 'up'-vector"""
	z, t = plane_fromface(verts[0], verts[1], verts[2])
	y = norm3(up)
	x = cross(y, z)
	uvs = []
	for v in verts:
		w = (v[0] - t[0], v[1] - t[1], v[2] - t[2])
		# this is the transposed 2x2 matrix * the vertex vector
		uv = (dot(x, w), dot(y,w)) # do projection
		uvs.append(uv)
	return uvs




