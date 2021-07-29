BL_ArmatureBone(PyObjectPlus)
=============================

.. module:: bge.types

base class --- :class:`PyObjectPlus`

.. class:: BL_ArmatureBone(PyObjectPlus)

   Proxy to Blender bone structure. All fields are read-only and comply to RNA names.
   All space attribute correspond to the rest pose.

   .. attribute:: name

      bone name.

      :type: string

   .. attribute:: connected

      true when the bone head is struck to the parent's tail.

      :type: boolean

   .. attribute:: hinge

      true when bone doesn't inherit rotation or scale from parent bone.

      :type: boolean

   .. attribute:: inherit_scale

      true when bone inherits scaling from parent bone.

      :type: boolean

   .. attribute:: bbone_segments

      number of B-bone segments.

      :type: integer

   .. attribute:: roll

      bone rotation around head-tail axis.

      :type: float

   .. attribute:: head

      location of head end of the bone in parent bone space.

      :type: vector [x, y, z]

   .. attribute:: tail

      location of head end of the bone in parent bone space.

      :type: vector [x, y, z]

   .. attribute:: length

      bone length.

      :type: float

   .. attribute:: arm_head

      location of head end of the bone in armature space.

      :type: vector [x, y, z]

   .. attribute:: arm_tail

      location of tail end of the bone in armature space.

      :type: vector [x, y, z]

   .. attribute:: arm_mat

      matrix of the bone head in armature space.

      :type: matrix [4][4]

      .. note::
      
         This matrix has no scale part. 

   .. attribute:: bone_mat

      rotation matrix of the bone in parent bone space.

      :type: matrix [3][3]

   .. attribute:: parent

      parent bone, or None for root bone.

      :type: :class:`BL_ArmatureBone`

   .. attribute:: children

      list of bone's children.

      :type: list of :class:`BL_ArmatureBone`
