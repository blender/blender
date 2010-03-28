"""
Documentation for the GameTypes Module.
=======================================

@group Base: PyObjectPlus, CValue, CPropValue, SCA_ILogicBrick, SCA_IObject, SCA_ISensor, SCA_IController, SCA_IActuator

@group Object: KX_GameObject, KX_LightObject, KX_Camera, BL_ArmatureObject

@group Animation: BL_ArmatureConstraint

@group Mesh: KX_MeshProxy, KX_PolyProxy, KX_VertexProxy

@group Shading: KX_PolygonMaterial, KX_BlenderMaterial, BL_Shader

@group Sensors: SCA_ActuatorSensor, SCA_AlwaysSensor, SCA_DelaySensor, SCA_JoystickSensor, SCA_KeyboardSensor, KX_MouseFocusSensor, SCA_MouseSensor, KX_NearSensor, KX_NetworkMessageSensor, SCA_PropertySensor, KX_RadarSensor, SCA_RandomSensor, KX_RaySensor, KX_TouchSensor, KX_ArmatureSensor

@group Actuators: SCA_2DFilterActuator, BL_ActionActuator, BL_ArmatureActuator, KX_SCA_AddObjectActuator, KX_CameraActuator, KX_ConstraintActuator, KX_SCA_DynamicActuator, KX_SCA_EndObjectActuator, KX_GameActuator, KX_IpoActuator, KX_NetworkMessageActuator, KX_ObjectActuator, KX_ParentActuator, SCA_PropertyActuator, SCA_RandomActuator, KX_SCA_ReplaceMeshActuator, KX_SceneActuator, BL_ShapeActionActuator, KX_SoundActuator, KX_StateActuator, KX_TrackToActuator, KX_VisibilityActuator

@group Controllers: SCA_ANDController, SCA_NANDController, SCA_NORController, SCA_ORController, SCA_PythonController, SCA_XNORController, SCA_XORController
"""
import GameLogic

class PyObjectPlus:
	"""
	PyObjectPlus base class of most other types in the Game Engine.
	
	@ivar invalid:	Test if the object has been freed by the game engine and is no longer valid.
					
					Normally this is not a problem but when storing game engine data in the GameLogic module,
					KX_Scenes or other KX_GameObjects its possible to hold a reference to invalid data.
					Calling an attribute or method on an invalid object will raise a SystemError.
					
					The invalid attribute allows testing for this case without exception handling.
	@type invalid:	bool
	"""
	
	def isA(game_type):
		"""
		Check if this is a type or a subtype game_type.

		@param game_type: the name of the type or the type its self from the L{GameTypes} module.
		@type game_type: string or type
		@return: True if this object is a type or a subtype of game_type.
		@rtype: bool
		"""

class CValue(PyObjectPlus):
	"""
	This class is a basis for other classes.
	@ivar name: The name of this CValue derived object (read-only).
	@type name: string
	@group Deprecated: getName
	"""
	def getName():
		"""
		Returns the name of the CValue.
		
		@deprecated: Use the L{name} attribute instead.
		@note: in most cases the CValue's subclasses will override this function.
		@rtype: string
		"""

class CPropValue(CValue):
	"""
	This class has no python functions
	"""
	pass

class SCA_ILogicBrick(CValue):
	"""
	Base class for all logic bricks.
	
	@ivar executePriority: This determines the order controllers are evaluated, and actuators are activated (lower priority is executed first).
	@type executePriority: int
	@ivar owner: The game object this logic brick is attached to (read-only).
	@type owner: L{KX_GameObject} or None in exceptional cases.
	@ivar name: The name of this logic brick (read-only).
	@type name: string
	"""

#{ Deprecated
	def getOwner():
		"""
		Gets the game object associated with this logic brick.
		
		@deprecated: Use the L{owner} attribute instead.
		@rtype: L{KX_GameObject}
		"""
	
	def setExecutePriority(priority):
		"""
		Sets the priority of this logic brick.
		
		This determines the order controllers are evaluated, and actuators are activated.
		Bricks with lower priority will be executed first.
		
		@deprecated: Use the L{executePriority} attribute instead.
		@type priority: integer
		@param priority: the priority of this logic brick.
		"""
	def getExecutePriority():
		"""
		Gets the execution priority of this logic brick.
		
		@deprecated: Use the L{executePriority} attribute instead.
		@rtype: integer
		@return: this logic bricks current priority.
		"""
#}

class SCA_IObject(CValue):
	"""
	This class has no python functions
	"""
	pass

class SCA_ISensor(SCA_ILogicBrick):
	"""
	Base class for all sensor logic bricks.
	
	@ivar usePosPulseMode: Flag to turn positive pulse mode on and off.
	@type usePosPulseMode: boolean
	@ivar useNegPulseMode: Flag to turn negative pulse mode on and off.
	@type useNegPulseMode: boolean
	@ivar frequency: The frequency for pulse mode sensors.
	@type frequency: int
	@ivar level: Option whether to detect level or edge transition when entering a state.
					It makes a difference only in case of logic state transition (state actuator).
					A level detector will immediately generate a pulse, negative or positive
					depending on the sensor condition, as soon as the state is activated.
					A edge detector will wait for a state change before generating a pulse.
					note: mutually exclusive with L{tap}, enabling will disable L{tap}.
	@type level: boolean
	@ivar tap: When enabled only sensors that are just activated will send a positive event,
					after this they will be detected as negative by the controllers.
					This will make a key thats held act as if its only tapped for an instant.
					note: mutually exclusive with L{level}, enabling will disable L{level}.
	@type tap: boolean
	@ivar invert: Flag to set if this sensor activates on positive or negative events.
	@type invert: boolean
	@ivar triggered: True if this sensor brick is in a positive state. (read-only)
	@type triggered: boolean
	@ivar positive: True if this sensor brick is in a positive state. (read-only)
	@type positive: boolean
	@ivar status: The status of the sensor. (read-only)
		KX_SENSOR_INACTIVE, KX_SENSOR_JUST_ACTIVATED, 
		KX_SENSOR_ACTIVE, KX_SENSOR_JUST_DEACTIVATED
		Note: this convenient attribute combines the values of triggered and positive attributes
	@type status: int from 0-3.
	"""
	
	def reset():
		"""
		Reset sensor internal state, effect depends on the type of sensor and settings.
		
		The sensor is put in its initial state as if it was just activated.
		"""
#{ Deprecated
	def isPositive():
		"""
		True if this sensor brick is in a positive state.
		
		@deprecated: use L{positive}
		"""
	
	def isTriggered():
		"""
		True if this sensor brick has triggered the current controller.
		
		@deprecated: use L{triggered}
		"""
	
	def getUsePosPulseMode():
		"""
		True if the sensor is in positive pulse mode.
		
		@deprecated: use L{usePosPulseMode}
		"""
	def setUsePosPulseMode(pulse):
		"""
		Sets positive pulse mode.
		
		@type pulse: boolean
		@param pulse: If True, will activate positive pulse mode for this sensor.
		@deprecated: use L{usePosPulseMode}
		"""
	def getFrequency():
		"""
		The frequency for pulse mode sensors.
		
		@rtype: integer
		@return: the pulse frequency in 1/50 sec.
		@deprecated: use L{frequency}
		"""
	def setFrequency(freq):
		"""
		Sets the frequency for pulse mode sensors.
		
		@type freq: integer
		@return: the pulse frequency in 1/50 sec.
		@deprecated: use L{frequency}
		"""
	def getUseNegPulseMode():
		"""
		True if the sensor is in negative pulse mode.
		
		@deprecated: use L{useNegPulseMode}
		"""
	def setUseNegPulseMode(pulse):
		"""
		Sets negative pulse mode.
		
		@type pulse: boolean
		@param pulse: If True, will activate negative pulse mode for this sensor.
		@deprecated: use L{useNegPulseMode}
		"""
	def getInvert():
		"""
		True if this sensor activates on negative events.
		
		@deprecated: use L{invert}
		"""
	def setInvert(invert):
		"""
		Sets if this sensor activates on positive or negative events.
		
		@type invert: boolean
		@param invert: true if activates on negative events; false if activates on positive events.
		@deprecated: use L{invert}
		"""
	def getLevel():
		"""
		Returns whether this sensor is a level detector or a edge detector.
		It makes a difference only in case of logic state transition (state actuator).
		A level detector will immediately generate a pulse, negative or positive
		depending on the sensor condition, as soon as the state is activated.
		A edge detector will wait for a state change before generating a pulse.
		
		@rtype: boolean
		@return: true if sensor is level sensitive, false if it is edge sensitive
		@deprecated: use L{level}
		"""
	def setLevel(level):
		"""
		Set whether to detect level or edge transition when entering a state.
		
		@param level: Detect level instead of edge? (KX_TRUE, KX_FALSE)
		@type level: boolean
		@deprecated: use L{level}
		"""
#}

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
	@ivar useHighPriority: When set the controller executes always before all other controllers that dont have this set.
	                note: Order of execution between high priority controllers is not guaranteed.
	@type useHighPriority: bool
	"""
#{ Deprecated
	def getState():
		"""
		Get the controllers state bitmask, this can be used with the GameObject's state to test if the the controller is active.
		This for instance will always be true however you could compare with a previous state to see when the state was activated.
		GameLogic.getCurrentController().state & GameLogic.getCurrentController().owner.state
		@deprecated: Use the L{state} property
		@rtype: int
		"""
	def getSensors():
		"""
		Gets a list of all sensors attached to this controller.
		@deprecated: use the L{sensors} property
		@rtype: list [L{SCA_ISensor}]
		"""
	def getSensor(name):
		"""
		Gets the named linked sensor.
		@deprecated: use the L{sensors}[name] property
		@type name: string
		@rtype: L{SCA_ISensor}
		"""
	def getActuators():
		"""
		Gets a list of all actuators linked to this controller.
		@deprecated: Use the L{actuators} property
		@rtype: list [L{SCA_IActuator}]
		"""
	def getActuator(name):
		"""
		Gets the named linked actuator.
		@deprecated: use the L{actuators}[name] property
		@type name: string
		@rtype: L{SCA_IActuator}
		"""
#}

class SCA_IActuator(SCA_ILogicBrick):
	"""
	Base class for all actuator logic bricks.
	"""

class BL_ActionActuator(SCA_IActuator):
	"""
	Action Actuators apply an action to an actor.
	
	@ivar action: The name of the action to set as the current action.
	@type action: string
	@ivar channelNames: A list of channel names that may be used with L{setChannel} and L{getChannel}
	@type channelNames: list of strings
	@ivar frameStart: Specifies the starting frame of the animation.
	@type frameStart: float
	@ivar frameEnd: Specifies the ending frame of the animation.
	@type frameEnd: float
	@ivar blendIn: Specifies the number of frames of animation to generate when making transitions between actions.
	@type blendIn: float
	@ivar priority: Sets the priority of this actuator. Actuators will lower
		                 priority numbers will override actuators with higher
		                 numbers.
	@type priority: integer
	@ivar frame: Sets the current frame for the animation.
	@type frame: float
	@ivar propName: Sets the property to be used in FromProp playback mode.
	@type propName: string
	@ivar blendTime: Sets the internal frame timer. This property must be in
						the range from 0.0 to blendIn.
	@type blendTime: float
	@ivar mode: The operation mode of the actuator. KX_ACTIONACT_PLAY, KX_ACTIONACT_PROPERTY, KX_ACTIONACT_FLIPPER, KX_ACTIONACT_LOOPSTOP, KX_ACTIONACT_LOOPEND
	@type mode: integer
	@ivar useContinue: The actions continue option, True or False.
					When True, the action will always play from where last left off,
					otherwise negative events to this actuator will reset it to its start frame.
	@type useContinue: boolean
	@ivar framePropName: The name of the property that is set to the current frame number.
	@type framePropName: string
	"""
	def setChannel(channel, matrix):
		"""
		Alternative to the 2 arguments, 4 arguments (channel, matrix, loc, size, quat) are also supported.
		
		@note: These values are relative to the bones rest position, currently the api has no way to get this info (which is annoying), but can be worked around by using bones with a rest pose that has no translation.
		@param channel: A string specifying the name of the bone channel, error raised if not in L{channelNames}.
		@type channel: string
		@param matrix: A 4x4 matrix specifying the overriding transformation
		               as an offset from the bone's rest position.
		@type matrix: list [[float]]
		"""

	def getChannel(channel):
		"""
		@param channel: A string specifying the name of the bone channel. error raised if not in L{channelNames}.
		@type channel: string
		@rtype: tuple
		@return: (loc, size, quat)
		"""

#{ Deprecated
	def setAction(action, reset = True):
		"""
		Sets the current action.
		@deprecated: use the L{action} property
		@param action: The name of the action to set as the current action.
		@type action: string
		@param reset: Optional parameter indicating whether to reset the
		              blend timer or not.  A value of 1 indicates that the
		              timer should be reset.  A value of 0 will leave it
		              unchanged.  If reset is not specified, the timer will
		              be reset.
		"""

	def setStart(start):
		"""
		Specifies the starting frame of the animation.
		@deprecated: Use the L{frameStart} property
		@param start: the starting frame of the animation
		@type start: float
		"""

	def setEnd(end):
		"""
		Specifies the ending frame of the animation.
		@deprecated: use the L{frameEnd} property 
		@param end: the ending frame of the animation
		@type end: float
		"""
	def setBlendin(blendin):
		"""
		Specifies the number of frames of animation to generate
		when making transitions between actions.
		@deprecated: use the L{blendIn} property
		@param blendin: the number of frames in transition.
		@type blendin: float
		"""

	def setPriority(priority):
		"""
		Sets the priority of this actuator.
		
		@deprecated: Use use the L{priority} property
		@param priority: Specifies the new priority.  Actuators will lower
		                 priority numbers will override actuators with higher
		                 numbers.
		@type priority: integer
		"""
	def setFrame(frame):
		"""
		Sets the current frame for the animation.
		
		@deprecated: use the L{frame} property
		@param frame: Specifies the new current frame for the animation
		@type frame: float
		"""

	def setProperty(prop):
		"""
		Sets the property to be used in FromProp playback mode.
		
		@deprecated: use the L{property} property
		@param prop: the name of the property to use.
		@type prop: string.
		"""

	def setBlendtime(blendtime):
		"""
		Sets the internal frame timer.
		
		Allows the script to directly modify the internal timer
		used when generating transitions between actions.  
		
		@deprecated: use the L{blendTime} property
		@param blendtime: The new time. This parameter must be in the range from 0.0 to 1.0.
		@type blendtime: float
		"""

	def setType(mode):
		"""
		Sets the operation mode of the actuator

		@deprecated: use the L{type} property
		@param mode: KX_ACTIONACT_PLAY, KX_ACTIONACT_PROPERTY, KX_ACTIONACT_FLIPPER, KX_ACTIONACT_LOOPSTOP, KX_ACTIONACT_LOOPEND
		@type mode: integer
		"""
	
	def setContinue(cont):
		"""
		Set the actions continue option True or False. see getContinue.

		@deprecated: use the L{useContinue} property
		@param cont: The continue option.
		@type cont: bool
		"""

	def getType():
		"""
		Returns the operation mode of the actuator

		@deprecated: use the L{type} property
		@rtype: integer
		@return: KX_ACTIONACT_PLAY, KX_ACTIONACT_PROPERTY, KX_ACTIONACT_FLIPPER, KX_ACTIONACT_LOOPSTOP, KX_ACTIONACT_LOOPEND
		"""

	def getContinue():
		"""
		When True, the action will always play from where last left off, otherwise negative events to this actuator will reset it to its start frame.

		@deprecated: use the L{useContinue} property
		@rtype: bool
		"""
	
	def getAction():
		"""
		getAction() returns the name of the action associated with this actuator.
		
		@deprecated: use the L{action} property
		@rtype: string
		"""
	
	def getStart():
		"""
		Returns the starting frame of the action.
		
		@deprecated: use the L{frameStart} property
		@rtype: float
		"""
	def getEnd():
		"""
		Returns the last frame of the action.
		
		@deprecated: use the L{frameEnd} property
		@rtype: float
		"""
	def getBlendin():
		"""
		Returns the number of interpolation animation frames to be generated when this actuator is triggered.
		
		@deprecated: use the L{blendIn} property
		@rtype: float
		"""
	def getPriority():
		"""
		Returns the priority for this actuator.  Actuators with lower Priority numbers will
		override actuators with higher numbers.
		
		@deprecated: use the L{priority} property
		@rtype: integer
		"""
	def getFrame():
		"""
		Returns the current frame number.
		
		@deprecated: use the L{frame} property
		@rtype: float
		"""
	def getProperty():
		"""
		Returns the name of the property to be used in FromProp mode.
		
		@deprecated: use the L{property} property
		@rtype: string
		"""
	def setFrameProperty(prop):
		"""
		@deprecated: use the L{framePropName} property
		@param prop: A string specifying the property of the object that will be updated with the action frame number.
		@type prop: string
		"""
	def getFrameProperty():
		"""
		Returns the name of the property that is set to the current frame number.
		
		@deprecated: use the L{framePropName} property
		@rtype: string
		"""
#}

class BL_Shader(PyObjectPlus):
	"""
	BL_Shader GLSL shaders.
	
	TODO - Description
	"""
	
	def setUniformfv(name, fList):
		"""
		Set a uniform with a list of float values
		
		@param name: the uniform name
		@type name: string
		
		@param fList: a list (2, 3 or 4 elements) of float values
		@type fList: list[float]
		"""

	def delSource():
		"""
		Clear the shader. Use this method before the source is changed with L{setSource}.
		"""
	def getFragmentProg():
		"""
		Returns the fragment program.
		
		@rtype: string
		@return: The fragment program.
		"""
	def getVertexProg():
		"""
		Get the vertex program.
		
		@rtype: string
		@return: The vertex program.
		"""
	def isValid():
		"""
		Check if the shader is valid.

		@rtype: bool
		@return: True if the shader is valid
		"""
	def setAttrib(enum):
		"""
		Set attribute location. (The parameter is ignored a.t.m. and the value of "tangent" is always used.)
		
		@param enum: attribute location value
		@type enum: integer
		"""
	def setNumberOfPasses( max_pass ):
		"""
		Set the maximum number of passes. Not used a.t.m.
		
		@param max_pass: the maximum number of passes
		@type max_pass: integer
		"""
	def setSampler(name, index):
		"""
		Set uniform texture sample index.
		
		@param name: Uniform name
		@type name: string

		@param index: Texture sample index.
		@type index: integer
		"""
	def setSource(vertexProgram, fragmentProgram):
		"""
		Set the vertex and fragment programs
		
		@param vertexProgram: Vertex program
		@type vertexProgram: string

		@param fragmentProgram: Fragment program
		@type fragmentProgram: string
		"""
	def setUniform1f(name, fx):
		"""
		Set a uniform with 1 float value.
		
		@param name: the uniform name
		@type name: string
		
		@param fx: Uniform value
		@type fx: float
		"""
	def setUniform1i(name, ix):
		"""
		Set a uniform with an integer value.
		
		@param name: the uniform name
		@type name: string

		@param ix: the uniform value
		@type ix: integer
		"""
	def setUniform2f(name, fx, fy):
		"""
		Set a uniform with 2 float values
		
		@param name: the uniform name
		@type name: string

		@param fx: first float value
		@type fx: float
		
		@param fy: second float value
		@type fy: float
		"""
	def setUniform2i(name, ix, iy):
		"""
		Set a uniform with 2 integer values
		
		@param name: the uniform name
		@type name: string

		@param ix: first integer value
		@type ix: integer
		
		@param iy: second integer value
		@type iy: integer
		"""
	def setUniform3f(name, fx,fy,fz):
		"""
		Set a uniform with 3 float values.
		
		@param name: the uniform name
		@type name: string

		@param fx: first float value
		@type fx: float
		
		@param fy: second float value
		@type fy: float

		@param fz: third float value
		@type fz: float
		"""
	def setUniform3i(name, ix,iy,iz):
		"""
		Set a uniform with 3 integer values
		
		@param name: the uniform name
		@type name: string

		@param ix: first integer value
		@type ix: integer
		
		@param iy: second integer value
		@type iy: integer
		
		@param iz: third integer value
		@type iz: integer
		"""
	def setUniform4f(name, fx,fy,fz,fw):
		"""
		Set a uniform with 4 float values.
		
		@param name: the uniform name
		@type name: string

		@param fx: first float value
		@type fx: float
		
		@param fy: second float value
		@type fy: float

		@param fz: third float value
		@type fz: float

		@param fw: fourth float value
		@type fw: float
		"""
	def setUniform4i(name, ix,iy,iz, iw):
		"""
		Set a uniform with 4 integer values
		
		@param name: the uniform name
		@type name: string

		@param ix: first integer value
		@type ix: integer
		
		@param iy: second integer value
		@type iy: integer
		
		@param iz: third integer value
		@type iz: integer
		
		@param iw: fourth integer value
		@type iw: integer
		"""
	def setUniformDef(name, type):
		"""
		Define a new uniform
		
		@param name: the uniform name
		@type name: string

		@param type: uniform type
		@type type: UNI_NONE, UNI_INT, UNI_FLOAT, UNI_INT2, UNI_FLOAT2,	UNI_INT3, UNI_FLOAT3, UNI_INT4,	UNI_FLOAT4,	UNI_MAT3, UNI_MAT4,	UNI_MAX
		"""
	def setUniformMatrix3(name, mat, transpose):
		"""
		Set a uniform with a 3x3 matrix value
		
		@param name: the uniform name
		@type name: string

		@param mat: A 3x3 matrix [[f,f,f], [f,f,f], [f,f,f]]
		@type mat: 3x3 matrix
		
		@param transpose: set to True to transpose the matrix
		@type transpose: bool
		"""
	def setUniformMatrix4(name, mat, transpose):
		"""
		Set a uniform with a 4x4 matrix value
		
		@param name: the uniform name
		@type name: string

		@param mat: A 4x4 matrix [[f,f,f,f], [f,f,f,f], [f,f,f,f], [f,f,f,f]]
		@type mat: 4x4 matrix
		
		@param transpose: set to True to transpose the matrix
		@type transpose: bool
		"""
	def setUniformiv(name, iList):
		"""
		Set a uniform with a list of integer values
		
		@param name: the uniform name
		@type name: string
		
		@param iList: a list (2, 3 or 4 elements) of integer values
		@type iList: list[integer]
		"""
	def validate():
		"""
		Validate the shader object.
		
		"""

class BL_ShapeActionActuator(SCA_IActuator):
	"""
	ShapeAction Actuators apply an shape action to an mesh object.\

	@ivar action: The name of the action to set as the current shape action.
	@type action: string
	@ivar frameStart: Specifies the starting frame of the shape animation.
	@type frameStart: float
	@ivar frameEnd: Specifies the ending frame of the shape animation.
	@type frameEnd: float
	@ivar blendIn: Specifies the number of frames of animation to generate when making transitions between actions.
	@type blendIn: float
	@ivar priority: Sets the priority of this actuator. Actuators will lower
		                 priority numbers will override actuators with higher
		                 numbers.
	@type priority: integer
	@ivar frame: Sets the current frame for the animation.
	@type frame: float
	@ivar propName: Sets the property to be used in FromProp playback mode.
	@type propName: string
	@ivar blendTime: Sets the internal frame timer. This property must be in
						the range from 0.0 to blendin.
	@type blendTime: float
	@ivar mode: The operation mode of the actuator.
					KX_ACTIONACT_PLAY, KX_ACTIONACT_PROPERTY, KX_ACTIONACT_FLIPPER,
					KX_ACTIONACT_LOOPSTOP, KX_ACTIONACT_LOOPEND
	@type mode: integer
	@ivar framePropName: The name of the property that is set to the current frame number.
	@type framePropName: string
	
	"""
#{ Deprecated
	def setAction(action, reset = True):
		"""
		Sets the current action.
		
		@deprecated: use the L{action} property
		@param action: The name of the action to set as the current action.
		@type action: string
		@param reset: Optional parameter indicating whether to reset the
		              blend timer or not.  A value of 1 indicates that the
		              timer should be reset.  A value of 0 will leave it
		              unchanged.  If reset is not specified, the timer will
		              be reset.
		"""

	def setStart(start):
		"""
		Specifies the starting frame of the animation.
		
		@deprecated: use the L{frameStart} property
		@param start: the starting frame of the animation
		@type start: float
		"""

	def setEnd(end):
		"""
		Specifies the ending frame of the animation.
		
		@deprecated: use the L{frameEnd} property
		@param end: the ending frame of the animation
		@type end: float
		"""
	def setBlendin(blendin):
		"""
		Specifies the number of frames of animation to generate
		when making transitions between actions.
		
		@deprecated: use the L{blendIn} property
		@param blendin: the number of frames in transition.
		@type blendin: float
		"""

	def setPriority(priority):
		"""
		Sets the priority of this actuator.
		
		@deprecated: use the L{priority} property
		@param priority: Specifies the new priority.  Actuators will lower
		                 priority numbers will override actuators with higher
		                 numbers.
		@type priority: integer
		"""
	def setFrame(frame):
		"""
		Sets the current frame for the animation.
		
		@deprecated: use the L{frame} property
		@param frame: Specifies the new current frame for the animation
		@type frame: float
		"""

	def setProperty(prop):
		"""
		Sets the property to be used in FromProp playback mode.
		
		@deprecated: use the L{property} property
		@param prop: the name of the property to use.
		@type prop: string.
		"""

	def setBlendtime(blendtime):
		"""
		Sets the internal frame timer.
		
		Allows the script to directly modify the internal timer
		used when generating transitions between actions.  
		
		@deprecated: use the L{blendTime} property
		@param blendtime: The new time. This parameter must be in the range from 0.0 to 1.0.
		@type blendtime: float
		"""

	def setType(mode):
		"""
		Sets the operation mode of the actuator

		@deprecated: use the L{type} property
		@param mode: KX_ACTIONACT_PLAY, KX_ACTIONACT_PROPERTY, KX_ACTIONACT_FLIPPER, KX_ACTIONACT_LOOPSTOP, KX_ACTIONACT_LOOPEND
		@type mode: integer
		"""
	
	def getType():
		"""
		Returns the operation mode of the actuator
		
		@deprecated: use the L{type} property
		@rtype: integer
		@return: KX_ACTIONACT_PLAY, KX_ACTIONACT_PROPERTY, KX_ACTIONACT_FLIPPER, KX_ACTIONACT_LOOPSTOP, KX_ACTIONACT_LOOPEND
		"""

	def getAction():
		"""
		getAction() returns the name of the action associated with this actuator.
		
		@deprecated: use the L{action} property
		@rtype: string
		"""
	
	def getStart():
		"""
		Returns the starting frame of the action.
		
		@deprecated: use the L{frameStart} property
		@rtype: float
		"""
	def getEnd():
		"""
		Returns the last frame of the action.
		
		@deprecated: use the L{frameEnd} property
		@rtype: float
		"""
	def getBlendin():
		"""
		Returns the number of interpolation animation frames to be generated when this actuator is triggered.
		
		@deprecated: use the L{blendIn} property
		@rtype: float
		"""
	def getPriority():
		"""
		Returns the priority for this actuator.  Actuators with lower Priority numbers will
		override actuators with higher numbers.
		
		@deprecated: use the L{priority} property
		@rtype: integer
		"""
	def getFrame():
		"""
		Returns the current frame number.
		
		@deprecated: use the L{frame} property
		@rtype: float
		"""
	def getProperty():
		"""
		Returns the name of the property to be used in FromProp mode.
		
		@deprecated: use the L{property} property
		@rtype: string
		"""
	def setFrameProperty(prop):
		"""
		@deprecated: use the L{framePropName} property
		@param prop: A string specifying the property of the object that will be updated with the action frame number.
		@type prop: string
		"""
	def getFrameProperty():
		"""
		Returns the name of the property that is set to the current frame number.
		
		@deprecated: use the L{framePropName} property
		@rtype: string
		"""
#}

class CListValue(CPropValue):
	"""
	CListValue
	
	This is a list like object used in the game engine internally that behaves similar to a python list in most ways.
	
	As well as the normal index lookup.
	C{val= clist[i]}
	
	CListValue supports string lookups.
	C{val= scene.objects["Cube"]}
	
	Other operations such as C{len(clist), list(clist), clist[0:10]} are also supported.
	"""
	def append(val):
		"""
		Add an item to the list (like pythons append)
		
		Warning: Appending values to the list can cause crashes when the list is used internally by the game engine.
		"""

	def count(val):
		"""
		Count the number of instances of a value in the list.
		
		@rtype: integer
		@return: number of instances
		"""
	def index(val):
		"""
		Return the index of a value in the list.
		
		@rtype: integer
		@return: The index of the value in the list.
		"""
	def reverse():
		"""
		Reverse the order of the list.
		"""
	def get(key, default=None):
		"""
		Return the value matching key, or the default value if its not found.
		@return: The key value or a default.
		"""
	def from_id(id):
		"""
		This is a funtion especially for the game engine to return a value with a spesific id.
		
		Since object names are not always unique, the id of an object can be used to get an object from the CValueList.
		
		Example.
			
		C{myObID = id(gameObject)}
		
		C{...}
		
		C{ob= scene.objects.from_id(myObID)}
		
		Where myObID is an int or long from the id function.
		
		This has the advantage that you can store the id in places you could not store a gameObject.
		
		Warning: the id is derived from a memory location and will be different each time the game engine starts.
		"""

class KX_BlenderMaterial(PyObjectPlus): # , RAS_IPolyMaterial)
	"""
	KX_BlenderMaterial
	
	"""
	
	def getShader():
		"""
		Returns the material's shader.
		
		@rtype: L{BL_Shader}
		@return: the material's shader
		"""

	def setBlending(src, dest):
		"""
		Set the pixel color arithmetic functions.
		
		@param src: Specifies how the red, green, blue,
					and alpha source blending factors are computed.
		@type src: 	GL_ZERO,
					GL_ONE,
					GL_SRC_COLOR,
					GL_ONE_MINUS_SRC_COLOR,
					GL_DST_COLOR,
					GL_ONE_MINUS_DST_COLOR,
					GL_SRC_ALPHA,
					GL_ONE_MINUS_SRC_ALPHA,
					GL_DST_ALPHA,
					GL_ONE_MINUS_DST_ALPHA,
					GL_SRC_ALPHA_SATURATE

		
		@param dest: Specifies how the red, green, blue,
                    and alpha destination blending factors are computed.
		@type dest: GL_ZERO,
					GL_ONE,
					GL_SRC_COLOR,
					GL_ONE_MINUS_SRC_COLOR,
					GL_DST_COLOR,
					GL_ONE_MINUS_DST_COLOR,
					GL_SRC_ALPHA,
					GL_ONE_MINUS_SRC_ALPHA,
					GL_DST_ALPHA,
					GL_ONE_MINUS_DST_ALPHA,
					GL_SRC_ALPHA_SATURATE

		"""
	def getMaterialIndex():
		"""
		Returns the material's index.
		
		@rtype: integer
		@return: the material's index
		"""

class KX_CameraActuator(SCA_IActuator):
	"""
	Applies changes to a camera.
	
	@ivar min: minimum distance to the target object maintained by the actuator
	@type min: float
	@ivar max: maximum distance to stay from the target object
	@type max: float
	@ivar height: height to stay above the target object
	@type height: float
	@ivar useXY: axis this actuator is tracking, true=X, false=Y
	@type useXY: boolean
	@ivar object: the object this actuator tracks.
	@type object: KX_GameObject or None
	@author: snail
	"""
#{ Deprecated
	def getObject(name_only = 1):
		"""
		Returns the name of the object this actuator tracks.
		
		@deprecated: Use the L{object} attribute instead.
		@type name_only: bool
		@param name_only: optional argument, when 0 return a KX_GameObject
		@rtype: string, KX_GameObject or None if no object is set
		"""
	
	def setObject(target):
		"""
		Sets the object this actuator tracks.
		
		@deprecated: Use the L{object} attribute instead.
		@param target: the object to track.
		@type target: L{KX_GameObject}, string or None
		"""
	
	def getMin():
		"""
		Returns the minimum distance to target maintained by the actuator.
		
		@deprecated: Use the L{min} attribute instead.
		@rtype: float
		"""
	
	def setMin(distance):
		"""
		Sets the minimum distance to the target object maintained by the
		actuator.
		
		@deprecated: Use the L{min} attribute instead.
		@param distance: The minimum distance to maintain.
		@type distance: float
		"""
		
	def getMax():
		"""
		Gets the maximum distance to stay from the target object.
		
		@deprecated: Use the L{max} attribute instead.
		@rtype: float
		"""
	
	def setMax(distance):
		"""
		Sets the maximum distance to stay from the target object.
		
		@deprecated: Use the L{max} attribute instead.
		@param distance: The maximum distance to maintain.
		@type distance: float
		"""

	def getHeight():
		"""
		Returns the height to stay above the target object.
		
		@deprecated: Use the L{height} attribute instead.
		@rtype: float
		"""
	
	def setHeight(height):
		"""
		Sets the height to stay above the target object.
		
		@deprecated: Use the L{height} attribute instead.
		@type height: float
		@param height: The height to stay above the target object.
		"""
	
	def setXY(xaxis):
		"""
		Sets the axis to get behind.
		
		@deprecated: Use the L{useXY} attribute instead.
		@param xaxis: False to track Y axis, True to track X axis.
		@type xaxis: boolean
		"""

	def getXY():
		"""
		Returns the axis this actuator is tracking.
		
		@deprecated: Use the L{useXY} attribute instead.
		@return: True if tracking X axis, False if tracking Y axis.
		@rtype: boolean
		"""
#}

class KX_ConstraintActuator(SCA_IActuator):
	"""
	A constraint actuator limits the position, rotation, distance or orientation of an object.
	
	Properties:
	
	@ivar damp: time constant of the constraint expressed in frame (not use by Force field constraint)
	@type damp: integer
	
	@ivar rotDamp: time constant for the rotation expressed in frame (only for the distance constraint)
	               0 = use damp for rotation as well
	@type rotDamp: integer
	
	@ivar direction: the reference direction in world coordinate for the orientation constraint
	@type direction: 3-tuple of float: [x,y,z]
	
	@ivar option: Binary combination of the following values:
				Applicable to Distance constraint:
					- KX_ACT_CONSTRAINT_NORMAL    (  64) : Activate alignment to surface
					- KX_ACT_CONSTRAINT_DISTANCE  ( 512) : Activate distance control
					- KX_ACT_CONSTRAINT_LOCAL		(1024) : direction of the ray is along the local axis
				Applicable to Force field constraint:					
					- KX_ACT_CONSTRAINT_DOROTFH   (2048) : Force field act on rotation as well
				Applicable to both:
					- KX_ACT_CONSTRAINT_MATERIAL  ( 128) : Detect material rather than property
					- KX_ACT_CONSTRAINT_PERMANENT ( 256) : No deactivation if ray does not hit target
	@type option: integer
	
	@ivar time: activation time of the actuator. The actuator disables itself after this many frame.
		        If set to 0, the actuator is not limited in time.
	@type time: integer
	
	@ivar propName: the name of the property or material for the ray detection of the distance constraint.
	@type propName: string
	
	@ivar min: The lower bound of the constraint
	           For the rotation and orientation constraint, it represents radiant
	@type min: float
	
	@ivar distance: the target distance of the distance constraint
	@type distance: float
	
	@ivar max: the upper bound of the constraint.
	           For rotation and orientation constraints, it represents radiant.
	@type max: float
	
	@ivar rayLength: the length of the ray of the distance constraint.
	@type rayLength: float
	
	@ivar limit: type of constraint, use one of the following constant:
	              KX_ACT_CONSTRAINT_LOCX  ( 1) : limit X coord
	              KX_ACT_CONSTRAINT_LOCY  ( 2) : limit Y coord
	              KX_ACT_CONSTRAINT_LOCZ  ( 3) : limit Z coord
	              KX_ACT_CONSTRAINT_ROTX  ( 4) : limit X rotation
	              KX_ACT_CONSTRAINT_ROTY  ( 5) : limit Y rotation
	              KX_ACT_CONSTRAINT_ROTZ  ( 6) : limit Z rotation
	              KX_ACT_CONSTRAINT_DIRPX ( 7) : set distance along positive X axis
	              KX_ACT_CONSTRAINT_DIRPY ( 8) : set distance along positive Y axis
	              KX_ACT_CONSTRAINT_DIRPZ ( 9) : set distance along positive Z axis
	              KX_ACT_CONSTRAINT_DIRNX (10) : set distance along negative X axis
	              KX_ACT_CONSTRAINT_DIRNY (11) : set distance along negative Y axis
	              KX_ACT_CONSTRAINT_DIRNZ (12) : set distance along negative Z axis
	              KX_ACT_CONSTRAINT_ORIX  (13) : set orientation of X axis
	              KX_ACT_CONSTRAINT_ORIY  (14) : set orientation of Y axis
	              KX_ACT_CONSTRAINT_ORIZ  (15) : set orientation of Z axis
	              KX_ACT_CONSTRAINT_FHPX  (16) : set force field along positive X axis
	              KX_ACT_CONSTRAINT_FHPY  (17) : set force field along positive Y axis
	              KX_ACT_CONSTRAINT_FHPZ  (18) : set force field along positive Z axis
	              KX_ACT_CONSTRAINT_FHNX  (19) : set force field along negative X axis
	              KX_ACT_CONSTRAINT_FHNY  (20) : set force field along negative Y axis
	              KX_ACT_CONSTRAINT_FHNZ  (21) : set force field along negative Z axis
	@type limit: integer
	"""
#{ Deprecated
	def setDamp(time):
		"""
		Sets the time this constraint is delayed.
		
		@param time: The number of frames to delay.  
		             Negative values are ignored.
		@type time: integer
		"""
	def getDamp():
		"""
		Returns the damping time of the constraint.
		
		@rtype: integer
		"""
	def setMin(lower):
		"""
		Sets the lower bound of the constraint.
		
		For rotational and orientation constraints, lower is specified in degrees.
		
		@type lower: float
		"""
	def getMin():
		"""
		Gets the lower bound of the constraint.
		
		For rotational and orientation constraints, the lower bound is returned in radians.
		
		@rtype: float
		"""
	def setMax(upper):
		"""
		Sets the upper bound of the constraint.
		
		For rotational and orientation constraints, upper is specified in degrees.
		
		@type upper: float
		"""
	def getMax():
		"""
		Gets the upper bound of the constraint.
		
		For rotational and orientation constraints, the upper bound is returned in radians.
		
		@rtype: float
		"""
	def setLimit(limit):
		"""
		Sets the type of constraint.
		
		See module L{GameLogic} for valid constraint types.
		
		@param limit:
			Position constraints: KX_CONSTRAINTACT_LOCX, KX_CONSTRAINTACT_LOCY, KX_CONSTRAINTACT_LOCZ
			Rotation constraints: KX_CONSTRAINTACT_ROTX, KX_CONSTRAINTACT_ROTY or KX_CONSTRAINTACT_ROTZ
			Distance contraints: KX_ACT_CONSTRAINT_DIRPX, KX_ACT_CONSTRAINT_DIRPY, KX_ACT_CONSTRAINT_DIRPZ, KX_ACT_CONSTRAINT_DIRNX, KX_ACT_CONSTRAINT_DIRNY, KX_ACT_CONSTRAINT_DIRNZ
			Orientation constraints: KX_ACT_CONSTRAINT_ORIX, KX_ACT_CONSTRAINT_ORIY, KX_ACT_CONSTRAINT_ORIZ
		"""
	def getLimit():
		"""
		Gets the type of constraint.
		
		See module L{GameLogic} for valid constraints.
		
		@return:
			Position constraints: KX_CONSTRAINTACT_LOCX, KX_CONSTRAINTACT_LOCY, KX_CONSTRAINTACT_LOCZ, 
			Rotation constraints: KX_CONSTRAINTACT_ROTX, KX_CONSTRAINTACT_ROTY, KX_CONSTRAINTACT_ROTZ,
			Distance contraints: KX_ACT_CONSTRAINT_DIRPX, KX_ACT_CONSTRAINT_DIRPY, KX_ACT_CONSTRAINT_DIRPZ, KX_ACT_CONSTRAINT_DIRNX, KX_ACT_CONSTRAINT_DIRNY, KX_ACT_CONSTRAINT_DIRNZ,
			Orientation constraints: KX_ACT_CONSTRAINT_ORIX, KX_ACT_CONSTRAINT_ORIY, KX_ACT_CONSTRAINT_ORIZ
		"""
	def setRotDamp(duration):
		"""
		Sets the time constant of the orientation constraint.
		
		@param duration: If the duration is negative, it is set to 0.
		@type duration: integer
		"""
	def getRotDamp():
		""" 
		Returns the damping time for application of the constraint.
		
		@rtype: integer
		"""
	def setDirection(vector):
		"""
		Sets the reference direction in world coordinate for the orientation constraint
		
		@type vector: 3-tuple
		"""
	def getDirection():
		"""
		Returns the reference direction of the orientation constraint in world coordinate.
		
		@rtype: 3-tuple
		"""
	def setOption(option):
		"""
		Sets several options of the distance constraint.
		
		@type option: integer
		@param option: Binary combination of the following values:
		               64  : Activate alignment to surface
		               128 : Detect material rather than property
		               256 : No deactivation if ray does not hit target
		               512 : Activate distance control
		"""
	def getOption():
		"""
		Returns the option parameter.
		
		@rtype: integer
		"""
	def setTime(duration):
		"""
		Sets the activation time of the actuator.
		
		@type duration: integer
		@param duration: The actuator disables itself after this many frame.
		                 If set to 0 or negative, the actuator is not limited in time.
		"""
	def getTime():
		"""
		Returns the time parameter.
		
		@rtype: integer
		"""
	def setProperty(property):
		"""
		Sets the name of the property or material for the ray detection of the distance constraint.
		
		@type property: string
		@param property: If empty, the ray will detect any collisioning object.
		"""
	def getProperty():
		"""
		Returns the property parameter.
		
		@rtype: string
		"""
	def setDistance(distance):
		"""
		Sets the target distance in distance constraint.
		
		@type distance: float
		"""
	def getDistance():
		"""
		Returns the distance parameter.
		
		@rtype: float
		"""
	def setRayLength(length):
		"""
		Sets the maximum ray length of the distance constraint.
		
		@type length: float
		"""
	def getRayLength():
		"""
		Returns the length of the ray
		
		@rtype: float
		"""
#}

class KX_ConstraintWrapper(PyObjectPlus):
	"""
	KX_ConstraintWrapper
	
	"""
	def getConstraintId(val):
		"""
		Returns the contraint's ID
		
		@rtype: integer
		@return: the constraint's ID
		"""

class KX_GameActuator(SCA_IActuator):
	"""
	The game actuator loads a new .blend file, restarts the current .blend file or quits the game.
	
	Properties:
	
	@ivar fileName: the new .blend file to load
	@type fileName: string.
	@ivar mode: The mode of this actuator
	@type mode: Constant in...
				- L{GameLogic.KX_GAME_LOAD}
				- L{GameLogic.KX_GAME_START}
				- L{GameLogic.KX_GAME_RESTART}
				- L{GameLogic.KX_GAME_QUIT}
				- L{GameLogic.KX_GAME_SAVECFG}
				- L{GameLogic.KX_GAME_LOADCFG}
	"""
#{ Deprecated
	def getFile():
		"""
		Returns the filename of the new .blend file to load.
		
		@deprecated: use the L{fileName} property
		@rtype: string
		"""
	def setFile(filename):
		"""
		Sets the new .blend file to load.
		
		@deprecated: use the L{fileName} property
		@param filename: The file name this actuator will load.
		@type filename: string
		"""
#}

class KX_GameObject(SCA_IObject):
	"""
	All game objects are derived from this class.
	
	Properties assigned to game objects are accessible as attributes of this class.
		- note: Calling ANY method or attribute on an object that has been removed from a scene will raise a SystemError, if an object may have been removed since last accessing it use the L{invalid} attribute to check.

	@ivar name: The object's name. (read-only)
	@type name: string.
	@ivar mass: The object's mass
		- note: The object must have a physics controller for the mass to be applied, otherwise the mass value will be returned as 0.0
	@type mass: float
	@ivar linVelocityMin: Enforces the object keeps moving at a minimum velocity.
		- note: Applies to dynamic and rigid body objects only.
		- note: A value of 0.0 disables this option.
		- note: While objects are stationary the minimum velocity will not be applied.
	@type linVelocityMin: float
	@ivar linVelocityMax: Clamp the maximum linear velocity to prevent objects moving beyond a set speed.
		- note: Applies to dynamic and rigid body objects only.
		- note: A value of 0.0 disables this option (rather then setting it stationary).
	@type linVelocityMax: float
	@ivar localInertia: the object's inertia vector in local coordinates. Read only.
	@type localInertia: list [ix, iy, iz]
	@ivar parent: The object's parent object. (read-only)
	@type parent: L{KX_GameObject} or None
	@ivar visible: visibility flag.
		- note: Game logic will still run for invisible objects.
	@type visible: boolean
	@ivar color: The object color of the object
	@type color: list [r, g, b, a]
	@ivar occlusion: occlusion capability flag.
	@type occlusion: boolean
	@ivar position: The object's position. 

					deprecated: use L{localPosition} and L{worldPosition}
	@type position: list [x, y, z] On write: local position, on read: world position
	@ivar orientation: The object's orientation. 3x3 Matrix. You can also write a Quaternion or Euler vector.

					   deprecated: use L{localOrientation} and L{worldOrientation}
	@type orientation: 3x3 Matrix [[float]] On write: local orientation, on read: world orientation
	@ivar scaling: The object's scaling factor. list [sx, sy, sz]

				   deprecated: use L{localScale} and L{worldScale}
	@type scaling: list [sx, sy, sz] On write: local scaling, on read: world scaling
	@ivar localOrientation: The object's local orientation. 3x3 Matrix. You can also write a Quaternion or Euler vector.
	@type localOrientation: 3x3 Matrix [[float]]
	@ivar worldOrientation: The object's world orientation.
	@type worldOrientation: 3x3 Matrix [[float]]
	@ivar localScale: The object's local scaling factor.
	@type localScale: list [sx, sy, sz]
	@ivar worldScale: The object's world scaling factor. Read-only
	@type worldScale: list [sx, sy, sz]
	@ivar localPosition: The object's local position.
	@type localPosition: list [x, y, z]
	@ivar worldPosition: The object's world position. 
	@type worldPosition: list [x, y, z]
	@ivar timeOffset: adjust the slowparent delay at runtime.
	@type timeOffset: float
	@ivar state: the game object's state bitmask, using the first 30 bits, one bit must always be set.
	@type state: int
	@ivar meshes: a list meshes for this object.
		- note: Most objects use only 1 mesh.
		- note: Changes to this list will not update the KX_GameObject.
	@type meshes: list of L{KX_MeshProxy}
	@ivar sensors: a sequence of L{SCA_ISensor} objects with string/index lookups and iterator support.
		- note: This attribute is experemental and may be removed (but probably wont be).
		- note: Changes to this list will not update the KX_GameObject.
	@type sensors: list
	@ivar controllers: a sequence of L{SCA_IController} objects with string/index lookups and iterator support.
		- note: This attribute is experemental and may be removed (but probably wont be).
		- note: Changes to this list will not update the KX_GameObject.
	@type controllers: list of L{SCA_ISensor}.
	@ivar actuators: a list of L{SCA_IActuator} with string/index lookups and iterator support.
		- note: This attribute is experemental and may be removed (but probably wont be).
		- note: Changes to this list will not update the KX_GameObject.
	@type actuators: list
	@ivar attrDict: get the objects internal python attribute dictionary for direct (faster) access.
	@type attrDict: dict
	@ivar children: direct children of this object, (read-only).
	@type children: L{CListValue} of L{KX_GameObject}'s
	@ivar childrenRecursive: all children of this object including childrens children, (read-only).
	@type childrenRecursive: L{CListValue} of L{KX_GameObject}'s
	@group Deprecated: getPosition, setPosition, setWorldPosition, getOrientation, setOrientation, getState, setState, getParent, getVisible, getMass, getMesh, getChildren, getChildrenRecursive
	@group Property Access: get, attrDict, getPropertyNames
	"""
	def endObject():
		"""
		Delete this object, can be used in place of the EndObject Actuator.
		The actual removal of the object from the scene is delayed.
		"""	
	def replaceMesh(mesh, useDisplayMesh=True, usePhysicsMesh=False):
		"""
		Replace the mesh of this object with a new mesh. This works the same was as the actuator.
		@type mesh: L{KX_MeshProxy} or mesh name
		@type useDisplayMesh: bool
		@param useDisplayMesh: when enabled the display mesh will be replaced (optional argument).
		@type usePhysicsMesh: bool
		@param usePhysicsMesh: when enabled the physics mesh will be replaced (optional argument).
		"""	
	def getVisible():
		"""
		Gets the game object's visible flag.
		
		@deprecated: use L{visible}
		@rtype: boolean
		"""	
	def setVisible(visible, recursive):
		"""
		Sets the game object's visible flag.
		
		@type visible: boolean
		@type recursive: boolean
		@param recursive: optional argument to set all childrens visibility flag too.
		"""
	def setOcclusion(occlusion, recursive):
		"""
		Sets the game object's occlusion capability.
		
		@type occlusion: boolean
		@type recursive: boolean
		@param recursive: optional argument to set all childrens occlusion flag too.
		"""
	def getState():
		"""
		Gets the game object's state bitmask.
		
		@deprecated: use L{state}
		@rtype: int
		@return: the objects state.
		"""	
	def setState(state):
		"""
		Sets the game object's state flag.
		The bitmasks for states from 1 to 30 can be set with (1<<0, 1<<1, 1<<2 ... 1<<29) 
		@deprecated: use L{state}
		@type state: integer
		"""
	def setPosition(pos):
		"""
		Sets the game object's position.
		Global coordinates for root object, local for child objects.
		
		@deprecated: use L{localPosition}
		@type pos: [x, y, z]
		@param pos: the new position, in local coordinates.
		"""
	def setWorldPosition(pos):
		"""
		Sets the game object's position in world coordinates regardless if the object is root or child.
		
		@deprecated: use L{worldPosition}
		@type pos: [x, y, z]
		@param pos: the new position, in world coordinates.
		"""
	def getPosition():
		"""
		Gets the game object's position.
		
		@deprecated: use L{worldPosition}
		@rtype: list [x, y, z]
		@return: the object's position in world coordinates.
		"""
	def setOrientation(orn):
		"""
		Sets the game object's orientation.
		
		@deprecated: use L{localOrientation}
		@type orn: 3x3 rotation matrix, or Quaternion.
		@param orn: a rotation matrix specifying the new rotation.
		@note: When using this matrix with Blender.Mathutils.Matrix() types, it will need to be transposed.
		"""
	def alignAxisToVect(vect, axis, factor):
		"""
		Aligns any of the game object's axis along the given vector.
		
		@type vect: 3d vector.
		@param vect: a vector to align the axis.
		@type axis: integer.
		@param axis:The axis you want to align
					- 0: X axis
					- 1: Y axis
					- 2: Z axis (default) 
		@type factor: float
		@param factor: Only rotate a feaction of the distance to the target vector (0.0 - 1.0)
		"""
	def getAxisVect(vect):
		"""
		Returns the axis vector rotates by the objects worldspace orientation.
		This is the equivalent of multiplying the vector by the orientation matrix.
		
		@type vect: 3d vector.
		@param vect: a vector to align the axis.
		@rtype: 3d vector.
		@return: The vector in relation to the objects rotation.

		"""
	def getOrientation():
		"""
		Gets the game object's orientation.
		
		@deprecated: use L{worldOrientation}
		@rtype: 3x3 rotation matrix
		@return: The game object's rotation matrix
		@note: When using this matrix with Blender.Mathutils.Matrix() types, it will need to be transposed.
		"""
	def applyMovement(movement, local = 0):
		"""
		Sets the game object's movement.
		
		@type movement: 3d vector.
		@param movement: movement vector.
		@type local: boolean
		@param local: - False: you get the "global" movement ie: relative to world orientation (default).
		              - True: you get the "local" movement ie: relative to object orientation.
		"""	
	def applyRotation(rotation, local = 0):
		"""
		Sets the game object's rotation.
		
		@type rotation: 3d vector.
		@param rotation: rotation vector.
		@type local: boolean
		@param local: - False: you get the "global" rotation ie: relative to world orientation (default).
					  - True: you get the "local" rotation ie: relative to object orientation.
		"""	
	def applyForce(force, local = 0):
		"""
		Sets the game object's force.
		
		This requires a dynamic object.
		
		@type force: 3d vector.
		@param force: force vector.
		@type local: boolean
		@param local: - False: you get the "global" force ie: relative to world orientation (default).
					  - True: you get the "local" force ie: relative to object orientation.
		"""	
	def applyTorque(torque, local = 0):
		"""
		Sets the game object's torque.
		
		This requires a dynamic object.
		
		@type torque: 3d vector.
		@param torque: torque vector.
		@type local: boolean
		@param local: - False: you get the "global" torque ie: relative to world orientation (default).
					  - True: you get the "local" torque ie: relative to object orientation.
		"""
	def getLinearVelocity(local = 0):
		"""
		Gets the game object's linear velocity.
		
		This method returns the game object's velocity through it's centre of mass,
		ie no angular velocity component.
		
		@type local: boolean
		@param local: - False: you get the "global" velocity ie: relative to world orientation (default).
		              - True: you get the "local" velocity ie: relative to object orientation.
		@rtype: list [vx, vy, vz]
		@return: the object's linear velocity.
		"""
	def setLinearVelocity(velocity, local = 0):
		"""
		Sets the game object's linear velocity.
		
		This method sets game object's velocity through it's centre of mass,
		ie no angular velocity component.
		
		This requires a dynamic object.
		
		@type velocity: 3d vector.
		@param velocity: linear velocity vector.
		@type local: boolean
		@param local: - False: you get the "global" velocity ie: relative to world orientation (default).
		              - True: you get the "local" velocity ie: relative to object orientation.
		"""
	def getAngularVelocity(local = 0):
		"""
		Gets the game object's angular velocity.
		
		@type local: boolean
		@param local: - False: you get the "global" velocity ie: relative to world orientation (default).
		              - True: you get the "local" velocity ie: relative to object orientation.
		@rtype: list [vx, vy, vz]
		@return: the object's angular velocity.
		"""
	def setAngularVelocity(velocity, local = 0):
		"""
		Sets the game object's angular velocity.
		
		This requires a dynamic object.
		
		@type velocity: 3d vector.
		@param velocity: angular velocity vector.
		@type local: boolean
		@param local: - False: you get the "global" velocity ie: relative to world orientation (default).
		              - True: you get the "local" velocity ie: relative to object orientation.
		"""
	def getVelocity(point):
		"""
		Gets the game object's velocity at the specified point.
		
		Gets the game object's velocity at the specified point, including angular
		components.
		
		@type point: list [x, y, z]
		@param point: the point to return the velocity for, in local coordinates. (optional: default = [0, 0, 0])
		@rtype: list [vx, vy, vz]
		@return: the velocity at the specified point.
		"""
	def getMass():
		"""
		Gets the game object's mass.
		
		@deprecated: use L{mass}
		@rtype: float
		@return: the object's mass.
		"""
	def getReactionForce():
		"""
		Gets the game object's reaction force.
		
		The reaction force is the force applied to this object over the last simulation timestep.
		This also includes impulses, eg from collisions.
		
		(B{This is not implimented for bullet physics at the moment})
		
		@rtype: list [fx, fy, fz]
		@return: the reaction force of this object.
		"""
	def applyImpulse(point, impulse):
		"""
		Applies an impulse to the game object.
		
		This will apply the specified impulse to the game object at the specified point.
		If point != getPosition(), applyImpulse will also change the object's angular momentum.
		Otherwise, only linear momentum will change.
		
		@type point: list [x, y, z]
		@param point: the point to apply the impulse to (in world coordinates)
		"""
	def suspendDynamics():
		"""
		Suspends physics for this object.
		"""
	def restoreDynamics():
		"""
		Resumes physics for this object.
		@Note: The objects linear velocity will be applied from when the dynamics were suspended.
		"""
	def enableRigidBody():
		"""
		Enables rigid body physics for this object.
		
		Rigid body physics allows the object to roll on collisions.
		@Note: This is not working with bullet physics yet.
		"""
	def disableRigidBody():
		"""
		Disables rigid body physics for this object.
		@Note: This is not working with bullet physics yet. The angular is removed but rigid body physics can still rotate it later.
		"""
	def getParent():
		"""
		Gets this object's parent.
		
		@deprecated: use L{parent}
		@rtype: L{KX_GameObject}
		@return: this object's parent object, or None if this object has no parent.
		"""
	def setParent(parent,compound,ghost):
		"""
		Sets this object's parent. 
		Control the shape status with the optional compound and ghost parameters:
		compound=1: the object shape should be added to the parent compound shape (default)
		compound=0: the object should keep its individual shape. 
		In that case you can control if it should be ghost or not:
		ghost=1 if the object should be made ghost while parented (default)
		ghost=0 if the object should be solid while parented 
		Note: if the object type is sensor, it stays ghost regardless of ghost parameter
		
		@type parent: L{KX_GameObject}
		@param parent: new parent object.
		@type compound: int
		@param compound: whether the shape should be added to the parent compound shape
		@type ghost: int
		@param ghost: whether the object should be ghost while parented
		"""
	def removeParent():
		"""
		Removes this objects parent.
		"""
	def getChildren():
		"""
		Return a list of immediate children of this object.
		@rtype: L{CListValue} of L{KX_GameObject}
		@return: a list of all this objects children.
		"""
	def getChildrenRecursive():
		"""
		Return a list of children of this object, including all their childrens children.
		@rtype: L{CListValue} of L{KX_GameObject}
		@return: a list of all this objects children recursivly.
		"""
	def getMesh(mesh):
		"""
		Gets the mesh object for this object.
		
		@type mesh: integer
		@param mesh: the mesh object to return (optional: default mesh = 0)
		@rtype: L{KX_MeshProxy}
		@return: the first mesh object associated with this game object, or None if this object has no meshs.
		"""
	def getPhysicsId():
		"""
		Returns the user data object associated with this game object's physics controller.
		"""
	def getPropertyNames():
		"""
		Gets a list of all property names.
		@rtype: list
		@return: All property names for this object.
		"""
	def getDistanceTo(other):
		"""
		Returns the distance to another object or point.
		
		@param other: a point or another L{KX_GameObject} to measure the distance to.
		@type other: L{KX_GameObject} or list [x, y, z]
		@rtype: float
		"""
	def getVectTo(other):
		"""
		Returns the vector and the distance to another object or point.
		The vector is normalized unless the distance is 0, in which a NULL vector is returned.
		
		@param other: a point or another L{KX_GameObject} to get the vector and distance to.
		@type other: L{KX_GameObject} or list [x, y, z]
		@rtype: 3-tuple (float, 3-tuple (x,y,z), 3-tuple (x,y,z))
		@return: (distance, globalVector(3), localVector(3))
		"""
	def rayCastTo(other,dist,prop):
		"""
		Look towards another point/object and find first object hit within dist that matches prop.

		The ray is always casted from the center of the object, ignoring the object itself.
		The ray is casted towards the center of another object or an explicit [x,y,z] point.
		Use rayCast() if you need to retrieve the hit point 

		@param other: [x,y,z] or object towards which the ray is casted
		@type other: L{KX_GameObject} or 3-tuple
		@param dist: max distance to look (can be negative => look behind); 0 or omitted => detect up to other
		@type dist: float
		@param prop: property name that object must have; can be omitted => detect any object
		@type prop: string
		@rtype: L{KX_GameObject}
		@return: the first object hit or None if no object or object does not match prop
		"""
	def rayCast(objto,objfrom,dist,prop,face,xray,poly):
		"""
		Look from a point/object to another point/object and find first object hit within dist that matches prop.
		if poly is 0, returns a 3-tuple with object reference, hit point and hit normal or (None,None,None) if no hit.
		if poly is 1, returns a 4-tuple with in addition a L{KX_PolyProxy} as 4th element.
		if poly is 2, returns a 5-tuple with in addition a 2D vector with the UV mapping of the hit point as 5th element.
		
		Ex::
			# shoot along the axis gun-gunAim (gunAim should be collision-free)
			ob,point,normal = gun.rayCast(gunAim,None,50)
			if ob:
				# hit something

		Notes:				
		The ray ignores the object on which the method is called.
		It is casted from/to object center or explicit [x,y,z] points.
		
		The face paremeter determines the orientation of the normal:: 
		  0 => hit normal is always oriented towards the ray origin (as if you casted the ray from outside)
		  1 => hit normal is the real face normal (only for mesh object, otherwise face has no effect)
		  
		The ray has X-Ray capability if xray parameter is 1, otherwise the first object hit (other than self object) stops the ray.
		The prop and xray parameters interact as follow::
		    prop off, xray off: return closest hit or no hit if there is no object on the full extend of the ray.
		    prop off, xray on : idem.
		    prop on,  xray off: return closest hit if it matches prop, no hit otherwise.
		    prop on,  xray on : return closest hit matching prop or no hit if there is no object matching prop on the full extend of the ray.
		The L{KX_PolyProxy} 4th element of the return tuple when poly=1 allows to retrieve information on the polygon hit by the ray.
		If there is no hit or the hit object is not a static mesh, None is returned as 4th element. 
		
		The ray ignores collision-free objects and faces that dont have the collision flag enabled, you can however use ghost objects.

		@param objto: [x,y,z] or object to which the ray is casted
		@type objto: L{KX_GameObject} or 3-tuple
		@param objfrom: [x,y,z] or object from which the ray is casted; None or omitted => use self object center
		@type objfrom: L{KX_GameObject} or 3-tuple or None
		@param dist: max distance to look (can be negative => look behind); 0 or omitted => detect up to to
		@type dist: float
		@param prop: property name that object must have; can be omitted or "" => detect any object
		@type prop: string
		@param face: normal option: 1=>return face normal; 0 or omitted => normal is oriented towards origin
		@type face: int
		@param xray: X-ray option: 1=>skip objects that don't match prop; 0 or omitted => stop on first object
		@type xray: int
		@param poly: polygon option: 0,1 or 2 to return a 3-, 4- or 5-tuple with information on the face hit
		    0 or omitted=> return value is a 3-tuple (object, hitpoint, hitnormal) or (None,None,None) if no hit
		    1=>return value is a 4-tuple and the 4th element is a L{KX_PolyProxy} or None if no hit or the object doesn't use a mesh collision shape.
		    2=>return value is a 5-tuple and the 5th element is a 2-tuple (u,v) with the UV mapping of the hit point or None if no hit, or the object doesn't use a mesh collision shape, or doesn't have a UV mapping.
		@type poly: int
		@rtype:    3-tuple (L{KX_GameObject}, 3-tuple (x,y,z), 3-tuple (nx,ny,nz))
		        or 4-tuple (L{KX_GameObject}, 3-tuple (x,y,z), 3-tuple (nx,ny,nz), L{KX_PolyProxy})
		        or 5-tuple (L{KX_GameObject}, 3-tuple (x,y,z), 3-tuple (nx,ny,nz), L{KX_PolyProxy}, 2-tuple (u,v))
		@return: (object,hitpoint,hitnormal) or (object,hitpoint,hitnormal,polygon) or (object,hitpoint,hitnormal,polygon,hituv)
		         object, hitpoint and hitnormal are None if no hit.
		         polygon is valid only if the object is valid and is a static object, a dynamic object using mesh collision shape or a soft body object, otherwise it is None
		         hituv is valid only if polygon is valid and the object has a UV mapping, otherwise it is None
		"""
	def setCollisionMargin(margin):
		"""
		Set the objects collision margin.
		
		note: If this object has no physics controller (a physics ID of zero), this function will raise RuntimeError.
		
		@type margin: float
		@param margin: the collision margin distance in blender units.
		"""
	def sendMessage(subject, body="", to=""):
		"""
		Sends a message.
	
		@param subject: The subject of the message
		@type subject: string
		@param body: The body of the message (optional)
		@type body: string
		@param to: The name of the object to send the message to (optional)
		@type to: string
		"""
	def reinstancePhysicsMesh(gameObject, meshObject):
		"""
		Updates the physics system with the changed mesh.
		
		If no arguments are given the physics mesh will be re-created from the first mesh assigned to the game object.

		@param gameObject: optional argument, set the physics shape from this gameObjets mesh.
		@type gameObject: string, L{KX_GameObject} or None
		@param meshObject: optional argument, set the physics shape from this mesh.
		@type meshObject: string, L{KX_MeshProxy} or None

		@note: if this object has instances the other instances will be updated too.
		@note: the gameObject argument has an advantage that it can convert from a mesh with modifiers applied (such as subsurf).
		@warning: only triangle mesh type objects are supported currently (not convex hull)
		@warning: if the object is a part of a combound object it will fail (parent or child)
		@warning: rebuilding the physics mesh can be slow, running many times per second will give a performance hit.
		@rtype: boolean
		@return: True if reinstance succeeded, False if it failed.
		"""
		
	def get(key, default=None):
		"""
		Return the value matching key, or the default value if its not found.
		@return: The key value or a default.
		"""


class KX_IpoActuator(SCA_IActuator):
	"""
	IPO actuator activates an animation.
	
	@ivar frameStart: Start frame.
	@type frameStart: float
	@ivar frameEnd: End frame.
	@type frameEnd: float
	@ivar propName: Use this property to define the Ipo position
	@type propName: string
	@ivar framePropName: Assign this property this action current frame number
	@type framePropName: string
	@ivar mode: Play mode for the ipo. (In GameLogic.KX_IPOACT_PLAY, KX_IPOACT_PINGPONG, KX_IPOACT_FLIPPER, KX_IPOACT_LOOPSTOP, KX_IPOACT_LOOPEND, KX_IPOACT_FROM_PROP)
	@type mode: int
	@ivar useIpoAsForce: Apply Ipo as a global or local force depending on the local option (dynamic objects only)
	@type useIpoAsForce: bool
	@ivar useIpoAdd: Ipo is added to the current loc/rot/scale in global or local coordinate according to Local flag
	@type useIpoAdd: bool
	@ivar useIpoLocal: Let the ipo acts in local coordinates, used in Force and Add mode.
	@type useIpoLocal: bool
	@ivar useChildren: Update IPO on all children Objects as well
	@type useChildren: bool
	"""
#{ Deprecated
	def set(mode, startframe, endframe, force):
		"""
		Sets the properties of the actuator.
		
		@deprecated: use other attributes.
		@param mode:       "Play", "PingPong", "Flipper", "LoopStop", "LoopEnd" or "FromProp"
		@type mode: string
		@param startframe: first frame to use
		@type startframe: integer
		@param endframe: last frame to use
		@type endframe: integer
		@param force: special mode
		@type force: integer (0=normal, 1=interpret location as force, 2=additive)
		"""
	def setProperty(property):
		"""
		Sets the name of the property to be used in FromProp mode.
		
		@deprecated: use L{propName}
		@type property: string
		"""
	def setStart(startframe):
		"""
		Sets the frame from which the IPO starts playing.
		
		@deprecated: use L{frameStart}
		@type startframe: integer
		"""
	def getStart():
		"""
		Returns the frame from which the IPO starts playing.
		
		@deprecated: use L{frameStart}
		@rtype: integer
		"""
	def setEnd(endframe):
		"""
		Sets the frame at which the IPO stops playing.
		
		@deprecated: use L{frameEnd}
		@type endframe: integer
		"""
	def getEnd():
		"""
		Returns the frame at which the IPO stops playing.
		
		@deprecated: use L{frameEnd}
		@rtype: integer
		"""
	def setIpoAsForce(force):
		"""
		Set whether to interpret the ipo as a force rather than a displacement.
		
		@deprecated: use L{useIpoAsForce}
		@type force: boolean
		@param force: KX_TRUE or KX_FALSE
		"""
	def getIpoAsForce():
		"""
		Returns whether to interpret the ipo as a force rather than a displacement.
		
		@deprecated: use L{useIpoAsForce}
		@rtype: boolean
		"""
	def setIpoAdd(add):
		"""
		Set whether to interpret the ipo as additive rather than absolute.
		
		@deprecated: use L{useIpoAdd}
		@type add: boolean
		@param add: KX_TRUE or KX_FALSE
		"""
	def getIpoAdd():
		"""
		Returns whether to interpret the ipo as additive rather than absolute.
		
		@deprecated: use L{useIpoAdd}
		@rtype: boolean
		"""
	def setType(mode):
		"""
		Sets the operation mode of the actuator.
		
		@deprecated: use L{type}
		@param mode: KX_IPOACT_PLAY, KX_IPOACT_PINGPONG, KX_IPOACT_FLIPPER, KX_IPOACT_LOOPSTOP, KX_IPOACT_LOOPEND
		@type mode: string
		"""
	def getType():
		"""
		Returns the operation mode of the actuator.
		
		@deprecated: use L{type}
		@rtype: integer
		@return: KX_IPOACT_PLAY, KX_IPOACT_PINGPONG, KX_IPOACT_FLIPPER, KX_IPOACT_LOOPSTOP, KX_IPOACT_LOOPEND
		"""
	def setForceIpoActsLocal(local):
		"""
		Set whether to apply the force in the object's local
		coordinates rather than the world global coordinates.
	
		@deprecated: use L{useIpoLocal}
		@param local: Apply the ipo-as-force in the object's local
		              coordinates? (KX_TRUE, KX_FALSE)
		@type local: boolean
		"""
	def getForceIpoActsLocal():
		"""
		Return whether to apply the force in the object's local
		coordinates rather than the world global coordinates.
		
		@deprecated: use L{useIpoLocal}
		"""
#}

class KX_LightObject(KX_GameObject):
	"""
	A Light object.

	Example:
	
	# Turn on a red alert light.
	import GameLogic
	
	co = GameLogic.getCurrentController()
	light = co.owner
	
	light.energy = 1.0
	light.colour = [1.0, 0.0, 0.0]
	
	@group Constants: NORMAL, SPOT, SUN
	@ivar SPOT:   A spot light source. See attribute 'type'
	@ivar SUN:    A point light source with no attenuation. See attribute 'type'
	@ivar NORMAL: A point light source. See attribute 'type'
	
	@ivar type:            The type of light - must be SPOT, SUN or NORMAL
	@ivar layer:           The layer mask that this light affects object on.
	@type layer:           bitfield
	@ivar energy:          The brightness of this light. 
	@type energy:          float
	@ivar distance:        The maximum distance this light can illuminate. (SPOT and NORMAL lights only)
	@type distance:        float
	@ivar colour:          The colour of this light. Black = [0.0, 0.0, 0.0], White = [1.0, 1.0, 1.0]
	@type colour:          list [r, g, b]
	@ivar color:           Synonym for colour.
	@ivar lin_attenuation: The linear component of this light's attenuation. (SPOT and NORMAL lights only)
	@type lin_attenuation: float
	@ivar quad_attenuation: The quadratic component of this light's attenuation (SPOT and NORMAL lights only)
	@type quad_attenuation: float
	@ivar spotsize:        The cone angle of the spot light, in degrees. (float) (SPOT lights only)
	                       0.0 <= spotsize <= 180.0. Spotsize = 360.0 is also accepted. 
	@ivar spotblend:       Specifies the intensity distribution of the spot light. (float) (SPOT lights only)
	                       Higher values result in a more focused light source.
	                       0.0 <= spotblend <= 1.0.
	
	"""

class KX_MeshProxy(SCA_IObject):
	"""
	A mesh object.
	
	You can only change the vertex properties of a mesh object, not the mesh topology.
	
	To use mesh objects effectively, you should know a bit about how the game engine handles them.
		1. Mesh Objects are converted from Blender at scene load.
		2. The Converter groups polygons by Material.  This means they can be sent to the
		   renderer efficiently.  A material holds:
			1. The texture.
			2. The Blender material.
			3. The Tile properties
			4. The face properties - (From the "Texture Face" panel)
			5. Transparency & z sorting
			6. Light layer
			7. Polygon shape (triangle/quad)
			8. Game Object
		3. Verticies will be split by face if necessary.  Verticies can only be shared between
		   faces if:
			1. They are at the same position
			2. UV coordinates are the same
			3. Their normals are the same (both polygons are "Set Smooth")
			4. They are the same colour
		   For example: a cube has 24 verticies: 6 faces with 4 verticies per face.
		   
	The correct method of iterating over every L{KX_VertexProxy} in a game object::
		import GameLogic
		
		co = GameLogic.getCurrentController()
		obj = co.owner
		
		m_i = 0
		mesh = obj.getMesh(m_i) # There can be more than one mesh...
		while mesh != None:
			for mat in range(mesh.getNumMaterials()):
				for v_index in range(mesh.getVertexArrayLength(mat)):
					vertex = mesh.getVertex(mat, v_index)
					# Do something with vertex here...
					# ... eg: colour the vertex red.
					vertex.colour = [1.0, 0.0, 0.0, 1.0]
			m_i += 1
			mesh = obj.getMesh(m_i)
	
	@ivar materials: 
	@type materials: list of L{KX_BlenderMaterial} or L{KX_PolygonMaterial} types

	@ivar numPolygons:
	@type numPolygons: integer

	@ivar numMaterials:
	@type numMaterials: integer
	"""
	
	def getNumMaterials():
		"""
		Gets the number of materials associated with this object.
		
		@rtype: integer
		"""
	
	def getMaterialName(matid):
		"""
		Gets the name of the specified material.
		
		@type matid: integer
		@param matid: the specified material.
		@rtype: string
		@return: the attached material name.
		"""
	def getTextureName(matid):
		"""
		Gets the name of the specified material's texture.
		
		@type matid: integer
		@param matid: the specified material
		@rtype: string
		@return: the attached material's texture name.
		"""
	def getVertexArrayLength(matid):
		"""
		Gets the length of the vertex array associated with the specified material.
		
		There is one vertex array for each material.
		
		@type matid: integer
		@param matid: the specified material
		@rtype: integer
		@return: the number of verticies in the vertex array.
		"""
	def getVertex(matid, index):
		"""
		Gets the specified vertex from the mesh object.
		
		@type matid: integer
		@param matid: the specified material
		@type index: integer
		@param index: the index into the vertex array.
		@rtype: L{KX_VertexProxy}
		@return: a vertex object.
		"""
	def getNumPolygons():
		"""
		Returns the number of polygon in the mesh.
		
		@rtype: integer
		"""
	def getPolygon(index):
		"""
		Gets the specified polygon from the mesh.
		
		@type index: integer
		@param index: polygon number
		@rtype: L{KX_PolyProxy}
		@return: a polygon object.
		"""

class SCA_MouseSensor(SCA_ISensor):
	"""
	Mouse Sensor logic brick.
	
	Properties:
	
	@ivar position: current [x,y] coordinates of the mouse, in frame coordinates (pixels)
	@type position: [integer,interger]
	@ivar mode: sensor mode: 1=KX_MOUSESENSORMODE_LEFTBUTTON  2=KX_MOUSESENSORMODE_MIDDLEBUTTON
	                         3=KX_MOUSESENSORMODE_RIGHTBUTTON 4=KX_MOUSESENSORMODE_WHEELUP
	                         5=KX_MOUSESENSORMODE_WHEELDOWN   9=KX_MOUSESENSORMODE_MOVEMENT
	@type mode: integer
	"""

	def getXPosition():
		"""
		Gets the x coordinate of the mouse.
		
		@deprecated: use the L{position} property
		@rtype: integer
		@return: the current x coordinate of the mouse, in frame coordinates (pixels)
		"""
	def getYPosition():
		"""
		Gets the y coordinate of the mouse.
		
		@deprecated: use the L{position} property
		@rtype: integer
		@return: the current y coordinate of the mouse, in frame coordinates (pixels).
		"""	
	def getButtonStatus(button):
		"""
		Get the mouse button status.
		
		@type button: int
		@param button: value in GameLogic members KX_MOUSE_BUT_LEFT, KX_MOUSE_BUT_MIDDLE, KX_MOUSE_BUT_RIGHT
		
		@rtype: integer
		@return: value in GameLogic members KX_INPUT_NONE, KX_INPUT_NONE, KX_INPUT_JUST_ACTIVATED, KX_INPUT_ACTIVE, KX_INPUT_JUST_RELEASED
		"""

class KX_MouseFocusSensor(SCA_MouseSensor):
	"""
	The mouse focus sensor detects when the mouse is over the current game object.
	
	The mouse focus sensor works by transforming the mouse coordinates from 2d device
	space to 3d space then raycasting away from the camera.
	
	@ivar raySource: The worldspace source of the ray (the view position)
	@type raySource: list (vector of 3 floats)
	@ivar rayTarget: The worldspace target of the ray.
	@type rayTarget: list (vector of 3 floats)
	@ivar rayDirection: The L{rayTarget} - L{raySource} normalized.
	@type rayDirection: list (normalized vector of 3 floats)
	@ivar hitObject: the last object the mouse was over.
	@type hitObject: L{KX_GameObject} or None
	@ivar hitPosition: The worldspace position of the ray intersecton.
	@type hitPosition: list (vector of 3 floats)
	@ivar hitNormal: the worldspace normal from the face at point of intersection.
	@type hitNormal: list (normalized vector of 3 floats)
	@ivar hitUV: the UV coordinates at the point of intersection.
		If the object has no UV mapping, it returns [0,0].
		The UV coordinates are not normalized, they can be < 0 or > 1 depending on the UV mapping.
	@type hitUV: list (vector of 2 floats)
	@ivar usePulseFocus: When enabled, moving the mouse over a different object generates a pulse. (only used when the 'Mouse Over Any' sensor option is set)
	@type usePulseFocus: bool
	"""
#{ Deprecated
	def getHitNormal():
		"""
		Returns the normal (in worldcoordinates) at the point of collision where the object was hit by this ray.
		
		@deprecated: use the L{hitNormal} property
		@rtype: list [x, y, z]
		@return: the ray collision normal.
		"""
	def getHitObject():
		"""
		Returns the object that was hit by this ray or None.
		
		@deprecated: use the L{hitObject} property
		@rtype: L{KX_GameObject} or None
		@return: the collision object.
		"""
	def getHitPosition():
		"""
		Returns the position (in worldcoordinates) at the point of collision where the object was hit by this ray.
		
		@deprecated: use the L{hitPosition} property
		@rtype: list [x, y, z]
		@return: the ray collision position.
		"""
	def getRayDirection():
		"""
		Returns the normalized direction (in worldcoordinates) of the ray cast by the mouse.
		
		@deprecated: use the L{rayDirection} property
		@rtype: list [x, y, z]
		@return: the ray direction.
		"""
	def getRaySource():
		"""
		Returns the position (in worldcoordinates) the ray was cast from by the mouse.
		
		@deprecated: use the L{raySource} property
		@rtype: list [x, y, z]
		@return: the ray source.
		"""
	def getRayTarget():
		"""
		Returns the target of the ray (in worldcoordinates) that seeks the focus object.
		
		@deprecated: use the L{rayTarget} property
		@rtype: list [x, y, z]
		@return: the ray target.
		"""
#}

class KX_TouchSensor(SCA_ISensor):
	"""
	Touch sensor detects collisions between objects.
	
	@ivar propName: The property or material to collide with.
	@type propName: string
	@ivar useMaterial: Determines if the sensor is looking for a property or material.
						KX_True = Find material; KX_False = Find property
	@type useMaterial: boolean
	@ivar usePulseCollision: When enabled, changes to the set of colliding objects generate a pulse.
	@type usePulseCollision: bool
	@ivar hitObject: The last collided object. (read-only)
	@type hitObject: L{KX_GameObject} or None
	@ivar hitObjectList: A list of colliding objects. (read-only)
	@type hitObjectList: L{CListValue} of L{KX_GameObject}
	"""
#{ Deprecated
	def setProperty(name):
		"""
		Set the property or material to collide with. Use
		setTouchMaterial() to switch between properties and
		materials.
		
		@deprecated: use the L{property} property
		@type name: string
		"""
		
	def getProperty():
		"""
		Returns the property or material to collide with. Use
		getTouchMaterial() to find out whether this sensor
		looks for properties or materials.
		
		@deprecated: use the L{property} property
		@rtype: string
		"""
	def getHitObject():
		"""
		Returns the last object hit by this touch sensor.
		
		@deprecated: use the L{hitObject} property
		@rtype: L{KX_GameObject}
		"""
	def getHitObjectList():
		"""
		Returns a list of all objects hit in the last frame. (B{deprecated})
		
		Only objects that have the requisite material/property are listed.
		
		@deprecated: use the L{hitObjectList} property
		@rtype: L{CListValue} of L{hitObjectList}
		"""
	def getTouchMaterial():
		"""
		Returns KX_TRUE if this sensor looks for a specific material,
		KX_FALSE if it looks for a specific property. (B{deprecated})
		
		@deprecated: use the L{useMaterial} property
		"""
#}

class KX_NearSensor(KX_TouchSensor):
	"""
	A near sensor is a specialised form of touch sensor.
	
	@ivar distance: The near sensor activates when an object is within this distance.
	@type distance: float
	@ivar resetDistance: The near sensor deactivates when the object exceeds this distance.
	@type resetDistance: float
	"""

class KX_NetworkMessageActuator(SCA_IActuator):
	"""
	Message Actuator
	
	@ivar propName: Messages will only be sent to objects with the given property name.
	@type propName: string
	@ivar subject: The subject field of the message.
	@type subject: string
	@ivar body: The body of the message.
	@type body: string
	@ivar usePropBody: Send a property instead of a regular body message.
	@type usePropBody: boolean
	"""
#{Deprecated
	def setToPropName(name):
		"""
		Messages will only be sent to objects with the given property name.
		
		@deprecated: Use the L{propName} attribute instead.
		@type name: string
		"""
	def setSubject(subject):
		"""
		Sets the subject field of the message.
		
		@deprecated: Use the L{subject} attribute instead.
		@type subject: string
		"""
	def setBodyType(bodytype):
		"""
		Sets the type of body to send.
		
		@deprecated: Use the L{usePropBody} attribute instead.
		@type bodytype: boolean
		@param bodytype: True to send the value of a property, False to send the body text.
		"""
	def setBody(body):
		"""
		Sets the message body.
		
		@deprecated: Use the L{body} attribute instead.
		@type body: string
		@param body: if the body type is True, this is the name of the property to send.
		             if the body type is False, this is the text to send.
		"""
#}

class KX_NetworkMessageSensor(SCA_ISensor):
	"""
	The Message Sensor logic brick.
	
	Currently only loopback (local) networks are supported.
	
	@ivar subject: The subject the sensor is looking for.
	@type subject: string
	@ivar frameMessageCount: The number of messages received since the last frame.
								(Read-only)
	@type frameMessageCount: int
	@ivar subjects: The list of message subjects received. (Read-only)
	@type subjects: list of strings
	@ivar bodies: The list of message bodies received. (Read-only)
	@type bodies: list of strings
	"""
#{ Deprecated
	def setSubjectFilterText(subject):
		"""
		Change the message subject text that this sensor is listening to.
		
		@deprecated: Use the L{subject} attribute instead.
		@type subject: string
		@param subject: the new message subject to listen for.
		"""
	
	def getFrameMessageCount():
		"""
		Get the number of messages received since the last frame.
		
		@deprecated: Use the L{frameMessageCount} attribute instead.
		@rtype: integer
		"""
	def getBodies():
		"""
		Gets the list of message bodies.
		
		@deprecated: Use the L{bodies} attribute instead.
		@rtype: list
		"""
	def getSubject():
		"""
		Gets the message subject this sensor is listening for from the Subject: field.
		
		@deprecated: Use the L{subject} attribute instead.
		@rtype: string
		"""
	def getSubjects():
		"""
		Gets the list of message subjects received.
		
		@deprecated: Use the L{subjects} attribute instead.
		@rtype: list
		"""
#}

class KX_ObjectActuator(SCA_IActuator):
	"""
	The object actuator ("Motion Actuator") applies force, torque, displacement, angular displacement,
	velocity, or angular velocity to an object.
	Servo control allows to regulate force to achieve a certain speed target.
	
	@ivar force: The force applied by the actuator
	@type force: list [x, y, z]
	@ivar useLocalForce: A flag specifying if the force is local
	@type useLocalForce: bool
	@ivar torque: The torque applied by the actuator
	@type torque: list [x, y, z]
	@ivar useLocalTorque: A flag specifying if the torque is local
	@type useLocalTorque: bool
	@ivar dLoc: The displacement vector applied by the actuator
	@type dLoc: list [x, y, z]
	@ivar useLocalDLoc: A flag specifying if the dLoc is local
	@type useLocalDLoc: bool
	@ivar dRot: The angular displacement vector applied by the actuator
		- note: Since the displacement is applied every frame, you must adjust the displacement
		based on the frame rate, or you game experience will depend on the player's computer
		speed.
	@type dRot: list [x, y, z]
	@ivar useLocalDRot: A flag specifying if the dRot is local
	@type useLocalDRot: bool
	@ivar linV: The linear velocity applied by the actuator
	@type linV: list [x, y, z]
	@ivar useLocalLinV: A flag specifying if the linear velocity is local
		- note: This is the target speed for servo controllers
	@type useLocalLinV: bool
	@ivar angV: The angular velocity applied by the actuator
	@type angV: list [x, y, z]
	@ivar useLocalAngV: A flag specifying if the angular velocity is local
	@type useLocalAngV: bool
	
	@ivar damping: The damping parameter of the servo controller
	@type damping: short
	
	@ivar forceLimitX: The min/max force limit along the X axis and activates or deactivates the limits in the servo controller
	@type forceLimitX: list [min(float), max(float), bool]
	@ivar forceLimitY: The min/max force limit along the Y axis and activates or deactivates the limits in the servo controller
	@type forceLimitY: list [min(float), max(float), bool]
	@ivar forceLimitZ: The min/max force limit along the Z axis and activates or deactivates the limits in the servo controller
	@type forceLimitZ: list [min(float), max(float), bool]
	
	@ivar pid: The PID coefficients of the servo controller
	@type pid: list of floats [proportional, integral, derivate]
	@ivar reference: The object that is used as reference to compute the velocity for the servo controller.
	@type reference: KX_GameObject or None
	
	@group Deprecated: getForce, setForce, getTorque, setTorque, getDLoc, setDLoc, getDRot, setDRot, getLinearVelocity, setLinearVelocity, getAngularVelocity,
						setAngularVelocity, getDamping, setDamping, getForceLimitX, setForceLimitX, getForceLimitY, setForceLimitY, getForceLimitZ, setForceLimitZ,
						getPID, setPID
	"""
	def getForce():
		"""
		Returns the force applied by the actuator.

		@deprecated: Use L{force} and L{useLocalForce} instead.
		@rtype: list [fx, fy, fz, local]
		@return: A four item list, containing the vector force, and a flag specifying whether the force is local.
		"""
	def setForce(fx, fy, fz, local):
		"""
		Sets the force applied by the actuator.
		
		@deprecated: Use L{force} and L{useLocalForce} instead.
		@type fx: float
		@param fx: the x component of the force.
		@type fy: float
		@param fy: the z component of the force.
		@type fz: float
		@param fz: the z component of the force.
		@type local: boolean
		@param local: - False: the force is applied in world coordinates.
		              - True: the force is applied in local coordinates.
		"""
	def getTorque():
		"""
		Returns the torque applied by the actuator.
		
		@deprecated: Use L{torque} and L{useLocalTorque} instead.
		@rtype: list [S{Tau}x, S{Tau}y, S{Tau}z, local]
		@return: A four item list, containing the vector torque, and a flag specifying whether
		         the torque is applied in local coordinates (True) or world coordinates (False)
		"""
	def setTorque(tx, ty, tz, local):
		"""
		Sets the torque applied by the actuator.
		
		@deprecated: Use L{torque} and L{useLocalTorque} instead.
		@type tx: float
		@param tx: the x component of the torque.
		@type ty: float
		@param ty: the z component of the torque.
		@type tz: float
		@param tz: the z component of the torque.
		@type local: boolean
		@param local: - False: the torque is applied in world coordinates.
		              - True: the torque is applied in local coordinates.
		"""
	def getDLoc():
		"""
		Returns the displacement vector applied by the actuator.
		
		@deprecated: Use L{dLoc} and L{useLocalDLoc} instead.
		@rtype: list [dx, dy, dz, local]
		@return: A four item list, containing the vector displacement, and whether
		         the displacement is applied in local coordinates (True) or world
			 coordinates (False)
		"""
	def setDLoc(dx, dy, dz, local):
		"""
		Sets the displacement vector applied by the actuator.
		
		Since the displacement is applied every frame, you must adjust the displacement
		based on the frame rate, or you game experience will depend on the player's computer
		speed.
		
		@deprecated: Use L{dLoc} and L{useLocalDLoc} instead.
		@type dx: float
		@param dx: the x component of the displacement vector.
		@type dy: float
		@param dy: the z component of the displacement vector.
		@type dz: float
		@param dz: the z component of the displacement vector.
		@type local: boolean
		@param local: - False: the displacement vector is applied in world coordinates.
		              - True: the displacement vector is applied in local coordinates.
		"""
	def getDRot():
		"""
		Returns the angular displacement vector applied by the actuator.
		
		@deprecated: Use L{dRot} and L{useLocalDRot} instead.
		@rtype: list [dx, dy, dz, local]
		@return: A four item list, containing the angular displacement vector, and whether
		         the displacement is applied in local coordinates (True) or world coordinates (False)
		"""
	def setDRot(dx, dy, dz, local):
		"""
		Sets the angular displacement vector applied by the actuator.
		
		Since the displacement is applied every frame, you must adjust the displacement
		based on the frame rate, or you game experience will depend on the player's computer
		speed.
		
		@deprecated: Use L{dRot} and L{useLocalDRot} instead.
		@type dx: float
		@param dx: the x component of the angular displacement vector.
		@type dy: float
		@param dy: the z component of the angular displacement vector.
		@type dz: float
		@param dz: the z component of the angular displacement vector.
		@type local: boolean
		@param local: - False: the angular displacement vector is applied in world coordinates.
		              - True: the angular displacement vector is applied in local coordinates.
		"""
	def getLinearVelocity():
		"""
		Returns the linear velocity applied by the actuator.
		For the servo control actuator, this is the target speed.

		@deprecated: Use L{linV} and L{useLocalLinV} instead.
		@rtype: list [vx, vy, vz, local]
		@return: A four item list, containing the vector velocity, and whether the velocity is applied in local coordinates (True) or world coordinates (False)
		"""
	def setLinearVelocity(vx, vy, vz, local):
		"""
		Sets the linear velocity applied by the actuator.
		For the servo control actuator, sets the target speed.

		@deprecated: Use L{linV} and L{useLocalLinV} instead.
		@type vx: float
		@param vx: the x component of the velocity vector.
		@type vy: float
		@param vy: the z component of the velocity vector.
		@type vz: float
		@param vz: the z component of the velocity vector.
		@type local: boolean
		@param local: - False: the velocity vector is in world coordinates.
		              - True: the velocity vector is in local coordinates.
		"""
	def getAngularVelocity():
		"""
		Returns the angular velocity applied by the actuator.

		@deprecated: Use L{angV} and L{useLocalAngV} instead.
		@rtype: list [S{omega}x, S{omega}y, S{omega}z, local]
		@return: A four item list, containing the vector velocity, and whether
		         the velocity is applied in local coordinates (True) or world
			 coordinates (False)
		"""
	def setAngularVelocity(wx, wy, wz, local):
		"""
		Sets the angular velocity applied by the actuator.

		@deprecated: Use L{angV} and L{useLocalAngV} instead.
		@type wx: float
		@param wx: the x component of the velocity vector.
		@type wy: float
		@param wy: the z component of the velocity vector.
		@type wz: float
		@param wz: the z component of the velocity vector.
		@type local: boolean
		@param local: - False: the velocity vector is applied in world coordinates.
		              - True: the velocity vector is applied in local coordinates.
		"""
	def getDamping():
		"""
		Returns the damping parameter of the servo controller.

		@deprecated: Use L{damping} instead.
		@rtype: integer
		@return: the time constant of the servo controller in frame unit.
		"""
	def setDamping(damp):
		"""
		Sets the damping parameter of the servo controller.

		@deprecated: Use L{damping} instead.
		@type damp: integer
		@param damp: the damping parameter in frame unit.
		"""
	def getForceLimitX():
		"""
		Returns the min/max force limit along the X axis used by the servo controller.

		@deprecated: Use L{forceLimitX} instead.
		@rtype: list [min, max, enabled]
		@return: A three item list, containing the min and max limits of the force as float
		         and whether the limits are active(true) or inactive(true)
		"""
	def setForceLimitX(min, max, enable):
		"""
		Sets the min/max force limit along the X axis and activates or deactivates the limits in the servo controller.

		@deprecated: Use L{forceLimitX} instead.
		@type min: float
		@param min: the minimum value of the force along the X axis.
		@type max: float
		@param max: the maximum value of the force along the X axis.
		@type enable: boolean
		@param enable: - True: the force will be limited to the min/max
		               - False: the force will not be limited		               
		"""
	def getForceLimitY():
		"""
		Returns the min/max force limit along the Y axis used by the servo controller.

		@deprecated: Use L{forceLimitY} instead.
		@rtype: list [min, max, enabled]
		@return: A three item list, containing the min and max limits of the force as float
		         and whether the limits are active(true) or inactive(true)
		"""
	def setForceLimitY(min, max, enable):
		"""
		Sets the min/max force limit along the Y axis and activates or deactivates the limits in the servo controller.

		@deprecated: Use L{forceLimitY} instead.
		@type min: float
		@param min: the minimum value of the force along the Y axis.
		@type max: float
		@param max: the maximum value of the force along the Y axis.
		@type enable: boolean
		@param enable: - True: the force will be limited to the min/max
		               - False: the force will not be limited		               
		"""
	def getForceLimitZ():
		"""
		Returns the min/max force limit along the Z axis used by the servo controller.

		@deprecated: Use L{forceLimitZ} instead.
		@rtype: list [min, max, enabled]
		@return: A three item list, containing the min and max limits of the force as float
		         and whether the limits are active(true) or inactive(true)
		"""
	def setForceLimitZ(min, max, enable):
		"""
		Sets the min/max force limit along the Z axis and activates or deactivates the limits in the servo controller.

		@deprecated: Use L{forceLimitZ} instead.
		@type min: float
		@param min: the minimum value of the force along the Z axis.
		@type max: float
		@param max: the maximum value of the force along the Z axis.
		@type enable: boolean
		@param enable: - True: the force will be limited to the min/max
		               - False: the force will not be limited		               
		"""
	def getPID():
		"""
		Returns the PID coefficient of the servo controller.

		@deprecated: Use L{pid} instead.
		@rtype: list [P, I, D]
		@return: A three item list, containing the PID coefficient as floats:
		         P : proportional coefficient
		         I : Integral coefficient
		         D : Derivate coefficient
		"""
	def setPID(P, I, D):
		"""
		Sets the PID coefficients of the servo controller.

		@deprecated: Use L{pid} instead.
		@type P: flat
		@param P: proportional coefficient
		@type I: float
		@param I: Integral coefficient
		@type D: float
		@param D: Derivate coefficient
		"""

class KX_ParentActuator(SCA_IActuator):
	"""
	The parent actuator can set or remove an objects parent object.	
	@ivar object: the object this actuator sets the parent too.
	@type object: KX_GameObject or None
	@ivar mode: The mode of this actuator
	@type mode: int from 0 to 1 L{GameLogic.Parent Actuator}
	@ivar compound: Whether the object shape should be added to the parent compound shape when parenting
	                Effective only if the parent is already a compound shape
	@type compound: bool
	@ivar ghost: whether the object should be made ghost when parenting
	             Effective only if the shape is not added to the parent compound shape
	@type ghost: bool
	
	"""
	def setObject(object):
		"""
		Sets the object to set as parent.
		
		Object can be either a L{KX_GameObject} or the name of the object.
		
		@deprecated: Use the L{object} property.
		@type object: L{KX_GameObject}, string or None
		"""
	def getObject(name_only = 1):
		"""
		Returns the name of the object to change to.
		
		@deprecated: Use the L{object} property.
		@type name_only: bool
		@param name_only: optional argument, when 0 return a KX_GameObject
		@rtype: string, KX_GameObject or None if no object is set
		"""

class KX_PhysicsObjectWrapper(PyObjectPlus):
	"""
	KX_PhysicsObjectWrapper
	
	"""
	def setActive(active):
		"""
		Set the object to be active.
		
		@param active: set to True to be active
		@type active: bool
		"""

	def setAngularVelocity(x, y, z, local):
		"""
		Set the angular velocity of the object.
		
		@param x: angular velocity for the x-axis
		@type x: float
		
		@param y: angular velocity for the y-axis
		@type y: float
		
		@param z: angular velocity for the z-axis
		@type z: float
		
		@param local: set to True for local axis
		@type local: bool
		"""
	def setLinearVelocity(x, y, z, local):
		"""
		Set the linear velocity of the object.
		
		@param x: linear velocity for the x-axis
		@type x: float
		
		@param y: linear velocity for the y-axis
		@type y: float
		
		@param z: linear velocity for the z-axis
		@type z: float
		
		@param local: set to True for local axis
		@type local: bool
		"""
	def setPosition(x, y, z):
		"""
		Set the position of the object
		
		@param x: x coordinate
		@type x: float
		
		@param y: y coordinate
		@type y: float
		
		@param z: z coordinate
		@type z: float
		"""

class KX_PolyProxy(SCA_IObject):
	"""
	A polygon holds the index of the vertex forming the poylgon.

	Note: 
	The polygon attributes are read-only, you need to retrieve the vertex proxy if you want
	to change the vertex settings. 
	
	@ivar matname: The name of polygon material, empty if no material.
	@type matname: string
	@ivar material: The material of the polygon
	@type material: L{KX_PolygonMaterial} or L{KX_BlenderMaterial}
	@ivar texture: The texture name of the polygon.
	@type texture: string
	@ivar matid: The material index of the polygon, use this to retrieve vertex proxy from mesh proxy
	@type matid: integer
	@ivar v1: vertex index of the first vertex of the polygon, use this to retrieve vertex proxy from mesh proxy
	@type v1: integer	
	@ivar v2: vertex index of the second vertex of the polygon, use this to retrieve vertex proxy from mesh proxy
	@type v2: integer	
	@ivar v3: vertex index of the third vertex of the polygon, use this to retrieve vertex proxy from mesh proxy
	@type v3: integer	
	@ivar v4: vertex index of the fourth vertex of the polygon, 0 if polygon has only 3 vertex
	          use this to retrieve vertex proxy from mesh proxy
	@type v4: integer	
	@ivar visible: visible state of the polygon: 1=visible, 0=invisible
	@type visible: integer
	@ivar collide: collide state of the polygon: 1=receives collision, 0=collision free.
	@type collide: integer
	"""

	def getMaterialName(): 
		"""
		Returns the polygon material name with MA prefix
		
		@rtype: string
		@return: material name
		"""
	def getMaterial(): 
		"""
		Returns the polygon material
		
		@rtype: L{KX_PolygonMaterial} or L{KX_BlenderMaterial}
		"""
	def getTextureName():
		"""
		Returns the polygon texture name
		
		@rtype: string
		@return: texture name
		"""
	def getMaterialIndex():
		"""
		Returns the material bucket index of the polygon. 
		This index and the ones returned by getVertexIndex() are needed to retrieve the vertex proxy from L{KX_MeshProxy}.
		
		@rtype: integer
		@return: the material index in the mesh
		"""
	def getNumVertex(): 
		"""
		Returns the number of vertex of the polygon.
		
		@rtype: integer
		@return: number of vertex, 3 or 4.
		"""
	def isVisible():
		"""
		Returns whether the polygon is visible or not
		
		@rtype: integer
		@return: 0=invisible, 1=visible
		"""
	def isCollider():
		"""
		Returns whether the polygon is receives collision or not
		
		@rtype: integer
		@return: 0=collision free, 1=receives collision
		"""
	def getVertexIndex(vertex):
		"""
		Returns the mesh vertex index of a polygon vertex
		This index and the one returned by getMaterialIndex() are needed to retrieve the vertex proxy from L{KX_MeshProxy}.
		
		@type vertex: integer
		@param vertex: index of the vertex in the polygon: 0->3
		@rtype: integer
		@return: mesh vertex index
		"""
	def getMesh():
		"""
		Returns a mesh proxy
		
		@rtype: L{KX_MeshProxy}
		@return: mesh proxy
		"""

class KX_PolygonMaterial:
	"""
	This is the interface to materials in the game engine.
	
	Materials define the render state to be applied to mesh objects.
	
	Warning:  Some of the methods/variables are CObjects.  If you mix these up,
	you will crash blender.
	
	This example requires:
		- PyOpenGL http://pyopengl.sourceforge.net/
		- GLEWPy http://glewpy.sourceforge.net/
	Example::
		
		import GameLogic
		import OpenGL
		from OpenGL.GL import *
		from OpenGL.GLU import *
		import glew
		from glew import *
		
		glewInit()
		
		vertex_shader = \"\"\"
		
		void main(void)
		{
			gl_Position = ftransform();
		}
		\"\"\"
		
		fragment_shader =\"\"\"
		
		void main(void)
		{
			gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0);
		}
		\"\"\"
		
		class MyMaterial:
			def __init__(self):
				self.pass_no = 0
				# Create a shader
				self.m_program = glCreateProgramObjectARB()
				# Compile the vertex shader
				self.shader(GL_VERTEX_SHADER_ARB, (vertex_shader))
				# Compile the fragment shader
				self.shader(GL_FRAGMENT_SHADER_ARB, (fragment_shader))
				# Link the shaders together
				self.link()
				
			def PrintInfoLog(self, tag, object):
				\"\"\"
				PrintInfoLog prints the GLSL compiler log
				\"\"\"
				print "Tag: 	def PrintGLError(self, tag = ""):
				
			def PrintGLError(self, tag = ""):
				\"\"\"
				Prints the current GL error status
				\"\"\"
				if len(tag):
					print tag
				err = glGetError()
				if err != GL_NO_ERROR:
					print "GL Error: %s\\n"%(gluErrorString(err))
		
			def shader(self, type, shaders):
				\"\"\"
				shader compiles a GLSL shader and attaches it to the current
				program.
				
				type should be either GL_VERTEX_SHADER_ARB or GL_FRAGMENT_SHADER_ARB
				shaders should be a sequence of shader source to compile.
				\"\"\"
				# Create a shader object
				shader_object = glCreateShaderObjectARB(type)
		
				# Add the source code
				glShaderSourceARB(shader_object, len(shaders), shaders)
				
				# Compile the shader
				glCompileShaderARB(shader_object)
				
				# Print the compiler log
				self.PrintInfoLog("vertex shader", shader_object)
				
				# Check if compiled, and attach if it did
				compiled = glGetObjectParameterivARB(shader_object, GL_OBJECT_COMPILE_STATUS_ARB)
				if compiled:
					glAttachObjectARB(self.m_program, shader_object)
					
				# Delete the object (glAttachObjectARB makes a copy)
				glDeleteObjectARB(shader_object)
				
				# print the gl error log
				self.PrintGLError()
				
			def link(self):
				\"\"\"
				Links the shaders together.
				\"\"\"
				# clear error indicator		
				glGetError()
				
				glLinkProgramARB(self.m_program)
		
				self.PrintInfoLog("link", self.m_program)
			
				linked = glGetObjectParameterivARB(self.m_program, GL_OBJECT_LINK_STATUS_ARB)
				if not linked:
					print "Shader failed to link"
					return
		
				glValidateProgramARB(self.m_program)
				valid = glGetObjectParameterivARB(self.m_program, GL_OBJECT_VALIDATE_STATUS_ARB)
				if not valid:
					print "Shader failed to validate"
					return
				
			def activate(self, rasty, cachingInfo, mat):
				self.pass_no+=1
				if (self.pass_no == 1):
					glDisable(GL_COLOR_MATERIAL)
					glUseProgramObjectARB(self.m_program)
					return True
				
				glEnable(GL_COLOR_MATERIAL)
				glUseProgramObjectARB(0)
				self.pass_no = 0	
				return False
		
		obj = GameLogic.getCurrentController().owner
		
		mesh = obj.meshes[0]
		
		for mat in mesh.materials:
			mat.setCustomMaterial(MyMaterial())
			print mat.texture
	
	@ivar texture: Texture name
	@type texture: string (read-only)
	
	@ivar gl_texture: OpenGL texture handle (eg for glBindTexture(GL_TEXTURE_2D, gl_texture)
	@type gl_texture: integer (read-only)
	
	@ivar material: Material name
	@type material: string (read-only)
	
	@ivar tface: Texture face properties
	@type tface: CObject (read-only)
	
	@ivar tile: Texture is tiling
	@type tile: boolean
	@ivar tilexrep: Number of tile repetitions in x direction.
	@type tilexrep: integer
	@ivar tileyrep: Number of tile repetitions in y direction.
	@type tileyrep: integer
	
	@ivar drawingmode: Drawing mode for the material.
		- 2  (drawingmode & 4)     Textured
		- 4  (drawingmode & 16)    Light
		- 14 (drawingmode & 16384) 3d Polygon Text
	@type drawingmode: bitfield
	
	@ivar transparent: This material is transparent.  All meshes with this
		material will be rendered after non transparent meshes from back
		to front.
	@type transparent: boolean
	
	@ivar zsort: Transparent polygons in meshes with this material will be sorted back to
		front before rendering.
		Non-Transparent polygons will be sorted front to back before rendering.
	@type zsort: boolean
	
	@ivar lightlayer: Light layers this material affects.
	@type lightlayer: bitfield.
	
	@ivar triangle: Mesh data with this material is triangles.  It's probably not safe to change this.
	@type triangle: boolean
	
	@ivar diffuse: The diffuse colour of the material.  black = [0.0, 0.0, 0.0] white = [1.0, 1.0, 1.0]
	@type diffuse: list [r, g, b]
	@ivar specular: The specular colour of the material. black = [0.0, 0.0, 0.0] white = [1.0, 1.0, 1.0]
	@type specular: list [r, g, b] 
	@ivar shininess: The shininess (specular exponent) of the material. 0.0 <= shininess <= 128.0
	@type shininess: float
	@ivar specularity: The amount of specular of the material. 0.0 <= specularity <= 1.0
	@type specularity: float
	"""
	def updateTexture(tface, rasty):
		"""
		Updates a realtime animation.
		
		@param tface: Texture face (eg mat.tface)
		@type tface: CObject
		@param rasty: Rasterizer
		@type rasty: CObject
		"""
	def setTexture(tface):
		"""
		Sets texture render state.
		
		Example::
			mat.setTexture(mat.tface)
		
		@param tface: Texture face
		@type tface: CObject
		"""
	def activate(rasty, cachingInfo):
		"""
		Sets material parameters for this object for rendering.
		
		Material Parameters set:
			1. Texture
			2. Backface culling
			3. Line drawing
			4. Specular Colour
			5. Shininess
			6. Diffuse Colour
			7. Polygon Offset.
		
		@param rasty: Rasterizer instance.
		@type rasty: CObject
		@param cachingInfo: Material cache instance.
		@type cachingInfo: CObject
		"""
	def setCustomMaterial(material):
		"""
		Sets the material state setup object.
		
		Using this method, you can extend or completely replace the gameengine material
		to do your own advanced multipass effects.
		
		Use this method to register your material class.  Instead of the normal material,
		your class's activate method will be called just before rendering the mesh.  
		This should setup the texture, material, and any other state you would like.
		It should return True to render the mesh, or False if you are finished.  You should
		clean up any state Blender does not set before returning False.

		Activate Method Definition::
				def activate(self, rasty, cachingInfo, material):
			
		Example::
			class PyMaterial:
				def __init__(self):
					self.pass_no = -1
				
				def activate(self, rasty, cachingInfo, material):
					# Activate the material here.
					#
					# The activate method will be called until it returns False.
					# Every time the activate method returns True the mesh will
					# be rendered.
					#
					# rasty is a CObject for passing to material.updateTexture() 
					#       and material.activate()
					# cachingInfo is a CObject for passing to material.activate()
					# material is the KX_PolygonMaterial instance this material
					#          was added to
					
					# default material properties:
					self.pass_no += 1
					if self.pass_no == 0:
						material.activate(rasty, cachingInfo)
						# Return True to do this pass
						return True
					
					# clean up and return False to finish.
					self.pass_no = -1
					return False
			
			# Create a new Python Material and pass it to the renderer.
			mat.setCustomMaterial(PyMaterial())
		
		@param material: The material object.
		@type material: instance
		"""

class KX_RadarSensor(KX_NearSensor):
	"""
	Radar sensor is a near sensor with a conical sensor object.
	
	@ivar coneOrigin: The origin of the cone with which to test. The origin
						is in the middle of the cone.
						(read-only)
	@type coneOrigin: list of floats [x, y, z]
	@ivar coneTarget: The center of the bottom face of the cone with which to test.
						(read-only)
	@type coneTarget: list of floats [x, y, z]
	@ivar distance: The height of the cone with which to test.
	@type distance: float
	@ivar angle: The angle of the cone (in degrees) with which to test.
	@type angle: float from 0 to 360
	@ivar axis: The axis on which the radar cone is cast
	@type axis: int from 0 to 5
		KX_RADAR_AXIS_POS_X, KX_RADAR_AXIS_POS_Y, KX_RADAR_AXIS_POS_Z,
		KX_RADAR_AXIS_NEG_X, KX_RADAR_AXIS_NEG_Y, KX_RADAR_AXIS_NEG_Z
	"""
#{Deprecated
	#--The following methods are deprecated, please use properties instead.
	def getConeOrigin():
		"""
		Returns the origin of the cone with which to test. The origin
		is in the middle of the cone.
		
		@deprecated: Use the L{coneOrigin} property.
		@rtype: list [x, y, z]
		"""

	def getConeTarget():
		"""
		Returns the center of the bottom face of the cone with which to test.
		
		@deprecated: Use the L{coneTarget} property.
		@rtype: list [x, y, z]
		"""
#}
	
	def getConeHeight():
		"""
		Returns the height of the cone with which to test.
		
		@rtype: float
		"""

class KX_RaySensor(SCA_ISensor):
	"""
	A ray sensor detects the first object in a given direction.
	
	@ivar propName: The property the ray is looking for.
	@type propName: string
	@ivar range: The distance of the ray.
	@type range: float
	@ivar useMaterial: Whether or not to look for a material (false = property)
	@type useMaterial: boolean
	@ivar useXRay: Whether or not to use XRay.
	@type useXRay: boolean
	@ivar hitObject: The game object that was hit by the ray. (Read-only)
	@type hitObject: KX_GameObject
	@ivar hitPosition: The position (in worldcoordinates) where the object was hit by the ray. (Read-only)
	@type hitPosition: list [x, y, z]
	@ivar hitNormal: The normal (in worldcoordinates) of the object at the location where the object was hit by the ray. (Read-only)
	@type hitNormal: list [x, y, z]
	@ivar rayDirection: The direction from the ray (in worldcoordinates). (Read-only)
	@type rayDirection: list [x, y, z]
	@ivar axis: The axis the ray is pointing on.
	@type axis: int from 0 to 5
		KX_RAY_AXIS_POS_X, KX_RAY_AXIS_POS_Y, KX_RAY_AXIS_POS_Z,
		KX_RAY_AXIS_NEG_X, KX_RAY_AXIS_NEG_Y, KX_RAY_AXIS_NEG_Z
	"""
#{ Deprecated
	def getHitObject():
		"""
		Returns the game object that was hit by this ray.
		
		@deprecated: Use the L{hitObject} attribute instead.
		@rtype: KX_GameObject
		"""
	def getHitPosition():
		"""
		Returns the position (in worldcoordinates) where the object was hit by this ray.
		
		@deprecated: Use the L{hitPosition} attribute instead.
		@rtype: list [x, y, z]
		"""
	def getHitNormal():
		"""
		Returns the normal (in worldcoordinates) of the object at the location where the object was hit by this ray.
		
		@deprecated: Use the L{hitNormal} attribute instead.
		@rtype: list [nx, ny, nz]
		"""
	def getRayDirection():
		"""
		Returns the direction from the ray (in worldcoordinates)
		
		@deprecated: Use the L{rayDirection} attribute instead.
		@rtype: list [dx, dy, dz]
		"""
#}

class KX_SCA_AddObjectActuator(SCA_IActuator):
	"""
	Edit Object Actuator (in Add Object Mode)
	@ivar object: the object this actuator adds.
	@type object: KX_GameObject or None
	@ivar objectLastCreated: the last added object from this actuator (read-only).
	@type objectLastCreated: KX_GameObject or None
	@ivar time: the lifetime of added objects, in frames. Set to 0 to disable automatic deletion.
	@type time: integer
	@ivar linearVelocity: the initial linear velocity of added objects.
	@type linearVelocity: list [vx, vy, vz]
	@ivar angularVelocity: the initial angular velocity of added objects.
	@type angularVelocity: list [vx, vy, vz]
	
	@warning: An Add Object actuator will be ignored if at game start, the linked object doesn't exist
		  (or is empty) or the linked object is in an active layer.
		  
		  This will genereate a warning in the console:
		  
		  C{ERROR: GameObject I{Name} has a AddObjectActuator I{ActuatorName} without object (in 'nonactive' layer)}
	"""
#{Deprecated
	def setObject(object):
		"""
		Sets the game object to add.
		
		A copy of the object will be added to the scene when the actuator is activated.
		
		If the object does not exist, this function is ignored.
		
		object can either be a L{KX_GameObject} or the name of an object or None.
		
		@deprecated: use the L{object} property
		@type object: L{KX_GameObject}, string or None
		"""
	def getObject(name_only = 0):
		"""
		Returns the name of the game object to be added.
		
		Returns None if no game object has been assigned to be added.
		
		@deprecated: use the L{object} property
		@type name_only: bool
		@param name_only: optional argument, when 0 return a KX_GameObject
		@rtype: string, KX_GameObject or None if no object is set
		"""
	def setTime(time):
		"""
		Sets the lifetime of added objects, in frames.
		
		If time == 0, the object will last forever.
		
		@deprecated: use the L{time} property
		@type time: integer
		@param time: The minimum value for time is 0.
		"""
	def getTime():
		"""
		Returns the lifetime of the added object, in frames.
		
		@deprecated: use the L{time} property
		@rtype: integer
		"""
	def setLinearVelocity(vx, vy, vz):
		"""
		Sets the initial linear velocity of added objects.
		
		@deprecated: use the L{linearVelocity} property
		@type vx: float
		@param vx: the x component of the initial linear velocity.
		@type vy: float
		@param vy: the y component of the initial linear velocity.
		@type vz: float
		@param vz: the z component of the initial linear velocity.
		"""
	def getLinearVelocity():
		"""
		Returns the initial linear velocity of added objects.
		
		@deprecated: use the L{linearVelocity} property
		@rtype: list [vx, vy, vz]
		"""
	def setAngularVelocity(vx, vy, vz):
		"""
		Sets the initial angular velocity of added objects.
		
		@deprecated: use the L{angularVelocity} property
		@type vx: float
		@param vx: the x component of the initial angular velocity.
		@type vy: float
		@param vy: the y component of the initial angular velocity.
		@type vz: float
		@param vz: the z component of the initial angular velocity.
		"""
	def getAngularVelocity():
		"""
		Returns the initial angular velocity of added objects.
		
		@deprecated: use the L{angularVelocity} property
		@rtype: list [vx, vy, vz]
		"""
	def getLastCreatedObject():
		"""
		Returns the last object created by this actuator.
		
		@deprecated: use the L{objectLastCreated} property
		@rtype: L{KX_GameObject}
		@return: A L{KX_GameObject} or None if no object has been created.
		"""
#}
	def instantAddObject():
		"""
		Returns the last object created by this actuator. The object can then be accessed from L{objectLastCreated}.
		
		@rtype: None
		"""

class KX_SCA_DynamicActuator(SCA_IActuator):
	"""
	Dynamic Actuator.
	@ivar mode: the type of operation of the actuator, 0-4
						KX_DYN_RESTORE_DYNAMICS, KX_DYN_DISABLE_DYNAMICS, 
						KX_DYN_ENABLE_RIGID_BODY, KX_DYN_DISABLE_RIGID_BODY, KX_DYN_SET_MASS
	@type mode: integer
	@ivar mass: the mass value for the KX_DYN_SET_MASS operation
	@type mass: float
	"""
#{ Deprecated
	def setOperation(operation):
		"""
		Set the type of operation when the actuator is activated:
				- 0 = restore dynamics
				- 1 = disable dynamics
				- 2 = enable rigid body
				- 3 = disable rigid body
				- 4 = set mass
		
		@deprecated: Use the L{mode} attribute instead.
		"""
	def getOperation():
		"""
		return the type of operation
		@deprecated: Use the L{mode} attribute instead.
		"""
#}

class KX_SCA_EndObjectActuator(SCA_IActuator):
	"""
	Edit Object Actuator (in End Object mode)
	
	This actuator has no python methods.
	"""

class KX_SCA_ReplaceMeshActuator(SCA_IActuator):
	"""
	Edit Object actuator, in Replace Mesh mode.
	
	Example::
		# Level-of-detail
		# Switch a game object's mesh based on its depth in the camera view.
		# +----------+     +-----------+     +-------------------------------------+
		# | Always   +-----+ Python    +-----+ Edit Object (Replace Mesh) LOD.Mesh |
		# +----------+     +-----------+     +-------------------------------------+
		import GameLogic

		# List detail meshes here
		# Mesh (name, near, far)
		# Meshes overlap so that they don't 'pop' when on the edge of the distance.
		meshes = ((".Hi", 0.0, -20.0),
				  (".Med", -15.0, -50.0),
				  (".Lo", -40.0, -100.0)
				)
		
		co = GameLogic.getCurrentController()
		obj = co.owner
		act = co.actuators["LOD." + obj.name]
		cam = GameLogic.getCurrentScene().active_camera
		
		def Depth(pos, plane):
			return pos[0]*plane[0] + pos[1]*plane[1] + pos[2]*plane[2] + plane[3]
		
		# Depth is negative and decreasing further from the camera
		depth = Depth(obj.position, cam.world_to_camera[2])
		
		newmesh = None
		curmesh = None
		# Find the lowest detail mesh for depth
		for mesh in meshes:
			if depth < mesh[1] and depth > mesh[2]:
				newmesh = mesh
			if "ME" + obj.name + mesh[0] == act.getMesh():
				curmesh = mesh
		
		if newmesh != None and "ME" + obj.name + newmesh[0] != act.getMesh():
			# The mesh is a different mesh - switch it.
			# Check the current mesh is not a better fit.
			if curmesh == None or curmesh[1] < depth or curmesh[2] > depth:
				act.mesh = obj.getName() + newmesh[0]
				GameLogic.addActiveActuator(act, True)
	
	@warning: Replace mesh actuators will be ignored if at game start, the
		named mesh doesn't exist.
		
		This will generate a warning in the console:
		
		C{ERROR: GameObject I{Name} ReplaceMeshActuator I{ActuatorName} without object}
	
	@ivar mesh: L{KX_MeshProxy} or the name of the mesh that will replace the current one
	            Set to None to disable actuator
	@type mesh: L{KX_MeshProxy} or None if no mesh is set
	
	@ivar useDisplayMesh: when true the displayed mesh is replaced.
	@type useDisplayMesh: boolean
	@ivar usePhysicsMesh: when true the physics mesh is replaced.
	@type usePhysicsMesh: boolean
	"""
	def setMesh(name):
		"""
		Sets the name of the mesh that will replace the current one.
		When the name is None it will unset the mesh value so no action is taken.
		
		@deprecated: Use the L{mesh} attribute instead.
		@type name: string or None
		"""
	def getMesh():
		"""
		Returns the name of the mesh that will replace the current one.
		
		Returns None if no mesh has been scheduled to be added.
		
		@deprecated: Use the L{mesh} attribute instead.
		@rtype: string or None
		"""
	def instantReplaceMesh():
		"""
		Immediately replace mesh without delay.
		@rtype: None
		"""

class KX_Scene(PyObjectPlus):
	"""
	An active scene that gives access to objects, cameras, lights and scene attributes.
	
	The activity culling stuff is supposed to disable logic bricks when their owner gets too far
	from the active camera.  It was taken from some code lurking at the back of KX_Scene - who knows 
	what it does!
	
	Example::
		import GameLogic
		
		# get the scene
		scene = GameLogic.getCurrentScene()
		
		# print all the objects in the scene
		for obj in scene.objects:
			print obj.name
		
		# get an object named 'Cube'
		obj = scene.objects["Cube"]
		
		# get the first object in the scene.
		obj = scene.objects[0]
	
	Example::
		# Get the depth of an object in the camera view.
		import GameLogic
		
		obj = GameLogic.getCurrentController().owner
		cam = GameLogic.getCurrentScene().active_camera
		
		# Depth is negative and decreasing further from the camera
		depth = obj.position[0]*cam.world_to_camera[2][0] + obj.position[1]*cam.world_to_camera[2][1] + obj.position[2]*cam.world_to_camera[2][2] + cam.world_to_camera[2][3]
	
	@bug: All attributes are read only at the moment.
		
	@ivar name: The scene's name, (read-only).
	@type name: string
	@ivar objects: A list of objects in the scene, (read-only).
	@type objects: L{CListValue} of L{KX_GameObject}
	@ivar objectsInactive: A list of objects on background layers (used for the addObject actuator), (read-only).
	@type objectsInactive: L{CListValue} of L{KX_GameObject}
	@ivar lights: A list of lights in the scene, (read-only).
	@type lights: L{CListValue} of L{KX_LightObject}
	@ivar cameras: A list of cameras in the scene, (read-only).
	@type cameras: L{CListValue} of L{KX_Camera}
	@ivar active_camera: The current active camera.
						note: this can be set directly from python to avoid using the L{KX_SceneActuator}.
	@type active_camera: L{KX_Camera}
	@ivar suspended: True if the scene is suspended, (read-only).
	@type suspended: boolean
	@ivar activity_culling: True if the scene is activity culling
	@type activity_culling: boolean
	@ivar activity_culling_radius: The distance outside which to do activity culling.  Measured in manhattan distance.
	@type activity_culling_radius: float
	@ivar dbvt_culling: True when Dynamic Bounding box Volume Tree is set (read-only).
	@type dbvt_culling: bool
	@ivar pre_draw: A list of callables to be run before the render step.
	@type pre_draw: list
	@ivar post_draw: A list of callables to be run after the render step.
	@type post_draw: list
	@group Deprecated: getLightList, getObjectList, getName
	"""
	
	def getLightList():
		"""
		Returns the list of lights in the scene.
		
		@deprecated: Use the L{lights} attribute instead.
		@rtype: list [L{KX_LightObject}]
		"""
	def getObjectList():
		"""
		Returns the list of objects in the scene.
		
		@deprecated: Use the L{objects} attribute instead.
		@rtype: list [L{KX_GameObject}]
		"""
	def getName():
		"""
		Returns the name of the scene.
		
		@deprecated: Use the L{name} attribute instead.
		@rtype: string
		"""

	def addObject(object, other, time=0):
		"""
		Adds an object to the scene like the Add Object Actuator would, and returns the created object.
		
		@param object: The object to add
		@type object: L{KX_GameObject} or string
		@param other: The object's center to use when adding the object
		@type other: L{KX_GameObject} or string
		@param time: The lifetime of the added object, in frames. A time of 0 means the object will last forever.
		@type time: int
		
		@rtype: L{KX_GameObject}
		"""
		
	def end():
		"""
		Removes the scene from the game.
		"""
		
	def restart():
		"""
		Restarts the scene.
		"""
		
	def replace(scene):
		"""
		Replaces this scene with another one.
		
		@param scene: The name of the scene to replace this scene with.
		@type scene: string
		"""
		
	def suspend():
		"""
		Suspends this scene.
		"""
		
	def resume():
		"""
		Resume this scene.
		"""
	
	def get(key, default=None):
		"""
		Return the value matching key, or the default value if its not found.
		@return: The key value or a default.
		"""

class KX_SceneActuator(SCA_IActuator):
	"""
	Scene Actuator logic brick.
	
	@warning: Scene actuators that use a scene name will be ignored if at game start, the
	          named scene doesn't exist or is empty

		  This will generate a warning in the console:
		  
		  C{ERROR: GameObject I{Name} has a SceneActuator I{ActuatorName} (SetScene) without scene}
	
	@ivar scene: the name of the scene to change to/overlay/underlay/remove/suspend/resume
	@type scene: string.
	@ivar camera: the camera to change to.
	              When setting the attribute, you can use either a L{KX_Camera} or the name of the camera.
	@type camera: L{KX_Camera} on read, string or L{KX_Camera} on write
	@ivar useRestart: Set flag to True to restart the sene
	@type useRestart: bool
	@ivar mode: The mode of the actuator
	@type mode: int from 0 to 5 L{GameLogic.Scene Actuator}
	"""
#{ Deprecated
	def setUseRestart(flag):
		"""
		Set flag to True to restart the scene.
		
		@deprecated: Use the L{useRestart} attribute instead.
		@type flag: boolean
		"""
	def setScene(scene):
		"""
		Sets the name of the scene to change to/overlay/underlay/remove/suspend/resume.
		
		@deprecated: use the L{scene} attribute instead.
		@type scene: string
		"""
	def setCamera(camera):
		"""
		Sets the camera to change to.
		
		Camera can be either a L{KX_Camera} or the name of the camera.
		
		@deprecated: use the L{camera} attribute instead.
		@type camera: L{KX_Camera} or string
		"""
	def getUseRestart():
		"""
		Returns True if the scene will be restarted.
		
		@deprecated: use the L{useRestart} attribute instead.
		@rtype: boolean
		"""
	def getScene():
		"""
		Returns the name of the scene to change to/overlay/underlay/remove/suspend/resume.
		
		Returns an empty string ("") if no scene has been set.
		
		@deprecated: use the L{scene} attribute instead.
		@rtype: string
		"""
	def getCamera():
		"""
		Returns the name of the camera to change to.
		
		@deprecated: use the L{camera} attribute instead.
		@rtype: string
		"""
#}

class KX_SoundActuator(SCA_IActuator):
	"""
	Sound Actuator.
	
	The L{startSound()}, L{pauseSound()} and L{stopSound()} do not require
	the actuator to be activated - they act instantly provided that the actuator has
	been activated once at least.

	@ivar fileName: The filename of the sound this actuator plays.
	@type fileName: string

	@ivar volume: The volume (gain) of the sound.
	@type volume: float

	@ivar pitch: The pitch of the sound.
	@type pitch: float
	
	@ivar rollOffFactor: The roll off factor. Rolloff defines the rate of attenuation as the sound gets further away.
	@type rollOffFactor: float
	
	@ivar looping: The loop mode of the actuator.
	@type looping: integer
	
	@ivar position: The position of the sound as a list: [x, y, z].
	@type position: float array
	
	@ivar velocity: The velocity of the emitter as a list: [x, y, z]. The relative velocity to the observer determines the pitch. List of 3 floats: [x, y, z].
	@type velocity: float array
	
	@ivar orientation: The orientation of the sound. When setting the orientation you can also use quaternion [float,float,float,float] or euler angles [float,float,float]
	@type orientation: 3x3 matrix [[float]]
	
	@ivar mode: The operation mode of the actuator. You can use one of the following constants:
				- KX_SOUNDACT_PLAYSTOP               (1)
				- KX_SOUNDACT_PLAYEND                (2)
				- KX_SOUNDACT_LOOPSTOP               (3)
				- KX_SOUNDACT_LOOPEND                (4)
				- KX_SOUNDACT_LOOPBIDIRECTIONAL      (5)
				- KX_SOUNDACT_LOOPBIDIRECTIONAL_STOP (6)
	@type mode:	integer
	"""

#{ Play Methods
	def startSound():
		"""
		Starts the sound.
		"""
	def pauseSound():
		"""
		Pauses the sound.
		"""
	def stopSound():
		"""
		Stops the sound.
		"""
#}

#{ Deprecated
	def setFilename(filename):
		"""
		Sets the filename of the sound this actuator plays.
		
		@deprecated: Use the L{fileName} attribute instead.
		@type filename: string
		"""
	def getFilename():
		"""
		Returns the filename of the sound this actuator plays.
		
		@deprecated: Use the L{fileName} attribute instead.
		@rtype: string
		"""
	def setGain(gain):
		"""
		Sets the gain (volume) of the sound
		
		@deprecated: Use the L{volume} attribute instead.
		@type gain: float
		@param gain: 0.0 (quiet) <= gain <= 1.0 (loud)
		"""
	def getGain():
		"""
		Gets the gain (volume) of the sound.
		
		@deprecated: Use the L{volume} attribute instead.
		@rtype: float
		"""
	def setPitch(pitch):
		"""
		Sets the pitch of the sound.
		
		@deprecated: Use the L{pitch} attribute instead.
		@type pitch: float
		"""
	def getPitch():
		"""
		Returns the pitch of the sound.
		
		@deprecated: Use the L{pitch} attribute instead.
		@rtype: float
		"""
	def setRollOffFactor(rolloff):
		"""
		Sets the rolloff factor for the sounds.
		
		Rolloff defines the rate of attenuation as the sound gets further away.
		Higher rolloff factors shorten the distance at which the sound can be heard.
		
		@deprecated: Use the L{rollOffFactor} attribute instead.
		@type rolloff: float
		"""
	def getRollOffFactor():
		"""
		Returns the rolloff factor for the sound.
		
		@deprecated: Use the L{rollOffFactor} attribute instead.
		@rtype: float
		"""
	def setLooping(loop):
		"""
		Sets the loop mode of the actuator.
		
		@bug: There are no constants defined for this method!
		@param loop: - Play Stop	1
					 - Play End		2
					 - Loop Stop	3
					 - Loop End		4
					 - Bidirection Stop	5
					 - Bidirection End	6

		@deprecated: Use the L{looping} attribute instead.
		@type loop: integer
		"""
	def getLooping():
		"""
		Returns the current loop mode of the actuator.
		
		@deprecated: Use the L{looping} attribute instead.
		@rtype: integer
		"""
	def setPosition(x, y, z):
		"""
		Sets the position this sound will come from.
		
		@deprecated: Use the L{position} attribute instead.
		@type x: float
		@param x: The x coordinate of the sound.
		@type y: float
		@param y: The y coordinate of the sound.
		@type z: float
		@param z: The z coordinate of the sound.
		"""
	def setVelocity(vx, vy, vz):
		"""
		Sets the velocity this sound is moving at.  
		
		The sound's pitch is determined from the velocity.
		
		@deprecated: Use the L{velocity} attribute instead.
		@type vx: float
		@param vx: The vx coordinate of the sound.
		@type vy: float
		@param vy: The vy coordinate of the sound.
		@type vz: float
		@param vz: The vz coordinate of the sound.
		"""
	def setOrientation(o11, o12, o13, o21, o22, o23, o31, o32, o33):
		"""
		Sets the orientation of the sound.
		
		The nine parameters specify a rotation matrix::
			| o11, o12, o13 |
			| o21, o22, o23 |
			| o31, o32, o33 |
		@deprecated: Use the L{orientation} attribute instead.
		"""
	
	def setType(mode):
		"""
		Sets the operation mode of the actuator.
		
		@deprecated: Use the L{type} attribute instead.
		@param mode: KX_SOUNDACT_PLAYSTOP, KX_SOUNDACT_PLAYEND, KX_SOUNDACT_LOOPSTOP, KX_SOUNDACT_LOOPEND, KX_SOUNDACT_LOOPBIDIRECTIONAL, KX_SOUNDACT_LOOPBIDIRECTIONAL_STOP
		@type mode: integer
		"""

	def getType():
		"""
		Returns the operation mode of the actuator.
		
		@deprecated: Use the L{type} attribute instead.
		@rtype: integer
		@return:  KX_SOUNDACT_PLAYSTOP, KX_SOUNDACT_PLAYEND, KX_SOUNDACT_LOOPSTOP, KX_SOUNDACT_LOOPEND, KX_SOUNDACT_LOOPBIDIRECTIONAL, KX_SOUNDACT_LOOPBIDIRECTIONAL_STOP
		"""
#}

class KX_StateActuator(SCA_IActuator):
	"""
	State actuator changes the state mask of parent object.
	
	Property:
	
	@ivar operation: type of bit operation to be applied on object state mask.
					You can use one of the following constant:
						- KX_STATE_OP_CPY (0) : Copy state mask
						- KX_STATE_OP_SET (1) : Add bits to state mask
						- KX_STATE_OP_CLR (2) : Substract bits to state mask
						- KX_STATE_OP_NEG (3) : Invert bits to state mask
	@type operation: integer
	
	@ivar mask: value that defines the bits that will be modified by the operation.
				The bits that are 1 in the mask will be updated in the object state,
				the bits that are 0 are will be left unmodified expect for the Copy operation
				which copies the mask to the object state
	@type mask: integer
	"""
	def setOperation(op):
		"""
		Set the type of bit operation to be applied on object state mask.
		Use setMask() to specify the bits that will be modified.
		
		@deprecated: Use the L{operation} attribute instead.
		@param op: bit operation (0=Copy, 1=Add, 2=Substract, 3=Invert)
		@type op: integer
		"""
	def setMask(mask):
		"""
		Set the value that defines the bits that will be modified by the operation.
		The bits that are 1 in the value will be updated in the object state,
		the bits that are 0 are will be left unmodified expect for the Copy operation
		which copies the value to the object state.
		
		@deprecated: Use the L{mask} attribute instead.
		@param mask: bits that will be modified
		@type mask: integer
		"""

class KX_TrackToActuator(SCA_IActuator):
	"""
	Edit Object actuator in Track To mode.
	
	@warning: Track To Actuators will be ignored if at game start, the
		object to track to is invalid.
		
		This will generate a warning in the console:
		
		C{ERROR: GameObject I{Name} no object in EditObjectActuator I{ActuatorName}}

	@ivar object: the object this actuator tracks.
	@type object: KX_GameObject or None
	@ivar time: the time in frames with which to delay the tracking motion
	@type time: integer
	@ivar use3D: the tracking motion to use 3D
	@type use3D: boolean
	
	"""
#{ Deprecated
	def setObject(object):
		"""
		Sets the object to track.
		
		@deprecated: Use the L{object} attribute instead.
		@type object: L{KX_GameObject}, string or None
		@param object: Either a reference to a game object or the name of the object to track.
		"""
	def getObject(name_only):
		"""
		Returns the name of the object to track.
		
		@deprecated: Use the L{object} attribute instead.
		@type name_only: bool
		@param name_only: optional argument, when 0 return a KX_GameObject
		@rtype: string, KX_GameObject or None if no object is set
		"""
	def setTime(time):
		"""
		Sets the time in frames with which to delay the tracking motion.
		
		@deprecated: Use the L{time} attribute instead.
		@type time: integer
		"""
	def getTime():
		"""
		Returns the time in frames with which the tracking motion is delayed.
		
		@deprecated: Use the L{time} attribute instead.
		@rtype: integer
		"""
	def setUse3D(use3d):
		"""
		DEPRECATED: Use the  property.
		Sets the tracking motion to use 3D.
		
		@deprecated: Use the L{use3D} attribute instead.
		@type use3d: boolean
		@param use3d: - True: allow the tracking motion to extend in the z-direction.
		              - False: lock the tracking motion to the x-y plane.
		"""
	def getUse3D():
		"""
		Returns True if the tracking motion will track in the z direction.
		
		@deprecated: Use the L{use3D} attribute instead.
		@rtype: boolean
		"""
#}

class KX_VehicleWrapper(PyObjectPlus):
	"""
	KX_VehicleWrapper
	
	TODO - description
	"""
	
	def addWheel(wheel, attachPos, attachDir, axleDir, suspensionRestLength, wheelRadius, hasSteering):
		
		"""
		Add a wheel to the vehicle
		
		@param wheel: The object to use as a wheel.
		@type wheel: L{KX_GameObject} or a KX_GameObject name
		@param attachPos: The position that this wheel will attach to.
		@type attachPos: vector of 3 floats
		@param attachDir: The direction this wheel points.
		@type attachDir: vector of 3 floats
		@param axleDir: The direction of this wheels axle.
		@type axleDir: vector of 3 floats
		@param suspensionRestLength: TODO - Description
		@type suspensionRestLength: float
		@param wheelRadius: The size of the wheel.
		@type wheelRadius: float
		"""

	def applyBraking(force, wheelIndex):
		"""
		Apply a braking force to the specified wheel
		
		@param force: the brake force
		@type force: float
		
		@param wheelIndex: index of the wheel where the force needs to be applied
		@type wheelIndex: integer
		"""
	def applyEngineForce(force, wheelIndex):
		"""
		Apply an engine force to the specified wheel
		
		@param force: the engine force
		@type force: float
		
		@param wheelIndex: index of the wheel where the force needs to be applied
		@type wheelIndex: integer
		"""
	def getConstraintId():
		"""
		Get the constraint ID
		
		@rtype: integer
		@return: the constraint id
		"""
	def getConstraintType():
		"""
		Returns the constraint type.
		
		@rtype: integer
		@return: constraint type
		"""
	def getNumWheels():
		"""
		Returns the number of wheels.
		
		@rtype: integer
		@return: the number of wheels for this vehicle
		"""
	def getWheelOrientationQuaternion(wheelIndex):
		"""
		Returns the wheel orientation as a quaternion.
		
		@param wheelIndex: the wheel index
		@type wheelIndex: integer
		
		@rtype: TODO - type should be quat as per method name but from the code it looks like a matrix
		@return: TODO Description
		"""
	def getWheelPosition(wheelIndex):
		"""
		Returns the position of the specified wheel
		
		@param wheelIndex: the wheel index
		@type wheelIndex: integer
		
		@rtype: list[x, y, z]
		@return: position vector
		"""
	def getWheelRotation(wheelIndex):
		"""
		Returns the rotation of the specified wheel
		
		@param wheelIndex: the wheel index
		@type wheelIndex: integer
		
		@rtype: float
		@return: the wheel rotation
		"""
	def setRollInfluence(rollInfluece, wheelIndex):
		"""
		Set the specified wheel's roll influence.
		The higher the roll influence the more the vehicle will tend to roll over in corners.
		
		@param rollInfluece: the wheel roll influence
		@type rollInfluece: float

		@param wheelIndex: the wheel index
		@type wheelIndex: integer
		"""
	def setSteeringValue(steering, wheelIndex):
		"""
		Set the specified wheel's steering
		
		@param steering: the wheel steering
		@type steering: float

		@param wheelIndex: the wheel index
		@type wheelIndex: integer
		"""
	def setSuspensionCompression(compression, wheelIndex):
		"""
		Set the specified wheel's compression
		
		@param compression: the wheel compression
		@type compression: float

		@param wheelIndex: the wheel index
		@type wheelIndex: integer
		"""
	def setSuspensionDamping(damping, wheelIndex):
		"""
		Set the specified wheel's damping
		
		@param damping: the wheel damping
		@type damping: float

		@param wheelIndex: the wheel index
		@type wheelIndex: integer
		"""
	def setSuspensionStiffness(stiffness, wheelIndex):
		"""
		Set the specified wheel's stiffness
		
		@param stiffness: the wheel stiffness
		@type stiffness: float

		@param wheelIndex: the wheel index
		@type wheelIndex: integer
		"""
	def setTyreFriction(friction, wheelIndex):
		"""
		Set the specified wheel's tyre friction
		
		@param friction: the tyre friction
		@type friction: float

		@param wheelIndex: the wheel index
		@type wheelIndex: integer
		"""

class KX_VertexProxy(SCA_IObject):
	"""
	A vertex holds position, UV, colour and normal information.
	
	Note:
	The physics simulation is NOT currently updated - physics will not respond
	to changes in the vertex position.
	
	@ivar XYZ: The position of the vertex.
	@type XYZ: list [x, y, z]
	@ivar UV: The texture coordinates of the vertex.
	@type UV: list [u, v]
	@ivar normal: The normal of the vertex 
	@type normal: list [nx, ny, nz]
	@ivar colour: The colour of the vertex. 
	              Black = [0.0, 0.0, 0.0, 1.0], White = [1.0, 1.0, 1.0, 1.0]
	@type colour: list [r, g, b, a]
	@ivar color: Synonym for colour.
	
	@group Position: x, y, z
	@ivar x: The x coordinate of the vertex.
	@type x: float
	@ivar y: The y coordinate of the vertex.
	@type y: float
	@ivar z: The z coordinate of the vertex.
	@type z: float
	
	@group Texture Coordinates: u, v
	@ivar u: The u texture coordinate of the vertex.
	@type u: float
	@ivar v: The v texture coordinate of the vertex.
	@type v: float
	
	@ivar u2: The second u texture coordinate of the vertex.
	@type u2: float
	@ivar v2: The second v texture coordinate of the vertex.
	@type v2: float
	
	@group Colour: r, g, b, a
	@ivar r: The red component of the vertex colour.   0.0 <= r <= 1.0
	@type r: float
	@ivar g: The green component of the vertex colour. 0.0 <= g <= 1.0
	@type g: float
	@ivar b: The blue component of the vertex colour.  0.0 <= b <= 1.0
	@type b: float
	@ivar a: The alpha component of the vertex colour. 0.0 <= a <= 1.0
	@type a: float
	"""
	
	def getXYZ():
		"""
		Gets the position of this vertex.
		
		@rtype: list [x, y, z]
		@return: this vertexes position in local coordinates.
		"""
	def setXYZ(pos):
		"""
		Sets the position of this vertex.
		
		@type pos: list [x, y, z]
		@param pos: the new position for this vertex in local coordinates.
		"""
	def getUV():
		"""
		Gets the UV (texture) coordinates of this vertex.
		
		@rtype: list [u, v]
		@return: this vertexes UV (texture) coordinates.
		"""
	def setUV(uv):
		"""
		Sets the UV (texture) coordinates of this vertex.
		
		@type uv: list [u, v]
		"""
	def getUV2():
		"""
		Gets the 2nd UV (texture) coordinates of this vertex.
		
		@rtype: list [u, v]
		@return: this vertexes UV (texture) coordinates.
		"""
	def setUV2(uv, unit):
		"""
		Sets the 2nd UV (texture) coordinates of this vertex.
		
		@type uv: list [u, v]
		@param unit: optional argument, FLAT==1, SECOND_UV==2, defaults to SECOND_UV
		@param unit:  int
		"""
	def getRGBA():
		"""
		Gets the colour of this vertex.
		
		The colour is represented as four bytes packed into an integer value.  The colour is 
		packed as RGBA.
		
		Since Python offers no way to get each byte without shifting, you must use the struct module to
		access colour in an machine independent way.
		
		Because of this, it is suggested you use the r, g, b and a attributes or the colour attribute instead.
		
		Example::
			import struct;
			col = struct.unpack('4B', struct.pack('I', v.getRGBA()))
			# col = (r, g, b, a)
			# black = (  0,   0,   0, 255)
			# white = (255, 255, 255, 255)
		
		@rtype: integer
		@return: packed colour. 4 byte integer with one byte per colour channel in RGBA format.
		"""
	def setRGBA(col):
		"""
		Sets the colour of this vertex.
		
		See getRGBA() for the format of col, and its relevant problems.  Use the r, g, b and a attributes
		or the colour attribute instead.
		
		setRGBA() also accepts a four component list as argument col.  The list represents the colour as [r, g, b, a]
		with black = [0.0, 0.0, 0.0, 1.0] and white = [1.0, 1.0, 1.0, 1.0]
		
		Example::
			v.setRGBA(0xff0000ff) # Red
			v.setRGBA(0xff00ff00) # Green on little endian, transparent purple on big endian
			v.setRGBA([1.0, 0.0, 0.0, 1.0]) # Red
			v.setRGBA([0.0, 1.0, 0.0, 1.0]) # Green on all platforms.
		
		@type col: integer or list [r, g, b, a]
		@param col: the new colour of this vertex in packed RGBA format.
		"""
	def getNormal():
		"""
		Gets the normal vector of this vertex.
		
		@rtype: list [nx, ny, nz]
		@return: normalised normal vector.
		"""
	def setNormal(normal):
		"""
		Sets the normal vector of this vertex.

		@type normal: sequence of floats [r, g, b]
		@param normal: the new normal of this vertex.
		"""

class KX_VisibilityActuator(SCA_IActuator):
	"""
	Visibility Actuator.
	@ivar visibility: whether the actuator makes its parent object visible or invisible
	@type visibility: boolean
	@ivar useOcclusion: whether the actuator makes its parent object an occluder or not
	@type useOcclusion: boolean
	@ivar useRecursion: whether the visibility/occlusion should be propagated to all children of the object
	@type useRecursion: boolean
	"""
#{ Deprecated
	def set(visible):
		"""
		Sets whether the actuator makes its parent object visible or invisible.
		
		@deprecated: Use the L{visibility} attribute instead.
		@param visible: - True: Makes its parent visible.
		                - False: Makes its parent invisible.
		"""
#}

class SCA_2DFilterActuator(SCA_IActuator):
	"""
	Create, enable and disable 2D filters
	
	Properties:
	
	The following properties don't have an immediate effect. 
	You must active the actuator to get the result.
	The actuator is not persistent: it automatically stops itself after setting up the filter
	but the filter remains active. To stop a filter you must activate the actuator with 'type'
	set to RAS_2DFILTER_DISABLED or RAS_2DFILTER_NOFILTER.
	
	@ivar shaderText: shader source code for custom shader
	@type shaderText: string
	@ivar disableMotionBlur: action on motion blur: 0=enable, 1=disable
	@type disableMotionBlur: integer
	@ivar mode: type of 2D filter, use one of the following constants:
				RAS_2DFILTER_ENABLED      (-2) : enable the filter that was previously disabled
				RAS_2DFILTER_DISABLED     (-1) : disable the filter that is currently active
				RAS_2DFILTER_NOFILTER      (0) : disable and destroy the filter that is currently active
				RAS_2DFILTER_MOTIONBLUR    (1) : create and enable preset filters
				RAS_2DFILTER_BLUR          (2)
				RAS_2DFILTER_SHARPEN       (3)
				RAS_2DFILTER_DILATION      (4)
				RAS_2DFILTER_EROSION       (5)
				RAS_2DFILTER_LAPLACIAN     (6)
				RAS_2DFILTER_SOBEL         (7)
				RAS_2DFILTER_PREWITT       (8)
				RAS_2DFILTER_GRAYSCALE     (9)
				RAS_2DFILTER_SEPIA        (10)
				RAS_2DFILTER_INVERT       (11)
				RAS_2DFILTER_CUSTOMFILTER (12) : customer filter, the code code is set via shaderText property
	@type mode: integer				
	@ivar passNumber: order number of filter in the stack of 2D filters. Filters are executed in increasing order of passNb.
	              Only be one filter can be defined per passNb.
	@type passNumber: integer (0-100)
	@ivar value: argument for motion blur filter
	@type value: float (0.0-100.0)
	"""

class SCA_ANDController(SCA_IController):
	"""
	An AND controller activates only when all linked sensors are activated.
	
	There are no special python methods for this controller.
	"""

class SCA_ActuatorSensor(SCA_ISensor):
	"""
	Actuator sensor detect change in actuator state of the parent object.
	It generates a positive pulse if the corresponding actuator is activated
	and a negative pulse if the actuator is deactivated.
	
	Properties:
	
	@ivar actuator: the name of the actuator that the sensor is monitoring.
	@type actuator: string
	"""
#{Deprecated
	def getActuator():
		"""
		Return the Actuator with which the sensor operates.
		
		@deprecated: Use the L{actuator} attribute instead.
		@rtype: string
		"""
	def setActuator(name):
		"""
		Sets the Actuator with which to operate. If there is no Actuator
		of this name, the function has no effect.
		
		@deprecated: Use the L{actuator} attribute instead.
		@param name: actuator name
		@type name: string
		"""
#}

class SCA_AlwaysSensor(SCA_ISensor):
	"""
	This sensor is always activated.
	"""

class SCA_DelaySensor(SCA_ISensor):
	"""
	The Delay sensor generates positive and negative triggers at precise time,
	expressed in number of frames. The delay parameter defines the length
	of the initial OFF period. A positive trigger is generated at the end of this period. 
	The duration parameter defines the length of the ON period following the OFF period.
	There is a negative trigger at the end of the ON period. If duration is 0, the sensor
	stays ON and there is no negative trigger.
	The sensor runs the OFF-ON cycle once unless the repeat option is set: the
	OFF-ON cycle repeats indefinately (or the OFF cycle if duration is 0).
	Use SCA_ISensor::reset() at any time to restart sensor.

	Properties:
	
	@ivar delay: length of the initial OFF period as number of frame, 0 for immediate trigger.
	@type delay: integer.
	@ivar duration: length of the ON period in number of frame after the initial OFF period.
	                If duration is greater than 0, a negative trigger is sent at the end of the ON pulse.
	@type duration: integer
	@ivar repeat: 1 if the OFF-ON cycle should be repeated indefinately, 0 if it should run once.
	@type repeat: integer
	"""
#{Deprecated
	def setDelay(delay):
		"""
		Set the initial delay before the positive trigger.
		
		@deprecated: Use the L{delay} attribute instead.
		@param delay: length of the initial OFF period as number of frame, 0 for immediate trigger
		@type delay: integer
		"""
	def setDuration(duration):
		"""
		Set the duration of the ON pulse after initial delay and the generation of the positive trigger.
		If duration is greater than 0, a negative trigger is sent at the end of the ON pulse.
		
		@deprecated: Use the L{duration} attribute instead.
		@param duration: length of the ON period in number of frame after the initial OFF period
		@type duration: integer
		"""	
	def setRepeat(repeat):
		"""
		Set if the sensor repeat mode.
		
		@deprecated: Use the L{repeat} attribute instead.
		@param repeat: 1 if the OFF-ON cycle should be repeated indefinately, 0 if it should run once.
		@type repeat: integer
		"""		
	def getDelay():
		"""
		Return the delay parameter value.
		
		@deprecated: Use the L{delay} attribute instead.
		@rtype: integer
		"""
	def getDuration():
		"""
		Return the duration parameter value
		
		@deprecated: Use the L{duration} attribute instead.
		@rtype: integer
		"""
	def getRepeat():
		"""
		Return the repeat parameter value
		
		@deprecated: Use the L{repeat} attribute instead.
		@rtype: KX_TRUE or KX_FALSE
		"""
#}

class SCA_JoystickSensor(SCA_ISensor):
	"""
	This sensor detects player joystick events.
	
	Properties:
	
	@ivar axisValues: (read-only) The state of the joysticks axis as a list of values L{numAxis} long.
						each spesifying the value of an axis between -32767 and 32767 depending on how far the axis is pushed, 0 for nothing. 
						The first 2 values are used by most joysticks and gamepads for directional control. 3rd and 4th values are only on some joysticks and can be used for arbitary controls.
						left:[-32767, 0, ...], right:[32767, 0, ...], up:[0, -32767, ...], down:[0, 32767, ...]
	@type axisValues: list of ints
	
	@ivar axisSingle: (read-only) like L{axisValues} but returns a single axis value that is set by the sensor.
						Only use this for "Single Axis" type sensors otherwise it will raise an error.
	@type axisSingle: int
	
	@ivar hatValues: (read-only) The state of the joysticks hats as a list of values L{numHats} long.
						each spesifying the direction of the hat from 1 to 12, 0 when inactive. 
						Hat directions are as follows...
							- 0:None
							- 1:Up
							- 2:Right
							- 4:Down
							- 8:Left
							- 3:Up - Right
							- 6:Down - Right
							- 12:Down - Left
							- 9:Up - Left
	
	@type hatValues: list of ints
	
	@ivar hatSingle: (read-only) like L{hatValues} but returns a single hat direction value that is set by the sensor.
	@type hatSingle: int
	
	@ivar numAxis: (read-only) The number of axes for the joystick at this index.
	@type numAxis: integer
	@ivar numButtons: (read-only) The number of buttons for the joystick at this index.
	@type numButtons: integer
	@ivar numHats: (read-only) The number of hats for the joystick at this index.
	@type numHats: integer
	@ivar connected: (read-only) True if a joystick is connected at this joysticks index.
	@type connected: boolean
	@ivar index: The joystick index to use (from 0 to 7). The first joystick is always 0.
	@type index: integer
	@ivar threshold: Axis threshold. Joystick axis motion below this threshold wont trigger an event. Use values between (0 and 32767), lower values are more sensitive.
	@type threshold: integer
	@ivar button: The button index the sensor reacts to (first button = 0). When the "All Events" toggle is set, this option has no effect.
	@type button: integer
	@ivar axis: The axis this sensor reacts to, as a list of two values [axisIndex, axisDirection]
	            axisIndex: the axis index to use when detecting axis movement, 1=primary directional control, 2=secondary directional control.
	            axisDirection: 0=right, 1=up, 2=left, 3=down
	@type axis: [integer, integer]
	@ivar hat: The hat the sensor reacts to, as a list of two values: [hatIndex, hatDirection]
	            hatIndex: the hat index to use when detecting hat movement, 1=primary hat, 2=secondary hat (4 max).
	            hatDirection: 1-12
	@type hat: [integer, integer]
	"""
	
	def getButtonActiveList():
		"""
		Returns a list containing the indicies of the currently pressed buttons.
		@rtype: list
		"""
	def getButtonStatus(buttonIndex):
		"""
		Returns a bool of the current pressed state of the specified button.
		@param buttonIndex: the button index, 0=first button
		@type buttonIndex: integer
		@rtype: bool
		"""
#{Deprecated
	def getIndex():
		"""
		Returns the joystick index to use (from 1 to 8).
		
		@deprecated: Use the L{index} attribute instead.
		@rtype: integer
		"""
	def setIndex(index):
		"""
		Sets the joystick index to use. 
		
		@deprecated: Use the L{index} attribute instead.
		@param index: The index of this joystick sensor, Clamped between 1 and 8.
		@type index: integer
		@note: This is only useful when you have more then 1 joystick connected to your computer - multiplayer games.
		"""
	def getAxis():
		"""
		Returns the current axis this sensor reacts to. See L{getAxisValue()<SCA_JoystickSensor.getAxisValue>} for the current axis state.
		
		@deprecated: Use the L{axis} attribute instead.
		@rtype: list
		@return: 2 values returned are [axisIndex, axisDirection] - see L{setAxis()<SCA_JoystickSensor.setAxis>} for their purpose.
		@note: When the "All Events" toggle is set, this option has no effect.
		"""
	def setAxis(axisIndex, axisDirection):
		"""
		@deprecated: Use the L{axis} attribute instead.
		@param axisIndex: Set the axis index to use when detecting axis movement.
		@type axisIndex: integer from 1 to 2
		@param axisDirection: Set the axis direction used for detecting motion. 0:right, 1:up, 2:left, 3:down.
		@type axisDirection: integer from 0 to 3
		@note: When the "All Events" toggle is set, this option has no effect.
		"""
	def getAxisValue():
		"""
		Returns the state of the joysticks axis. See differs to L{getAxis()<SCA_JoystickSensor.getAxis>} returning the current state of the joystick.
		
		@deprecated: Use the L{axisValues} attribute instead.
		@rtype: list
		@return: 4 values, each spesifying the value of an axis between -32767 and 32767 depending on how far the axis is pushed, 0 for nothing. 

			The first 2 values are used by most joysticks and gamepads for directional control. 3rd and 4th values are only on some joysticks and can be used for arbitary controls.

			left:[-32767, 0, ...], right:[32767, 0, ...], up:[0, -32767, ...], down:[0, 32767, ...]
		@note: Some gamepads only set the axis on and off like a button.
		"""
	def getThreshold():
		"""
		Get the axis threshold. See L{setThreshold()<SCA_JoystickSensor.setThreshold>} for details.
		
		@deprecated: Use the L{threshold} attribute instead.
		@rtype: integer
		"""
	def setThreshold(threshold):
		"""
		Set the axis threshold.
		
		@deprecated: Use the L{threshold} attribute instead.
		@param threshold: Joystick axis motion below this threshold wont trigger an event. Use values between (0 and 32767), lower values are more sensitive.
		@type threshold: integer
		"""
	def getButton():
		"""
		Returns the button index the sensor reacts to. See L{getButtonValue()<SCA_JoystickSensor.getButtonValue>} for a list of pressed buttons.
		
		@deprecated: Use the L{button} attribute instead.
		@rtype: integer
		@note: When the "All Events" toggle is set, this option has no effect.
		"""
	def setButton(index):
		"""
		Sets the button index the sensor reacts to when the "All Events" option is not set.
		
		@deprecated: Use the L{button} attribute instead.
		@note: When the "All Events" toggle is set, this option has no effect.
		"""
	def getButtonValue():
		"""
		Returns a list containing the indicies of the currently pressed buttons.
		
		@deprecated: Use the L{getButtonActiveList} method instead.
		@rtype: list
		"""
	def getHat():
		"""
		Returns the current hat direction this sensor is set to.
		[hatNumber, hatDirection].
		
		@deprecated: Use the L{hat} attribute instead.
		@rtype: list
		@note: When the "All Events" toggle is set, this option has no effect.
		"""
	def setHat(index,direction):
		"""
		Sets the hat index the sensor reacts to when the "All Events" option is not set.
		
		@deprecated: Use the L{hat} attribute instead.
		@type index: integer
		"""
	def getNumAxes():
		"""
		Returns the number of axes for the joystick at this index.
		
		@deprecated: Use the L{numAxis} attribute instead.
		@rtype: integer
		"""
	def getNumButtons():
		"""
		Returns the number of buttons for the joystick at this index.
		
		@deprecated: Use the L{numButtons} attribute instead.
		@rtype: integer
		"""
	def getNumHats():
		"""
		Returns the number of hats for the joystick at this index.
		
		@deprecated: Use the L{numHats} attribute instead.
		@rtype: integer
		"""
	def isConnected():
		"""
		Returns True if a joystick is detected at this joysticks index.
		
		@deprecated: Use the L{connected} attribute instead.
		@rtype: bool
		"""
#}

class SCA_KeyboardSensor(SCA_ISensor):
	"""
	A keyboard sensor detects player key presses.
	
	See module L{GameKeys} for keycode values.
	
	@ivar key: The key code this sensor is looking for.
	@type key: keycode from L{GameKeys} module
	@ivar hold1: The key code for the first modifier this sensor is looking for.
	@type hold1: keycode from L{GameKeys} module
	@ivar hold2: The key code for the second modifier this sensor is looking for.
	@type hold2: keycode from L{GameKeys} module
	@ivar toggleProperty: The name of the property that indicates whether or not to log keystrokes as a string.
	@type toggleProperty: string
	@ivar targetProperty: The name of the property that receives keystrokes in case in case a string is logged.
	@type targetProperty: string
	@ivar useAllKeys: Flag to determine whether or not to accept all keys.
	@type useAllKeys: boolean
	@ivar events: a list of pressed keys that have either been pressed, or just released, or are active this frame. (read-only).

			- 'keycode' matches the values in L{GameKeys}.
			- 'status' uses...
				- L{GameLogic.KX_INPUT_NONE}
				- L{GameLogic.KX_INPUT_JUST_ACTIVATED}
				- L{GameLogic.KX_INPUT_ACTIVE}
				- L{GameLogic.KX_INPUT_JUST_RELEASED}
			
	@type events: list [[keycode, status], ...]
	"""
	
	def getKeyStatus(keycode):
		"""
		Get the status of a key.
		
		@rtype: key state L{GameLogic} members (KX_INPUT_NONE, KX_INPUT_JUST_ACTIVATED, KX_INPUT_ACTIVE, KX_INPUT_JUST_RELEASED)
		@return: The state of the given key
		@type keycode: integer
		@param keycode: The code that represents the key you want to get the state of
		"""
	
#{Deprecated
	def getKey():
		"""
		Returns the key code this sensor is looking for.
		
		@deprecated: Use the L{key} attribute instead.
		@rtype: keycode from L{GameKeys} module
		"""
	
	def setKey(keycode):
		"""
		Set the key this sensor should listen for.
		
		@deprecated: Use the L{key} attribute instead.
		@type keycode: keycode from L{GameKeys} module
		"""
	
	def getHold1():
		"""
		Returns the key code for the first modifier this sensor is looking for.
		
		@deprecated: Use the L{hold1} attribute instead.
		@rtype: keycode from L{GameKeys} module
		"""
	
	def setHold1(keycode):
		"""
		Sets the key code for the first modifier this sensor should look for.
		
		@deprecated: Use the L{hold1} attribute instead.
		@type keycode: keycode from L{GameKeys} module
		"""
	
	def getHold2():
		"""
		Returns the key code for the second modifier this sensor is looking for.
		
		@deprecated: Use the L{hold2} attribute instead.
		@rtype: keycode from L{GameKeys} module
		"""
	
	def setHold2(keycode):
		"""
		Sets the key code for the second modifier this sensor should look for.
		
		@deprecated: Use the L{hold2} attribute instead.
		@type keycode: keycode from L{GameKeys} module
		"""
	
	def getPressedKeys():
		"""
		Get a list of keys that have either been pressed, or just released this frame.
		
		@deprecated: Use the L{events} attribute instead.
		@rtype: list of key status. [[keycode, status]]
		"""
	
	def getCurrentlyPressedKeys():
		"""
		Get a list of currently pressed keys that have either been pressed, or just released
		
		@deprecated: Use the L{events} attribute instead.
		@rtype: list of key status. [[keycode, status]]
		"""
#}

class SCA_NANDController(SCA_IController):
	"""
	An NAND controller activates when all linked sensors are not active.
	
	There are no special python methods for this controller.
	"""

class SCA_NORController(SCA_IController):
	"""
	An NOR controller activates only when all linked sensors are de-activated.
	
	There are no special python methods for this controller.
	"""

class SCA_ORController(SCA_IController):
	"""
	An OR controller activates when any connected sensor activates.
	
	There are no special python methods for this controller.
	"""

class SCA_PropertyActuator(SCA_IActuator):
	"""
	Property Actuator

	Properties:
	
	@ivar propName: the property on which to operate.
	@type propName: string
	@ivar value: the value with which the actuator operates.
	@type value: string
	@ivar mode: TODO - add constants to game logic dict!.
	@type mode: int
	"""
#{ Deprecated
	def setProperty(prop):
		"""
		Set the property on which to operate. 
		
		If there is no property of this name, the call is ignored.
		
		@deprecated: Use the L{propName} attribute instead.
		@type prop: string
		@param prop: The name of the property to set.
		"""
	def getProperty():
		"""
		Returns the name of the property on which to operate.
		
		@deprecated: Use the L{propName} attribute instead.
		@rtype: string
		"""
	def setValue(value):
		"""
		Set the value with which the actuator operates. 
		
		If the value is not compatible with the type of the 
		property, the subsequent action is ignored.
		
		@deprecated: Use the L{value} attribute instead.
		@type value: string
		"""
	def getValue():
		"""
		Gets the value with which this actuator operates.
		
		@deprecated: Use the L{value} attribute instead.
		@rtype: string
		"""
#}

class SCA_PropertySensor(SCA_ISensor):
	"""
	Activates when the game object property matches.
	
	Properties:
	
	@ivar mode: type of check on the property: 
	            KX_PROPSENSOR_EQUAL(1), KX_PROPSENSOR_NOTEQUAL(2), KX_PROPSENSOR_INTERVAL(3), 
	            KX_PROPSENSOR_CHANGED(4), KX_PROPSENSOR_EXPRESSION(5)
	@type mode: integer
	@ivar propName: the property the sensor operates.
	@type propName: string
	@ivar value: the value with which the sensor compares to the value of the property.
	@type value: string
	@ivar min: the minimum value of the range used to evaluate the property when in interval mode.
	@type min: string
	@ivar max: the maximum value of the range used to evaluate the property when in interval mode.
	@type max: string
	"""
#{ Deprecated
	def getType():
		"""
		Gets when to activate this sensor.
		
		@deprecated: Use the L{mode} attribute instead.
		@return: KX_PROPSENSOR_EQUAL, KX_PROPSENSOR_NOTEQUAL,
			 KX_PROPSENSOR_INTERVAL, KX_PROPSENSOR_CHANGED,
			 or KX_PROPSENSOR_EXPRESSION.
		"""

	def setType(checktype):
		"""
		Set the type of check to perform.
		
		@deprecated: Use the L{mode} attribute instead.
		@type checktype: KX_PROPSENSOR_EQUAL, KX_PROPSENSOR_NOTEQUAL,
			KX_PROPSENSOR_INTERVAL, KX_PROPSENSOR_CHANGED,
			or KX_PROPSENSOR_EXPRESSION.
		"""
	
	def getProperty():
		"""
		Return the property with which the sensor operates.
		
		@deprecated: Use the L{propName} attribute instead.
		@rtype: string
		@return: the name of the property this sensor is watching.
		"""
	def setProperty(name):
		"""
		Sets the property with which to operate.  If there is no property
		of that name, this call is ignored.
		
		@deprecated: Use the L{propName} attribute instead.
		@type name: string.
		"""
	def getValue():
		"""
		Return the value with which the sensor compares to the value of the property.
		
		@deprecated: Use the L{value} attribute instead.
		@rtype: string
		@return: the value of the property this sensor is watching.
		"""
	def setValue(value):
		"""
		Set the value with which the sensor operates. If the value
		is not compatible with the type of the property, the subsequent
		action is ignored.
		
		@deprecated: Use the L{value} attribute instead.
		@type value: string
		"""
#}

class SCA_PythonController(SCA_IController):
	"""
	A Python controller uses a Python script to activate it's actuators,
	based on it's sensors.
	
	Properties:
	
	@ivar script: The value of this variable depends on the execution methid.
		- When 'Script' execution mode is set this value contains the entire python script as a single string (not the script name as you might expect) which can be modified to run different scripts.
		- When 'Module' execution mode is set this value will contain a single line string - module name and function "module.func" or "package.modile.func" where the module names are python textblocks or external scripts.
		note: once this is set the script name given for warnings will remain unchanged.
	@type script: string
	@ivar mode: the execution mode for this controller (read-only).
		- Script: 0, Execite the L{script} as a python code.
		- Module: 1, Execite the L{script} as a module and function.
	@type mode: int
	
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
		Gets the Python script body this controller executes.
		
		@deprecated: Use the L{script} attribute instead.
		@rtype: string
		"""
	def setScript(script_body):
		"""
		Sets the Python script string this controller executes.
		
		@deprecated: Use the L{script} attribute instead.
		@type script_body: string.
		"""

class SCA_RandomActuator(SCA_IActuator):
	"""
	Random Actuator
	
	Properties:
	
	@ivar seed: Seed of the random number generator.
	            Equal seeds produce equal series. If the seed is 0, 
	            the generator will produce the same value on every call.
	@type seed: integer
	@ivar para1: the first parameter of the active distribution. 
	             Refer to the documentation of the generator types for the meaning
	             of this value.
	@type para1: float, read-only
	@ivar para2: the second parameter of the active distribution. 
	             Refer to the documentation of the generator types for the meaning
	             of this value.
	@type para2: float, read-only
	@ivar distribution: distribution type:
	                    KX_RANDOMACT_BOOL_CONST, KX_RANDOMACT_BOOL_UNIFORM, KX_RANDOMACT_BOOL_BERNOUILLI,
	                    KX_RANDOMACT_INT_CONST, KX_RANDOMACT_INT_UNIFORM, KX_RANDOMACT_INT_POISSON, 
	                    KX_RANDOMACT_FLOAT_CONST, KX_RANDOMACT_FLOAT_UNIFORM, KX_RANDOMACT_FLOAT_NORMAL,
	                    KX_RANDOMACT_FLOAT_NEGATIVE_EXPONENTIAL
	@type distribution: integer, read-only
	@ivar propName: the name of the property to set with the random value.
	                If the generator and property types do not match, the assignment is ignored.
	@type propName: string

	"""
	def setBoolConst(value):
		"""
		Sets this generator to produce a constant boolean value.
		
		@param value: The value to return.
		@type value: boolean
		"""
	def setBoolUniform():
		"""
		Sets this generator to produce a uniform boolean distribution.
		
		The generator will generate True or False with 50% chance.
		"""
	def setBoolBernouilli(value):
		"""
		Sets this generator to produce a Bernouilli distribution.
		
		@param value: Specifies the proportion of False values to produce.
				- 0.0: Always generate True
				- 1.0: Always generate False
		@type value: float
		"""
	def setIntConst(value):
		"""
		Sets this generator to always produce the given value.
		
		@param value: the value this generator produces.
		@type value: integer
		"""
	def setIntUniform(lower_bound, upper_bound):
		"""
		Sets this generator to produce a random value between the given lower and
		upper bounds (inclusive).
		
		@type lower_bound: integer
		@type upper_bound: integer
		"""
	def setIntPoisson(value):
		"""
		Generate a Poisson-distributed number. 
		
		This performs a series of Bernouilli tests with parameter value. 
		It returns the number of tries needed to achieve succes.
		
		@type value: float
		"""
	def setFloatConst(value):
		"""
		Always generate the given value.
		
		@type value: float
		"""
	def setFloatUniform(lower_bound, upper_bound):
		"""
		Generates a random float between lower_bound and upper_bound with a
		uniform distribution.
		
		@type lower_bound: float
		@type upper_bound: float
		"""
	def setFloatNormal(mean, standard_deviation):
		"""
		Generates a random float from the given normal distribution.
		
		@type mean: float
		@param mean: The mean (average) value of the generated numbers
		@type standard_deviation: float
		@param standard_deviation: The standard deviation of the generated numbers.
		"""
	def setFloatNegativeExponential(half_life):
		"""
		Generate negative-exponentially distributed numbers. 
		
		The half-life 'time' is characterized by half_life.
		
		@type half_life: float
		"""
#{ Deprecated
	def setSeed(seed):
		"""
		Sets the seed of the random number generator.
		
		Equal seeds produce equal series. If the seed is 0, 
		the generator will produce the same value on every call.
		
		@deprecated: Use the L{seed} attribute instead.
		@type seed: integer
		"""
	def getSeed():
		"""
		Returns the initial seed of the generator.
		
		@deprecated: Use the L{seed} attribute instead.
		@rtype: integer
		"""
	def getPara1():
		"""
		Returns the first parameter of the active distribution. 
		
		Refer to the documentation of the generator types for the meaning
		of this value.
		
		@deprecated: Use the L{para1} attribute instead.
		@rtype: float
		"""
	def getPara2():
		"""
		Returns the second parameter of the active distribution. 
		
		Refer to the documentation of the generator types for the meaning
		of this value.
		
		@deprecated: Use the L{para2} attribute instead.
		@rtype: float
		"""
	def getDistribution():
		"""
		Returns the type of random distribution.
		
		@deprecated: Use the L{distribution} attribute instead.
		@rtype: distribution type
		@return: KX_RANDOMACT_BOOL_CONST, KX_RANDOMACT_BOOL_UNIFORM, KX_RANDOMACT_BOOL_BERNOUILLI,
		        KX_RANDOMACT_INT_CONST, KX_RANDOMACT_INT_UNIFORM, KX_RANDOMACT_INT_POISSON, 
		        KX_RANDOMACT_FLOAT_CONST, KX_RANDOMACT_FLOAT_UNIFORM, KX_RANDOMACT_FLOAT_NORMAL,
		        KX_RANDOMACT_FLOAT_NEGATIVE_EXPONENTIAL
		"""
	def setProperty(property):
		"""
		Set the property to which the random value is assigned. 
		
		If the generator and property types do not match, the assignment is ignored.
		
		@deprecated: Use the L{propName} attribute instead.
		@type property: string
		@param property: The name of the property to set.
		"""
	def getProperty():
		"""
		Returns the name of the property to set.
		
		@deprecated: Use the L{propName} attribute instead.
		@rtype: string
		"""
#}


class SCA_RandomSensor(SCA_ISensor):
	"""
	This sensor activates randomly.

	@ivar lastDraw: The seed of the random number generator.
	@type lastDraw: int
	@ivar seed: The seed of the random number generator.
	@type seed: int
	"""
	
	def setSeed(seed):
		"""
		Sets the seed of the random number generator.
		
		If the seed is 0, the generator will produce the same value on every call.
		
		@type seed: integer.
		"""
	def getSeed():
		"""
		Returns the initial seed of the generator.  Equal seeds produce equal random
		series.
		
		@rtype: integer
		"""
	def getLastDraw():
		"""
		Returns the last random number generated.
		
		@rtype: integer
		"""

class SCA_XNORController(SCA_IController):
	"""
	An XNOR controller activates when all linked sensors are the same (activated or inative).
	
	There are no special python methods for this controller.
	"""

class SCA_XORController(SCA_IController):
	"""
	An XOR controller activates when there is the input is mixed, but not when all are on or off.
	
	There are no special python methods for this controller.
	"""

class KX_Camera(KX_GameObject):
	"""
	A Camera object.
	
	@group Constants: INSIDE, INTERSECT, OUTSIDE
	@ivar INSIDE: see sphereInsideFrustum() and boxInsideFrustum()
	@ivar INTERSECT: see sphereInsideFrustum() and boxInsideFrustum()
	@ivar OUTSIDE: see sphereInsideFrustum() and boxInsideFrustum()
	
	@ivar lens: The camera's lens value. 
	@type lens: float
	@ivar near: The camera's near clip distance. 
	@type near: float
	@ivar far: The camera's far clip distance.
	@type far: float
	@ivar perspective: True if this camera has a perspective transform, False for an orthographic projection.
	@type perspective: boolean
	@ivar frustum_culling: True if this camera is frustum culling. 
	@type frustum_culling: boolean
	@ivar projection_matrix: This camera's 4x4 projection matrix.
	@type projection_matrix: 4x4 Matrix [[float]]
	@ivar modelview_matrix: This camera's 4x4 model view matrix. (read-only)
	                        Regenerated every frame from the camera's position and orientation.
	@type modelview_matrix: 4x4 Matrix [[float]] 
	@ivar camera_to_world: This camera's camera to world transform. (read-only)
	                       Regenerated every frame from the camera's position and orientation.
	@type camera_to_world: 4x4 Matrix [[float]]
	@ivar world_to_camera: This camera's world to camera transform. (read-only)
	                       Regenerated every frame from the camera's position and orientation.
	                       This is camera_to_world inverted.
	@type world_to_camera: 4x4 Matrix [[float]]
	@ivar useViewport: True when the camera is used as a viewport, set True to enable a viewport for this camera.
	@type useViewport: bool
	
	@group Deprecated: enableViewport, getProjectionMatrix, setProjectionMatrix
	"""
	
	def sphereInsideFrustum(centre, radius):
		"""
		Tests the given sphere against the view frustum.
		
		@note: when the camera is first initialized the result will be invalid because the projection matrix has not been set.
		@param centre: The centre of the sphere (in world coordinates.)
		@type centre: list [x, y, z]
		@param radius: the radius of the sphere
		@type radius: float
		@return: INSIDE, OUTSIDE or INTERSECT
		
		Example::
			import GameLogic
			co = GameLogic.getCurrentController()
			cam = co.owner
			
			# A sphere of radius 4.0 located at [x, y, z] = [1.0, 1.0, 1.0]
			if (cam.sphereInsideFrustum([1.0, 1.0, 1.0], 4) != cam.OUTSIDE):
				# Sphere is inside frustum !
				# Do something useful !
			else:
				# Sphere is outside frustum
		"""
	def boxInsideFrustum(box):
		"""
		Tests the given box against the view frustum.
		
		Example::
			import GameLogic
			co = GameLogic.getCurrentController()
			cam = co.owner
			
			# Box to test...
			box = []
			box.append([-1.0, -1.0, -1.0])
			box.append([-1.0, -1.0,  1.0])
			box.append([-1.0,  1.0, -1.0])
			box.append([-1.0,  1.0,  1.0])
			box.append([ 1.0, -1.0, -1.0])
			box.append([ 1.0, -1.0,  1.0])
			box.append([ 1.0,  1.0, -1.0])
			box.append([ 1.0,  1.0,  1.0])
			
			if (cam.boxInsideFrustum(box) != cam.OUTSIDE):
				# Box is inside/intersects frustum !
				# Do something useful !
			else:
				# Box is outside the frustum !
		
		@note: when the camera is first initialized the result will be invalid because the projection matrix has not been set.
		@return: INSIDE, OUTSIDE or INTERSECT
		@type box: list
		@param box: Eight (8) corner points of the box (in world coordinates.)
		"""
	def pointInsideFrustum(point):
		"""
		Tests the given point against the view frustum.
		
		Example::
			import GameLogic
			co = GameLogic.getCurrentController()
			cam = co.owner
	
			# Test point [0.0, 0.0, 0.0]
			if (cam.pointInsideFrustum([0.0, 0.0, 0.0])):
				# Point is inside frustum !
				# Do something useful !
			else:
				# Box is outside the frustum !
		
		@note: when the camera is first initialized the result will be invalid because the projection matrix has not been set.
		@rtype: boolean
		@return: True if the given point is inside this camera's viewing frustum.
		@type point: [x, y, z]
		@param point: The point to test (in world coordinates.)
		"""
	def getCameraToWorld():
		"""
		Returns the camera-to-world transform.
		
		@rtype: matrix (4x4 list)
		@return: the camera-to-world transform matrix.
		"""
	def getWorldToCamera():
		"""
		Returns the world-to-camera transform.
		
		This returns the inverse matrix of getCameraToWorld().
		
		@rtype: matrix (4x4 list)
		@return: the world-to-camera transform matrix.
		"""
	def getProjectionMatrix():
		"""
		Returns the camera's projection matrix.
		
		@deprecated: Use the L{projection_matrix} attribute instead.
		@rtype: matrix (4x4 list)
		@return: the camera's projection matrix.
		"""
	def setProjectionMatrix(matrix):
		"""
		Sets the camera's projection matrix.
		
		You should use normalised device coordinates for the clipping planes:
		left = -1.0, right = 1.0, top = 1.0, bottom = -1.0, near = cam.near, far = cam.far
		
		Example::
			import GameLogic

			def Scale(matrix, size):
				for y in range(4):
					for x in range(4):
						matrix[y][x] = matrix[y][x] * size[y]
				return matrix
			
			# Generate a perspective projection matrix
			def Perspective(cam):
				return [[cam.near, 0.0     ,  0.0                                  ,  0.0                                      ],
					[0.0     , cam.near,  0.0                                  ,  0.0                                      ],
					[0.0     , 0.0     , -(cam.far+cam.near)/(cam.far-cam.near), -2.0*cam.far*cam.near/(cam.far - cam.near)],
					[0.0     , 0.0     , -1.0                                  ,  0.0                                      ]]
			
			# Generate an orthographic projection matrix
			# You will need to scale the camera
			def Orthographic(cam):
				return [[1.0/cam.scaling[0], 0.0               ,  0.0                   ,  0.0                                  ],
					[0.0               , 1.0/cam.scaling[1],  0.0                   ,  0.0                                  ],
					[0.0               , 0.0               , -2.0/(cam.far-cam.near), -(cam.far+cam.near)/(cam.far-cam.near)],
					[0.0               , 0.0               ,  0.0                   ,  1.0                                  ]]
			
			# Generate an isometric projection matrix
			def Isometric(cam):
				return Scale([[0.707, 0.0  , 0.707, 0.0],
					      [0.408, 0.816,-0.408, 0.0],
					      [0.0  , 0.0  , 0.0  , 0.0],
					      [0.0  , 0.0  , 0.0  , 1.0]],
					      [1.0/cam.scaling[0], 1.0/cam.scaling[1], 1.0/cam.scaling[2], 1.0])
			
			co = GameLogic.getCurrentController()
			cam = co.owner
			cam.setProjectionMatrix(Perspective(cam)))
		
		@deprecated: Use the L{projection_matrix} attribute instead.
		@type matrix: 4x4 matrix.
		@param matrix: The new projection matrix for this camera.
		"""

	def enableViewport(viewport):
		"""
		Use this camera to draw a viewport on the screen (for split screen games or overlay scenes). The viewport region is defined with L{setViewport}.
		
		@deprecated: Use the L{useViewport} attribute instead.
		@type viewport: bool
		@param viewport: the new viewport status
		"""
	def setOnTop():
		"""
		Set this cameras viewport ontop of all other viewport.
		"""
	def setViewport(left, bottom, right, top):
		"""
		Sets the region of this viewport on the screen in pixels.
		
		Use L{Rasterizer.getWindowHeight} L{Rasterizer.getWindowWidth} to calculate values relative to the entire display.
		
		@type left: int
		@type bottom: int
		@type right: int
		@type top: int
		"""
	def getScreenPosition(arg):
		"""
		Gets the position of an object projected on screen space.

		Example:
		# For an object in the middle of the screen, coord = [0.5,0.5]
		coord = camera.getScreenPosition(object)

		@param arg: L{KX_GameObject}, object name or list [x, y, z]
		@rtype: list [x, y]
		@return: the object's position in screen coordinates.
		"""
	def getScreenVect(x, y):
		"""
		Gets the vector from the camera position in the screen coordinate direction.

		Example:
		# Gets the vector of the camera front direction:
		m_vect = camera.getScreenVect(0.5,0.5)

		@type x: float
		@type y: float
		@rtype: 3d vector
		@return: the vector from a screen coordinate.
		"""
	def getScreenRay(x, y, dist, property):
		"""
		Look towards a screen coordinate (x,y) and find first object hit within dist that matches prop.
		The ray is similar to KX_GameObject->rayCastTo.

		Example:
		# Gets an object with a property "wall" in front of the camera within a distance of 100:
		target = camera.getScreenRay(0.5,0.5,100,"wall")

		@type x: float
		@type y: float
		@param dist: max distance to look (can be negative => look behind); 0 or omitted => detect up to other
		@type dist: float
		@param property: property name that object must have; can be omitted => detect any object
		@type property: string
		@rtype: L{KX_GameObject}
		@return: the first object hit or None if no object or object does not match prop
		"""

class BL_ArmatureObject(KX_GameObject):
	"""
	An armature object.
	
	@ivar constraints: The list of armature constraint defined on this armature
	                   Elements of the list can be accessed by index or string.
	                   The key format for string access is '<bone_name>:<constraint_name>'
	@type constraints: list of L{BL_ArmatureConstraint}
	@ivar channels: The list of armature channels. 
		Elements of the list can be accessed by index or name the bone. 
	@type channels: list of L{BL_ArmatureChannel}
	"""
	
	def update():
		"""
		Ensures that the armature will be updated on next graphic frame.
		
		This action is unecessary if a KX_ArmatureActuator with mode run is active
		or if an action is playing. Use this function in other cases. It must be called
		on each frame to ensure that the armature is updated continously.
		"""
		
class BL_ArmatureActuator(SCA_IActuator):
	"""
	Armature Actuators change constraint condition on armatures.
	
	@group Constants: KX_ACT_ARMATURE_RUN, KX_ACT_ARMATURE_ENABLE, KX_ACT_ARMATURE_DISABLE, KX_ACT_ARMATURE_SETTARGET, KX_ACT_ARMATURE_SETWEIGHT
	@ivar KX_ACT_ARMATURE_RUN: see type
	@ivar KX_ACT_ARMATURE_ENABLE: see type
	@ivar KX_ACT_ARMATURE_DISABLE: see type
	@ivar KX_ACT_ARMATURE_SETTARGET: see type
	@ivar KX_ACT_ARMATURE_SETWEIGHT: see type
 	@ivar type: The type of action that the actuator executes when it is active.

				KX_ACT_ARMATURE_RUN(0): just make sure the armature will be updated on the next graphic frame
				This is the only persistent mode of the actuator: it executes automatically once per frame until stopped by a controller

				KX_ACT_ARMATURE_ENABLE(1): enable the constraint.

				KX_ACT_ARMATURE_DISABLE(2): disable the constraint (runtime constraint values are not updated).

				KX_ACT_ARMATURE_SETTARGET(3): change target and subtarget of constraint

				KX_ACT_ARMATURE_SETWEIGHT(4): change weight of (only for IK constraint)
	@type type: integer
	@ivar constraint: The constraint object this actuator is controlling.
	@type constraint: L{BL_ArmatureConstraint}
	@ivar target: The object that this actuator will set as primary target to the constraint it controls
	@type target: L{KX_GameObject}
	@ivar subtarget: The object that this actuator will set as secondary target to the constraint it controls.
	                 Currently, the only secondary target is the pole target for IK constraint.
	@type subtarget: L{KX_GameObject}
	@ivar weight: The weight this actuator will set on the constraint it controls.
	              Currently only the IK constraint has a weight. It must be a value between 0 and 1.
	              A weight of 0 disables a constraint while still updating constraint runtime values (see L{BL_ArmatureConstraint})
	@type weight: float
	"""

class KX_ArmatureSensor(SCA_ISensor):
	"""
	Armature sensor detect conditions on armatures.
	
	@group Constants: KX_ARMSENSOR_STATE_CHANGED, KX_ARMSENSOR_LIN_ERROR_BELOW, KX_ARMSENSOR_LIN_ERROR_ABOVE, KX_ARMSENSOR_ROT_ERROR_BELOW, KX_ARMSENSOR_ROT_ERROR_ABOVE
	@ivar KX_ARMSENSOR_STATE_CHANGED: see type
 	@ivar KX_ARMSENSOR_LIN_ERROR_BELOW: see type
	@ivar KX_ARMSENSOR_LIN_ERROR_ABOVE: see type
	@ivar KX_ARMSENSOR_ROT_ERROR_BELOW: see type
	@ivar KX_ARMSENSOR_ROT_ERROR_ABOVE: see type
	@ivar type: The type of measurement that the sensor make when it is active.

				KX_ARMSENSOR_STATE_CHANGED(0): detect that the constraint is changing state (active/inactive)

				KX_ARMSENSOR_LIN_ERROR_BELOW(1): detect that the constraint linear error is above a threshold

				KX_ARMSENSOR_LIN_ERROR_ABOVE(2): detect that the constraint linear error is below a threshold

				KX_ARMSENSOR_ROT_ERROR_BELOW(3): detect that the constraint rotation error is above a threshold

				KX_ARMSENSOR_ROT_ERROR_ABOVE(4): detect that the constraint rotation error is below a threshold
	@type type: integer
	@ivar constraint: The constraint object this sensor is watching.
	@type constraint: L{BL_ArmatureConstraint}
	@ivar value: The threshold used in the comparison with the constraint error
	             The linear error is only updated on CopyPose/Distance IK constraint with iTaSC solver
	             The rotation error is only updated on CopyPose+rotation IK constraint with iTaSC solver
	             The linear error on CopyPose is always >= 0: it is the norm of the distance between the target and the bone
	             The rotation error on CopyPose is always >= 0: it is the norm of the equivalent rotation vector between the bone and the target orientations
	             The linear error on Distance can be positive if the distance between the bone and the target is greater than the desired distance, and negative if the distance is smaller
	@type value: float
	"""

class BL_ArmatureConstraint(PyObjectPlus):
	"""
	Proxy to Armature Constraint. Allows to change constraint on the fly.
	Obtained through L{BL_ArmatureObject}.constraints.
	Note: not all armature constraints are supported in the GE.
	
	@group Constants: CONSTRAINT_TYPE_TRACKTO, CONSTRAINT_TYPE_KINEMATIC, CONSTRAINT_TYPE_ROTLIKE, CONSTRAINT_TYPE_LOCLIKE, CONSTRAINT_TYPE_MINMAX, CONSTRAINT_TYPE_SIZELIKE, CONSTRAINT_TYPE_LOCKTRACK, CONSTRAINT_TYPE_STRETCHTO, CONSTRAINT_TYPE_CLAMPTO, CONSTRAINT_TYPE_TRANSFORM, CONSTRAINT_TYPE_DISTLIMIT,CONSTRAINT_IK_COPYPOSE, CONSTRAINT_IK_DISTANCE,CONSTRAINT_IK_MODE_INSIDE, CONSTRAINT_IK_MODE_OUTSIDE,CONSTRAINT_IK_MODE_ONSURFACE,CONSTRAINT_IK_FLAG_TIP,CONSTRAINT_IK_FLAG_ROT, CONSTRAINT_IK_FLAG_STRETCH, CONSTRAINT_IK_FLAG_POS
	@ivar CONSTRAINT_TYPE_TRACKTO: see type
	@ivar CONSTRAINT_TYPE_KINEMATIC: see type
	@ivar CONSTRAINT_TYPE_ROTLIKE: see type
	@ivar CONSTRAINT_TYPE_LOCLIKE: see type
	@ivar CONSTRAINT_TYPE_MINMAX: see type
	@ivar CONSTRAINT_TYPE_SIZELIKE: see type
	@ivar CONSTRAINT_TYPE_LOCKTRACK: see type
	@ivar CONSTRAINT_TYPE_STRETCHTO: see type
	@ivar CONSTRAINT_TYPE_CLAMPTO: see type
	@ivar CONSTRAINT_TYPE_TRANSFORM: see type
	@ivar CONSTRAINT_TYPE_DISTLIMIT: see type
	@ivar CONSTRAINT_IK_COPYPOSE: see ik_type
	@ivar CONSTRAINT_IK_DISTANCE: see ik_type
	@ivar CONSTRAINT_IK_MODE_INSIDE: see ik_mode
	@ivar CONSTRAINT_IK_MODE_OUTSIDE: see ik_mode
	@ivar CONSTRAINT_IK_MODE_ONSURFACE: see ik_mode
	@ivar CONSTRAINT_IK_FLAG_TIP: see ik_flag
	@ivar CONSTRAINT_IK_FLAG_ROT: see ik_flag
	@ivar CONSTRAINT_IK_FLAG_STRETCH: see ik_flag
	@ivar CONSTRAINT_IK_FLAG_POS: see ik_flag
	@ivar type: Type of constraint, read-only
	@type type: integer, one of CONSTRAINT_TYPE_ constant
	@ivar name: Name of constraint constructed as <bone_name>:<constraint_name>
	            This name is also the key subscript on L{BL_ArmatureObject}.constraints list
	@type name: string
	@ivar enforce: fraction of constraint effect that is enforced. Between 0 and 1.
	@type enforce: float
	@ivar headtail: position of target between head and tail of the target bone: 0=head, 1=tail
	                Only used if the target is a bone (i.e target object is an armature)
	@type headtail: float
	@ivar lin_error: runtime linear error (in Blender unit) on constraint at the current frame.
	                 This is a runtime value updated on each frame by the IK solver. Only available on IK constraint and iTaSC solver.
	@type lin_error: float
	@ivar rot_error: runtime rotation error (in radiant) on constraint at the current frame.
	                 This is a runtime value updated on each frame by the IK solver. Only available on IK constraint and iTaSC solver.
	                 It is only set if the constraint has a rotation part, for example, a CopyPose+Rotation IK constraint.
	@type rot_error: float
	@ivar target: Primary target object for the constraint. The position of this object in the GE will be used as target for the constraint.
	@type target: L{KX_GameObject}
	@ivar subtarget: Secondary target object for the constraint. The position of this object in the GE will be used as secondary target for the constraint.
	                 Currently this is only used for pole target on IK constraint.
	@type subtarget: L{KX_GameObject}
	@ivar active: True if the constraint is active.
	              Note: an inactive constraint does not update lin_error and rot_error.
	@type active: boolean
	@ivar ik_weight: Weight of the IK constraint between 0 and 1. 
	                 Only defined for IK constraint.
	@type ik_weight: float
	@ivar ik_type: Type of IK constraint, read-only

				   CONSTRAINT_IK_COPYPOSE(0): constraint is trying to match the position and eventually the rotation of the target.

				   CONSTRAINT_IK_DISTANCE(1): constraint is maintaining a certain distance to target subject to ik_mode
	@type ik_type: integer
	@ivar ik_flag: Combination of IK constraint option flags, read-only

				   CONSTRAINT_IK_FLAG_TIP(1) : set when the constraint operates on the head of the bone and not the tail

				   CONSTRAINT_IK_FLAG_ROT(2) : set when the constraint tries to match the orientation of the target

				   CONSTRAINT_IK_FLAG_STRETCH(16) : set when the armature is allowed to stretch (only the bones with stretch factor > 0.0)

				   CONSTRAINT_IK_FLAG_POS(32) : set when the constraint tries to match the position of the target
	@type ik_flag: integer
	@ivar ik_dist: Distance the constraint is trying to maintain with target, only used when ik_type=CONSTRAINT_IK_DISTANCE
	@type ik_dist: float
	@ivar ik_mode: Additional mode for IK constraint. Currently only used for Distance constraint:

				   CONSTRAINT_IK_MODE_INSIDE(0) : the constraint tries to keep the bone within ik_dist of target

				   CONSTRAINT_IK_MODE_OUTSIDE(1) : the constraint tries to keep the bone outside ik_dist of the target

				   CONSTRAINT_IK_MODE_ONSURFACE(2) : the constraint tries to keep the bone exactly at ik_dist of the target
	@type ik_mode: integer
	"""

class BL_ArmatureChannel(PyObjectPlus):
	"""
	Proxy to armature pose channel. Allows to read and set armature pose.
	The attributes are identical to RNA attributes, but mostly in read-only mode.
	
	@group Constants: PCHAN_ROT_QUAT, PCHAN_ROT_XYZ, PCHAN_ROT_XZY, PCHAN_ROT_YXZ, PCHAN_ROT_YZX, PCHAN_ROT_ZXY, PCHAN_ROT_ZYX
	@ivar PCHAN_ROT_QUAT: see rotation_mode
	@ivar PCHAN_ROT_XYZ: see rotation_mode
	@ivar PCHAN_ROT_XZY: see rotation_mode
	@ivar PCHAN_ROT_YXZ: see rotation_mode
	@ivar PCHAN_ROT_YZX: see rotation_mode
	@ivar PCHAN_ROT_ZXY: see rotation_mode
	@ivar PCHAN_ROT_ZYX: see rotation_mode
	@ivar name: channel name (=bone name), read-only.
	@type name: string
	@ivar bone: return the bone object corresponding to this pose channel, read-only.
	@type bone: L{BL_ArmatureBone}
	@ivar parent: return the parent channel object, None if root channel, read-only.
	@type parent: L{BL_ArmatureChannel}
	@ivar has_ik: true if the bone is part of an active IK chain, read-only.
		This flag is not set when an IK constraint is defined but not enabled (miss target information for example)
	@type has_ik: boolean
	@ivar ik_dof_x: true if the bone is free to rotation in the X axis, read-only.
	@type ik_dof_x: boolean
	@ivar ik_dof_y: true if the bone is free to rotation in the Y axis, read-only.
	@type ik_dof_y: boolean
	@ivar ik_dof_z: true if the bone is free to rotation in the Z axis, read-only.
	@type ik_dof_z: boolean
	@ivar ik_limit_x: true if a limit is imposed on X rotation, read-only.
	@type ik_limit_x: boolean
	@ivar ik_limit_y: true if a limit is imposed on Y rotation, read-only.
	@type ik_limit_y: boolean
	@ivar ik_limit_z: true if a limit is imposed on Z rotation, read-only.
	@type ik_limit_z: boolean
	@ivar ik_rot_control: true if channel rotation should applied as IK constraint, read-only.
	@type ik_rot_control: boolean
	@ivar ik_lin_control: true if channel size should applied as IK constraint, read-only.
	@type ik_lin_control: boolean
	@ivar location: displacement of the bone head in armature local space, read-write.
		You can only move a bone if it is unconnected to its parent. An action playing on the armature may change the value. An IK chain does not update this value, see joint_rotation.
		Changing this field has no immediate effect, the pose is updated when the armature is updated during the graphic render (see L{BL_ArmatureObject.update})
	@type location: vector [X,Y,Z]
	@ivar scale: scale of the bone relative to its parent, read-write.
		An action playing on the armature may change the value.  An IK chain does not update this value, see joint_rotation.
		Changing this field has no immediate effect, the pose is updated when the armature is updated during the graphic render (see L{BL_ArmatureObject.update})
	@type scale: vector [sizeX, sizeY, sizeZ]
	@ivar rotation: rotation of the bone relative to its parent expressed as a quaternion, read-write.
		This field is only used if rotation_mode is 0. An action playing on the armature may change the value.  An IK chain does not update this value, see joint_rotation.
		Changing this field has no immediate effect, the pose is updated when the armature is updated during the graphic render (see L{BL_ArmatureObject.update})
	@type rotation: vector [qr, qi, qj, qk]
	@ivar euler_rotation: rotation of the bone relative to its parent expressed as a set of euler angles, read-write.
		This field is only used if rotation_mode is > 0. You must always pass the angles in [X,Y,Z] order; the order of applying the angles to the bone depends on rotation_mode. An action playing on the armature may change this field.  An IK chain does not update this value, see joint_rotation.
		Changing this field has no immediate effect, the pose is updated when the armature is updated during the graphic render (see L{BL_ArmatureObject.update})
	@type euler_rotation: vector [X, Y, Z]
	@ivar rotation_mode: method of updating the bone rotation, read-write.
		Use the following constants (euler mode are named as in BLender UI but the actual axis order is reversed):
		  - PCHAN_ROT_QUAT(0) : use quaternioin in rotation attribute to update bone rotation
		  - PCHAN_ROT_XYZ(1) : use euler_rotation and apply angles on bone's Z, Y, X axis successively
		  - PCHAN_ROT_XZY(2) : use euler_rotation and apply angles on bone's Y, Z, X axis successively
		  - PCHAN_ROT_YXZ(3) : use euler_rotation and apply angles on bone's Z, X, Y axis successively
		  - PCHAN_ROT_YZX(4) : use euler_rotation and apply angles on bone's X, Z, Y axis successively
		  - PCHAN_ROT_ZXY(5) : use euler_rotation and apply angles on bone's Y, X, Z axis successively
		  - PCHAN_ROT_ZYX(6) : use euler_rotation and apply angles on bone's X, Y, Z axis successively
	@type rotation_mode: integer
	@ivar channel_matrix: pose matrix in bone space (deformation of the bone due to action, constraint, etc), Read-only.
		This field is updated after the graphic render, it represents the current pose.
	@type channel_matrix: matrix [4][4]
	@ivar pose_matrix: pose matrix in armature space, read-only,
		This field is updated after the graphic render, it represents the current pose.
	@type pose_matrix: matrix [4][4]
	@ivar pose_head: position of bone head in armature space, read-only.
	@type pose_head: vector [x, y, z]
	@ivar pose_tail: position of bone tail in armature space, read-only.
	@type pose_tail: vector [x, y, z]
	@ivar ik_min_x: minimum value of X rotation in degree (<= 0) when X rotation is limited (see ik_limit_x), read-only.
	@type ik_min_x: float
	@ivar ik_max_x: maximum value of X rotation in degree (>= 0) when X rotation is limited (see ik_limit_x), read-only.
	@type ik_max_x: float
	@ivar ik_min_y: minimum value of Y rotation in degree (<= 0) when Y rotation is limited (see ik_limit_y), read-only.
	@type ik_min_y: float
	@ivar ik_max_y: maximum value of Y rotation in degree (>= 0) when Y rotation is limited (see ik_limit_y), read-only.
	@type ik_max_y: float
	@ivar ik_min_z: minimum value of Z rotation in degree (<= 0) when Z rotation is limited (see ik_limit_z), read-only.
	@type ik_min_z: float
	@ivar ik_max_z: maximum value of Z rotation in degree (>= 0) when Z rotation is limited (see ik_limit_z), read-only.
	@type ik_max_z: float
	@ivar ik_stiffness_x: bone rotation stiffness in X axis, read-only
	@type ik_stiffness_x: float between 0 and 1
	@ivar ik_stiffness_y: bone rotation stiffness in Y axis, read-only
	@type ik_stiffness_y: float between 0 and 1
	@ivar ik_stiffness_z: bone rotation stiffness in Z axis, read-only
	@type ik_stiffness_z: float between 0 and 1
	@ivar ik_stretch: ratio of scale change that is allowed, 0=bone can't change size, read-only.
	@type ik_stretch: float
	@ivar ik_rot_weight: weight of rotation constraint when ik_rot_control is set, read-write.
	@type ik_rot_weight: float between 0 and 1
	@ivar ik_lin_weight: weight of size constraint when ik_lin_control is set, read-write.
	@type ik_lin_weight: float between 0 and 1
	@ivar joint_rotation: control bone rotation in term of joint angle (for robotic applications), read-write.
		When writing to this attribute, you pass a [x, y, z] vector and an appropriate set of euler angles or quaternion is calculated according to the rotation_mode.
		When you read this attribute, the current pose matrix is converted into a [x, y, z] vector representing the joint angles.
		The value and the meaning of the x, y, z depends on the ik_dof_ attributes:
		  - 1DoF joint X, Y or Z: the corresponding x, y, or z value is used an a joint angle in radiant
		  - 2DoF joint X+Y or Z+Y: treated as 2 successive 1DoF joints: first X or Z, then Y. The x or z value is used as a joint angle in radiant along the X or Z axis, followed by a rotation along the new Y axis of y radiants.
		  - 2DoF joint X+Z: treated as a 2DoF joint with rotation axis on the X/Z plane. The x and z values are used as the coordinates of the rotation vector in the X/Z plane.
		  - 3DoF joint X+Y+Z: treated as a revolute joint. The [x,y,z] vector represents the equivalent rotation vector to bring the joint from the rest pose to the new pose.

		Notes:
		  - The bone must be part of an IK chain if you want to set the ik_dof_ attributes via the UI, but this will interfere with this attribute since the IK solver will overwrite the pose. You can stay in control of the armature if you create an IK constraint but do not finalize it (e.g. don't set a target): the IK solver will not run but the IK panel will show up on the UI for each bone in the chain.
		  - [0,0,0] always corresponds to the rest pose. 
		  - You must request the armature pose to update and wait for the next graphic frame to see the effect of setting this attribute (see L{BL_ArmatureObject.update}). 
		  - You can read the result of the calculation in rotation or euler_rotation attributes after setting this attribute.
	@type joint_rotation: vector [x, y, z]
	"""
	
class BL_ArmatureBone(PyObjectPlus):
	"""
	Proxy to Blender bone structure. All fields are read-only and comply to RNA names.
	All space attribute correspond to the rest pose.
	
	@ivar name: bone name
	@type name: string
	@ivar connected: true when the bone head is struck to the parent's tail
	@type connected: boolean
	@ivar hinge: true when bone doesn't inherit rotation or scale from parent bone
	@type hinge: boolean
	@ivar inherit_scale: true when bone inherits scaling from parent bone
	@type inherit_scale: boolean
	@ivar bbone_segments: number of B-bone segments
	@type bbone_segments: integer
	@ivar roll: bone rotation around head-tail axis
	@type roll: float
	@ivar head: location of head end of the bone in parent bone space
	@type head: vector [x, y, z]
	@ivar tail: location of head end of the bone in parent bone space
	@type tail: vector [x, y, z]
	@ivar length: bone length
	@type length: float
	@ivar arm_head: location of head end of the bone in armature space
	@type arm_head: vector [x, y, z]
	@ivar arm_tail: location of tail end of the bone in armature space
	@type arm_tail: vector [x, y, z]
	@ivar arm_mat: matrix of the bone head in armature space
		This matrix has no scale part.
	@type arm_mat: matrix [4][4]
	@ivar bone_mat: rotation matrix of the bone in parent bone space.
	@type bone_mat: matrix [3][3]
	@ivar parent: parent bone, or None for root bone
	@type parent: L{BL_ArmatureBone}
	@ivar children: list of bone's children
	@type children: list of L{BL_ArmatureBone}
	"""
# Util func to extract all attrs
"""
import types
attrs = []
for name, val in locals().items():
	if name.startswith('__'):
		continue
	if type(val) == types.ClassType:
		for line in val.__doc__.split('\n'):
			if '@ivar' in line:
				attrs.append(name + '::' + line.split()[1].replace(':', ''))

for a in attrs:
	print a
"""


# Util func to construct a mapping from deprecated attrs to new ones.
"""
import types
import re
import pprint
depAttrs = {}
for name, val in locals().items():
	if name.startswith('__'):
		continue
	if type(val) == types.ClassType:
		print "\t# %s" % name
		
		# Inspect each attribute.
		for attrName in dir(val):
			if attrName.startswith('__'):
				continue
			attr = getattr(val, attrName)
			
			# Check whether this attribute is deprecated by searching each line.
			newAttrName = None
			for line in attr.__doc__.split('\n'):
				match = re.search(r'@deprecated.*L{(\w+)}', line)
				if match:
					newAttrName = match.group(1)
					break
			if not newAttrName:
				continue
			
			# Store the mappings to new attributes in a list (because there
			# could be collisions).
			if attrName not in depAttrs:
				depAttrs[attrName] = {}
			mapping = depAttrs[attrName]
			
			for line in val.__doc__.split('\n'):
				if ("@type %s:" % newAttrName) in line:
					# The attribute is being replaced in this class (i.e. the
					# deprecated attribute wasn't inherited from a parent). We
					# have a winner!
					funcType = None
					if 'sequence' in line:
						funcType = 'Keyed'
					else:
						funcType = 'Simple'
					
					if attrName.startswith('get') or attrName.startswith('is'):
						func = "replace%sGetter" % funcType
					elif attrName.startswith('set') or attrName.startswith('enable'):
						func = "replace%sSetter" % funcType
					else:
						func = 'UNKNOWN'
					
					# Another mapping, from a conversion tuple to lists of class
					# names.
					conversion = (func, newAttrName)
					if conversion not in mapping:
						mapping[conversion] = []
					mapping[conversion].append(name)
					break

pprint.pprint(depAttrs, width = 100)
"""
