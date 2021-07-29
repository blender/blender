from ... data_structures cimport DoubleList, Vector3DList, EdgeIndicesList, FloatList, PolySpline

def splinesFromEdges(Vector3DList vertices, EdgeIndicesList edges, DoubleList radii, str radiusType):
    if edges.length == 0: return []
    if edges.getMaxIndex() >= vertices.length:
        raise Exception("Invalid edge indices")
    if radiusType == "EDGE":
        if edges.length != radii.length:
            raise Exception("wrong radius amount")
    elif radiusType == "VERTEX":
        if vertices.length != radii.length:
            raise Exception("wrong radius amount")

    cdef:
        long i
        list splines = []
        FloatList edgeRadii
        Vector3DList edgeVertices
        bint radiusPerVertex = radiusType == "VERTEX"

    for i in range(edges.length):
        edgeVertices = Vector3DList.__new__(Vector3DList, length = 2)
        edgeVertices.data[0] = vertices.data[edges.data[i].v1]
        edgeVertices.data[1] = vertices.data[edges.data[i].v2]

        edgeRadii = FloatList.__new__(FloatList, length = 2)
        if radiusPerVertex:
            edgeRadii.data[0] = radii.data[edges.data[i].v1]
            edgeRadii.data[1] = radii.data[edges.data[i].v2]
        else:
            edgeRadii.data[0] = radii.data[i]
            edgeRadii.data[1] = radii.data[i]

        splines.append(PolySpline.__new__(PolySpline, edgeVertices, edgeRadii))
    return splines
