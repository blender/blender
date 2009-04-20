# $Id$
# Documentation for KX_NearSensor
from KX_TouchSensor import *

class KX_NearSensor(KX_TouchSensor):
	"""
	A near sensor is a specialised form of touch sensor.
	
	@ivar distance: The near sensor activates when an object is within this distance.
	@type distance: float
	@ivar resetDistance: The near sensor deactivates when the object exceeds this distance.
	@type resetDistance: float
	"""

