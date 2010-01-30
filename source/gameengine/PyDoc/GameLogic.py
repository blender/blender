# $Id$
"""
Documentation for the GameLogic Module.
=======================================
	
	Module to access logic functions, imported automatically into the python controllers namespace.
	
	Examples::
		# To get the controller thats running this python script:
		cont = GameLogic.getCurrentController() # GameLogic is automatically imported
		
		# To get the game object this controller is on:
		obj = cont.owner
	L{KX_GameObject} and L{KX_Camera} or L{KX_LightObject} methods are
	available depending on the type of object::
		# To get a sensor linked to this controller.
		# "sensorname" is the name of the sensor as defined in the Blender interface.
		# +---------------------+  +--------+
		# | Sensor "sensorname" +--+ Python +
		# +---------------------+  +--------+
		sens = cont.sensors["sensorname"]
	
		# To get a sequence of all sensors:
		sensors = co.sensors

	See the sensor's reference for available methods:
		- L{DelaySensor<GameTypes.SCA_DelaySensor>}
		- L{JoystickSensor<GameTypes.SCA_JoystickSensor>}
		- L{KeyboardSensor<GameTypes.SCA_KeyboardSensor>}
		- L{MouseFocusSensor<GameTypes.KX_MouseFocusSensor>}
		- L{MouseSensor<GameTypes.SCA_MouseSensor>}
		- L{NearSensor<GameTypes.KX_NearSensor>}
		- L{NetworkMessageSensor<GameTypes.KX_NetworkMessageSensor>}
		- L{PropertySensor<GameTypes.SCA_PropertySensor>}
		- L{RadarSensor<GameTypes.KX_RadarSensor>}
		- L{RandomSensor<GameTypes.SCA_RandomSensor>}
		- L{RaySensor<GameTypes.KX_RaySensor>}
		- L{TouchSensor<GameTypes.KX_TouchSensor>}
	
	You can also access actuators linked to the controller::
		# To get an actuator attached to the controller:
		#                          +--------+  +-------------------------+
		#                          + Python +--+ Actuator "actuatorname" |
		#                          +--------+  +-------------------------+
		actuator = co.actuators["actuatorname"]
		
		# Activate an actuator
		controller.activate(actuator)
		
	See the actuator's reference for available methods:
		- L{2DFilterActuator<GameTypes.SCA_2DFilterActuator>}
		- L{ActionActuator<GameTypes.BL_ActionActuator>}
		- L{AddObjectActuator<GameTypes.KX_SCA_AddObjectActuator>}
		- L{CameraActuator<GameTypes.KX_CameraActuator>}
		- L{ConstraintActuator<GameTypes.KX_ConstraintActuator>}
		- L{DynamicActuator<GameTypes.KX_SCA_DynamicActuator>}
		- L{EndObjectActuator<GameTypes.KX_SCA_EndObjectActuator>}
		- L{GameActuator<GameTypes.KX_GameActuator>}
		- L{IpoActuator<GameTypes.KX_IpoActuator>}
		- L{NetworkMessageActuator<GameTypes.KX_NetworkMessageActuator>}
		- L{ObjectActuator<GameTypes.KX_ObjectActuator>}
		- L{ParentActuator<GameTypes.KX_ParentActuator>}
		- L{PropertyActuator<GameTypes.SCA_PropertyActuator>}
		- L{RandomActuator<GameTypes.SCA_RandomActuator>}
		- L{ReplaceMeshActuator<GameTypes.KX_SCA_ReplaceMeshActuator>}
		- L{SceneActuator<GameTypes.KX_SceneActuator>}
		- L{ShapeActionActuator<GameTypes.BL_ShapeActionActuator>}
		- L{SoundActuator<GameTypes.KX_SoundActuator>}
		- L{StateActuator<GameTypes.KX_StateActuator>}
		- L{TrackToActuator<GameTypes.KX_TrackToActuator>}
		- L{VisibilityActuator<GameTypes.KX_VisibilityActuator>}

	Most logic brick's methods are accessors for the properties available in the logic buttons.
	Consult the logic bricks documentation for more information on how each logic brick works.
	
	There are also methods to access the current L{KX_Scene}::
		# Get the current scene
		scene = GameLogic.getCurrentScene()
		
		# Get the current camera
		cam = scene.active_camera

	Matricies as used by the game engine are B{row major}::
		matrix[row][col] = float
	L{KX_Camera} has some examples using matricies.


@group Constants: KX_TRUE, KX_FALSE
@var KX_TRUE: True value used by some modules.
@var KX_FALSE: False value used by some modules.

@group Property Sensor: KX_PROPSENSOR_*
@var KX_PROPSENSOR_EQUAL:		Activate when the property is equal to the sensor value.
@var KX_PROPSENSOR_NOTEQUAL:	Activate when the property is not equal to the sensor value.
@var KX_PROPSENSOR_INTERVAL:	Activate when the property is between the specified limits.
@var KX_PROPSENSOR_CHANGED:	Activate when the property changes
@var KX_PROPSENSOR_EXPRESSION:	Activate when the expression matches

@group Constraint Actuator: KX_CONSTRAINTACT_*
@var KX_CONSTRAINTACT_LOCX: See L{KX_ConstraintActuator}
@var KX_CONSTRAINTACT_LOCY: See L{KX_ConstraintActuator}
@var KX_CONSTRAINTACT_LOCZ: See L{KX_ConstraintActuator}
@var KX_CONSTRAINTACT_ROTX: See L{KX_ConstraintActuator}
@var KX_CONSTRAINTACT_ROTY: See L{KX_ConstraintActuator}
@var KX_CONSTRAINTACT_ROTZ: See L{KX_ConstraintActuator}
@var KX_CONSTRAINTACT_DIRNX: See L{KX_ConstraintActuator}
@var KX_CONSTRAINTACT_DIRNY: See L{KX_ConstraintActuator}
@var KX_CONSTRAINTACT_DIRPX: See L{KX_ConstraintActuator}
@var KX_CONSTRAINTACT_DIRPY: See L{KX_ConstraintActuator}
@var KX_CONSTRAINTACT_ORIX: See L{KX_ConstraintActuator}
@var KX_CONSTRAINTACT_ORIY: See L{KX_ConstraintActuator}
@var KX_CONSTRAINTACT_ORIZ: See L{KX_ConstraintActuator}

@group IPO Actuator: KX_IPOACT_*
@var KX_IPOACT_PLAY:	 See L{KX_IpoActuator}
@var KX_IPOACT_PINGPONG:	 See L{KX_IpoActuator}
@var KX_IPOACT_FLIPPER:	 See L{KX_IpoActuator}
@var KX_IPOACT_LOOPSTOP:	 See L{KX_IpoActuator}
@var KX_IPOACT_LOOPEND:	 See L{KX_IpoActuator}
@var KX_IPOACT_FROM_PROP:	 See L{KX_IpoActuator}

@group Random Distributions: KX_RANDOMACT_*
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

@group Action Actuator: KX_ACTIONACT_*
@var KX_ACTIONACT_PLAY:	    See L{BL_ActionActuator}
@var KX_ACTIONACT_FLIPPER:  See L{BL_ActionActuator}
@var KX_ACTIONACT_LOOPSTOP: See L{BL_ActionActuator}
@var KX_ACTIONACT_LOOPEND:  See L{BL_ActionActuator}
@var KX_ACTIONACT_PROPERTY: See L{BL_ActionActuator}

@group Sound Actuator: KX_SOUNDACT_*
@var KX_SOUNDACT_PLAYSTOP:		    See L{KX_SoundActuator}
@var KX_SOUNDACT_PLAYEND:		    See L{KX_SoundActuator}
@var KX_SOUNDACT_LOOPSTOP:		    See L{KX_SoundActuator}
@var KX_SOUNDACT_LOOPEND:		    See L{KX_SoundActuator}
@var KX_SOUNDACT_LOOPBIDIRECTIONAL:	    See L{KX_SoundActuator}
@var KX_SOUNDACT_LOOPBIDIRECTIONAL_STOP:    See L{KX_SoundActuator}

@group Radar Sensor: KX_RADAR_*
@var KX_RADAR_AXIS_POS_X:		    See L{KX_RadarSensor}
@var KX_RADAR_AXIS_POS_Y:		    See L{KX_RadarSensor}
@var KX_RADAR_AXIS_POS_Z:		    See L{KX_RadarSensor}
@var KX_RADAR_AXIS_NEG_X:		    See L{KX_RadarSensor}
@var KX_RADAR_AXIS_NEG_Y:		    See L{KX_RadarSensor}
@var KX_RADAR_AXIS_NEG_Z:		    See L{KX_RadarSensor}

@group Ray Sensor: KX_RAY_*
@var KX_RAY_AXIS_POS_X:		    See L{KX_RaySensor}
@var KX_RAY_AXIS_POS_Y:		    See L{KX_RaySensor}
@var KX_RAY_AXIS_POS_Z:		    See L{KX_RaySensor}
@var KX_RAY_AXIS_NEG_X:		    See L{KX_RaySensor}
@var KX_RAY_AXIS_NEG_Y:		    See L{KX_RaySensor}
@var KX_RAY_AXIS_NEG_Z:		    See L{KX_RaySensor}

@group Dynamic Actuator: KX_DYN_*
@var KX_DYN_RESTORE_DYNAMICS:	See L{KX_SCA_DynamicActuator}
@var KX_DYN_DISABLE_DYNAMICS:	See L{KX_SCA_DynamicActuator}
@var KX_DYN_ENABLE_RIGID_BODY:	See L{KX_SCA_DynamicActuator}
@var KX_DYN_DISABLE_RIGID_BODY:	See L{KX_SCA_DynamicActuator}
@var KX_DYN_SET_MASS:			See L{KX_SCA_DynamicActuator}

@group Game Actuator: KX_GAME_*
@var KX_GAME_LOAD:		See L{KX_GameActuator}
@var KX_GAME_START:		See L{KX_GameActuator}
@var KX_GAME_RESTART:	See L{KX_GameActuator}
@var KX_GAME_QUIT:		See L{KX_GameActuator}
@var KX_GAME_SAVECFG:	See L{KX_GameActuator}
@var KX_GAME_LOADCFG:	See L{KX_GameActuator}

@group Scene Actuator: KX_SCENE_*
@var KX_SCENE_RESTART:			See L{KX_SceneActuator}
@var KX_SCENE_SET_SCENE:			See L{KX_SceneActuator}
@var KX_SCENE_SET_CAMERA:		See L{KX_SceneActuator}
@var KX_SCENE_ADD_FRONT_SCENE:	See L{KX_SceneActuator}
@var KX_SCENE_ADD_BACK_SCENE:	See L{KX_SceneActuator}
@var KX_SCENE_REMOVE_SCENE:		See L{KX_SceneActuator}
@var KX_SCENE_SUSPEND:			See L{KX_SceneActuator}
@var KX_SCENE_RESUME:			See L{KX_SceneActuator}

@group Input Status: KX_INPUT_*
@var KX_INPUT_NONE:				See L{SCA_MouseSensor}
@var KX_INPUT_JUST_ACTIVATED:	See L{SCA_MouseSensor}
@var KX_INPUT_ACTIVE:			See L{SCA_MouseSensor}
@var KX_INPUT_JUST_RELEASED:	See L{SCA_MouseSensor}


@group Mouse Buttons: KX_MOUSE_BUT_*
@var KX_MOUSE_BUT_LEFT:		See L{SCA_MouseSensor}
@var KX_MOUSE_BUT_MIDDLE:	See L{SCA_MouseSensor}
@var KX_MOUSE_BUT_RIGHT:	See L{SCA_MouseSensor}

@group States: KX_STATE*
@var KX_STATE1:
@var KX_STATE10:
@var KX_STATE11:
@var KX_STATE12:
@var KX_STATE13:
@var KX_STATE14:
@var KX_STATE15:
@var KX_STATE16:
@var KX_STATE17:
@var KX_STATE18:
@var KX_STATE19:
@var KX_STATE2:
@var KX_STATE20:
@var KX_STATE21:
@var KX_STATE22:
@var KX_STATE23:
@var KX_STATE24:
@var KX_STATE25:
@var KX_STATE26:
@var KX_STATE27:
@var KX_STATE28:
@var KX_STATE29:
@var KX_STATE3:
@var KX_STATE30:
@var KX_STATE4:
@var KX_STATE5:
@var KX_STATE6:
@var KX_STATE7:
@var KX_STATE8:
@var KX_STATE9:
@var KX_STATE_OP_CLR:
@var KX_STATE_OP_CPY:
@var KX_STATE_OP_NEG:
@var KX_STATE_OP_SET:

@group 2D Filter: RAS_2DFILTER_*
@var RAS_2DFILTER_BLUR:
@var RAS_2DFILTER_CUSTOMFILTER:
@var RAS_2DFILTER_DILATION:
@var RAS_2DFILTER_DISABLED:
@var RAS_2DFILTER_ENABLED:
@var RAS_2DFILTER_EROSION:
@var RAS_2DFILTER_GRAYSCALE:
@var RAS_2DFILTER_INVERT:
@var RAS_2DFILTER_LAPLACIAN:
@var RAS_2DFILTER_MOTIONBLUR:
@var RAS_2DFILTER_NOFILTER:
@var RAS_2DFILTER_PREWITT:
@var RAS_2DFILTER_SEPIA:
@var RAS_2DFILTER_SHARPEN:
@var RAS_2DFILTER_SOBEL:

@group Constraint Actuator: KX_ACT_CONSTRAINT_*
@var KX_ACT_CONSTRAINT_DISTANCE:
@var KX_ACT_CONSTRAINT_DOROTFH:
@var KX_ACT_CONSTRAINT_FHNX:
@var KX_ACT_CONSTRAINT_FHNY:
@var KX_ACT_CONSTRAINT_FHNZ:
@var KX_ACT_CONSTRAINT_FHPX:
@var KX_ACT_CONSTRAINT_FHPY:
@var KX_ACT_CONSTRAINT_FHPZ:
@var KX_ACT_CONSTRAINT_LOCAL:
@var KX_ACT_CONSTRAINT_MATERIAL:
@var KX_ACT_CONSTRAINT_NORMAL:
@var KX_ACT_CONSTRAINT_PERMANENT:

@group Parent Actuator: KX_PARENT_*
@var KX_PARENT_REMOVE:
@var KX_PARENT_SET:

@group Shader: MODELMATRIX*, MODELVIEWMATRIX*, VIEWMATRIX*, CAM_POS, CONSTANT_TIMER, SHD_TANGENT
@var VIEWMATRIX:
@var VIEWMATRIX_INVERSE:
@var VIEWMATRIX_INVERSETRANSPOSE:
@var VIEWMATRIX_TRANSPOSE:
@var MODELMATRIX:
@var MODELMATRIX_INVERSE:
@var MODELMATRIX_INVERSETRANSPOSE:
@var MODELMATRIX_TRANSPOSE:
@var MODELVIEWMATRIX:
@var MODELVIEWMATRIX_INVERSE:
@var MODELVIEWMATRIX_INVERSETRANSPOSE:
@var MODELVIEWMATRIX_TRANSPOSE:
@var CAM_POS: Current camera position
@var CONSTANT_TIMER: User a timer for the uniform value.
@var SHD_TANGENT: Not yet documented.

@group Blender Material: BL_*
@var BL_DST_ALPHA:
@var BL_DST_COLOR:
@var BL_ONE:
@var BL_ONE_MINUS_DST_ALPHA:
@var BL_ONE_MINUS_DST_COLOR:
@var BL_ONE_MINUS_SRC_ALPHA:
@var BL_ONE_MINUS_SRC_COLOR:
@var BL_SRC_ALPHA:
@var BL_SRC_ALPHA_SATURATE:
@var BL_SRC_COLOR:
@var BL_ZERO:

@group Deprecated: addActiveActuator

@var globalDict:	A dictionary that is saved between loading blend files so you can use
					it to store inventory and other variables you want to store between
					scenes and blend files. It can also be written to a file and loaded
					later on with the game load/save actuators.
					note: only python built in types such as int/string/bool/float/tuples/lists
					can be saved, GameObjects, Actuators etc will not work as expectred.
"""

import GameTypes

# TODO
# error

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
def getSceneList():
	"""
	Gets a list of the current scenes loaded in the game engine.
	
	@note: Scenes in your blend file that have not been converted wont be in this list. This list will only contain scenes such as overlays scenes.
	
	@rtype: list of L{KX_Scene}
	"""
def addActiveActuator(actuator, activate):
	"""
	Activates the given actuator.
	
	@deprecated: Use L{GameTypes.SCA_PythonController.activate} and L{GameTypes.SCA_PythonController.deactivate} instead.
	@type actuator: L{SCA_IActuator} or the actuator name as a string.
	@type activate: boolean
	@param activate: whether to activate or deactivate the given actuator.
	"""
def sendMessage(subject, body="", to="", message_from=""):
	"""
	Sends a message to sensors in any active scene.
	
	@param subject: The subject of the message
	@type subject: string
	@param body: The body of the message (optional)
	@type body: string
	@param to: The name of the object to send the message to (optional)
	@type to: string
	@param message_from: The name of the object that the message is coming from (optional)
	@type message_from: string
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
def getMaxLogicFrame():
	"""
	Gets the maximum number of logic frame per render frame.
	
	@return: The maximum number of logic frame per render frame
	@rtype: interger
	"""
def setMaxLogicFrame(maxlogic):
	"""
	Sets the maximum number of logic frame that are executed per render frame.
	This does not affect the physic system that still runs at full frame rate.	
	 
	@param maxlogic: The new maximum number of logic frame per render frame. Valid values: 1..5
	@type maxlogic: integer
	"""
def getMaxPhysicsFrame():
	"""
	Gets the maximum number of physics frame per render frame.
	
	@return: The maximum number of physics frame per render frame
	@rtype: interger
	"""
def setMaxPhysicsFrame(maxphysics):
	"""
	Sets the maximum number of physics timestep that are executed per render frame.
	Higher value allows physics to keep up with realtime even if graphics slows down the game.
	Physics timestep is fixed and equal to 1/tickrate (see setLogicTicRate)
	maxphysics/ticrate is the maximum delay of the renderer that physics can compensate.
	 
	@param maxphysics: The new maximum number of physics timestep per render frame. Valid values: 1..5.
	@type maxphysics: integer
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
	The default is 60 Hz.
	
	@param ticrate: The new logic update frequency (in Hz).
	@type ticrate: float
	"""
def getPhysicsTicRate():
	"""
	NOT IMPLEMENTED
	Gets the physics update frequency
	
	@return: The physics update frequency in Hz
	@rtype: float
	"""
def setPhysicsTicRate(ticrate):
	"""
	NOT IMPLEMENTED
	Sets the physics update frequency
	
	The physics update frequency is the number of times the physics system is executed every second.
	The default is 60 Hz.
	
	@param ticrate: The new update frequency (in Hz).
	@type ticrate: float
	"""
def saveGlobalDict():
	"""
	Saves GameLogic.globalDict to a file.
	"""
def loadGlobalDict():
	"""
	Loads GameLogic.globalDict from a file.
	"""

#{ Utility functions
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
def PrintGLInfo():
	"""
	Prints GL Extension Info into the console
	"""
def getRandomFloat():
	"""
	Returns a random floating point value in the range [0...1)
	"""
#}
