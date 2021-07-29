from ... math cimport Euler3, Vector3, Matrix4

cdef eulerToDirection_LowLevel(Vector3* target, Euler3* rotation, char axis)

cdef directionToMatrix_LowLevel(Matrix4* target,
                                Vector3* direction, Vector3* guide,
                                char trackAxis, char guideAxis)
