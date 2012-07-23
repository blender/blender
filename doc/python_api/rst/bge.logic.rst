
Game Logic (bge.logic)
======================

*****
Intro
*****

Module to access logic functions, imported automatically into the python controllers namespace.

.. module:: bge.logic

.. code-block:: python

   # To get the controller thats running this python script:
   cont = bge.logic.getCurrentController() # bge.logic is automatically imported
   
   # To get the game object this controller is on:
   obj = cont.owner

:class:`~bge.types.KX_GameObject` and :class:`~bge.types.KX_Camera` or :class:`~bge.types.KX_LightObject` methods are available depending on the type of object

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

.. hlist::
   :columns: 3

   * :class:`~bge.types.KX_MouseFocusSensor`
   * :class:`~bge.types.KX_NearSensor`
   * :class:`~bge.types.KX_NetworkMessageSensor`
   * :class:`~bge.types.KX_RadarSensor`
   * :class:`~bge.types.KX_RaySensor`
   * :class:`~bge.types.KX_TouchSensor`
   * :class:`~bge.types.SCA_DelaySensor`
   * :class:`~bge.types.SCA_JoystickSensor`
   * :class:`~bge.types.SCA_KeyboardSensor`
   * :class:`~bge.types.SCA_MouseSensor`
   * :class:`~bge.types.SCA_PropertySensor`
   * :class:`~bge.types.SCA_RandomSensor`

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

.. hlist::
   :columns: 3
   
   * :class:`~bge.types.BL_ActionActuator`
   * :class:`~bge.types.KX_CameraActuator`
   * :class:`~bge.types.KX_ConstraintActuator`
   * :class:`~bge.types.KX_GameActuator`
   * :class:`~bge.types.KX_NetworkMessageActuator`
   * :class:`~bge.types.KX_ObjectActuator`
   * :class:`~bge.types.KX_ParentActuator`
   * :class:`~bge.types.KX_SCA_AddObjectActuator`
   * :class:`~bge.types.KX_SCA_DynamicActuator`
   * :class:`~bge.types.KX_SCA_EndObjectActuator`
   * :class:`~bge.types.KX_SCA_ReplaceMeshActuator`
   * :class:`~bge.types.KX_SceneActuator`
   * :class:`~bge.types.KX_SoundActuator`
   * :class:`~bge.types.KX_StateActuator`
   * :class:`~bge.types.KX_TrackToActuator`
   * :class:`~bge.types.KX_VisibilityActuator`
   * :class:`~bge.types.SCA_2DFilterActuator`
   * :class:`~bge.types.SCA_PropertyActuator`
   * :class:`~bge.types.SCA_RandomActuator`

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

:class:`bge.types.KX_Camera` has some examples using matrices.

*********
Variables
*********

.. data:: globalDict

   A dictionary that is saved between loading blend files so you can use it to store inventory and other variables you want to store between scenes and blend files.
   It can also be written to a file and loaded later on with the game load/save actuators.

   .. note:: only python built in types such as int/string/bool/float/tuples/lists can be saved, GameObjects, Actuators etc will not work as expected.

.. data:: keyboard

   The current keyboard wrapped in an :class:`~bge.types.SCA_PythonKeyboard` object.

.. data:: mouse

   The current mouse wrapped in an :class:`~bge.types.SCA_PythonMouse` object.

*****************
General functions
*****************

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

.. function:: startGame(blend)

   Loads the blend file.
   
   :arg blend: The name of the blend file
   :type blend: string

.. function:: endGame()

   Ends the current game.

.. function:: restartGame()

   Restarts the current game by reloading the .blend file (the last saved version, not what is currently running).
   
.. function:: LibLoad(blend, type, data, load_actions=False, verbose=False, load_scripts=True)
   
   Converts the all of the datablocks of the given type from the given blend.
   
   :arg blend: The path to the blend file (or the name to use for the library if data is supplied)
   :type blend: string
   :arg type: The datablock type (currently only "Action", "Mesh" and "Scene" are supported)
   :type type: string
   :arg data: Binary data from a blend file (optional)
   :type data: bytes
   :arg load_actions: Search for and load all actions in a given Scene and not just the "active" actions (Scene type only)
   :type load_actions: bool
   :arg verbose: Whether or not to print debugging information (e.g., "SceneName: Scene")
   :type verbose: bool
   :arg load_scripts: Whether or not to load text datablocks as well (can be disabled for some extra security)
   :type load_scripts: bool
   
.. function:: LibNew(name, type, data)

   Uses existing datablock data and loads in as a new library.
   
   :arg name: A unique library name used for removal later
   :type name: string
   :arg type: The datablock type (currently only "Mesh" is supported)
   :type type: string
   :arg data: A list of names of the datablocks to load
   :type data: list of strings
   
.. function:: LibFree(name)

   Frees a library, removing all objects and meshes from the currently active scenes.

   :arg name: The name of the library to free (the name used in LibNew)
   :type name: string
   
.. function:: LibList()

   Returns a list of currently loaded libraries.
   
   :rtype: list [str]

.. function:: addScene(name, overlay=1)

   Loads a scene into the game engine.

   .. note::

      This function is not effective immediately, the scene is queued
      and added on the next logic cycle where it will be available
      from `getSceneList`

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

   Gets the maximum number of logic frames per render frame.
   
   :return: The maximum number of logic frames per render frame
   :rtype: integer

.. function:: setMaxLogicFrame(maxlogic)

   Sets the maximum number of logic frames that are executed per render frame.
   This does not affect the physic system that still runs at full frame rate.   
    
   :arg maxlogic: The new maximum number of logic frames per render frame. Valid values: 1..5
   :type maxlogic: integer

.. function:: getMaxPhysicsFrame()

   Gets the maximum number of physics frames per render frame.
   
   :return: The maximum number of physics frames per render frame
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

*****************
Utility functions
*****************

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

.. function:: getAverageFrameRate()

   Gets the estimated/average framerate for all the active scenes, not only the current scene.

   :return: The estimated average framerate in frames per second
   :rtype: float

.. function:: getBlendFileList(path = "//")

   Returns a list of blend files in the same directory as the open blend file, or from using the option argument.

   :arg path: Optional directory argument, will be expanded (like expandPath) into the full path.
   :type path: string
   :return: A list of filenames, with no directory prefix
   :rtype: list

.. function:: getRandomFloat()

   Returns a random floating point value in the range [0 - 1)

.. function:: PrintGLInfo()

   Prints GL Extension Info into the console
   
*********
Constants
*********

.. data:: KX_TRUE

   True value used by some modules.

.. data:: KX_FALSE

   False value used by some modules.

=======
Sensors
======= 

.. _sensor-status:

-------------
Sensor Status
-------------

.. data:: KX_SENSOR_INACTIVE
.. data:: KX_SENSOR_JUST_ACTIVATED
.. data:: KX_SENSOR_ACTIVE
.. data:: KX_SENSOR_JUST_DEACTIVATED

.. _logic-property-sensor:

---------------
Property Sensor
---------------

.. data:: KX_PROPSENSOR_EQUAL

   Activate when the property is equal to the sensor value.

   :value: 1

.. data:: KX_PROPSENSOR_NOTEQUAL

   Activate when the property is not equal to the sensor value.
   
   :value: 2

.. data:: KX_PROPSENSOR_INTERVAL

   Activate when the property is between the specified limits.
   
   :value: 3
   
.. data:: KX_PROPSENSOR_CHANGED

   Activate when the property changes   

   :value: 4

.. data:: KX_PROPSENSOR_EXPRESSION

   Activate when the expression matches
   
   :value: 5

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


=========
Actuators
=========

.. _action-actuator:

---------------
Action Actuator
---------------

See :class:`bge.types.BL_ActionActuator`

.. data:: KX_ACTIONACT_PLAY
.. data:: KX_ACTIONACT_PINGPONG
.. data:: KX_ACTIONACT_FLIPPER
.. data:: KX_ACTIONACT_LOOPSTOP
.. data:: KX_ACTIONACT_LOOPEND
.. data:: KX_ACTIONACT_PROPERTY

-------------------
Constraint Actuator
-------------------

.. _constraint-actuator-option:

See :class:`bge.types.KX_ConstraintActuator.option`

* Applicable to Distance constraint:

  .. data:: KX_ACT_CONSTRAINT_NORMAL

     Activate alignment to surface
   
  .. data:: KX_ACT_CONSTRAINT_DISTANCE

     Activate distance control

  .. data:: KX_ACT_CONSTRAINT_LOCAL

     Direction of the ray is along the local axis

* Applicable to Force field constraint:

  .. data:: KX_ACT_CONSTRAINT_DOROTFH

     Force field act on rotation as well

* Applicable to both:

  .. data:: KX_ACT_CONSTRAINT_MATERIAL

     Detect material rather than property
   
  .. data:: KX_ACT_CONSTRAINT_PERMANENT

     No deactivation if ray does not hit target

.. _constraint-actuator-limit:

See :class:`bge.types.KX_ConstraintActuator.limit`

.. data:: KX_CONSTRAINTACT_LOCX

   Limit X coord.
   
.. data:: KX_CONSTRAINTACT_LOCY

   Limit Y coord

.. data:: KX_CONSTRAINTACT_LOCZ

   Limit Z coord
   
.. data:: KX_CONSTRAINTACT_ROTX

   Limit X rotation

.. data:: KX_CONSTRAINTACT_ROTY

   Limit Y rotation
   
.. data:: KX_CONSTRAINTACT_ROTZ

   Limit Z rotation
   
.. data:: KX_CONSTRAINTACT_DIRNX

   Set distance along negative X axis

.. data:: KX_CONSTRAINTACT_DIRNY

   Set distance along negative Y axis
   
.. data:: KX_CONSTRAINTACT_DIRNZ

   Set distance along negative Z axis
   
.. data:: KX_CONSTRAINTACT_DIRPX

   Set distance along positive X axis

.. data:: KX_CONSTRAINTACT_DIRPY

   Set distance along positive Y axis
   
.. data:: KX_CONSTRAINTACT_DIRPZ

   Set distance along positive Z axis
   
.. data:: KX_CONSTRAINTACT_ORIX

   Set orientation of X axis
   
.. data:: KX_CONSTRAINTACT_ORIY

   Set orientation of Y axis
   
.. data:: KX_CONSTRAINTACT_ORIZ

   Set orientation of Z axis
   
.. data:: KX_ACT_CONSTRAINT_FHNX

   Set force field along negative X axis
   
.. data:: KX_ACT_CONSTRAINT_FHNY

   Set force field along negative Y axis
   
.. data:: KX_ACT_CONSTRAINT_FHNZ

   Set force field along negative Z axis
   
.. data:: KX_ACT_CONSTRAINT_FHPX

   Set force field along positive X axis

.. data:: KX_ACT_CONSTRAINT_FHPY

   Set force field along positive Y axis
   
.. data:: KX_ACT_CONSTRAINT_FHPZ

   Set force field along positive Z axis

----------------
Dynamic Actuator
----------------

See :class:`bge.types.KX_SCA_DynamicActuator`

.. data:: KX_DYN_RESTORE_DYNAMICS
.. data:: KX_DYN_DISABLE_DYNAMICS
.. data:: KX_DYN_ENABLE_RIGID_BODY
.. data:: KX_DYN_DISABLE_RIGID_BODY
.. data:: KX_DYN_SET_MASS

.. _game-actuator:

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

---------------
Parent Actuator
---------------

.. data:: KX_PARENT_REMOVE
.. data:: KX_PARENT_SET

.. _logic-random-distributions:

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

.. _logic-sound-actuator:

--------------
Sound Actuator
--------------
      
See :class:`bge.types.KX_SoundActuator`

.. data:: KX_SOUNDACT_PLAYSTOP

   :value: 1
   
.. data:: KX_SOUNDACT_PLAYEND

   :value: 2
   
.. data:: KX_SOUNDACT_LOOPSTOP

   :value: 3
   
.. data:: KX_SOUNDACT_LOOPEND

   :value: 4
   
.. data:: KX_SOUNDACT_LOOPBIDIRECTIONAL

   :value: 5
   
.. data:: KX_SOUNDACT_LOOPBIDIRECTIONAL_STOP

   :value: 6
   

=======
Various
=======

.. _input-status:

------------
Input Status
------------

See :class:`bge.types.SCA_PythonKeyboard`, :class:`bge.types.SCA_PythonMouse`, :class:`bge.types.SCA_MouseSensor`, :class:`bge.types.SCA_KeyboardSensor`

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

.. _state-actuator-operation:

See :class:`bge.types.KX_StateActuator.operation`

.. data:: KX_STATE_OP_CLR

   Substract bits to state mask
   
   :value: 0

.. data:: KX_STATE_OP_CPY

   Copy state mask
   
   :value: 1
   
.. data:: KX_STATE_OP_NEG

   Invert bits to state mask
   
   :value: 2
   
.. data:: KX_STATE_OP_SET

   Add bits to state mask
   
   :value: 3
   
.. _Two-D-FilterActuator-mode:

---------
2D Filter
---------

.. data:: RAS_2DFILTER_BLUR

   :value: 2
   
.. data:: RAS_2DFILTER_CUSTOMFILTER

   Customer filter, the code code is set via shaderText property.
   
   :value: 12
   
.. data:: RAS_2DFILTER_DILATION

   :value: 4
   
.. data:: RAS_2DFILTER_DISABLED

   Disable the filter that is currently active

   :value: -1
   
.. data:: RAS_2DFILTER_ENABLED

   Enable the filter that was previously disabled

   :value: -2
   
.. data:: RAS_2DFILTER_EROSION

   :value: 5
   
.. data:: RAS_2DFILTER_GRAYSCALE

   :value: 9
   
.. data:: RAS_2DFILTER_INVERT

   :value: 11
   
.. data:: RAS_2DFILTER_LAPLACIAN

   :value: 6
   
.. data:: RAS_2DFILTER_MOTIONBLUR

   Create and enable preset filters

   :value: 1
   
.. data:: RAS_2DFILTER_NOFILTER

   Disable and destroy the filter that is currently active

   :value: 0
   
.. data:: RAS_2DFILTER_PREWITT

   :value: 8
   
.. data:: RAS_2DFILTER_SEPIA

   :value: 10
   
.. data:: RAS_2DFILTER_SHARPEN

   :value: 3
   
.. data:: RAS_2DFILTER_SOBEL

   :value: 7
   
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
