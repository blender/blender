"""
Get the property associated with a hovered button.
Returns a tuple of the datablock, data path to the property, and array index.
"""

# Example inserting keyframe for the hovered property.
active_property = bpy.context.property
if active_property:
    datablock, data_path, index = active_property
    datablock.keyframe_insert(data_path=data_path, index=index, frame=1)
