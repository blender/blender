"""
Accessing Evaluated Geometry
++++++++++++++++++++++++++++
"""
import bpy

# The GeometrySet can only be retrieved from an evaluated object. So one always
# needs a depsgraph that has evaluated the object.
depsgraph = bpy.context.view_layer.depsgraph
ob = bpy.context.active_object
ob_eval = depsgraph.id_eval_get(ob)

# Get the final evaluated geometry of an object.
geometry = ob_eval.evaluated_geometry()

# Print basic information like the number of elements.
print(geometry)

# A geometry set may have a name. It can be set with the Set Geometry Name node.
print(geometry.name)

# Access "realized" geometry components.
print(geometry.mesh)
print(geometry.pointcloud)
print(geometry.curves)
print(geometry.volume)
print(geometry.grease_pencil)

# Access the mesh without final subdivision applied.
print(geometry.mesh_base)

# Accessing instances is a bit more tricky, because there is no specific
# mechanism to expose instances. Instead, two accessors are provided which
# are easy to keep working in the future even if we get a proper Instances type.

# This is a pointcloud that provides access to all the instance attributes.
# There is a point per instances. May return None if there is no instances data.
instances_pointcloud = geometry.instances_pointcloud()

if instances_pointcloud is not None:
    # This is a list containing the data that is instanced. The list may contain
    # None, objects, collections or other GeometrySets. If the geometry does not
    # have instances, the list is empty.
    references = geometry.instance_references()

    # Besides normal generic attributes, there are also two important
    # instance-specific attributes. "instance_transform" is a 4x4 matrix attribute
    # containing the transforms of each instance.
    instance_transforms = instances_pointcloud.attributes["instance_transform"]

    # ".reference_index" contains indices into the `references` list above and
    # determines what geometry each instance uses.
    reference_indices = instances_pointcloud.attributes[".reference_index"]
