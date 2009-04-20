# $Id$
# Documentation for KX_RaySensor
from SCA_ISensor import *

class KX_RaySensor(SCA_ISensor):
	"""
	A ray sensor detects the first object in a given direction.
	
	@ivar property: The property the ray is looking for.
	@type property: string
	@ivar range: The distance of the ray.
	@type range: float
	@ivar useMaterial: Whether or not to look for a material (false = property)
	@type useMaterial: boolean
	@ivar useXRay: Whether or not to use XRay.
	@type useXRay: boolean
	@ivar hitObject: The game object that was hit by the ray. (Read-only)
	@type hitObject: KX_GameObject
	@ivar hitPosition: The position (in worldcoordinates) where the object was hit by the ray. (Read-only)
	@type hitPosition: list [x, y, z]
	@ivar hitNormal: The normal (in worldcoordinates) of the object at the location where the object was hit by the ray. (Read-only)
	@type hitNormal: list [x, y, z]
	@ivar rayDirection: The direction from the ray (in worldcoordinates). (Read-only)
	@type rayDirection: list [x, y, z]
	@ivar axis: The axis the ray is pointing on.
	@type axis: int from 0 to 5
		KX_RAY_AXIS_POS_X, KX_RAY_AXIS_POS_Y, KX_RAY_AXIS_POS_Z,
		KX_RAY_AXIS_NEG_X, KX_RAY_AXIS_NEG_Y, KX_RAY_AXIS_NEG_Z
	"""
	
	def getHitObject():
		"""
		DEPRECATED: Use the hitObject property instead.
		Returns the game object that was hit by this ray.
		
		@rtype: KX_GameObject
		"""
	def getHitPosition():
		"""
		DEPRECATED: Use the hitPosition property instead.
		Returns the position (in worldcoordinates) where the object was hit by this ray.
		
		@rtype: list [x, y, z]
		"""
	def getHitNormal():
		"""
		DEPRECATED: Use the hitNormal property instead.
		Returns the normal (in worldcoordinates) of the object at the location where the object was hit by this ray.
		
		@rtype: list [nx, ny, nz]
		"""
	def getRayDirection():
		"""
		DEPRECATED: Use the rayDirection property instead.
		Returns the direction from the ray (in worldcoordinates)
		
		@rtype: list [dx, dy, dz]
		"""
