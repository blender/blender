KX_VehicleWrapper(PyObjectPlus)
===============================

.. module:: bge.types

base class --- :class:`PyObjectPlus`

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

