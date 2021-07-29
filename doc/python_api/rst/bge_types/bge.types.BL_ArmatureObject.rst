BL_ArmatureObject(KX_GameObject)
================================

.. module:: bge.types

base class --- :class:`KX_GameObject`

.. class:: BL_ArmatureObject(KX_GameObject)

   An armature object.

   .. attribute:: constraints

      The list of armature constraint defined on this armature.
      Elements of the list can be accessed by index or string.
      The key format for string access is '<bone_name>:<constraint_name>'.

      :type: list of :class:`BL_ArmatureConstraint`

   .. attribute:: channels

      The list of armature channels.
      Elements of the list can be accessed by index or name the bone.

      :type: list of :class:`BL_ArmatureChannel`

   .. method:: update()

      Ensures that the armature will be updated on next graphic frame.

      This action is unecessary if a KX_ArmatureActuator with mode run is active
      or if an action is playing. Use this function in other cases. It must be called
      on each frame to ensure that the armature is updated continously.

