"""
Get the property associated with a hovered button.
Returns a tuple of the data-block, data path to the property, and array index.

.. note::

   When the property doesn't have an associated :class:`bpy.types.ID` non-ID data may be returned.
   This may occur when accessing windowing data, for example, operator properties.
"""

# Example inserting keyframe for the hovered property.
active_property = bpy.context.property
if active_property:
    datablock, data_path, index = active_property
    datablock.keyframe_insert(data_path=data_path, index=index, frame=1)
