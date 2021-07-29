from .. data_structures.lists.base_lists cimport EulerList, Matrix4x4List, QuaternionList
from . matrix cimport Matrix3_or_Matrix4
from . euler cimport Euler3
from . quaternion cimport Quaternion

cdef matrixToEuler(Euler3* target, Matrix3_or_Matrix4* m)
