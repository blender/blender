"""The Blender Material module

  This module provides access to *Material* datablocks

  Example::

    from Blender import Material, NMesh, Object, Scene
    m = Material.New()                         # create free Material datablock
    m.rgbCol = (1.0, 0.0, 0.3)                    # assign RGB values
    mesh = NMesh.GetRaw()                      # get new mesh
    mesh.addMaterial(m)                        # add material to mesh
    object = Object.New('Mesh')                # create new object 
    object.link(mesh)                          # link mesh data to object
    Scene.getCurrent().link(ob)                # link object to current scene
"""

import _Blender.Material as _Material
import shadow
#import Blender.Curve as Curve

# These are getters and setters needed for emulation

def _getRGB(obj):
	return (obj.R, obj.G, obj.B)

def _getSpec(obj):
	return (obj.specR, obj.specG, obj.specB)

def _getMir(obj):
	return (obj.mirR, obj.mirG, obj.mirB)

def _setRGB(obj, rgb):
	obj.R, obj.G, obj.B = rgb

def _setSpec(obj, rgb):
	obj.specR, obj.specG, obj.specB = rgb

def _setMir(obj, rgb):
	obj.mirR, obj.mirG, obj.mirB = rgb



class Material(shadow.hasIPO, shadow.hasModes):
	"""Material DataBlock object

    See example in the Material module documentation on how to create
    an instance of a Material object.

  Attributes

    The following attributes are colour vectors (r, g, b)
	
    rgbCol     -- The color vector (R, G, B).
	              The RGB values can be accessed individually as .R, .G and .B 

    specCol    -- Specularity color vector (specR, specG, specG)

    mirCol     -- Mirror color vector (mirR, mirG, mirB)

    The following are float values:
	
    alpha      -- The transparency

    ref        -- Reflectivity float value

    emit       -- Emit intensity value 

    amb        -- Ambient intensity value 

    spec       -- specularity value 

    specTransp -- Specular transpareny 

    haloSize   -- Halo size

    mode       -- The material mode bit vector - see Material.ModeFlags

    hard       -- The hardness value

""" 

	_emulation = {'Mode'   : "mode",
	              'Ref'    : "ref",
				  'HaSize' : "haloSize",
				  'SpTra'  : "specTransp",
				  'Alpha'  : "alpha",
				  'Spec'   : "spec",
				  'Emit'   : "emit",
				  'Hard'   : "hard",
				  'Amb'    : "amb",
				 } 

	_getters   = {'rgbCol'  : _getRGB,
	              'specCol' : _getSpec,
				  'mirCol'  : _getMir,
                 } 

	_setters   = {'rgbCol'  : _setRGB,
	              'specCol' : _setSpec,
				  'mirCol'  : _setMir,
				  } 

	t = _Material.Modes

	Modes =         {'traceable' : t.TRACEABLE,
					 'shadow'    : t.SHADOW,
					 'shadeless' : t.SHADELESS,
					 'wire'      : t.WIRE,
					 'vcolLight' : t.VCOL_LIGHT,
					 'vcolPaint' : t.VCOL_PAINT,
					 'zTransp'   : t.ZTRANSP,
					 'zInvert'   : t.ZINVERT,
					 'onlyShadow': t.ONLYSHADOW,
					 'star'      : t.STAR,
					 'texFace'   : t.TEXFACE,
					 'noMist'    : t.NOMIST,
					} 

	t = _Material.HaloModes

	HaloModes =     { "rings"   : t.RINGS,
					  "lines"   : t.LINES,
					  "tex"     : t.TEX,
					  "haloPuno": t.PUNO,
					  "shade"   : t.SHADE,
					  "flare"   : t.FLARE,
					}


	del t

	def setMode(self, *args):
		"""Set the mode of 'self'. This function takes a variable number
of string arguments of the types listed in self.Modes.

  Example::

    m = Material.New()
    m.setMode('shadow', 'wire')
"""
		flags = 0
		try:
			for a in args:
				flags |= self.Modes[a]
		except:
			raise TypeError, "mode must be one of" % self.Modes.keys()
		self._object.mode = flags

	def setHaloMode(self, *args):
		"""Sets the material to Halo mode. 
This function takes a variable number of string arguments of the types 
listed in self.HaloModes"""
		flags = _Material.Modes.HALO

		try:
			for a in args:
				flags |= self.HaloModes[a]
		except:
			raise TypeError, "mode must be one of" % self.HaloModes.keys()
		self._object.mode = flags

	
class ModeFlags:
	"""Readonly dictionary

...containing Material mode bitvectors:

|------------------------------------------|
| Name         |  Description              |
|==========================================|
| TRACEABLE    | visible for shadow lamps  |
|------------------------------------------|
| SHADOW       | cast shadow               |
|------------------------------------------|
| SHADELESS    | do not shade              |
|------------------------------------------|
| WIRE         | draw in wireframe         |
|------------------------------------------|
| VCOL_LIGHT   | use vertex colors         |
|              | with lighting             |
|------------------------------------------|
| VCOL_PAINT   | vertex colours            |
|------------------------------------------|
| HALO         | Halo material             |
|------------------------------------------|
| ZTRANSP      | Z transparency            |
|------------------------------------------|
| ZINVERT      | invert Z                  |
|------------------------------------------|
| ONLYSHADOW   | only shadow, but          |
|              | don't render              |
|------------------------------------------|
| STAR         |  ?                        |
|------------------------------------------|
| TEXFACE      | textured faces            |
|------------------------------------------|
| NOMIST       | disable mist              |
|------------------------------------------|

These mode flags directly represent the buttons in the Material parameters
window (EditButtons)

Example::

  # be 'm' a material
  from Blender.Material.Modes import *
  m.mode |= (TRACEABLE + WIRE)    # Set 'wire' and 'traceable' flagsd
  m.mode &= ~SHADELESS            # clear 'shadeless' flag
"""
	
	t = _Material.Modes
	TRACEABLE  = t.TRACEABLE
	SHADOW     = t.SHADOW
	SHADELESS  = t.SHADELESS
	WIRE       = t.WIRE
	VCOL_LIGHT = t.VCOL_LIGHT
	VCOL_PAINT = t.VCOL_PAINT
	HALO       = t.HALO
	ZTRANSP    = t.ZTRANSP
	ZINVERT    = t.ZINVERT
	ONLYSHADOW = t.ONLYSHADOW
	STAR       = t.STAR
	TEXFACE    = t.TEXFACE
	NOMIST     = t.NOMIST
	del t
	
# override:
ModeFlags = _Material.Modes

def get(name = None):
	"""If 'name' given, the Material 'name' is returned if existing, 'None' otherwise.
If no name is given, a list of all Materials is returned"""
	if name:
		return Material(_Material.get(name))
	else:
		return shadow._List(_Material.get(), Material)
	
Get = get  # emulation

def New(name = None):
	"""Creates a new, empty Material and returns it. 

Example::

  from Blender import Material
  mat = Material.New()
"""
	mat = Material(_Material.New())
	if name:
		mat.name = name
	return mat
