"""The Blender Ipo module

This module provides access to **Ipo** objects in Blender.

An Ipo object is a datablock of IpoCurves which control properties of
an object in time.

Note that IpoCurves assigned to rotation values (which must be specified
in radians) appear scaled in the IpoWindow (which is in fact true, due
to the fact that conversion to an internal unit of 10.0 angles happens).

Example::
  
  from Blender import Ipo, Object

  ipo = Ipo.New('Object', 'ObIpo')                   # Create object ipo with name 'ObIpo'
  curve = ipo.addCurve('LocY')                       # add IpoCurve for LocY
  curve.setInterpolation('Bezier')                   # set interpolation type
  curve.setExtrapolation('CyclicLinear')             # set extrapolation type

  curve.addBezier((0.0, 0.0))                        # add automatic handle bezier point
  curve.addBezier((20.0, 5.0), 'Free', (10.0, 4.0))  # specify left handle, right auto handle
  curve.addBezier((30.0, 1.0), 'Vect')               # automatic split handle
  curve.addBezier((100.0, 1.0))                      # auto handle

  curve.update()                                     # recalculate curve handles

  curve.eval(35.0)                                   # evaluate curve at 35.0

  ob = Object.get('Plane')
  ob.setIpo(ipo)                                     # assign ipo to object
"""

import _Blender.Ipo as _Ipo

import shadow

_RotIpoCurves = ["RotX", "RotY", "RotZ", "dRotX", "dRotY", "dRotZ"]

_radian_factor = 5.72957814 # 18.0 / 3.14159255

def _convertBPoint(b):
	f = _radian_factor
	newb = BezierPoint() 
	p = b.pt
	q = newb.pt
	q[0], q[1] = (p[0], f * p[1])
	p = b.h1
	q = newb.h1
	q[0], q[1] = (p[0], f * p[1])
	p = b.h2
	q = newb.h2
	q[0], q[1] = (p[0], f * p[1])
	return newb


class IpoBlock(shadow.shadowEx):
	"""Wrapper for Blender Ipo DataBlock

  Attributes
  
    curves -- list of owned IpoCurves
"""
	def get(self, channel = None):
		"""Returns curve with channel identifier 'channel', which is one of the properties
listed in the Ipo Window, 'None' if not found.
If 'channel' is not specified, all curves are returned in a list"""
		if channel:
			for c in self._object.curves:
				if c.name == channel:
					return IpoCurve(c)
			return None
		else:
			return map(lambda x: IpoCurve(x), self._object.curves)

	def __getitem__(self, k):
		"""Emulates dictionary syntax, e.g. ipocurve = ipo['LocX']"""
		curve = self.get(k)
		if not curve:
			raise KeyError, "Ipo does not have a curve for channel %s" % k
		return curve		

	def __setitem__(self, k, val):
		"""Emulates dictionary syntax, e.g. ipo['LocX'] = ipocurve"""
		c = self.addCurve(k, val)
		
	has_key = get # dict emulation

	items = get   # dict emulation

	def keys(self):
		return map(lambda x: x.name, self.get())

	def addCurve(self, channel, curve = None):
		"""Adds a curve of channel type 'channel' to the Ipo Block. 'channel' must be one of
the object properties listed in the Ipo Window. If 'curve' is not specified,
an empty curve is created, otherwise, the existing IpoCurve 'curve' is copied and
added to the IpoBlock 'self'.
In any case, the added curve is returned.
""" 		
		if curve:
			if curve.__class__.__name__ != "IpoCurve":
				raise TypeError, "IpoCurve expected"
			c = self._object.addCurve(channel, curve._object)

			### RotIpo conversion hack
			if channel in _RotIpoCurves:
				print "addCurve, converting", curve.name
				c.points = map(_convertBPoint, curve.bezierPoints)
			else:
				c.points = curve.bezierPoints
		else:	
			c = self._object.addCurve(channel)
		return IpoCurve(c)

	_getters = { 'curves' : get }

class BezierPoint:
	"""BezierPoint object

  Attributes
  
    pt   -- Coordinates of the Bezier point

    h1   -- Left handle coordinates

    h2   -- Right handle coordinates

    h1t  -- Left handle type (see IpoCurve.addBezier(...) )

    h2t  -- Right handle type
"""

BezierPoint = _Ipo.BezTriple # override

class IpoCurve(shadow.shadowEx):
	"""Wrapper for Blender IpoCurve

  Attributes

    bezierPoints -- A list of BezierPoints (see class BezierPoint),
    defining the curve shape
"""

	InterpolationTypes = _Ipo.InterpolationTypes
	ExtrapolationTypes = _Ipo.ExtrapolationTypes

	def __init__(self, object):
		self._object = object
		self.__dict__['bezierPoints'] = self._object.points

	def __getitem__(self, k):
		"""Emulate a sequence of BezierPoints"""
		print k, type(k)
		return self.bezierPoints[k]

	def __repr__(self):
		return "[IpoCurve %s]" % self.name

	def __len__(self):
		return len(self.bezierPoints)

	def eval(self, time):
		"""Returns float value of curve 'self' evaluated at time 'time' which
must be a float."""
		return self._object.eval(time)

	def addBezier(self, p, leftType = 'Auto', left = None, rightType = None, right = None):
		"""Adds a Bezier triple to the IpoCurve.

The following values are float tuples (x,y), denoting position of a control vertex:

p     -- The position of the Bezier point

left  -- The position of the leftmost handle

right -- The position of the rightmost handle

'leftType', 'rightType' must be one of:

"Auto"  --  automatic handle calculation. In this case, 'left' and 'right' don't need to be specified

"Vect"  --  automatic split handle calculation. 'left' and 'right' are disregarded.

"Align" --  Handles are aligned automatically. In this case, 'right' does not need to be specified.

"Free"  --  Handles can be set freely - this requires both arguments 'left' and 'right'.

"""

		b = _Ipo.BezTriple()
		b.pt[0], b.pt[1] = (p[0], p[1])
		b.h1t = leftType

		if rightType:
			b.h2t = rightType
		else:
			b.h2t = leftType

		if left:
			b.h1[0], b.h1[1] = (left[0], left[1])

		if right:	
			b.h2[0], b.h2[1] = (right[0], right[1])

		self.__dict__['bezierPoints'].append(b)
		return b

	def update(self, noconvert = 0):
		# This is an ugly fix for the 'broken' storage of Rotation
		# ipo values. The angles are stored in units of 10.0 degrees,
		# which is totally inconsistent with anything I know :-)
		# We can't (at the moment) change the internals, so we
		# apply a conversion kludge..
		if self._object.name in _RotIpoCurves and not noconvert:
			points = map(_convertBPoint, self.bezierPoints)
		else:
			points = self.bezierPoints
		self._object.points = points
		self._object.update()

	def getInterpolationType(self, ipotype):
		"Returns the Interpolation type - see also IpoCurve.InterpolationTypes"
		return self._object.getInterpolationType()
		
	def setInterpolationType(self, ipotype):
		"""Sets the interpolation type which must be one of IpoCurve.InterpolationTypes"""
		try:
			self._object.setInterpolationType(ipotype)
		except:
			raise TypeError, "must be one of %s" % self.InterpolationTypes.keys()

	def getExtrapolationType(self, ipotype):
		"Returns the Extrapolation type - see also IpoCurve.ExtrapolationTypes"
		return self._object.getExtrapolationType()

	def setExtrapolationType(self, ipotype):
		"""Sets the interpolation type which must be one of IpoCurve.ExtrapolationTypes"""
		try:
			self._object.setInterpolationType(ipotype)
		except:
			raise TypeError, "must be one of %s" % self.ExtrapolationTypes.keys()
	

def New(blocktype, name = None):
	"""Returns a new IPO block of type 'blocktype' which must be one of:
["Object", "Camera", "World", "Material"]
"""
	if name:
		i = _Ipo.New(blocktype, name)
	else:
		i = _Ipo.New(blocktype)
	return IpoBlock(i)

def Eval(ipocurve, time):  # emulation code
	"""This function is just there for compatibility. 
Use IpoCurve.eval(time) instead"""
	return ipocurve.eval(time)

def Recalc(ipocurve):  # emulation code
	"""This function is just there for compatibility. Note that Ipos
assigned to rotation values will *not* get converted to the proper
unit of radians.
In the new style API, use IpoCurve.update() instead"""
	return ipocurve.update(1)

def get(name = None):
	"""If 'name' given, the Ipo 'name' is returned if existing, 'None' otherwise.
If no name is given, a list of all Ipos is returned"""
	if name:
		ipo = _Ipo.get(name)	
		if ipo:
			return IpoBlock(ipo)
		else:
			return None
	else:
		return shadow._List(_Ipo.get(), IpoBlock)

Get = get # emulation
