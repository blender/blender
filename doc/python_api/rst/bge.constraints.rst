
Game Engine bge.constraints Module
==================================

.. note::
   This documentation is still very weak, and needs some help!

.. function:: createConstraint([obj1, [obj2, [restLength, [restitution, [damping]]]]])

   Creates a constraint.

   :arg obj1: first object on Constraint
   :type obj1: :class:'bge.types.KX_GameObject' #I think, there is no error when I use one

   :arg obj2: second object on Constraint
   :type obj2: :class:'bge.types.KX_GameObject' #too

   :arg restLength: #to be filled
   :type restLength: float

   :arg restitution: #to be filled
   :type restitution: float

   :arg damping: #to be filled
   :type damping: float

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
   :rtype: :class:'KX_VehicleWrapper'

.. function:: removeConstraint(constraintId)

   Removes a constraint.

   :arg constraintId: The id of the constraint to be removed.
   :type constraintId: int

.. function:: setCcdMode(ccdMode)

   ..note::
      Very experimental, not recommended

   Sets the CCD mode in the Physics Environment.

   :arg ccdMode: The new CCD mode.
   :type ccdMode: int

.. function:: setContactBreakingTreshold(breakingTreshold)

   .. note::
      Reasonable default is 0.02 (if units are meters)

   Sets the contact breaking treshold in the Physics Environment.

   :arg breakingTreshold: The new contact breaking treshold.
   :type breakingTreshold: float

.. function:: setDeactivationAngularTreshold(angularTreshold)

   Sets the deactivation angular treshold.

   :arg angularTreshold: New deactivation angular treshold.
   :type angularTreshold: float

.. function:: setDeactivationLinearTreshold(linearTreshold)

   Sets the deactivation linear treshold.

   :arg linearTreshold: New deactivation linear treshold.
   :type linearTreshold: float

.. function:: setDeactivationTime(time)

   Sets the time after which a resting rigidbody gets deactived.

   :arg time: The deactivation time.
   :type time: float

.. function:: setDebugMode(mode)

   Sets the debug mode.

   Debug modes:
      - No debug: 0
      - Draw wireframe: 1
      - Draw Aabb: 2 #What's Aabb?
      - Draw freatures text: 4
      - Draw contact points: 8
      - No deactivation: 16
      - No help text: 32
      - Draw text: 64
      - Profile timings: 128
      - Enable sat comparision: 256
      - Disable Bullet LCP: 512
      - Enable CCD: 1024
      - Draw Constraints: #(1 << 11) = ?
      - Draw Constraint Limits: #(1 << 12) = ?
      - Fast Wireframe: #(1 << 13) = ?

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

   Not implemented.

.. function:: setNumIterations(numiter)

   Sets the number of iterations for an iterative constraint solver.

   :arg numiter: New number of iterations.
   :type numiter: int

.. function:: setNumTimeSubSteps(numsubstep)

   Sets the number of substeps for each physics proceed. Tradeoff quality for performance.

   :arg numsubstep: New number of substeps.
   :type numsubstep: int

.. function:: setSolverDamping(damping)

   ..note::
      Very experimental, not recommended

   Sets the solver damping.

   :arg damping: New damping for the solver.
   :type damping: float

.. function:: setSolverTau(tau)

   .. note::
      Very experimental, not recommended

   Sets the solver tau.

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

   Sets the sor constant.

   :arg sor: New sor value.
   :type sor: float

.. function:: setUseEpa(epa)

   Not implemented.
