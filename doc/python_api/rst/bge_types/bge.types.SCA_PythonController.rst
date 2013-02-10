SCA_PythonController(SCA_IController)
=====================================

.. module:: bge.types

base class --- :class:`SCA_IController`

.. class:: SCA_PythonController(SCA_IController)

   A Python controller uses a Python script to activate it's actuators, 
   based on it's sensors.

   .. attribute:: script

      The value of this variable depends on the execution methid.

      * When 'Script' execution mode is set this value contains the entire python script as a single string (not the script name as you might expect) which can be modified to run different scripts.
      * When 'Module' execution mode is set this value will contain a single line string - module name and function "module.func" or "package.modile.func" where the module names are python textblocks or external scripts.

      :type: string
      
      .. note::
      
         Once this is set the script name given for warnings will remain unchanged.

   .. attribute:: mode

      the execution mode for this controller (read-only).

      * Script: 0, Execite the :data:`script` as a python code.
      * Module: 1, Execite the :data:`script` as a module and function.

      :type: integer

   .. method:: activate(actuator)

      Activates an actuator attached to this controller.

      :arg actuator: The actuator to operate on.
      :type actuator: actuator or the actuator name as a string

   .. method:: deactivate(actuator)

      Deactivates an actuator attached to this controller.

      :arg actuator: The actuator to operate on.
      :type actuator: actuator or the actuator name as a string

