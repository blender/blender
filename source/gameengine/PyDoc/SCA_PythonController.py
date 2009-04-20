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
	@ivar state: the controllers state bitmask.
	             This can be used with the GameObject's state to test if the controller is active.
	@type state: integer
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
		
	def getSensors():
		"""
		Gets a list of all sensors attached to this controller.
		
		@rtype: list [L{SCA_ISensor}]
		"""
	def getSensor(name):
		"""
		Gets the named linked sensor.
		
		@type name: string
		@rtype: L{SCA_ISensor}
		"""
	def getActuators():
		"""
		Gets a list of all actuators linked to this controller.
		
		@rtype: list [L{SCA_IActuator}]
		"""
	def getActuator(name):
		"""
		Gets the named linked actuator.
		
		@type name: string
		@rtype: L{SCA_IActuator}
		"""
	def getScript():
		"""
		DEPRECATED: use the script property
		Gets the Python script this controller executes.
		
		@rtype: string
		"""
	def setScript(script):
		"""
		Sets the Python script this controller executes.
		
		@type script: string.
		"""
	def getState():
		"""
		DEPRECATED: use the state property
		Get the controllers state bitmask, this can be used with the GameObject's state to test if the the controller is active.
		This for instance will always be true however you could compare with a previous state to see when the state was activated.
		GameLogic.getCurrentController().getState() & GameLogic.getCurrentController().getOwner().getState()
		
		@rtype: int
		"""

