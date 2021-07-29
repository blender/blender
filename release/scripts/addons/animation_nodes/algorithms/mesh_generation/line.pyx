from ... math cimport Vector3, toVector3
from ... data_structures cimport Vector3DList, EdgeIndicesList

# Vertices
###########################################

def vertices(start, end, long steps):
    assert steps >= 2
    cdef:
        Vector3 _start = toVector3(start)
        Vector3 _end = toVector3(end)
        float stepsInverse = 1 / <float>(steps - 1)
        float startWeight, endWeight
        long i

    vertices = Vector3DList(length = steps)
    for i in range(steps):
        startWeight = 1 - i * stepsInverse
        endWeight = i * stepsInverse
        vertices.data[i].x = _start.x * startWeight + _end.x * endWeight
        vertices.data[i].y = _start.y * startWeight + _end.y * endWeight
        vertices.data[i].z = _start.z * startWeight + _end.z * endWeight
    return vertices


# Edges
############################################

def edges(long steps):
    edges = EdgeIndicesList(length = steps - 1)
    cdef long i
    for i in range(steps - 1):
        edges.data[i].v1 = i
        edges.data[i].v2 = i + 1
    return edges
