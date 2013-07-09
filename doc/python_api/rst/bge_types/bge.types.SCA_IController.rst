SCA_IController(SCA_ILogicBrick)
================================

.. module:: bge.types

base class --- :class:`SCA_ILogicBrick`

.. class:: SCA_IController(SCA_ILogicBrick)

   Base class for all controller logic bricks.

   .. attribute:: state

      The controllers state bitmask. This can be used with the GameObject's state to test if the controller is active.
      
      :type: int bitmask

   .. attribute:: sensors

      A list of sensors linked to this controller.
      
      :type: sequence supporting index/string lookups and iteration.

      .. note::

         The sensors are not necessarily owned by the same object.

      .. note::
         
         When objects are instanced in dupligroups links may be lost from objects outside the dupligroup.

   .. attribute:: actuators

      A list of actuators linked to this controller.
      
      :type: sequence supporting index/string lookups and iteration.

      .. note::

         The sensors are not necessarily owned by the same object.

      .. note::
         
         When objects are instanced in dupligroups links may be lost from objects outside the dupligroup.

   .. attribute:: useHighPriority

      When set the controller executes always before all other controllers that dont have this set.
      
      :type: boolen

      .. note::
         
         Order of execution between high priority controllers is not guaranteed.

