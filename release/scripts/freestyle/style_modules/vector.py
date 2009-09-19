# This module defines 3d geometrical vectors with the standard
# operations on them.
#
# Written by: Konrad Hinsen 
# Last revision: 1996-1-26
#

"""This module defines three-dimensional geometrical vectors. Vectors support
the usual mathematical operations (v1, v2: vectors, s: scalar): 
  v1+v2		  addition
  v1-v2		  subtraction
  v1*v2		  scalar product
  s*v1		  multiplication with a scalar
  v1/s		  division by a scalar
  v1.cross(v2)	  cross product
  v1.length()	  length
  v1.normal()	  normal vector in direction of v1
  v1.angle(v2)	  angle between two vectors
  v1.x(), v1[0]	  first element
  v1.y(), v1[1]	  second element
  v1.z(), v1[2]	  third element

The module offers the following items for export:
  Vec3D(x,y,z)	  the constructor for vectors
  isVector(x)	  a type check function
  ex, ey, ez	  unit vectors along the x-, y-, and z-axes (predefined constants)

Note: vector elements can be any kind of numbers on which the operations
addition, subtraction, multiplication, division, comparison, sqrt, and acos
are defined. Integer elements are treated as floating point elements.
"""

import math, types

class Vec3:

    isVec3 = 1

    def __init__(self, x=0., y=0., z=0.):
	self.data = [x,y,z]

    def __repr__(self):
	return 'Vec3(%s,%s,%s)' % (`self.data[0]`,\
				     `self.data[1]`,`self.data[2]`)

    def __str__(self):
	return `self.data`

    def __add__(self, other):
	return Vec3(self.data[0]+other.data[0],\
		      self.data[1]+other.data[1],self.data[2]+other.data[2])
    __radd__ = __add__

    def __neg__(self):
	return Vec3(-self.data[0], -self.data[1], -self.data[2])

    def __sub__(self, other):
	return Vec3(self.data[0]-other.data[0],\
		      self.data[1]-other.data[1],self.data[2]-other.data[2])

    def __rsub__(self, other):
	return Vec3(other.data[0]-self.data[0],\
		      other.data[1]-self.data[1],other.data[2]-self.data[2])

    def __mul__(self, other):
	if isVec3(other):
	    return reduce(lambda a,b: a+b,
			  map(lambda a,b: a*b, self.data, other.data))
	else:
	    return Vec3(self.data[0]*other, self.data[1]*other,
			  self.data[2]*other)

    def __rmul__(self, other):
	if isVec3(other):
	    return reduce(lambda a,b: a+b,
			  map(lambda a,b: a*b, self.data, other.data))
	else:
	    return Vec3(other*self.data[0], other*self.data[1],
			  other*self.data[2])

    def __div__(self, other):
	if isVec3(other):
	    raise TypeError, "Can't divide by a vector"
	else:
	    return Vec3(_div(self.data[0],other), _div(self.data[1],other),
			  _div(self.data[2],other))
	    
    def __rdiv__(self, other):
	raise TypeError, "Can't divide by a vector"

    def __cmp__(self, other):
	return cmp(self.data[0],other.data[0]) \
	       or cmp(self.data[1],other.data[1]) \
	       or cmp(self.data[2],other.data[2])

    def __getitem__(self, index):
	return self.data[index]

    def x(self):
	return self.data[0]
    def y(self):
	return self.data[1]
    def z(self):
	return self.data[2]

    def length(self):
	return math.sqrt(self*self)

    def normal(self):
	len = self.length()
	if len == 0:
	    raise ZeroDivisionError, "Can't normalize a zero-length vector"
	return self/len

    def cross(self, other):
	if not isVec3(other):
	    raise TypeError, "Cross product with non-vector"
	return Vec3(self.data[1]*other.data[2]-self.data[2]*other.data[1],
		      self.data[2]*other.data[0]-self.data[0]*other.data[2],
		      self.data[0]*other.data[1]-self.data[1]*other.data[0])

    def angle(self, other):
	if not isVec3(other):
	    raise TypeError, "Angle between vector and non-vector"
	cosa = (self*other)/(self.length()*other.length())
	cosa = max(-1.,min(1.,cosa))
	return math.acos(cosa)


class Vec2:

    isVec2 = 1

    def __init__(self, x=0., y=0.):
	self.data = [x,y]

    def __repr__(self):
	return 'Vec2(%s,%s,%s)' % (`self.data[0]`,\
				     `self.data[1]`)

    def __str__(self):
	return `self.data`

    def __add__(self, other):
	return Vec2(self.data[0]+other.data[0],\
		      self.data[1]+other.data[1])
    __radd__ = __add__

    def __neg__(self):
	return Vec2(-self.data[0], -self.data[1])

    def __sub__(self, other):
	return Vec2(self.data[0]-other.data[0],\
		      self.data[1]-other.data[1])

    def __rsub__(self, other):
	return Vec2(other.data[0]-self.data[0],\
		      other.data[1]-self.data[1])

    def __mul__(self, other):
	if isVec2(other):
	    return reduce(lambda a,b: a+b,
			  map(lambda a,b: a*b, self.data, other.data))
	else:
	    return Vec2(self.data[0]*other, self.data[1]*other)

    def __rmul__(self, other):
	if isVec2(other):
	    return reduce(lambda a,b: a+b,
			  map(lambda a,b: a*b, self.data, other.data))
	else:
	    return Vec2(other*self.data[0], other*self.data[1])

    def __div__(self, other):
	if isVec2(other):
	    raise TypeError, "Can't divide by a vector"
	else:
	    return Vec2(_div(self.data[0],other), _div(self.data[1],other))
	    
    def __rdiv__(self, other):
	raise TypeError, "Can't divide by a vector"

    def __cmp__(self, other):
	return cmp(self.data[0],other.data[0]) \
	       or cmp(self.data[1],other.data[1])

    def __getitem__(self, index):
	return self.data[index]

    def x(self):
	return self.data[0]
    def y(self):
	return self.data[1]

    def length(self):
	return math.sqrt(self*self)

    def normal(self):
	len = self.length()
	if len == 0:
	    raise ZeroDivisionError, "Can't normalize a zero-length vector"
	return self/len

    #def cross(self, other):
#	if not isVec2(other):
#	    raise TypeError, "Cross product with non-vector"
#	return Vec2(self.data[1]*other.data[2]-self.data[2]*other.data[1],
#		      self.data[2]*other.data[0]-self.data[0]*other.data[2],
#		      self.data[0]*other.data[1]-self.data[1]*other.data[0])

    def angle(self, other):
	if not isVec2(other):
	    raise TypeError, "Angle between vector and non-vector"
	cosa = (self*other)/(self.length()*other.length())
	cosa = max(-1.,min(1.,cosa))
	return math.acos(cosa)



# Type check

def isVec3(x):
    return hasattr(x,'isVec3')

def isVec2(x):
    return hasattr(x,'isVec2')

# "Correct" division for arbitrary number types

def _div(a,b):
    if type(a) == types.IntType and type(b) == types.IntType:
	return float(a)/float(b)
    else:
	return a/b


# Some useful constants

ex = Vec3(1.,0.,0.)
ey = Vec3(0.,1.,0.)
ez = Vec3(0.,0.,1.)
