# $Id$
# Documentation for KX_RaySensor
from SCA_ISensor import *

class KX_RaySensor(SCA_ISensor):
	"""
	A ray sensor detects the first object in a given direction.
	"""
	
	def getHitObject():
		"""
		Returns the game object that was hit by this ray.
		
		@rtype: KX_GameObject
		"""
	def getHitPosition():
		"""
		Returns the position (in worldcoordinates) where the object was hit by this ray.
		
		@rtype: list [x, y, z]
		"""
	def getHitNormal():
		"""
		Returns the normal (in worldcoordinates) of the object at the location where the object was hit by this ray.
		
		@rtype: list [nx, ny, nz]
		"""
	def getRayDirection():
		"""
		Returns the direction from the ray (in worldcoordinates)
		
		@rtype: list [dx, dy, dz]
		"""
