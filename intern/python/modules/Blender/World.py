#import _Blender.World as _World

import shadow

def _getAmbCol(obj):
	return obj.ambR, obj.ambG, obj.ambB 

def _setAmbCol(obj, rgb):
	obj.ambR, obj.ambG, obj.ambB = rgb

def _getZenCol(obj):
	return obj.zenR, obj.zenG, obj.zenB 

def _setZenCol(obj, rgb):
	obj.zenR, obj.zenG, obj.zenB = rgb

def _getHorCol(obj):
	return obj.horR, obj.horG, obj.horB 

def _setHorCol(obj, rgb):
	obj.horR, obj.horG, obj.horB = rgb

def _setMist(obj, mist):
	obj.mistStart = mist.start
	obj.mistDepth = mist.depth
	obj.mistHeight = mist.height
	obj.mistType = mist.type

def _getMist(obj):
	mist = Mist()
	mist.start = obj.mistStart
	mist.depth = obj.mistDepth 
	mist.height = obj.mistHeight
	mist.type = obj.mistType 
	return mist

class World(shadow.hasIPO, shadow.hasModes):
	"""Wrapper for Blender World DataBlock
	
  Attributes
  
    horCol       -- horizon colour triple '(r, g, b)' where r, g, b must lie
                in the range of [0.0, 1.0]

    zenCol       -- zenith colour triple

    ambCol       -- ambient colour triple
	
	exposure     -- exposure value

    mist         -- mist structure, see class Mist

    starDensity  -- star density (the higher, the more stars)

    starMinDist  -- the minimum distance to the camera

    starSize     -- size of the stars

    starColNoise -- star colour noise

    gravity      -- The gravity constant (9.81 for earth gravity)
"""

	SkyTypes   = {'blend' : 1,
	              'real'  : 2,
	              'paper' : 4,
	             }

	Modes      = {'mist' : 1,
	              'stars'  : 2,
	             }

	_emulation = {'Expos' : "exposure",
	              'HorR' : "horR",
	              'HorG' : "horG",
	              'HorB' : "horB",
	              'ZenR' : "zenR",
	              'ZenG' : "zenG",
	              'ZenB' : "zenB",
	              'StarDi' : "starDensity",
	              'StarSi' : "starSize",
	              'MisSta' : "mistStart",
	              'MisDi' : "mistDepth",
	              'MisHi' : "mistHeight",
                 } 

	_setters = {'horCol' : _getHorCol,
	            'zenCol' : _getZenCol,
	            'ambCol' : _getAmbCol,
	            'mist' : _getMist,
	           }  			

	_setters = {'horCol' : _setHorCol,
	            'zenCol' : _setZenCol,
	            'ambCol' : _setAmbCol,
	            'mist' : _setMist,
	           }  			

	def getSkyType(self):
		"""Returns a list of the set Sky properties, see setSkyType()"""
		list = []
		for k in self.SkyTypes.keys():
			i = self.SkyTypes[k]
			if self._object.skyType & i:
				list.append(k)
		return list		

	def setSkyType(self, *args):
		"""Set the sky type. This function takes a variable number
of string arguments of ['blend', 'real', 'paper']"""
 		flags = 0
		try:
			for a in args:
				flags |= self.SkyTypes[a]
		except:
			raise TypeError, "mode must be one of" % self.SkyTypes.keys()
		self._object.skyType = flags


class Mist:
	"""Mist structure

  Attributes

    start  -- start of the mist

    depth  -- depth of the "mist wall"

    height -- height of the mist layer
"""

	Types = { 'quadratic' : 0,
	          'linear'    : 1,
	          'sqrt'      : 2,
	        }

	def __init__(self):
		self.start = 0.0
		self.depth = 0.0
		self.height = 0.0
		self.type = 0
		
	def setType(self, name):
		"""Set the Mist type (one of ['quadratic', 'linear', 'sqrt'])"""
		try:
			t = self.Types[name]
		else:
			raise TypeError, "type must be one of %s" % self.Types.keys()
		self.type = t

	def getType(self):
		"""Returns the Mist type as string. See setType()"""
		for k in self.Types.keys():
			if self.Types[k] == self.type:
				return k


