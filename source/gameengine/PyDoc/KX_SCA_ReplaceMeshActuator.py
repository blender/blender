# $Id$
# Documentation for KX_SCA_ReplaceMeshActuator
from SCA_IActuator import *

class KX_SCA_ReplaceMeshActuator(SCA_IActuator):
	"""
	Edit Object actuator, in Replace Mesh mode.
	
	@warning: Replace mesh actuators will be ignored if at game start, the
		named mesh doesn't exist.
		
		This will generate a warning in the console:
		
		C{ERROR: GameObject I{OBName} ReplaceMeshActuator I{ActuatorName} without object}

	"""
	def setMesh(name):
		"""
		Sets the name of the mesh that will replace the current one.
		
		@type name: string
		"""

