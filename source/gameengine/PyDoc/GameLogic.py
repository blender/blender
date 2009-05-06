# $Id$
"""
Documentation for the GameLogic Module.
=======================================

	Modules available in the game engine:
		- GameLogic
		- L{GameKeys}
		- L{Rasterizer}
		- L{GameTypes}
	
	Undocumented modules:
		- VideoTexture
		- CValue
		- Expression
		- PhysicsConstraints
	
	All the other modules are accessible through the methods in GameLogic.
	
	See L{WhatsNew} for updates, changes and new functionality in the Game Engine Python API.
	
	Examples::
		# To get the controller thats running this python script:
		co = GameLogic.getCurrentController() # GameLogic is automatically imported
		
		# To get the game object this controller is on:
		obj = co.getOwner()
	L{KX_GameObject} and L{KX_Camera} or L{KX_LightObject} methods are
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
		- L{DelaySensor<SCA_DelaySensor.SCA_DelaySensor>}
		- L{JoystickSensor<SCA_JoystickSensor.SCA_JoystickSensor>}
		- L{KeyboardSensor<SCA_KeyboardSensor.SCA_KeyboardSensor>}
		- L{MouseFocusSensor<KX_MouseFocusSensor.KX_MouseFocusSensor>}
		- L{MouseSensor<SCA_MouseSensor.SCA_MouseSensor>}
		- L{NearSensor<KX_NearSensor.KX_NearSensor>}
		- L{NetworkMessageSensor<KX_NetworkMessageSensor.KX_NetworkMessageSensor>}
		- L{PropertySensor<SCA_PropertySensor.SCA_PropertySensor>}
		- L{RadarSensor<KX_RadarSensor.KX_RadarSensor>}
		- L{RandomSensor<SCA_RandomSensor.SCA_RandomSensor>}
		- L{RaySensor<KX_RaySensor.KX_RaySensor>}
		- L{TouchSensor<KX_TouchSensor.KX_TouchSensor>}
	
	You can also access actuators linked to the controller::
		# To get an actuator attached to the controller:
		#                          +--------+  +-------------------------+
		#                          + Python +--+ Actuator "actuatorname" |
		#                          +--------+  +-------------------------+
		actuator = co.getActuator("actuatorname")
		
		# Activate an actuator
		controller.activate(actuator)
		
	See the actuator's reference for available methods:
		- L{2DFilterActuator<SCA_2DFilterActuator.SCA_2DFilterActuator>}
		- L{ActionActuator<BL_ActionActuator.BL_ActionActuator>}
		- L{AddObjectActuator<KX_SCA_AddObjectActuator.KX_SCA_AddObjectActuator>}
		- L{CameraActuator<KX_CameraActuator.KX_CameraActuator>}
		- L{CDActuator<KX_CDActuator.KX_CDActuator>}
		- L{ConstraintActuator<KX_ConstraintActuator.KX_ConstraintActuator>}
		- L{DynamicActuator<KX_SCA_DynamicActuator.KX_SCA_DynamicActuator>}
		- L{EndObjectActuator<KX_SCA_EndObjectActuator.KX_SCA_EndObjectActuator>}
		- L{GameActuator<KX_GameActuator.KX_GameActuator>}
		- L{IpoActuator<KX_IpoActuator.KX_IpoActuator>}
		- L{NetworkMessageActuator<KX_NetworkMessageActuator.KX_NetworkMessageActuator>}
		- L{ObjectActuator<KX_ObjectActuator.KX_ObjectActuator>}
		- L{ParentActuator<KX_ParentActuator.KX_ParentActuator>}
		- L{PropertyActuator<SCA_PropertyActuator.SCA_PropertyActuator>}
		- L{RandomActuator<SCA_RandomActuator.SCA_RandomActuator>}
		- L{ReplaceMeshActuator<KX_SCA_ReplaceMeshActuator.KX_SCA_ReplaceMeshActuator>}
		- L{SceneActuator<KX_SceneActuator.KX_SceneActuator>}
		- L{ShapeActionActuator<BL_ShapeActionActuator.BL_ShapeActionActuator>}
		- L{SoundActuator<KX_SoundActuator.KX_SoundActuator>}
		- L{StateActuator<KX_StateActuator.KX_StateActuator>}
		- L{TrackToActuator<KX_TrackToActuator.KX_TrackToActuator>}
		- L{VisibilityActuator<KX_VisibilityActuator.KX_VisibilityActuator>}

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




@group Constraint Actuator: KX_CONSTRAINTACT_LOCX, KX_CONSTRAINTACT_LOCY, KX_CONSTRAINTACT_LOCZ, KX_CONSTRAINTACT_ROTX, KX_CONSTRAINTACT_ROTY, KX_CONSTRAINTACT_ROTZ, KX_CONSTRAINTACT_DIRNX, KX_CONSTRAINTACT_DIRNY, KX_CONSTRAINTACT_DIRPX, KX_CONSTRAINTACT_DIRPY, KX_CONSTRAINTACT_ORIX, KX_CONSTRAINTACT_ORIY, KX_CONSTRAINTACT_ORIZ
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

@group IPO Actuator: KX_IPOACT_PLAY, KX_IPOACT_PINGPONG, KX_IPOACT_FLIPPER, KX_IPOACT_LOOPSTOP, KX_IPOACT_LOOPEND, KX_IPOACT_FROM_PROP
@var KX_IPOACT_PLAY:	 See L{KX_IpoActuator}
@var KX_IPOACT_PINGPONG:	 See L{KX_IpoActuator}
@var KX_IPOACT_FLIPPER:	 See L{KX_IpoActuator}
@var KX_IPOACT_LOOPSTOP:	 See L{KX_IpoActuator}
@var KX_IPOACT_LOOPEND:	 See L{KX_IpoActuator}
@var KX_IPOACT_FROM_PROP:	 See L{KX_IpoActuator}

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

@group Radar Sensor: KX_RADAR_AXIS_POS_X, KX_RADAR_AXIS_POS_Y, KX_RADAR_AXIS_POS_Z, KX_RADAR_AXIS_NEG_X, KX_RADAR_AXIS_NEG_Y, KX_RADAR_AXIS_NEG_Z
@var KX_RADAR_AXIS_POS_X:		    See L{KX_RadarSensor}
@var KX_RADAR_AXIS_POS_Y:		    See L{KX_RadarSensor}
@var KX_RADAR_AXIS_POS_Z:		    See L{KX_RadarSensor}
@var KX_RADAR_AXIS_NEG_X:		    See L{KX_RadarSensor}
@var KX_RADAR_AXIS_NEG_Y:		    See L{KX_RadarSensor}
@var KX_RADAR_AXIS_NEG_Z:		    See L{KX_RadarSensor}

@group Ray Sensor: KX_RAY_AXIS_POS_X, KX_RAY_AXIS_POS_Y, KX_RAY_AXIS_POS_Z, KX_RAY_AXIS_NEG_X, KX_RAY_AXIS_NEG_Y, KX_RAY_AXIS_NEG_Z
@var KX_RAY_AXIS_POS_X:		    See L{KX_RaySensor}
@var KX_RAY_AXIS_POS_Y:		    See L{KX_RaySensor}
@var KX_RAY_AXIS_POS_Z:		    See L{KX_RaySensor}
@var KX_RAY_AXIS_NEG_X:		    See L{KX_RaySensor}
@var KX_RAY_AXIS_NEG_Y:		    See L{KX_RaySensor}
@var KX_RAY_AXIS_NEG_Z:		    See L{KX_RaySensor}

@group Dynamic Actuator: KX_DYN_RESTORE_DYNAMICS, KX_DYN_DISABLE_DYNAMICS, KX_DYN_ENABLE_RIGID_BODY, KX_DYN_DISABLE_RIGID_BODY,	KX_DYN_SET_MASS
@var KX_DYN_RESTORE_DYNAMICS:	See L{KX_SCA_DynamicActuator}
@var KX_DYN_DISABLE_DYNAMICS:	See L{KX_SCA_DynamicActuator}
@var KX_DYN_ENABLE_RIGID_BODY:	See L{KX_SCA_DynamicActuator}
@var KX_DYN_DISABLE_RIGID_BODY:	See L{KX_SCA_DynamicActuator}
@var KX_DYN_SET_MASS:			See L{KX_SCA_DynamicActuator}

@group Game Actuator: KX_GAME_LOAD, KX_GAME_START, KX_GAME_RESTART, KX_GAME_QUIT, KX_GAME_SAVECFG, KX_GAME_LOADCFG
@var KX_Game_LOAD:		See L{KX_GameActuator}
@var KX_Game_START:		See L{KX_GameActuator}
@var KX_Game_RESTART:	See L{KX_GameActuator}
@var KX_Game_QUIT:		See L{KX_GameActuator}
@var KX_Game_SAVECFG:	See L{KX_GameActuator}
@var KX_Game_LOADCFG:	See L{KX_GameActuator}

@group Scene Actuator: KX_SCENE_RESTART, KX_SCENE_SET_SCENE, KX_SCENE_SET_CAMERA, KX_SCENE_ADD_FRONT_SCENE, KX_SCENE_ADD_BACK_SCENE, KX_SCENE_REMOVE_SCENE, KX_SCENE_SUSPEND, KX_SCENE_RESUME
KX_SCENE_RESTART:			See L{KX_SceneActuator}
KX_SCENE_SET_SCENE:			See L{KX_SceneActuator}
KX_SCENE_SET_CAMERA:		See L{KX_SceneActuator}
KX_SCENE_ADD_FRONT_SCENE:	See L{KX_SceneActuator}
KX_SCENE_ADD_BACK_SCENE:	See L{KX_SceneActuator}
KX_SCENE_REMOVE_SCENE:		See L{KX_SceneActuator}
KX_SCENE_SUSPEND:			See L{KX_SceneActuator}
KX_SCENE_RESUME:			See L{KX_SceneActuator}

@group Input Status: KX_INPUT_NONE, KX_INPUT_JUST_ACTIVATED, KX_INPUT_ACTIVE, KX_INPUT_JUST_RELEASED
@var KX_INPUT_NONE:				See L{SCA_MouseSensor}
@var KX_INPUT_JUST_ACTIVATED:	See L{SCA_MouseSensor}
@var KX_INPUT_ACTIVE:			See L{SCA_MouseSensor}
@var KX_INPUT_JUST_RELEASED:	See L{SCA_MouseSensor}


@group Mouse Buttons: KX_MOUSE_BUT_LEFT, KX_MOUSE_BUT_MIDDLE, KX_MOUSE_BUT_RIGHT
@var KX_MOUSE_BUT_LEFT:		See L{SCA_MouseSensor}
@var KX_MOUSE_BUT_MIDDLE:	See L{SCA_MouseSensor}
@var KX_MOUSE_BUT_RIGHT:	See L{SCA_MouseSensor}

@group States: KX_STATE1, KX_STATE10, KX_STATE11, KX_STATE12, KX_STATE13, KX_STATE14, KX_STATE15, KX_STATE16, KX_STATE17, KX_STATE18, KX_STATE19, KX_STATE2, KX_STATE20, KX_STATE21, KX_STATE22, KX_STATE23, KX_STATE24, KX_STATE25, KX_STATE26, KX_STATE27, KX_STATE28, KX_STATE29, KX_STATE3, KX_STATE30, KX_STATE4, KX_STATE5, KX_STATE6, KX_STATE7, KX_STATE8, KX_STATE9, KX_STATE_OP_CLR, KX_STATE_OP_CPY, KX_STATE_OP_NEG, KX_STATE_OP_SET
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

@group UNSORTED: BL_DST_ALPHA, BL_DST_COLOR, BL_ONE, BL_ONE_MINUS_DST_ALPHA, BL_ONE_MINUS_DST_COLOR, BL_ONE_MINUS_SRC_ALPHA, BL_ONE_MINUS_SRC_COLOR, BL_SRC_ALPHA, BL_SRC_ALPHA_SATURATE, BL_SRC_COLOR, BL_ZERO, CAM_POS, CONSTANT_TIMER, KX_ACT_CONSTRAINT_DISTANCE, KX_ACT_CONSTRAINT_DOROTFH, KX_ACT_CONSTRAINT_FHNX, KX_ACT_CONSTRAINT_FHNY, KX_ACT_CONSTRAINT_FHNZ, KX_ACT_CONSTRAINT_FHPX, KX_ACT_CONSTRAINT_FHPY, KX_ACT_CONSTRAINT_FHPZ, KX_ACT_CONSTRAINT_LOCAL, KX_ACT_CONSTRAINT_MATERIAL, KX_ACT_CONSTRAINT_NORMAL, KX_ACT_CONSTRAINT_PERMANENT, MODELMATRIX, MODELMATRIX_INVERSE, MODELMATRIX_INVERSETRANSPOSE, MODELMATRIX_TRANSPOSE, MODELVIEWMATRIX, MODELVIEWMATRIX_INVERSE, MODELVIEWMATRIX_INVERSETRANSPOSE, MODELVIEWMATRIX_TRANSPOSE, RAS_2DFILTER_BLUR, RAS_2DFILTER_CUSTOMFILTER, RAS_2DFILTER_DILATION, RAS_2DFILTER_DISABLED, RAS_2DFILTER_ENABLED, RAS_2DFILTER_EROSION, RAS_2DFILTER_GRAYSCALE, RAS_2DFILTER_INVERT, RAS_2DFILTER_LAPLACIAN, RAS_2DFILTER_MOTIONBLUR, RAS_2DFILTER_NOFILTER, RAS_2DFILTER_PREWITT, RAS_2DFILTER_SEPIA, RAS_2DFILTER_SHARPEN, RAS_2DFILTER_SOBEL, SHD_TANGENT, VIEWMATRIX, VIEWMATRIX_INVERSE, VIEWMATRIX_INVERSETRANSPOSE, VIEWMATRIX_TRANSPOSE
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
@var CAM_POS:
@var CONSTANT_TIMER:
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
@var MODELMATRIX:
@var MODELMATRIX_INVERSE:
@var MODELMATRIX_INVERSETRANSPOSE:
@var MODELMATRIX_TRANSPOSE:
@var MODELVIEWMATRIX:
@var MODELVIEWMATRIX_INVERSE:
@var MODELVIEWMATRIX_INVERSETRANSPOSE:
@var MODELVIEWMATRIX_TRANSPOSE:
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
@var SHD_TANGENT:
@var VIEWMATRIX:
@var VIEWMATRIX_INVERSE:
@var VIEWMATRIX_INVERSETRANSPOSE:
@var VIEWMATRIX_TRANSPOSE:

@group Deprecated: addActiveActuator
"""

# TODO
# globalDict
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
	Activates the given actuator. (B{deprecated})
	
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
def PrintGLInfo():
	"""
	Prints GL Extension Info into the console
	"""