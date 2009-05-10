# $Id$
# Documentation for KX_CameraActuator
from SCA_ILogicBrick import *

class SCA_IController(SCA_ILogicBrick):
	"""
	Base class for all controller logic bricks.
	
	@ivar state: the controllers state bitmask.
	             This can be used with the GameObject's state to test if the controller is active.
	@type state: int bitmask
	@ivar sensors: a list of sensors linked to this controller
					- note: the sensors are not necessarily owned by the same object.
					- note: when objects are instanced in dupligroups links may be lost from objects outside the dupligroup.
	@type sensors: sequence supporting index/string lookups and iteration.
	@ivar actuators: a list of actuators linked to this controller.
						- note: the sensors are not necessarily owned by the same object.
						- note: when objects are instanced in dupligroups links may be lost from objects outside the dupligroup.
	@type actuators: sequence supporting index/string lookups and iteration.
	
	@group Deprecated: getState, getSensors, getActuators, getSensor, getActuator
	"""

	def getState():
		"""
		DEPRECATED: use the state property
		Get the controllers state bitmask, this can be used with the GameObject's state to test if the the controller is active.
		This for instance will always be true however you could compare with a previous state to see when the state was activated.
		GameLogic.getCurrentController().getState() & GameLogic.getCurrentController().getOwner().getState()
		
		@rtype: int
		"""
	def getSensors():
		"""
		DEPRECATED: use the sensors property
		Gets a list of all sensors attached to this controller.
		
		@rtype: list [L{SCA_ISensor}]
		"""
	def getSensor(name):
		"""
		DEPRECATED: use the sensors[name] property
		Gets the named linked sensor.
		
		@type name: string
		@rtype: L{SCA_ISensor}
		"""
	def getActuators():
		"""
		DEPRECATED: use the actuators property
		Gets a list of all actuators linked to this controller.
		
		@rtype: list [L{SCA_IActuator}]
		"""
	def getActuator(name):
		"""
		DEPRECATED: use the actuators[name] property
		Gets the named linked actuator.
		
		@type name: string
		@rtype: L{SCA_IActuator}
		"""
		