KX_ParentActuator(SCA_IActuator)
================================

.. module:: bge.types

base class --- :class:`SCA_IActuator`

.. class:: KX_ParentActuator(SCA_IActuator)

   The parent actuator can set or remove an objects parent object.

   .. attribute:: object

      the object this actuator sets the parent too.

      :type: :class:`KX_GameObject` or None

   .. attribute:: mode

      The mode of this actuator.

      :type: integer from 0 to 1.

   .. attribute:: compound

      Whether the object shape should be added to the parent compound shape when parenting.

      Effective only if the parent is already a compound shape.

      :type: boolean

   .. attribute:: ghost

      Whether the object should be made ghost when parenting
      Effective only if the shape is not added to the parent compound shape.

      :type: boolean

