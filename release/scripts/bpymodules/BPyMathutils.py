# $Id$
#
# --------------------------------------------------------------------------
# helper functions to be used by other scripts
# --------------------------------------------------------------------------
# ***** BEGIN GPL LICENSE BLOCK *****
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# ***** END GPL LICENCE BLOCK *****
# --------------------------------------------------------------------------

import Blender
from Blender.Mathutils import *

# ------ Mersenne Twister - start

# Copyright (C) 1997 Makoto Matsumoto and Takuji Nishimura.
# Any feedback is very welcome. For any question, comments,
# see http://www.math.keio.ac.jp/matumoto/emt.html or email
# matumoto@math.keio.ac.jp

# The link above is dead, this is the new one:
# http://www.math.sci.hiroshima-u.ac.jp/m-mat/MT/emt.html
# And here the license info, from Mr. Matsumoto's site:
# Until 2001/4/6, MT had been distributed under GNU Public License,
# but after 2001/4/6, we decided to let MT be used for any purpose, including
# commercial use. 2002-versions mt19937ar.c, mt19937ar-cok.c are considered
# to be usable freely.
#
# So from the year above (1997), this code is under GPL.

# Period parameters
N = 624
M = 397
MATRIX_A = 0x9908b0dfL   # constant vector a
UPPER_MASK = 0x80000000L # most significant w-r bits
LOWER_MASK = 0x7fffffffL # least significant r bits

# Tempering parameters
TEMPERING_MASK_B = 0x9d2c5680L
TEMPERING_MASK_C = 0xefc60000L

def TEMPERING_SHIFT_U(y):
    return (y >> 11)

def TEMPERING_SHIFT_S(y):
    return (y << 7)

def TEMPERING_SHIFT_T(y):
    return (y << 15)

def TEMPERING_SHIFT_L(y):
    return (y >> 18)

mt = []   # the array for the state vector
mti = N+1 # mti==N+1 means mt[N] is not initialized

# initializing the array with a NONZERO seed
def sgenrand(seed):
  # setting initial seeds to mt[N] using
  # the generator Line 25 of Table 1 in
  # [KNUTH 1981, The Art of Computer Programming
  #    Vol. 2 (2nd Ed.), pp102]

  global mt, mti

  mt = []
  
  mt.append(seed & 0xffffffffL)
  for i in xrange(1, N + 1):
    mt.append((69069 * mt[i-1]) & 0xffffffffL)

  mti = i
# end sgenrand


def genrand():
  global mt, mti
  
  mag01 = [0x0L, MATRIX_A]
  # mag01[x] = x * MATRIX_A  for x=0,1
  y = 0
  
  if mti >= N: # generate N words at one time
    if mti == N+1:   # if sgenrand() has not been called,
      sgenrand(4357) # a default initial seed is used

    for kk in xrange((N-M) + 1):
      y = (mt[kk]&UPPER_MASK)|(mt[kk+1]&LOWER_MASK)
      mt[kk] = mt[kk+M] ^ (y >> 1) ^ mag01[y & 0x1]

    for kk in xrange(kk, N):
      y = (mt[kk]&UPPER_MASK)|(mt[kk+1]&LOWER_MASK)
      mt[kk] = mt[kk+(M-N)] ^ (y >> 1) ^ mag01[y & 0x1]

    y = (mt[N-1]&UPPER_MASK)|(mt[0]&LOWER_MASK)
    mt[N-1] = mt[M-1] ^ (y >> 1) ^ mag01[y & 0x1]

    mti = 0

  y = mt[mti]
  mti += 1
  y ^= TEMPERING_SHIFT_U(y)
  y ^= TEMPERING_SHIFT_S(y) & TEMPERING_MASK_B
  y ^= TEMPERING_SHIFT_T(y) & TEMPERING_MASK_C
  y ^= TEMPERING_SHIFT_L(y)

  return ( float(y) / 0xffffffffL ) # reals

#------ Mersenne Twister -- end 




""" 2d convexhull
Based from Dinu C. Gherman's work,
modified for Blender/Mathutils by Campell Barton
"""
######################################################################
# Public interface
######################################################################
def convexHull(point_list_2d):
	"""Calculate the convex hull of a set of vectors
	The vectors can be 3 or 4d but only the Xand Y are used.
	returns a list of convex hull indicies to the given point list
	"""

	######################################################################
	# Helpers
	######################################################################

	def _myDet(p, q, r):
		"""Calc. determinant of a special matrix with three 2D points.

		The sign, "-" or "+", determines the side, right or left,
		respectivly, on which the point r lies, when measured against
		a directed vector from p to q.
		"""
		return (q.x*r.y + p.x*q.y + r.x*p.y)  -  (q.x*p.y + r.x*q.y + p.x*r.y)
	
	def _isRightTurn((p, q, r)):
		"Do the vectors pq:qr form a right turn, or not?"
		#assert p[0] != q[0] and q[0] != r[0] and p[0] != r[0]
		if _myDet(p[0], q[0], r[0]) < 0:
			return 1
		else:
			return 0

	# Get a local list copy of the points and sort them lexically.
	points = [(p, i) for i, p in enumerate(point_list_2d)]
	
	try:	points.sort(key = lambda a: (a[0].x, a[0].y))
	except:	points.sort(lambda a,b: cmp((a[0].x, a[0].y), (b[0].x, b[0].y)))

	# Build upper half of the hull.
	upper = [points[0], points[1]] # cant remove these.
	for i in xrange(len(points)-2):
		upper.append(points[i+2])
		while len(upper) > 2 and not _isRightTurn(upper[-3:]):
			del upper[-2]

	# Build lower half of the hull.
	points.reverse()
	lower = [points.pop(0), points.pop(1)]
	for p in points:
		lower.append(p)
		while len(lower) > 2 and not _isRightTurn(lower[-3:]):
			del lower[-2]
	
	# Concatenate both halfs and return.
	return [p[1] for ls in (upper, lower) for p in ls]


def plane2mat(plane, normalize= False):
	'''
	Takes a plane and converts to a matrix
	points between 0 and 1 are up
	1 and 2 are right
	assumes the plane has 90d corners
	'''
	cent= (plane[0]+plane[1]+plane[2]+plane[3] ) /4.0

	
	up= cent - ((plane[0]+plane[1])/2.0)
	right= cent - ((plane[1]+plane[2])/2.0)
	z= up.cross(right)
	
	if normalize:
		up.normalize()
		right.normalize()
		z.normalize()
	
	mat= Matrix(up, right, z)
	
	# translate
	mat.resize4x4()
	tmat= Blender.Mathutils.TranslationMatrix(cent)
	return mat * tmat


# Used for mesh_solidify.py and mesh_wire.py

# returns a length from an angle
# Imaging a 2d space.
# there is a hoz line at Y1 going to inf on both X ends, never moves (LINEA)
# down at Y0 is a unit length line point up at (angle) from X0,Y0 (LINEB)
# This function returns the length of LINEB at the point it would intersect LINEA
# - Use this for working out how long to make the vector - differencing it from surrounding faces,
# import math
from math import pi, sin, cos, sqrt

def angleToLength(angle):
	# Alredy accounted for
	if angle < 0.000001:	return 1.0
	else:					return abs(1.0 / cos(pi*angle/180));
