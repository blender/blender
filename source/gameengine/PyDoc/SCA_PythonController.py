# Documentation for SCA_PythonController
from SCA_ILogicBrick import *

class SCA_PythonController(SCA_ILogicBrick):
	"""
	A Python controller uses a Python script to activate it's actuators,
	based on it's sensors.
	"""

	def getSensors():
		"""
		Gets a list of all sensors attached to this controller.
		
		@rtype: list [SCA_ISensor]
		"""
	def getSensor(name):
		"""
		Gets the named linked sensor.
		
		@type name: string
		@rtype: SCA_ISensor
		"""
	def getActuators():
		"""
		Gets a list of all actuators linked to this controller.
		
		@rtype: list [SCA_IActuator]
		"""
	def getActuator(name):
		"""
		Gets the named linked actuator.
		
		@type name: string
		@rtype: SCA_IActuator
		"""
	def getScript():
		"""
		Gets the Python script this controller executes.
		
		@rtype: string
		"""
	def setScript(script):
		"""
		Sets the Python script this controller executes.
		
		@type script: string.
		"""

