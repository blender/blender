# $Id$
# Documentation for KX_MouseFocusSensor
from SCA_MouseSensor import *

class KX_MouseFocusSensor(SCA_MouseSensor):
	"""
	The mouse focus sensor detects when the mouse is over the current game object.
	
	The mouse focus sensor works by transforming the mouse coordinates from 2d device
	space to 3d space then raycasting away from the camera.
	"""
	
	def getHitNormal():
		"""
		Returns the normal (in worldcoordinates) at the point of collision where the object was hit by this ray.
		
		@rtype: list [x, y, z]
		@return: the ray collision normal.
		"""
	def getHitObject():
		"""
		Returns the object that was hit by this ray or None.
		
		@rtype: L{KX_GameObject} or None
		@return: the collision object.
		"""
	def getHitPosition():
		"""
		Returns the position (in worldcoordinates) at the point of collision where the object was hit by this ray.
		
		@rtype: list [x, y, z]
		@return: the ray collision position.
		"""
	def getRayDirection():
		"""
		Returns the normalized direction (in worldcoordinates) of the ray cast by the mouse.
		
		@rtype: list [x, y, z]
		@return: the ray direction.
		"""
	def getRaySource():
		"""
		Returns the position (in worldcoordinates) the ray was cast from by the mouse.
		
		@rtype: list [x, y, z]
		@return: the ray source.
		"""
	def getRayTarget():
		"""
		Returns the target of the ray (in worldcoordinates) that seeks the focus object.
		
		@rtype: list [x, y, z]
		@return: the ray target.
		"""