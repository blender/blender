BL_ArmatureChannel(PyObjectPlus)
================================

.. module:: bge.types

base class --- :class:`PyObjectPlus`

.. class:: BL_ArmatureChannel(PyObjectPlus)

   Proxy to armature pose channel. Allows to read and set armature pose.
   The attributes are identical to RNA attributes, but mostly in read-only mode.

   .. attribute:: name

      channel name (=bone name), read-only.

      :type: string

   .. attribute:: bone

      return the bone object corresponding to this pose channel, read-only.

      :type: :class:`BL_ArmatureBone`

   .. attribute:: parent

      return the parent channel object, None if root channel, read-only.

      :type: :class:`BL_ArmatureChannel`

   .. attribute:: has_ik

      true if the bone is part of an active IK chain, read-only.
      This flag is not set when an IK constraint is defined but not enabled (miss target information for example).

      :type: boolean

   .. attribute:: ik_dof_x

      true if the bone is free to rotation in the X axis, read-only.

      :type: boolean

   .. attribute:: ik_dof_y

      true if the bone is free to rotation in the Y axis, read-only.

      :type: boolean

   .. attribute:: ik_dof_z

      true if the bone is free to rotation in the Z axis, read-only.

      :type: boolean

   .. attribute:: ik_limit_x

      true if a limit is imposed on X rotation, read-only.

      :type: boolean

   .. attribute:: ik_limit_y

      true if a limit is imposed on Y rotation, read-only.

      :type: boolean

   .. attribute:: ik_limit_z

      true if a limit is imposed on Z rotation, read-only.

      :type: boolean

   .. attribute:: ik_rot_control

      true if channel rotation should applied as IK constraint, read-only.

      :type: boolean

   .. attribute:: ik_lin_control

      true if channel size should applied as IK constraint, read-only.

      :type: boolean

   .. attribute:: location

      displacement of the bone head in armature local space, read-write.

      :type: vector [X, Y, Z].

      .. note::
      
         You can only move a bone if it is unconnected to its parent. An action playing on the armature may change the value. An IK chain does not update this value, see joint_rotation.

      .. note::
      
         Changing this field has no immediate effect, the pose is updated when the armature is updated during the graphic render (see :data:`BL_ArmatureObject.update`).

   .. attribute:: scale

      scale of the bone relative to its parent, read-write.

      :type: vector [sizeX, sizeY, sizeZ].

      .. note::
      
         An action playing on the armature may change the value.  An IK chain does not update this value, see joint_rotation.

      .. note::
      
         Changing this field has no immediate effect, the pose is updated when the armature is updated during the graphic render (see :data:`BL_ArmatureObject.update`)

   .. attribute:: rotation_quaternion

      rotation of the bone relative to its parent expressed as a quaternion, read-write.

      :type: vector [qr, qi, qj, qk].

      .. note::
      
         This field is only used if rotation_mode is 0. An action playing on the armature may change the value.  An IK chain does not update this value, see joint_rotation.

      .. note::
      
         Changing this field has no immediate effect, the pose is updated when the armature is updated during the graphic render (see :data:`BL_ArmatureObject.update`)

   .. attribute:: rotation_euler

      rotation of the bone relative to its parent expressed as a set of euler angles, read-write.

      :type: vector [X, Y, Z].

      .. note::
      
         This field is only used if rotation_mode is > 0. You must always pass the angles in [X, Y, Z] order; the order of applying the angles to the bone depends on rotation_mode. An action playing on the armature may change this field.  An IK chain does not update this value, see joint_rotation.

      .. note::
      
         Changing this field has no immediate effect, the pose is updated when the armature is updated during the graphic render (see :data:`BL_ArmatureObject.update`)

   .. attribute:: rotation_mode

      Method of updating the bone rotation, read-write.

      :type: integer (one of :ref:`these constants <armaturechannel-constants-rotation-mode>`)

   .. attribute:: channel_matrix

      pose matrix in bone space (deformation of the bone due to action, constraint, etc), Read-only.
      This field is updated after the graphic render, it represents the current pose.

      :type: matrix [4][4]

   .. attribute:: pose_matrix

      pose matrix in armature space, read-only, 
      This field is updated after the graphic render, it represents the current pose.

      :type: matrix [4][4]

   .. attribute:: pose_head

      position of bone head in armature space, read-only.

      :type: vector [x, y, z]

   .. attribute:: pose_tail

      position of bone tail in armature space, read-only.

      :type: vector [x, y, z]

   .. attribute:: ik_min_x

      minimum value of X rotation in degree (<= 0) when X rotation is limited (see ik_limit_x), read-only.

      :type: float

   .. attribute:: ik_max_x

      maximum value of X rotation in degree (>= 0) when X rotation is limited (see ik_limit_x), read-only.

      :type: float

   .. attribute:: ik_min_y

      minimum value of Y rotation in degree (<= 0) when Y rotation is limited (see ik_limit_y), read-only.

      :type: float

   .. attribute:: ik_max_y

      maximum value of Y rotation in degree (>= 0) when Y rotation is limited (see ik_limit_y), read-only.

      :type: float

   .. attribute:: ik_min_z

      minimum value of Z rotation in degree (<= 0) when Z rotation is limited (see ik_limit_z), read-only.

      :type: float

   .. attribute:: ik_max_z

      maximum value of Z rotation in degree (>= 0) when Z rotation is limited (see ik_limit_z), read-only.

      :type: float

   .. attribute:: ik_stiffness_x

      bone rotation stiffness in X axis, read-only.

      :type: float between 0 and 1

   .. attribute:: ik_stiffness_y

      bone rotation stiffness in Y axis, read-only.

      :type: float between 0 and 1

   .. attribute:: ik_stiffness_z

      bone rotation stiffness in Z axis, read-only.

      :type: float between 0 and 1

   .. attribute:: ik_stretch

      ratio of scale change that is allowed, 0=bone can't change size, read-only.

      :type: float

   .. attribute:: ik_rot_weight

      weight of rotation constraint when ik_rot_control is set, read-write.

      :type: float between 0 and 1

   .. attribute:: ik_lin_weight

      weight of size constraint when ik_lin_control is set, read-write.

      :type: float between 0 and 1

   .. attribute:: joint_rotation

      Control bone rotation in term of joint angle (for robotic applications), read-write.

      When writing to this attribute, you pass a [x, y, z] vector and an appropriate set of euler angles or quaternion is calculated according to the rotation_mode.

      When you read this attribute, the current pose matrix is converted into a [x, y, z] vector representing the joint angles.

      The value and the meaning of the x, y, z depends on the ik_dof_x/ik_dof_y/ik_dof_z attributes:

      * 1DoF joint X, Y or Z: the corresponding x, y, or z value is used an a joint angle in radiant
      * 2DoF joint X+Y or Z+Y: treated as 2 successive 1DoF joints: first X or Z, then Y. The x or z value is used as a joint angle in radiant along the X or Z axis, followed by a rotation along the new Y axis of y radiants.
      * 2DoF joint X+Z: treated as a 2DoF joint with rotation axis on the X/Z plane. The x and z values are used as the coordinates of the rotation vector in the X/Z plane.
      * 3DoF joint X+Y+Z: treated as a revolute joint. The [x, y, z] vector represents the equivalent rotation vector to bring the joint from the rest pose to the new pose.

      :type: vector [x, y, z]
      
      .. note::
      
         The bone must be part of an IK chain if you want to set the ik_dof_x/ik_dof_y/ik_dof_z attributes via the UI, but this will interfere with this attribute since the IK solver will overwrite the pose. You can stay in control of the armature if you create an IK constraint but do not finalize it (e.g. don't set a target) the IK solver will not run but the IK panel will show up on the UI for each bone in the chain.

      .. note::
      
         [0, 0, 0] always corresponds to the rest pose.

      .. note::
      
         You must request the armature pose to update and wait for the next graphic frame to see the effect of setting this attribute (see :data:`BL_ArmatureObject.update`).

      .. note::
      
         You can read the result of the calculation in rotation or euler_rotation attributes after setting this attribute.

