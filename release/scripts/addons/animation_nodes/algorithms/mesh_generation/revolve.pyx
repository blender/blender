from . import grid
from libc.math cimport sin, cos
from libc.math cimport M_PI as PI
from ... data_structures cimport Spline, Vector3DList
from ... math cimport Vector3, subVec3, lengthVec3, crossVec3, normalizeVec3_InPlace

# Vertices
####################################################

def vertices(Spline axis not None, Spline profile not None,
             int nSplineSamples, int nSurfaceSamples, str type):

    assert axis.isEvaluable()
    assert profile.isEvaluable()
    assert nSplineSamples >= 2
    assert nSurfaceSamples >= 3

    cdef Vector3DList axisSamples, profileSamples, tangents
    profileSamples = profile.getSamples(nSplineSamples)
    if type == "PARAMETER":
        axisSamples = axis.getSamples(nSplineSamples)
        tangents = profile.getTangentSamples(nSplineSamples)
    elif type == "PROJECT":
        axisSamples = Vector3DList()
        tangents = Vector3DList()
        for point in profileSamples:
            location, tangent = axis.projectExtended(point)
            axisSamples.append(location)
            tangents.append(tangent)

    return tubeVertices(axisSamples, profileSamples, tangents, nSurfaceSamples)

def tubeVertices(Vector3DList centerPoints not None,
                 Vector3DList ringPoints not None,
                 Vector3DList tangents not None, int resolution):
    assert resolution >= 0
    assert len(centerPoints) == len(ringPoints) == len(tangents)

    cdef:
        int i, ringAmount = len(ringPoints)
        Vector3DList vertices = Vector3DList(length = ringAmount * resolution)

    for i in range(ringAmount):
        alignedCircleVertices_LowLevel(
            center =        centerPoints.data + i,
            pointOnCircle = ringPoints.data + i,
            tangent =       tangents.data + i,
            resolution =    resolution,
            output =        vertices.data + i * resolution)

    return vertices

cdef alignedCircleVertices_LowLevel(Vector3* center, Vector3* pointOnCircle,
    	   Vector3* tangent, int resolution, Vector3* output):

    cdef:
        Vector3 dirX, dirY
        float radius, angleStep, angle
        int i

    subVec3(&dirX, pointOnCircle, center)
    crossVec3(&dirY, tangent, &dirX)

    radius = lengthVec3(&dirX)
    normalizeVec3_InPlace(&dirX)
    normalizeVec3_InPlace(&dirY)

    angle = 0
    angleStep = 2 * PI / <float>resolution
    for i in range(resolution):
        output[i].x = center.x + radius * (cos(angle) * dirX.x + sin(angle) * dirY.x)
        output[i].y = center.y + radius * (cos(angle) * dirX.y + sin(angle) * dirY.y)
        output[i].z = center.z + radius * (cos(angle) * dirX.z + sin(angle) * dirY.z)
        angle += angleStep


# Edges
####################################################

def edges(int nSplineSamples, int nSurfaceSamples):
    return grid.quadEdges(nSplineSamples, nSurfaceSamples, joinVertical = True)


# Polygons
####################################################

def polygons(int nSplineSamples, int nSurfaceSamples):
    return grid.quadPolygons(nSplineSamples, nSurfaceSamples, joinVertical = True)
