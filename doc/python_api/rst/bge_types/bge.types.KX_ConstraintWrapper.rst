KX_ConstraintWrapper(PyObjectPlus)
==================================

.. module:: bge.types

base class --- :class:`PyObjectPlus`

.. class:: KX_ConstraintWrapper(PyObjectPlus)

   KX_ConstraintWrapper

   .. method:: getConstraintId(val)

      Returns the contraint ID

      :return: the constraint ID
      :rtype: integer

   .. method:: setParam(axis, value0, value1)

      Set the contraint limits

      :arg axis:
      :type axis: integer

   .. note::
      For each axis:
      * Lowerlimit == Upperlimit -> axis is locked
      * Lowerlimit > Upperlimit -> axis is free
      * Lowerlimit < Upperlimit -> axis it limited in that range

      PHY_LINEHINGE_CONSTRAINT = 2 or PHY_ANGULAR_CONSTRAINT = 3:
      axis = 3 is a constraint limit, with low/high limit value

         * 3: X axis angle

      :arg value0 (min): Set the minimum limit of the axis
      :type value0: float
      :arg value1 (max): Set the maximum limit of the axis
      :type value1: float

      PHY_CONE_TWIST_CONSTRAINT = 3:
      axis = 3..5 are constraint limits, high limit values
         * 3: X axis angle
         * 4: Y axis angle
         * 5: Z axis angle

      :arg value0 (min): Set the minimum limit of the axis
      :type value0: float
      :arg value1 (max): Set the maximum limit of the axis
      :type value1: float

      PHY_GENERIC_6DOF_CONSTRAINT = 12:
      axis = 0..2 are constraint limits, with low/high limit value
         * 0: X axis position
         * 1: Y axis position
         * 2: Z axis position

      axis = 3..5 are relative constraint (Euler) angles in degrees
         * 3: X axis angle
         * 4: Y axis angle
         * 5: Z axis angle

      :arg value0 (min): Set the minimum limit of the axis
      :type value0: float
      :arg value1 (max): Set the maximum limit of the axis
      :type value1: float

      axis = 6..8 are translational motors, with value0=target velocity, value1 = max motor force
         * 6: X axis position
         * 7: Y axis position
         * 8: Z axis position

      axis = 9..11 are rotational motors, with value0=target velocity, value1 = max motor force
         * 9: X axis angle
         * 10: Y axis angle
         * 11: Z axis angle

      :arg value0 (speed): Set the linear velocity of the axis
      :type value0: float Range: -10,000.00 to 10,000.00
      :arg value1 (force): Set the maximum force limit of the axis
      :type value1: float Range: -10,000.00 to 10,000.00

      axis = 12..14 are for linear springs on each of the position of freedom
         * 12: X axis position
         * 13: Y axis position
         * 14: Z axis position

      axis = 15..17 are for angular springs on each of the degrees of freedom
         * 15: X axis angle
         * 16: Y axis angle
         * 17: Z axis angle

      :arg value0 (stiffness): Set the stiffness of the spring
      :type value0: float
      :arg value1 (damping): Tendency of the spring to return to it's original position
      :type value1: float
                    1.0 = springs back to original position (no damping)
                    0.0 = don't springs back

   .. method:: getParam(axis)

      Get the contraint position or euler angle of a generic 6DOF constraint

      :arg axis:
      :type axis: integer

      axis = 0..2 are linear constraint values
         * 0: X axis position
         * 1: Y axis position
         * 2: Z axis position

      :return: position
      :rtype: float

      axis = 3..5 are relative constraint (Euler) angles in degrees 
         * 3: X axis angle
         * 4: Y axis angle
         * 5: Z axis angle

      :return: angle
      :rtype: float

   .. attribute:: constraint_id

      Returns the contraint ID  (read only)

      :type: integer

   .. attribute:: constraint_type

      Returns the contraint type (read only)

      :type: integer

         * 1 = POINTTOPOINT_CONSTRAINT
         * 2 = LINEHINGE_CONSTRAINT
         * 3 = ANGULAR_CONSTRAINT (aka LINEHINGE_CONSTRAINT)
         * 4 = CONETWIST_CONSTRAINT
         * 11 = VEHICLE_CONSTRAINT
         * 12 = GENERIC_6DOF_CONSTRAINT
