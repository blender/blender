# $Id$
# Documentation for SCA_PythonController
from SCA_IController import *

class SCA_PythonController(SCA_IController):
	"""
	A Python controller uses a Python script to activate it's actuators,
	based on it's sensors.
	
	Properties:
	
	@ivar script: the Python script this controller executes
	@type script: string, read-only
	
	@group Deprecated: getScript, setScript
	"""
	def activate(actuator):
		"""
		Activates an actuator attached to this controller.
		@type actuator: actuator or the actuator name as a string
		"""
	def deactivate(actuator):
		"""
		Deactivates an actuator attached to this controller.
		@type actuator: actuator or the actuator name as a string
		"""
	def getScript():
		"""
		DEPRECATED: use the script property
		Gets the Python script this controller executes.
		
		@rtype: string
		"""
	def setScript(script):
		"""
		DEPRECATED: use the script property
		Sets the Python script this controller executes.
		
		@type script: string.
		"""

