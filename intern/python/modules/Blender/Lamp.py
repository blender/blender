"""The Blender Lamp module

This module provides control over **Lamp** objects in Blender.

Example::

  from Blender import Lamp
  l = Lamp.New('Spot')
  l.setMode('square', 'shadow')
  ob = Object.New('Lamp')
  ob.link(l)
"""

import _Blender.Lamp as _Lamp
import shadow

_validBufferSizes = [512, 768, 1024, 1536, 2560]	

def _setBufferSize(self, bufsize):
	"""Set the lamp's buffersize. This function makes sure that a valid
bufferSize value is set (unlike setting lamp.bufferSize directly)"""
	if bufsize not in _validBufferSizes:
		print """Buffer size should be one of:
%s
Setting to default 512""" % _validBufferSizes
		bufsize = 512
	self._object.bufferSize = bufsize

class Lamp(shadow.hasIPO, shadow.hasModes):
	"""Wrapper for Blender Lamp DataBlock
	
  Attributes
  
    mode         -- Lamp mode value - see EditButtons. Do not access directly 
                    See setMode()

    type         -- Lamp type value - see EditButtons. No direct access, please.
                    See setType()

    col          -- RGB vector (R, G, B) of lamp colour

    energy       -- Intensity (float)

    dist         -- clipping distance of a spot lamp or decay range

    spotSize     -- float angle (in degrees) of spot cone 
                    (between 0.0 and 180.0)

    spotBlend    -- value defining the blurriness of the spot edge

    haloInt      -- Halo intensity

    clipStart    -- shadow buffer clipping start

    clipStart    -- shadow buffer clipping end

    bias         -- The bias value for the shadowbuffer routine

    softness     -- The filter value for the shadow blurring

    samples      -- Number of samples in shadow calculation - the
                    larger, the better

    bufferSize   -- Size of the shadow buffer which should be one of:
                    [512, 768, 1024, 1536, 2560]	

    haloStep     -- Number of steps in halo calculation - the smaller, the
                    the better (and slower). A value of 0 disables shadow
                    halo calculation
	"""

	_emulation = {'Energ' : "energy",
	              'SpoSi' : "spotSize",
                  'SpoBl' : "SpotBlend",
                  'HaInt' : "haloInt",
                  'Dist'  : "dist",
                  'Quad1' : "quad1",
                  'Quad2' : "quad2",
                 } 

	_setters = {'bufferSize' : _setBufferSize}

	t = _Lamp.Types

	Types = {'Lamp'  : t.LOCAL,
			 'Spot'  : t.SPOT,
			 'Sun'   : t.SUN,
			 'Hemi'  : t.HEMI,
			} 

	t = _Lamp.Modes

	Modes = {'quad'       : t.QUAD,
			 'sphere'     : t.SPHERE,
			 'shadow'     : t.SHAD,
			 'halo'       : t.HALO,
			 'layer'      : t.LAYER,
			 'negative'   : t.NEG,
			 'onlyShadow' : t.ONLYSHADOW,
			 'square'     : t.SQUARE,
			}

	del t

	def __repr__(self):
		return "[Lamp \"%s\"]" % self.name

	def setType(self, name):
		"""Set the Lamp type of Lamp 'self'. 'name' must be a string of:

* 'Lamp': A standard point light source

* 'Spot': A spot light

* 'Sun' : A unidirectional light source, very far away (like a Sun!)

* 'Hemi': A diffuse hemispherical light source (daylight without sun)"""

		try:
			self._object.type = self.Types[name]
		except:
			raise TypeError, "type must be one of %s" % self.Types.keys()

	def getType(self):
		"""Returns the lamp's type as string. See setType()"""
		for k in self.Types.keys():
			if self.Types[k] == self.type:
				return k

	def getMode(self):
		"""Returns the Lamp modes as a list of strings"""
		return shadow._getModeBits(self.Modes, self._object.mode)

	def setMode(self, *args):
		"""Set the Lamp mode of Lamp 'self'. This function takes a variable number
of string arguments of the types listed in self.Modes.

  Example::

    l = Lamp.New()
    l.setMode('quad', 'shadow')
"""
		print args
		self._object.mode = shadow._setModeBits(self.Modes, args)

	def getBufferSize(self):
		return self.bufferSize

def New(type = "Lamp", name = "Lamp"):
	"""Returns a new Lamp datablock of type 'type' and optional name 'name'
"""
	t = Lamp.Types[type]
	rawlamp = _Lamp.New()
	rawlamp.type = t
	rawlamp.name = name
	return Lamp(rawlamp)
	

def get(name = None):
	"""If 'name' given, the Lamp 'name' is returned if existing, 'None' otherwise.
If no name is given, a list of all Lamps is returned"""

	if name:
		return Lamp(_Lamp.get(name))
	else:
		return shadow._List(_Lamp.get(), Lamp)

Types = _Lamp.Types
