"""
Basic Physics Constraint
++++++++++++++++++++++++
Example of how to create a hinge Physics Constraint between two objects.
"""
from bge import logic
from bge import constraints

# get object list
objects = logic.getCurrentScene().objects

# get object named Object1 and Object 2
object_1 = objects["Object1"]
object_2 = objects["Object2"]

# want to use Edge constraint type
constraint_type = 2

# get Object1 and Object2 physics IDs
physics_id_1 = object_1.getPhysicsId()
physics_id_2 = object_2.getPhysicsId()

# Use bottom right edge of Object1 for hinge position
edge_position_x = 1.0
edge_position_y = 0.0
edge_position_z = -1.0

# use Object1 y axis for angle to point hinge
edge_angle_x = 0.0
edge_angle_y = 1.0
edge_angle_z = 0.0

# create an edge constraint
constraints.createConstraint(physics_id_1, physics_id_2,
                             constraint_type,
                             edge_position_x, edge_position_y, edge_position_z,
                             edge_angle_x, edge_angle_y, edge_angle_z)
