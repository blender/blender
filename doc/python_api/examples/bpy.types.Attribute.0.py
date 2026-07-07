"""
Attributes are used to store data that corresponds to geometry elements.
Geometry elements are items in one of the geometry domains like points, curves, or faces.

An attribute has a ``name``, a ``type``, and is stored on a ``domain``.

``name``
   The name of this attribute. Names have to be unique within the same geometry.
   If the name starts with a ``.``, the attribute is hidden from the UI.
``type``
   The type of data that this attribute stores, e.g. a float, integer, color, etc.
   See `Attribute Type Items <bpy_types_enum_items/attribute_type_items.html>`__.
``domain``
   The geometry domain that the attribute is stored on.
   See `Attribute Domain Items <bpy_types_enum_items/attribute_domain_items.html>`__.


Using Attributes
++++++++++++++++

Attributes can be stored on geometries like :class:`Mesh`, :class:`Curves`, :class:`PointCloud`, etc.
These geometries have attribute groups (usually called ``attributes``).
Using the groups, attributes can then be accessed by their name:

.. code-block:: python

   radii = curves.attributes["radius"]

Creating and storing custom attributes is done using the ``attributes.new`` function:

.. code-block:: python

   # Add a new attribute named `my_attribute_name` of type `float` on the point domain of the geometry.
   my_attribute = curves.attributes.new("my_attribute_name", 'FLOAT', 'POINT')

Removing attributes can be done like so:

.. code-block:: python

   attribute = drawing.attributes["some_attribute"]
   drawing.attributes.remove(attribute)

.. note::

   Some attributes are required and cannot be removed, like ``"position"``.

Attribute values are read by accessing their ``attribute.data`` collection property.
However, in cases where multiple values should be read at once,
it is better to use the :class:`bpy_prop_collection.foreach_get` function and read the values into a ``numpy`` buffer.

.. code-block:: python

   import numpy as np

   # Get the radius attribute.
   radii = curves.attributes["radius"]
   # Print the radius of the first point.
   print(radii.data[0].value)
   # Output: 0.005

   # Get the total number of points.
   num_points = attributes.domain_size('POINT')
   # Create an empty buffer to read all the radii into.
   radii_data = np.zeros(num_points, dtype=np.float32)
   # Read all the radii of the curves into `radii_data` at once.
   radii.data.foreach_get('value', radii_data)
   # Print all the radii.
   print(radii_data)
   # Output: [0.1, 0.2, 0.3, 0.4, ... ]

.. note::

   Some attribute types use different named properties to access their value.
   Instead of ``value``, vectors use ``vector``, and colors use ``color``.

Writing to different attribute types is very similar. You can simply assign to a value directly.
Again, when writing to multiple values, it is recommended to use the :class:`bpy_prop_collection.foreach_set` function
to write the values from a ``numpy`` buffer.

.. code-block:: python

   import numpy as np

   radii = curves.attributes["radius"]
   # Write a radius with a value of 0.5 to the first point.
   radii.data[0].value = 0.5
   print(radii.data[0].value)
   # Output: 0.5

   num_points = attributes.domain_size('POINT')
   # Generate random radii with values between 0.001 and 0.05 using numpy.
   new_radii = np.random.uniform(0.001, 0.05, num_points)
   # Write the new radii to the radius attribute.
   radii.data.foreach_set('value', new_radii)


The :class:`bpy_prop_collection.foreach_get` / :class:`bpy_prop_collection.foreach_set` methods require a flat array.
This is sometimes not desirable, e.g. when reading/writing positions, which are 3D vectors.
In these cases, it's possible to use ``np.ravel`` to pass the data as a flat array:

.. code-block:: python

   num_points = attributes.domain_size('POINT')
   positions = curves.attributes['position']
   # Here, we're using a numpy array with shape (num_points, 3) so that each
   # element is a 3d vector.
   positions_data = np.zeros((num_points, 3), dtype=np.float32)
   # The `np.ravel` function will pass the `positions_data` as a flat array
   # without changing the original shape.
   positions.data.foreach_get('vector', np.ravel(positions_data))
   print(positions_data)
   # Output: [[0.1, 0.2, 0.3], [0.4, 0.5, 0.6], ...]

"""
