"""
Manipulator Overview
--------------------

Manipulators are created using two classes.

- :class:`bpy.types.ManipulatorGroup` - stores a list of manipulators.

  The manipulator group is associated with a space and region type.
- :class:`bpy.types.Manipulator` - a single item which can be used.

  Each manipulator group has a collection of manipulators which it manages.

The following example shows a manipulator group with a single,
manipulator used to control a lamp objects energy.

.. literalinclude:: __/__/__/release/scripts/templates_py/manipulator_simple.py


It's also possible to use a manipulator to run an operator.

.. literalinclude:: __/__/__/release/scripts/templates_py/manipulator_operator_target.py

This more comprehensive example shows how an operator can create a temporary manipulator group to adjust its settings.

.. literalinclude:: __/__/__/release/scripts/templates_py/manipulator_operator.py

"""

