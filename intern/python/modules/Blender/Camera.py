"""The Blender Camera module

This module provides access to **Camera** objects in Blender

Example::

  from Blender import Camera, Object, Scene
  c = Camera.New('ortho')      # create new ortho camera data
  c.lens = 35.0                # set lens value
  cur = Scene.getCurrent()     # get current Scene
  ob = Object.New('Camera')    # make camera object
  ob.link(c)                   # link camera data with this object
  cur.link(ob)                 # link object into scene
  cur.setCurrentCamera(ob)     # make this camera the active
"""

import shadow
#import _Blender.Camera as _Camera


class Camera(shadow.hasIPO):
	"""Wrapper for Camera DataBlock

  Attributes

    lens      -- The lens value

    clipStart -- The clipping start of the view frustum

    clipEnd   -- The end clipping plane of the view frustum

    type      -- The camera type:
                 0: perspective camera,
			     1: orthogonal camera     - (see Types)
  
    mode      -- Drawing mode; see Modes
"""

	_emulation = {'Lens'   : "lens",
	              'ClSta'  : "clipStart",
				  'ClEnd'  : "clipEnd",
				 } 

	Types = {'persp' : 0,
			 'ortho' : 1,
			} 

	Modes = {'showLimits' : 1,
			 'showMist'   : 2,
			} 

	def __init__(self, object):
		self._object = object

	def getType(self):
		"""Returns camera type: "ortho" or "persp" """
		if self.type == self.Types['ortho']:
			return 'ortho'
		else:
			return 'persp'

	def setType(self, type):
		"""Sets Camera type to 'type' which must be one of ["persp", "ortho"]"""
		self._object.type = self.Types[type]

	def setMode(self, *modes):
		"""Sets Camera modes *the nice way*, instead of direct access
of the 'mode' member. 
This function takes a variable number of string arguments of the types 
listed in self.Modes.


Example::

  c = Camera.New()
  c.setMode('showMist', 'showLimits')
"""  
		flags = 0
		try:
			for a in modes:
				flags |= self.Modes[a]
		except:
			raise TypeError, "mode must be one of %s" % self.Modes.keys()
		self.mode = flags

	def __repr__(self):
		return "[Camera \"%s\"]" % self.name

def New(type = 'persp'):
	"""Creates new camera Object and returns it. 'type', if specified,
must be one of Types"""
	cam = Camera(_Camera.New())
	cam.setType(type)
	return cam

def get(name = None):
	"""Returns the Camera with name 'name', if given. Otherwise, a list
of all Cameras is returned"""
	if name:
		return Camera(_Camera.get(name))
	else:
		return shadow._List(_Camera.get(), Camera)
	
Get = get  # emulation

  
