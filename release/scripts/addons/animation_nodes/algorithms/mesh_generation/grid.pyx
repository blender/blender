from ... data_structures cimport Vector3DList, EdgeIndicesList, PolygonIndicesList

# Vertices
############################################

def vertices_Step(float xDistance, float yDistance, int xDivisions, int yDivisions):
    return vertices_Size(xDistance * (xDivisions - 1), yDistance * (yDivisions - 1),
                         xDivisions, yDivisions)

def vertices_Size(float length, float width, int xDivisions, int yDivisions):
    assert xDivisions >= 2
    assert yDivisions >= 2
    cdef:
        float xStep = length / <float>(xDivisions - 1)
        float yStep = width / <float>(yDivisions - 1)
        int x, y

    vertices = Vector3DList(length = xDivisions * yDivisions)

    cdef:
        double xOffset = -length / 2.0
        double yOffset = -width / 2.0

    cdef int rowOffset
    for x in range(xDivisions):
        rowOffset = x * yDivisions
        for y in range(yDivisions):
            vertices.data[rowOffset + y].x = x * xStep + xOffset
            vertices.data[rowOffset + y].y = y * yStep + yOffset
            vertices.data[rowOffset + y].z = 0
    return vertices


# Edges
############################################

def quadEdges(xDivisions, yDivisions, joinHorizontal = False, joinVertical = False):
    edges = EdgeIndicesList()
    edges.extend(innerQuadEdges(xDivisions, yDivisions))
    if joinHorizontal:
        edges.extend(joinHorizontalEdgesQuadEdges(xDivisions, yDivisions))
    if joinVertical:
        edges.extend(joinVerticalEdgesQuadEdges(xDivisions, yDivisions))
    return edges

def innerQuadEdges(long xDivisions, long yDivisions):
    assert xDivisions >= 2
    assert yDivisions >= 2

    cdef EdgeIndicesList edges = EdgeIndicesList(
            length = 2 * xDivisions * yDivisions - xDivisions - yDivisions)

    cdef long index = 0
    cdef long i, j
    for i in range(xDivisions):
        for j in range(yDivisions - 1):
            edges.data[index].v1 = i * yDivisions + j
            edges.data[index].v2 = i * yDivisions + j + 1
            index += 1
    for i in range(yDivisions):
        for j in range(xDivisions - 1):
            edges.data[index].v1 = j * yDivisions + i
            edges.data[index].v2 = j * yDivisions + i + yDivisions
            index += 1
    return edges

def joinHorizontalEdgesQuadEdges(long xDivisions, long yDivisions):
    cdef EdgeIndicesList edges = EdgeIndicesList()
    cdef long i, offset = (xDivisions - 1) * yDivisions
    for i in range(yDivisions):
        edges.append((i, i + offset))
    return edges

def joinVerticalEdgesQuadEdges(long xDivisions, long yDivisions):
    cdef EdgeIndicesList edges = EdgeIndicesList()
    cdef long i
    for i in range(xDivisions):
        edges.append((i * yDivisions, (i + 1) * yDivisions - 1))
    return edges


# Polygons
############################################


def quadPolygons(xDivisions, yDivisions, joinHorizontal = False, joinVertical = False):
    polygons = PolygonIndicesList()
    polygons.extend(innerQuadPolygons(xDivisions, yDivisions))
    if joinHorizontal:
        polygons.extend(joinHorizontalEdgesQuadPolygons(xDivisions, yDivisions))
    if joinVertical:
        polygons.extend(joinVerticalEdgesQuadPolygons(xDivisions, yDivisions))
    if joinHorizontal and joinVertical:
        polygons.append(joinCornersWithQuad(xDivisions, yDivisions))
    return polygons

def innerQuadPolygons(long xDivisions, long yDivisions):
    cdef long polyAmount = (xDivisions - 1) * (yDivisions - 1)
    polygons = PolygonIndicesList(
                    indicesAmount = 4 * polyAmount,
                    polygonAmount = polyAmount)

    cdef long i, j, offset = 0
    for i in range(yDivisions - 1):
        for j in range(xDivisions - 1):
            polygons.polyStarts.data[offset / 4] = offset
            polygons.polyLengths.data[offset / 4] = 4
            polygons.indices.data[offset + 0] = (j + 1) * yDivisions + i
            polygons.indices.data[offset + 1] = (j + 1) * yDivisions + i + 1
            polygons.indices.data[offset + 2] = (j + 0) * yDivisions + i + 1
            polygons.indices.data[offset + 3] = (j + 0) * yDivisions + i
            offset += 4
    return polygons

def joinHorizontalEdgesQuadPolygons(xDivisions, yDivisions):
    polygons = PolygonIndicesList()
    offset = yDivisions * (xDivisions - 1)
    for i in range(yDivisions - 1):
        polygons.append((i + 1, i + offset + 1, i + offset, i))
    return polygons

def joinVerticalEdgesQuadPolygons(xDivisions, yDivisions):
    polygons = PolygonIndicesList()
    for i in range(0, (xDivisions - 1) * yDivisions, yDivisions):
        polygons.append((i + yDivisions - 1, i + 2 * yDivisions - 1, i + yDivisions, i))
    return polygons

def joinCornersWithQuad(xDivisions, yDivisions):
    return (0, yDivisions - 1, yDivisions * xDivisions - 1, yDivisions * (xDivisions - 1))
