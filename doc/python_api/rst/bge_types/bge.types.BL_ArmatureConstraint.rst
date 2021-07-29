BL_ArmatureConstraint(PyObjectPlus)
===================================

.. module:: bge.types

base class --- :class:`PyObjectPlus`

.. class:: BL_ArmatureConstraint(PyObjectPlus)

   Proxy to Armature Constraint. Allows to change constraint on the fly.
   Obtained through :class:`BL_ArmatureObject`.constraints.

   .. note::
   
      Not all armature constraints are supported in the GE.

      
   .. attribute:: type

      Type of constraint, (read-only).

      Use one of :ref:`these constants<armatureconstraint-constants-type>`.
      
      :type: integer, one of CONSTRAINT_TYPE_* constants

   .. attribute:: name

      Name of constraint constructed as <bone_name>:<constraint_name>. constraints list.

      :type: string

      This name is also the key subscript on :class:`BL_ArmatureObject`.

   .. attribute:: enforce

      fraction of constraint effect that is enforced. Between 0 and 1.

      :type: float

   .. attribute:: headtail

      Position of target between head and tail of the target bone: 0=head, 1=tail.

      :type: float.

      .. note::
      
         Only used if the target is a bone (i.e target object is an armature.

   .. attribute:: lin_error

      runtime linear error (in Blender units) on constraint at the current frame.

      This is a runtime value updated on each frame by the IK solver. Only available on IK constraint and iTaSC solver.

      :type: float

   .. attribute:: rot_error

      Runtime rotation error (in radiant) on constraint at the current frame.

      :type: float.

      This is a runtime value updated on each frame by the IK solver. Only available on IK constraint and iTaSC solver.

      It is only set if the constraint has a rotation part, for example, a CopyPose+Rotation IK constraint.

   .. attribute:: target

      Primary target object for the constraint. The position of this object in the GE will be used as target for the constraint.

      :type: :class:`KX_GameObject`.

   .. attribute:: subtarget

      Secondary target object for the constraint. The position of this object in the GE will be used as secondary target for the constraint.

      :type: :class:`KX_GameObject`.

      Currently this is only used for pole target on IK constraint.

   .. attribute:: active

      True if the constraint is active.

      :type: boolean
      
      .. note::
      
         An inactive constraint does not update lin_error and rot_error.

   .. attribute:: ik_weight

      Weight of the IK constraint between 0 and 1.

      Only defined for IK constraint.

      :type: float

   .. attribute:: ik_type

      Type of IK constraint, (read-only).

      Use one of :ref:`these constants<armatureconstraint-constants-ik-type>`.
      
      :type: integer.

   .. attribute:: ik_flag

      Combination of IK constraint option flags, read-only.
      
      Use one of :ref:`these constants<armatureconstraint-constants-ik-flag>`.

      :type: integer

   .. attribute:: ik_dist

      Distance the constraint is trying to maintain with target, only used when ik_type=CONSTRAINT_IK_DISTANCE.

      :type: float

   .. attribute:: ik_mode

      Use one of :ref:`these constants<armatureconstraint-constants-ik-mode>`.
      
      Additional mode for IK constraint. Currently only used for Distance constraint:

      :type: integer

