"""The Blender Scene module

  This module provides *Scene* manipulation routines.

  Example::

    from Blender import Scene

    curscene = Scene.getCurrent()
    ob = curscene.getChildren()[0]       # first object
    newscene = Scene.New('testscene')
    cam = curscene.getCurrentCamera()    # get current camera object
    newscene.link(ob)                    # link 'ob' to Scene
    newscene.link(cam)
    newscene.makeCurrent()               # make current Scene
"""  
import _Blender.Scene as _Scene

from Object import Object
import shadow

class Scene(shadow.shadowEx):
	"""Wrapper for Scene DataBlock 
"""
	def link(self, object):
		"""Links Object 'object' into Scene 'self'."""
		# This is a strange workaround; Python does not release 
		# 'self' (and thus self._object) when an exception in the C API occurs. 
		# Therefore, we catch that exception and do it ourselves..
		# Maybe Python 2.2 is able to resolve this reference dependency ?
		try:
			return self._object.link(object._object)
		except:
			del self._object
			raise 

	def unlink(self, object):
		"""Unlinks (deletes) Object 'object' from Scene."""
		ret = self._object.unlink(object._object)
		return ret

	def copy(self, duplicate_objects = 1):	
		"""Returns a copy of itself.

The optional argument defines, how the Scene's children objects are
duplicated::

  0: Link Objects
  1: Link Object data
  2: Full Copy"""
		return Scene(self._object.copy(duplicate_objects))

	def update(self):
		"""Updates scene 'self'.
  This function explicitely resorts the base list of a newly created object
  hierarchy."""
		return self._object.update()

	def makeCurrent(self):
		"""Makes 'self' the current Scene"""
		return self._object.makeCurrent()

	def frameSettings(self, start = None, end = None, current = None):
		"""Sets or retrieves the Scene's frame settings.
If the frame arguments are specified, they are set.
A tuple (start, end, current) is returned in any case."""
		if start and end and current:
			return self._object.frameSettings(start, end, current)
		else:
			return self._object.frameSettings()

	def currentFrame(self, frame = None):
		"""If 'frame' is given, the current frame is set and returned in any case"""
		if frame:
			return self._object.frameSettings(-1, -1, frame)
		return self._object.frameSettings()[2]

	def startFrame(self, frame = None):
		"""If 'frame' is given, the start frame is set and returned in any case"""
		if frame:
			return self._object.frameSettings(frame, -1, -1)
		return self._object.frameSettings()[0]

	def endFrame(self, frame = None):
		"""If 'frame' is given, the end frame is set and returned in any case"""
		if frame:
			return self._object.frameSettings(-1, frame, -1)
		return self._object.frameSettings()[1]

	def getChildren(self):
		"""Returns a list of the Scene's children Objects"""
		return shadow._List(self._object.getChildren(), Object)

	def getCurrentCamera(self):
		"""Returns current active camera Object"""
		cam = self._object.getCurrentCamera()
		if cam:
			return Object(cam)

	def setCurrentCamera(self, object):
		"""Sets the current active camera Object 'object'"""
		return self._object.setCurrentCamera(object._object)

	def getRenderdir(self):
		"""Returns directory where rendered images are saved to"""
		return self._object.getRenderdir(self._object)

	def getBackbufdir(self):	
		"""Returns the Backbuffer images location"""
		return self._object.getBackbufdir(self._object)

# Module methods

def New(name = 'Scene'):
	"""Creates and returns new Scene with (optionally given) name"""
	return Scene(_Scene.New(name))

def get(name = None):
	"""Returns a Scene object with name 'name' if given, None if not existing,
or a list of all Scenes otherwise."""
	if name:
		ob = _Scene.get(name)
		if ob:
			return Scene(ob)
		else:
			return None
	else:
		return shadow._List(_Scene.get(), Scene)

Get = get  # emulation

def getCurrent():
	"""Returns the currently active Scene"""
	sc = Scene(_Scene.getCurrent())
	return sc

def unlink(scene):
	"""Removes the Scene 'scene' from Blender"""
	if scene._object.name == _Scene.getCurrent().name:
		raise SystemError, "current Scene can not be removed!"
	for ob in scene.getChildren():
		scene.unlink(ob)
	return _Scene.unlink(scene._object)
