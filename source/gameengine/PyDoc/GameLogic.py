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
	L{KX_GameObject} and L{KX_Camera} or L{KX_Light} methods are
	available depending on the type of object::
		# To get a sensor linked to this controller.
		# "sensorname" is the name of the sensor as defined in the Blender interface.
		# +---------------------+  +--------+
		# | Sensor "sensorname" +--+ Python +
		# +---------------------+  +--------+
		sens = co.getSensor("sensorname")
	
		# To get a list of all sensors:
		sensors = co.getSensors()

	See the sensor's reference for available methods:
		- L{KX_NetworkMessageSensor}
		- L{KX_RaySensor}
		- L{KX_MouseFocusSensor}
		- L{KX_NearSensor}
		- L{KX_RadarSensor}
		- L{KX_TouchSensor}
		- L{SCA_KeyboardSensor}
		- L{SCA_MouseSensor}
		- L{SCA_PropertySensor} 
		- L{SCA_RandomSensor} 
	
	You can also access actuators linked to the controller::
		# To get an actuator attached to the controller:
		#                          +--------+  +-------------------------+
		#                          + Python +--+ Actuator "actuatorname" |
		#                          +--------+  +-------------------------+
		actuator = co.getActuator("actuatorname")
		
		# Activate an actuator
		GameLogic.addActiveActuator(actuator, True)
		
	See the actuator's reference for available methods:
		- L{BL_ActionActuator}
		- L{KX_CameraActuator}
		- L{KX_CDActuator}
		- L{KX_ConstraintActuator}
		- L{KX_GameActuator}
		- L{KX_IpoActuator}
		- L{KX_NetworkMessageActuator}
		- L{KX_ObjectActuator}
		- L{KX_SCA_AddObjectActuator}
		- L{KX_SCA_EndObjectActuator}
		- L{KX_SCA_ReplaceMeshActuator}
		- L{KX_SceneActuator}
		- L{KX_SoundActuator}
		- L{KX_TrackToActuator}
		- L{KX_VisibilityActuator}
		- L{SCA_PropertyActuator}
		- L{SCA_RandomActuator} 
	
	There are also methods to access the current L{KX_Scene}::
		# Get the current scene
		scene = GameLogic.getCurrentScene()
		
		# Get the current camera
		cam = scene.active_camera

	Matricies as used by the game engine are B{row major}::
		matrix[row][col] = blah
	L{KX_Camera} has some examples using matricies.


@group Constants: KX_TRUE, KX_FALSE
@var KX_TRUE: True value used by some modules.
@var KX_FALSE: False value used by some modules.

@group Property Sensor: KX_PROPSENSOR_EQUAL, KX_PROPSENSOR_NOTEQUAL, KX_PROPSENSOR_INTERVAL, KX_PROPSENSOR_CHANGED, KX_PROPSENSOR_EXPRESSION
@var KX_PROPSENSOR_EQUAL:		Activate when the property is equal to the sensor value.
@var KX_PROPSENSOR_NOTEQUAL:	Activate when the property is not equal to the sensor value.
@var KX_PROPSENSOR_INTERVAL:	Activate when the property is between the specified limits.
@var KX_PROPSENSOR_CHANGED:	Activate when the property changes
@var KX_PROPSENSOR_EXPRESSION:	Activate when the expression matches

@group Constraint Actuator: KX_CONSTRAINTACT_LOCX, KX_CONSTRAINTACT_LOCY, KX_CONSTRAINTACT_LOCZ, KX_CONSTRAINTACT_ROTX, KX_CONSTRAINTACT_ROTY, KX_CONSTRAINTACT_ROTZ
@var KX_CONSTRAINTACT_LOCX: See L{KX_ConstraintActuator}
@var KX_CONSTRAINTACT_LOCY: See L{KX_ConstraintActuator}
@var KX_CONSTRAINTACT_LOCZ: See L{KX_ConstraintActuator}
@var KX_CONSTRAINTACT_ROTX: See L{KX_ConstraintActuator}
@var KX_CONSTRAINTACT_ROTY: See L{KX_ConstraintActuator}
@var KX_CONSTRAINTACT_ROTZ: See L{KX_ConstraintActuator}

@group IPO Actuator: KX_IPOACT_PLAY, KX_IPOACT_PINGPONG, KX_IPOACT_FLIPPER, KX_IPOACT_LOOPSTOP, KX_IPOACT_LOOPEND
@var KX_IPOACT_PLAY:	 See L{KX_IpoActuator}
@var KX_IPOACT_PINGPONG:	 See L{KX_IpoActuator}
@var KX_IPOACT_FLIPPER:	 See L{KX_IpoActuator}
@var KX_IPOACT_LOOPSTOP:	 See L{KX_IpoActuator}
@var KX_IPOACT_LOOPEND:	 See L{KX_IpoActuator}

@group Random Distributions: KX_RANDOMACT_BOOL_CONST, KX_RANDOMACT_BOOL_UNIFORM, KX_RANDOMACT_BOOL_BERNOUILLI, KX_RANDOMACT_INT_CONST, KX_RANDOMACT_INT_UNIFORM, KX_RANDOMACT_INT_POISSON, KX_RANDOMACT_FLOAT_CONST, KX_RANDOMACT_FLOAT_UNIFORM, KX_RANDOMACT_FLOAT_NORMAL, KX_RANDOMACT_FLOAT_NEGATIVE_EXPONENTIAL
@var KX_RANDOMACT_BOOL_CONST:		See L{SCA_RandomActuator}
@var KX_RANDOMACT_BOOL_UNIFORM:		See L{SCA_RandomActuator}
@var KX_RANDOMACT_BOOL_BERNOUILLI:		See L{SCA_RandomActuator}
@var KX_RANDOMACT_INT_CONST:		See L{SCA_RandomActuator}
@var KX_RANDOMACT_INT_UNIFORM:		See L{SCA_RandomActuator}
@var KX_RANDOMACT_INT_POISSON:		See L{SCA_RandomActuator}
@var KX_RANDOMACT_FLOAT_CONST:		See L{SCA_RandomActuator}
@var KX_RANDOMACT_FLOAT_UNIFORM:		See L{SCA_RandomActuator}
@var KX_RANDOMACT_FLOAT_NORMAL:		See L{SCA_RandomActuator}
@var KX_RANDOMACT_FLOAT_NEGATIVE_EXPONENTIAL:		See L{SCA_RandomActuator}

"""


def getCurrentController():
	"""
	Gets the Python controller associated with this Python script.
	
	@rtype: L{SCA_PythonController}
	"""
def getCurrentScene():
	"""
	Gets the current Scene.
	
	@rtype: L{KX_Scene}
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

