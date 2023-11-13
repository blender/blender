"""
This example shows how to use B-Bone segment matrices to emulate deformation
produced by the Armature modifier or constraint when assigned to the given bone
(without Preserve Volume). The coordinates are processed in armature Pose space:
"""


def bbone_deform_matrix(pose_bone, point):
    index, blend_next = pose_bone.bbone_segment_index(point)

    rest1 = pose_bone.bbone_segment_matrix(index, rest=True)
    pose1 = pose_bone.bbone_segment_matrix(index, rest=False)
    deform1 = pose1 @ rest1.inverted()

    # bbone_segment_index ensures that index + 1 is always valid
    rest2 = pose_bone.bbone_segment_matrix(index + 1, rest=True)
    pose2 = pose_bone.bbone_segment_matrix(index + 1, rest=False)
    deform2 = pose2 @ rest2.inverted()

    deform = deform1 * (1 - blend_next) + deform2 * blend_next

    return pose_bone.matrix @ deform @ pose_bone.bone.matrix_local.inverted()


# Armature modifier deforming vertices:
mesh = bpy.data.objects["Mesh"]
pose_bone = bpy.data.objects["Armature"].pose.bones["Bone"]

for vertex in mesh.data.vertices:
    vertex.co = bbone_deform_matrix(pose_bone, vertex.co) @ vertex.co

# Armature constraint modifying an object transform:
empty = bpy.data.objects["Empty"]
matrix = empty.matrix_world

empty.matrix_world = bbone_deform_matrix(pose_bone, matrix.translation) @ matrix
