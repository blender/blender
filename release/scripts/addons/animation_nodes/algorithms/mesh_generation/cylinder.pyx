from libc.math cimport sin, cos
from libc.math cimport M_PI as PI
from ... data_structures cimport Vector3DList, EdgeIndicesList, PolygonIndicesList

# Vertices
###########################################

def vertices(float radius, float height, Py_ssize_t resolution):
    if resolution < 2:
        raise Exception("resolution has to be >= 2")

    cdef:
        Py_ssize_t i
        Vector3DList vertices
        float angleFactor = 2 * PI / resolution
        float angle, x, y

    vertices = Vector3DList(length = resolution * 2)
    for i in range(resolution):
        angle = i * angleFactor
        x = cos(angle) * radius
        y = sin(angle) * radius

        vertices.data[i].x = x
        vertices.data[i].y = y
        vertices.data[i].z = 0

        vertices.data[i + resolution].x = x
        vertices.data[i + resolution].y = y
        vertices.data[i + resolution].z = height

    return vertices


# Edges
############################################

planeEdges = EdgeIndicesList.fromValues([(0, 2), (1, 3), (0, 1), (2, 3)])

def edges(Py_ssize_t resolution):
    if resolution < 2:
        raise Exception("resolution has to be >= 2")

    cdef:
        EdgeIndicesList edges
        Py_ssize_t i, edgeAmount

    if resolution == 2:
        edges = planeEdges.copy()
    else:
        edges = EdgeIndicesList(length = 3 * resolution)

        for i in range(resolution - 1):
            edges.data[3 * i + 0].v1 = i
            edges.data[3 * i + 0].v2 = i + resolution

            edges.data[3 * i + 1].v1 = i
            edges.data[3 * i + 1].v2 = i + 1

            edges.data[3 * i + 2].v1 = i + resolution
            edges.data[3 * i + 2].v2 = i + resolution + 1

        edges.data[edges.length - 3].v1 = resolution - 1
        edges.data[edges.length - 3].v2 = 2 * resolution - 1

        edges.data[edges.length - 2].v1 = resolution - 1
        edges.data[edges.length - 2].v2 = 0

        edges.data[edges.length - 1].v1 = 2 * resolution - 1
        edges.data[edges.length - 1].v2 = resolution

    return edges


# Polygons
############################################

planePolygons = PolygonIndicesList.fromValues([(0, 1, 3, 2)])

def polygons(Py_ssize_t resolution, bint caps = True):
    if resolution < 2:
        raise Exception("resolution has to be >= 2")

    cdef:
        PolygonIndicesList polygons
        Py_ssize_t i, polygonAmount, indicesAmount

    if resolution == 2:
        polygons = planePolygons.copy()
    else:
        if caps:
            indicesAmount = 6 * resolution
            polygonAmount = resolution + 2
        else:
            indicesAmount = 4 * resolution
            polygonAmount = resolution

        polygons = PolygonIndicesList(
            indicesAmount = indicesAmount,
            polygonAmount = polygonAmount)

        for i in range(resolution - 1):
            polygons.polyStarts.data[i] = 4 * i
            polygons.polyLengths.data[i] = 4

            polygons.indices.data[4 * i + 0] = i
            polygons.indices.data[4 * i + 1] = i + 1
            polygons.indices.data[4 * i + 2] = resolution + i + 1
            polygons.indices.data[4 * i + 3] = resolution + i

        polygons.polyStarts.data[resolution - 1] = 4 * (resolution - 1)
        polygons.polyLengths.data[resolution - 1] = 4
        polygons.indices.data[4 * (resolution - 1) + 0] = resolution - 1
        polygons.indices.data[4 * (resolution - 1) + 1] = 0
        polygons.indices.data[4 * (resolution - 1) + 2] = resolution
        polygons.indices.data[4 * (resolution - 1) + 3] = 2 * resolution - 1

        if caps:
            polygons.polyStarts.data[resolution] = 4 * resolution
            polygons.polyLengths.data[resolution] = resolution
            polygons.polyStarts.data[resolution + 1] = 5 * resolution
            polygons.polyLengths.data[resolution + 1] = resolution

            for i in range(resolution):
                polygons.indices.data[4 * resolution + i] = resolution - i - 1
                polygons.indices.data[5 * resolution + i] = resolution + i
    return polygons
