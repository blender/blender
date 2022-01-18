"""
This method enables conversions between Local and Pose space for bones in
the middle of updating the armature without having to update dependencies
after each change, by manually carrying updated matrices in a recursive walk.
"""

def set_pose_matrices(obj, matrix_map):
    "Assign pose space matrices of all bones at once, ignoring constraints."

    def rec(pbone, parent_matrix):
        if pbone.name in matrix_map:
            matrix = matrix_map[pbone.name]

            ## Instead of:
            # pbone.matrix = matrix
            # bpy.context.view_layer.update()

            # Compute and assign local matrix, using the new parent matrix
            if pbone.parent:
                pbone.matrix_basis = pbone.bone.convert_local_to_pose(
                    matrix,
                    pbone.bone.matrix_local,
                    parent_matrix=parent_matrix,
                    parent_matrix_local=pbone.parent.bone.matrix_local,
                    invert=True
                )
            else:
                pbone.matrix_basis = pbone.bone.convert_local_to_pose(
                    matrix,
                    pbone.bone.matrix_local,
                    invert=True
                )
        else:
            # Compute the updated pose matrix from local and new parent matrix
            if pbone.parent:
                matrix = pbone.bone.convert_local_to_pose(
                    pbone.matrix_basis,
                    pbone.bone.matrix_local,
                    parent_matrix=parent_matrix,
                    parent_matrix_local=pbone.parent.bone.matrix_local,
                )
            else:
                matrix = pbone.bone.convert_local_to_pose(
                    pbone.matrix_basis,
                    pbone.bone.matrix_local,
                )

        # Recursively process children, passing the new matrix through
        for child in pbone.children:
            rec(child, matrix)

    # Scan all bone trees from their roots
    for pbone in obj.pose.bones:
        if not pbone.parent:
            rec(pbone, None)
