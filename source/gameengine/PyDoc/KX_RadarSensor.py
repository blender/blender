# $Id$
# Documentation for KX_RadarSensor
from KX_NearSensor import *

class KX_RadarSensor(KX_NearSensor):
	"""
	Radar sensor is a near sensor with a conical sensor object.
	"""
	
	def getConeOrigin():
		"""
		Returns the origin of the cone with which to test. The origin
		is in the middle of the cone.
		
		@rtype: list [x, y, z]
		"""

	def getConeTarget():
		"""
		Returns the center of the bottom face of the cone with which to test.
		
		@rtype: list [x, y, z]
		"""
	
	def getConeHeight():
		"""
		Returns the height of the cone with which to test.
		
		@rtype: float
		"""

