# $Id$
"""
Documentation for the GameLogic Module.
=======================================

	There are only three importable modules in the game engine:
		- GameLogic
		- L{GameKeys}
		- L{Rasterizer}
	
	All the other modules are accessibly through the methods in GameLogic.
	
	Examples::
		# To get a controller:
		import GameLogic
		co = GameLogic.getCurrentController()
		
		# To get the game object associated with this controller:
		obj = co.getOwner()
	L{KX_GameObject} and possibly L{KX_Camera} and L{KX_Light} methods are
	available depending on the type of object::
		# To get a sensor linked to this controller.
		# "sensorname" is the name of the sensor as defined in the Blender interface.
		sens = co.getSensor("sensorname")
	L{KX_NetworkMessageSensor}, L{KX_RaySensor}, L{KX_TouchSensor}, L{SCA_KeyboardSensor}, 
	L{SCA_MouseSensor}, L{SCA_PropertySensor} and L{SCA_RandomSensor} methods are available, 
	depending on the type of sensor::
		# To get a list of all sensors:
		sensors = co.B{getSensors}()



Constants
=========
	- KX_TRUE:  True value used by some modules
	- KX_FALSE: False value used by some modules.

Property Sensor
---------------
	Tests that the property sensor is able to perform. See L{SCA_PropertySensor}
		- KX_PROPSENSOR_EQUAL:		Activate when the property is equal to the sensor value.
		- KX_PROPSENSOR_NOTEQUAL:	Activate when the property is not equal to the sensor value.
		- KX_PROPSENSOR_INTERVAL:	Activate when the property is between the specified limits.
		- KX_PROPSENSOR_CHANGED:	Activate when the property changes
		- KX_PROPSENSOR_EXPRESSION:	Activate when the expression matches

Constraint Actuator
-------------------
	The axis and type (location/rotation) of constraint. See L{KX_ConstraintActuator}
		- KX_CONSTRAINTACT_LOCX
		- KX_CONSTRAINTACT_LOCY
		- KX_CONSTRAINTACT_LOCZ
		- KX_CONSTRAINTACT_ROTX
		- KX_CONSTRAINTACT_ROTY
		- KX_CONSTRAINTACT_ROTZ

IPO Actuator
------------
	IPO Types.  See L{KX_IpoActuator}
		- KX_IPOACT_PLAY
		- KX_IPOACT_PINGPONG
		- KX_IPOACT_FLIPPER
		- KX_IPOACT_LOOPSTOP
		- KX_IPOACT_LOOPEND

Random Distributions
--------------------
	See L{SCA_RandomActuator}
		- KX_RANDOMACT_BOOL_CONST
		- KX_RANDOMACT_BOOL_UNIFORM
		- KX_RANDOMACT_BOOL_BERNOUILLI
		- KX_RANDOMACT_INT_CONST
		- KX_RANDOMACT_INT_UNIFORM
		- KX_RANDOMACT_INT_POISSON
		- KX_RANDOMACT_FLOAT_CONST
		- KX_RANDOMACT_FLOAT_UNIFORM
		- KX_RANDOMACT_FLOAT_NORMAL
		- KX_RANDOMACT_FLOAT_NEGATIVE_EXPONENTIAL

"""


def getCurrentController():
	"""
	Gets the Python controller associated with this Python script.
	
	@rtype: L{SCA_PythonController}
	"""
def addActiveActuator(actuator, activate):
	"""
	Activates the given actuator.
	
	@type actuator: L{SCA_IActuator}
	@type activate: boolean
	@param activate: whether to activate or deactivate the given actuator.
	"""
def getRandomFloat():
	"""
	Returns a random floating point value in the range [0...1)
	"""
def setGravity(gravity):
	"""
	Sets the world gravity.
	
	@type gravity: list [fx, fy, fz]
	"""
def getSpectrum():
	"""
	Returns a 512 point list from the sound card.
	This only works if the fmod sound driver is being used.
	
	@rtype: list [float], len(getSpectrum()) == 512
	"""
def stopDSP():
	"""
	Stops the sound driver using DSP effects.
	
	Only the fmod sound driver supports this.
	DSP can be computationally expensive.
	"""
