#------------------------------------------------------------------------------
# simple 3D vector / matrix class 
#
# (c) 9.1999, Martin Strubel // onk@section5.de
# updated 4.2001 
#
# This module consists of a rather low level command oriented
# and a more OO oriented part for 3D vector/matrix manipulation
#
# For documentation, please look at the EXAMPLE code below - execute by:
#
# >  python vect.py
#
#
# permission to use in scientific and free programs granted
# In doubt, please contact author.
# 
# history:
#
# 1.5: Euler/Rotation matrix support moved here
# 1.4: high level Vector/Matrix classes extended/improved
#

"""Vector and matrix math module

	Version 1.5
	by onk@section5.de

	This is a lightweight 3D matrix and vector module, providing basic vector
	and matrix math plus a more object oriented layer. 

	For examples, look at vect.test()
"""

VERSION = 1.5

TOLERANCE = 0.0000001

VectorType = 'Vector3'
MatrixType = 'Matrix3'
FloatType = type(1.0)
		
def dot(x, y):
	"(x,y) - Returns the dot product of vector 'x' and 'y'"
	return (x[0] * y[0] + x[1] * y[1] + x[2] * y[2])

def cross(x, y):
	"(x,y) - Returns the cross product of vector 'x' and 'y'"
	return	(x[1] * y[2] - x[2] * y[1],
			x[2] * y[0] - x[0] * y[2],
			x[0] * y[1] - x[1] * y[0])

def matrix():
	"Returns Unity matrix"
	return ((1.0, 0.0, 0.0), (0.0, 1.0, 0.0), (0.0, 0.0, 1.0))

def matxvec(m, x):
	"y = matxvec(m,x) - Returns product of Matrix 'm' and vector 'x'"
	vx = m[0][0] * x[0] + m[1][0] * x[1] + m[2][0] * x[2]
	vy = m[0][1] * x[0] + m[1][1] * x[1] + m[2][1] * x[2]
	vz = m[0][2] * x[0] + m[1][2] * x[1] + m[2][2] * x[2]
	return (vx, vy, vz)

def matfromnormal(z, y = (0.0, 1.0, 0.0)):
	"""(z, y) - returns transformation matrix for local coordinate system
		where 'z' = local z, with optional *up* axis 'y'"""
	y = norm3(y)
	x = cross(y, z)
	y = cross(z, x)
	return (x, y, z)

def matxmat(m, n):
	"(m,n) - Returns matrix product of 'm' and 'n'"
	return (matxvec(m, n[0]), matxvec(m, n[1]), matxvec(m, n[2]))

def len(x):
	"(x) - Returns the length of vector 'x'"
	import math
	return math.sqrt(x[0]*x[0] + x[1]*x[1] + x[2]*x[2])

len3 = len # compatiblity reasons

def norm3(x):
	"(x) - Returns the vector 'x' normed to 1.0"
	import math
	r = math.sqrt(x[0]*x[0] + x[1]*x[1] + x[2]*x[2])
	return (x[0]/r, x[1]/r, x[2]/r)

def add3(x, y):
	"(x,y) - Returns vector ('x' + 'y')"
	return (x[0]+y[0], x[1]+y[1], x[2]+y[2])

def sub3(x, y):
	"(x,y) - Returns vector ('x' - 'y')"
	return ((x[0] - y[0]), (x[1] - y[1]), (x[2] - y[2]))

def dist3(x, y):
	"(x,y) - Returns euclidian distance from Point 'x' to 'y'"
	return len3(sub3(x, y))

def scale3(s, x):
	"(s,x) - Returns the vector 'x' scaled by 's'"
	return (s*x[0], s*x[1], s*x[2])

def scalemat(s,m):
	"(s,m) - Returns the Matrix 'm' scaled by 's'"
	return (scale3(s, m[0]), scale3(s, m[1]), scale3(s,m[2]))

def invmatdet(m):
	"""n, det = invmat(m) - Inverts matrix without determinant correction.
	     Inverse matrix 'n' and Determinant 'det' are returned"""

	# Matrix: (row vectors)
	# 00 10 20
	# 01 11 21
	# 02 12 22

	wk = [0.0, 0.0, 0.0]

	t = m[1][1] * m[2][2] - m[1][2] * m[2][1]
	wk[0] = t
	det = t * m[0][0]

	t = m[2][1] * m[0][2] - m[0][1] * m[2][2]
	wk[1] = t
	det = det + t * m[1][0]

	t = m[0][1] * m[1][2] - m[1][1] * m[0][2]
	wk[2] = t
	det = det + t * m[2][0]

	v0 = (wk[0], wk[1], wk[2])

	t = m[2][0] * m[1][2] - m[1][0] * m[2][2]
	wk[0] = t
	det = det + t * m[0][1]
	
	t = m[0][0] * m[2][2] - m[0][2] * m[2][0]
	wk[1] = t
	det = det + t * m[1][1]

	t = m[1][0] * m[0][2] - m[0][0] * m[1][2]
	wk[2] = t
	det = det + t * m[2][1]

	v1 = (wk[0], wk[1], wk[2])

	t = m[1][0] * m[2][1] - m[1][1] * m[2][0]
	wk[0] = t
	det = det + t * m[0][2]

	t = m[2][0] * m[0][1] - m[0][0] * m[2][1]
	wk[1] = t
	det = det + t * m[1][2]

	t = m[0][0] * m[1][1] - m[1][0] * m[0][1]
	wk[2] = t
	det = det + t * m[2][2]

	v2 = (wk[0], wk[1], wk[2])
	# det = 3 * determinant
	return ((v0,v1,v2), det/3.0)

def invmat(m):
	"(m) - Inverts the 3x3 matrix 'm', result in 'n'"
	n, det = invmatdet(m)
	if det < 0.000001:
		raise ZeroDivisionError, "minor rank matrix"
	d = 1.0/det
	return	(scale3(d, n[0]),
			 scale3(d, n[1]),
			 scale3(d, n[2]))

def transmat(m):
	# can be used to invert orthogonal rotation matrices
	"(m) - Returns transposed matrix of 'm'"
	return	((m[0][0], m[1][0], m[2][0]),
			 (m[0][1], m[1][1], m[2][1]),
			 (m[0][2], m[1][2], m[2][2]))

def coplanar(verts):
	"checks whether list of 4 vertices is coplanar"
	v1 = verts[0]
	v2 = verts[1]
	a = sub3(v2, v1)
	v1 = verts[1]
	v2 = verts[2]
	b = sub3(v2, v1)
	if dot(cross(a,b), sub3(verts[3] - verts[2])) < 0.0001:
		return 1
	return 0	

################################################################################
# Matrix / Vector highlevel
# (and slower)
# TODO: include better type checks !

class Vector:
	"""Vector class

  This vector class provides vector operations as addition, multiplication, etc.

  Usage::

    v = Vector(x, y, z) 

  where 'x', 'y', 'z' are float values, representing coordinates.
  Note: This datatype emulates a float triple."""

	def __init__(self, x = 0.0, y = 0.0, z = 0.0):
		# don't change these to lists, very ugly referencing details...
		self.v = (x, y, z)  
		# ... can lead to same data being shared by several matrices..
		# (unless you want this to happen)
		self.type = VectorType

	def __neg__(self):
		return self.new(-self.v[0], -self.v[1], -self.v[2])

	def __getitem__(self, i):
		"Tuple emulation"
		return self.v[i]

#	def __setitem__(self, i, arg):
#		self.v[i] = arg

	def new(self, *args):
		return Vector(args[0], args[1], args[2])

	def __cmp__(self, v):
		"Comparison only supports '=='"
		if self[0] == v[0] and self[1] == v[1] and self[1] == v[1]:
			return 0
		return 1

	def __add__(self, v):
		"Addition of 'Vector' objects"
		return self.new(self[0] + v[0],
		              self[1] + v[1],
		              self[2] + v[2])

	def __sub__(self, v):
		"Subtraction of 'Vector' objects"
		return self.new(self[0] - v[0],
		              self[1] - v[1],
		              self[2] - v[2])

	def __rmul__(self, s):	# scaling by s
		return self.new(s * self[0], s * self[1], s * self[2])

	def __mul__(self, t):	# dot product
		"""Left multiplikation supports:

	- scaling with a float value

	- Multiplikation with *Matrix* object"""

		if type(t) == FloatType:
			return self.__rmul__(t)
		elif t.type == MatrixType:
			return Matrix(self[0] * t[0], self[1] * t[1], self[2] * t[2])
		else:	
			return dot(self, t)

	def cross(self, v):
		"(Vector v) - returns the cross product of 'self' with 'v'"
		return  self.new(self[1] * v[2] - self[2] * v[1], 
		                 self[2] * v[0] - self[0] * v[2], 
		                 self[0] * v[1] - self[1] * v[0])
		
	def __repr__(self):
		return "(%.3f, %.3f, %.3f)" % (self.v[0], self.v[1], self.v[2])
		
class Matrix(Vector):
	"""Matrix class

  This class is representing a vector of Vectors.

  Usage::

    M = Matrix(v1, v2, v3) 

  where 'v'n are Vector class instances.
  Note: This datatype emulates a 3x3 float array."""
	
	def __init__(self, v1 = Vector(1.0, 0.0, 0.0), 
	                   v2 = Vector(0.0, 1.0, 0.0), 
	                   v3 = Vector(0.0, 0.0, 1.0)):
		self.v = [v1, v2, v3]
		self.type = MatrixType

	def __setitem__(self, i, arg):
		self.v[i] = arg

	def new(self, *args):
		return Matrix(args[0], args[1], args[2])

	def __repr__(self):
		return "Matrix:\n       %s\n       %s\n       %s\n" % (self.v[0], self.v[1], self.v[2])

	def __mul__(self, m):
		"""Left multiplication supported with:

	- Scalar (float)

	- Matrix

	- Vector: row_vector * matrix; same as self.transposed() * vector
"""
		try:
			if type(m) == FloatType:
				return self.__rmul__(m)
			if m.type == MatrixType:
				M = matxmat(self, m)
				return self.new(Vector(M[0][0], M[0][1], M[0][2]),
								Vector(M[1][0], M[1][1], M[1][2]),
								Vector(M[2][0], M[2][1], M[2][2]))
			if m.type == VectorType:
				v = matxvec(self, m)
				return Vector(v[0], v[1], v[2])
		except:
			raise TypeError, "bad multiplicator type"

	def inverse(self):
		"""returns the matrix inverse"""
		M = invmat(self)
		return self.new(Vector(M[0][0], M[0][1], M[0][2]),
		                Vector(M[1][0], M[1][1], M[1][2]),
		                Vector(M[2][0], M[2][1], M[2][2]))
		
	def transposed(self):
		"returns the transposed matrix"
		M = self
		return self.new(Vector(M[0][0], M[1][0], M[2][0]),
		                Vector(M[1][0], M[1][1], M[2][1]),
		                Vector(M[2][0], M[1][2], M[2][2]))

	def det(self):
		"""returns the determinant"""
		M, det = invmatdet(self)
		return det

	def tr(self):
		"""returns trace (sum of diagonal elements) of matrix"""
		return self.v[0][0] + self.v[1][1] + self.v[2][2]

	def __rmul__(self, m):
		"Right multiplication supported with scalar"
		if type(m) == FloatType:
			return self.new(m * self[0],
			                m * self[1],
			                m * self[2])
		else:
			raise TypeError, "bad multiplicator type"

	def __div__(self, m):
		"""Division supported with:

	- Scalar

	- Matrix: a / b equivalent b.inverse * a
"""
		if type(m) == FloatType:
			m = 1.0 /m
			return m * self
		elif m.type == MatrixType:
			return self.inverse() * m
		else:
			raise TypeError, "bad multiplicator type"

	def __rdiv__(self, m):
		"Right division of matrix equivalent to multiplication with matrix.inverse()"
		return m * self.inverse()

	def asEuler(self):
		"""returns Matrix 'self' as Eulers. Note that this not the only result, due to
the nature of sin() and cos(). The Matrix MUST be a rotation matrix, i.e. orthogonal and
normalized."""
		from math import cos, sin, acos, asin, atan2, atan
		mat = self.v
		sy = mat[0][2]
		# for numerical stability:
		if sy > 1.0:
			if sy > 1.0 + TOLERANCE:
				raise RuntimeError, "FATAL: bad matrix given"
			else:
				sy = 1.0
		phi_y = -asin(sy)

		if abs(sy) > (1.0 - TOLERANCE):
			# phi_x can be arbitrarely chosen, we set it = 0.0
			phi_x = 0.0
			sz = mat[1][0]
			cz = mat[2][0]
			phi_z = atan(sz/cz)
		else:
			cy = cos(phi_y)
			cz = mat[0][0] / cy
			sz = mat[0][1] / cy
			phi_z = atan2(sz, cz)

			sx = mat[1][2] / cy
			cx = mat[2][2] / cy
			phi_x = atan2(sx, cx)
		return phi_x, phi_y, phi_z

Ex = Vector(1.0, 0.0, 0.0)
Ey = Vector(0.0, 1.0, 0.0)
Ez = Vector(0.0, 0.0, 1.0)

One = Matrix(Ex, Ey, Ez)
orig = (0.0, 0.0, 0.0)

def rotmatrix(phi_x, phi_y, phi_z, reverse = 0):
	"""Creates rotation matrix from euler angles. Rotations are applied in order
X, then Y, then Z. If the reverse is desired, you have to transpose the matrix after."""
	from math import sin, cos
	s = sin(phi_z)
	c = cos(phi_z)
	matz = Matrix(Vector(c, s, 0.0), Vector(-s, c, 0.0), Ez)

	s = sin(phi_y)
	c = cos(phi_y)
	maty = Matrix(Vector(c, 0.0, -s), Ey, Vector(s, 0.0, c))

	s = sin(phi_x)
	c = cos(phi_x)
	matx = Matrix(Ex, Vector(0.0, c, s), Vector(0.0, -s, c))

	return matz * maty * matx


def test():
	"The module test"
	print "********************"
	print "VECTOR TEST"
	print "********************"

	a = Vector(1.1, 0.0, 0.0)
	b = Vector(0.0, 2.0, 0.0)

	print "vectors: a = %s, b = %s" % (a, b)
	print "dot:", a * a
	print "scalar:", 4.0 * a
	print "scalar:", a * 4.0
	print "cross:", a.cross(b)
	print "add:", a + b
	print "sub:", a - b
	print "sub:", b - a
	print
	print "********************"
	print "MATRIX TEST"
	print "********************"
	c = a.cross(b)
	m = Matrix(a, b, c)
	v = Vector(1.0, 2.0, 3.0)
	E = One
	print "Original", m
	print "det", m.det()
	print "add", m + m
	print "scalar", 0.5 * m
	print "sub", m - 0.5 * m
	print "vec mul", v * m
	print "mul vec", m * v
	n = m * m 
	print "mul:", n
	print "matrix div (mul inverse):", n / m
	print "scal div (inverse):", 1.0 / m
	print "mat * inverse", m * m.inverse()
	print "mat * inverse (/-notation):", m * (1.0 / m)
	print "div scal", m / 2.0

# matrices with rang < dimension have det = 0.0
	m = Matrix(a, 2.0 * a, c)
	print "minor rang", m
	print "det:", m.det()

if __name__ == '__main__':
	test()

