
Game Engine  bge.types Module.
==============================

.. module:: bge.types

.. class:: PyObjectPlus

   PyObjectPlus base class of most other types in the Game Engine.

   .. attribute:: invalid

      Test if the object has been freed by the game engine and is no longer valid.

      Normally this is not a problem but when storing game engine data in the GameLogic module, 
      KX_Scenes or other KX_GameObjects its possible to hold a reference to invalid data.
      Calling an attribute or method on an invalid object will raise a SystemError.

      The invalid attribute allows testing for this case without exception handling.

      *type* bool

   .. method:: isA(game_type)

      Check if this is a type or a subtype game_type.

      :arg game_type: the name of the type or the type its self from the :mod:`bge.types` module.
      :type game_type: string or type
      :return: True if this object is a type or a subtype of game_type.
      :rtype: bool

.. class:: CValue(PyObjectPlus)

   This class is a basis for other classes.

   .. attribute:: name

      The name of this CValue derived object (read-only). **type** string

.. class:: CPropValue(CValue)

   This class has no python functions

.. class:: SCA_ILogicBrick(CValue)

   Base class for all logic bricks.

   .. attribute:: executePriority

      This determines the order controllers are evaluated, and actuators are activated (lower priority is executed first).

      *type* executePriority: int

   .. attribute:: owner

      The game object this logic brick is attached to (read-only). **type** :class:`KX_GameObject` or None in exceptional cases.

   .. attribute:: name

      The name of this logic brick (read-only). **type** string

.. class:: SCA_PythonKeyboard(PyObjectPlus)

   The current keyboard.

   .. attribute:: events

      A list of pressed keys that have either been pressed, or just released, or are active this frame. (read-only).

      * 'keycode' matches the values in :mod:`bge.keys`.
      * 'status' uses...
         * :mod:`bge.logic.KX_INPUT_NONE`
         * :mod:`bge.logic.KX_INPUT_JUST_ACTIVATED`
         * :mod:`bge.logic.KX_INPUT_ACTIVE`
         * :mod:`bge.logic.KX_INPUT_JUST_RELEASED`

      *type* list [[keycode, status], ...]

.. class:: SCA_PythonMouse(PyObjectPlus)

   The current mouse.

   .. attribute:: events

      a list of pressed buttons that have either been pressed, or just released, or are active this frame. (read-only).

      * 'keycode' matches the values in :mod:`bge.keys`.
      * 'status' uses...
         * :mod:`bge.logic.KX_INPUT_NONE`
         * :mod:`bge.logic.KX_INPUT_JUST_ACTIVATED`
         * :mod:`bge.logic.KX_INPUT_ACTIVE`
         * :mod:`bge.logic.KX_INPUT_JUST_RELEASED`

      *type* list [[keycode, status], ...]

   .. attribute:: position

      The normalized x and y position of the mouse cursor. **type** list [x, y]

   .. attribute:: visible

      The visibility of the mouse cursor. **type** boolean

.. class:: SCA_IObject(CValue)

   This class has no python functions

.. class:: SCA_ISensor(SCA_ILogicBrick)

   Base class for all sensor logic bricks.

   .. attribute:: usePosPulseMode

      Flag to turn positive pulse mode on and off. **type** boolean

   .. attribute:: useNegPulseMode

      Flag to turn negative pulse mode on and off. **type** boolean

   .. attribute:: frequency

      The frequency for pulse mode sensors. **type** integer

   .. attribute:: level

      level Option whether to detect level or edge transition when entering a state.
      It makes a difference only in case of logic state transition (state actuator).
      A level detector will immediately generate a pulse, negative or positive
      depending on the sensor condition, as soon as the state is activated.
      A edge detector will wait for a state change before generating a pulse.
      note: mutually exclusive with :data:`tap`, enabling will disable :data:`tap`.

      *type* boolean

   .. attribute:: tap

      When enabled only sensors that are just activated will send a positive event, 
      after this they will be detected as negative by the controllers.
      This will make a key thats held act as if its only tapped for an instant.
      note: mutually exclusive with :data:`level`, enabling will disable :data:`level`.

      *type* boolean

   .. attribute:: invert

      Flag to set if this sensor activates on positive or negative events. **type** boolean

   .. attribute:: triggered

      True if this sensor brick is in a positive state. (read-only). **type** boolean

   .. attribute:: positive

      True if this sensor brick is in a positive state. (read-only). **type** boolean

   .. attribute:: status

      The status of the sensor. (read-only). **type** int from 0-3.

      * KX_SENSOR_INACTIVE
      * KX_SENSOR_JUST_ACTIVATED
      * KX_SENSOR_ACTIVE
      * KX_SENSOR_JUST_DEACTIVATED

      .. note:: this convenient attribute combines the values of triggered and positive attributes.

   .. method:: reset()

      Reset sensor internal state, effect depends on the type of sensor and settings.

      The sensor is put in its initial state as if it was just activated.

.. class:: SCA_IController(SCA_ILogicBrick)

   Base class for all controller logic bricks.

   .. attribute:: state

      The controllers state bitmask. This can be used with the GameObject's state to test if the controller is active. **type** int bitmask

   .. attribute:: sensors

      A list of sensors linked to this controller. **type** sequence supporting index/string lookups and iteration.

      .. note:: The sensors are not necessarily owned by the same object.
      .. note:: When objects are instanced in dupligroups links may be lost from objects outside the dupligroup.

   .. attribute:: actuators

      A list of actuators linked to this controller. **type** sequence supporting index/string lookups and iteration.

      .. note:: The sensors are not necessarily owned by the same object.
      .. note:: When objects are instanced in dupligroups links may be lost from objects outside the dupligroup.

   .. attribute:: useHighPriority

      When set the controller executes always before all other controllers that dont have this set. **type** bool

      .. note:: Order of execution between high priority controllers is not guaranteed.

.. class:: SCA_IActuator(SCA_ILogicBrick)

   Base class for all actuator logic bricks.

.. class:: BL_ActionActuator(SCA_IActuator)

   Action Actuators apply an action to an actor.

   .. attribute:: action

      The name of the action to set as the current action. **type** string

   .. attribute:: channelNames

      A list of channel names that may be used with :data:`setChannel` and :data:`getChannel`. **type** list of strings

   .. attribute:: frameStart

      Specifies the starting frame of the animation. **type** float

   .. attribute:: frameEnd

      Specifies the ending frame of the animation. **type** float

   .. attribute:: blendIn

      Specifies the number of frames of animation to generate when making transitions between actions. **type** float

   .. attribute:: priority

      Sets the priority of this actuator. Actuators will lower priority numbers will override actuators with higher numbers. **type** integer

   .. attribute:: frame

      Sets the current frame for the animation. **type** float

   .. attribute:: propName

      Sets the property to be used in FromProp playback mode. **type** string

   .. attribute:: blendTime

      Sets the internal frame timer. This property must be in the range from 0.0 to blendIn. **type** float

   .. attribute:: mode

      The operation mode of the actuator. KX_ACTIONACT_PLAY, KX_ACTIONACT_PROPERTY, KX_ACTIONACT_FLIPPER, KX_ACTIONACT_LOOPSTOP, KX_ACTIONACT_LOOPEND. **type** integer

   .. attribute:: useContinue

      The actions continue option, True or False. When True, the action will always play from where last left off, otherwise negative events to this actuator will reset it to its start frame. **type** boolean

   .. attribute:: framePropName

      The name of the property that is set to the current frame number. **type** string

   .. method:: setChannel(channel, matrix)

      Alternative to the 2 arguments, 4 arguments (channel, matrix, loc, size, quat) are also supported.

      :arg channel: A string specifying the name of the bone channel, error raised if not in :data:`channelNames`.
      :type channel: string
      :arg matrix: A 4x4 matrix specifying the overriding transformation as an offset from the bone's rest position.
      :arg  matrix: list [[float]]

      .. note:: These values are relative to the bones rest position, currently the api has no way to get this info (which is annoying), but can be worked around by using bones with a rest pose that has no translation.

   .. method:: getChannel(channel)

      :arg channel: A string specifying the name of the bone channel. error raised if not in :data:`channelNames`.
      :type channel: string
      :return: (loc, size, quat)
      :rtype: tuple

.. class:: BL_Shader(PyObjectPlus)

   BL_Shader GLSL shaders.

   TODO - Description

   .. method:: setUniformfv(name, fList)

      Set a uniform with a list of float values

      :arg name: the uniform name
      :type name: string
      :arg fList: a list (2, 3 or 4 elements) of float values
      :type fList: list[float]

   .. method:: delSource()

      Clear the shader. Use this method before the source is changed with :data:`setSource`.

   .. method:: getFragmentProg()

      Returns the fragment program.

      :return: The fragment program.
      :rtype: string

   .. method:: getVertexProg()

      Get the vertex program.

      :return: The vertex program.
      :rtype: string

   .. method:: isValid()

      Check if the shader is valid.

      :return: True if the shader is valid
      :rtype: bool

   .. method:: setAttrib(enum)

      Set attribute location. (The parameter is ignored a.t.m. and the value of "tangent" is always used.)

      :arg enum: attribute location value
      :type enum: integer

   .. method:: setNumberOfPasses( max_pass )

      Set the maximum number of passes. Not used a.t.m.

      :arg max_pass: the maximum number of passes
      :type max_pass: integer

   .. method:: setSampler(name, index)

      Set uniform texture sample index.

      :arg name: Uniform name
      :type name: string
      :arg index: Texture sample index.
      :type index: integer

   .. method:: setSource(vertexProgram, fragmentProgram)

      Set the vertex and fragment programs

      :arg vertexProgram: Vertex program
      :type vertexProgram: string
      :arg fragmentProgram: Fragment program
      :type fragmentProgram: string

   .. method:: setUniform1f(name, fx)

      Set a uniform with 1 float value.

      :arg name: the uniform name
      :type name: string
      :arg fx: Uniform value
      :type fx: float

   .. method:: setUniform1i(name, ix)

      Set a uniform with an integer value.

      :arg name: the uniform name
      :type name: string
      :arg ix: the uniform value
      :type ix: integer

   .. method:: setUniform2f(name, fx, fy)

      Set a uniform with 2 float values

      :arg name: the uniform name
      :type name: string
      :arg fx: first float value
      :type fx: float

      :arg fy: second float value
      :type fy: float

   .. method:: setUniform2i(name, ix, iy)

      Set a uniform with 2 integer values

      :arg name: the uniform name
      :type name: string
      :arg ix: first integer value
      :type ix: integer
      :arg iy: second integer value
      :type iy: integer

   .. method:: setUniform3f(name, fx, fy, fz)

      Set a uniform with 3 float values.

      :arg name: the uniform name
      :type name: string
      :arg fx: first float value
      :type fx: float
      :arg fy: second float value
      :type fy: float
      :arg fz: third float value
      :type fz: float

   .. method:: setUniform3i(name, ix, iy, iz)

      Set a uniform with 3 integer values

      :arg name: the uniform name
      :type name: string
      :arg ix: first integer value
      :type ix: integer
      :arg iy: second integer value
      :type iy: integer
      :arg iz: third integer value
      :type iz: integer

   .. method:: setUniform4f(name, fx, fy, fz, fw)

      Set a uniform with 4 float values.

      :arg name: the uniform name
      :type name: string
      :arg fx: first float value
      :type fx: float
      :arg fy: second float value
      :type fy: float
      :arg fz: third float value
      :type fz: float
      :arg fw: fourth float value
      :type fw: float

   .. method:: setUniform4i(name, ix, iy, iz, iw)

      Set a uniform with 4 integer values

      :arg name: the uniform name
      :type name: string
      :arg ix: first integer value
      :type ix: integer
      :arg iy: second integer value
      :type iy: integer
      :arg iz: third integer value
      :type iz: integer
      :arg iw: fourth integer value
      :type iw: integer

   .. method:: setUniformDef(name, type)

      Define a new uniform

      :arg name: the uniform name
      :type name: string
      :arg type: uniform type
      :type type: UNI_NONE, UNI_INT, UNI_FLOAT, UNI_INT2, UNI_FLOAT2, UNI_INT3, UNI_FLOAT3, UNI_INT4, UNI_FLOAT4, UNI_MAT3, UNI_MAT4, UNI_MAX

   .. method:: setUniformMatrix3(name, mat, transpose)

      Set a uniform with a 3x3 matrix value

      :arg name: the uniform name
      :type name: string
      :arg mat: A 3x3 matrix [[f, f, f], [f, f, f], [f, f, f]]
      :type mat: 3x3 matrix
      :arg transpose: set to True to transpose the matrix
      :type transpose: bool

   .. method:: setUniformMatrix4(name, mat, transpose)

      Set a uniform with a 4x4 matrix value

      :arg name: the uniform name
      :type name: string
      :arg mat: A 4x4 matrix [[f, f, f, f], [f, f, f, f], [f, f, f, f], [f, f, f, f]]
      :type mat: 4x4 matrix
      :arg transpose: set to True to transpose the matrix
      :type transpose: bool

   .. method:: setUniformiv(name, iList)

      Set a uniform with a list of integer values

      :arg name: the uniform name
      :type name: string
      :arg iList: a list (2, 3 or 4 elements) of integer values
      :type iList: list[integer]

   .. method:: validate()

      Validate the shader object.

.. class:: BL_ShapeActionActuator(SCA_IActuator)

   ShapeAction Actuators apply an shape action to an mesh object.

   .. attribute:: action

      The name of the action to set as the current shape action. **type** string

   .. attribute:: frameStart

      Specifies the starting frame of the shape animation. **type** float

   .. attribute:: frameEnd

      Specifies the ending frame of the shape animation. **type** float

   .. attribute:: blendIn

      Specifies the number of frames of animation to generate when making transitions between actions. **type** float

   .. attribute:: priority

      Sets the priority of this actuator. Actuators will lower priority numbers will override actuators with higher numbers. **type** integer

   .. attribute:: frame

      Sets the current frame for the animation. **type** float

   .. attribute:: propName

      Sets the property to be used in FromProp playback mode. **type** string

   .. attribute:: blendTime

      Sets the internal frame timer. This property must be in the range from 0.0 to blendin. **type** float

   .. attribute:: mode

      The operation mode of the actuator in [KX_ACTIONACT_PLAY, KX_ACTIONACT_PROPERTY, KX_ACTIONACT_FLIPPER, KX_ACTIONACT_LOOPSTOP, KX_ACTIONACT_LOOPEND] **type** integer

   .. attribute:: framePropName

      The name of the property that is set to the current frame number. **type** string

.. class:: CListValue(CPropValue)

   CListValue

   This is a list like object used in the game engine internally that behaves similar to a python list in most ways.

   As well as the normal index lookup.
   ``val= clist[i]``

   CListValue supports string lookups.
   ``val= scene.objects["Cube"]``

   Other operations such as ``len(clist), list(clist), clist[0:10]`` are also supported.

   .. method:: append(val)

      Add an item to the list (like pythons append)

      .. warning:: Appending values to the list can cause crashes when the list is used internally by the game engine.

   .. method:: count(val)

      Count the number of instances of a value in the list.

      :return: number of instances
      :rtype: integer

   .. method:: index(val)

      Return the index of a value in the list.

      :return: The index of the value in the list.
      :rtype: integer

   .. method:: reverse()

      Reverse the order of the list.

   .. method:: get(key, default=None)

      Return the value matching key, or the default value if its not found.

      :return: The key value or a default.

   .. method:: from_id(id)

      This is a funtion especially for the game engine to return a value with a spesific id.

      Since object names are not always unique, the id of an object can be used to get an object from the CValueList.

      Example.

      ``myObID=id(gameObject)``
      ``ob= scene.objects.from_id(myObID)``

      Where myObID is an int or long from the id function.

      This has the advantage that you can store the id in places you could not store a gameObject.

      .. warning:: the id is derived from a memory location and will be different each time the game engine starts.

.. class:: KX_BlenderMaterial(PyObjectPlus)

   KX_BlenderMaterial

   .. method:: getShader()

      Returns the material's shader.

      :return: the material's shader
      :rtype: :class:`BL_Shader`

   .. method:: setBlending(src, dest)

      Set the pixel color arithmetic functions.

      :arg src: Specifies how the red, green, blue, and alpha source blending factors are computed.
      :type src: Value in...

         * GL_ZERO,
         * GL_ONE, 
         * GL_SRC_COLOR, 
         * GL_ONE_MINUS_SRC_COLOR, 
         * GL_DST_COLOR, 
         * GL_ONE_MINUS_DST_COLOR, 
         * GL_SRC_ALPHA, 
         * GL_ONE_MINUS_SRC_ALPHA, 
         * GL_DST_ALPHA, 
         * GL_ONE_MINUS_DST_ALPHA, 
         * GL_SRC_ALPHA_SATURATE

      :arg dest: Specifies how the red, green, blue, and alpha destination blending factors are computed.
      :type dest: Value in...

         * GL_ZERO
         * GL_ONE
         * GL_SRC_COLOR
         * GL_ONE_MINUS_SRC_COLOR
         * GL_DST_COLOR
         * GL_ONE_MINUS_DST_COLOR
         * GL_SRC_ALPHA
         * GL_ONE_MINUS_SRC_ALPHA
         * GL_DST_ALPHA
         * GL_ONE_MINUS_DST_ALPHA
         * GL_SRC_ALPHA_SATURATE

   .. method:: getMaterialIndex()

      Returns the material's index.

      :return: the material's index
      :rtype: integer

.. class:: KX_CameraActuator(SCA_IActuator)

   Applies changes to a camera.

   .. attribute:: min

      minimum distance to the target object maintained by the actuator. **type** float

   .. attribute:: max

      maximum distance to stay from the target object. **type** float

   .. attribute:: height

      height to stay above the target object. **type** float

   .. attribute:: useXY

      axis this actuator is tracking, True=X, False=Y. **type** boolean

   .. attribute:: object

      the object this actuator tracks. **type** :class:`KX_GameObject` or None

   @author: snail

.. class:: KX_ConstraintActuator(SCA_IActuator)

   A constraint actuator limits the position, rotation, distance or orientation of an object.

   Properties:

   .. attribute:: damp

      Time constant of the constraint expressed in frame (not use by Force field constraint). **type** integer

   .. attribute:: rotDamp

      Time constant for the rotation expressed in frame (only for the distance constraint), 0 = use damp for rotation as well. **type** integer

   .. attribute:: direction

      The reference direction in world coordinate for the orientation constraint. **type** 3-tuple of float: (x, y, z)

   .. attribute:: option

      Binary combination of the following values. **type** integer

      * Applicable to Distance constraint
         * KX_ACT_CONSTRAINT_NORMAL    (  64) : Activate alignment to surface
         * KX_ACT_CONSTRAINT_DISTANCE  ( 512) : Activate distance control
         * KX_ACT_CONSTRAINT_LOCAL      (1024) : direction of the ray is along the local axis
      * Applicable to Force field constraint:
         * KX_ACT_CONSTRAINT_DOROTFH   (2048) : Force field act on rotation as well
      * Applicable to both:
         * KX_ACT_CONSTRAINT_MATERIAL  ( 128) : Detect material rather than property
         * KX_ACT_CONSTRAINT_PERMANENT ( 256) : No deactivation if ray does not hit target

   .. attribute:: time

      activation time of the actuator. The actuator disables itself after this many frame. If set to 0, the actuator is not limited in time. **type** integer

   .. attribute:: propName

      the name of the property or material for the ray detection of the distance constraint. **type** string

   .. attribute:: min

      The lower bound of the constraint. For the rotation and orientation constraint, it represents radiant **type** float

   .. attribute:: distance

      the target distance of the distance constraint **type** float

   .. attribute:: max

      the upper bound of the constraint. For rotation and orientation constraints, it represents radiant. **type** float

   .. attribute:: rayLength

      the length of the ray of the distance constraint.

      *type* float

   .. attribute:: limit

      type of constraint. **type** integer.

      use one of the following constant:

      * KX_ACT_CONSTRAINT_LOCX  ( 1) : limit X coord
      * KX_ACT_CONSTRAINT_LOCY  ( 2) : limit Y coord
      * KX_ACT_CONSTRAINT_LOCZ  ( 3) : limit Z coord
      * KX_ACT_CONSTRAINT_ROTX  ( 4) : limit X rotation
      * KX_ACT_CONSTRAINT_ROTY  ( 5) : limit Y rotation
      * KX_ACT_CONSTRAINT_ROTZ  ( 6) : limit Z rotation
      * KX_ACT_CONSTRAINT_DIRPX ( 7) : set distance along positive X axis
      * KX_ACT_CONSTRAINT_DIRPY ( 8) : set distance along positive Y axis
      * KX_ACT_CONSTRAINT_DIRPZ ( 9) : set distance along positive Z axis
      * KX_ACT_CONSTRAINT_DIRNX (10) : set distance along negative X axis
      * KX_ACT_CONSTRAINT_DIRNY (11) : set distance along negative Y axis
      * KX_ACT_CONSTRAINT_DIRNZ (12) : set distance along negative Z axis
      * KX_ACT_CONSTRAINT_ORIX  (13) : set orientation of X axis
      * KX_ACT_CONSTRAINT_ORIY  (14) : set orientation of Y axis
      * KX_ACT_CONSTRAINT_ORIZ  (15) : set orientation of Z axis
      * KX_ACT_CONSTRAINT_FHPX  (16) : set force field along positive X axis
      * KX_ACT_CONSTRAINT_FHPY  (17) : set force field along positive Y axis
      * KX_ACT_CONSTRAINT_FHPZ  (18) : set force field along positive Z axis
      * KX_ACT_CONSTRAINT_FHNX  (19) : set force field along negative X axis
      * KX_ACT_CONSTRAINT_FHNY  (20) : set force field along negative Y axis
      * KX_ACT_CONSTRAINT_FHNZ  (21) : set force field along negative Z axis

.. class:: KX_ConstraintWrapper(PyObjectPlus)

   KX_ConstraintWrapper

   .. method:: getConstraintId(val)

      Returns the contraint's ID

      :return: the constraint's ID
      :rtype: integer

.. class:: KX_GameActuator(SCA_IActuator)

   The game actuator loads a new .blend file, restarts the current .blend file or quits the game.

   Properties:

   .. attribute:: fileName

      the new .blend file to load **type** string.

   .. attribute:: mode

      The mode of this actuator **type** Constant in...

      * :mod:`bge.logic.KX_GAME_LOAD`
      * :mod:`bge.logic.KX_GAME_START`
      * :mod:`bge.logic.KX_GAME_RESTART`
      * :mod:`bge.logic.KX_GAME_QUIT`
      * :mod:`bge.logic.KX_GAME_SAVECFG`
      * :mod:`bge.logic.KX_GAME_LOADCFG`

.. class:: KX_GameObject(SCA_IObject)

   All game objects are derived from this class.

   Properties assigned to game objects are accessible as attributes of this class.

   .. note:: Calling ANY method or attribute on an object that has been removed from a scene will raise a SystemError, if an object may have been removed since last accessing it use the :data:`invalid` attribute to check.

   .. attribute:: name

      The object's name. (read-only). **type** string.

   .. attribute:: mass

      The object's mass

      .. note:: The object must have a physics controller for the mass to be applied, otherwise the mass value will be returned as 0.0 **type** float

   .. attribute:: linVelocityMin

      Enforces the object keeps moving at a minimum velocity.

      .. note:: Applies to dynamic and rigid body objects only.
      .. note:: A value of 0.0 disables this option.
      .. note:: While objects are stationary the minimum velocity will not be applied. **type** float

   .. attribute:: linVelocityMax

      Clamp the maximum linear velocity to prevent objects moving beyond a set speed.

      .. note:: Applies to dynamic and rigid body objects only.
      .. note:: A value of 0.0 disables this option (rather then setting it stationary). **type** float

   .. attribute:: localInertia

      the object's inertia vector in local coordinates. Read only. **type** list [ix, iy, iz]

   .. attribute:: parent

      The object's parent object. (read-only). **type** :class:`KX_GameObject` or None

   .. attribute:: visible

      visibility flag.

      .. note:: Game logic will still run for invisible objects. **type** boolean

   .. attribute:: color

      The object color of the object **type** list [r, g, b, a]

   .. attribute:: occlusion

      occlusion capability flag. **type** boolean

   .. attribute:: position

      The object's position.

      .. deprecated:: use :data:`localPosition` and :data:`worldPosition`. **type** list [x, y, z] On write: local position, on read: world position

   .. attribute:: orientation

      The object's orientation. 3x3 Matrix. You can also write a Quaternion or Euler vector.

      .. deprecated:: use :data:`localOrientation` and :data:`worldOrientation`. **type** 3x3 Matrix [[float]] On write: local orientation, on read: world orientation

   .. attribute:: scaling

      The object's scaling factor. list [sx, sy, sz]

      .. deprecated:: use :data:`localScale` and :data:`worldScale`. **type** list [sx, sy, sz] On write: local scaling, on read: world scaling

   .. attribute:: localOrientation

      The object's local orientation. 3x3 Matrix. You can also write a Quaternion or Euler vector. **type** 3x3 Matrix [[float]]

   .. attribute:: worldOrientation

      The object's world orientation. **type** 3x3 Matrix [[float]]

   .. attribute:: localScale

      The object's local scaling factor. **type** list [sx, sy, sz]

   .. attribute:: worldScale

      The object's world scaling factor. Read-only **type** list [sx, sy, sz]

   .. attribute:: localPosition

      The object's local position. **type** list [x, y, z]

   .. attribute:: worldPosition

      The object's world position. **type** list [x, y, z]

   .. attribute:: timeOffset

      adjust the slowparent delay at runtime. **type** float

   .. attribute:: state

      the game object's state bitmask, using the first 30 bits, one bit must always be set. **type** int

   .. attribute:: meshes

      a list meshes for this object.

      .. note:: Most objects use only 1 mesh.
      .. note:: Changes to this list will not update the KX_GameObject. **type** list of :class:`KX_MeshProxy`

   .. attribute:: sensors

      a sequence of :class:`SCA_ISensor` objects with string/index lookups and iterator support.

      .. note:: This attribute is experemental and may be removed (but probably wont be).
      .. note:: Changes to this list will not update the KX_GameObject. **type** list

   .. attribute:: controllers

      a sequence of :class:`SCA_IController` objects with string/index lookups and iterator support.
      .. note:: This attribute is experemental and may be removed (but probably wont be).
      .. note:: Changes to this list will not update the KX_GameObject. **type** list of :class:`SCA_ISensor`.

   .. attribute:: actuators

      a list of :class:`SCA_IActuator` with string/index lookups and iterator support.

      .. note:: This attribute is experemental and may be removed (but probably wont be).
      .. note:: Changes to this list will not update the KX_GameObject. **type** list

   .. attribute:: attrDict

      get the objects internal python attribute dictionary for direct (faster) access. **type** dict

   .. attribute:: children

      direct children of this object, (read-only). **type** :class:`CListValue` of :class:`KX_GameObject`'s

   .. attribute:: childrenRecursive

      all children of this object including childrens children, (read-only). **type** :class:`CListValue` of :class:`KX_GameObject`'s

   .. method:: endObject()

      Delete this object, can be used in place of the EndObject Actuator.

      The actual removal of the object from the scene is delayed.

   .. method:: replaceMesh(mesh, useDisplayMesh=True, usePhysicsMesh=False)

      Replace the mesh of this object with a new mesh. This works the same was as the actuator.

      :arg mesh: mesh to replace or the meshes name.
      :type mesh: :class:`MeshProxy` or string
      :arg useDisplayMesh: when enabled the display mesh will be replaced (optional argument).
      :type useDisplayMesh: bool
      :arg usePhysicsMesh: when enabled the physics mesh will be replaced (optional argument).
      :type usePhysicsMesh: bool

   .. method:: setVisible(visible, recursive)

      Sets the game object's visible flag.

      :arg visible: the visible state to set.
      :type visible: boolean
      :arg recursive: optional argument to set all childrens visibility flag too.
      :type recursive: boolean

   .. method:: setOcclusion(occlusion, recursive)

      Sets the game object's occlusion capability.

      :arg occlusion: the state to set the occlusion to.
      :type occlusion: boolean
      :arg recursive: optional argument to set all childrens occlusion flag too.
      :type recursive: boolean

   .. method:: alignAxisToVect(vect, axis=2, factor=1.0)

      Aligns any of the game object's axis along the given vector.


      :arg vect: a vector to align the axis.
      :type vect: 3D vector
      :arg axis: The axis you want to align

         * 0: X axis
         * 1: Y axis
         * 2: Z axis

      :type axis: integer
      :arg factor: Only rotate a feaction of the distance to the target vector (0.0 - 1.0)
      :type factor: float

   .. method:: getAxisVect(vect)

      Returns the axis vector rotates by the objects worldspace orientation.
      This is the equivalent of multiplying the vector by the orientation matrix.

      :arg vect: a vector to align the axis.
      :type vect: 3D Vector
      :return: The vector in relation to the objects rotation.
      :rtype: 3d vector.

   .. method:: applyMovement(movement, local=False)

      Sets the game object's movement.

      :arg movement: movement vector.
      :type movement: 3D Vector
      :arg local:
         * False: you get the "global" movement ie: relative to world orientation.
         * True: you get the "local" movement ie: relative to object orientation.
      :arg local: boolean

   .. method:: applyRotation(rotation, local=False)

      Sets the game object's rotation.

      :arg rotation: rotation vector.
      :type rotation: 3D Vector
      :arg local:
         * False: you get the "global" rotation ie: relative to world orientation.
         * True: you get the "local" rotation ie: relative to object orientation.
      :arg local: boolean

   .. method:: applyForce(force, local=False)

      Sets the game object's force.

      This requires a dynamic object.

      :arg force: force vector.
      :type force: 3D Vector
      :arg local:
         * False: you get the "global" force ie: relative to world orientation.
         * True: you get the "local" force ie: relative to object orientation.
      :type local: boolean

   .. method:: applyTorque(torque, local=False)

      Sets the game object's torque.

      This requires a dynamic object.

      :arg torque: torque vector.
      :type torque: 3D Vector
      :arg local:
         * False: you get the "global" torque ie: relative to world orientation.
         * True: you get the "local" torque ie: relative to object orientation.
      :type local: boolean

   .. method:: getLinearVelocity(local=False)

      Gets the game object's linear velocity.

      This method returns the game object's velocity through it's centre of mass, ie no angular velocity component.

      :arg local:
         * False: you get the "global" velocity ie: relative to world orientation.
         * True: you get the "local" velocity ie: relative to object orientation.
      :type local: boolean
      :return: the object's linear velocity.
      :rtype: list [vx, vy, vz]

   .. method:: setLinearVelocity(velocity, local=False)

      Sets the game object's linear velocity.

      This method sets game object's velocity through it's centre of mass, 
      ie no angular velocity component.

      This requires a dynamic object.

      :arg velocity: linear velocity vector.
      :type velocity: 3D Vector
      :arg local:
         * False: you get the "global" velocity ie: relative to world orientation.
         * True: you get the "local" velocity ie: relative to object orientation.
      :type local: boolean

   .. method:: getAngularVelocity(local=False)

      Gets the game object's angular velocity.

      :arg local:
         * False: you get the "global" velocity ie: relative to world orientation.
         * True: you get the "local" velocity ie: relative to object orientation.
      :type local: boolean
      :return: the object's angular velocity.
      :rtype: list [vx, vy, vz]

   .. method:: setAngularVelocity(velocity, local=False)

      Sets the game object's angular velocity.

      This requires a dynamic object.

      :arg velocity: angular velocity vector.
      :type velocity: boolean
      :arg local:
         * False: you get the "global" velocity ie: relative to world orientation.
         * True: you get the "local" velocity ie: relative to object orientation.

   .. method:: getVelocity(point=(0, 0, 0))

      Gets the game object's velocity at the specified point.

      Gets the game object's velocity at the specified point, including angular
      components.

      :arg point: optional point to return the velocity for, in local coordinates.
      :type point: 3D Vector
      :return: the velocity at the specified point.
      :rtype: list [vx, vy, vz]

   .. method:: getReactionForce()

      Gets the game object's reaction force.

      The reaction force is the force applied to this object over the last simulation timestep.
      This also includes impulses, eg from collisions.

      :return: the reaction force of this object.
      :rtype: list [fx, fy, fz]

      .. note:: This is not implimented at the moment.

   .. method:: applyImpulse(point, impulse)

      Applies an impulse to the game object.

      This will apply the specified impulse to the game object at the specified point.
      If point != position, applyImpulse will also change the object's angular momentum.
      Otherwise, only linear momentum will change.

      :arg point: the point to apply the impulse to (in world coordinates)
      :type point: the point to apply the impulse to (in world coordinates)

   .. method:: suspendDynamics()

      Suspends physics for this object.

   .. method:: restoreDynamics()

      Resumes physics for this object.

      .. note:: The objects linear velocity will be applied from when the dynamics were suspended.

   .. method:: enableRigidBody()

      Enables rigid body physics for this object.

      Rigid body physics allows the object to roll on collisions.

      .. note:: This is not working with bullet physics yet.

   .. method:: disableRigidBody()

      Disables rigid body physics for this object.

      .. note:: This is not working with bullet physics yet. The angular is removed but rigid body physics can still rotate it later.

   .. method:: setParent(parent, compound=True, ghost=True)

      Sets this object's parent.
      Control the shape status with the optional compound and ghost parameters:

      In that case you can control if it should be ghost or not:

      :arg parent: new parent object.
      :type parent: :class:`KX_GameObject`
      :arg compound: whether the shape should be added to the parent compound shape.

         * True: the object shape should be added to the parent compound shape.
         * False: the object should keep its individual shape.

      :type compound: boolean
      :arg ghost: whether the object should be ghost while parented.

         * True: if the object should be made ghost while parented.
         * False: if the object should be solid while parented.

      :type ghost: boolean

      .. note:: if the object type is sensor, it stays ghost regardless of ghost parameter

   .. method:: removeParent()

      Removes this objects parent.

   .. method:: getPhysicsId()

      Returns the user data object associated with this game object's physics controller.

   .. method:: getPropertyNames()

      Gets a list of all property names.

      :return: All property names for this object.
      :rtype: list

   .. method:: getDistanceTo(other)

      :arg other: a point or another :class:`KX_GameObject` to measure the distance to.
      :type other: :class:`KX_GameObject` or list [x, y, z]
      :return: distance to another object or point.
      :rtype: float

   .. method:: getVectTo(other)

      Returns the vector and the distance to another object or point.
      The vector is normalized unless the distance is 0, in which a zero length vector is returned.

      :arg other: a point or another :class:`KX_GameObject` to get the vector and distance to.
      :type other: :class:`KX_GameObject` or list [x, y, z]
      :return: (distance, globalVector(3), localVector(3))
      :rtype: 3-tuple (float, 3-tuple (x, y, z), 3-tuple (x, y, z))

   .. method:: rayCastTo(other, dist, prop)

      Look towards another point/object and find first object hit within dist that matches prop.

      The ray is always casted from the center of the object, ignoring the object itself.
      The ray is casted towards the center of another object or an explicit [x, y, z] point.
      Use rayCast() if you need to retrieve the hit point

      :arg other: [x, y, z] or object towards which the ray is casted
      :type other: :class:`KX_GameObject` or 3-tuple
      :arg dist: max distance to look (can be negative => look behind); 0 or omitted => detect up to other
      :type dist: float
      :arg prop: property name that object must have; can be omitted => detect any object
      :type prop: string
      :return: the first object hit or None if no object or object does not match prop
      :rtype: :class:`KX_GameObject`

   .. method:: rayCast(objto, objfrom, dist, prop, face, xray, poly)

      Look from a point/object to another point/object and find first object hit within dist that matches prop.
      if poly is 0, returns a 3-tuple with object reference, hit point and hit normal or (None, None, None) if no hit.
      if poly is 1, returns a 4-tuple with in addition a :class:`KX_PolyProxy` as 4th element.
      if poly is 2, returns a 5-tuple with in addition a 2D vector with the UV mapping of the hit point as 5th element.

      .. code-block:: python

         # shoot along the axis gun-gunAim (gunAim should be collision-free)
         obj, point, normal = gun.rayCast(gunAim, None, 50)
         if obj:
            # do something
            pass

      The face paremeter determines the orientation of the normal.

      * 0 => hit normal is always oriented towards the ray origin (as if you casted the ray from outside)
      * 1 => hit normal is the real face normal (only for mesh object, otherwise face has no effect)

      The ray has X-Ray capability if xray parameter is 1, otherwise the first object hit (other than self object) stops the ray.
      The prop and xray parameters interact as follow.

      * prop off, xray off: return closest hit or no hit if there is no object on the full extend of the ray.
      * prop off, xray on : idem.
      * prop on, xray off: return closest hit if it matches prop, no hit otherwise.
      * prop on, xray on : return closest hit matching prop or no hit if there is no object matching prop on the full extend of the ray.

      The :class:`KX_PolyProxy` 4th element of the return tuple when poly=1 allows to retrieve information on the polygon hit by the ray.
      If there is no hit or the hit object is not a static mesh, None is returned as 4th element.

      The ray ignores collision-free objects and faces that dont have the collision flag enabled, you can however use ghost objects.

      :arg objto: [x, y, z] or object to which the ray is casted
      :type objto: :class:`KX_GameObject` or 3-tuple
      :arg objfrom: [x, y, z] or object from which the ray is casted; None or omitted => use self object center
      :type objfrom: :class:`KX_GameObject` or 3-tuple or None
      :arg dist: max distance to look (can be negative => look behind); 0 or omitted => detect up to to
      :type dist: float
      :arg prop: property name that object must have; can be omitted or "" => detect any object
      :type prop: string
      :arg face: normal option: 1=>return face normal; 0 or omitted => normal is oriented towards origin
      :type face: integer
      :arg xray: X-ray option: 1=>skip objects that don't match prop; 0 or omitted => stop on first object
      :type xray: integer
      :arg poly: polygon option: 0, 1 or 2 to return a 3-, 4- or 5-tuple with information on the face hit.

         * 0 or omitted: return value is a 3-tuple (object, hitpoint, hitnormal) or (None, None, None) if no hit
         * 1: return value is a 4-tuple and the 4th element is a :class:`KX_PolyProxy` or None if no hit or the object doesn't use a mesh collision shape.
         * 2: return value is a 5-tuple and the 5th element is a 2-tuple (u, v) with the UV mapping of the hit point or None if no hit, or the object doesn't use a mesh collision shape, or doesn't have a UV mapping.

      :type poly: integer
      :return: (object, hitpoint, hitnormal) or (object, hitpoint, hitnormal, polygon) or (object, hitpoint, hitnormal, polygon, hituv).

         * object, hitpoint and hitnormal are None if no hit.
         * polygon is valid only if the object is valid and is a static object, a dynamic object using mesh collision shape or a soft body object, otherwise it is None
         * hituv is valid only if polygon is valid and the object has a UV mapping, otherwise it is None

      :rtype:

         * 3-tuple (:class:`KX_GameObject`, 3-tuple (x, y, z), 3-tuple (nx, ny, nz))
         * or 4-tuple (:class:`KX_GameObject`, 3-tuple (x, y, z), 3-tuple (nx, ny, nz), :class:`PolyProxy`)
         * or 5-tuple (:class:`KX_GameObject`, 3-tuple (x, y, z), 3-tuple (nx, ny, nz), :class:`PolyProxy`, 2-tuple (u, v))

      .. note:: The ray ignores the object on which the method is called. It is casted from/to object center or explicit [x, y, z] points.

   .. method:: setCollisionMargin(margin)

      Set the objects collision margin.

      .. note:: If this object has no physics controller (a physics ID of zero), this function will raise RuntimeError.

      :arg margin: the collision margin distance in blender units.
      :type margin: float

   .. method:: sendMessage(subject, body="", to="")

      Sends a message.

      :arg subject: The subject of the message
      :type subject: string
      :arg body: The body of the message (optional)
      :type body: string
      :arg to: The name of the object to send the message to (optional)
      :type to: string

   .. method:: reinstancePhysicsMesh(gameObject, meshObject)

      Updates the physics system with the changed mesh.

      If no arguments are given the physics mesh will be re-created from the first mesh assigned to the game object.

      :arg gameObject: optional argument, set the physics shape from this gameObjets mesh.
      :type gameObject: string, :class:`KX_GameObject` or None
      :arg meshObject: optional argument, set the physics shape from this mesh.
      :type meshObject: string, :class:`MeshProxy` or None

      .. note:: if this object has instances the other instances will be updated too.
      .. note:: the gameObject argument has an advantage that it can convert from a mesh with modifiers applied (such as subsurf).
      .. warning:: only triangle mesh type objects are supported currently (not convex hull)
      .. warning:: if the object is a part of a combound object it will fail (parent or child)
      .. warning:: rebuilding the physics mesh can be slow, running many times per second will give a performance hit.

      :return: True if reinstance succeeded, False if it failed.
      :rtype: boolean

   .. method:: get(key, default=None)

      Return the value matching key, or the default value if its not found.
      :return: The key value or a default.

.. class:: KX_IpoActuator(SCA_IActuator)

   IPO actuator activates an animation.

   .. attribute:: frameStart

      Start frame. **type** float

   .. attribute:: frameEnd

      End frame. **type** float

   .. attribute:: propName

      Use this property to define the Ipo position **type** string

   .. attribute:: framePropName

      Assign this property this action current frame number **type** string

   .. attribute:: mode

      Play mode for the ipo. (In GameLogic.KX_IPOACT_PLAY, KX_IPOACT_PINGPONG, KX_IPOACT_FLIPPER, KX_IPOACT_LOOPSTOP, KX_IPOACT_LOOPEND, KX_IPOACT_FROM_PROP) **type** integer

   .. attribute:: useIpoAsForce

      Apply Ipo as a global or local force depending on the local option (dynamic objects only) **type** bool

   .. attribute:: useIpoAdd

      Ipo is added to the current loc/rot/scale in global or local coordinate according to Local flag **type** bool

   .. attribute:: useIpoLocal

      Let the ipo acts in local coordinates, used in Force and Add mode. **type** bool

   .. attribute:: useChildren

      Update IPO on all children Objects as well **type** bool

.. class:: KX_LightObject(KX_GameObject)

   A Light object.

   .. code-block:: python

      # Turn on a red alert light.
      import bge

      co = bge.logic.getCurrentController()
      light = co.owner

      light.energy = 1.0
      light.colour = [1.0, 0.0, 0.0]

   .. data:: SPOT

      A spot light source. See attribute :data:`type`

   .. data:: SUN

      A point light source with no attenuation. See attribute :data:`type`

   .. data:: NORMAL

      A point light source. See attribute :data:`type`

   .. attribute:: type

      The type of light - must be SPOT, SUN or NORMAL

   .. attribute:: layer

      The layer mask that this light affects object on. **type** bitfield

   .. attribute:: energy

      The brightness of this light. **type** float

   .. attribute:: distance

      The maximum distance this light can illuminate. (SPOT and NORMAL lights only) **type** float

   .. attribute:: colour

      The colour of this light. Black = [0.0, 0.0, 0.0], White = [1.0, 1.0, 1.0]. **type** list [r, g, b]

   .. attribute:: color

      Synonym for colour.

   .. attribute:: lin_attenuation

      The linear component of this light's attenuation. (SPOT and NORMAL lights only) **type** float

   .. attribute:: quad_attenuation

      The quadratic component of this light's attenuation (SPOT and NORMAL lights only) **type** float

   .. attribute:: spotsize

      The cone angle of the spot light, in degrees (SPOT lights only). **type** float in [0 - 180].

   .. attribute:: spotblend

      Specifies the intensity distribution of the spot light (SPOT lights only). **type** float in [0 - 1]

      .. note:: Higher values result in a more focused light source.

.. class:: KX_MeshProxy(SCA_IObject)

   A mesh object.

   You can only change the vertex properties of a mesh object, not the mesh topology.

   To use mesh objects effectively, you should know a bit about how the game engine handles them.

   #. Mesh Objects are converted from Blender at scene load.
   #. The Converter groups polygons by Material.  This means they can be sent to the renderer efficiently.  A material holds:

      #. The texture.
      #. The Blender material.
      #. The Tile properties
      #. The face properties - (From the "Texture Face" panel)
      #. Transparency & z sorting
      #. Light layer
      #. Polygon shape (triangle/quad)
      #. Game Object

   #. Verticies will be split by face if necessary.  Verticies can only be shared between faces if:

      #. They are at the same position
      #. UV coordinates are the same
      #. Their normals are the same (both polygons are "Set Smooth")
      #. They are the same colour, for example: a cube has 24 verticies: 6 faces with 4 verticies per face.

   The correct method of iterating over every :class:`KX_VertexProxy` in a game object
   
   .. code-block:: python

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

   .. attribute:: materials

      **type** list of :class:`KX_BlenderMaterial` or :class:`KX_PolygonMaterial` types

   .. attribute:: numPolygons

      **type** integer

   .. attribute:: numMaterials

      **type** integer

   .. method:: getNumMaterials()

      :return: number of materials associated with this object
      :rtype: integer

   .. method:: getMaterialName(matid)

      Gets the name of the specified material.

      :arg matid: the specified material.
      :type matid: integer
      :return: the attached material name.
      :rtype: string

   .. method:: getTextureName(matid)

      Gets the name of the specified material's texture.

      :arg matid: the specified material
      :type matid: integer
      :return: the attached material's texture name.
      :rtype: string

   .. method:: getVertexArrayLength(matid)

      Gets the length of the vertex array associated with the specified material.

      There is one vertex array for each material.

      :arg matid: the specified material
      :type matid: integer
      :return: the number of verticies in the vertex array.
      :rtype: integer

   .. method:: getVertex(matid, index)

      Gets the specified vertex from the mesh object.

      :arg matid: the specified material
      :type matid: integer
      :arg index: the index into the vertex array.
      :type index: integer
      :return: a vertex object.
      :rtype: :class:`KX_VertexProxy`

   .. method:: getNumPolygons()

      :return: The number of polygon in the mesh.
      :rtype: integer

   .. method:: getPolygon(index)

      Gets the specified polygon from the mesh.

      :arg index: polygon number
      :type index: integer
      :return: a polygon object.
      :rtype: :class:`PolyProxy`

.. class:: SCA_MouseSensor(SCA_ISensor)

   Mouse Sensor logic brick.

   Properties:

   .. attribute:: position

      current [x, y] coordinates of the mouse, in frame coordinates (pixels) **type** [integer, interger]

   .. attribute:: mode

      sensor mode. **type** integer

         * KX_MOUSESENSORMODE_LEFTBUTTON(1)
         * KX_MOUSESENSORMODE_MIDDLEBUTTON(2)
         * KX_MOUSESENSORMODE_RIGHTBUTTON(3)
         * KX_MOUSESENSORMODE_WHEELUP(4)
         * KX_MOUSESENSORMODE_WHEELDOWN(5)
         * KX_MOUSESENSORMODE_MOVEMENT(6)

   .. method:: getButtonStatus(button)

      Get the mouse button status.

      :arg button: value in GameLogic members KX_MOUSE_BUT_LEFT, KX_MOUSE_BUT_MIDDLE, KX_MOUSE_BUT_RIGHT
      :type button: integer
      :return: value in GameLogic members KX_INPUT_NONE, KX_INPUT_NONE, KX_INPUT_JUST_ACTIVATED, KX_INPUT_ACTIVE, KX_INPUT_JUST_RELEASED
      :rtype: integer

.. class:: KX_MouseFocusSensor(SCA_MouseSensor)

   The mouse focus sensor detects when the mouse is over the current game object.

   The mouse focus sensor works by transforming the mouse coordinates from 2d device
   space to 3d space then raycasting away from the camera.

   .. attribute:: raySource

      The worldspace source of the ray (the view position) **type** list (vector of 3 floats)

   .. attribute:: rayTarget

      The worldspace target of the ray. **type** list (vector of 3 floats)

   .. attribute:: rayDirection

      The :data:`rayTarget` - :class:`raySource` normalized. **type** list (normalized vector of 3 floats)

   .. attribute:: hitObject

      the last object the mouse was over. **type** :class:`KX_GameObject` or None

   .. attribute:: hitPosition

      The worldspace position of the ray intersecton. **type** list (vector of 3 floats)

   .. attribute:: hitNormal

      the worldspace normal from the face at point of intersection. **type** list (normalized vector of 3 floats)

   .. attribute:: hitUV

      the UV coordinates at the point of intersection. **type** list (vector of 2 floats)

      If the object has no UV mapping, it returns [0, 0].

      The UV coordinates are not normalized, they can be < 0 or > 1 depending on the UV mapping.

   .. attribute:: usePulseFocus

      When enabled, moving the mouse over a different object generates a pulse. (only used when the 'Mouse Over Any' sensor option is set) **type** bool

.. class:: KX_TouchSensor(SCA_ISensor)

   Touch sensor detects collisions between objects.

   .. attribute:: propName

      The property or material to collide with. **type** string

   .. attribute:: useMaterial

      Determines if the sensor is looking for a property or material. KX_True = Find material; KX_False = Find property. **type** boolean

   .. attribute:: usePulseCollision

      When enabled, changes to the set of colliding objects generate a pulse. **type** bool

   .. attribute:: hitObject

      The last collided object. (read-only) **type** :class:`KX_GameObject` or None

   .. attribute:: hitObjectList

      A list of colliding objects. (read-only) **type** :class:`CListValue` of :class:`KX_GameObject`

.. class:: KX_NearSensor(KX_TouchSensor)

   A near sensor is a specialised form of touch sensor.

   .. attribute:: distance

      The near sensor activates when an object is within this distance. **type** float

   .. attribute:: resetDistance

      The near sensor deactivates when the object exceeds this distance. **type** float

.. class:: KX_NetworkMessageActuator(SCA_IActuator)

   Message Actuator

   .. attribute:: propName

      Messages will only be sent to objects with the given property name. **type** string

   .. attribute:: subject

      The subject field of the message. **type** string

   .. attribute:: body

      The body of the message. **type** string

   .. attribute:: usePropBody

      Send a property instead of a regular body message. **type** boolean

.. class:: KX_NetworkMessageSensor(SCA_ISensor)

   The Message Sensor logic brick.

   Currently only loopback (local) networks are supported.

   .. attribute:: subject

      The subject the sensor is looking for. **type** string

   .. attribute:: frameMessageCount

      The number of messages received since the last frame. (read-only). **type** integer

   .. attribute:: subjects

      The list of message subjects received. (read-only). **type** list of strings

   .. attribute:: bodies

      The list of message bodies received. (read-only) **type** list of strings

.. class:: KX_ObjectActuator(SCA_IActuator)

   The object actuator ("Motion Actuator") applies force, torque, displacement, angular displacement, 
   velocity, or angular velocity to an object.
   Servo control allows to regulate force to achieve a certain speed target.

   .. attribute:: force

      The force applied by the actuator **type** list [x, y, z]

   .. attribute:: useLocalForce

      A flag specifying if the force is local **type** bool

   .. attribute:: torque

      The torque applied by the actuator **type** list [x, y, z]

   .. attribute:: useLocalTorque

      A flag specifying if the torque is local **type** bool

   .. attribute:: dLoc

      The displacement vector applied by the actuator **type** list [x, y, z]

   .. attribute:: useLocalDLoc

      A flag specifying if the dLoc is local **type** bool

   .. attribute:: dRot

      The angular displacement vector applied by the actuator

      .. note:: Since the displacement is applied every frame, you must adjust the displacement based on the frame rate, or you game experience will depend on the player's computer speed. **type** list [x, y, z]

   .. attribute:: useLocalDRot

      A flag specifying if the dRot is local **type** bool

   .. attribute:: linV

      The linear velocity applied by the actuator **type** list [x, y, z]

   .. attribute:: useLocalLinV

      A flag specifying if the linear velocity is local.

      .. note:: This is the target speed for servo controllers **type** bool

   .. attribute:: angV

      The angular velocity applied by the actuator **type** list [x, y, z]

   .. attribute:: useLocalAngV

      A flag specifying if the angular velocity is local **type** bool

   .. attribute:: damping

      The damping parameter of the servo controller **type** short

   .. attribute:: forceLimitX

      The min/max force limit along the X axis and activates or deactivates the limits in the servo controller **type** list [min(float), max(float), bool]

   .. attribute:: forceLimitY

      The min/max force limit along the Y axis and activates or deactivates the limits in the servo controller **type** list [min(float), max(float), bool]

   .. attribute:: forceLimitZ

      The min/max force limit along the Z axis and activates or deactivates the limits in the servo controller **type** list [min(float), max(float), bool]

   .. attribute:: pid

      The PID coefficients of the servo controller **type** list of floats [proportional, integral, derivate]

   .. attribute:: reference

      The object that is used as reference to compute the velocity for the servo controller. **type** :class:`KX_GameObject` or None

.. class:: KX_ParentActuator(SCA_IActuator)

   The parent actuator can set or remove an objects parent object.

   .. attribute:: object

      the object this actuator sets the parent too. **type** :class:`KX_GameObject` or None

   .. attribute:: mode

      The mode of this actuator **type** integer from 0 to 1.

   .. attribute:: compound

      Whether the object shape should be added to the parent compound shape when parenting.

      Effective only if the parent is already a compound shape **type** bool

   .. attribute:: ghost

      whether the object should be made ghost when parenting
                Effective only if the shape is not added to the parent compound shape **type** bool

.. class:: KX_PhysicsObjectWrapper(PyObjectPlus)

   KX_PhysicsObjectWrapper

   .. method:: setActive(active)

      Set the object to be active.

      :arg active: set to True to be active
      :type active: bool

   .. method:: setAngularVelocity(x, y, z, local)

      Set the angular velocity of the object.

      :arg x: angular velocity for the x-axis
      :type x: float

      :arg y: angular velocity for the y-axis
      :type y: float

      :arg z: angular velocity for the z-axis
      :type z: float

      :arg local: set to True for local axis
      :type local: bool

   .. method:: setLinearVelocity(x, y, z, local)

      Set the linear velocity of the object.

      :arg x: linear velocity for the x-axis
      :type x: float

      :arg y: linear velocity for the y-axis
      :type y: float

      :arg z: linear velocity for the z-axis
      :type z: float

      :arg local: set to True for local axis
      :type local: bool

.. class:: KX_PolyProxy(SCA_IObject)

   A polygon holds the index of the vertex forming the poylgon.

   Note:
   The polygon attributes are read-only, you need to retrieve the vertex proxy if you want
   to change the vertex settings.

   .. attribute:: matname

      The name of polygon material, empty if no material. **type** string

   .. attribute:: material

      The material of the polygon **type** :class:`KX_PolygonMaterial` or :class:`KX_BlenderMaterial`

   .. attribute:: texture

      The texture name of the polygon. **type** string

   .. attribute:: matid

      The material index of the polygon, use this to retrieve vertex proxy from mesh proxy **type** integer

   .. attribute:: v1

      vertex index of the first vertex of the polygon, use this to retrieve vertex proxy from mesh proxy **type** integer

   .. attribute:: v2

      vertex index of the second vertex of the polygon, use this to retrieve vertex proxy from mesh proxy **type** integer

   .. attribute:: v3

      vertex index of the third vertex of the polygon, use this to retrieve vertex proxy from mesh proxy **type** integer

   .. attribute:: v4

      vertex index of the fourth vertex of the polygon, 0 if polygon has only 3 vertex
             use this to retrieve vertex proxy from mesh proxy **type** integer

   .. attribute:: visible

      visible state of the polygon: 1=visible, 0=invisible **type** integer

   .. attribute:: collide

      collide state of the polygon: 1=receives collision, 0=collision free. **type** integer

   .. method:: getMaterialName()

      Returns the polygon material name with MA prefix

      :return: material name
      :rtype: string

   .. method:: getMaterial()

      :return: The polygon material
      :rtype: :class:`KX_PolygonMaterial` or :class:`KX_BlenderMaterial`

   .. method:: getTextureName()

      :return: The texture name
      :rtype: string

   .. method:: getMaterialIndex()

      Returns the material bucket index of the polygon.
      This index and the ones returned by getVertexIndex() are needed to retrieve the vertex proxy from :class:`MeshProxy`.

      :return: the material index in the mesh
      :rtype: integer

   .. method:: getNumVertex()

      Returns the number of vertex of the polygon.

      :return: number of vertex, 3 or 4.
      :rtype: integer

   .. method:: isVisible()

      Returns whether the polygon is visible or not

      :return: 0=invisible, 1=visible
      :rtype: boolean

   .. method:: isCollider()

      Returns whether the polygon is receives collision or not

      :return: 0=collision free, 1=receives collision
      :rtype: integer

   .. method:: getVertexIndex(vertex)

      Returns the mesh vertex index of a polygon vertex
      This index and the one returned by getMaterialIndex() are needed to retrieve the vertex proxy from :class:`MeshProxy`.

      :arg vertex: index of the vertex in the polygon: 0->3
      :arg vertex: integer
      :return: mesh vertex index
      :rtype: integer

   .. method:: getMesh()

      Returns a mesh proxy

      :return: mesh proxy
      :rtype: :class:`MeshProxy`

.. class:: KX_PolygonMaterial

   This is the interface to materials in the game engine.

   Materials define the render state to be applied to mesh objects.

   .. warning:: Some of the methods/variables are CObjects.  If you mix these up, you will crash blender.

   This example requires

   * PyOpenGL <http://pyopengl.sourceforge.net>
   * GLEWPy <http://glewpy.sourceforge.net>

   .. code-block:: python

      import GameLogic
      import OpenGL
      from OpenGL.GL import *
      from OpenGL.GLU import *
      import glew
      from glew import *
      
      glewInit()
      
      vertex_shader = """
      
      void main(void)
      {
         gl_Position = ftransform();
      }
      """
      
      fragment_shader ="""
      
      void main(void)
      {
         gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0);
      }
      """
      
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
            """
            PrintInfoLog prints the GLSL compiler log
            """
            print "Tag:    def PrintGLError(self, tag = ""):
            
         def PrintGLError(self, tag = ""):
            """
            Prints the current GL error status
            """
            if len(tag):
               print tag
            err = glGetError()
            if err != GL_NO_ERROR:
               print "GL Error: %s\\n"%(gluErrorString(err))
      
         def shader(self, type, shaders):
            """
            shader compiles a GLSL shader and attaches it to the current
            program.
            
            type should be either GL_VERTEX_SHADER_ARB or GL_FRAGMENT_SHADER_ARB
            shaders should be a sequence of shader source to compile.
            """
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
            """
            Links the shaders together.
            """
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

   .. attribute:: texture

      Texture name **type** string (read-only)

   .. attribute:: gl_texture

      OpenGL texture handle (eg for glBindTexture(GL_TEXTURE_2D, gl_texture) **type** integer (read-only)

   .. attribute:: material

      Material name **type** string (read-only)

   .. attribute:: tface

      Texture face properties **type** CObject (read-only)

   .. attribute:: tile

      Texture is tiling **type** boolean

   .. attribute:: tilexrep

      Number of tile repetitions in x direction. **type** integer

   .. attribute:: tileyrep

      Number of tile repetitions in y direction. **type** integer

   .. attribute:: drawingmode

      Drawing mode for the material.
      - 2  (drawingmode & 4)     Textured
      - 4  (drawingmode & 16)    Light
      - 14 (drawingmode & 16384) 3d Polygon Text **type** bitfield

   .. attribute:: transparent

      This material is transparent. All meshes with this
      material will be rendered after non transparent meshes from back
      to front. **type** boolean

   .. attribute:: zsort

      Transparent polygons in meshes with this material will be sorted back to
      front before rendering.
      Non-Transparent polygons will be sorted front to back before rendering. **type** boolean

   .. attribute:: lightlayer

      Light layers this material affects. **type** bitfield.

   .. attribute:: triangle

      Mesh data with this material is triangles. It's probably not safe to change this. **type** boolean

   .. attribute:: diffuse

      The diffuse colour of the material. black = [0.0, 0.0, 0.0] white = [1.0, 1.0, 1.0] **type** list [r, g, b]

   .. attribute:: specular

      The specular colour of the material. black = [0.0, 0.0, 0.0] white = [1.0, 1.0, 1.0] **type** list [r, g, b]

   .. attribute:: shininess

      The shininess (specular exponent) of the material. 0.0 <= shininess <= 128.0 **type** float

   .. attribute:: specularity

      The amount of specular of the material. 0.0 <= specularity <= 1.0 **type** float

   .. method:: updateTexture(tface, rasty)

      Updates a realtime animation.

      :arg tface: Texture face (eg mat.tface)
      :type tface: CObject
      :arg rasty: Rasterizer
      :type rasty: CObject

   .. method:: setTexture(tface)

      Sets texture render state.

      .. code-block:: python

         mat.setTexture(mat.tface)

      :arg tface: Texture face
      :type tface: CObject

   .. method:: activate(rasty, cachingInfo)

      Sets material parameters for this object for rendering.

      Material Parameters set:

         #. Texture
         #. Backface culling
         #. Line drawing
         #. Specular Colour
         #. Shininess
         #. Diffuse Colour
         #. Polygon Offset.

      :arg rasty: Rasterizer instance.
      :type rasty: CObject
      :arg cachingInfo: Material cache instance.
      :type cachingInfo: CObject

   .. method:: setCustomMaterial(material)

      Sets the material state setup object.

      Using this method, you can extend or completely replace the gameengine material
      to do your own advanced multipass effects.

      Use this method to register your material class.  Instead of the normal material, 
      your class's activate method will be called just before rendering the mesh.
      This should setup the texture, material, and any other state you would like.
      It should return True to render the mesh, or False if you are finished.  You should
      clean up any state Blender does not set before returning False.

      Activate Method Definition::
      `def activate(self, rasty, cachingInfo, material):`

		.. code-block:: python

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

      :arg material: The material object.
      :type material: instance

.. class:: KX_RadarSensor(KX_NearSensor)

   Radar sensor is a near sensor with a conical sensor object.

   .. attribute:: coneOrigin

      The origin of the cone with which to test. The origin is in the middle of the cone. (read-only) **type** list of floats [x, y, z]

   .. attribute:: coneTarget

      The center of the bottom face of the cone with which to test. (read-only) **type** list of floats [x, y, z]

   .. attribute:: distance

      The height of the cone with which to test. **type** float

   .. attribute:: angle

      The angle of the cone (in degrees) with which to test. **type** float from 0 to 360

   .. attribute:: axis

      The axis on which the radar cone is cast **type** integer from 0 to 5

      KX_RADAR_AXIS_POS_X, KX_RADAR_AXIS_POS_Y, KX_RADAR_AXIS_POS_Z, 
      KX_RADAR_AXIS_NEG_X, KX_RADAR_AXIS_NEG_Y, KX_RADAR_AXIS_NEG_Z

   .. method:: getConeHeight()

      :return: The height of the cone with which to test.
      :rtype: float

.. class:: KX_RaySensor(SCA_ISensor)

   A ray sensor detects the first object in a given direction.

   .. attribute:: propName

      The property the ray is looking for. **type** string

   .. attribute:: range

      The distance of the ray. **type** float

   .. attribute:: useMaterial

      Whether or not to look for a material (false = property) **type** boolean

   .. attribute:: useXRay

      Whether or not to use XRay. **type** boolean

   .. attribute:: hitObject

      The game object that was hit by the ray. (read-only) **type** :class:`KX_GameObject`

   .. attribute:: hitPosition

      The position (in worldcoordinates) where the object was hit by the ray. (read-only) **type** list [x, y, z]

   .. attribute:: hitNormal

      The normal (in worldcoordinates) of the object at the location where the object was hit by the ray. (read-only) **type** list [x, y, z]

   .. attribute:: rayDirection

      The direction from the ray (in worldcoordinates). (read-only) **type** list [x, y, z]

   .. attribute:: axis

      The axis the ray is pointing on. **type** integer from 0 to 5

      * KX_RAY_AXIS_POS_X
      * KX_RAY_AXIS_POS_Y
      * KX_RAY_AXIS_POS_Z
      * KX_RAY_AXIS_NEG_X
      * KX_RAY_AXIS_NEG_Y
      * KX_RAY_AXIS_NEG_Z

.. class:: KX_SCA_AddObjectActuator(SCA_IActuator)

   Edit Object Actuator (in Add Object Mode)

   .. attribute:: object

      the object this actuator adds. **type** :class:`KX_GameObject` or None

   .. attribute:: objectLastCreated

      the last added object from this actuator (read-only). **type** :class:`KX_GameObject` or None

   .. attribute:: time

      the lifetime of added objects, in frames. Set to 0 to disable automatic deletion. **type** integer

   .. attribute:: linearVelocity

      the initial linear velocity of added objects. **type** list [vx, vy, vz]

   .. attribute:: angularVelocity

      the initial angular velocity of added objects. **type** list [vx, vy, vz]

   .. warning:: An Add Object actuator will be ignored if at game start, the linked object doesn't exist
        (or is empty) or the linked object is in an active layer.

      This will genereate a warning in the console:

      ``Error: GameObject 'Name' has a AddObjectActuator 'ActuatorName' without object (in 'nonactive' layer)``

   .. method:: instantAddObject()

      :return: The last object created by this actuator. The object can then be accessed from :data:`objectLastCreated`.
      :rtype: None

.. class:: KX_SCA_DynamicActuator(SCA_IActuator)

   Dynamic Actuator.

   .. attribute:: mode

      **type** integer

      the type of operation of the actuator, 0-4

      * KX_DYN_RESTORE_DYNAMICS(0)
      * KX_DYN_DISABLE_DYNAMICS(1)
      * KX_DYN_ENABLE_RIGID_BODY(2)
      * KX_DYN_DISABLE_RIGID_BODY(3)
      * KX_DYN_SET_MASS(4)

   .. attribute:: mass

      the mass value for the KX_DYN_SET_MASS operation **type** float

.. class:: KX_SCA_EndObjectActuator(SCA_IActuator)

   Edit Object Actuator (in End Object mode)

   This actuator has no python methods.

.. class:: KX_SCA_ReplaceMeshActuator(SCA_IActuator)

   Edit Object actuator, in Replace Mesh mode.

   .. code-block:: python

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

   .. warning:: Replace mesh actuators will be ignored if at game start, the named mesh doesn't exist.

      This will generate a warning in the console

      ``Error: GameObject 'Name' ReplaceMeshActuator 'ActuatorName' without object``

   .. attribute:: mesh

      :class:`MeshProxy` or the name of the mesh that will replace the current one.
   
      Set to None to disable actuator **type** :class:`MeshProxy` or None if no mesh is set

   .. attribute:: useDisplayMesh

      when true the displayed mesh is replaced. **type** boolean

   .. attribute:: usePhysicsMesh

      when true the physics mesh is replaced. **type** boolean

   .. method:: instantReplaceMesh()

      Immediately replace mesh without delay.

.. class:: KX_Scene(PyObjectPlus)

   An active scene that gives access to objects, cameras, lights and scene attributes.

   The activity culling stuff is supposed to disable logic bricks when their owner gets too far
   from the active camera.  It was taken from some code lurking at the back of KX_Scene - who knows
   what it does!

   .. code-block:: python

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

   .. code-block:: python

      # Get the depth of an object in the camera view.
      import GameLogic

      obj = GameLogic.getCurrentController().owner
      cam = GameLogic.getCurrentScene().active_camera

      # Depth is negative and decreasing further from the camera
      depth = obj.position[0]*cam.world_to_camera[2][0] + obj.position[1]*cam.world_to_camera[2][1] + obj.position[2]*cam.world_to_camera[2][2] + cam.world_to_camera[2][3]

   @bug: All attributes are read only at the moment.

   .. attribute:: name

      The scene's name, (read-only). **type** string

   .. attribute:: objects

      A list of objects in the scene, (read-only). **type** :class:`CListValue` of :class:`KX_GameObject`

   .. attribute:: objectsInactive

      A list of objects on background layers (used for the addObject actuator), (read-only). **type** :class:`CListValue` of :class:`KX_GameObject`

   .. attribute:: lights

      A list of lights in the scene, (read-only). **type** :class:`CListValue` of :class:`KX_LightObject`

   .. attribute:: cameras

      A list of cameras in the scene, (read-only). **type** :class:`CListValue` of :class:`KX_Camera`

   .. attribute:: active_camera

      The current active camera.

      .. note:: this can be set directly from python to avoid using the :class:`KX_SceneActuator`. **type** :class:`KX_Camera`

   .. attribute:: suspended

      True if the scene is suspended, (read-only). **type** boolean

   .. attribute:: activity_culling

      True if the scene is activity culling **type** boolean

   .. attribute:: activity_culling_radius

      The distance outside which to do activity culling. Measured in manhattan distance. **type** float

   .. attribute:: dbvt_culling

      True when Dynamic Bounding box Volume Tree is set (read-only). **type** bool

   .. attribute:: pre_draw

      A list of callables to be run before the render step. **type** list

   .. attribute:: post_draw

      A list of callables to be run after the render step. **type** list

   .. method:: addObject(object, other, time=0)

      Adds an object to the scene like the Add Object Actuator would.

      :arg object: The object to add
      :type object: :class:`KX_GameObject` or string
      :arg other: The object's center to use when adding the object
      :type other: :class:`KX_GameObject` or string
      :arg time: The lifetime of the added object, in frames. A time of 0 means the object will last forever.
      :type time: integer
      :return: The newly added object.
      :rtype: :class:`KX_GameObject`

   .. method:: end()

      Removes the scene from the game.

   .. method:: restart()

      Restarts the scene.

   .. method:: replace(scene)

      Replaces this scene with another one.

      :arg scene: The name of the scene to replace this scene with.
      :type scene: string

   .. method:: suspend()

      Suspends this scene.

   .. method:: resume()

      Resume this scene.

   .. method:: get(key, default=None)

      Return the value matching key, or the default value if its not found.
      :return: The key value or a default.

.. class:: KX_SceneActuator(SCA_IActuator)

   Scene Actuator logic brick.

   .. warning:: Scene actuators that use a scene name will be ignored if at game start, the named scene doesn't exist or is empty

      This will generate a warning in the console:

        ``Error: GameObject 'Name' has a SceneActuator 'ActuatorName' (SetScene) without scene``

   .. attribute:: scene

      the name of the scene to change to/overlay/underlay/remove/suspend/resume **type** string.

   .. attribute:: camera

      the camera to change to.

      .. note:: When setting the attribute, you can use either a :class:`KX_Camera` or the name of the camera. **type** :class:`KX_Camera` on read, string or :class:`KX_Camera` on write

   .. attribute:: useRestart

      Set flag to True to restart the sene **type** bool

   .. attribute:: mode

      The mode of the actuator **type** integer from 0 to 5.

.. class:: KX_SoundActuator(SCA_IActuator)

   Sound Actuator.

   The :data:`startSound`, :data:`pauseSound` and :data:`stopSound` do not requirethe actuator to be activated - they act instantly provided that the actuator has been activated once at least.

   .. attribute:: fileName

      The filename of the sound this actuator plays. **type** string

   .. attribute:: volume

      The volume (gain) of the sound. **type** float

   .. attribute:: pitch

      The pitch of the sound. **type** float

   .. attribute:: rollOffFactor

      The roll off factor. Rolloff defines the rate of attenuation as the sound gets further away. **type** float

   .. attribute:: looping

      The loop mode of the actuator. **type** integer

   .. attribute:: position

      The position of the sound as a list: [x, y, z]. **type** float array

   .. attribute:: velocity

      The velocity of the emitter as a list: [x, y, z]. The relative velocity to the observer determines the pitch. List of 3 floats: [x, y, z]. **type** float array

   .. attribute:: orientation

      The orientation of the sound. When setting the orientation you can also use quaternion [float, float, float, float] or euler angles [float, float, float] **type** 3x3 matrix [[float]]

   .. attribute:: mode

      The operation mode of the actuator. **type** integer
      
      You can use one of the following constants:
      * KX_SOUNDACT_PLAYSTOP               (1)
      * KX_SOUNDACT_PLAYEND                (2)
      * KX_SOUNDACT_LOOPSTOP               (3)
      * KX_SOUNDACT_LOOPEND                (4)
      * KX_SOUNDACT_LOOPBIDIRECTIONAL      (5)
      * KX_SOUNDACT_LOOPBIDIRECTIONAL_STOP (6) 

.. class:: KX_StateActuator(SCA_IActuator)

   State actuator changes the state mask of parent object.

   Property:

   .. attribute:: operation

      type of bit operation to be applied on object state mask.

      You can use one of the following constant:

      * KX_STATE_OP_CPY (0) : Copy state mask
      * KX_STATE_OP_SET (1) : Add bits to state mask
      * KX_STATE_OP_CLR (2) : Substract bits to state mask
      * KX_STATE_OP_NEG (3) : Invert bits to state mask **type** integer

   .. attribute:: mask

      value that defines the bits that will be modified by the operation.
           The bits that are 1 in the mask will be updated in the object state, 
            the bits that are 0 are will be left unmodified expect for the Copy operation
            which copies the mask to the object state **type** integer

.. class:: KX_TrackToActuator(SCA_IActuator)

   Edit Object actuator in Track To mode.

   .. warning:: Track To Actuators will be ignored if at game start, the
      object to track to is invalid.

      This will generate a warning in the console:

      ``Error: GameObject 'Name' no object in EditObjectActuator 'ActuatorName'``

   .. attribute:: object

      the object this actuator tracks. **type** :class:`KX_GameObject` or None

   .. attribute:: time

      the time in frames with which to delay the tracking motion **type** integer

   .. attribute:: use3D

      the tracking motion to use 3D **type** boolean

.. class:: KX_VehicleWrapper(PyObjectPlus)

   KX_VehicleWrapper

   TODO - description

   .. method:: addWheel(wheel, attachPos, attachDir, axleDir, suspensionRestLength, wheelRadius, hasSteering)

      Add a wheel to the vehicle

      :arg wheel: The object to use as a wheel.
      :type wheel: :class:`KX_GameObject` or a KX_GameObject name
      :arg attachPos: The position that this wheel will attach to.
      :type attachPos: vector of 3 floats
      :arg attachDir: The direction this wheel points.
      :type attachDir: vector of 3 floats
      :arg axleDir: The direction of this wheels axle.
      :type axleDir: vector of 3 floats
      :arg suspensionRestLength: TODO - Description
      :type suspensionRestLength: float
      :arg wheelRadius: The size of the wheel.
      :type wheelRadius: float

   .. method:: applyBraking(force, wheelIndex)

      Apply a braking force to the specified wheel

      :arg force: the brake force
      :type force: float

      :arg wheelIndex: index of the wheel where the force needs to be applied
      :type wheelIndex: integer

   .. method:: applyEngineForce(force, wheelIndex)

      Apply an engine force to the specified wheel

      :arg force: the engine force
      :type force: float

      :arg wheelIndex: index of the wheel where the force needs to be applied
      :type wheelIndex: integer

   .. method:: getConstraintId()

      Get the constraint ID

      :return: the constraint id
      :rtype: integer

   .. method:: getConstraintType()

      Returns the constraint type.

      :return: constraint type
      :rtype: integer

   .. method:: getNumWheels()

      Returns the number of wheels.

      :return: the number of wheels for this vehicle
      :rtype: integer

   .. method:: getWheelOrientationQuaternion(wheelIndex)

      Returns the wheel orientation as a quaternion.

      :arg wheelIndex: the wheel index
      :type wheelIndex: integer

      :return: TODO Description
      :rtype: TODO - type should be quat as per method name but from the code it looks like a matrix

   .. method:: getWheelPosition(wheelIndex)

      Returns the position of the specified wheel

      :arg wheelIndex: the wheel index
      :type wheelIndex: integer
      :return: position vector
      :rtype: list[x, y, z]

   .. method:: getWheelRotation(wheelIndex)

      Returns the rotation of the specified wheel

      :arg wheelIndex: the wheel index
      :type wheelIndex: integer

      :return: the wheel rotation
      :rtype: float

   .. method:: setRollInfluence(rollInfluece, wheelIndex)

      Set the specified wheel's roll influence.
      The higher the roll influence the more the vehicle will tend to roll over in corners.

      :arg rollInfluece: the wheel roll influence
      :type rollInfluece: float

      :arg wheelIndex: the wheel index
      :type wheelIndex: integer

   .. method:: setSteeringValue(steering, wheelIndex)

      Set the specified wheel's steering

      :arg steering: the wheel steering
      :type steering: float

      :arg wheelIndex: the wheel index
      :type wheelIndex: integer

   .. method:: setSuspensionCompression(compression, wheelIndex)

      Set the specified wheel's compression

      :arg compression: the wheel compression
      :type compression: float

      :arg wheelIndex: the wheel index
      :type wheelIndex: integer

   .. method:: setSuspensionDamping(damping, wheelIndex)

      Set the specified wheel's damping

      :arg damping: the wheel damping
      :type damping: float

      :arg wheelIndex: the wheel index
      :type wheelIndex: integer

   .. method:: setSuspensionStiffness(stiffness, wheelIndex)

      Set the specified wheel's stiffness

      :arg stiffness: the wheel stiffness
      :type stiffness: float

      :arg wheelIndex: the wheel index
      :type wheelIndex: integer

   .. method:: setTyreFriction(friction, wheelIndex)

      Set the specified wheel's tyre friction

      :arg friction: the tyre friction
      :type friction: float

      :arg wheelIndex: the wheel index
      :type wheelIndex: integer

.. class:: KX_VertexProxy(SCA_IObject)

   A vertex holds position, UV, colour and normal information.

   Note:
   The physics simulation is NOT currently updated - physics will not respond
   to changes in the vertex position.

   .. attribute:: XYZ

      The position of the vertex. **type** list [x, y, z]

   .. attribute:: UV

      The texture coordinates of the vertex. **type** list [u, v]

   .. attribute:: normal

      The normal of the vertex **type** list [nx, ny, nz]

   .. attribute:: colour

      The colour of the vertex. **type** list [r, g, b, a]

      Black = [0.0, 0.0, 0.0, 1.0], White = [1.0, 1.0, 1.0, 1.0]

   .. attribute:: color

      Synonym for colour.

   .. attribute:: x

      The x coordinate of the vertex. **type** float

   .. attribute:: y

      The y coordinate of the vertex. **type** float

   .. attribute:: z

      The z coordinate of the vertex. **type** float

   .. attribute:: u

      The u texture coordinate of the vertex. **type** float

   .. attribute:: v

      The v texture coordinate of the vertex. **type** float

   .. attribute:: u2

      The second u texture coordinate of the vertex. **type** float

   .. attribute:: v2

      The second v texture coordinate of the vertex. **type** float

   .. attribute:: r

      The red component of the vertex colour. 0.0 <= r <= 1.0 **type** float

   .. attribute:: g

      The green component of the vertex colour. 0.0 <= g <= 1.0 **type** float

   .. attribute:: b

      The blue component of the vertex colour. 0.0 <= b <= 1.0 **type** float

   .. attribute:: a

      The alpha component of the vertex colour. 0.0 <= a <= 1.0 **type** float

   .. method:: getXYZ()

      Gets the position of this vertex.

      :return: this vertexes position in local coordinates.
      :rtype: list [x, y, z]

   .. method:: setXYZ(pos)

      Sets the position of this vertex.

         **type** list [x, y, z]

      :arg pos: the new position for this vertex in local coordinates.

   .. method:: getUV()

      Gets the UV (texture) coordinates of this vertex.

      :return: this vertexes UV (texture) coordinates.
      :rtype: list [u, v]

   .. method:: setUV(uv)

      Sets the UV (texture) coordinates of this vertex.

         **type** list [u, v]

   .. method:: getUV2()

      Gets the 2nd UV (texture) coordinates of this vertex.

      :return: this vertexes UV (texture) coordinates.
      :rtype: list [u, v]

   .. method:: setUV2(uv, unit)

      Sets the 2nd UV (texture) coordinates of this vertex.

         **type** list [u, v]

      :arg unit: optional argument, FLAT==1, SECOND_UV==2, defaults to SECOND_UV
      :arg unit:  integer

   .. method:: getRGBA()

      Gets the colour of this vertex.

      The colour is represented as four bytes packed into an integer value.  The colour is
      packed as RGBA.

      Since Python offers no way to get each byte without shifting, you must use the struct module to
      access colour in an machine independent way.

      Because of this, it is suggested you use the r, g, b and a attributes or the colour attribute instead.

      .. code-block:: python

         import struct;
         col = struct.unpack('4B', struct.pack('I', v.getRGBA()))
         # col = (r, g, b, a)
         # black = (  0, 0, 0, 255)
         # white = (255, 255, 255, 255)

      :return: packed colour. 4 byte integer with one byte per colour channel in RGBA format.
      :rtype: integer

   .. method:: setRGBA(col)

      Sets the colour of this vertex.

      See getRGBA() for the format of col, and its relevant problems.  Use the r, g, b and a attributes
      or the colour attribute instead.

      setRGBA() also accepts a four component list as argument col.  The list represents the colour as [r, g, b, a]
      with black = [0.0, 0.0, 0.0, 1.0] and white = [1.0, 1.0, 1.0, 1.0]

      .. code-block:: python

         v.setRGBA(0xff0000ff) # Red
         v.setRGBA(0xff00ff00) # Green on little endian, transparent purple on big endian
         v.setRGBA([1.0, 0.0, 0.0, 1.0]) # Red
         v.setRGBA([0.0, 1.0, 0.0, 1.0]) # Green on all platforms.

      :arg col: the new colour of this vertex in packed RGBA format.
      :type col: integer or list [r, g, b, a]

   .. method:: getNormal()

      Gets the normal vector of this vertex.

      :return: normalised normal vector.
      :rtype: list [nx, ny, nz]

   .. method:: setNormal(normal)

      Sets the normal vector of this vertex.

         **type** sequence of floats [r, g, b]

      :arg normal: the new normal of this vertex.

.. class:: KX_VisibilityActuator(SCA_IActuator)

   Visibility Actuator.

   .. attribute:: visibility

      whether the actuator makes its parent object visible or invisible **type** boolean

   .. attribute:: useOcclusion

      whether the actuator makes its parent object an occluder or not **type** boolean

   .. attribute:: useRecursion

      whether the visibility/occlusion should be propagated to all children of the object **type** boolean

.. class:: SCA_2DFilterActuator(SCA_IActuator)

   Create, enable and disable 2D filters

   Properties:

   The following properties don't have an immediate effect.
   You must active the actuator to get the result.
   The actuator is not persistent: it automatically stops itself after setting up the filter
   but the filter remains active. To stop a filter you must activate the actuator with 'type'
   set to RAS_2DFILTER_DISABLED or RAS_2DFILTER_NOFILTER.

   .. attribute:: shaderText

      shader source code for custom shader **type** string

   .. attribute:: disableMotionBlur

      action on motion blur: 0=enable, 1=disable **type** integer

   .. attribute:: mode

      type of 2D filter, use one of the following constants:

      * RAS_2DFILTER_ENABLED      (-2) : enable the filter that was previously disabled
      * RAS_2DFILTER_DISABLED     (-1) : disable the filter that is currently active
      * RAS_2DFILTER_NOFILTER      (0) : disable and destroy the filter that is currently active
      * RAS_2DFILTER_MOTIONBLUR    (1) : create and enable preset filters
      * RAS_2DFILTER_BLUR          (2)
      * RAS_2DFILTER_SHARPEN       (3)
      * RAS_2DFILTER_DILATION      (4)
      * RAS_2DFILTER_EROSION       (5)
      * RAS_2DFILTER_LAPLACIAN     (6)
      * RAS_2DFILTER_SOBEL         (7)
      * RAS_2DFILTER_PREWITT       (8)
      * RAS_2DFILTER_GRAYSCALE     (9)
      * RAS_2DFILTER_SEPIA        (10)
      * RAS_2DFILTER_INVERT       (11)
      * RAS_2DFILTER_CUSTOMFILTER (12) : customer filter, the code code is set via shaderText property **type** integer

   .. attribute:: passNumber

      order number of filter in the stack of 2D filters. Filters are executed in increasing order of passNb.

      Only be one filter can be defined per passNb. **type** integer (0-100)

   .. attribute:: value

      argument for motion blur filter **type** float (0.0-100.0)

.. class:: SCA_ANDController(SCA_IController)

   An AND controller activates only when all linked sensors are activated.

   There are no special python methods for this controller.

.. class:: SCA_ActuatorSensor(SCA_ISensor)

   Actuator sensor detect change in actuator state of the parent object.
   It generates a positive pulse if the corresponding actuator is activated
   and a negative pulse if the actuator is deactivated.

   Properties:

   .. attribute:: actuator

      the name of the actuator that the sensor is monitoring. **type** string

.. class:: SCA_AlwaysSensor(SCA_ISensor)

   This sensor is always activated.

.. class:: SCA_DelaySensor(SCA_ISensor)

   The Delay sensor generates positive and negative triggers at precise time, 
   expressed in number of frames. The delay parameter defines the length of the initial OFF period. A positive trigger is generated at the end of this period.

   The duration parameter defines the length of the ON period following the OFF period.
   There is a negative trigger at the end of the ON period. If duration is 0, the sensor stays ON and there is no negative trigger.

   The sensor runs the OFF-ON cycle once unless the repeat option is set: the OFF-ON cycle repeats indefinately (or the OFF cycle if duration is 0).

   Use :class:`SCA_ISensor.reset` at any time to restart sensor.

   Properties:

   .. attribute:: delay

      length of the initial OFF period as number of frame, 0 for immediate trigger. **type** integer.

   .. attribute:: duration

      length of the ON period in number of frame after the initial OFF period.

      If duration is greater than 0, a negative trigger is sent at the end of the ON pulse. **type** integer

   .. attribute:: repeat

      1 if the OFF-ON cycle should be repeated indefinately, 0 if it should run once. **type** integer

.. class:: SCA_JoystickSensor(SCA_ISensor)

   This sensor detects player joystick events.

   Properties:

   .. attribute:: axisValues

      The state of the joysticks axis as a list of values :data:`numAxis` long. (read-only). **type** list of ints.

      Each spesifying the value of an axis between -32767 and 32767 depending on how far the axis is pushed, 0 for nothing.
      The first 2 values are used by most joysticks and gamepads for directional control. 3rd and 4th values are only on some joysticks and can be used for arbitary controls.

      * left:[-32767, 0, ...]
      * right:[32767, 0, ...]
      * up:[0, -32767, ...]
      * down:[0, 32767, ...]

   .. attribute:: axisSingle

      like :data:`axisValues` but returns a single axis value that is set by the sensor. (read-only). **type** integer

      .. note:: only use this for "Single Axis" type sensors otherwise it will raise an error.

   .. attribute:: hatValues

      The state of the joysticks hats as a list of values :data:`numHats` long. (read-only) **type** list of ints

      Each spesifying the direction of the hat from 1 to 12, 0 when inactive.

      Hat directions are as follows...

      * 0:None
      * 1:Up
      * 2:Right
      * 4:Down
      * 8:Left
      * 3:Up - Right
      * 6:Down - Right
      * 12:Down - Left
      * 9:Up - Left

   .. attribute:: hatSingle

      Like :data:`hatValues` but returns a single hat direction value that is set by the sensor. (read-only). **type** integer

   .. attribute:: numAxis

      The number of axes for the joystick at this index. (read-only). **type** integer

   .. attribute:: numButtons

      The number of buttons for the joystick at this index. (read-only). **type** integer

   .. attribute:: numHats

      The number of hats for the joystick at this index. (read-only). **type** integer

   .. attribute:: connected

      True if a joystick is connected at this joysticks index. (read-only). **type** boolean

   .. attribute:: index

      The joystick index to use (from 0 to 7). The first joystick is always 0. **type** integer

   .. attribute:: threshold

      Axis threshold. Joystick axis motion below this threshold wont trigger an event. Use values between (0 and 32767), lower values are more sensitive. **type** integer

   .. attribute:: button

      The button index the sensor reacts to (first button = 0). When the "All Events" toggle is set, this option has no effect. **type** integer

   .. attribute:: axis

      The axis this sensor reacts to, as a list of two values [axisIndex, axisDirection]

      * axisIndex: the axis index to use when detecting axis movement, 1=primary directional control, 2=secondary directional control.
      * axisDirection: 0=right, 1=up, 2=left, 3=down. **type** [integer, integer]

   .. attribute:: hat

      The hat the sensor reacts to, as a list of two values: [hatIndex, hatDirection]

      * hatIndex: the hat index to use when detecting hat movement, 1=primary hat, 2=secondary hat (4 max).
      * hatDirection: 1-12 **type** [integer, integer]

   .. method:: getButtonActiveList()

      :return: A list containing the indicies of the currently pressed buttons.
      :rtype: list

   .. method:: getButtonStatus(buttonIndex)

      :arg buttonIndex: the button index, 0=first button
      :type buttonIndex: integer
      :return: The current pressed state of the specified button.
      :rtype: boolean

.. class:: SCA_KeyboardSensor(SCA_ISensor)

   A keyboard sensor detects player key presses.

   See module :mod:`bge.keys` for keycode values.

   .. attribute:: key

      The key code this sensor is looking for. **type** keycode from :mod:`bge.keys` module

   .. attribute:: hold1

      The key code for the first modifier this sensor is looking for. **type** keycode from :mod:`bge.keys` module

   .. attribute:: hold2

      The key code for the second modifier this sensor is looking for. **type** keycode from :mod:`bge.keys` module

   .. attribute:: toggleProperty

      The name of the property that indicates whether or not to log keystrokes as a string. **type** string

   .. attribute:: targetProperty

      The name of the property that receives keystrokes in case in case a string is logged. **type** string

   .. attribute:: useAllKeys

      Flag to determine whether or not to accept all keys. **type** boolean

   .. attribute:: events

      a list of pressed keys that have either been pressed, or just released, or are active this frame. (read-only). **type** list [[keycode, status], ...]

      * 'keycode' matches the values in :mod:`bge.keys`.
      * 'status' uses...

         * :mod:`bge.logic.KX_INPUT_NONE`
         * :mod:`bge.logic.KX_INPUT_JUST_ACTIVATED`
         * :mod:`bge.logic.KX_INPUT_ACTIVE`
         * :mod:`bge.logic.KX_INPUT_JUST_RELEASED`

   .. method:: getKeyStatus(keycode)

      Get the status of a key.

      :arg keycode: The code that represents the key you want to get the state of
      :type keycode: integer
      :return: The state of the given key
      :rtype: key state :mod:`bge.logic` members (KX_INPUT_NONE, KX_INPUT_JUST_ACTIVATED, KX_INPUT_ACTIVE, KX_INPUT_JUST_RELEASED)

.. class:: SCA_NANDController(SCA_IController)

   An NAND controller activates when all linked sensors are not active.

   There are no special python methods for this controller.

.. class:: SCA_NORController(SCA_IController)

   An NOR controller activates only when all linked sensors are de-activated.

   There are no special python methods for this controller.

.. class:: SCA_ORController(SCA_IController)

   An OR controller activates when any connected sensor activates.

   There are no special python methods for this controller.

.. class:: SCA_PropertyActuator(SCA_IActuator)

   Property Actuator

   Properties:

   .. attribute:: propName

      the property on which to operate. **type** string

   .. attribute:: value

      the value with which the actuator operates. **type** string

   .. attribute:: mode

      TODO - add constants to game logic dict!. **type** integer

.. class:: SCA_PropertySensor(SCA_ISensor)

   Activates when the game object property matches.

   Properties:

   .. attribute:: mode

      Type of check on the property. **type** integer

      * KX_PROPSENSOR_EQUAL(1)
      * KX_PROPSENSOR_NOTEQUAL(2)
      * KX_PROPSENSOR_INTERVAL(3)
      * KX_PROPSENSOR_CHANGED(4)
      * KX_PROPSENSOR_EXPRESSION(5) 

   .. attribute:: propName

      the property the sensor operates. **type** string

   .. attribute:: value

      the value with which the sensor compares to the value of the property. **type** string

   .. attribute:: min

      the minimum value of the range used to evaluate the property when in interval mode. **type** string

   .. attribute:: max

      the maximum value of the range used to evaluate the property when in interval mode. **type** string

.. class:: SCA_PythonController(SCA_IController)

   A Python controller uses a Python script to activate it's actuators, 
   based on it's sensors.

   Properties:

   .. attribute:: script

      The value of this variable depends on the execution methid.

      * When 'Script' execution mode is set this value contains the entire python script as a single string (not the script name as you might expect) which can be modified to run different scripts.
      * When 'Module' execution mode is set this value will contain a single line string - module name and function "module.func" or "package.modile.func" where the module names are python textblocks or external scripts.

      .. note:: once this is set the script name given for warnings will remain unchanged. **type** string

   .. attribute:: mode

      the execution mode for this controller (read-only).

      * Script: 0, Execite the :data:`script` as a python code.
      * Module: 1, Execite the :data:`script` as a module and function. **type** integer

   .. method:: activate(actuator)

      Activates an actuator attached to this controller.

      :arg actuator: The actuator to operate on.
      :type actuator: actuator or the actuator name as a string

   .. method:: deactivate(actuator)

      Deactivates an actuator attached to this controller.

      :arg actuator: The actuator to operate on.
      :type actuator: actuator or the actuator name as a string

.. class:: SCA_RandomActuator(SCA_IActuator)

   Random Actuator

   Properties:

   .. attribute:: seed

      Seed of the random number generator. **type** integer.

      Equal seeds produce equal series. If the seed is 0, the generator will produce the same value on every call.

   .. attribute:: para1

      the first parameter of the active distribution. **type** float, read-only.

      Refer to the documentation of the generator types for the meaning of this value. 

   .. attribute:: para2

      the second parameter of the active distribution. **type** float, read-only

      Refer to the documentation of the generator types for the meaning of this value.

   .. attribute:: distribution

      distribution type. (read-only). **type** integer

      * KX_RANDOMACT_BOOL_CONST
      * KX_RANDOMACT_BOOL_UNIFORM
      * KX_RANDOMACT_BOOL_BERNOUILLI
      * KX_RANDOMACT_INT_CONST
      * KX_RANDOMACT_INT_UNIFORM
      * KX_RANDOMACT_INT_POISSON
      * KX_RANDOMACT_FLOAT_CONST
      * KX_RANDOMACT_FLOAT_UNIFORM
      * KX_RANDOMACT_FLOAT_NORMAL
      * KX_RANDOMACT_FLOAT_NEGATIVE_EXPONENTIAL

   .. attribute:: propName

      the name of the property to set with the random value. **type** string

      If the generator and property types do not match, the assignment is ignored.

   .. method:: setBoolConst(value)

      Sets this generator to produce a constant boolean value.

      :arg value: The value to return.
      :type value: boolean

   .. method:: setBoolUniform()

      Sets this generator to produce a uniform boolean distribution.

      The generator will generate True or False with 50% chance.

   .. method:: setBoolBernouilli(value)

      Sets this generator to produce a Bernouilli distribution.

      :arg value: Specifies the proportion of False values to produce.

         * 0.0: Always generate True
         * 1.0: Always generate False
      :type value: float

   .. method:: setIntConst(value)

      Sets this generator to always produce the given value.

      :arg value: the value this generator produces.
      :type value: integer

   .. method:: setIntUniform(lower_bound, upper_bound)

      Sets this generator to produce a random value between the given lower and
      upper bounds (inclusive).

      :type lower_bound: integer
      :type upper_bound: integer

   .. method:: setIntPoisson(value)

      Generate a Poisson-distributed number.

      This performs a series of Bernouilli tests with parameter value.
      It returns the number of tries needed to achieve succes.

      :type value: float

   .. method:: setFloatConst(value)

      Always generate the given value.

      :type value: float

   .. method:: setFloatUniform(lower_bound, upper_bound)

      Generates a random float between lower_bound and upper_bound with a
      uniform distribution.

      :type lower_bound: float
      :type upper_bound: float

   .. method:: setFloatNormal(mean, standard_deviation)

      Generates a random float from the given normal distribution.

      :arg mean: The mean (average) value of the generated numbers
      :type mean: float
      :arg standard_deviation: The standard deviation of the generated numbers.
      :type standard_deviation: float

   .. method:: setFloatNegativeExponential(half_life)

      Generate negative-exponentially distributed numbers.

      The half-life 'time' is characterized by half_life.
      
      :type half_life: float

.. class:: SCA_RandomSensor(SCA_ISensor)

   This sensor activates randomly.

   .. attribute:: lastDraw

      The seed of the random number generator. **type** integer

   .. attribute:: seed

      The seed of the random number generator. **type** integer

   .. method:: setSeed(seed)

      Sets the seed of the random number generator.

      If the seed is 0, the generator will produce the same value on every call.

      :type seed: integer

   .. method:: getSeed()

      :return: The initial seed of the generator.  Equal seeds produce equal random series.
      :rtype: integer

   .. method:: getLastDraw()

      :return: The last random number generated.
      :rtype: integer

.. class:: SCA_XNORController(SCA_IController)

   An XNOR controller activates when all linked sensors are the same (activated or inative).

   There are no special python methods for this controller.

.. class:: SCA_XORController(SCA_IController)

   An XOR controller activates when there is the input is mixed, but not when all are on or off.

   There are no special python methods for this controller.

.. class:: KX_Camera(KX_GameObject)

   A Camera object.

   .. attribute:: INSIDE

      see :data:`sphereInsideFrustum` and :data:`boxInsideFrustum`

   .. attribute:: INTERSECT

      see :data:`sphereInsideFrustum` and :data:`boxInsideFrustum`

   .. attribute:: OUTSIDE

      see :data:`sphereInsideFrustum` and :data:`boxInsideFrustum`

   .. attribute:: lens

      The camera's lens value. **type** float

   .. attribute:: near

      The camera's near clip distance. **type** float

   .. attribute:: far

      The camera's far clip distance. **type** float

   .. attribute:: perspective

      True if this camera has a perspective transform, False for an orthographic projection. **type** boolean

   .. attribute:: frustum_culling

      True if this camera is frustum culling. **type** boolean

   .. attribute:: projection_matrix

      This camera's 4x4 projection matrix. **type** 4x4 Matrix [[float]]

   .. attribute:: modelview_matrix

      This camera's 4x4 model view matrix. (read-only). **type** 4x4 Matrix [[float]]

      .. note:: This matrix is regenerated every frame from the camera's position and orientation. 

   .. attribute:: camera_to_world

      This camera's camera to world transform. (read-only). **type** 4x4 Matrix [[float]]

      .. note:: This matrix is regenerated every frame from the camera's position and orientation.

   .. attribute:: world_to_camera

      This camera's world to camera transform. (read-only). **type** 4x4 Matrix [[float]]

      .. note:: Regenerated every frame from the camera's position and orientation.
      .. note:: This is camera_to_world inverted.

   .. attribute:: useViewport

      True when the camera is used as a viewport, set True to enable a viewport for this camera. **type** boolean

   .. method:: sphereInsideFrustum(centre, radius)

      Tests the given sphere against the view frustum.

      :arg centre: The centre of the sphere (in world coordinates.)
      :type centre: list [x, y, z]
      :arg radius: the radius of the sphere
      :type radius: float
      :return: INSIDE, OUTSIDE or INTERSECT
      :rtype: integer

      .. code-block:: python

			import GameLogic
			co = GameLogic.getCurrentController()
			cam = co.owner
			
			# A sphere of radius 4.0 located at [x, y, z] = [1.0, 1.0, 1.0]
			if (cam.sphereInsideFrustum([1.0, 1.0, 1.0], 4) != cam.OUTSIDE):
				# Sphere is inside frustum !
				# Do something useful !
			else:
				# Sphere is outside frustum

      .. note:: when the camera is first initialized the result will be invalid because the projection matrix has not been set.

   .. method:: boxInsideFrustum(box)

      Tests the given box against the view frustum.

      .. code-block:: python

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

      :arg box: Eight (8) corner points of the box (in world coordinates.)
      :type box: list of lists
      :return: INSIDE, OUTSIDE or INTERSECT

      .. note:: when the camera is first initialized the result will be invalid because the projection matrix has not been set.

   .. method:: pointInsideFrustum(point)

      Tests the given point against the view frustum.

      .. code-block:: python

			import GameLogic
			co = GameLogic.getCurrentController()
			cam = co.owner
	
			# Test point [0.0, 0.0, 0.0]
			if (cam.pointInsideFrustum([0.0, 0.0, 0.0])):
				# Point is inside frustum !
				# Do something useful !
			else:
				# Box is outside the frustum !

      :arg point: The point to test (in world coordinates.)
      :type point: 3D Vector
      :return: True if the given point is inside this camera's viewing frustum.
      :rtype: boolean

      .. note:: when the camera is first initialized the result will be invalid because the projection matrix has not been set.

   .. method:: getCameraToWorld()

      Returns the camera-to-world transform.

      :return: the camera-to-world transform matrix.
      :rtype: matrix (4x4 list)

   .. method:: getWorldToCamera()

      Returns the world-to-camera transform.

      This returns the inverse matrix of getCameraToWorld().

      :return: the world-to-camera transform matrix.
      :rtype: matrix (4x4 list)

   .. method:: setOnTop()

      Set this cameras viewport ontop of all other viewport.

   .. method:: setViewport(left, bottom, right, top)

      Sets the region of this viewport on the screen in pixels.

      Use :data:`bge.render.getWindowHeight` and :data:`bge.render.getWindowWidth` to calculate values relative to the entire display.

      :arg left: left pixel coordinate of this viewport
      :type left: integer
      :arg bottom: bottom pixel coordinate of this viewport
      :type bottom: integer
      :arg right: right pixel coordinate of this viewport
      :type right: integer
      :arg top: top pixel coordinate of this viewport
      :type top: integer

   .. method:: getScreenPosition(object)

      Gets the position of an object projected on screen space.

      .. code-block:: python

         # For an object in the middle of the screen, coord = [0.5, 0.5]
         coord = camera.getScreenPosition(object)

      :arg object: object name or list [x, y, z]
      :type object: :class:`KX_GameObject` or 3D Vector
      :return: the object's position in screen coordinates.
      :rtype: list [x, y]

   .. method:: getScreenVect(x, y)

      Gets the vector from the camera position in the screen coordinate direction.

      .. code-block:: python

         # Gets the vector of the camera front direction:
         m_vect = camera.getScreenVect(0.5, 0.5)


      :arg x: X Axis
      :type x: float
      :arg y: Y Axis
      :type y: float
      :rtype: 3D Vector
      :return: The vector from screen coordinate.

   .. method:: getScreenRay(x, y, dist=inf, property=None)

      Look towards a screen coordinate (x, y) and find first object hit within dist that matches prop.
      The ray is similar to KX_GameObject->rayCastTo.

      .. code-block:: python

         # Gets an object with a property "wall" in front of the camera within a distance of 100:
         target = camera.getScreenRay(0.5, 0.5, 100, "wall")

      :arg x: X Axis
      :type x: float
      :arg y: Y Axis
      :type y: float
      :arg dist: max distance to look (can be negative => look behind); 0 or omitted => detect up to other
      :type dist: float
      :arg property: property name that object must have; can be omitted => detect any object
      :type property: string
      :rtype: :class:`KX_GameObject`
      :return: the first object hit or None if no object or object does not match prop

.. class:: BL_ArmatureObject(KX_GameObject)

   An armature object.

   .. attribute:: constraints

      The list of armature constraint defined on this armature.
      Elements of the list can be accessed by index or string.
      The key format for string access is '<bone_name>:<constraint_name>' **type** list of :class:`BL_ArmatureConstraint`

   .. attribute:: channels

      The list of armature channels.
      Elements of the list can be accessed by index or name the bone. **type** list of :class:`BL_ArmatureChannel`

   .. method:: update()

      Ensures that the armature will be updated on next graphic frame.

      This action is unecessary if a KX_ArmatureActuator with mode run is active
      or if an action is playing. Use this function in other cases. It must be called
      on each frame to ensure that the armature is updated continously.

.. class:: BL_ArmatureActuator(SCA_IActuator)

   Armature Actuators change constraint condition on armatures.

   .. attribute:: KX_ACT_ARMATURE_RUN

      see type

   .. attribute:: KX_ACT_ARMATURE_ENABLE

      see type

   .. attribute:: KX_ACT_ARMATURE_DISABLE

      see type

   .. attribute:: KX_ACT_ARMATURE_SETTARGET

      see type

   .. attribute:: KX_ACT_ARMATURE_SETWEIGHT

      see type

    .. attribute:: type

      The type of action that the actuator executes when it is active.

      * KX_ACT_ARMATURE_RUN(0) just make sure the armature will be updated on the next graphic frame. This is the only persistent mode of the actuator: it executes automatically once per frame until stopped by a controller
      * KX_ACT_ARMATURE_ENABLE(1) enable the constraint.
      * KX_ACT_ARMATURE_DISABLE(2) disable the constraint (runtime constraint values are not updated).
      * KX_ACT_ARMATURE_SETTARGET(3) change target and subtarget of constraint.
      * KX_ACT_ARMATURE_SETWEIGHT(4) change weight of (only for IK constraint). **type** integer

   .. attribute:: constraint

      The constraint object this actuator is controlling. **type** :class:`BL_ArmatureConstraint`

   .. attribute:: target

      The object that this actuator will set as primary target to the constraint it controls **type** :class:`KX_GameObject`

   .. attribute:: subtarget

      The object that this actuator will set as secondary target to the constraint it controls. **type** :class:`KX_GameObject`.
      
      .. note:: Currently, the only secondary target is the pole target for IK constraint.

   .. attribute:: weight

      The weight this actuator will set on the constraint it controls. **type** float.

      .. note:: Currently only the IK constraint has a weight. It must be a value between 0 and 1.

      .. note:: A weight of 0 disables a constraint while still updating constraint runtime values (see :class:`BL_ArmatureConstraint`)

.. class:: KX_ArmatureSensor(SCA_ISensor)

   Armature sensor detect conditions on armatures.

   See :data:`type`

   .. data:: KX_ARMSENSOR_STATE_CHANGED
   .. data:: KX_ARMSENSOR_LIN_ERROR_BELOW
   .. data:: KX_ARMSENSOR_LIN_ERROR_ABOVE
   .. data:: KX_ARMSENSOR_ROT_ERROR_BELOW
   .. data:: KX_ARMSENSOR_ROT_ERROR_ABOVE

   .. attribute:: type

      The type of measurement that the sensor make when it is active. **type** integer.

      * KX_ARMSENSOR_STATE_CHANGED(0) detect that the constraint is changing state (active/inactive)
      * KX_ARMSENSOR_LIN_ERROR_BELOW(1) detect that the constraint linear error is above a threshold
      * KX_ARMSENSOR_LIN_ERROR_ABOVE(2) detect that the constraint linear error is below a threshold
      * KX_ARMSENSOR_ROT_ERROR_BELOW(3) detect that the constraint rotation error is above a threshold
      * KX_ARMSENSOR_ROT_ERROR_ABOVE(4) detect that the constraint rotation error is below a threshold

   .. attribute:: constraint

      The constraint object this sensor is watching. **type** :class:`BL_ArmatureConstraint`

   .. attribute:: value
   
      **type** float

      The threshold used in the comparison with the constraint error
      The linear error is only updated on CopyPose/Distance IK constraint with iTaSC solver
      The rotation error is only updated on CopyPose+rotation IK constraint with iTaSC solver
      The linear error on CopyPose is always >= 0: it is the norm of the distance between the target and the bone
      The rotation error on CopyPose is always >= 0: it is the norm of the equivalent rotation vector between the bone and the target orientations
      The linear error on Distance can be positive if the distance between the bone and the target is greater than the desired distance, and negative if the distance is smaller.

.. class:: BL_ArmatureConstraint(PyObjectPlus)

   Proxy to Armature Constraint. Allows to change constraint on the fly.
   Obtained through :class:`BL_ArmatureObject`.constraints.

   .. note:: not all armature constraints are supported in the GE.


   Constants related to see :data:`type`

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


   Constants related to see :data:`ik_type`

   .. data:: CONSTRAINT_IK_COPYPOSE
   .. data:: CONSTRAINT_IK_DISTANCE
   .. data:: CONSTRAINT_IK_MODE_INSIDE
   .. data:: CONSTRAINT_IK_MODE_OUTSIDE
   .. data:: CONSTRAINT_IK_MODE_ONSURFACE
   .. data:: CONSTRAINT_IK_FLAG_TIP
   .. data:: CONSTRAINT_IK_FLAG_ROT
   .. data:: CONSTRAINT_IK_FLAG_STRETCH
   .. data:: CONSTRAINT_IK_FLAG_POS

   .. attribute:: type

      Type of constraint, (read-only) **type** integer, one of CONSTRAINT_TYPE_* constants

   .. attribute:: name

      Name of constraint constructed as <bone_name>:<constraint_name>. constraints list **type** string

      This name is also the key subscript on :class:`BL_ArmatureObject`.

   .. attribute:: enforce

      fraction of constraint effect that is enforced. Between 0 and 1. **type** float

   .. attribute:: headtail

      Position of target between head and tail of the target bone: 0=head, 1=tail. **type** float.

      .. note:: Only used if the target is a bone (i.e target object is an armature.

   .. attribute:: lin_error

      runtime linear error (in Blender units) on constraint at the current frame.

      This is a runtime value updated on each frame by the IK solver. Only available on IK constraint and iTaSC solver. **type** float

   .. attribute:: rot_error

      Runtime rotation error (in radiant) on constraint at the current frame. **type** float.

      This is a runtime value updated on each frame by the IK solver. Only available on IK constraint and iTaSC solver.

      It is only set if the constraint has a rotation part, for example, a CopyPose+Rotation IK constraint.

   .. attribute:: target

      Primary target object for the constraint. The position of this object in the GE will be used as target for the constraint. **type** :class:`KX_GameObject`.

   .. attribute:: subtarget

      Secondary target object for the constraint. The position of this object in the GE will be used as secondary target for the constraint. **type** :class:`KX_GameObject`.

      Currently this is only used for pole target on IK constraint.

   .. attribute:: active

      True if the constraint is active.

      .. note:: an inactive constraint does not update lin_error and rot_error. **type** boolean

   .. attribute:: ik_weight

      Weight of the IK constraint between 0 and 1.

      Only defined for IK constraint. **type** float

   .. attribute:: ik_type

      Type of IK constraint, (read-only). **type** integer.

      * CONSTRAINT_IK_COPYPOSE(0) constraint is trying to match the position and eventually the rotation of the target.
      * CONSTRAINT_IK_DISTANCE(1) constraint is maintaining a certain distance to target subject to ik_mode

   .. attribute:: ik_flag

      Combination of IK constraint option flags, read-only

      * CONSTRAINT_IK_FLAG_TIP(1) : set when the constraint operates on the head of the bone and not the tail
      * CONSTRAINT_IK_FLAG_ROT(2) : set when the constraint tries to match the orientation of the target
      * CONSTRAINT_IK_FLAG_STRETCH(16) : set when the armature is allowed to stretch (only the bones with stretch factor > 0.0)
      * CONSTRAINT_IK_FLAG_POS(32) : set when the constraint tries to match the position of the target **type** integer

   .. attribute:: ik_dist

      Distance the constraint is trying to maintain with target, only used when ik_type=CONSTRAINT_IK_DISTANCE **type** float

   .. attribute:: ik_mode

      Additional mode for IK constraint. Currently only used for Distance constraint:

      * CONSTRAINT_IK_MODE_INSIDE(0) : the constraint tries to keep the bone within ik_dist of target
      * CONSTRAINT_IK_MODE_OUTSIDE(1) : the constraint tries to keep the bone outside ik_dist of the target
      * CONSTRAINT_IK_MODE_ONSURFACE(2) : the constraint tries to keep the bone exactly at ik_dist of the target **type** integer

.. class:: BL_ArmatureChannel(PyObjectPlus)

   Proxy to armature pose channel. Allows to read and set armature pose.
   The attributes are identical to RNA attributes, but mostly in read-only mode.

   See :data:`rotation_mode`

   .. data:: PCHAN_ROT_QUAT
   .. data:: PCHAN_ROT_XYZ
   .. data:: PCHAN_ROT_XZY
   .. data:: PCHAN_ROT_YXZ
   .. data:: PCHAN_ROT_YZX
   .. data:: PCHAN_ROT_ZXY
   .. data:: PCHAN_ROT_ZYX

   .. attribute:: name

      channel name (=bone name), read-only. **type** string

   .. attribute:: bone

      return the bone object corresponding to this pose channel, read-only. **type** :class:`BL_ArmatureBone`

   .. attribute:: parent

      return the parent channel object, None if root channel, read-only. **type** :class:`BL_ArmatureChannel`

   .. attribute:: has_ik

      true if the bone is part of an active IK chain, read-only.
      This flag is not set when an IK constraint is defined but not enabled (miss target information for example) **type** boolean

   .. attribute:: ik_dof_x

      true if the bone is free to rotation in the X axis, read-only. **type** boolean

   .. attribute:: ik_dof_y

      true if the bone is free to rotation in the Y axis, read-only. **type** boolean

   .. attribute:: ik_dof_z

      true if the bone is free to rotation in the Z axis, read-only. **type** boolean

   .. attribute:: ik_limit_x

      true if a limit is imposed on X rotation, read-only. **type** boolean

   .. attribute:: ik_limit_y

      true if a limit is imposed on Y rotation, read-only. **type** boolean

   .. attribute:: ik_limit_z

      true if a limit is imposed on Z rotation, read-only. **type** boolean

   .. attribute:: ik_rot_control

      true if channel rotation should applied as IK constraint, read-only. **type** boolean

   .. attribute:: ik_lin_control

      true if channel size should applied as IK constraint, read-only. **type** boolean

   .. attribute:: location

      displacement of the bone head in armature local space, read-write. **type** vector [X, Y, Z].

      .. note:: You can only move a bone if it is unconnected to its parent. An action playing on the armature may change the value. An IK chain does not update this value, see joint_rotation.
      .. note:: Changing this field has no immediate effect, the pose is updated when the armature is updated during the graphic render (see :data:`BL_ArmatureObject.update`).

   .. attribute:: scale

      scale of the bone relative to its parent, read-write. **type** vector [sizeX, sizeY, sizeZ].

      .. note:: An action playing on the armature may change the value.  An IK chain does not update this value, see joint_rotation.
      .. note:: Changing this field has no immediate effect, the pose is updated when the armature is updated during the graphic render (see :data:`BL_ArmatureObject.update`)

   .. attribute:: rotation

      rotation of the bone relative to its parent expressed as a quaternion, read-write. **type** vector [qr, qi, qj, qk].

      .. note:: This field is only used if rotation_mode is 0. An action playing on the armature may change the value.  An IK chain does not update this value, see joint_rotation.
      .. note:: Changing this field has no immediate effect, the pose is updated when the armature is updated during the graphic render (see :data:`BL_ArmatureObject.update`)

   .. attribute:: euler_rotation

      rotation of the bone relative to its parent expressed as a set of euler angles, read-write. **type** vector [X, Y, Z].

      .. note:: This field is only used if rotation_mode is > 0. You must always pass the angles in [X, Y, Z] order; the order of applying the angles to the bone depends on rotation_mode. An action playing on the armature may change this field.  An IK chain does not update this value, see joint_rotation.
      .. note:: Changing this field has no immediate effect, the pose is updated when the armature is updated during the graphic render (see :data:`BL_ArmatureObject.update`)

   .. attribute:: rotation_mode

      Method of updating the bone rotation, read-write. **type** integer

      Use the following constants (euler mode are named as in Blender UI but the actual axis order is reversed).

      * PCHAN_ROT_QUAT(0) : use quaternioin in rotation attribute to update bone rotation
      * PCHAN_ROT_XYZ(1) : use euler_rotation and apply angles on bone's Z, Y, X axis successively
      * PCHAN_ROT_XZY(2) : use euler_rotation and apply angles on bone's Y, Z, X axis successively
      * PCHAN_ROT_YXZ(3) : use euler_rotation and apply angles on bone's Z, X, Y axis successively
      * PCHAN_ROT_YZX(4) : use euler_rotation and apply angles on bone's X, Z, Y axis successively
      * PCHAN_ROT_ZXY(5) : use euler_rotation and apply angles on bone's Y, X, Z axis successively
      * PCHAN_ROT_ZYX(6) : use euler_rotation and apply angles on bone's X, Y, Z axis successively

   .. attribute:: channel_matrix

      pose matrix in bone space (deformation of the bone due to action, constraint, etc), Read-only.
      This field is updated after the graphic render, it represents the current pose. **type** matrix [4][4]

   .. attribute:: pose_matrix

      pose matrix in armature space, read-only, 
      This field is updated after the graphic render, it represents the current pose. **type** matrix [4][4]

   .. attribute:: pose_head

      position of bone head in armature space, read-only. **type** vector [x, y, z]

   .. attribute:: pose_tail

      position of bone tail in armature space, read-only. **type** vector [x, y, z]

   .. attribute:: ik_min_x

      minimum value of X rotation in degree (<= 0) when X rotation is limited (see ik_limit_x), read-only. **type** float

   .. attribute:: ik_max_x

      maximum value of X rotation in degree (>= 0) when X rotation is limited (see ik_limit_x), read-only. **type** float

   .. attribute:: ik_min_y

      minimum value of Y rotation in degree (<= 0) when Y rotation is limited (see ik_limit_y), read-only. **type** float

   .. attribute:: ik_max_y

      maximum value of Y rotation in degree (>= 0) when Y rotation is limited (see ik_limit_y), read-only. **type** float

   .. attribute:: ik_min_z

      minimum value of Z rotation in degree (<= 0) when Z rotation is limited (see ik_limit_z), read-only. **type** float

   .. attribute:: ik_max_z

      maximum value of Z rotation in degree (>= 0) when Z rotation is limited (see ik_limit_z), read-only. **type** float

   .. attribute:: ik_stiffness_x

      bone rotation stiffness in X axis, read-only **type** float between 0 and 1

   .. attribute:: ik_stiffness_y

      bone rotation stiffness in Y axis, read-only **type** float between 0 and 1

   .. attribute:: ik_stiffness_z

      bone rotation stiffness in Z axis, read-only **type** float between 0 and 1

   .. attribute:: ik_stretch

      ratio of scale change that is allowed, 0=bone can't change size, read-only. **type** float

   .. attribute:: ik_rot_weight

      weight of rotation constraint when ik_rot_control is set, read-write. **type** float between 0 and 1

   .. attribute:: ik_lin_weight

      weight of size constraint when ik_lin_control is set, read-write. **type** float between 0 and 1

   .. attribute:: joint_rotation

      Control bone rotation in term of joint angle (for robotic applications), read-write. **type** vector [x, y, z]

      When writing to this attribute, you pass a [x, y, z] vector and an appropriate set of euler angles or quaternion is calculated according to the rotation_mode.

      When you read this attribute, the current pose matrix is converted into a [x, y, z] vector representing the joint angles.

      The value and the meaning of the x, y, z depends on the ik_dof_x/ik_dof_y/ik_dof_z attributes:

      * 1DoF joint X, Y or Z: the corresponding x, y, or z value is used an a joint angle in radiant
      * 2DoF joint X+Y or Z+Y: treated as 2 successive 1DoF joints: first X or Z, then Y. The x or z value is used as a joint angle in radiant along the X or Z axis, followed by a rotation along the new Y axis of y radiants.
      * 2DoF joint X+Z: treated as a 2DoF joint with rotation axis on the X/Z plane. The x and z values are used as the coordinates of the rotation vector in the X/Z plane.
      * 3DoF joint X+Y+Z: treated as a revolute joint. The [x, y, z] vector represents the equivalent rotation vector to bring the joint from the rest pose to the new pose.

      .. note:: The bone must be part of an IK chain if you want to set the ik_dof_x/ik_dof_y/ik_dof_z attributes via the UI, but this will interfere with this attribute since the IK solver will overwrite the pose. You can stay in control of the armature if you create an IK constraint but do not finalize it (e.g. don't set a target) the IK solver will not run but the IK panel will show up on the UI for each bone in the chain.
      .. note:: [0, 0, 0] always corresponds to the rest pose.
      .. note:: You must request the armature pose to update and wait for the next graphic frame to see the effect of setting this attribute (see :data:`BL_ArmatureObject.update`).
      .. note:: You can read the result of the calculation in rotation or euler_rotation attributes after setting this attribute.

.. class:: BL_ArmatureBone(PyObjectPlus)

   Proxy to Blender bone structure. All fields are read-only and comply to RNA names.
   All space attribute correspond to the rest pose.

   .. attribute:: name

      bone name **type** string

   .. attribute:: connected

      true when the bone head is struck to the parent's tail **type** boolean

   .. attribute:: hinge

      true when bone doesn't inherit rotation or scale from parent bone **type** boolean

   .. attribute:: inherit_scale

      true when bone inherits scaling from parent bone **type** boolean

   .. attribute:: bbone_segments

      number of B-bone segments **type** integer

   .. attribute:: roll

      bone rotation around head-tail axis **type** float

   .. attribute:: head

      location of head end of the bone in parent bone space **type** vector [x, y, z]

   .. attribute:: tail

      location of head end of the bone in parent bone space **type** vector [x, y, z]

   .. attribute:: length

      bone length **type** float

   .. attribute:: arm_head

      location of head end of the bone in armature space **type** vector [x, y, z]

   .. attribute:: arm_tail

      location of tail end of the bone in armature space **type** vector [x, y, z]

   .. attribute:: arm_mat

      matrix of the bone head in armature space **type** matrix [4][4]

      .. note:: This matrix has no scale part. 

   .. attribute:: bone_mat

      rotation matrix of the bone in parent bone space. **type** matrix [3][3]

   .. attribute:: parent

      parent bone, or None for root bone **type** :class:`BL_ArmatureBone`

   .. attribute:: children

      list of bone's children. **type** list of :class:`BL_ArmatureBone`
