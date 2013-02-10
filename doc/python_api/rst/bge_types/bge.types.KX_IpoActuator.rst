KX_IpoActuator(SCA_IActuator)
=============================

.. module:: bge.types

base class --- :class:`SCA_IActuator`

.. class:: KX_IpoActuator(SCA_IActuator)

   IPO actuator activates an animation.

   .. attribute:: frameStart

      Start frame.

      :type: float

   .. attribute:: frameEnd

      End frame.

      :type: float

   .. attribute:: propName

      Use this property to define the Ipo position.

      :type: string

   .. attribute:: framePropName

      Assign this property this action current frame number.

      :type: string

   .. attribute:: mode

      Play mode for the ipo. Can be on of :ref:`these constants <ipo-actuator>`

      :type: integer

   .. attribute:: useIpoAsForce

      Apply Ipo as a global or local force depending on the local option (dynamic objects only).

      :type: boolean

   .. attribute:: useIpoAdd

      Ipo is added to the current loc/rot/scale in global or local coordinate according to Local flag.

      :type: boolean

   .. attribute:: useIpoLocal

      Let the ipo acts in local coordinates, used in Force and Add mode.

      :type: boolean

   .. attribute:: useChildren

      Update IPO on all children Objects as well.

      :type: boolean

