# $Id$
"""
Documentation for the GameLogic Module.
=======================================

	There are only three importable modules in the game engine:
		- GameLogic
		- L{GameKeys}
		- L{Rasterizer}
	
	All the other modules are accessible through the methods in GameLogic.
	
	See L{WhatsNew} for updates, changes and new functionality in the Game Engine Python API.
	
	Examples::
		# To get a controller:
		co = GameLogic.getCurrentController() # GameLogic is automatically imported
		
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
		- L{SCA_DelaySensor}
	
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
	
	Most logic brick's methods are accessors for the properties available in the logic buttons.
	Consult the logic bricks documentation for more information on how each logic brick works.
	
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

@group Action Actuator: KX_ACTIONACT_PLAY, KX_ACTIONACT_FLIPPER, KX_ACTIONACT_LOOPSTOP, KX_ACTIONACT_LOOPEND, KX_ACTIONACT_PROPERTY
@var KX_ACTIONACT_PLAY:	    See L{BL_ActionActuator}
@var KX_ACTIONACT_FLIPPER:  See L{BL_ActionActuator}
@var KX_ACTIONACT_LOOPSTOP: See L{BL_ActionActuator}
@var KX_ACTIONACT_LOOPEND:  See L{BL_ActionActuator}
@var KX_ACTIONACT_PROPERTY: See L{BL_ActionActuator}

@group Sound Actuator: KX_SOUNDACT_PLAYSTOP, KX_SOUNDACT_PLAYEND, KX_SOUNDACT_LOOPSTOP, KX_SOUNDACT_LOOPEND, KX_SOUNDACT_LOOPBIDIRECTIONAL, KX_SOUNDACT_LOOPBIDIRECTIONAL_STOP
@var KX_SOUNDACT_PLAYSTOP:		    See L{KX_SoundActuator}
@var KX_SOUNDACT_PLAYEND:		    See L{KX_SoundActuator}
@var KX_SOUNDACT_LOOPSTOP:		    See L{KX_SoundActuator}
@var KX_SOUNDACT_LOOPEND:		    See L{KX_SoundActuator}
@var KX_SOUNDACT_LOOPBIDIRECTIONAL:	    See L{KX_SoundActuator}
@var KX_SOUNDACT_LOOPBIDIRECTIONAL_STOP:    See L{KX_SoundActuator}
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
def getLogicTicRate():
	"""
	Gets the logic update frequency.
	
	@return: The logic frequency in Hz
	@rtype: float
	"""
def setLogicTicRate(ticrate):
	"""
	Sets the logic update frequency.
	
	The logic update frequency is the number of times logic bricks are executed every second.
	The default is 30 Hz.
	
	@param ticrate: The new logic update frequency (in Hz).
	@type ticrate: float
	"""
def getPhysicsTicRate():
	"""
	Gets the physics update frequency
	
	@return: The physics update frequency in Hz
	@rtype: float
	"""
def setPhysicsTicRate(ticrate):
	"""
	Sets the physics update frequency
	
	The physics update frequency is the number of times the physics system is executed every second.
	The default is 60 Hz.
	
	@param ticrate: The new update frequency (in Hz).
	@type ticrate: float
	"""
def getAverageFrameRate():
	"""
	Gets the estimated average framerate
	
	@return: The estimed average framerate in frames per second
	@rtype: float
	"""

def expandPath(path):
	"""
	Converts a blender internal path into a proper file system path.

	Use / as directory separator in path
	You can use '//' at the start of the string to define a relative path;
	Blender replaces that string by the directory of the startup .blend or runtime file
	to make a full path name (doesn't change during the game, even if you load other .blend).
	The function also converts the directory separator to the local file system format.

	@param path: The path string to be converted/expanded.
	@type path: string
	@return: The converted string
	@rtype: string
	"""

def getBlendFileList(path = "//"):
	"""
	Returns a list of blend files in the same directory as the open blend file, or from using the option argument.

	@param path: Optional directory argument, will be expanded (like expandPath) into the full path.
	@type path: string
	@return: A list of filenames, with no directory prefix
	@rtype: list
	"""
