# -*- coding: utf-8 -*-
# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####


import bpy
import time
import copy

from mathutils import (
        Euler,
        Matrix,
        Vector,
        )
from math import pi, sin, degrees, radians, atan2, copysign, cos, acos
from math import floor
from random import random, uniform, seed, choice, getstate, setstate, randint
from collections import deque, OrderedDict

tau = 2 * pi

# Initialise the split error and axis vectors
splitError = 0.0
zAxis = Vector((0, 0, 1))
yAxis = Vector((0, 1, 0))
xAxis = Vector((1, 0, 0))


# This class will contain a part of the tree which needs to be extended and the required tree parameters
class stemSpline:
    def __init__(self, spline, curvature, curvatureV, attractUp, segments, maxSegs,
                 segLength, childStems, stemRadStart, stemRadEnd, splineNum, ofst, pquat):
        self.spline = spline
        self.p = spline.bezier_points[-1]
        self.curv = curvature
        self.curvV = curvatureV
        self.vertAtt = attractUp
        self.seg = segments
        self.segMax = maxSegs
        self.segL = segLength
        self.children = childStems
        self.radS = stemRadStart
        self.radE = stemRadEnd
        self.splN = splineNum
        self.offsetLen = ofst
        self.patentQuat = pquat
        self.curvSignx = 1
        self.curvSigny = 1

    # This method determines the quaternion of the end of the spline
    def quat(self):
        if len(self.spline.bezier_points) == 1:
            return ((self.spline.bezier_points[-1].handle_right -
                     self.spline.bezier_points[-1].co).normalized()).to_track_quat('Z', 'Y')
        else:
            return ((self.spline.bezier_points[-1].co -
                     self.spline.bezier_points[-2].co).normalized()).to_track_quat('Z', 'Y')

    # Determine the declination
    def dec(self):
        tempVec = zAxis.copy()
        tempVec.rotate(self.quat())
        return zAxis.angle(tempVec)

    # Update the end of the spline and increment the segment count
    def updateEnd(self):
        self.p = self.spline.bezier_points[-1]
        self.seg += 1


# This class contains the data for a point where a new branch will sprout
class childPoint:
    def __init__(self, coords, quat, radiusPar, offset, sOfst, lengthPar, parBone):
        self.co = coords
        self.quat = quat
        self.radiusPar = radiusPar
        self.offset = offset
        self.stemOffset = sOfst
        self.lengthPar = lengthPar
        self.parBone = parBone


# This function calculates the shape ratio as defined in the paper
def shapeRatio(shape, ratio, pruneWidthPeak=0.0, prunePowerHigh=0.0, prunePowerLow=0.0, custom=None):
    if shape == 0:
        return 0.05 + 0.95 * ratio  # 0.2 + 0.8 * ratio
    elif shape == 1:
        return 0.2 + 0.8 * sin(pi * ratio)
    elif shape == 2:
        return 0.2 + 0.8 * sin(0.5 * pi * ratio)
    elif shape == 3:
        return 1.0
    elif shape == 4:
        return 0.5 + 0.5 * ratio
    elif shape == 5:
        if ratio <= 0.7:
            return 0.05 + 0.95 * ratio / 0.7
        else:
            return 0.05 + 0.95 * (1.0 - ratio) / 0.3
    elif shape == 6:
        return 1.0 - 0.8 * ratio
    elif shape == 7:
        if ratio <= 0.7:
            return 0.5 + 0.5 * ratio / 0.7
        else:
            return 0.5 + 0.5 * (1.0 - ratio) / 0.3
    elif shape == 8:
        r = 1 - ratio
        if r == 1:
            v = custom[3]
        elif r >= custom[2]:
            pos = (r - custom[2]) / (1 - custom[2])
            # if (custom[0] >= custom[1] <= custom[3]) or (custom[0] <= custom[1] >= custom[3]):
            pos = pos * pos
            v = (pos * (custom[3] - custom[1])) + custom[1]
        else:
            pos = r / custom[2]
            # if (custom[0] >= custom[1] <= custom[3]) or (custom[0] <= custom[1] >= custom[3]):
            pos = 1 - (1 - pos) * (1 - pos)
            v = (pos * (custom[1] - custom[0])) + custom[0]

        return v

    elif shape == 9:
        if (ratio < (1 - pruneWidthPeak)) and (ratio > 0.0):
            return ((ratio / (1 - pruneWidthPeak))**prunePowerHigh)
        elif (ratio >= (1 - pruneWidthPeak)) and (ratio < 1.0):
            return (((1 - ratio) / pruneWidthPeak)**prunePowerLow)
        else:
            return 0.0

    elif shape == 10:
        return 0.5 + 0.5 * (1 - ratio)


# This function determines the actual number of splits at a given point using the global error
def splits(n):
    global splitError
    nEff = round(n + splitError, 0)
    splitError -= (nEff - n)
    return int(nEff)


def splits2(n):
    r = random()
    if r < n:
        return 1
    else:
        return 0


def splits3(n):
    ni = int(n)
    nf = n - int(n)
    r = random()
    if r < nf:
        return ni + 1
    else:
        return ni + 0


# Determine the declination from a given quaternion
def declination(quat):
    tempVec = zAxis.copy()
    tempVec.rotate(quat)
    tempVec.normalize()
    return degrees(acos(tempVec.z))


# Determines the angle of upward rotation of a segment due to attractUp
def curveUp(attractUp, quat, curveRes):
    tempVec = yAxis.copy()
    tempVec.rotate(quat)
    tempVec.normalize()

    dec = radians(declination(quat))
    curveUpAng = attractUp * dec * abs(tempVec.z) / curveRes
    if (-dec + curveUpAng) < -pi:
        curveUpAng = -pi + dec
    if (dec - curveUpAng) < 0:
        curveUpAng = dec
    return curveUpAng


# Evaluate a bezier curve for the parameter 0<=t<=1 along its length
def evalBez(p1, h1, h2, p2, t):
    return ((1 - t)**3) * p1 + (3 * t * (1 - t)**2) * h1 + (3 * (t**2) * (1 - t)) * h2 + (t**3) * p2


# Evaluate the unit tangent on a bezier curve for t
def evalBezTan(p1, h1, h2, p2, t):
    return (
            (-3 * (1 - t)**2) * p1 + (-6 * t * (1 - t) + 3 * (1 - t)**2) * h1 +
            (-3 * (t**2) + 6 * t * (1 - t)) * h2 + (3 * t**2) * p2
            ).normalized()


# Determine the range of t values along a splines length where child stems are formed
def findChildPoints(stemList, numChild):
    numPoints = sum([len(n.spline.bezier_points) for n in stemList])
    numSplines = len(stemList)
    numSegs = numPoints - numSplines
    numPerSeg = numChild / numSegs
    numMain = round(numPerSeg * stemList[0].segMax, 0)
    return [(a + 1) / (numMain) for a in range(int(numMain))]


def findChildPoints2(stemList, numChild):
    return [(a + 1) / (numChild) for a in range(int(numChild))]


# Find the coordinates, quaternion and radius for each t on the stem
def interpStem1(stem, tVals, lPar, parRad):
    points = stem.spline.bezier_points
    numPoints = len(points)
    checkVal = (stem.segMax - (numPoints - 1)) / stem.segMax
    # Loop through all the parametric values to be determined
    tempList = deque()
    for t in tVals:
        if t == 1.0:
            index = numPoints - 2
            coord = points[-1].co
            quat = (points[-1].handle_right - points[-1].co).to_track_quat('Z', 'Y')
            radius = points[-1].radius

            tempList.append(
                    childPoint(coord, quat, (parRad, radius), t, lPar, 'bone' +
                              (str(stem.splN).rjust(3, '0')) + '.' + (str(index).rjust(3, '0')))
                    )

        elif (t >= checkVal) and (t < 1.0):
            scaledT = (t - checkVal) / ((1 - checkVal) + .0001)
            length = (numPoints - 1) * scaledT
            index = int(length)

            tTemp = length - index
            coord = evalBez(
                        points[index].co, points[index].handle_right,
                        points[index + 1].handle_left, points[index + 1].co, tTemp
                        )
            quat = (
                evalBezTan(
                    points[index].co, points[index].handle_right,
                    points[index + 1].handle_left, points[index + 1].co, tTemp)
                    ).to_track_quat('Z', 'Y')
            # Not sure if this is the parent radius at the child point or parent start radius
            radius = (1 - tTemp) * points[index].radius + tTemp * points[index + 1].radius

            tempList.append(
                    childPoint(
                            coord, quat, (parRad, radius), t, lPar, 'bone' +
                            (str(stem.splN).rjust(3, '0')) + '.' + (str(index).rjust(3, '0')))
                            )
    return tempList


def interpStem(stem, tVals, lPar, parRad, maxOffset, baseSize):
    points = stem.spline.bezier_points
    numSegs = len(points) - 1
    stemLen = stem.segL * numSegs

    checkBottom = stem.offsetLen / maxOffset
    checkTop = checkBottom + (stemLen / maxOffset)

    # Loop through all the parametric values to be determined
    tempList = deque()
    for t in tVals:
        if (t >= checkBottom) and (t <= checkTop) and (t < 1.0):
            scaledT = (t - checkBottom) / (checkTop - checkBottom)
            ofst = ((t - baseSize) / (checkTop - baseSize)) * (1 - baseSize) + baseSize

            length = numSegs * scaledT
            index = int(length)
            tTemp = length - index

            coord = evalBez(
                        points[index].co, points[index].handle_right,
                        points[index + 1].handle_left, points[index + 1].co, tTemp
                        )
            quat = (
                evalBezTan(
                    points[index].co, points[index].handle_right,
                    points[index + 1].handle_left, points[index + 1].co, tTemp
                    )
                ).to_track_quat('Z', 'Y')
            # Not sure if this is the parent radius at the child point or parent start radius
            radius = (1 - tTemp) * points[index].radius + tTemp * points[index + 1].radius

            tempList.append(
                    childPoint(
                        coord, quat, (parRad, radius), t, ofst, lPar,
                        'bone' + (str(stem.splN).rjust(3, '0')) + '.' + (str(index).rjust(3, '0')))
                    )

    # add stem at tip
    index = numSegs - 1
    coord = points[-1].co
    quat = (points[-1].handle_right - points[-1].co).to_track_quat('Z', 'Y')
    radius = points[-1].radius
    tempList.append(
                childPoint(
                        coord, quat, (parRad, radius), 1, 1, lPar,
                        'bone' + (str(stem.splN).rjust(3, '0')) + '.' + (str(index).rjust(3, '0'))
                        )
                    )

    return tempList


# round down bone number
def roundBone(bone, step):
    bone_i = bone[:-3]
    bone_n = int(bone[-3:])
    bone_n = int(bone_n / step) * step
    return bone_i + str(bone_n).rjust(3, '0')


# Convert a list of degrees to radians
def toRad(list):
    return [radians(a) for a in list]


def anglemean(a1, a2, fac):
    x1 = sin(a1)
    y1 = cos(a1)
    x2 = sin(a2)
    y2 = cos(a2)
    x = x1 + (x2 - x1) * fac
    y = y1 + (y2 - y1) * fac
    return atan2(x, y)


# This is the function which extends (or grows) a given stem.
def growSpline(n, stem, numSplit, splitAng, splitAngV, splineList,
               hType, splineToBone, closeTip, kp, splitHeight, outAtt, stemsegL,
               lenVar, taperCrown, boneStep, rotate, rotateV):

    # curv at base
    sCurv = stem.curv
    if (n == 0) and (kp <= splitHeight):
        sCurv = 0.0

    # curveangle = sCurv + (uniform(-stem.curvV, stem.curvV) * kp)
    # curveVar = uniform(-stem.curvV, stem.curvV) * kp
    curveangle = sCurv + (uniform(0, stem.curvV) * kp * stem.curvSignx)
    curveVar = uniform(0, stem.curvV) * kp * stem.curvSigny
    stem.curvSignx *= -1
    stem.curvSigny *= -1

    curveVarMat = Matrix.Rotation(curveVar, 3, 'Y')

    # First find the current direction of the stem
    dir = stem.quat()

    if n == 0:
        adir = zAxis.copy()
        adir.rotate(dir)

        ry = atan2(adir[0], adir[2])
        adir.rotate(Euler((0, -ry, 0)))
        rx = atan2(adir[1], adir[2])

        dir = Euler((-rx, ry, 0), 'XYZ')

    # length taperCrown
    if n == 0:
        dec = declination(dir) / 180
        dec = dec ** 2
        tf = 1 - (dec * taperCrown * 30)
        tf = max(.1, tf)
    else:
        tf = 1.0

    # outward attraction
    if (n > 0) and (kp > 0) and (outAtt > 0):
        p = stem.p.co.copy()
        d = atan2(p[0], -p[1]) + tau
        edir = dir.to_euler('XYZ', Euler((0, 0, d), 'XYZ'))
        d = anglemean(edir[2], d, (kp * outAtt))
        dirv = Euler((edir[0], edir[1], d), 'XYZ')
        dir = dirv.to_quaternion()
    """
    # parent weight
    parWeight = kp * degrees(stem.curvV) * pi
    parWeight = parWeight * kp
    parWeight = kp
    if (n > 1) and (parWeight != 0):
        d1 = zAxis.copy()
        d2 = zAxis.copy()
        d1.rotate(dir)
        d2.rotate(stem.patentQuat)

        x = d1[0] + ((d2[0] - d1[0]) * parWeight)
        y = d1[1] + ((d2[1] - d1[1]) * parWeight)
        z = d1[2] + ((d2[2] - d1[2]) * parWeight)

        d3 = Vector((x, y, z))
        dir = d3.to_track_quat('Z', 'Y')
    """

    # If the stem splits, we need to add new splines etc
    if numSplit > 0:
        # Get the curve data
        cuData = stem.spline.id_data.name
        cu = bpy.data.curves[cuData]

        # calc split angles
        angle = choice([-1, 1]) * (splitAng + uniform(-splitAngV, splitAngV))
        if n > 0:
            # make branches flatter
            angle *= max(1 - declination(dir) / 90, 0) * .67 + .33
        spreadangle = choice([-1, 1]) * (splitAng + uniform(-splitAngV, splitAngV))

        # branchRotMat = Matrix.Rotation(radians(uniform(0, 360)), 3, 'Z')
        if not hasattr(stem, 'rLast'):
            stem.rLast = radians(uniform(0, 360))

        br = rotate[0] + uniform(-rotateV[0], rotateV[0])
        branchRot = stem.rLast + br
        branchRotMat = Matrix.Rotation(branchRot, 3, 'Z')
        stem.rLast = branchRot

        # Now for each split add the new spline and adjust the growth direction
        for i in range(numSplit):
            # find split scale
            lenV = uniform(1 - lenVar, 1 + lenVar)
            bScale = min(lenV * tf, 1)

            newSpline = cu.splines.new('BEZIER')
            newPoint = newSpline.bezier_points[-1]
            (newPoint.co, newPoint.handle_left_type, newPoint.handle_right_type) = (stem.p.co, 'VECTOR', 'VECTOR')
            newPoint.radius = (
                        stem.radS * (1 - stem.seg / stem.segMax) + stem.radE * (stem.seg / stem.segMax)
                        ) * bScale
            # Here we make the new "sprouting" stems diverge from the current direction
            divRotMat = Matrix.Rotation(angle + curveangle, 3, 'X')
            dirVec = zAxis.copy()
            dirVec.rotate(divRotMat)

            # horizontal curvature variation
            dirVec.rotate(curveVarMat)

            if n == 0:  # Special case for trunk splits
                dirVec.rotate(branchRotMat)

                ang = pi - ((tau) / (numSplit + 1)) * (i + 1)
                dirVec.rotate(Matrix.Rotation(ang, 3, 'Z'))

            # Spread the stem out in a random fashion
            spreadMat = Matrix.Rotation(spreadangle, 3, 'Y')
            if n != 0:  # Special case for trunk splits
                dirVec.rotate(spreadMat)

            dirVec.rotate(dir)

            # Introduce upward curvature
            upRotAxis = xAxis.copy()
            upRotAxis.rotate(dirVec.to_track_quat('Z', 'Y'))
            curveUpAng = curveUp(stem.vertAtt, dirVec.to_track_quat('Z', 'Y'), stem.segMax)
            upRotMat = Matrix.Rotation(-curveUpAng, 3, upRotAxis)
            dirVec.rotate(upRotMat)

            # Make the growth vec the length of a stem segment
            dirVec.normalize()

            # split length variation
            stemL = stemsegL * lenV
            dirVec *= stemL * tf
            ofst = stem.offsetLen + (stem.segL * (len(stem.spline.bezier_points) - 1))

            # dirVec *= stem.segL

            # Get the end point position
            end_co = stem.p.co.copy()

            # Add the new point and adjust its coords, handles and radius
            newSpline.bezier_points.add()
            newPoint = newSpline.bezier_points[-1]
            (newPoint.co, newPoint.handle_left_type, newPoint.handle_right_type) = (end_co + dirVec, hType, hType)
            newPoint.radius = (
                        stem.radS * (1 - (stem.seg + 1) / stem.segMax) +
                        stem.radE * ((stem.seg + 1) / stem.segMax)
                        ) * bScale
            if (stem.seg == stem.segMax - 1) and closeTip:
                newPoint.radius = 0.0
            # If this isn't the last point on a stem, then we need to add it
            # to the list of stems to continue growing
            # print(stem.seg != stem.segMax, stem.seg, stem.segMax)
            # if stem.seg != stem.segMax: # if probs not nessesary
            nstem = stemSpline(
                        newSpline, stem.curv, stem.curvV, stem.vertAtt, stem.seg + 1,
                        stem.segMax, stemL, stem.children,
                        stem.radS * bScale, stem.radE * bScale, len(cu.splines) - 1, ofst, stem.quat()
                        )
            nstem.splitlast = 1  # numSplit  # keep track of numSplit for next stem
            nstem.rLast = branchRot + pi
            splineList.append(nstem)
            bone = 'bone' + (str(stem.splN)).rjust(3, '0') + '.' + \
                    (str(len(stem.spline.bezier_points) - 2)).rjust(3, '0')
            bone = roundBone(bone, boneStep[n])
            splineToBone.append((bone, False, True, len(stem.spline.bezier_points) - 2))

        # The original spline also needs to keep growing so adjust its direction too
        divRotMat = Matrix.Rotation(-angle + curveangle, 3, 'X')
        dirVec = zAxis.copy()
        dirVec.rotate(divRotMat)

        # horizontal curvature variation
        dirVec.rotate(curveVarMat)

        if n == 0:  # Special case for trunk splits
            dirVec.rotate(branchRotMat)

        # spread
        spreadMat = Matrix.Rotation(-spreadangle, 3, 'Y')
        if n != 0:  # Special case for trunk splits
            dirVec.rotate(spreadMat)

        dirVec.rotate(dir)

        stem.splitlast = 1  # numSplit #keep track of numSplit for next stem

    else:
        # If there are no splits then generate the growth direction without accounting for spreading of stems
        dirVec = zAxis.copy()
        divRotMat = Matrix.Rotation(curveangle, 3, 'X')
        dirVec.rotate(divRotMat)

        # horizontal curvature variation
        dirVec.rotate(curveVarMat)

        dirVec.rotate(dir)

        stem.splitlast = 0  # numSplit #keep track of numSplit for next stem

    # Introduce upward curvature
    upRotAxis = xAxis.copy()
    upRotAxis.rotate(dirVec.to_track_quat('Z', 'Y'))
    curveUpAng = curveUp(stem.vertAtt, dirVec.to_track_quat('Z', 'Y'), stem.segMax)
    upRotMat = Matrix.Rotation(-curveUpAng, 3, upRotAxis)
    dirVec.rotate(upRotMat)

    dirVec.normalize()
    dirVec *= stem.segL * tf

    # Get the end point position
    end_co = stem.p.co.copy()

    stem.spline.bezier_points.add()
    newPoint = stem.spline.bezier_points[-1]
    (newPoint.co, newPoint.handle_left_type, newPoint.handle_right_type) = (end_co + dirVec, hType, hType)
    newPoint.radius = stem.radS * (1 - (stem.seg + 1) / stem.segMax) + \
                      stem.radE * ((stem.seg + 1) / stem.segMax)

    if (stem.seg == stem.segMax - 1) and closeTip:
        newPoint.radius = 0.0
    # There are some cases where a point cannot have handles as VECTOR straight away, set these now
    if len(stem.spline.bezier_points) == 2:
        tempPoint = stem.spline.bezier_points[0]
        (tempPoint.handle_left_type, tempPoint.handle_right_type) = ('VECTOR', 'VECTOR')
    # Update the last point in the spline to be the newly added one
    stem.updateEnd()
    # return splineList


def genLeafMesh(leafScale, leafScaleX, leafScaleT, leafScaleV, loc, quat,
                offset, index, downAngle, downAngleV, rotate, rotateV, oldRot,
                bend, leaves, leafShape, leafangle, horzLeaves):
    if leafShape == 'hex':
        verts = [
            Vector((0, 0, 0)), Vector((0.5, 0, 1 / 3)), Vector((0.5, 0, 2 / 3)),
            Vector((0, 0, 1)), Vector((-0.5, 0, 2 / 3)), Vector((-0.5, 0, 1 / 3))
            ]
        edges = [[0, 1], [1, 2], [2, 3], [3, 4], [4, 5], [5, 0], [0, 3]]
        faces = [[0, 1, 2, 3], [0, 3, 4, 5]]
    elif leafShape == 'rect':
        # verts = [Vector((1, 0, 0)), Vector((1, 0, 1)), Vector((-1, 0, 1)), Vector((-1, 0, 0))]
        verts = [Vector((.5, 0, 0)), Vector((.5, 0, 1)), Vector((-.5, 0, 1)), Vector((-.5, 0, 0))]
        edges = [[0, 1], [1, 2], [2, 3], [3, 0]]
        faces = [[0, 1, 2, 3]]
    elif leafShape == 'dFace':
        verts = [Vector((.5, .5, 0)), Vector((.5, -.5, 0)), Vector((-.5, -.5, 0)), Vector((-.5, .5, 0))]
        edges = [[0, 1], [1, 2], [2, 3], [3, 0]]
        faces = [[0, 3, 2, 1]]
    elif leafShape == 'dVert':
        verts = [Vector((0, 0, 1))]
        edges = []
        faces = []

    vertsList = []
    facesList = []
    normal = Vector((0, 0, 1))

    if leaves < 0:
        rotMat = Matrix.Rotation(oldRot, 3, 'Y')
    else:
        rotMat = Matrix.Rotation(oldRot, 3, 'Z')

    # If the -ve flag for rotate is used we need to find which side of the stem
    # the last child point was and then grow in the opposite direction
    if rotate < 0.0:
        oldRot = -copysign(rotate + uniform(-rotateV, rotateV), oldRot)
    else:
        # If the special -ve flag for leaves is used we need a different rotation of the leaf geometry
        if leaves == -1:
            # oldRot = 0
            rotMat = Matrix.Rotation(0, 3, 'Y')
        elif leaves < -1:
            oldRot += rotate / (-leaves - 1)
        else:
            oldRot += rotate + uniform(-rotateV, rotateV)
    """
    if leaves < 0:
        rotMat = Matrix.Rotation(oldRot, 3, 'Y')
    else:
        rotMat = Matrix.Rotation(oldRot, 3, 'Z')
    """
    if leaves >= 0:
        # downRotMat = Matrix.Rotation(downAngle+uniform(-downAngleV, downAngleV), 3, 'X')

        if downAngleV > 0.0:
            downV = -downAngleV * offset
        else:
            downV = uniform(-downAngleV, downAngleV)
        downRotMat = Matrix.Rotation(downAngle + downV, 3, 'X')

    # leaf scale variation
    if (leaves < -1) and (rotate != 0):
        f = 1 - abs((oldRot - (rotate / (-leaves - 1))) / (rotate / 2))
    else:
        f = offset

    if leafScaleT < 0:
        leafScale = leafScale * (1 - (1 - f) * -leafScaleT)
    else:
        leafScale = leafScale * (1 - f * leafScaleT)

    leafScale = leafScale * uniform(1 - leafScaleV, 1 + leafScaleV)

    if leafShape == 'dFace':
        leafScale = leafScale * .1

    # If the bending of the leaves is used we need to rotate them differently
    if (bend != 0.0) and (leaves >= 0):
        normal = yAxis.copy()
        orientationVec = zAxis.copy()

        normal.rotate(quat)
        orientationVec.rotate(quat)

        thetaPos = atan2(loc.y, loc.x)
        thetaBend = thetaPos - atan2(normal.y, normal.x)
        rotateZ = Matrix.Rotation(bend * thetaBend, 3, 'Z')
        normal.rotate(rotateZ)
        orientationVec.rotate(rotateZ)

        phiBend = atan2((normal.xy).length, normal.z)
        orientation = atan2(orientationVec.y, orientationVec.x)
        rotateZOrien = Matrix.Rotation(orientation, 3, 'X')

        rotateX = Matrix.Rotation(bend * phiBend, 3, 'Z')

        rotateZOrien2 = Matrix.Rotation(-orientation, 3, 'X')

    # For each of the verts we now rotate and scale them, then append them to the list to be added to the mesh
    for v in verts:
        v.z *= leafScale
        v.y *= leafScale
        v.x *= leafScaleX * leafScale

        v.rotate(Euler((0, 0, radians(180))))

        # leafangle
        v.rotate(Matrix.Rotation(radians(-leafangle), 3, 'X'))

        if rotate < 0:
            v.rotate(Euler((0, 0, radians(90))))
            if oldRot < 0:
                v.rotate(Euler((0, 0, radians(180))))

        if (leaves > 0) and (rotate > 0) and horzLeaves:
            nRotMat = Matrix.Rotation(-oldRot + rotate, 3, 'Z')
            v.rotate(nRotMat)

        if leaves > 0:
            v.rotate(downRotMat)

        v.rotate(rotMat)
        v.rotate(quat)

        if (bend != 0.0) and (leaves > 0):
            # Correct the rotation
            v.rotate(rotateZ)
            v.rotate(rotateZOrien)
            v.rotate(rotateX)
            v.rotate(rotateZOrien2)

    if leafShape == 'dVert':
        normal = verts[0]
        normal.normalize()
        v = loc
        vertsList.append([v.x, v.y, v.z])
    else:
        for v in verts:
            v += loc
            vertsList.append([v.x, v.y, v.z])
        for f in faces:
            facesList.append([f[0] + index, f[1] + index, f[2] + index, f[3] + index])

    return vertsList, facesList, normal, oldRot


def create_armature(armAnim, leafP, cu, frameRate, leafMesh, leafObj, leafVertSize, leaves,
                    levelCount, splineToBone, treeOb, wind, gust, gustF, af1, af2, af3,
                    leafAnim, loopFrames, previewArm, armLevels, makeMesh, boneStep):
    arm = bpy.data.armatures.new('tree')
    armOb = bpy.data.objects.new('treeArm', arm)
    bpy.context.scene.objects.link(armOb)
    # Create a new action to store all animation
    newAction = bpy.data.actions.new(name='windAction')
    armOb.animation_data_create()
    armOb.animation_data.action = newAction
    arm.draw_type = 'STICK'
    arm.use_deform_delay = True
    # Add the armature modifier to the curve
    armMod = treeOb.modifiers.new('windSway', 'ARMATURE')
    if previewArm:
        armMod.show_viewport = False
        arm.draw_type = 'WIRE'
        treeOb.hide = True
    armMod.use_apply_on_spline = True
    armMod.object = armOb
    armMod.use_bone_envelopes = True
    armMod.use_vertex_groups = False  # curves don't have vertex groups (yet)
    # If there are leaves then they need a modifier
    if leaves:
        armMod = leafObj.modifiers.new('windSway', 'ARMATURE')
        armMod.object = armOb
        armMod.use_bone_envelopes = False
        armMod.use_vertex_groups = True
    # Make sure all objects are deselected (may not be required?)
    for ob in bpy.data.objects:
        ob.select = False

    fps = bpy.context.scene.render.fps
    animSpeed = (24 / fps) * frameRate

    # Set the armature as active and go to edit mode to add bones
    bpy.context.scene.objects.active = armOb
    bpy.ops.object.mode_set(mode='EDIT')
    # For all the splines in the curve we need to add bones at each bezier point
    for i, parBone in enumerate(splineToBone):
        if (i < levelCount[armLevels]) or (armLevels == -1) or (not makeMesh):
            s = cu.splines[i]
            b = None
            # Get some data about the spline like length and number of points
            numPoints = len(s.bezier_points) - 1

            # find branching level
            level = 0
            for l, c in enumerate(levelCount):
                if i < c:
                    level = l
                    break
            level = min(level, 3)

            step = boneStep[level]

            # Calculate things for animation
            if armAnim:
                splineL = numPoints * ((s.bezier_points[0].co - s.bezier_points[1].co).length)
                # Set the random phase difference of the animation
                bxOffset = uniform(0, tau)
                byOffset = uniform(0, tau)
                # Set the phase multiplier for the spline
                # bMult_r = (s.bezier_points[0].radius / max(splineL, 1e-6)) * (1 / 15) * (1 / frameRate)
                # This shouldn't have to be in degrees but it looks much better in animation
                # bMult = degrees(bMult_r)
                bMult = (1 / max(splineL ** .5, 1e-6)) * (1 / 4)
                # print((1 / bMult) * tau) #print wavelength in frames

                windFreq1 = bMult * animSpeed
                windFreq2 = 0.7 * bMult * animSpeed
                if loopFrames != 0:
                    bMult_l = 1 / (loopFrames / tau)
                    fRatio = max(1, round(windFreq1 / bMult_l))
                    fgRatio = max(1, round(windFreq2 / bMult_l))
                    windFreq1 = fRatio * bMult_l
                    windFreq2 = fgRatio * bMult_l

            # For all the points in the curve (less the last) add a bone and name it by the spline it will affect
            nx = 0
            for n in range(0, numPoints, step):
                oldBone = b
                boneName = 'bone' + (str(i)).rjust(3, '0') + '.' + (str(n)).rjust(3, '0')
                b = arm.edit_bones.new(boneName)
                b.head = s.bezier_points[n].co
                nx += step
                nx = min(nx, numPoints)
                b.tail = s.bezier_points[nx].co

                b.head_radius = s.bezier_points[n].radius
                b.tail_radius = s.bezier_points[n + 1].radius
                b.envelope_distance = 0.001
                """
                # If there are leaves then we need a new vertex group so they will attach to the bone
                if not leafAnim:
                    if (len(levelCount) > 1) and (i >= levelCount[-2]) and leafObj:
                        leafObj.vertex_groups.new(boneName)
                    elif (len(levelCount) == 1) and leafObj:
                        leafObj.vertex_groups.new(boneName)
                """
                # If this is first point of the spline then it must be parented to the level above it
                if n == 0:
                    if parBone:
                        b.parent = arm.edit_bones[parBone]
                # Otherwise, we need to attach it to the previous bone in the spline
                else:
                    b.parent = oldBone
                    b.use_connect = True
                # If there isn't a previous bone then it shouldn't be attached
                if not oldBone:
                    b.use_connect = False

                # Add the animation to the armature if required
                if armAnim:
                    # Define all the required parameters of the wind sway by the dimension of the spline
                    # a0 = 4 * splineL * (1 - n / (numPoints + 1)) / max(s.bezier_points[n].radius, 1e-6)
                    a0 = 2 * (splineL / numPoints) * (1 - n / (numPoints + 1)) / max(s.bezier_points[n].radius, 1e-6)
                    a0 = a0 * min(step, numPoints)
                    # a0 = (splineL / numPoints) / max(s.bezier_points[n].radius, 1e-6)
                    a1 = (wind / 50) * a0
                    a2 = a1 * .65  # (windGust / 50) * a0 + a1 / 2

                    p = s.bezier_points[nx].co - s.bezier_points[n].co
                    p.normalize()
                    ag = (wind * gust / 50) * a0
                    a3 = -p[0] * ag
                    a4 = p[2] * ag

                    a1 = radians(a1)
                    a2 = radians(a2)
                    a3 = radians(a3)
                    a4 = radians(a4)

                    # wind bending
                    if loopFrames == 0:
                        swayFreq = gustF * (tau / fps) * frameRate  # animSpeed # .075 # 0.02
                    else:
                        swayFreq = 1 / (loopFrames / tau)

                    # Prevent tree base from rotating
                    if (boneName == "bone000.000") or (boneName == "bone000.001"):
                        a1 = 0
                        a2 = 0
                        a3 = 0
                        a4 = 0

                    # Add new fcurves for each sway as well as the modifiers
                    swayX = armOb.animation_data.action.fcurves.new(
                                            'pose.bones["' + boneName + '"].rotation_euler', 0
                                            )
                    swayY = armOb.animation_data.action.fcurves.new(
                                            'pose.bones["' + boneName + '"].rotation_euler', 2
                                            )
                    swayXMod1 = swayX.modifiers.new(type='FNGENERATOR')
                    swayXMod2 = swayX.modifiers.new(type='FNGENERATOR')

                    swayYMod1 = swayY.modifiers.new(type='FNGENERATOR')
                    swayYMod2 = swayY.modifiers.new(type='FNGENERATOR')

                    # Set the parameters for each modifier
                    swayXMod1.amplitude = a1
                    swayXMod1.phase_offset = bxOffset
                    swayXMod1.phase_multiplier = windFreq1

                    swayXMod2.amplitude = a2
                    swayXMod2.phase_offset = 0.7 * bxOffset
                    swayXMod2.phase_multiplier = windFreq2
                    swayXMod2.use_additive = True

                    swayYMod1.amplitude = a1
                    swayYMod1.phase_offset = byOffset
                    swayYMod1.phase_multiplier = windFreq1

                    swayYMod2.amplitude = a2
                    swayYMod2.phase_offset = 0.7 * byOffset
                    swayYMod2.phase_multiplier = windFreq2
                    swayYMod2.use_additive = True

                    # wind bending
                    swayYMod3 = swayY.modifiers.new(type='FNGENERATOR')
                    swayYMod3.amplitude = a3
                    swayYMod3.phase_multiplier = swayFreq
                    swayYMod3.value_offset = .6 * a3
                    swayYMod3.use_additive = True

                    swayXMod3 = swayX.modifiers.new(type='FNGENERATOR')
                    swayXMod3.amplitude = a4
                    swayXMod3.phase_multiplier = swayFreq
                    swayXMod3.value_offset = .6 * a4
                    swayXMod3.use_additive = True

    if leaves:
        bonelist = [b.name for b in arm.edit_bones]
        vertexGroups = OrderedDict()
        for i, cp in enumerate(leafP):
            # find leafs parent bone
            leafParent = roundBone(cp.parBone, boneStep[armLevels])
            idx = int(leafParent[4:-4])
            while leafParent not in bonelist:
                # find parent bone of parent bone
                leafParent = splineToBone[idx]
                idx = int(leafParent[4:-4])

            if leafAnim:
                bname = "leaf" + str(i)
                b = arm.edit_bones.new(bname)
                b.head = cp.co
                b.tail = cp.co + Vector((0, 0, .02))
                b.envelope_distance = 0.0
                b.parent = arm.edit_bones[leafParent]

                vertexGroups[bname] = [
                                    v.index for v in
                                    leafMesh.vertices[leafVertSize * i:(leafVertSize * i + leafVertSize)]
                                    ]

                if armAnim:
                    # Define all the required parameters of the wind sway by the dimension of the spline
                    a1 = wind * .25
                    a1 *= af1

                    bMult = (1 / animSpeed) * 6
                    bMult *= 1 / max(af2, .001)

                    ofstRand = af3
                    bxOffset = uniform(-ofstRand, ofstRand)
                    byOffset = uniform(-ofstRand, ofstRand)

                    # Add new fcurves for each sway as well as the modifiers
                    swayX = armOb.animation_data.action.fcurves.new(
                                                'pose.bones["' + bname + '"].rotation_euler', 0
                                                )
                    swayY = armOb.animation_data.action.fcurves.new(
                                                'pose.bones["' + bname + '"].rotation_euler', 2
                                                )
                    # Add keyframe so noise works
                    swayX.keyframe_points.add()
                    swayY.keyframe_points.add()
                    swayX.keyframe_points[0].co = (0, 0)
                    swayY.keyframe_points[0].co = (0, 0)

                    # Add noise modifiers
                    swayXMod = swayX.modifiers.new(type='NOISE')
                    swayYMod = swayY.modifiers.new(type='NOISE')

                    if loopFrames != 0:
                        swayXMod.use_restricted_range = True
                        swayXMod.frame_end = loopFrames
                        swayXMod.blend_in = 4
                        swayXMod.blend_out = 4
                        swayYMod.use_restricted_range = True
                        swayYMod.frame_end = loopFrames
                        swayYMod.blend_in = 4
                        swayYMod.blend_out = 4

                    swayXMod.scale = bMult
                    swayXMod.strength = a1
                    swayXMod.offset = bxOffset

                    swayYMod.scale = bMult
                    swayYMod.strength = a1
                    swayYMod.offset = byOffset

            else:
                if leafParent not in vertexGroups:
                    vertexGroups[leafParent] = []
                vertexGroups[leafParent].extend(
                                        [v.index for v in
                                        leafMesh.vertices[leafVertSize * i:(leafVertSize * i + leafVertSize)]]
                                        )

        for group in vertexGroups:
            leafObj.vertex_groups.new(group)
            leafObj.vertex_groups[group].add(vertexGroups[group], 1.0, 'ADD')

    # Now we need the rotation mode to be 'XYZ' to ensure correct rotation
    bpy.ops.object.mode_set(mode='OBJECT')
    for p in armOb.pose.bones:
        p.rotation_mode = 'XYZ'
    treeOb.parent = armOb


def kickstart_trunk(addstem, levels, leaves, branches, cu, curve, curveRes,
                    curveV, attractUp, length, lengthV, ratio, ratioPower,
                    resU, scale0, scaleV0, scaleVal, taper, minRadius, rootFlare):
    newSpline = cu.splines.new('BEZIER')
    cu.resolution_u = resU
    newPoint = newSpline.bezier_points[-1]
    newPoint.co = Vector((0, 0, 0))
    newPoint.handle_right = Vector((0, 0, 1))
    newPoint.handle_left = Vector((0, 0, -1))
    # (newPoint.handle_right_type, newPoint.handle_left_type) = ('VECTOR', 'VECTOR')
    branchL = scaleVal * length[0]
    curveVal = curve[0] / curveRes[0]
    # curveVal = curveVal * (branchL / scaleVal)
    if levels == 1:
        childStems = leaves
    else:
        childStems = branches[1]
    startRad = scaleVal * ratio * scale0 * uniform(1 - scaleV0, 1 + scaleV0)  # * (scale0 + uniform(-scaleV0, scaleV0))
    endRad = (startRad * (1 - taper[0])) ** ratioPower
    startRad = max(startRad, minRadius)
    endRad = max(endRad, minRadius)
    newPoint.radius = startRad * rootFlare
    addstem(
        stemSpline(
                newSpline, curveVal, curveV[0] / curveRes[0], attractUp[0],
                0, curveRes[0], branchL / curveRes[0],
                childStems, startRad, endRad, 0, 0, None
                )
            )


def fabricate_stems(addsplinetobone, addstem, baseSize, branches, childP, cu, curve, curveBack,
                    curveRes, curveV, attractUp, downAngle, downAngleV, leafDist, leaves, length,
                    lengthV, levels, n, ratioPower, resU, rotate, rotateV, scaleVal, shape, storeN,
                    taper, shapeS, minRadius, radiusTweak, customShape, rMode, segSplits,
                    useOldDownAngle, useParentAngle, boneStep):

    # prevent baseSize from going to 1.0
    baseSize = min(0.999, baseSize)

    # Store the old rotation to allow new stems to be rotated away from the previous one.
    oldRotate = 0

    # use fancy child point selection / rotation
    if (n == 1) and (rMode != "original"):
        childP_T = OrderedDict()
        childP_L = []
        for p in childP:
            if p.offset == 1:
                childP_L.append(p)
            else:
                if p.offset not in childP_T:
                    childP_T[p.offset] = [p]
                else:
                    childP_T[p.offset].append(p)

        childP_T = [childP_T[k] for k in sorted(childP_T.keys())]

        childP = []
        rot_a = []
        for p in childP_T:
            if rMode == "rotate":
                if rotate[n] < 0.0:
                    oldRotate = -copysign(rotate[n], oldRotate)
                else:
                    oldRotate += rotate[n]
                bRotate = oldRotate + uniform(-rotateV[n], rotateV[n])

                # choose start point whose angle is closest to the rotate angle
                a1 = bRotate % tau
                a_diff = []
                for a in p:
                    a2 = atan2(a.co[0], -a.co[1])
                    d = min((a1 - a2 + tau) % tau, (a2 - a1 + tau) % tau)
                    a_diff.append(d)

                idx = a_diff.index(min(a_diff))

                # find actual rotate angle from branch location
                br = p[idx]
                b = br.co
                vx = sin(bRotate)
                vy = cos(bRotate)
                v = Vector((vx, vy))

                bD = ((b[0] * b[0] + b[1] * b[1]) ** .5)
                bL = br.lengthPar * length[1] * shapeRatio(shape, (1 - br.offset) / (1 - baseSize), custom=customShape)

                # account for down angle
                if downAngleV[1] > 0:
                    downA = downAngle[n] + (-downAngleV[n] * (1 - (1 - br.offset) / (1 - baseSize)) ** 2)
                else:
                    downA = downAngle[n]
                if downA < (.5 * pi):
                    downA = sin(downA) ** 2
                    bL *= downA

                bL *= 0.33
                v *= (bD + bL)

                bv = Vector((b[0], -b[1]))
                cv = v - bv
                a = atan2(cv[0], cv[1])
                # rot_a.append(a)
                """
                # add fill points at top  #experimental
                fillHeight = 1 - degrees(rotateV[3]) # 0.8
                if fillHeight < 1:
                    w = (p[0].offset - fillHeight) / (1- fillHeight)
                    prob_b = random() < w
                else:
                    prob_b = False

                if (p[0].offset > fillHeight): # prob_b and (len(p) > 1):  ##(p[0].offset > fillHeight) and
                    childP.append(p[randint(0, len(p)-1)])
                    rot_a.append(bRotate)# + pi)
                """
                childP.append(p[idx])
                rot_a.append(a)

            else:
                idx = randint(0, len(p) - 1)
                childP.append(p[idx])
            # childP.append(p[idx])

        childP.extend(childP_L)
        rot_a.extend([0] * len(childP_L))

        oldRotate = 0

    for i, p in enumerate(childP):
        # Add a spline and set the coordinate of the first point.
        newSpline = cu.splines.new('BEZIER')
        cu.resolution_u = resU
        newPoint = newSpline.bezier_points[-1]
        newPoint.co = p.co
        tempPos = zAxis.copy()
        # If the -ve flag for downAngle is used we need a special formula to find it
        if useOldDownAngle:
            if downAngleV[n] < 0.0:
                downV = downAngleV[n] * (1 - 2 * (.2 + .8 * ((1 - p.offset) / (1 - baseSize))))
            # Otherwise just find a random value
            else:
                downV = uniform(-downAngleV[n], downAngleV[n])
        else:
            if downAngleV[n] < 0.0:
                downV = uniform(-downAngleV[n], downAngleV[n])
            else:
                downV = -downAngleV[n] * (1 - (1 - p.offset) / (1 - baseSize)) ** 2  # (110, 80) = (60, -50)

        if p.offset == 1:
            downRotMat = Matrix.Rotation(0, 3, 'X')
        else:
            downRotMat = Matrix.Rotation(downAngle[n] + downV, 3, 'X')

        # If the -ve flag for rotate is used we need to find which side of the stem
        # the last child point was and then grow in the opposite direction
        if rotate[n] < 0.0:
            oldRotate = -copysign(rotate[n], oldRotate)
        # Otherwise just generate a random number in the specified range
        else:
            oldRotate += rotate[n]
        bRotate = oldRotate + uniform(-rotateV[n], rotateV[n])

        if (n == 1) and (rMode == "rotate"):
            bRotate = rot_a[i]

        rotMat = Matrix.Rotation(bRotate, 3, 'Z')

        # Rotate the direction of growth and set the new point coordinates
        tempPos.rotate(downRotMat)
        tempPos.rotate(rotMat)

        # use quat angle
        if (rMode == "rotate") and (n == 1) and (p.offset != 1):
            if useParentAngle:
                edir = p.quat.to_euler('XYZ', Euler((0, 0, bRotate), 'XYZ'))
                edir[0] = 0
                edir[1] = 0

                edir[2] = -edir[2]
                tempPos.rotate(edir)

                dec = declination(p.quat)
                tempPos.rotate(Matrix.Rotation(radians(dec), 3, 'X'))

                edir[2] = -edir[2]
                tempPos.rotate(edir)
        else:
            tempPos.rotate(p.quat)

        newPoint.handle_right = p.co + tempPos

        # Make length variation inversely proportional to segSplits
        # lenV = (1 - min(segSplits[n], 1)) * lengthV[n]

        # Find branch length and the number of child stems.
        maxbL = scaleVal
        for l in length[:n + 1]:
            maxbL *= l
        lMax = length[n]  # * uniform(1 - lenV, 1 + lenV)
        if n == 1:
            lShape = shapeRatio(shape, (1 - p.stemOffset) / (1 - baseSize), custom=customShape)
        else:
            lShape = shapeRatio(shapeS, (1 - p.stemOffset) / (1 - baseSize))
        branchL = p.lengthPar * lMax * lShape
        childStems = branches[min(3, n + 1)] * (0.1 + 0.9 * (branchL / maxbL))

        # If this is the last level before leaves then we need to generate the child points differently
        if (storeN == levels - 1):
            if leaves < 0:
                childStems = False
            else:
                childStems = leaves * (0.1 + 0.9 * (branchL / maxbL)) * shapeRatio(leafDist, (1 - p.offset))

        # print("n=%d, levels=%d, n'=%d, childStems=%s"%(n, levels, storeN, childStems))

        # Determine the starting and ending radii of the stem using the tapering of the stem
        startRad = min((p.radiusPar[0] * ((branchL / p.lengthPar) ** ratioPower)) * radiusTweak[n], p.radiusPar[1])
        if p.offset == 1:
            startRad = p.radiusPar[1]
        endRad = (startRad * (1 - taper[n])) ** ratioPower
        startRad = max(startRad, minRadius)
        endRad = max(endRad, minRadius)
        newPoint.radius = startRad

        # stem curvature
        curveVal = curve[n] / curveRes[n]
        curveVar = curveV[n] / curveRes[n]

        # curveVal = curveVal * (branchL / scaleVal)

        # Add the new stem to list of stems to grow and define which bone it will be parented to
        addstem(
            stemSpline(
                newSpline, curveVal, curveVar, attractUp[n],
                0, curveRes[n], branchL / curveRes[n], childStems,
                startRad, endRad, len(cu.splines) - 1, 0, p.quat
                )
            )

        bone = roundBone(p.parBone, boneStep[n - 1])
        if p.offset == 1:
            isend = True
        else:
            isend = False
        addsplinetobone((bone, isend))


def perform_pruning(baseSize, baseSplits, childP, cu, currentMax, currentMin, currentScale, curve,
                    curveBack, curveRes, deleteSpline, forceSprout, handles, n, oldMax, orginalSplineToBone,
                    originalCo, originalCurv, originalCurvV, originalHandleL, originalHandleR, originalLength,
                    originalSeg, prune, prunePowerHigh, prunePowerLow, pruneRatio, pruneWidth, pruneBase,
                    pruneWidthPeak, randState, ratio, scaleVal, segSplits, splineToBone, splitAngle, splitAngleV,
                    st, startPrune, branchDist, length, splitByLen, closeTip, nrings, splitBias, splitHeight,
                    attractOut, rMode, lengthV, taperCrown, boneStep, rotate, rotateV):
    while startPrune and ((currentMax - currentMin) > 0.005):
        setstate(randState)

        # If the search will halt after this iteration, then set the adjustment of stem
        # length to take into account the pruning ratio
        if (currentMax - currentMin) < 0.01:
            currentScale = (currentScale - 1) * pruneRatio + 1
            startPrune = False
            forceSprout = True
        # Change the segment length of the stem by applying some scaling
        st.segL = originalLength * currentScale
        # To prevent millions of splines being created we delete any old ones and
        # replace them with only their first points to begin the spline again
        if deleteSpline:
            for x in splineList:
                cu.splines.remove(x.spline)
            newSpline = cu.splines.new('BEZIER')
            newPoint = newSpline.bezier_points[-1]
            newPoint.co = originalCo
            newPoint.handle_right = originalHandleR
            newPoint.handle_left = originalHandleL
            (newPoint.handle_left_type, newPoint.handle_right_type) = ('VECTOR', 'VECTOR')
            st.spline = newSpline
            st.curv = originalCurv
            st.curvV = originalCurvV
            st.seg = originalSeg
            st.p = newPoint
            newPoint.radius = st.radS
            splineToBone = orginalSplineToBone

        # Initialise the spline list for those contained in the current level of branching
        splineList = [st]

        # split length variation
        stemsegL = splineList[0].segL  # initial segment length used for variation
        splineList[0].segL = stemsegL * uniform(1 - lengthV[n], 1 + lengthV[n])  # variation for first stem

        # For each of the segments of the stem which must be grown we have to add to each spline in splineList
        for k in range(curveRes[n]):
            # Make a copy of the current list to avoid continually adding to the list we're iterating over
            tempList = splineList[:]
            # print('Leng: ', len(tempList))

            # for curve variation
            if curveRes[n] > 1:
                kp = (k / (curveRes[n] - 1))  # * 2
            else:
                kp = 1.0

            # split bias
            splitValue = segSplits[n]
            if n == 0:
                splitValue = ((2 * splitBias) * (kp - .5) + 1) * splitValue
                splitValue = max(splitValue, 0.0)

            # For each of the splines in this list set the number of splits and then grow it
            for spl in tempList:
                # adjust numSplit
                lastsplit = getattr(spl, 'splitlast', 0)
                splitVal = splitValue
                if lastsplit == 0:
                    splitVal = splitValue * 1.33
                elif lastsplit == 1:
                    splitVal = splitValue * splitValue

                if k == 0:
                    numSplit = 0
                elif (n == 0) and (k < ((curveRes[n] - 1) * splitHeight)) and (k != 1):
                    numSplit = 0
                elif (k == 1) and (n == 0):
                    numSplit = baseSplits
                # allways split at splitHeight
                elif (n == 0) and (k == int((curveRes[n] - 1) * splitHeight) + 1) and (splitVal > 0):
                    numSplit = 1
                else:
                    if (n >= 1) and splitByLen:
                        L = ((spl.segL * curveRes[n]) / scaleVal)
                        lf = 1
                        for l in length[:n + 1]:
                            lf *= l
                        L = L / lf
                        numSplit = splits2(splitVal * L)
                    else:
                        numSplit = splits2(splitVal)

                if (k == int(curveRes[n] / 2 + 0.5)) and (curveBack[n] != 0):
                    spl.curv += 2 * (curveBack[n] / curveRes[n])  # was -4 *

                growSpline(
                        n, spl, numSplit, splitAngle[n], splitAngleV[n], splineList,
                        handles, splineToBone, closeTip, kp, splitHeight, attractOut[n],
                        stemsegL, lengthV[n], taperCrown, boneStep, rotate, rotateV
                        )

        # If pruning is enabled then we must to the check to see if the end of the spline is within the evelope
        if prune:
            # Check each endpoint to see if it is inside
            for s in splineList:
                coordMag = (s.spline.bezier_points[-1].co.xy).length
                ratio = (scaleVal - s.spline.bezier_points[-1].co.z) / (scaleVal * max(1 - pruneBase, 1e-6))
                # Don't think this if part is needed
                if (n == 0) and (s.spline.bezier_points[-1].co.z < pruneBase * scaleVal):
                    insideBool = True  # Init to avoid UnboundLocalError later
                else:
                    insideBool = (
                    (coordMag / scaleVal) < pruneWidth * shapeRatio(9, ratio, pruneWidthPeak, prunePowerHigh,
                                                                    prunePowerLow))
                # If the point is not inside then we adjust the scale and current search bounds
                if not insideBool:
                    oldMax = currentMax
                    currentMax = currentScale
                    currentScale = 0.5 * (currentMax + currentMin)
                    break
            # If the scale is the original size and the point is inside then
            # we need to make sure it won't be pruned or extended to the edge of the envelope
            if insideBool and (currentScale != 1):
                currentMin = currentScale
                currentMax = oldMax
                currentScale = 0.5 * (currentMax + currentMin)
            if insideBool and ((currentMax - currentMin) == 1):
                currentMin = 1

        # If the search will halt on the next iteration then we need
        # to make sure we sprout child points to grow the next splines or leaves
        if (((currentMax - currentMin) < 0.005) or not prune) or forceSprout:
            if (n == 0) and (rMode != "original"):
                tVals = findChildPoints2(splineList, st.children)
            else:
                tVals = findChildPoints(splineList, st.children)
            # print("debug tvals[%d] , splineList[%d], %s" % ( len(tVals), len(splineList), st.children))
            # If leaves is -ve then we need to make sure the only point which sprouts is the end of the spline
            if not st.children:
                tVals = [1.0]
            # remove some of the points because of baseSize
            trimNum = int(baseSize * (len(tVals) + 1))
            tVals = tVals[trimNum:]

            # grow branches in rings
            if (n == 0) and (nrings > 0):
                # tVals = [(floor(t * nrings)) / nrings for t in tVals[:-1]]
                tVals = [(floor(t * nrings) / nrings) * uniform(.995, 1.005) for t in tVals[:-1]]
                tVals.append(1)
                tVals = [t for t in tVals if t > baseSize]

            # branch distribution
            if n == 0:
                tVals = [((t - baseSize) / (1 - baseSize)) for t in tVals]
                if branchDist < 1.0:
                    tVals = [t ** (1 / branchDist) for t in tVals]
                else:
                    tVals = [1 - (1 - t) ** branchDist for t in tVals]
                tVals = [t * (1 - baseSize) + baseSize for t in tVals]

            # For all the splines, we interpolate them and add the new points to the list of child points
            maxOffset = max([s.offsetLen + (len(s.spline.bezier_points) - 1) * s.segL for s in splineList])
            for s in splineList:
                # print(str(n)+'level: ', s.segMax*s.segL)
                childP.extend(interpStem(s, tVals, s.segMax * s.segL, s.radS, maxOffset, baseSize))

        # Force the splines to be deleted
        deleteSpline = True
        # If pruning isn't enabled then make sure it doesn't loop
        if not prune:
            startPrune = False
    return ratio, splineToBone


# calculate taper automaticly
def findtaper(length, taper, shape, shapeS, levels, customShape):
    taperS = []
    for i, t in enumerate(length):
        if i == 0:
            shp = 1.0
        elif i == 1:
            shp = shapeRatio(shape, 0, custom=customShape)
        else:
            shp = shapeRatio(shapeS, 0)
        t = t * shp
        taperS.append(t)

    taperP = []
    for i, t in enumerate(taperS):
        pm = 1
        for x in range(i + 1):
            pm *= taperS[x]
        taperP.append(pm)

    taperR = []
    for i, t in enumerate(taperP):
        t = sum(taperP[i:levels])
        taperR.append(t)

    taperT = []
    for i, t in enumerate(taperR):
        try:
            t = taperP[i] / taperR[i]
        except ZeroDivisionError:
            t = 1.0
        taperT.append(t)

    taperT = [t * taper[i] for i, t in enumerate(taperT)]

    return taperT


def addTree(props):
    global splitError
    # startTime = time.time()
    # Set the seed for repeatable results
    seed(props.seed)

    # Set all other variables
    levels = props.levels
    length = props.length
    lengthV = props.lengthV
    taperCrown = props.taperCrown
    branches = props.branches
    curveRes = props.curveRes
    curve = toRad(props.curve)
    curveV = toRad(props.curveV)
    curveBack = toRad(props.curveBack)
    baseSplits = props.baseSplits
    segSplits = props.segSplits
    splitByLen = props.splitByLen
    rMode = props.rMode
    splitAngle = toRad(props.splitAngle)
    splitAngleV = toRad(props.splitAngleV)
    scale = props.scale
    scaleV = props.scaleV
    attractUp = props.attractUp
    attractOut = props.attractOut
    shape = int(props.shape)
    shapeS = int(props.shapeS)
    customShape = props.customShape
    branchDist = props.branchDist
    nrings = props.nrings
    baseSize = props.baseSize
    baseSize_s = props.baseSize_s
    splitHeight = props.splitHeight
    splitBias = props.splitBias
    ratio = props.ratio
    minRadius = props.minRadius
    closeTip = props.closeTip
    rootFlare = props.rootFlare
    autoTaper = props.autoTaper
    taper = props.taper
    radiusTweak = props.radiusTweak
    ratioPower = props.ratioPower
    downAngle = toRad(props.downAngle)
    downAngleV = toRad(props.downAngleV)
    rotate = toRad(props.rotate)
    rotateV = toRad(props.rotateV)
    scale0 = props.scale0
    scaleV0 = props.scaleV0
    prune = props.prune
    pruneWidth = props.pruneWidth
    pruneBase = props.pruneBase
    pruneWidthPeak = props.pruneWidthPeak
    prunePowerLow = props.prunePowerLow
    prunePowerHigh = props.prunePowerHigh
    pruneRatio = props.pruneRatio
    leafDownAngle = radians(props.leafDownAngle)
    leafDownAngleV = radians(props.leafDownAngleV)
    leafRotate = radians(props.leafRotate)
    leafRotateV = radians(props.leafRotateV)
    leafScale = props.leafScale
    leafScaleX = props.leafScaleX
    leafScaleT = props.leafScaleT
    leafScaleV = props.leafScaleV
    leafShape = props.leafShape
    leafDupliObj = props.leafDupliObj
    bend = props.bend
    leafangle = props.leafangle
    horzLeaves = props.horzLeaves
    leafDist = int(props.leafDist)
    bevelRes = props.bevelRes
    resU = props.resU

    useArm = props.useArm
    previewArm = props.previewArm
    armAnim = props.armAnim
    leafAnim = props.leafAnim
    frameRate = props.frameRate
    loopFrames = props.loopFrames

    # windSpeed = props.windSpeed
    # windGust = props.windGust

    wind = props.wind
    gust = props.gust
    gustF = props.gustF

    af1 = props.af1
    af2 = props.af2
    af3 = props.af3

    makeMesh = props.makeMesh
    armLevels = props.armLevels
    boneStep = props.boneStep

    useOldDownAngle = props.useOldDownAngle
    useParentAngle = props.useParentAngle

    if not makeMesh:
        boneStep = [1, 1, 1, 1]

    # taper
    if autoTaper:
        taper = findtaper(length, taper, shape, shapeS, levels, customShape)
        # pLevels = branches[0]
        # taper = findtaper(length, taper, shape, shapeS, pLevels, customShape)

    leafObj = None

    # Some effects can be turned ON and OFF, the necessary variables are changed here
    if not props.bevel:
        bevelDepth = 0.0
    else:
        bevelDepth = 1.0

    if not props.showLeaves:
        leaves = 0
    else:
        leaves = props.leaves

    if props.handleType == '0':
        handles = 'AUTO'
    else:
        handles = 'VECTOR'

    for ob in bpy.data.objects:
        ob.select = False

    # Initialise the tree object and curve and adjust the settings
    cu = bpy.data.curves.new('tree', 'CURVE')
    treeOb = bpy.data.objects.new('tree', cu)
    bpy.context.scene.objects.link(treeOb)

    # treeOb.location=bpy.context.scene.cursor_location attractUp

    cu.dimensions = '3D'
    cu.fill_mode = 'FULL'
    cu.bevel_depth = bevelDepth
    cu.bevel_resolution = bevelRes
    cu.use_uv_as_generated = True

    # Fix the scale of the tree now
    scaleVal = scale + uniform(-scaleV, scaleV)
    scaleVal += copysign(1e-6, scaleVal)  # Move away from zero to avoid div by zero

    pruneBase = min(pruneBase, baseSize)
    # If pruning is turned on we need to draw the pruning envelope
    if prune:
        enHandle = 'VECTOR'
        enNum = 128
        enCu = bpy.data.curves.new('envelope', 'CURVE')
        enOb = bpy.data.objects.new('envelope', enCu)
        enOb.parent = treeOb
        bpy.context.scene.objects.link(enOb)
        newSpline = enCu.splines.new('BEZIER')
        newPoint = newSpline.bezier_points[-1]
        newPoint.co = Vector((0, 0, scaleVal))
        (newPoint.handle_right_type, newPoint.handle_left_type) = (enHandle, enHandle)
        # Set the coordinates by varying the z value, envelope will be aligned to the x-axis
        for c in range(enNum):
            newSpline.bezier_points.add()
            newPoint = newSpline.bezier_points[-1]
            ratioVal = (c + 1) / (enNum)
            zVal = scaleVal - scaleVal * (1 - pruneBase) * ratioVal
            newPoint.co = Vector(
                            (
                            scaleVal * pruneWidth *
                            shapeRatio(9, ratioVal, pruneWidthPeak, prunePowerHigh, prunePowerLow),
                            0, zVal
                            )
                        )
            (newPoint.handle_right_type, newPoint.handle_left_type) = (enHandle, enHandle)
        newSpline = enCu.splines.new('BEZIER')
        newPoint = newSpline.bezier_points[-1]
        newPoint.co = Vector((0, 0, scaleVal))
        (newPoint.handle_right_type, newPoint.handle_left_type) = (enHandle, enHandle)
        # Create a second envelope but this time on the y-axis
        for c in range(enNum):
            newSpline.bezier_points.add()
            newPoint = newSpline.bezier_points[-1]
            ratioVal = (c + 1) / (enNum)
            zVal = scaleVal - scaleVal * (1 - pruneBase) * ratioVal
            newPoint.co = Vector(
                            (
                            0, scaleVal * pruneWidth *
                            shapeRatio(9, ratioVal, pruneWidthPeak, prunePowerHigh, prunePowerLow),
                            zVal
                            )
                        )
            (newPoint.handle_right_type, newPoint.handle_left_type) = (enHandle, enHandle)

    childP = []
    stemList = []

    levelCount = []
    splineToBone = deque([''])
    addsplinetobone = splineToBone.append

    # Each of the levels needed by the user we grow all the splines
    for n in range(levels):
        storeN = n
        stemList = deque()
        addstem = stemList.append
        # If n is used as an index to access parameters for the tree
        # it must be at most 3 or it will reference outside the array index
        n = min(3, n)
        splitError = 0.0

        # closeTip only on last level
        closeTipp = all([(n == levels - 1), closeTip])

        # If this is the first level of growth (the trunk) then we need some special work to begin the tree
        if n == 0:
            kickstart_trunk(addstem, levels, leaves, branches, cu, curve, curveRes,
                            curveV, attractUp, length, lengthV, ratio, ratioPower, resU,
                            scale0, scaleV0, scaleVal, taper, minRadius, rootFlare)
        # If this isn't the trunk then we may have multiple stem to intialise
        else:
            # For each of the points defined in the list of stem starting points we need to grow a stem.
            fabricate_stems(addsplinetobone, addstem, baseSize, branches, childP, cu, curve, curveBack,
                            curveRes, curveV, attractUp, downAngle, downAngleV, leafDist, leaves, length, lengthV,
                            levels, n, ratioPower, resU, rotate, rotateV, scaleVal, shape, storeN,
                            taper, shapeS, minRadius, radiusTweak, customShape, rMode, segSplits,
                            useOldDownAngle, useParentAngle, boneStep)

        # change base size for each level
        if n > 0:
            baseSize *= baseSize_s  # decrease at each level
        if (n == levels - 1):
            baseSize = 0

        childP = []
        # Now grow each of the stems in the list of those to be extended
        for st in stemList:
            # When using pruning, we need to ensure that the random effects
            # will be the same for each iteration to make sure the problem is linear
            randState = getstate()
            startPrune = True
            lengthTest = 0.0
            # Store all the original values for the stem to make sure
            # we have access after it has been modified by pruning
            originalLength = st.segL
            originalCurv = st.curv
            originalCurvV = st.curvV
            originalSeg = st.seg
            originalHandleR = st.p.handle_right.copy()
            originalHandleL = st.p.handle_left.copy()
            originalCo = st.p.co.copy()
            currentMax = 1.0
            currentMin = 0.0
            currentScale = 1.0
            oldMax = 1.0
            deleteSpline = False
            orginalSplineToBone = copy.copy(splineToBone)
            forceSprout = False
            # Now do the iterative pruning, this uses a binary search and halts once the difference
            # between upper and lower bounds of the search are less than 0.005
            ratio, splineToBone = perform_pruning(
                                        baseSize, baseSplits, childP, cu, currentMax, currentMin,
                                        currentScale, curve, curveBack, curveRes, deleteSpline, forceSprout,
                                        handles, n, oldMax, orginalSplineToBone, originalCo, originalCurv,
                                        originalCurvV, originalHandleL, originalHandleR, originalLength,
                                        originalSeg, prune, prunePowerHigh, prunePowerLow, pruneRatio,
                                        pruneWidth, pruneBase, pruneWidthPeak, randState, ratio, scaleVal,
                                        segSplits, splineToBone, splitAngle, splitAngleV, st, startPrune,
                                        branchDist, length, splitByLen, closeTipp, nrings, splitBias,
                                        splitHeight, attractOut, rMode, lengthV, taperCrown, boneStep,
                                        rotate, rotateV
                                        )

        levelCount.append(len(cu.splines))

    # If we need to add leaves, we do it here
    leafVerts = []
    leafFaces = []
    leafNormals = []

    leafMesh = None  # in case we aren't creating leaves, we'll still have the variable

    leafP = []
    if leaves:
        oldRot = 0.0
        n = min(3, n + 1)
        # For each of the child points we add leaves
        for cp in childP:
            # If the special flag is set then we need to add several leaves at the same location
            if leaves < 0:
                oldRot = -leafRotate / 2
                for g in range(abs(leaves)):
                    (vertTemp, faceTemp, normal, oldRot) = genLeafMesh(
                                                                leafScale, leafScaleX, leafScaleT,
                                                                leafScaleV, cp.co, cp.quat, cp.offset,
                                                                len(leafVerts), leafDownAngle, leafDownAngleV,
                                                                leafRotate, leafRotateV,
                                                                oldRot, bend, leaves, leafShape,
                                                                leafangle, horzLeaves
                                                                )
                    leafVerts.extend(vertTemp)
                    leafFaces.extend(faceTemp)
                    leafNormals.extend(normal)
                    leafP.append(cp)
            # Otherwise just add the leaves like splines
            else:
                (vertTemp, faceTemp, normal, oldRot) = genLeafMesh(
                                                            leafScale, leafScaleX, leafScaleT, leafScaleV,
                                                            cp.co, cp.quat, cp.offset, len(leafVerts),
                                                            leafDownAngle, leafDownAngleV, leafRotate,
                                                            leafRotateV, oldRot, bend, leaves, leafShape,
                                                            leafangle, horzLeaves
                                                            )
                leafVerts.extend(vertTemp)
                leafFaces.extend(faceTemp)
                leafNormals.extend(normal)
                leafP.append(cp)

        # Create the leaf mesh and object, add geometry using from_pydata,
        # edges are currently added by validating the mesh which isn't great
        leafMesh = bpy.data.meshes.new('leaves')
        leafObj = bpy.data.objects.new('leaves', leafMesh)
        bpy.context.scene.objects.link(leafObj)
        leafObj.parent = treeOb
        leafMesh.from_pydata(leafVerts, (), leafFaces)

        # set vertex normals for dupliVerts
        if leafShape == 'dVert':
            leafMesh.vertices.foreach_set('normal', leafNormals)

        # enable duplication
        if leafShape == 'dFace':
            leafObj.dupli_type = "FACES"
            leafObj.use_dupli_faces_scale = True
            leafObj.dupli_faces_scale = 10.0
            try:
                if leafDupliObj not in "NONE":
                    bpy.data.objects[leafDupliObj].parent = leafObj
            except KeyError:
                pass
        elif leafShape == 'dVert':
            leafObj.dupli_type = "VERTS"
            leafObj.use_dupli_vertices_rotation = True
            try:
                if leafDupliObj not in "NONE":
                    bpy.data.objects[leafDupliObj].parent = leafObj
            except KeyError:
                pass

        # add leaf UVs
        if leafShape == 'rect':
            leafMesh.uv_textures.new("leafUV")
            uvlayer = leafMesh.uv_layers.active.data

            u1 = .5 * (1 - leafScaleX)
            u2 = 1 - u1

            for i in range(0, len(leafFaces)):
                uvlayer[i * 4 + 0].uv = Vector((u2, 0))
                uvlayer[i * 4 + 1].uv = Vector((u2, 1))
                uvlayer[i * 4 + 2].uv = Vector((u1, 1))
                uvlayer[i * 4 + 3].uv = Vector((u1, 0))

        elif leafShape == 'hex':
            leafMesh.uv_textures.new("leafUV")
            uvlayer = leafMesh.uv_layers.active.data

            u1 = .5 * (1 - leafScaleX)
            u2 = 1 - u1

            for i in range(0, int(len(leafFaces) / 2)):
                uvlayer[i * 8 + 0].uv = Vector((.5, 0))
                uvlayer[i * 8 + 1].uv = Vector((u1, 1 / 3))
                uvlayer[i * 8 + 2].uv = Vector((u1, 2 / 3))
                uvlayer[i * 8 + 3].uv = Vector((.5, 1))

                uvlayer[i * 8 + 4].uv = Vector((.5, 0))
                uvlayer[i * 8 + 5].uv = Vector((.5, 1))
                uvlayer[i * 8 + 6].uv = Vector((u2, 2 / 3))
                uvlayer[i * 8 + 7].uv = Vector((u2, 1 / 3))

        leafMesh.validate()

    leafVertSize = {'hex': 6, 'rect': 4, 'dFace': 4, 'dVert': 1}[leafShape]

    armLevels = min(armLevels, levels)
    armLevels -= 1

    # unpack vars from splineToBone
    splineToBone1 = splineToBone
    splineToBone = [s[0] if len(s) > 1 else s for s in splineToBone1]
    isend = [s[1] if len(s) > 1 else False for s in splineToBone1]
    issplit = [s[2] if len(s) > 2 else False for s in splineToBone1]
    splitPidx = [s[3] if len(s) > 2 else 0 for s in splineToBone1]

    # If we need an armature we add it
    if useArm:
        # Create the armature and objects
        create_armature(
                    armAnim, leafP, cu, frameRate, leafMesh, leafObj, leafVertSize,
                    leaves, levelCount, splineToBone, treeOb, wind, gust, gustF, af1,
                    af2, af3, leafAnim, loopFrames, previewArm, armLevels, makeMesh, boneStep
                    )

    # print(time.time()-startTime)

    # mesh branches
    if makeMesh:
        t1 = time.time()

        treeMesh = bpy.data.meshes.new('treemesh')
        treeObj = bpy.data.objects.new('treemesh', treeMesh)
        bpy.context.scene.objects.link(treeObj)

        treeVerts = []
        treeEdges = []
        root_vert = []
        vert_radius = []
        vertexGroups = OrderedDict()
        lastVerts = []

        for i, curve in enumerate(cu.splines):
            points = curve.bezier_points

            # find branching level
            level = 0
            for l, c in enumerate(levelCount):
                if i < c:
                    level = l
                    break
            level = min(level, 3)

            step = boneStep[level]
            vindex = len(treeVerts)

            p1 = points[0]

            # add extra vertex for splits
            if issplit[i]:
                pb = int(splineToBone[i][4:-4])
                pn = splitPidx[i]  # int(splineToBone[i][-3:])
                p_1 = cu.splines[pb].bezier_points[pn]
                p_2 = cu.splines[pb].bezier_points[pn + 1]
                p = evalBez(p_1.co, p_1.handle_right, p_2.handle_left, p_2.co, 1 - 1 / (resU + 1))
                treeVerts.append(p)

                root_vert.append(False)
                vert_radius.append((p1.radius * .75, p1.radius * .75))
                treeEdges.append([vindex, vindex + 1])
                vindex += 1

            if isend[i]:
                parent = lastVerts[int(splineToBone[i][4:-4])]
                vindex -= 1
            else:
                # add first point
                treeVerts.append(p1.co)
                root_vert.append(True)
                vert_radius.append((p1.radius, p1.radius))
            """
            # add extra vertex for splits
            if issplit[i]:
                p2 = points[1]
                p = evalBez(p1.co, p1.handle_right, p2.handle_left, p2.co, .001)
                treeVerts.append(p)
                root_vert.append(False)
                vert_radius.append((p1.radius, p1.radius)) #(p1.radius * .95, p1.radius * .95)
                treeEdges.append([vindex,vindex+1])
                vindex += 1
            """
            # dont make vertex group if above armLevels
            if (i >= levelCount[armLevels]):
                idx = i
                groupName = splineToBone[idx]
                g = True
                while groupName not in vertexGroups:
                    # find parent bone of parent bone
                    b = splineToBone[idx]
                    idx = int(b[4:-4])
                    groupName = splineToBone[idx]
            else:
                g = False

            for n, p2 in enumerate(points[1:]):
                if not g:
                    groupName = 'bone' + (str(i)).rjust(3, '0') + '.' + (str(n)).rjust(3, '0')
                    groupName = roundBone(groupName, step)
                    if groupName not in vertexGroups:
                        vertexGroups[groupName] = []

                # parent first vert in split to parent branch bone
                if issplit[i] and n == 0:
                    if g:
                        vertexGroups[groupName].append(vindex - 1)
                    else:
                        vertexGroups[splineToBone[i]].append(vindex - 1)

                for f in range(1, resU + 1):
                    pos = f / resU
                    p = evalBez(p1.co, p1.handle_right, p2.handle_left, p2.co, pos)
                    radius = p1.radius + (p2.radius - p1.radius) * pos

                    treeVerts.append(p)
                    root_vert.append(False)
                    vert_radius.append((radius, radius))

                    if (isend[i]) and (n == 0) and (f == 1):
                        edge = [parent, n * resU + f + vindex]
                    else:
                        edge = [n * resU + f + vindex - 1, n * resU + f + vindex]
                        # add vert to group
                        vertexGroups[groupName].append(n * resU + f + vindex - 1)
                    treeEdges.append(edge)

                vertexGroups[groupName].append(n * resU + resU + vindex)

                p1 = p2

            lastVerts.append(len(treeVerts) - 1)

        treeMesh.from_pydata(treeVerts, treeEdges, ())

        for group in vertexGroups:
            treeObj.vertex_groups.new(group)
            treeObj.vertex_groups[group].add(vertexGroups[group], 1.0, 'ADD')

        # add armature
        if useArm:
            armMod = treeObj.modifiers.new('windSway', 'ARMATURE')
            if previewArm:
                bpy.data.objects['treeArm'].hide = True
                bpy.data.armatures['tree'].draw_type = 'STICK'
            armMod.object = bpy.data.objects['treeArm']
            armMod.use_bone_envelopes = False
            armMod.use_vertex_groups = True
            treeObj.parent = bpy.data.objects['treeArm']

        # add skin modifier and set data
        skinMod = treeObj.modifiers.new('Skin', 'SKIN')
        skinMod.use_smooth_shade = True
        if previewArm:
            skinMod.show_viewport = False
        skindata = treeObj.data.skin_vertices[0].data
        for i, radius in enumerate(vert_radius):
            skindata[i].radius = radius
            skindata[i].use_root = root_vert[i]

        print("mesh time", time.time() - t1)
