"""Quaternion module

	This module provides conversion routines between Matrices, Quaternions (rotations around
	an axis) and Eulers.
	
	(c) 2000, onk@section5.de	"""

# NON PUBLIC XXX

from math import sin, cos, acos
from util import vect
reload(vect)

Vector = vect.Vector

Matrix = vect.Matrix

class Quat:
	"""Simple Quaternion class

Usually, you create a quaternion from a rotation axis (x, y, z) and a given 
angle 'theta', defining the right hand rotation:

   q = fromRotAxis((x, y, z), theta)

This class supports multiplication, providing an efficient way to
chain rotations"""

	def __init__(self, w = 1.0, x = 0.0, y = 0.0, z = 0.0):
		self.v = (w, x, y, z)

	def asRotAxis(self):
		"""returns rotation axis (x, y, z) and angle phi (right hand rotation)"""
		phi2 = acos(self.v[0])
		if phi2 == 0.0:
			return Vector(0.0, 0.0, 1.0), 0.0
		else:
			s = 1 / (sin(phi2))
		
		v = Vector(s * self.v[1], s * self.v[2], s * self.v[3])
		return v, 2.0 * phi2

	def __mul__(self, other):
		w1, x1, y1, z1 = self.v
		w2, x2, y2, z2 = other.v

		w = w1*w2 - x1*x2 - y1*y2 - z1*z2
		x = w1*x2 + x1*w2 + y1*z2 - z1*y2
		y = w1*y2 - x1*z2 + y1*w2 + z1*x2 
		z = w1*z2 + x1*y2 - y1*x2 + z1*w2 
		return Quat(w, x, y, z)

	def asMatrix(self):
		w, x, y, z = self.v

		v1 = Vector(1.0 - 2.0 * (y*y + z*z), 2.0 * (x*y + w*z), 2.0 * (x*z - w*y))
		v2 = Vector(2.0 * (x*y - w*z), 1.0 - 2.0 * (x*x + z*z), 2.0 * (y*z + w*x))
		v3 = Vector(2.0 * (x*z + w*y), 2.0 * (y*z - w*x), 1.0 - 2.0 * (x*x + y*y))

		return Matrix(v1, v2, v3)

#	def asEuler1(self, transp = 0):
#		m = self.asMatrix()
#		if transp:
#			m = m.transposed()
#		return m.asEuler()

	def asEuler(self, transp = 0):
		from math import atan, asin, atan2
		w, x, y, z = self.v
		x2 = x*x
		z2 = z*z
		tmp = x2 - z2
		r = (w*w + tmp - y*y )
		phi_z = atan2(2.0 * (x * y + w * z) , r)
		phi_y = asin(2.0 * (w * y - x * z))
		phi_x = atan2(2.0 * (w * x + y * z) , (r - 2.0*tmp))

		return phi_x, phi_y, phi_z

def fromRotAxis(axis, phi):
	"""computes quaternion from (axis, phi)"""
	phi2 = 0.5 * phi
	s = sin(phi2)
	return Quat(cos(phi2), axis[0] * s, axis[1] * s, axis[2] * s)

#def fromEuler1(eul):
	#qx = fromRotAxis((1.0, 0.0, 0.0), eul[0])
	#qy = fromRotAxis((0.0, 1.0, 0.0), eul[1])
	#qz = fromRotAxis((0.0, 0.0, 1.0), eul[2])
	#return qz * qy * qx

def fromEuler(eul):
	from math import sin, cos
	e = eul[0] / 2.0
	cx = cos(e)
	sx = sin(e)
	e = eul[1] / 2.0
	cy = cos(e)
	sy = sin(e)
	e = eul[2] / 2.0
	cz = cos(e)
	sz = sin(e)

	w = cx * cy * cz - sx * sy * sz
	x = sx * cy * cz - cx * sy * sz
	y = cx * sy * cz + sx * cy * sz
	z = cx * cy * sz + sx * sy * cz
	return Quat(w, x, y, z)
