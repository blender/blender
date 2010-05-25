
Game Engine bge.logic Module.
=============================
	
Module to access logic functions, imported automatically into the python controllers namespace.

.. module:: bge.logic

.. code-block:: python

   # To get the controller thats running this python script:
   cont = bge.logic.getCurrentController() # bge.logic is automatically imported
   
   # To get the game object this controller is on:
   obj = cont.owner

:class:`bge.types.KX_GameObject` and :class:`bge.types.KX_Camera` or :class:`bge.types.KX_LightObject` methods are available depending on the type of object

.. code-block:: python

   # To get a sensor linked to this controller.
   # "sensorname" is the name of the sensor as defined in the Blender interface.
   # +---------------------+  +--------+
   # | Sensor "sensorname" +--+ Python +
   # +---------------------+  +--------+
   sens = cont.sensors["sensorname"]

   # To get a sequence of all sensors:
   sensors = co.sensors

See the sensor's reference for available methods:

* :class:`bge.types.SCA_DelaySensor`
* :class:`bge.types.SCA_JoystickSensor`
* :class:`bge.types.SCA_KeyboardSensor`
* :class:`bge.types.KX_MouseFocusSensor`
* :class:`bge.types.SCA_MouseSensor`
* :class:`bge.types.KX_NearSensor`
* :class:`bge.types.KX_NetworkMessageSensor`
* :class:`bge.types.SCA_PropertySensor`
* :class:`bge.types.KX_RadarSensor`
* :class:`bge.types.SCA_RandomSensor`
* :class:`bge.types.KX_RaySensor`
* :class:`bge.types.KX_TouchSensor`

You can also access actuators linked to the controller

.. code-block:: python

   # To get an actuator attached to the controller:
   #                          +--------+  +-------------------------+
   #                          + Python +--+ Actuator "actuatorname" |
   #                          +--------+  +-------------------------+
   actuator = co.actuators["actuatorname"]
   
   # Activate an actuator
   controller.activate(actuator)


See the actuator's reference for available methods

* :class:`bge.types.SCA_2DFilterActuator`
* :class:`bge.types.BL_ActionActuator`
* :class:`bge.types.KX_SCA_AddObjectActuator`
* :class:`bge.types.KX_CameraActuator`
* :class:`bge.types.KX_ConstraintActuator`
* :class:`bge.types.KX_SCA_DynamicActuator`
* :class:`bge.types.KX_SCA_EndObjectActuator`
* :class:`bge.types.KX_GameActuator`
* :class:`bge.types.KX_IpoActuator`
* :class:`bge.types.KX_NetworkMessageActuator`
* :class:`bge.types.KX_ObjectActuator`
* :class:`bge.types.KX_ParentActuator`
* :class:`bge.types.SCA_PropertyActuator`
* :class:`bge.types.SCA_RandomActuator`
* :class:`bge.types.KX_SCA_ReplaceMeshActuator`
* :class:`bge.types.KX_SceneActuator`
* :class:`bge.types.BL_ShapeActionActuator`
* :class:`bge.types.KX_SoundActuator`
* :class:`bge.types.KX_StateActuator`
* :class:`bge.types.KX_TrackToActuator`
* :class:`bge.types.KX_VisibilityActuator`

Most logic brick's methods are accessors for the properties available in the logic buttons.
Consult the logic bricks documentation for more information on how each logic brick works.

There are also methods to access the current :class:`bge.types.KX_Scene`

.. code-block:: python

   # Get the current scene
   scene = bge.logic.getCurrentScene()

   # Get the current camera
   cam = scene.active_camera

Matricies as used by the game engine are **row major**
``matrix[row][col] = float``

:class:`bge.types.KX_Camera` has some examples using matricies.


.. data:: globalDict

   A dictionary that is saved between loading blend files so you can use it to store inventory and other variables you want to store between scenes and blend files.
   It can also be written to a file and loaded later on with the game load/save actuators.

   .. note:: only python built in types such as int/string/bool/float/tuples/lists can be saved, GameObjects, Actuators etc will not work as expectred.

.. data:: keyboard:		The current keyboard wrapped in an SCA_PythonKeyboard object.
.. data:: mouse:			The current mouse wrapped in an SCA_PythonMouse object.

.. function:: getCurrentController()

   Gets the Python controller associated with this Python script.
   
   :rtype: :class:`bge.types.SCA_PythonController`

.. function:: getCurrentScene()

   Gets the current Scene.
   
   :rtype: :class:`bge.types.KX_Scene`

.. function:: getSceneList()

   Gets a list of the current scenes loaded in the game engine.
   
   :rtype: list of :class:`bge.types.KX_Scene`
   
   .. note:: Scenes in your blend file that have not been converted wont be in this list. This list will only contain scenes such as overlays scenes.

.. function:: loadGlobalDict()

   Loads bge.logic.globalDict from a file.

.. function:: saveGlobalDict()

   Saves bge.logic.globalDict to a file.

.. function:: addScene(name, overlay=1)

   Loads a scene into the game engine.

   :arg name: The name of the scene
   :type name: string
   :arg overlay: Overlay or underlay (optional)
   :type overlay: integer

.. function:: sendMessage(subject, body="", to="", message_from="")

   Sends a message to sensors in any active scene.
   
   :arg subject: The subject of the message
   :type subject: string
   :arg body: The body of the message (optional)
   :type body: string
   :arg to: The name of the object to send the message to (optional)
   :type to: string
   :arg message_from: The name of the object that the message is coming from (optional)
   :type message_from: string

.. function:: setGravity(gravity)

   Sets the world gravity.
   
   :type gravity: list [fx, fy, fz]

.. function:: getSpectrum()

   Returns a 512 point list from the sound card.
   This only works if the fmod sound driver is being used.
   
   :rtype: list [float], len(getSpectrum()) == 512

.. function:: stopDSP()

   Stops the sound driver using DSP effects.
   
   Only the fmod sound driver supports this.
   DSP can be computationally expensive.

.. function:: getMaxLogicFrame()

   Gets the maximum number of logic frame per render frame.
   
   :return: The maximum number of logic frame per render frame
   :rtype: integer

.. function:: setMaxLogicFrame(maxlogic)

   Sets the maximum number of logic frame that are executed per render frame.
   This does not affect the physic system that still runs at full frame rate.   
    
   :arg maxlogic: The new maximum number of logic frame per render frame. Valid values: 1..5
   :type maxlogic: integer

.. function:: getMaxPhysicsFrame()

   Gets the maximum number of physics frame per render frame.
   
   :return: The maximum number of physics frame per render frame
   :rtype: integer

.. function:: setMaxPhysicsFrame(maxphysics)

   Sets the maximum number of physics timestep that are executed per render frame.
   Higher value allows physics to keep up with realtime even if graphics slows down the game.
   Physics timestep is fixed and equal to 1/tickrate (see setLogicTicRate)
   maxphysics/ticrate is the maximum delay of the renderer that physics can compensate.
    
   :arg maxphysics: The new maximum number of physics timestep per render frame. Valid values: 1..5.
   :type maxphysics: integer

.. function:: getLogicTicRate()

   Gets the logic update frequency.
   
   :return: The logic frequency in Hz
   :rtype: float

.. function:: setLogicTicRate(ticrate)

   Sets the logic update frequency.
   
   The logic update frequency is the number of times logic bricks are executed every second.
   The default is 60 Hz.
   
   :arg ticrate: The new logic update frequency (in Hz).
   :type ticrate: float

.. function:: getPhysicsTicRate()

   Gets the physics update frequency
   
   :return: The physics update frequency in Hz
   :rtype: float
   
   .. warning: Not implimented yet

.. function:: setPhysicsTicRate(ticrate)

   Sets the physics update frequency
   
   The physics update frequency is the number of times the physics system is executed every second.
   The default is 60 Hz.
   
   :arg ticrate: The new update frequency (in Hz).
   :type ticrate: float

   .. warning: Not implimented yet

.. function:: saveGlobalDict()

   Saves bge.logic.globalDict to a file.

.. function:: loadGlobalDict()

   Loads bge.logic.globalDict from a file.


Utility functions

.. function:: getAverageFrameRate()

   Gets the estimated average framerate
   
   :return: The estimed average framerate in frames per second
   :rtype: float

.. function:: expandPath(path)

   Converts a blender internal path into a proper file system path.

   Use / as directory separator in path
   You can use '//' at the start of the string to define a relative path;
   Blender replaces that string by the directory of the startup .blend or runtime file
   to make a full path name (doesn't change during the game, even if you load other .blend).
   The function also converts the directory separator to the local file system format.

   :arg path: The path string to be converted/expanded.
   :type path: string
   :return: The converted string
   :rtype: string


.. function:: getBlendFileList(path = "//")

   Returns a list of blend files in the same directory as the open blend file, or from using the option argument.

   :arg path: Optional directory argument, will be expanded (like expandPath) into the full path.
   :type path: string
   :return: A list of filenames, with no directory prefix
   :rtype: list

.. function:: PrintGLInfo()

   Prints GL Extension Info into the console

.. function:: getRandomFloat()

   Returns a random floating point value in the range [0 - 1)

=========
Constants
=========

.. data:: KX_TRUE: True value used by some modules.
.. data:: KX_FALSE: False value used by some modules.

---------------
Property Sensor
---------------

.. data:: KX_PROPSENSOR_EQUAL

   Activate when the property is equal to the sensor value.

.. data:: KX_PROPSENSOR_NOTEQUAL

   Activate when the property is not equal to the sensor value.

.. data:: KX_PROPSENSOR_INTERVAL

   Activate when the property is between the specified limits.

.. data:: KX_PROPSENSOR_CHANGED

   Activate when the property changes

.. data:: KX_PROPSENSOR_EXPRESSION

   Activate when the expression matches

-------------------
Constraint Actuator
-------------------

See :class:`bge.types.KX_ConstraintActuator`

.. data:: KX_CONSTRAINTACT_LOCX
.. data:: KX_CONSTRAINTACT_LOCY
.. data:: KX_CONSTRAINTACT_LOCZ
.. data:: KX_CONSTRAINTACT_ROTX
.. data:: KX_CONSTRAINTACT_ROTY
.. data:: KX_CONSTRAINTACT_ROTZ
.. data:: KX_CONSTRAINTACT_DIRNX
.. data:: KX_CONSTRAINTACT_DIRNY
.. data:: KX_CONSTRAINTACT_DIRPX
.. data:: KX_CONSTRAINTACT_DIRPY
.. data:: KX_CONSTRAINTACT_ORIX
.. data:: KX_CONSTRAINTACT_ORIY
.. data:: KX_CONSTRAINTACT_ORIZ

------------
IPO Actuator
------------

See :class:`bge.types.KX_IpoActuator`

.. data:: KX_IPOACT_PLAY
.. data:: KX_IPOACT_PINGPONG
.. data:: KX_IPOACT_FLIPPER
.. data:: KX_IPOACT_LOOPSTOP
.. data:: KX_IPOACT_LOOPEND
.. data:: KX_IPOACT_FROM_PROP

--------------------
Random Distributions
--------------------

See :class:`bge.types.SCA_RandomActuator`

.. data:: KX_RANDOMACT_BOOL_CONST
.. data:: KX_RANDOMACT_BOOL_UNIFORM
.. data:: KX_RANDOMACT_BOOL_BERNOUILLI
.. data:: KX_RANDOMACT_INT_CONST
.. data:: KX_RANDOMACT_INT_UNIFORM
.. data:: KX_RANDOMACT_INT_POISSON
.. data:: KX_RANDOMACT_FLOAT_CONST
.. data:: KX_RANDOMACT_FLOAT_UNIFORM
.. data:: KX_RANDOMACT_FLOAT_NORMAL
.. data:: KX_RANDOMACT_FLOAT_NEGATIVE_EXPONENTIAL

---------------
Action Actuator
---------------

See :class:`bge.types.BL_ActionActuator`

.. data:: KX_ACTIONACT_PLAY
.. data:: KX_ACTIONACT_FLIPPER
.. data:: KX_ACTIONACT_LOOPSTOP
.. data:: KX_ACTIONACT_LOOPEND
.. data:: KX_ACTIONACT_PROPERTY

--------------
Sound Actuator
--------------

See :class:`bge.types.KX_SoundActuator`

.. data:: KX_SOUNDACT_PLAYSTOP
.. data:: KX_SOUNDACT_PLAYEND
.. data:: KX_SOUNDACT_LOOPSTOP
.. data:: KX_SOUNDACT_LOOPEND
.. data:: KX_SOUNDACT_LOOPBIDIRECTIONAL
.. data:: KX_SOUNDACT_LOOPBIDIRECTIONAL_STOP

------------
Radar Sensor
------------

See :class:`bge.types.KX_RadarSensor`

.. data:: KX_RADAR_AXIS_POS_X
.. data:: KX_RADAR_AXIS_POS_Y
.. data:: KX_RADAR_AXIS_POS_Z
.. data:: KX_RADAR_AXIS_NEG_X
.. data:: KX_RADAR_AXIS_NEG_Y
.. data:: KX_RADAR_AXIS_NEG_Z

----------
Ray Sensor
----------

See :class:`bge.types.KX_RaySensor`

.. data:: KX_RAY_AXIS_POS_X
.. data:: KX_RAY_AXIS_POS_Y
.. data:: KX_RAY_AXIS_POS_Z
.. data:: KX_RAY_AXIS_NEG_X
.. data:: KX_RAY_AXIS_NEG_Y
.. data:: KX_RAY_AXIS_NEG_Z

----------------
Dynamic Actuator
----------------

See :class:`bge.types.KX_SCA_DynamicActuator`

.. data:: KX_DYN_RESTORE_DYNAMICS
.. data:: KX_DYN_DISABLE_DYNAMICS
.. data:: KX_DYN_ENABLE_RIGID_BODY
.. data:: KX_DYN_DISABLE_RIGID_BODY
.. data:: KX_DYN_SET_MASS

-------------
Game Actuator
-------------

See :class:`bge.types.KX_GameActuator`

.. data:: KX_GAME_LOAD
.. data:: KX_GAME_START
.. data:: KX_GAME_RESTART
.. data:: KX_GAME_QUIT
.. data:: KX_GAME_SAVECFG
.. data:: KX_GAME_LOADCFG

--------------
Scene Actuator
--------------

See :class:`bge.types.KX_SceneActuator`

.. data:: KX_SCENE_RESTART
.. data:: KX_SCENE_SET_SCENE
.. data:: KX_SCENE_SET_CAMERA
.. data:: KX_SCENE_ADD_FRONT_SCENE
.. data:: KX_SCENE_ADD_BACK_SCENE
.. data:: KX_SCENE_REMOVE_SCENE
.. data:: KX_SCENE_SUSPEND
.. data:: KX_SCENE_RESUME

------------
Input Status
------------

See :class:`bge.types.SCA_MouseSensor`

.. data:: KX_INPUT_NONE
.. data:: KX_INPUT_JUST_ACTIVATED
.. data:: KX_INPUT_ACTIVE
.. data:: KX_INPUT_JUST_RELEASED

-------------
Mouse Buttons
-------------

See :class:`bge.types.SCA_MouseSensor`

.. data:: KX_MOUSE_BUT_LEFT
.. data:: KX_MOUSE_BUT_MIDDLE
.. data:: KX_MOUSE_BUT_RIGHT

------
States
------

See :class:`bge.types.KX_StateActuator`

.. data:: KX_STATE1
.. data:: KX_STATE2
.. data:: KX_STATE3
.. data:: KX_STATE4
.. data:: KX_STATE5
.. data:: KX_STATE6
.. data:: KX_STATE7
.. data:: KX_STATE8
.. data:: KX_STATE9
.. data:: KX_STATE10
.. data:: KX_STATE11
.. data:: KX_STATE12
.. data:: KX_STATE13
.. data:: KX_STATE14
.. data:: KX_STATE15
.. data:: KX_STATE16
.. data:: KX_STATE17
.. data:: KX_STATE18
.. data:: KX_STATE19
.. data:: KX_STATE20
.. data:: KX_STATE21
.. data:: KX_STATE22
.. data:: KX_STATE23
.. data:: KX_STATE24
.. data:: KX_STATE25
.. data:: KX_STATE26
.. data:: KX_STATE27
.. data:: KX_STATE28
.. data:: KX_STATE29
.. data:: KX_STATE30
.. data:: KX_STATE_OP_CLR
.. data:: KX_STATE_OP_CPY
.. data:: KX_STATE_OP_NEG
.. data:: KX_STATE_OP_SET

---------
2D Filter
---------

.. data:: RAS_2DFILTER_BLUR
.. data:: RAS_2DFILTER_CUSTOMFILTER
.. data:: RAS_2DFILTER_DILATION
.. data:: RAS_2DFILTER_DISABLED
.. data:: RAS_2DFILTER_ENABLED
.. data:: RAS_2DFILTER_EROSION
.. data:: RAS_2DFILTER_GRAYSCALE
.. data:: RAS_2DFILTER_INVERT
.. data:: RAS_2DFILTER_LAPLACIAN
.. data:: RAS_2DFILTER_MOTIONBLUR
.. data:: RAS_2DFILTER_NOFILTER
.. data:: RAS_2DFILTER_PREWITT
.. data:: RAS_2DFILTER_SEPIA
.. data:: RAS_2DFILTER_SHARPEN
.. data:: RAS_2DFILTER_SOBEL

-------------------
Constraint Actuator
-------------------

.. data:: KX_ACT_CONSTRAINT_DISTANCE
.. data:: KX_ACT_CONSTRAINT_DOROTFH
.. data:: KX_ACT_CONSTRAINT_FHNX
.. data:: KX_ACT_CONSTRAINT_FHNY
.. data:: KX_ACT_CONSTRAINT_FHNZ
.. data:: KX_ACT_CONSTRAINT_FHPX
.. data:: KX_ACT_CONSTRAINT_FHPY
.. data:: KX_ACT_CONSTRAINT_FHPZ
.. data:: KX_ACT_CONSTRAINT_LOCAL
.. data:: KX_ACT_CONSTRAINT_MATERIAL
.. data:: KX_ACT_CONSTRAINT_NORMAL
.. data:: KX_ACT_CONSTRAINT_PERMANENT

---------------
Parent Actuator
---------------

.. data:: KX_PARENT_REMOVE
.. data:: KX_PARENT_SET

------
Shader
------

.. data:: VIEWMATRIX
.. data:: VIEWMATRIX_INVERSE
.. data:: VIEWMATRIX_INVERSETRANSPOSE
.. data:: VIEWMATRIX_TRANSPOSE
.. data:: MODELMATRIX
.. data:: MODELMATRIX_INVERSE
.. data:: MODELMATRIX_INVERSETRANSPOSE
.. data:: MODELMATRIX_TRANSPOSE
.. data:: MODELVIEWMATRIX
.. data:: MODELVIEWMATRIX_INVERSE
.. data:: MODELVIEWMATRIX_INVERSETRANSPOSE
.. data:: MODELVIEWMATRIX_TRANSPOSE
.. data:: CAM_POS

   Current camera position

.. data:: CONSTANT_TIMER

   User a timer for the uniform value.

.. data:: SHD_TANGENT

----------------
Blender Material
----------------

.. data:: BL_DST_ALPHA
.. data:: BL_DST_COLOR
.. data:: BL_ONE
.. data:: BL_ONE_MINUS_DST_ALPHA
.. data:: BL_ONE_MINUS_DST_COLOR
.. data:: BL_ONE_MINUS_SRC_ALPHA
.. data:: BL_ONE_MINUS_SRC_COLOR
.. data:: BL_SRC_ALPHA
.. data:: BL_SRC_ALPHA_SATURATE
.. data:: BL_SRC_COLOR
.. data:: BL_ZERO
