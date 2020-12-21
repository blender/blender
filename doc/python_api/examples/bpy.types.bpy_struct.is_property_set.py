"""
.. note::

   Properties defined at run-time store the values of the properties as custom-properties.

   This method checks if the underlying data exists, causing the property to be considered *set*.

   A common pattern for operators is to calculate a value for the properties
   that have not had their values explicitly set by the caller
   (where the caller could be a key-binding, menu-items or Python script for example).

   In the case of executing operators multiple times, values are re-used from the previous execution.

   For example: subdividing a mesh with a smooth value of 1.0 will keep using
   that value on subsequent calls to subdivision, unless the operator is called with
   that property set to a different value.

   This behavior can be disabled using the ``SKIP_SAVE`` option when the property is declared (see: :mod:`bpy.props`).

   The ``ghost`` argument allows detecting how a value from a previous execution is handled.

   - When true: The property is considered unset even if the value from a previous call is used.
   - When false: The existence of any values causes ``is_property_set`` to return true.

   While this argument should typically be omitted, there are times when
   it's important to know if a value is anything besides the default.

   For example, the previous value may have been scaled by the scene's unit scale.
   In this case scaling the value multiple times would cause problems, so the ``ghost`` argument should be false.
"""
