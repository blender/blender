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

---------------
Armature Sensor
---------------

.. _armaturesensor-type:

See :class:`bge.types.KX_ArmatureSensor.type`

.. data:: KX_ARMSENSOR_STATE_CHANGED

  Detect that the constraint is changing state (active/inactive)

  :value: 0
  
.. data:: KX_ARMSENSOR_LIN_ERROR_BELOW

  Detect that the constraint linear error is above a threshold
  
  :value: 1
  
.. data:: KX_ARMSENSOR_LIN_ERROR_ABOVE

  Detect that the constraint linear error is below a threshold

  :value: 2
  
.. data:: KX_ARMSENSOR_ROT_ERROR_BELOW

  Detect that the constraint rotation error is above a threshold
  
  :value: 3
  
.. data:: KX_ARMSENSOR_ROT_ERROR_ABOVE

  Detect that the constraint rotation error is below a threshold
  
  :value: 4
  

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

.. data:: KX_PROPSENSOR_LESSTHAN

   Activate when the property is less than the sensor value

   :value: 6

.. data:: KX_PROPSENSOR_GREATERTHAN

   Activate when the property is greater than the sensor value

   :value: 7

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

-----------------
Armature Actuator
-----------------

 .. _armatureactuator-constants-type:
   
See :class:`bge.types.BL_ArmatureActuator.type`

.. data:: KX_ACT_ARMATURE_RUN

  Just make sure the armature will be updated on the next graphic frame.
  This is the only persistent mode of the actuator:
  it executes automatically once per frame until stopped by a controller
  
  :value: 0

.. data:: KX_ACT_ARMATURE_ENABLE

  Enable the constraint.

  :value: 1

.. data:: KX_ACT_ARMATURE_DISABLE

  Disable the constraint (runtime constraint values are not updated).

  :value: 2

.. data:: KX_ACT_ARMATURE_SETTARGET

  Change target and subtarget of constraint.
  
  :value: 3

.. data:: KX_ACT_ARMATURE_SETWEIGHT

  Change weight of constraint (IK only).

  :value: 4

.. data:: KX_ACT_ARMATURE_SETINFLUENCE

  Change influence of constraint.

  :value: 5

-------------------
Constraint Actuator
-------------------

.. _constraint-actuator-option:

See :class:`bge.types.KX_ConstraintActuator.option`

* Applicable to Distance constraint:

.. data:: KX_CONSTRAINTACT_NORMAL

     Activate alignment to surface
   
.. data:: KX_CONSTRAINTACT_DISTANCE

     Activate distance control

.. data:: KX_CONSTRAINTACT_LOCAL

     Direction of the ray is along the local axis

* Applicable to Force field constraint:

.. data:: KX_CONSTRAINTACT_DOROTFH

     Force field act on rotation as well

* Applicable to both:

.. data:: KX_CONSTRAINTACT_MATERIAL

     Detect material rather than property
   
.. data:: KX_CONSTRAINTACT_PERMANENT

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
   
.. data:: KX_CONSTRAINTACT_FHNX

   Set force field along negative X axis
   
.. data:: KX_CONSTRAINTACT_FHNY

   Set force field along negative Y axis
   
.. data:: KX_CONSTRAINTACT_FHNZ

   Set force field along negative Z axis
   
.. data:: KX_CONSTRAINTACT_FHPX

   Set force field along positive X axis

.. data:: KX_CONSTRAINTACT_FHPY

   Set force field along positive Y axis
   
.. data:: KX_CONSTRAINTACT_FHPZ

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

.. _mouse-actuator:

---------------
Mouse Actuator
---------------

.. data:: KX_ACT_MOUSE_OBJECT_AXIS_X
.. data:: KX_ACT_MOUSE_OBJECT_AXIS_Y
.. data:: KX_ACT_MOUSE_OBJECT_AXIS_Z

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

-----------------
Steering Actuator
-----------------

.. _logic-steering-actuator:

See :class:`bge.types.KX_SteeringActuator.behavior`

.. data:: KX_STEERING_SEEK

   :value: 1

.. data:: KX_STEERING_FLEE

   :value: 2

.. data:: KX_STEERING_PATHFOLLOWING

   :value: 3

.. _logic-trackto-actuator:

-----------------
TrackTo Actuator
-----------------

See :class:`bge.types.KX_TrackToActuator`

.. data:: KX_TRACK_UPAXIS_POS_X
.. data:: KX_TRACK_UPAXIS_POS_Y
.. data:: KX_TRACK_UPAXIS_POS_Z
.. data:: KX_TRACK_TRAXIS_POS_X
.. data:: KX_TRACK_TRAXIS_POS_Y
.. data:: KX_TRACK_TRAXIS_POS_Z
.. data:: KX_TRACK_TRAXIS_NEG_X
.. data:: KX_TRACK_TRAXIS_NEG_Y
.. data:: KX_TRACK_TRAXIS_NEG_Z

=======
Various
=======

---------
2D Filter
---------

.. _Two-D-FilterActuator-mode:

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

----------------
Armature Channel
----------------
.. _armaturechannel-constants-rotation-mode:

See :class:`bge.types.BL_ArmatureChannel.rotation_mode`

.. note:
  euler mode are named as in Blender UI but the actual axis order is reversed

.. data:: ROT_MODE_QUAT

  Use quaternion in rotation attribute to update bone rotation.

  :value: 0

.. data:: ROT_MODE_XYZ

  Use euler_rotation and apply angles on bone's Z, Y, X axis successively.

  :value: 1

.. data:: ROT_MODE_XZY

  Use euler_rotation and apply angles on bone's Y, Z, X axis successively.

  :value: 2

.. data:: ROT_MODE_YXZ

  Use euler_rotation and apply angles on bone's Z, X, Y axis successively.

  :value: 3

.. data:: ROT_MODE_YZX

  Use euler_rotation and apply angles on bone's X, Z, Y axis successively.

  :value: 4

.. data:: ROT_MODE_ZXY

  Use euler_rotation and apply angles on bone's Y, X, Z axis successively.

  :value: 5

.. data:: ROT_MODE_ZYX

  Use euler_rotation and apply angles on bone's X, Y, Z axis successively.

  :value: 6


-------------------
Armature Constraint
-------------------
.. _armatureconstraint-constants-type:

See :class:`bge.types.BL_ArmatureConstraint.type`

.. data:: CONSTRAINT_TYPE_TRACKTO
.. data:: CONSTRAINT_TYPE_KINEMATIC
.. data:: CONSTRAINT_TYPE_ROTLIKE
.. data:: CONSTRAINT_TYPE_LOCLIKE
.. data:: CONSTRAINT_TYPE_MINMAX
.. data:: CONSTRAINT_TYPE_SIZELIKE
.. data:: CONSTRAINT_TYPE_LOCKTRACK
.. data:: CONSTRAINT_TYPE_STRETCHTO
.. data:: CONSTRAINT_TYPE_CLAMPTO
.. data:: CONSTRAINT_TYPE_TRANSFORM
.. data:: CONSTRAINT_TYPE_DISTLIMIT

.. _armatureconstraint-constants-ik-type:

See :class:`bge.types.BL_ArmatureConstraint.ik_type`
  
.. data:: CONSTRAINT_IK_COPYPOSE

   constraint is trying to match the position and eventually the rotation of the target.

   :value: 0

.. data:: CONSTRAINT_IK_DISTANCE

   Constraint is maintaining a certain distance to target subject to ik_mode

   :value: 1

.. _armatureconstraint-constants-ik-flag:

See :class:`bge.types.BL_ArmatureConstraint.ik_flag`

.. data:: CONSTRAINT_IK_FLAG_TIP

   Set when the constraint operates on the head of the bone and not the tail

   :value: 1

.. data:: CONSTRAINT_IK_FLAG_ROT

   Set when the constraint tries to match the orientation of the target

   :value: 2

.. data:: CONSTRAINT_IK_FLAG_STRETCH

   Set when the armature is allowed to stretch (only the bones with stretch factor > 0.0)

   :value: 16
   
.. data:: CONSTRAINT_IK_FLAG_POS

   Set when the constraint tries to match the position of the target.

   :value: 32

.. _armatureconstraint-constants-ik-mode:

See :class:`bge.types.BL_ArmatureConstraint.ik_mode`

.. data:: CONSTRAINT_IK_MODE_INSIDE

   The constraint tries to keep the bone within ik_dist of target

   :value: 0

.. data:: CONSTRAINT_IK_MODE_OUTSIDE

   The constraint tries to keep the bone outside ik_dist of the target

   :value: 1
   
.. data:: CONSTRAINT_IK_MODE_ONSURFACE

   The constraint tries to keep the bone exactly at ik_dist of the target.

   :value: 2

.. _input-status:

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

------------
Input Status
------------

See :class:`bge.types.SCA_PythonKeyboard`, :class:`bge.types.SCA_PythonMouse`, :class:`bge.types.SCA_MouseSensor`, :class:`bge.types.SCA_KeyboardSensor`

.. data:: KX_INPUT_NONE
.. data:: KX_INPUT_JUST_ACTIVATED
.. data:: KX_INPUT_ACTIVE
.. data:: KX_INPUT_JUST_RELEASED

-------------
KX_GameObject
-------------
.. _gameobject-playaction-mode:

See :class:`bge.types.KX_GameObject.playAction`

.. data:: KX_ACTION_MODE_PLAY

   Play the action once.
   
   :value: 0

.. data:: KX_ACTION_MODE_LOOP

   Loop the action (repeat it).
   
   :value: 1

.. data:: KX_ACTION_MODE_PING_PONG

   Play the action one direct then back the other way when it has completed.
   
   :value: 2

.. _gameobject-playaction-blend:

.. data:: KX_ACTION_BLEND_BLEND

   Blend layers using linear interpolation

   :value: 0

.. data:: KX_ACTION_BLEND_ADD

   Adds the layers together

   :value: 1

-------------
Mouse Buttons
-------------

See :class:`bge.types.SCA_MouseSensor`

.. data:: KX_MOUSE_BUT_LEFT
.. data:: KX_MOUSE_BUT_MIDDLE
.. data:: KX_MOUSE_BUT_RIGHT

--------------------------
Navigation Mesh Draw Modes
--------------------------

.. _navmesh-draw-mode:

.. data:: RM_WALLS

   Draw only the walls.

.. data:: RM_POLYS

   Draw only polygons.
 
.. data:: RM_TRIS

   Draw triangle mesh.
   
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
=======

Game Logic (bge.logic)
======================

************
Introduction
************

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
   * :class:`~bge.types.KX_MouseActuator`
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

.. data:: joysticks

   A list of attached :class:`~bge.types.SCA_PythonJoystick`.
   The list size is the maximum number of supported joysticks.
   If no joystick is available for a given slot, the slot is set to None.

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
   
.. function:: LibLoad(blend, type, data, load_actions=False, verbose=False, load_scripts=True, async=False)
   
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
   :arg async: Whether or not to do the loading asynchronously (in another thread). Only the "Scene" type is currently supported for this feature.
   :type async: bool
   
   :rtype: :class:`bge.types.KX_LibLoadStatus`

   .. note:: Asynchronously loaded libraries will not be available immediately after LibLoad() returns. Use the returned KX_LibLoadStatus to figure out when the libraries are ready.
   
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
   
   :arg gravity: gravity vector
   :type gravity: Vector((fx, fy, fz))

.. function:: getSpectrum()

   Returns a 512 point list from the sound card.
   This only works if the fmod sound driver is being used.
   
   :rtype: list [float], len(getSpectrum()) == 512

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

.. function:: getAnimRecordFrame()

    Gets the current frame number used for recording animations. This
    number is incremented automatically by Blender when the "Record
    animation" feature is turned on.

    :rtype: int

.. function:: setAnimRecordFrame(framenr)

    Sets the current frame number used for recording animations. This
    number is automatically incremented by Blender when the "Record
    animation" feature is turned on.

    The frame number Must be non-negative, unless Blender has
    :attr:`bpy.types.UserPreferencesEdit.use_negative_frames` enabled
    in its user preferences. Only use non-negative numbers to be on
    the safe side, unless you know what you are doing.

    :arg framenr: The new frame number.
    :type framenr: int

.. function:: getExitKey()

   Gets the key used to exit the game engine

   :return: The key (defaults to :mod:`bge.events.ESCKEY`)
   :rtype: int

.. function:: setExitKey(key)

   Sets the key used to exit the game engine

   :arg key: A key constant from :mod:`bge.events`
   :type key: int

.. function:: NextFrame()

   Render next frame (if Python has control)

.. function:: setRender(render)

   Sets the global flag that controls the render of the scene. 
   If True, the render is done after the logic frame.
   If False, the render is skipped and another logic frame starts immediately.

   .. note::

      GPU VSync no longer limits the number of frame per second when render is off, 
      but the *Use Frame Rate* option still regulates the fps. To run as many frames
      as possible, untick this option (Render Properties, System panel).

   :arg render: the render flag
   :type render: bool

.. function:: getRender()

   Get the current value of the global render flag

   :return: The flag value
   :rtype: bool

**********************
Time related functions
**********************

.. function:: getClockTime()

    Get the current BGE render time, in seconds. The BGE render time is the
    simulation time corresponding to the next scene that will be rendered.

    :rtype: double

.. function:: getFrameTime()

    Get the current BGE frame time, in seconds. The BGE frame time is the
    simulation time corresponding to the current call of the logic system.
    Generally speaking, it is what the user is interested in.

    :rtype: double

.. function:: getRealTime()

    Get the number of real (system-clock) seconds elapsed since the beginning
    of the simulation.

    :rtype: double

.. function:: getTimeScale()

    Get the time multiplier between real-time and simulation time. The default
    value is 1.0. A value greater than 1.0 means that the simulation is going
    faster than real-time, a value lower than 1.0 means that the simulation is
    going slower than real-time.

    :rtype: double

.. function:: setTimeScale(time_scale)

    Set the time multiplier between real-time and simulation time. A value
    greater than 1.0 means that the simulation is going faster than real-time,
    a value lower than 1.0 means that the simulation is going slower than
    real-time. Note that a too large value may lead to some physics
    instabilities.

    :arg time_scale: The new time multiplier.

.. function:: getUseExternalClock()

    Get if the BGE use the inner BGE clock, or rely or on an external
    clock. The default is to use the inner BGE clock.

    :rtype: bool

.. function:: setUseExternalClock(use_external_clock)

    Set if the BGE use the inner BGE clock, or rely or on an external
    clock. If the user selects the use of an external clock, he should call
    regularly the setClockTime method.

    :arg use_external_clock: the new setting

.. function:: setClockTime(new_time)

    Set the next value of the simulation clock. It is preferable to use this
    method from a custom main function in python, as calling it in the logic
    block can easily lead to a blocked system (if the time does not advance
    enough to run at least the next logic step).

    :arg new_time: the next value of the BGE clock (in second).
    

*****************
Utility functions
*****************

.. function:: expandPath(path)

   Converts a blender internal path into a proper file system path.

   Use / as directory separator in path
   You can use '//' at the start of the string to define a relative path;
   Blender replaces that string by the directory of the current .blend or runtime file
   to make a full path name. The function also converts the directory separator to
   the local file system format.

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

.. function:: PrintMemInfo()

   Prints engine statistics into the console

.. function:: getProfileInfo()

   Returns a Python dictionary that contains the same information as the on screen profiler. The keys are the profiler categories and the values are tuples with the first element being time taken (in ms) and the second element being the percentage of total time.
   
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

---------------
Armature Sensor
---------------

.. _armaturesensor-type:

See :class:`bge.types.KX_ArmatureSensor.type`

.. data:: KX_ARMSENSOR_STATE_CHANGED

  Detect that the constraint is changing state (active/inactive)

  :value: 0
  
.. data:: KX_ARMSENSOR_LIN_ERROR_BELOW

  Detect that the constraint linear error is above a threshold
  
  :value: 1
  
.. data:: KX_ARMSENSOR_LIN_ERROR_ABOVE

  Detect that the constraint linear error is below a threshold

  :value: 2
  
.. data:: KX_ARMSENSOR_ROT_ERROR_BELOW

  Detect that the constraint rotation error is above a threshold
  
  :value: 3
  
.. data:: KX_ARMSENSOR_ROT_ERROR_ABOVE

  Detect that the constraint rotation error is below a threshold
  
  :value: 4
  

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

.. data:: KX_PROPSENSOR_LESSTHAN

   Activate when the property is less than the sensor value

   :value: 6

.. data:: KX_PROPSENSOR_GREATERTHAN

   Activate when the property is greater than the sensor value

   :value: 7

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

-----------------
Armature Actuator
-----------------

 .. _armatureactuator-constants-type:
   
See :class:`bge.types.BL_ArmatureActuator.type`

.. data:: KX_ACT_ARMATURE_RUN

  Just make sure the armature will be updated on the next graphic frame.
  This is the only persistent mode of the actuator:
  it executes automatically once per frame until stopped by a controller
  
  :value: 0

.. data:: KX_ACT_ARMATURE_ENABLE

  Enable the constraint.

  :value: 1

.. data:: KX_ACT_ARMATURE_DISABLE

  Disable the constraint (runtime constraint values are not updated).

  :value: 2

.. data:: KX_ACT_ARMATURE_SETTARGET

  Change target and subtarget of constraint.
  
  :value: 3

.. data:: KX_ACT_ARMATURE_SETWEIGHT

  Change weight of constraint (IK only).

  :value: 4

.. data:: KX_ACT_ARMATURE_SETINFLUENCE

  Change influence of constraint.

  :value: 5

-------------------
Constraint Actuator
-------------------

.. _constraint-actuator-option:

See :class:`bge.types.KX_ConstraintActuator.option`

* Applicable to Distance constraint:

.. data:: KX_CONSTRAINTACT_NORMAL

     Activate alignment to surface
   
.. data:: KX_CONSTRAINTACT_DISTANCE

     Activate distance control

.. data:: KX_CONSTRAINTACT_LOCAL

     Direction of the ray is along the local axis

* Applicable to Force field constraint:

.. data:: KX_CONSTRAINTACT_DOROTFH

     Force field act on rotation as well

* Applicable to both:

.. data:: KX_CONSTRAINTACT_MATERIAL

     Detect material rather than property
   
.. data:: KX_CONSTRAINTACT_PERMANENT

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
   
.. data:: KX_CONSTRAINTACT_FHNX

   Set force field along negative X axis
   
.. data:: KX_CONSTRAINTACT_FHNY

   Set force field along negative Y axis
   
.. data:: KX_CONSTRAINTACT_FHNZ

   Set force field along negative Z axis
   
.. data:: KX_CONSTRAINTACT_FHPX

   Set force field along positive X axis

.. data:: KX_CONSTRAINTACT_FHPY

   Set force field along positive Y axis
   
.. data:: KX_CONSTRAINTACT_FHPZ

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

.. _mouse-actuator:

---------------
Mouse Actuator
---------------

.. data:: KX_ACT_MOUSE_OBJECT_AXIS_X
.. data:: KX_ACT_MOUSE_OBJECT_AXIS_Y
.. data:: KX_ACT_MOUSE_OBJECT_AXIS_Z

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

-----------------
Steering Actuator
-----------------

.. _logic-steering-actuator:

See :class:`bge.types.KX_SteeringActuator.behavior`

.. data:: KX_STEERING_SEEK

   :value: 1

.. data:: KX_STEERING_FLEE

   :value: 2

.. data:: KX_STEERING_PATHFOLLOWING

   :value: 3

.. _logic-trackto-actuator:

-----------------
TrackTo Actuator
-----------------

See :class:`bge.types.KX_TrackToActuator`

.. data:: KX_TRACK_UPAXIS_POS_X
.. data:: KX_TRACK_UPAXIS_POS_Y
.. data:: KX_TRACK_UPAXIS_POS_Z
.. data:: KX_TRACK_TRAXIS_POS_X
.. data:: KX_TRACK_TRAXIS_POS_Y
.. data:: KX_TRACK_TRAXIS_POS_Z
.. data:: KX_TRACK_TRAXIS_NEG_X
.. data:: KX_TRACK_TRAXIS_NEG_Y
.. data:: KX_TRACK_TRAXIS_NEG_Z

=======
Various
=======

---------
2D Filter
---------

.. _Two-D-FilterActuator-mode:

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

----------------
Armature Channel
----------------
.. _armaturechannel-constants-rotation-mode:

See :class:`bge.types.BL_ArmatureChannel.rotation_mode`

.. note:
  euler mode are named as in Blender UI but the actual axis order is reversed

.. data:: ROT_MODE_QUAT

  Use quaternion in rotation attribute to update bone rotation.

  :value: 0

.. data:: ROT_MODE_XYZ

  Use euler_rotation and apply angles on bone's Z, Y, X axis successively.

  :value: 1

.. data:: ROT_MODE_XZY

  Use euler_rotation and apply angles on bone's Y, Z, X axis successively.

  :value: 2

.. data:: ROT_MODE_YXZ

  Use euler_rotation and apply angles on bone's Z, X, Y axis successively.

  :value: 3

.. data:: ROT_MODE_YZX

  Use euler_rotation and apply angles on bone's X, Z, Y axis successively.

  :value: 4

.. data:: ROT_MODE_ZXY

  Use euler_rotation and apply angles on bone's Y, X, Z axis successively.

  :value: 5

.. data:: ROT_MODE_ZYX

  Use euler_rotation and apply angles on bone's X, Y, Z axis successively.

  :value: 6


-------------------
Armature Constraint
-------------------
.. _armatureconstraint-constants-type:

See :class:`bge.types.BL_ArmatureConstraint.type`

.. data:: CONSTRAINT_TYPE_TRACKTO
.. data:: CONSTRAINT_TYPE_KINEMATIC
.. data:: CONSTRAINT_TYPE_ROTLIKE
.. data:: CONSTRAINT_TYPE_LOCLIKE
.. data:: CONSTRAINT_TYPE_MINMAX
.. data:: CONSTRAINT_TYPE_SIZELIKE
.. data:: CONSTRAINT_TYPE_LOCKTRACK
.. data:: CONSTRAINT_TYPE_STRETCHTO
.. data:: CONSTRAINT_TYPE_CLAMPTO
.. data:: CONSTRAINT_TYPE_TRANSFORM
.. data:: CONSTRAINT_TYPE_DISTLIMIT

.. _armatureconstraint-constants-ik-type:

See :class:`bge.types.BL_ArmatureConstraint.ik_type`
  
.. data:: CONSTRAINT_IK_COPYPOSE

   constraint is trying to match the position and eventually the rotation of the target.

   :value: 0

.. data:: CONSTRAINT_IK_DISTANCE

   Constraint is maintaining a certain distance to target subject to ik_mode

   :value: 1

.. _armatureconstraint-constants-ik-flag:

See :class:`bge.types.BL_ArmatureConstraint.ik_flag`

.. data:: CONSTRAINT_IK_FLAG_TIP

   Set when the constraint operates on the head of the bone and not the tail

   :value: 1

.. data:: CONSTRAINT_IK_FLAG_ROT

   Set when the constraint tries to match the orientation of the target

   :value: 2

.. data:: CONSTRAINT_IK_FLAG_STRETCH

   Set when the armature is allowed to stretch (only the bones with stretch factor > 0.0)

   :value: 16
   
.. data:: CONSTRAINT_IK_FLAG_POS

   Set when the constraint tries to match the position of the target.

   :value: 32

.. _armatureconstraint-constants-ik-mode:

See :class:`bge.types.BL_ArmatureConstraint.ik_mode`

.. data:: CONSTRAINT_IK_MODE_INSIDE

   The constraint tries to keep the bone within ik_dist of target

   :value: 0

.. data:: CONSTRAINT_IK_MODE_OUTSIDE

   The constraint tries to keep the bone outside ik_dist of the target

   :value: 1
   
.. data:: CONSTRAINT_IK_MODE_ONSURFACE

   The constraint tries to keep the bone exactly at ik_dist of the target.

   :value: 2

.. _input-status:

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

------------
Input Status
------------

See :class:`bge.types.SCA_PythonKeyboard`, :class:`bge.types.SCA_PythonMouse`, :class:`bge.types.SCA_MouseSensor`, :class:`bge.types.SCA_KeyboardSensor`

.. data:: KX_INPUT_NONE
.. data:: KX_INPUT_JUST_ACTIVATED
.. data:: KX_INPUT_ACTIVE
.. data:: KX_INPUT_JUST_RELEASED

-------------
KX_GameObject
-------------
.. _gameobject-playaction-mode:

See :class:`bge.types.KX_GameObject.playAction`

.. data:: KX_ACTION_MODE_PLAY

   Play the action once.
   
   :value: 0

.. data:: KX_ACTION_MODE_LOOP

   Loop the action (repeat it).
   
   :value: 1

.. data:: KX_ACTION_MODE_PING_PONG

   Play the action one direct then back the other way when it has completed.
   
   :value: 2

.. _gameobject-playaction-blend:

.. data:: KX_ACTION_BLEND_BLEND

   Blend layers using linear interpolation

   :value: 0

.. data:: KX_ACTION_BLEND_ADD

   Adds the layers together

   :value: 1

-------------
Mouse Buttons
-------------

See :class:`bge.types.SCA_MouseSensor`

.. data:: KX_MOUSE_BUT_LEFT
.. data:: KX_MOUSE_BUT_MIDDLE
.. data:: KX_MOUSE_BUT_RIGHT

--------------------------
Navigation Mesh Draw Modes
--------------------------

.. _navmesh-draw-mode:

.. data:: RM_WALLS

   Draw only the walls.

.. data:: RM_POLYS

   Draw only polygons.
 
.. data:: RM_TRIS

   Draw triangle mesh.
   
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
