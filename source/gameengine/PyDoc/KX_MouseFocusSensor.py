# Documentation for KX_MouseFocusSensor
from SCA_MouseSensor import *

class KX_MouseFocusSensor(SCA_MouseSensor):
	"""
	The mouse focus sensor detects when the mouse is over the current game object.
	
	The mouse focus sensor works by transforming the mouse coordinates from 2d device
	space to 3d space then raycasting away from the camera.
	"""
	
	def GetRayTarget():
		"""
		Returns the end point of the sensor ray.
		
		@rtype: list [x, y, z]
		@return: the end point of the sensor ray, in world coordinates.
		"""
	def GetRaySource():
		"""
		Returns the start point of the sensor ray.
		
		@rtype: list [x, y, z]
		@return: the start point of the sensor ray, in world coordinates.
		"""
