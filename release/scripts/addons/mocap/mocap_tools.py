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

# <pep8 compliant>

from math import sqrt, radians, floor, ceil
import bpy
import time
from mathutils import Vector, Matrix


# A Python implementation of n sized Vectors.
# Mathutils has a max size of 4, and we need at least 5 for Simplify Curves and even more for Cross Correlation.
# Vector utility functions
class NdVector:
    vec = []

    def __init__(self, vec):
        self.vec = vec[:]

    def __len__(self):
        return len(self.vec)

    def __mul__(self, otherMember):
        # assume anything with list access is a vector
        if isinstance(otherMember, NdVector):
            a = self.vec
            b = otherMember.vec
            n = len(self)
            return sum([a[i] * b[i] for i in range(n)])
        else:
            # int/float
            return NdVector([otherMember * x for x in self.vec])

    def __sub__(self, otherVec):
        a = self.vec
        b = otherVec.vec
        n = len(self)
        return NdVector([a[i] - b[i] for i in range(n)])

    def __add__(self, otherVec):
        a = self.vec
        b = otherVec.vec
        n = len(self)
        return NdVector([a[i] + b[i] for i in range(n)])

    def __div__(self, scalar):
        return NdVector([x / scalar for x in self.vec])

    @property
    def length(self):
        return sqrt(self * self)

    @property
    def lengthSq(self):
        return (self * self)

    def normalize(self):
        len = self.length
        self.vec = [x / len for x in self.vec]

    def copy(self):
        return NdVector(self.vec)

    def __getitem__(self, i):
        return self.vec[i]

    @property
    def x(self):
        return self.vec[0]

    @property
    def y(self):
        return self.vec[1]

    def resize_2d(self):
        return Vector((self.x, self.y))


#Sampled Data Point class for Simplify Curves
class dataPoint:
    index = 0
    # x,y1,y2,y3 coordinate of original point
    co = NdVector((0, 0, 0, 0, 0))
    #position according to parametric view of original data, [0,1] range
    u = 0
    #use this for anything
    temp = 0

    def __init__(self, index, co, u=0):
        self.index = index
        self.co = co
        self.u = u


# Helper to convert from a sampled fcurve back to editable keyframes one.
def make_editable_fcurves(fcurves):
    for fc in fcurves:
        if fc.sampled_points:
            fc.convert_to_keyframes(floor(fc.sampled_points[0].co[0]), ceil(fc.sampled_points[-1].co[0]) + 1)


#Cross Correlation Function
#http://en.wikipedia.org/wiki/Cross_correlation
#IN:   curvesA, curvesB - bpy_collection/list of fcurves to analyze. Auto-Correlation is when they are the same.
#        margin - When searching for the best "start" frame, how large a neighborhood of frames should we inspect (similar to epsilon in Calculus)
#OUT:   startFrame, length of new anim, and curvesA
def crossCorrelationMatch(curvesA, curvesB, margin):
    dataA = []
    dataB = []
    start = int(max(curvesA[0].range()[0], curvesB[0].range()[0]))
    end = int(min(curvesA[0].range()[1], curvesB[0].range()[1]))

    #transfer all fcurves data on each frame to a single NdVector.
    for i in range(1, end):
        vec = []
        for fcurve in curvesA:
            if fcurve.data_path in [otherFcurve.data_path for otherFcurve in curvesB]:
                vec.append(fcurve.evaluate(i))
        dataA.append(NdVector(vec))
        vec = []
        for fcurve in curvesB:
            if fcurve.data_path in [otherFcurve.data_path for otherFcurve in curvesA]:
                vec.append(fcurve.evaluate(i))
        dataB.append(NdVector(vec))

    #Comparator for Cross Correlation. "Classic" implementation uses dot product, as do we.
    def comp(a, b):
        return a * b

    #Create Rxy, which holds the Cross Correlation data.
    N = len(dataA)
    Rxy = [0.0] * N
    for i in range(N):
        for j in range(i, min(i + N, N)):
            Rxy[i] += comp(dataA[j], dataB[j - i])
        for j in range(i):
            Rxy[i] += comp(dataA[j], dataB[j - i + N])
        Rxy[i] /= float(N)

    #Find the Local maximums in the Cross Correlation data via numerical derivative.
    def LocalMaximums(Rxy):
        Rxyd = [Rxy[i] - Rxy[i - 1] for i in range(1, len(Rxy))]
        maxs = []
        for i in range(1, len(Rxyd) - 1):
            a = Rxyd[i - 1]
            b = Rxyd[i]
            #sign change (zerocrossing) at point i, denoting max point (only)
            if (a >= 0 and b < 0) or (a < 0 and b >= 0):
                maxs.append((i, max(Rxy[i], Rxy[i - 1])))
        return [x[0] for x in maxs]
        #~ return max(maxs, key=lambda x: x[1])[0]

    #flms - the possible offsets of the first part of the animation. In Auto-Corr, this is the length of the loop.
    flms = LocalMaximums(Rxy[0:int(len(Rxy))])
    ss = []

    #for every local maximum, find the best one - i.e. also has the best start frame.
    for flm in flms:
        diff = []

        for i in range(len(dataA) - flm):
            diff.append((dataA[i] - dataB[i + flm]).lengthSq)

        def lowerErrorSlice(diff, e):
            #index, error at index
            bestSlice = (0, 100000)
            for i in range(e, len(diff) - e):
                errorSlice = sum(diff[i - e:i + e + 1])
                if errorSlice < bestSlice[1]:
                    bestSlice = (i, errorSlice, flm)
            return bestSlice

        s = lowerErrorSlice(diff, margin)
        ss.append(s)

    #Find the best result and return it.
    ss.sort(key=lambda x: x[1])
    return ss[0][2], ss[0][0], dataA


#Uses auto correlation (cross correlation of the same set of curves) and trims the active_object's fcurves
#Except for location curves (which in mocap tend to be not cyclic, e.g. a walk cycle forward)
#Transfers the fcurve data to a list of NdVector (length of list is number of fcurves), and calls the cross correlation function.
#Then trims the fcurve accordingly.
#IN: Nothing, set the object you want as active and call. Assumes object has animation_data.action!
#OUT: Trims the object's fcurves (except location curves).
def autoloop_anim():
    context = bpy.context
    obj = context.active_object

    def locCurve(x):
        x.data_path == "location"

    fcurves = [x for x in obj.animation_data.action.fcurves if not locCurve(x)]

    margin = 10

    flm, s, data = crossCorrelationMatch(fcurves, fcurves, margin)
    loop = data[s:s + flm]

    #performs blending with a root falloff on the seam's neighborhood to ensure good tiling.
    for i in range(1, margin + 1):
        w1 = sqrt(float(i) / margin)
        loop[-i] = (loop[-i] * w1) + (loop[0] * (1 - w1))

    for curve in fcurves:
        pts = curve.keyframe_points
        for i in range(len(pts) - 1, -1, -1):
            pts.remove(pts[i])

    for c, curve in enumerate(fcurves):
        pts = curve.keyframe_points
        for i in range(len(loop)):
            pts.insert(i + 2, loop[i][c])

    context.scene.frame_end = flm


#simplifyCurves: performes the bulk of the samples to bezier conversion.
#IN:    curveGroup - which can be a collection of singleFcurves, or grouped (via nested lists) .
#         error - threshold of permittable error (max distance) of the new beziers to the original data
#         reparaError - threshold of error where we should try to fix the parameterization rather than split the existing curve. > error, usually by a small constant factor for best performance.
#         maxIterations - maximum number of iterations of reparameterizations we should attempt. (Newton-Rahpson is not guarenteed to converge, so this is needed).
#         group_mode - boolean, indicating wether we should place bezier keyframes on the same x (frame), or optimize each individual curve.
#OUT: None. Deletes the existing curves and creates the new beziers.
def simplifyCurves(curveGroup, error, reparaError, maxIterations, group_mode):
    #Calculates the unit tangent of point v
    def unitTangent(v, data_pts):
        tang = NdVector((0, 0, 0, 0, 0))
        if v != 0:
            #If it's not the first point, we can calculate a leftside tangent
            tang += data_pts[v].co - data_pts[v - 1].co
        if v != len(data_pts) - 1:
            #If it's not the last point, we can calculate a rightside tangent
            tang += data_pts[v + 1].co - data_pts[v].co
        tang.normalize()
        return tang

    #assign parametric u value for each point in original data, via relative arc length
    #http://en.wikipedia.org/wiki/Arc_length
    def chordLength(data_pts, s, e):
        totalLength = 0
        for pt in data_pts[s:e + 1]:
            i = pt.index
            if i == s:
                chordLength = 0
            else:
                chordLength = (data_pts[i].co - data_pts[i - 1].co).length
            totalLength += chordLength
            pt.temp = totalLength
        for pt in data_pts[s:e + 1]:
            if totalLength == 0:
                print(s, e)
            pt.u = (pt.temp / totalLength)

    # get binomial coefficient lookup table, this function/table is only called with args
    # (3,0),(3,1),(3,2),(3,3),(2,0),(2,1),(2,2)!
    binomDict = {(3, 0): 1,
                 (3, 1): 3,
                 (3, 2): 3,
                 (3, 3): 1,
                 (2, 0): 1,
                 (2, 1): 2,
                 (2, 2): 1,
                 }

    #value at pt t of a single bernstein Polynomial
    def bernsteinPoly(n, i, t):
        binomCoeff = binomDict[(n, i)]
        return binomCoeff * pow(t, i) * pow(1 - t, n - i)

    # fit a single cubic to data points in range [s(tart),e(nd)].
    def fitSingleCubic(data_pts, s, e):

        # A - matrix used for calculating C matrices for fitting
        def A(i, j, s, e, t1, t2):
            if j == 1:
                t = t1
            if j == 2:
                t = t2
            u = data_pts[i].u
            return t * bernsteinPoly(3, j, u)

        # X component, used for calculating X matrices for fitting
        def xComponent(i, s, e):
            di = data_pts[i].co
            u = data_pts[i].u
            v0 = data_pts[s].co
            v3 = data_pts[e].co
            a = v0 * bernsteinPoly(3, 0, u)
            b = v0 * bernsteinPoly(3, 1, u)
            c = v3 * bernsteinPoly(3, 2, u)
            d = v3 * bernsteinPoly(3, 3, u)
            return (di - (a + b + c + d))

        t1 = unitTangent(s, data_pts)
        t2 = unitTangent(e, data_pts)
        c11 = sum([A(i, 1, s, e, t1, t2) * A(i, 1, s, e, t1, t2) for i in range(s, e + 1)])
        c12 = sum([A(i, 1, s, e, t1, t2) * A(i, 2, s, e, t1, t2) for i in range(s, e + 1)])
        c21 = c12
        c22 = sum([A(i, 2, s, e, t1, t2) * A(i, 2, s, e, t1, t2) for i in range(s, e + 1)])

        x1 = sum([xComponent(i, s, e) * A(i, 1, s, e, t1, t2) for i in range(s, e + 1)])
        x2 = sum([xComponent(i, s, e) * A(i, 2, s, e, t1, t2) for i in range(s, e + 1)])

        # calculate Determinate of the 3 matrices
        det_cc = c11 * c22 - c21 * c12
        det_cx = c11 * x2 - c12 * x1
        det_xc = x1 * c22 - x2 * c12

        # if matrix is not homogenous, fudge the data a bit
        if det_cc == 0:
            det_cc = 0.01

        # alpha's are the correct offset for bezier handles
        alpha0 = det_xc / det_cc   # offset from right (first) point
        alpha1 = det_cx / det_cc   # offset from left (last) point

        sRightHandle = data_pts[s].co.copy()
        sTangent = t1 * abs(alpha0)
        sRightHandle += sTangent  # position of first pt's handle
        eLeftHandle = data_pts[e].co.copy()
        eTangent = t2 * abs(alpha1)
        eLeftHandle += eTangent  # position of last pt's handle.

        # return a 4 member tuple representing the bezier
        return (data_pts[s].co,
              sRightHandle,
              eLeftHandle,
              data_pts[e].co)

    # convert 2 given data points into a cubic bezier.
    # handles are offset along the tangent at
    # a 3rd of the length between the points.
    def fitSingleCubic2Pts(data_pts, s, e):
        alpha0 = alpha1 = (data_pts[s].co - data_pts[e].co).length / 3

        sRightHandle = data_pts[s].co.copy()
        sTangent = unitTangent(s, data_pts) * abs(alpha0)
        sRightHandle += sTangent  # position of first pt's handle
        eLeftHandle = data_pts[e].co.copy()
        eTangent = unitTangent(e, data_pts) * abs(alpha1)
        eLeftHandle += eTangent  # position of last pt's handle.

        #return a 4 member tuple representing the bezier
        return (data_pts[s].co,
          sRightHandle,
          eLeftHandle,
          data_pts[e].co)

    #evaluate bezier, represented by a 4 member tuple (pts) at point t.
    def bezierEval(pts, t):
        sumVec = NdVector((0, 0, 0, 0, 0))
        for i in range(4):
            sumVec += pts[i] * bernsteinPoly(3, i, t)
        return sumVec

    #calculate the highest error between bezier and original data
    #returns the distance and the index of the point where max error occurs.
    def maxErrorAmount(data_pts, bez, s, e):
        maxError = 0
        maxErrorPt = s
        if e - s < 3:
            return 0, None
        for pt in data_pts[s:e + 1]:
            bezVal = bezierEval(bez, pt.u)
            normalize_error = pt.co.length
            if normalize_error == 0:
                normalize_error = 1
            tmpError = (pt.co - bezVal).length / normalize_error
            if tmpError >= maxError:
                maxError = tmpError
                maxErrorPt = pt.index
        return maxError, maxErrorPt

    #calculated bezier derivative at point t.
    #That is, tangent of point t.
    def getBezDerivative(bez, t):
        n = len(bez) - 1
        sumVec = NdVector((0, 0, 0, 0, 0))
        for i in range(n - 1):
            sumVec += (bez[i + 1] - bez[i]) * bernsteinPoly(n - 1, i, t)
        return sumVec

    #use Newton-Raphson to find a better paramterization of datapoints,
    #one that minimizes the distance (or error)
    # between bezier and original data.
    def newtonRaphson(data_pts, s, e, bez):
        for pt in data_pts[s:e + 1]:
            if pt.index == s:
                pt.u = 0
            elif pt.index == e:
                pt.u = 1
            else:
                u = pt.u
                qu = bezierEval(bez, pt.u)
                qud = getBezDerivative(bez, u)
                #we wish to minimize f(u),
                #the squared distance between curve and data
                fu = (qu - pt.co).length ** 2
                fud = (2 * (qu.x - pt.co.x) * (qud.x)) - (2 * (qu.y - pt.co.y) * (qud.y))
                if fud == 0:
                    fu = 0
                    fud = 1
                pt.u = pt.u - (fu / fud)

    #Create data_pts, a list of dataPoint type, each is assigned index i, and an NdVector
    def createDataPts(curveGroup, group_mode):
        make_editable_fcurves(curveGroup if group_mode else (curveGroup,))

        if group_mode:
            print([x.data_path for x in curveGroup])
            comp_cos = (0,) * (4 - len(curveGroup))  # We need to add that number of null cos to get our 5D vector.
            kframes = sorted(set(kf.co.x for fc in curveGroup for kf in fc.keyframe_points))
            data_pts = [dataPoint(i, NdVector((fra,) + tuple(fc.evaluate(fra) for fc in curveGroup) + comp_cos))
                        for i, fra in enumerate(kframes)]
        else:
            data_pts = [dataPoint(i, NdVector((kf.co.x, kf.co.y, 0, 0, 0)))
                        for i, kf in enumerate(curveGroup.keyframe_points)]
        return data_pts

    #Recursively fit cubic beziers to the data_pts between s and e
    def fitCubic(data_pts, s, e):
        # if there are less than 3 points, fit a single basic bezier
        if e - s < 3:
            bez = fitSingleCubic2Pts(data_pts, s, e)
        else:
            #if there are more, parameterize the points
            # and fit a single cubic bezier
            chordLength(data_pts, s, e)
            bez = fitSingleCubic(data_pts, s, e)

        #calculate max error and point where it occurs
        maxError, maxErrorPt = maxErrorAmount(data_pts, bez, s, e)
        #if error is small enough, reparameterization might be enough
        if maxError < reparaError and maxError > error:
            for i in range(maxIterations):
                newtonRaphson(data_pts, s, e, bez)
                if e - s < 3:
                    bez = fitSingleCubic2Pts(data_pts, s, e)
                else:
                    bez = fitSingleCubic(data_pts, s, e)

            #recalculate max error and point where it occurs
            maxError, maxErrorPt = maxErrorAmount(data_pts, bez, s, e)

        #repara wasn't enough, we need 2 beziers for this range.
        #Split the bezier at point of maximum error
        if maxError > error:
            fitCubic(data_pts, s, maxErrorPt)
            fitCubic(data_pts, maxErrorPt, e)
        else:
            #error is small enough, return the beziers.
            beziers.append(bez)
            return

    # deletes the sampled points and creates beziers.
    def createNewCurves(curveGroup, beziers, group_mode):
        #remove all existing data points
        if group_mode:
            for fcurve in curveGroup:
                for i in range(len(fcurve.keyframe_points) - 1, 0, -1):
                    fcurve.keyframe_points.remove(fcurve.keyframe_points[i], fast=True)
        else:
            fcurve = curveGroup
            for i in range(len(fcurve.keyframe_points) - 1, 0, -1):
                fcurve.keyframe_points.remove(fcurve.keyframe_points[i], fast=True)

        #insert the calculated beziers to blender data.
        if group_mode:
            for fullbez in beziers:
                for i, fcurve in enumerate(curveGroup):
                    bez = [Vector((vec[0], vec[i + 1])) for vec in fullbez]
                    newKey = fcurve.keyframe_points.insert(frame=bez[0].x, value=bez[0].y, options={'FAST'})
                    newKey.handle_right = (bez[1].x, bez[1].y)

                    newKey = fcurve.keyframe_points.insert(frame=bez[3].x, value=bez[3].y, options={'FAST'})
                    newKey.handle_left = (bez[2].x, bez[2].y)
        else:
            for bez in beziers:
                for vec in bez:
                    vec.resize_2d()
                newKey = fcurve.keyframe_points.insert(frame=bez[0].x, value=bez[0].y, options={'FAST'})
                newKey.handle_right = (bez[1].x, bez[1].y)

                newKey = fcurve.keyframe_points.insert(frame=bez[3].x, value=bez[3].y, options={'FAST'})
                newKey.handle_left = (bez[2].x, bez[2].y)

        # We used fast remove/insert, time to update the curves!
        for fcurve in (curveGroup if group_mode else (curveGroup,)):
            fcurve.update()

    # indices are detached from data point's frame (x) value and
    # stored in the dataPoint object, represent a range

    data_pts = createDataPts(curveGroup, group_mode)

    if not data_pts:
        return

    s = 0  # start
    e = len(data_pts) - 1  # end

    beziers = []

    #begin the recursive fitting algorithm.
    fitCubic(data_pts, s, e)

    #remove old Fcurves and insert the new ones
    createNewCurves(curveGroup, beziers, group_mode)


#Main function of simplification, which called by Operator
#IN:
#       sel_opt- either "sel" (selected) or "all" for which curves to effect
#       error- maximum error allowed, in fraction (20% = 0.0020, which is the default),
#       i.e. divide by 10000 from percentage wanted.
#       group_mode- boolean, to analyze each curve seperately or in groups,
#       where a group is all curves that effect the same property/RNA path
def fcurves_simplify(context, obj, sel_opt="all", error=0.002, group_mode=True):
    # main vars
    fcurves = obj.animation_data.action.fcurves

    if sel_opt == "sel":
        sel_fcurves = [fcurve for fcurve in fcurves if fcurve.select]
    else:
        sel_fcurves = fcurves[:]

    #Error threshold for Newton Raphson reparamatizing
    reparaError = error * 32
    maxIterations = 16

    if group_mode:
        fcurveDict = {}
        #this loop sorts all the fcurves into groups of 3 or 4,
        #based on their RNA Data path, which corresponds to
        #which property they effect
        for curve in sel_fcurves:
            if curve.data_path in fcurveDict:  # if this bone has been added, append the curve to its list
                fcurveDict[curve.data_path].append(curve)
            else:
                fcurveDict[curve.data_path] = [curve]  # new bone, add a new dict value with this first curve
        fcurveGroups = fcurveDict.values()
    else:
        fcurveGroups = sel_fcurves

    if error > 0.00000:
        #simplify every selected curve.
        totalt = 0
        for i, fcurveGroup in enumerate(fcurveGroups):
            print("Processing curve " + str(i + 1) + "/" + str(len(fcurveGroups)))
            t = time.clock()
            simplifyCurves(fcurveGroup, error, reparaError, maxIterations, group_mode)
            t = time.clock() - t
            print(str(t)[:5] + " seconds to process last curve")
            totalt += t
            print(str(totalt)[:5] + " seconds, total time elapsed")

    return


def detect_min_max(v):
    """
    Converted from MATLAB script at http://billauer.co.il/peakdet.html

    Yields indices of peaks, i.e. local minima/maxima.

    % Eli Billauer, 3.4.05 (Explicitly not copyrighted).
    % This function is released to the public domain; Any use is allowed.
    """

    min_val, max_val = float('inf'), -float('inf')

    check_max = True

    for i, val in enumerate(v):
        if val > max_val:
            max_val = val
        if val < min_val:
            min_val = val

        if check_max:
            if val < max_val:
                yield i
                min_val = val
                check_max = False
        else:
            if val > min_val:
                yield i
                max_val = val
                check_max = True


def denoise(obj, fcurves):
    """
    Implementation of non-linear blur filter.
    Finds spikes in the fcurve, and replaces spikes that are too big with the average of the surrounding keyframes.
    """
    make_editable_fcurves(fcurves)

    for fcurve in fcurves:
        org_pts = fcurve.keyframe_points[:]

        for idx in detect_min_max(pt.co.y for pt in fcurve.keyframe_points[1:-1]):
            # Find the neighbours
            prev_pt = org_pts[idx - 1].co.y
            next_pt = org_pts[idx + 1].co.y
            this_pt = org_pts[idx]

            # Check the distance from the min/max to the average of the surrounding points.
            avg = (prev_pt + next_pt) / 2
            is_peak = abs(this_pt.co.y - avg) > avg * 0.02

            if is_peak:
                diff = avg - fcurve.keyframe_points[idx].co.y
                fcurve.keyframe_points[idx].co.y = avg
                fcurve.keyframe_points[idx].handle_left.y += diff
                fcurve.keyframe_points[idx].handle_right.y += diff

        # Important to update the curve after modifying it!
        fcurve.update()


# Recieves armature, and rotations all bones by 90 degrees along the X axis
# This fixes the common axis issue BVH files have when importing.
# IN: Armature (bpy.types.Armature)
def rotate_fix_armature(arm_data):
    global_matrix = Matrix.Rotation(radians(90), 4, "X")
    bpy.ops.object.mode_set(mode='EDIT', toggle=False)
    #disconnect all bones for ease of global rotation
    connectedBones = []
    for bone in arm_data.edit_bones:
        if bone.use_connect:
            connectedBones.append(bone.name)
            bone.use_connect = False

    #rotate all the bones around their center
    for bone in arm_data.edit_bones:
        bone.transform(global_matrix)

    #reconnect the bones
    for bone in connectedBones:
        arm_data.edit_bones[bone].use_connect = True
    bpy.ops.object.mode_set(mode='OBJECT', toggle=False)


#Roughly scales the performer armature to match the enduser armature
#IN: perfromer_obj, enduser_obj, Blender objects whose .data is an armature.
def scale_fix_armature(performer_obj, enduser_obj):
    perf_bones = performer_obj.data.bones
    end_bones = enduser_obj.data.bones

    def calculateBoundingRadius(bones):
        # Calculate the average position of each bone
        center = sum((bone.head_local for bone in bones), Vector())
        center /= len(bones)

        # The radius is defined as the max distance from the center.
        radius = max((bone.head_local - center).length for bone in bones)
        return radius

    perf_rad = calculateBoundingRadius(performer_obj.data.bones)
    end_rad = calculateBoundingRadius(enduser_obj.data.bones)

    factor = end_rad / perf_rad
    performer_obj.scale = factor * Vector((1, 1, 1))


#Guess Mapping
#Given a performer and enduser armature, attempts to guess the hiearchy mapping
def guessMapping(performer_obj, enduser_obj):
    perf_bones = performer_obj.data.bones
    end_bones = enduser_obj.data.bones

    root = perf_bones[0]

    def findBoneSide(bone):
        if "Left" in bone:
            return "Left", bone.replace("Left", "").lower().replace(".", "")
        if "Right" in bone:
            return "Right", bone.replace("Right", "").lower().replace(".", "")
        if "L" in bone:
            return "Left", bone.replace("Left", "").lower().replace(".", "")
        if "R" in bone:
            return "Right", bone.replace("Right", "").lower().replace(".", "")
        return "", bone

    def nameMatch(bone_a, bone_b):
        # nameMatch - recieves two strings, returns 2 if they are relatively the same, 1 if they are the same but R and L and 0 if no match at all
        side_a, noside_a = findBoneSide(bone_a)
        side_b, noside_b = findBoneSide(bone_b)
        if side_a == side_b:
            if noside_a in noside_b or noside_b in noside_a:
                return 2
        else:
            if noside_a in noside_b or noside_b in noside_a:
                return 1
        return 0

    def guessSingleMapping(perf_bone):
        possible_bones = [end_bones[0]]

        while possible_bones:
            for end_bone in possible_bones:
                match = nameMatch(perf_bone.name, end_bone.name)
                if match == 2 and not perf_bone.map:
                    perf_bone.map = end_bone.name
                #~ elif match == 1 and not perf_bone.map:
                    #~ oppo = perf_bones[oppositeBone(perf_bone)].map
                    # if oppo:
                    #   perf_bone = oppo
            newPossibleBones = []
            for end_bone in possible_bones:
                newPossibleBones += list(end_bone.children)
            possible_bones = newPossibleBones

        for child in perf_bone.children:
            guessSingleMapping(child)

    guessSingleMapping(root)


# Creates limit rotation constraints on the enduser armature based on range of motion (max min of fcurves) of the performer.
# IN: context (bpy.context, etc.), and 2 blender objects which are armatures
# OUT: creates the limit constraints.
def limit_dof(context, performer_obj, enduser_obj):
    limitDict = {}
    perf_bones = [bone for bone in performer_obj.pose.bones if bone.bone.map]
    c_frame = context.scene.frame_current
    for bone in perf_bones:
        limitDict[bone.bone.map] = [1000, 1000, 1000, -1000, -1000, -1000]
    for t in range(context.scene.frame_start, context.scene.frame_end):
        context.scene.frame_set(t)
        for bone in perf_bones:
            end_bone = enduser_obj.pose.bones[bone.bone.map]
            bake_matrix = bone.matrix
            rest_matrix = end_bone.bone.matrix_local

            if end_bone.parent and end_bone.bone.use_inherit_rotation:
                srcParent = bone.parent
                parent_mat = srcParent.matrix
                parent_rest = end_bone.parent.bone.matrix_local
                parent_rest_inv = parent_rest.inverted()
                parent_mat_inv = parent_mat.inverted()
                bake_matrix = parent_mat_inv * bake_matrix
                rest_matrix = parent_rest_inv * rest_matrix

            rest_matrix_inv = rest_matrix.inverted()
            bake_matrix = rest_matrix_inv * bake_matrix

            mat = bake_matrix
            euler = mat.to_euler()
            limitDict[bone.bone.map][0] = min(limitDict[bone.bone.map][0], euler.x)
            limitDict[bone.bone.map][1] = min(limitDict[bone.bone.map][1], euler.y)
            limitDict[bone.bone.map][2] = min(limitDict[bone.bone.map][2], euler.z)
            limitDict[bone.bone.map][3] = max(limitDict[bone.bone.map][3], euler.x)
            limitDict[bone.bone.map][4] = max(limitDict[bone.bone.map][4], euler.y)
            limitDict[bone.bone.map][5] = max(limitDict[bone.bone.map][5], euler.z)
    for bone in enduser_obj.pose.bones:
        existingConstraint = [constraint for constraint in bone.constraints if constraint.name == "DOF Limitation"]
        if existingConstraint:
            bone.constraints.remove(existingConstraint[0])
    end_bones = [bone for bone in enduser_obj.pose.bones if bone.name in limitDict.keys()]
    for bone in end_bones:
        #~ if not bone.is_in_ik_chain:
        newCons = bone.constraints.new("LIMIT_ROTATION")
        newCons.name = "DOF Limitation"
        newCons.owner_space = "LOCAL"
        newCons.min_x, newCons.min_y, newCons.min_z, newCons.max_x, newCons.max_y, newCons.max_z = limitDict[bone.name]
        newCons.use_limit_x = True
        newCons.use_limit_y = True
        newCons.use_limit_z = True
    context.scene.frame_set(c_frame)


# Removes the constraints that were added by limit_dof on the enduser_obj
def limit_dof_toggle_off(context, enduser_obj):
    for bone in enduser_obj.pose.bones:
        existingConstraint = [constraint for constraint in bone.constraints if constraint.name == "DOF Limitation"]
        if existingConstraint:
            bone.constraints.remove(existingConstraint[0])


# Reparameterizes a blender path via keyframing it's eval_time to match a stride_object's forward velocity.
# IN: Context, stride object (blender object with location keyframes), path object.
def path_editing(context, stride_obj, path):
    y_fcurve = [fcurve for fcurve in stride_obj.animation_data.action.fcurves if fcurve.data_path == "location"][1]
    s, e = context.scene.frame_start, context.scene.frame_end  # y_fcurve.range()
    s = int(s)
    e = int(e)
    y_s = y_fcurve.evaluate(s)
    y_e = y_fcurve.evaluate(e)
    direction = (y_e - y_s) / abs(y_e - y_s)
    existing_cons = [constraint for constraint in stride_obj.constraints if constraint.type == "FOLLOW_PATH"]
    for cons in existing_cons:
        stride_obj.constraints.remove(cons)
    path_cons = stride_obj.constraints.new("FOLLOW_PATH")
    if direction < 0:
        path_cons.forward_axis = "TRACK_NEGATIVE_Y"
    else:
        path_cons.forward_axis = "FORWARD_Y"
    path_cons.target = path
    path_cons.use_curve_follow = True
    path.data.path_duration = e - s
    try:
        path.data.animation_data.action.fcurves
    except AttributeError:
        path.data.keyframe_insert("eval_time", frame=0)
    eval_time_fcurve = [fcurve for fcurve in path.data.animation_data.action.fcurves if fcurve.data_path == "eval_time"]
    eval_time_fcurve = eval_time_fcurve[0]
    totalLength = 0
    parameterization = {}
    print("evaluating curve")
    for t in range(s, e - 1):
        if s == t:
            chordLength = 0
        else:
            chordLength = (y_fcurve.evaluate(t) - y_fcurve.evaluate(t + 1))
        totalLength += chordLength
        parameterization[t] = totalLength
    for t in range(s + 1, e - 1):
        if totalLength == 0:
            print("no forward motion")
        parameterization[t] /= totalLength
        parameterization[t] *= e - s
    parameterization[e] = e - s
    for t in parameterization.keys():
        eval_time_fcurve.keyframe_points.insert(frame=t, value=parameterization[t])
    y_fcurve.mute = True
    print("finished path editing")


#Animation Stitching
#Stitches two retargeted animations together via NLA settings.
#IN: enduser_obj, a blender armature that has had two retargets applied.
def anim_stitch(context, enduser_obj):
    stitch_settings = enduser_obj.data.stitch_settings
    action_1 = stitch_settings.first_action
    action_2 = stitch_settings.second_action
    if stitch_settings.stick_bone != "":
        selected_bone = enduser_obj.pose.bones[stitch_settings.stick_bone]
    else:
        selected_bone = enduser_obj.pose.bones[0]
    scene = context.scene
    TrackNamesA = enduser_obj.data.mocapNLATracks[action_1]
    TrackNamesB = enduser_obj.data.mocapNLATracks[action_2]
    enduser_obj.data.active_mocap = action_1
    anim_data = enduser_obj.animation_data
    # add tracks for action 2
    mocapAction = bpy.data.actions[TrackNamesB.base_track]
    mocapTrack = anim_data.nla_tracks.new()
    mocapTrack.name = TrackNamesB.base_track
    mocapStrip = mocapTrack.strips.new(TrackNamesB.base_track, stitch_settings.blend_frame, mocapAction)
    mocapStrip.extrapolation = "HOLD_FORWARD"
    mocapStrip.blend_in = stitch_settings.blend_amount
    mocapStrip.action_frame_start += stitch_settings.second_offset
    mocapStrip.action_frame_end += stitch_settings.second_offset
    constraintTrack = anim_data.nla_tracks.new()
    constraintTrack.name = TrackNamesB.auto_fix_track
    constraintAction = bpy.data.actions[TrackNamesB.auto_fix_track]
    constraintStrip = constraintTrack.strips.new(TrackNamesB.auto_fix_track, stitch_settings.blend_frame, constraintAction)
    constraintStrip.extrapolation = "HOLD_FORWARD"
    constraintStrip.blend_in = stitch_settings.blend_amount
    userTrack = anim_data.nla_tracks.new()
    userTrack.name = TrackNamesB.manual_fix_track
    userAction = bpy.data.actions[TrackNamesB.manual_fix_track]
    userStrip = userTrack.strips.new(TrackNamesB.manual_fix_track, stitch_settings.blend_frame, userAction)
    userStrip.extrapolation = "HOLD_FORWARD"
    userStrip.blend_in = stitch_settings.blend_amount
    #stride bone
    if enduser_obj.parent:
        if enduser_obj.parent.name == "stride_bone":
            stride_bone = enduser_obj.parent
            stride_anim_data = stride_bone.animation_data
            stride_anim_data.use_nla = True
            stride_anim_data.action = None
            for track in stride_anim_data.nla_tracks:
                stride_anim_data.nla_tracks.remove(track)
            actionATrack = stride_anim_data.nla_tracks.new()
            actionATrack.name = TrackNamesA.stride_action
            actionAStrip = actionATrack.strips.new(TrackNamesA.stride_action, 0, bpy.data.actions[TrackNamesA.stride_action])
            actionAStrip.extrapolation = "NOTHING"
            actionBTrack = stride_anim_data.nla_tracks.new()
            actionBTrack.name = TrackNamesB.stride_action
            actionBStrip = actionBTrack.strips.new(TrackNamesB.stride_action, stitch_settings.blend_frame, bpy.data.actions[TrackNamesB.stride_action])
            actionBStrip.action_frame_start += stitch_settings.second_offset
            actionBStrip.action_frame_end += stitch_settings.second_offset
            actionBStrip.extrapolation = "NOTHING"
            #we need to change the stride_bone's action to add the offset
            aStrideCurves = [fcurve for fcurve in bpy.data.actions[TrackNamesA.stride_action].fcurves if fcurve.data_path == "location"]
            bStrideCurves = [fcurve for fcurve in bpy.data.actions[TrackNamesB.stride_action].fcurves if fcurve.data_path == "location"]
            scene.frame_set(stitch_settings.blend_frame - 1)
            desired_pos = (enduser_obj.matrix_world * selected_bone.matrix.to_translation())
            scene.frame_set(stitch_settings.blend_frame)
            actual_pos = (enduser_obj.matrix_world * selected_bone.matrix.to_translation())
            print(desired_pos, actual_pos)
            offset = Vector(actual_pos) - Vector(desired_pos)

            for i, fcurve in enumerate(bStrideCurves):
                print(offset[i], i, fcurve.array_index)
                for pt in fcurve.keyframe_points:
                    pt.co.y -= offset[i]
                    pt.handle_left.y -= offset[i]
                    pt.handle_right.y -= offset[i]

            #actionBStrip.blend_in = stitch_settings.blend_amount


#Guesses setting for animation stitching via Cross Correlation
def guess_anim_stitch(context, enduser_obj):
    stitch_settings = enduser_obj.data.stitch_settings
    action_1 = stitch_settings.first_action
    action_2 = stitch_settings.second_action
    TrackNamesA = enduser_obj.data.mocapNLATracks[action_1]
    TrackNamesB = enduser_obj.data.mocapNLATracks[action_2]
    mocapA = bpy.data.actions[TrackNamesA.base_track]
    mocapB = bpy.data.actions[TrackNamesB.base_track]
    curvesA = mocapA.fcurves
    curvesB = mocapB.fcurves
    flm, s, data = crossCorrelationMatch(curvesA, curvesB, 10)
    print("Guessed the following for start and offset: ", s, flm)
    enduser_obj.data.stitch_settings.blend_frame = flm
    enduser_obj.data.stitch_settings.second_offset = s
