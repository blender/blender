"""
Some properties are converted to Python objects when you retrieve them. This
needs to be avoided in order to create the subscription, by using
``datablock.path_resolve("property_name", False)``:
"""
subscribe_to = bpy.context.object.path_resolve("name", False)
