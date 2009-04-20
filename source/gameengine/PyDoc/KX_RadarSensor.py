# $Id$
# Documentation for KX_RadarSensor
from KX_NearSensor import *

class KX_RadarSensor(KX_NearSensor):
	"""
	Radar sensor is a near sensor with a conical sensor object.
	
	@ivar coneOrigin: The origin of the cone with which to test. The origin
						is in the middle of the cone.
						(Read only)
	@type coneOrigin: list of floats [x, y, z]
	@ivar coneTarget: The center of the bottom face of the cone with which to test.
						(Read only)
	@type coneTarget: list of floats [x, y, z]
	@ivar distance: The height of the cone with which to test.
	@type distance: float
	@ivar angle: The angle of the cone (in degrees) with which to test.
	@type angle: float from 0 to 360
	@ivar axis: The axis on which the radar cone is cast
	@type axis: int from 0 to 5
		KX_RADAR_AXIS_POS_X, KX_RADAR_AXIS_POS_Y, KX_RADAR_AXIS_POS_Z,
		KX_RADAR_AXIS_NEG_X, KX_RADAR_AXIS_NEG_Y, KX_RADAR_AXIS_NEG_Z
	"""
	
	
	#--The following methods are deprecated, please use properties instead.
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

