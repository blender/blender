
Physics Constraints (bge.constraints)
=====================================

.. module:: bge.constraints

.. literalinclude:: ../examples/bge.constraints.py
   :language: rest
   :lines: 2-4

.. literalinclude:: ../examples/bge.constraints.py
   :lines: 6-

.. function:: createConstraint(physicsid, physicsid2, constrainttype, [pivotX, pivotY, pivotZ, [axisX, axisY, axisZ, [flag]]]])

   Creates a constraint.

   :arg physicsid: the physics id of the first object in constraint
   :type physicsid: int

   :arg physicsid2: the physics id of the second object in constraint
   :type physicsid2: int

   :arg constrainttype: the type of the constraint. The constraint types are:

   - :class:`POINTTOPOINT_CONSTRAINT`
   - :class:`LINEHINGE_CONSTRAINT`
   - :class:`ANGULAR_CONSTRAINT`
   - :class:`CONETWIST_CONSTRAINT`
   - :class:`VEHICLE_CONSTRAINT`

   :type constrainttype: int

   :arg pivotX: pivot X position
   :type pivotX: float

   :arg pivotY: pivot Y position
   :type pivotY: float

   :arg pivotZ: pivot Z position
   :type pivotZ: float

   :arg axisX: X axis
   :type axisX: float

   :arg axisY: Y axis
   :type axisY: float

   :arg axisZ: Z axis
   :type axisZ: float

   :arg flag: .. to do
   :type flag: int

.. attribute:: error

   Simbolic constant string that indicates error.

.. function:: exportBulletFile(filename)

   export a .bullet file

   :arg filename: File name
   :type filename: string

.. function:: getAppliedImpulse(constraintId)

   :arg constraintId: The id of the constraint.
   :type constraintId: int

   :return: the most recent applied impulse.
   :rtype: float

.. function:: getVehicleConstraint(constraintId)

   :arg constraintId: The id of the vehicle constraint.
   :type constraintId: int

   :return: a vehicle constraint object.
   :rtype: :class:`bge.types.KX_VehicleWrapper`

.. function:: getCharacter(gameobj)

   :arg gameobj: The game object with the character physics.
   :type gameobj: :class:`bge.types.KX_GameObject`

   :return: character wrapper
   :rtype: :class:`bge.types.KX_CharacterWrapper`

.. function:: removeConstraint(constraintId)

   Removes a constraint.

   :arg constraintId: The id of the constraint to be removed.
   :type constraintId: int

.. function:: setCcdMode(ccdMode)

   .. note::
      Very experimental, not recommended

   Sets the CCD (Continous Colision Detection) mode in the Physics Environment.

   :arg ccdMode: The new CCD mode.
   :type ccdMode: int

.. function:: setContactBreakingTreshold(breakingTreshold)

   .. note::
      Reasonable default is 0.02 (if units are meters)

   Sets tresholds to do with contact point management.

   :arg breakingTreshold: The new contact breaking treshold.
   :type breakingTreshold: float

.. function:: setDeactivationAngularTreshold(angularTreshold)

   Sets the angular velocity treshold.

   :arg angularTreshold: New deactivation angular treshold.
   :type angularTreshold: float

.. function:: setDeactivationLinearTreshold(linearTreshold)

   Sets the linear velocity treshold.

   :arg linearTreshold: New deactivation linear treshold.
   :type linearTreshold: float

.. function:: setDeactivationTime(time)

   Sets the time after which a resting rigidbody gets deactived.

   :arg time: The deactivation time.
   :type time: float

.. function:: setDebugMode(mode)

   Sets the debug mode.

   Debug modes:
      - :class:`DBG_NODEBUG`
      - :class:`DBG_DRAWWIREFRAME`
      - :class:`DBG_DRAWAABB`
      - :class:`DBG_DRAWFREATURESTEXT`
      - :class:`DBG_DRAWCONTACTPOINTS`
      - :class:`DBG_NOHELPTEXT`
      - :class:`DBG_DRAWTEXT`
      - :class:`DBG_PROFILETIMINGS`
      - :class:`DBG_ENABLESATCOMPARISION`
      - :class:`DBG_DISABLEBULLETLCP`
      - :class:`DBG_ENABLECCD`
      - :class:`DBG_DRAWCONSTRAINTS`
      - :class:`DBG_DRAWCONSTRAINTLIMITS`
      - :class:`DBG_FASTWIREFRAME`

   :arg mode: The new debug mode.
   :type mode: int

.. function:: setGravity(x, y, z)

   Sets the gravity force.

   :arg x: Gravity X force.
   :type x: float

   :arg y: Gravity Y force.
   :type y: float

   :arg z: Gravity Z force.
   :type z: float

.. function:: setLinearAirDamping(damping)

   .. note::
      Not implemented.

   Sets the linear air damping for rigidbodies.

.. function:: setNumIterations(numiter)

   Sets the number of iterations for an iterative constraint solver.

   :arg numiter: New number of iterations.
   :type numiter: int

.. function:: setNumTimeSubSteps(numsubstep)

   Sets the number of substeps for each physics proceed. Tradeoff quality for performance.

   :arg numsubstep: New number of substeps.
   :type numsubstep: int

.. function:: setSolverDamping(damping)

   .. note::
      Very experimental, not recommended

   Sets the damper constant of a penalty based solver.

   :arg damping: New damping for the solver.
   :type damping: float

.. function:: setSolverTau(tau)

   .. note::
      Very experimental, not recommended

   Sets the spring constant of a penalty based solver.

   :arg tau: New tau for the solver.
   :type tau: float

.. function:: setSolverType(solverType)

   .. note::
      Very experimental, not recommended

   Sets the solver type.

   :arg solverType: The new type of the solver.
   :type solverType: int

.. function:: setSorConstant(sor)

   .. note::
      Very experimental, not recommended

   Sets the successive overrelaxation constant.

   :arg sor: New sor value.
   :type sor: float

.. function:: setUseEpa(epa)

   Not implemented.

.. data:: DBG_NODEBUG

   .. note::
      Debug mode to be used with function :class:`setDebugMode`

   No debug.

.. data:: DBG_DRAWWIREFRAME

   .. note::
      Debug mode to be used with function :class:`setDebugMode`

   Draw wireframe in debug.

.. data:: DBG_DRAWAABB

   .. note::
      Debug mode to be used with function :class:`setDebugMode`

   Draw Axis Aligned Bounding Box in debug.

.. data:: DBG_DRAWFREATURESTEXT

   .. note::
      Debug mode to be used with function :class:`setDebugMode`

   Draw freatures text in debug.

.. data:: DBG_DRAWCONTACTPOINTS

   .. note::
      Debug mode to be used with function :class:`setDebugMode`

   Draw contact points in debug.

.. data:: DBG_NOHELPTEXT

   .. note::
      Debug mode to be used with function :class:`setDebugMode`

   Debug without help text.

.. data:: DBG_DRAWTEXT

   .. note::
      Debug mode to be used with function :class:`setDebugMode`

   Draw text in debug.

.. data:: DBG_PROFILETIMINGS

   .. note::
      Debug mode to be used with function :class:`setDebugMode`

   Draw profile timings in debug.

.. data:: DBG_ENABLESATCOMPARISION

   .. note::
      Debug mode to be used with function :class:`setDebugMode`

   Enable sat comparision in debug.

.. data:: DBG_DISABLEBULLETLCP

   .. note::
      Debug mode to be used with function :class:`setDebugMode`

   Disable Bullet LCP.

.. data:: DBG_ENABLECCD

   .. note::
      Debug mode to be used with function :class:`setDebugMode`

   Enable Continous Colision Detection in debug.

.. data:: DBG_DRAWCONSTRAINTS

   .. note::
      Debug mode to be used with function :class:`setDebugMode`

   Draw constraints in debug.

.. data:: DBG_DRAWCONSTRAINTLIMITS

   .. note::
      Debug mode to be used with function :class:`setDebugMode`

   Draw constraint limits in debug.

.. data:: DBG_FASTWIREFRAME

   .. note::
      Debug mode to be used with function :class:`setDebugMode`

   Draw a fast wireframe in debug.

.. data:: POINTTOPOINT_CONSTRAINT

   .. note::
      Constraint type to be used with function :class:`createConstraint`

   .. to do

.. data:: LINEHINGE_CONSTRAINT

   .. note::
      Constraint type to be used with function :class:`createConstraint`

   .. to do

.. data:: ANGULAR_CONSTRAINT

   .. note::
      Constraint type to be used with function :class:`createConstraint`

   .. to do

.. data:: CONETWIST_CONSTRAINT

   .. note::
       Constraint type to be used with function :class:`createConstraint`

   .. to do

.. data:: VEHICLE_CONSTRAINT

   .. note::
      Constraint type to be used with function :class:`createConstraint`

   .. to do
