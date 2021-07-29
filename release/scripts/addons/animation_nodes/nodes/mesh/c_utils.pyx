cimport cython
from libc.string cimport memcpy

from ... algorithms.mesh_generation import cylinder

from ... data_structures cimport (
    LongList,
    DoubleList,
    Vector3DList,
    Matrix4x4List,
    EdgeIndicesList,
    PolygonIndicesList,
    CDefaultList,
    MeshData
)

from ... math cimport (
    Vector3, Matrix4,
    scaleVec3, subVec3, crossVec3, distanceVec3, lengthVec3, dotVec3,
    transformVec3AsPoint_InPlace, normalizeVec3_InPlace, scaleVec3_Inplace,
    normalizeLengthVec3_Inplace, transformVec3AsPoint
)

# Edge Operations
###########################################

def createEdgeIndices(_indices1, _indices2):
    cdef CDefaultList indices1 = CDefaultList(LongList, _indices1, 0)
    cdef CDefaultList indices2 = CDefaultList(LongList, _indices2, 0)
    cdef Py_ssize_t amount = CDefaultList.getMaxLength(indices1, indices2)
    cdef EdgeIndicesList edges = EdgeIndicesList(length = amount)
    cdef unsigned int index1, index2
    cdef Py_ssize_t i

    for i in range(amount):
        index1 = (<long*>indices1.get(i))[0]
        index2 = (<long*>indices2.get(i))[0]
        edges.data[i].v1 = index1 if index1 >= 0 else 0
        edges.data[i].v2 = index2 if index2 >= 0 else 0
    return edges

def createEdges(Vector3DList points1, Vector3DList points2):
    assert(len(points1) == len(points2))

    cdef Vector3DList newPoints = Vector3DList(length = len(points1) * 2)
    cdef EdgeIndicesList edges = EdgeIndicesList(length = len(points1))
    cdef Py_ssize_t i

    for i in range(len(points1)):
        newPoints.data[2 * i + 0] = points1.data[i]
        newPoints.data[2 * i + 1] = points2.data[i]
        edges.data[i].v1 = 2 * i + 0
        edges.data[i].v2 = 2 * i + 1

    return newPoints, edges

def calculateEdgeLengths(Vector3DList vertices, EdgeIndicesList edges):
    ensureValidEdges(vertices, edges)

    cdef DoubleList distances = DoubleList(length = len(edges))
    cdef Py_ssize_t i
    cdef Vector3 *v1
    cdef Vector3 *v2
    for i in range(len(edges)):
        v1 = vertices.data + edges.data[i].v1
        v2 = vertices.data + edges.data[i].v2
        distances.data[i] = distanceVec3(v1, v2)
    return distances

def calculateEdgeCenters(Vector3DList vertices, EdgeIndicesList edges):
    ensureValidEdges(vertices, edges)

    cdef Vector3DList centers = Vector3DList(length = len(edges))
    cdef Py_ssize_t i
    cdef Vector3 *v1
    cdef Vector3 *v2

    for i in range(len(edges)):
        v1 = vertices.data + edges.data[i].v1
        v2 = vertices.data + edges.data[i].v2
        centers.data[i].x = (v1.x + v2.x) / 2
        centers.data[i].y = (v1.y + v2.y) / 2
        centers.data[i].z = (v1.z + v2.z) / 2

    return centers

def getEdgeStartPoints(Vector3DList vertices, EdgeIndicesList edges):
    return getEdgePoints(vertices, edges, 0)

def getEdgeEndPoints(Vector3DList vertices, EdgeIndicesList edges):
    return getEdgePoints(vertices, edges, 1)

def getEdgePoints(Vector3DList vertices, EdgeIndicesList edges, int index):
    ensureValidEdges(vertices, edges)
    assert index in (0, 1)

    cdef:
        Vector3DList points = Vector3DList(length = len(edges))
        Py_ssize_t i

    if index == 0:
        for i in range(len(points)):
            points.data[i] = vertices.data[edges.data[i].v1]
    elif index == 1:
        for i in range(len(points)):
            points.data[i] = vertices.data[edges.data[i].v2]
    return points

def ensureValidEdges(Vector3DList vertices, EdgeIndicesList edges):
    if len(edges) == 0:
        return Vector3DList()
    if edges.getMaxIndex() >= len(vertices):
        raise IndexError("Edges are invalid")


# Polygon Operations
######################################################

def polygonIndicesListFromVertexAmounts(LongList vertexAmounts):
    cdef:
        cdef long i, polyLength, currentStart
        long totalLength = vertexAmounts.getSumOfElements()
        long polygonAmount = len(vertexAmounts)
        PolygonIndicesList polygonIndices

    polygonIndices = PolygonIndicesList(
        indicesAmount = totalLength,
        polygonAmount = polygonAmount)

    for i in range(totalLength):
        polygonIndices.indices.data[i] = i

    currentStart = 0
    for i in range(polygonAmount):
        polyLength = vertexAmounts.data[i]
        if polyLength >= 3:
            polygonIndices.polyStarts.data[i] = currentStart
            polygonIndices.polyLengths.data[i] = polyLength
            currentStart += polyLength
        else:
            raise Exception("vertex amount < 3")

    return polygonIndices

def transformPolygons(Vector3DList vertices, PolygonIndicesList polygons, Matrix4x4List matrices):
    cdef:
        Matrix4 *_matrices = matrices.data
        Matrix4 *matrix
        long i, j
        long start, length
        Vector3 *_vertices = vertices.data
        unsigned int *_polyStarts = polygons.polyStarts.data
        unsigned int *_polyLengths = polygons.polyLengths.data
        unsigned int *_indices = polygons.indices.data

    for i in range(matrices.length):
        matrix = _matrices + i
        start = _polyStarts[i]
        length = _polyLengths[i]
        for j in range(length):
            transformVec3AsPoint_InPlace(_vertices + _indices[start + j], matrix)

def separatePolygons(Vector3DList oldVertices, PolygonIndicesList oldPolygons):
    cdef Vector3DList newVertices
    cdef PolygonIndicesList newPolygons

    newVertices = Vector3DList(length = oldPolygons.indices.length)
    newPolygons = PolygonIndicesList(indicesAmount = oldPolygons.indices.length,
                                     polygonAmount = oldPolygons.getLength())

    memcpy(newPolygons.polyStarts.data,
           oldPolygons.polyStarts.data,
           oldPolygons.polyStarts.length * oldPolygons.polyStarts.getElementSize())

    memcpy(newPolygons.polyLengths.data,
           oldPolygons.polyLengths.data,
           oldPolygons.polyLengths.length * oldPolygons.polyLengths.getElementSize())

    cdef:
        long i
        Vector3* _oldVertices = oldVertices.data
        Vector3* _newVertices = newVertices.data
        unsigned int* _oldIndices = oldPolygons.indices.data
        unsigned int* _newIndices = newPolygons.indices.data

    for i in range(oldPolygons.indices.length):
        _newIndices[i] = i
        _newVertices[i] = _oldVertices[_oldIndices[i]]

    return newVertices, newPolygons


# Extract Polygon Transforms
###########################################

def extractPolygonTransforms(Vector3DList vertices, PolygonIndicesList polygons,
                             bint calcNormal = True, bint calcInverted = False):
    if not calcNormal and not calcInverted:
        return None

    cdef Py_ssize_t i
    cdef Vector3 center, normal, tangent, bitangent
    cdef Matrix4x4List transforms, invertedTransforms

    if calcNormal:
        transforms = Matrix4x4List(length = polygons.getLength())
    if calcInverted:
        invertedTransforms = Matrix4x4List(length = polygons.getLength())

    for i in range(transforms.length):
        extractPolygonData(
            vertices.data,
            polygons.indices.data + polygons.polyStarts.data[i],
            polygons.polyLengths.data[i],
            &center, &normal, &tangent)

        normalizeVec3_InPlace(&normal)
        normalizeVec3_InPlace(&tangent)
        crossVec3(&bitangent, &tangent, &normal)
        scaleVec3_Inplace(&bitangent, -1)

        if calcNormal:
            createMatrix(transforms.data + i, &center, &normal, &tangent, &bitangent)
        if calcInverted:
            createInvertedMatrix(invertedTransforms.data + i, &center, &normal, &tangent, &bitangent)

    if calcNormal and calcInverted:
        return transforms, invertedTransforms
    elif calcNormal:
        return transforms
    else:
        return invertedTransforms

@cython.cdivision(True)
cdef void extractPolygonData(Vector3 *vertices,
                        unsigned int *indices, unsigned int vertexAmount,
                        Vector3 *center, Vector3 *normal, Vector3 *tangent):
    # Center
    cdef Py_ssize_t i
    cdef Vector3 *current
    cdef Vector3 sum = {"x" : 0, "y" : 0, "z" : 0}

    for i in range(vertexAmount):
        current = vertices + indices[i]
        sum.x += current.x
        sum.y += current.y
        sum.z += current.z
    scaleVec3(center, &sum, 1 / <float>vertexAmount)

    # Normal
    cdef Vector3 a, b
    subVec3(&a, vertices + indices[1], vertices + indices[0])
    subVec3(&b, vertices + indices[2], vertices + indices[0])
    crossVec3(normal, &a, &b)

    # Tangent
    tangent[0] = a

cdef void createMatrix(Matrix4 *m, Vector3 *center, Vector3 *normal, Vector3 *tangent, Vector3 *bitangent):
    m.a11, m.a12, m.a13, m.a14 = tangent.x, bitangent.x, normal.x, center.x
    m.a21, m.a22, m.a23, m.a24 = tangent.y, bitangent.y, normal.y, center.y
    m.a31, m.a32, m.a33, m.a34 = tangent.z, bitangent.z, normal.z, center.z
    m.a41, m.a42, m.a43, m.a44 = 0, 0, 0, 1

cdef void createInvertedMatrix(Matrix4 *m, Vector3 *center, Vector3 *normal, Vector3 *tangent, Vector3 *bitangent):
    m.a11, m.a12, m.a13 = tangent.x,   tangent.y,   tangent.z,
    m.a21, m.a22, m.a23 = bitangent.x, bitangent.y, bitangent.z
    m.a31, m.a32, m.a33 = normal.x,    normal.y,    normal.z
    m.a41, m.a42, m.a43, m.a44 = 0, 0, 0, 1

    m.a14 = -(tangent.x   * center.x + tangent.y   * center.y + tangent.z   * center.z)
    m.a24 = -(bitangent.x * center.x + bitangent.y * center.y + bitangent.z * center.z)
    m.a34 = -(normal.x    * center.x + normal.y    * center.y + normal.z    * center.z)


# Edges to Tubes
###########################################

# random vector that is unlikely to be colinear to any input
cdef Vector3 upVector = {"x" : 0.5234643, "y" : 0.9871562, "z" : 0.6132743}
normalizeVec3_InPlace(&upVector)

def edgesToTubes(Vector3DList vertices, EdgeIndicesList edges, radius, Py_ssize_t resolution, bint caps = True):
    if len(edges) > 0 and edges.getMaxIndex() >= vertices.length:
        raise IndexError("invalid edges")

    cdef:
        Vector3DList tubeVertices = cylinder.vertices(radius = 1, height = 1, resolution = resolution)
        PolygonIndicesList tubePolygons = cylinder.polygons(resolution, caps)
        MeshData tubeMesh = MeshData(tubeVertices, None, tubePolygons)
        CDefaultList radii = CDefaultList(DoubleList, radius, 0)
        Matrix4x4List transformations = Matrix4x4List(length = edges.length)
        Py_ssize_t i
        Matrix4 *m
        Vector3 *v1
        Vector3 *v2
        Vector3 xDir, yDir, zDir
        float _radius
        float height

    for i in range(edges.length):
        v1 = vertices.data + edges.data[i].v1
        v2 = vertices.data + edges.data[i].v2
        subVec3(&zDir, v2, v1)
        crossVec3(&xDir, &zDir, &upVector)
        crossVec3(&yDir, &xDir, &zDir)
        _radius = (<double*>radii.get(i))[0]
        normalizeLengthVec3_Inplace(&xDir, _radius)
        normalizeLengthVec3_Inplace(&yDir, _radius)

        m = transformations.data + i
        m.a11, m.a12, m.a13, m.a14 = -xDir.x, yDir.x, zDir.x, v1.x
        m.a21, m.a22, m.a23, m.a24 = -xDir.y, yDir.y, zDir.y, v1.y
        m.a31, m.a32, m.a33, m.a34 = -xDir.z, yDir.z, zDir.z, v1.z
        m.a41, m.a42, m.a43, m.a44 = 0, 0, 0, 1

    result = replicateMeshData(tubeMesh, transformations)
    return result.vertices, result.polygons


# Replicate Mesh Data
###################################

def replicateMeshData(MeshData source, transformations):
    cdef:
        Py_ssize_t i, j, offset
        Py_ssize_t amount = len(transformations)
        Py_ssize_t vertexAmount, edgeAmount, polygonAmount, polygonIndicesAmount
        Vector3DList oldVertices, newVertices
        EdgeIndicesList oldEdges, newEdges
        PolygonIndicesList oldPolygons, newPolygons

    oldVertices = source.vertices
    oldEdges = source.edges
    oldPolygons = source.polygons

    vertexAmount = len(oldVertices)
    edgeAmount = len(oldEdges)
    polygonAmount = len(oldPolygons)
    polygonIndicesAmount = len(oldPolygons.indices)

    newVertices = getTransformedVertices(oldVertices, transformations)
    newEdges = EdgeIndicesList(length = amount * edgeAmount)
    newPolygons = PolygonIndicesList(indicesAmount = amount * polygonIndicesAmount,
                                     polygonAmount = amount * polygonAmount)

    # Edges
    for i in range(amount):
        offset = i * edgeAmount
        for j in range(edgeAmount):
            newEdges.data[offset + j].v1 = oldEdges.data[j].v1 + vertexAmount * i
            newEdges.data[offset + j].v2 = oldEdges.data[j].v2 + vertexAmount * i

    # Polygon Indices
    for i in range(amount):
        offset = i * polygonIndicesAmount
        for j in range(polygonIndicesAmount):
            newPolygons.indices.data[offset + j] = oldPolygons.indices.data[j] + vertexAmount * i
    # Polygon Starts
    for i in range(amount):
        offset = i * polygonAmount
        for j in range(polygonAmount):
            newPolygons.polyStarts.data[offset + j] = oldPolygons.polyStarts.data[j] + polygonIndicesAmount * i
    # Polygon Lengths
    for i in range(amount):
        memcpy(newPolygons.polyLengths.data + i * polygonAmount,
               oldPolygons.polyLengths.data,
               sizeof(unsigned int) * polygonAmount)

    return MeshData(newVertices, newEdges, newPolygons)

def getTransformedVertices(Vector3DList oldVertices, transformations):
    if isinstance(transformations, Vector3DList):
        return getTransformedVertices_VectorList(oldVertices, transformations)
    elif isinstance(transformations, Matrix4x4List):
        return getTransformedVertices_MatrixList(oldVertices, transformations)

def getTransformedVertices_VectorList(Vector3DList oldVertices, Vector3DList transformations):
    cdef:
        Py_ssize_t i, j, offset
        Vector3DList newVertices = Vector3DList(oldVertices.length * transformations.length)
        Vector3* _oldVertices = oldVertices.data
        Vector3* _newVertices = newVertices.data
        Vector3* _transformations = transformations.data

    for i in range(transformations.length):
        offset = i * oldVertices.length
        for j in range(oldVertices.length):
            _newVertices[offset + j].x = _oldVertices[j].x + _transformations[i].x
            _newVertices[offset + j].y = _oldVertices[j].y + _transformations[i].y
            _newVertices[offset + j].z = _oldVertices[j].z + _transformations[i].z

    return newVertices

def getTransformedVertices_MatrixList(Vector3DList oldVertices, Matrix4x4List transformations):
    cdef:
        Py_ssize_t i, j, offset
        Vector3DList newVertices = Vector3DList(oldVertices.length * transformations.length)

    for i in range(transformations.length):
        offset = i * oldVertices.length
        for j in range(oldVertices.length):
            transformVec3AsPoint(newVertices.data + offset + j,
                                 oldVertices.data + j,
                                 transformations.data + i)
    return newVertices
