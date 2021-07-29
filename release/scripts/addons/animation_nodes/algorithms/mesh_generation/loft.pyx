from ... math cimport Vector3, mixVec3Arrays
from ... data_structures cimport Vector3DList, Spline, BezierSpline
from ... utils.lists cimport findListSegment_LowLevel, findListSegment
from ... data_structures.splines.base_spline cimport SplineEvaluationFunction

from . import grid

cdef class LinearLoft:
    cdef:
        public list splines
        public int splineSamples, subdivisions
        public float start, end
        public bint cyclic
        public str distributionType
        public int uniformResolution

    def validate(self):
        if self.start > self.end:
            self.start, self.end = self.end, self.start
        self.start = min(max(self.start, 0), 1)
        self.end = min(max(self.end, 0), 1)
        self.splineSamples = max(self.splineSamples, 2)
        self.subdivisions = max(self.subdivisions, 0)
        self.uniformResolution = max(self.uniformResolution, 2)

        if self.distributionType not in ("RESOLUTION", "UNIFORM"): return False
        if len(self.splines) < 2: return False
        for spline in self.splines:
            if not isinstance(spline, Spline): return False
            if not spline.isEvaluable(): return False
        return True

    def calcVertices(self):
        cdef:
            int samples = self.splineSamples
            int subdivisions = self.subdivisions
            int splineAmount = len(self.splines)
            long startIndices[2]
            long endIndices[2]
            float startT, endT

        # find which segments to split
        findListSegment_LowLevel(splineAmount, self.cyclic, self.start, startIndices, &startT)
        findListSegment_LowLevel(splineAmount, self.cyclic, self.end, endIndices, &endT)

        cdef:
            Vector3DList vertices, tmp1, tmp2
            long totalLineAmount, i, lineIndex

        controlLines = endIndices[0] - startIndices[0] + 2
        subdivisionLines = (controlLines - 1) * subdivisions
        totalLineAmount = controlLines + subdivisionLines

        vertices = Vector3DList(length = totalLineAmount * samples)

        if startIndices[0] == endIndices[0]: # <- only one segment in the result
            tmp1 = Vector3DList(length = samples);
            tmp2 = Vector3DList(length = samples);
            self.writeSplineLine(self.splines[startIndices[0]], tmp1.data)
            self.writeSplineLine(self.splines[startIndices[1]], tmp2.data)

            self.writeMixedLine(target = vertices.data,
                    sourceA = tmp1.data,
                    sourceB = tmp2.data,
                    factor = startT)
            self.writeMixedLine(target = vertices.data + (subdivisions + 1) * samples,
                    sourceA = tmp1.data,
                    sourceB = tmp2.data,
                    factor = endT)
            self.writeSubdivisionLines(target = vertices.data + samples,
                    sourceA = vertices.data,
                    sourceB = vertices.data + (subdivisions + 1) * samples)
        else: # <- multiple segments in the result
            # TODO: speedup when startT or endT is 0 or 1
            tmp1 = Vector3DList(length = samples)
            tmp2 = Vector3DList(length = samples)
            self.writeSplineLine(self.splines[startIndices[0]], tmp1.data)
            self.writeSplineLine(self.splines[startIndices[1]], tmp2.data)
            self.writeMixedLine(target = vertices.data,
                    sourceA = tmp1.data,
                    sourceB = tmp2.data,
                    factor = startT)
            self.writeSplineLine(self.splines[endIndices[0]], tmp1.data)
            self.writeSplineLine(self.splines[endIndices[1]], tmp2.data)
            self.writeMixedLine(target = vertices.data + (totalLineAmount - 1) * samples,
                    sourceA = tmp1.data,
                    sourceB = tmp2.data,
                    factor = endT)

            for i in range(endIndices[0] - startIndices[0]):
                spline = self.splines[i + startIndices[1]]
                lineIndex = (i + 1) * (subdivisions + 1)
                self.writeSplineLine(spline, vertices.data + lineIndex * samples)
                self.writeSubdivisionLines(
                        target = vertices.data + (lineIndex - subdivisions) * samples,
                        sourceA = vertices.data + (lineIndex - subdivisions - 1) * samples,
                        sourceB = vertices.data + lineIndex * samples)

            self.writeSubdivisionLines(
                    target = vertices.data + (totalLineAmount - 1 - subdivisions) * samples,
                    sourceA = vertices.data + (totalLineAmount - 2 - subdivisions) * samples,
                    sourceB = vertices.data + (totalLineAmount - 1) * samples)

        # remove last line when the first line can be reused
        if self.cyclic and self.start == 0 and self.end == 1:
            vertices.length -= 1

        return vertices

    cdef writeSplineLine(self, Spline spline, Vector3* target):
        cdef SplineEvaluationFunction evaluateFunction
        if self.distributionType == "RESOLUTION":
            evaluateFunction = spline.evaluate_LowLevel
        elif self.distributionType == "UNIFORM":
            evaluateFunction = spline.evaluateUniform_LowLevel
            spline.ensureUniformConverter(self.uniformResolution)
        spline.sampleEvaluationFunction_LowLevel(evaluateFunction, self.splineSamples,
                                                 0.0, 1.0, target)

    cdef writeMixedLine(self, Vector3* target, Vector3* sourceA, Vector3* sourceB, float factor):
        mixVec3Arrays(target, sourceA, sourceB, self.splineSamples, factor)

    cdef writeSubdivisionLines(self, Vector3* target, Vector3* sourceA, Vector3* sourceB):
        cdef int i
        for i in range(self.subdivisions):
            self.writeMixedLine(target = target + i * self.splineSamples,
                sourceA = sourceA,
                sourceB = sourceB,
                factor = (i + 1) / <float>(self.subdivisions + 1))

    def calcEdgeIndices(self):
        return grid.quadEdges(**self.getGridParameters())

    def calcPolygonIndices(self):
        return grid.quadPolygons(**self.getGridParameters())

    def getGridParameters(self):
        startIndices, _ = findListSegment(len(self.splines), self.cyclic, self.start)
        endIndices, _ = findListSegment(len(self.splines), self.cyclic, self.end)

        controlLines = endIndices[0] - startIndices[0] + 2
        totalLines = controlLines + (controlLines - 1) * self.subdivisions

        joinHorizontal = self.cyclic and self.start == 0 and self.end == 1
        if joinHorizontal:
            totalLines -= 1

        return dict(xDivisions = totalLines,
                    yDivisions = self.splineSamples,
                    joinHorizontal = joinHorizontal,
                    joinVertical = allSplinesCyclic(self.splines))


cdef class SmoothLoft:
    cdef:
        public list splines
        public int splineSamples, surfaceSamples
        public float start, end, smoothness
        public bint cyclic
        public str splineDistributionType, surfaceDistributionType
        public int uniformResolution

    def validate(self):
        if self.start > self.end:
            self.start, self.end = self.end, self.start
        self.start = min(max(self.start, 0), 1)
        self.end = min(max(self.end, 0), 1)
        self.splineSamples = max(self.splineSamples, 2)
        self.surfaceSamples = max(self.surfaceSamples, 2)
        self.uniformResolution = max(self.uniformResolution, 2)

        if self.splineDistributionType not in ("RESOLUTION", "UNIFORM"): return False
        if self.surfaceDistributionType not in ("RESOLUTION", "UNIFORM"): return False
        if len(self.splines) < 2: return False
        for spline in self.splines:
            if not isinstance(spline, Spline): return False
            if not spline.isEvaluable(): return False
        return True

    def calcVertices(self):
        cdef:
            int i
            int splineAmount = len(self.splines)
            Vector3DList vertices = Vector3DList(length = self.splineSamples * self.surfaceSamples)
            Vector3DList surfaceSplinePoints = Vector3DList(length = splineAmount)
            BezierSpline surfaceSpline

        surfaceSpline = BezierSpline(
            points = surfaceSplinePoints,
            leftHandles = Vector3DList(length = splineAmount),
            rightHandles = Vector3DList(length = splineAmount),
            cyclic = self.cyclic)

        cdef Spline spline
        if self.splineDistributionType == "UNIFORM":
            for spline in self.splines:
                spline.ensureUniformConverter(self.uniformResolution)

        for i in range(self.splineSamples):
            self.createSurfaceSpline(i, surfaceSplinePoints.data)
            surfaceSpline.calculateSmoothHandles(self.smoothness)
            self.sampleSurfaceSpline(surfaceSpline, vertices.data + i * self.surfaceSamples)

        return vertices

    cdef createSurfaceSpline(self, float index, Vector3* _surfaceSplinePoints):
        cdef:
            int k
            Spline spline
            float t
            float tCyclic = index / <float>self.splineSamples
            float tNormal = index / <float>(self.splineSamples - 1)
        if self.splineDistributionType == "RESOLUTION":
            for k in range(len(self.splines)):
                spline = self.splines[k]
                t = tCyclic if spline.cyclic else tNormal
                spline.evaluate_LowLevel(t, _surfaceSplinePoints + k)
        elif self.splineDistributionType == "UNIFORM":
            for k in range(len(self.splines)):
                spline = self.splines[k]
                t = tCyclic if spline.cyclic else tNormal
                spline.evaluateUniform_LowLevel(t, _surfaceSplinePoints + k)

    cdef sampleSurfaceSpline(self, BezierSpline spline, Vector3* output):
        if self.surfaceDistributionType == "UNIFORM":
            spline.ensureUniformConverter(self.uniformResolution)
            spline.getUniformSamples_LowLevel(self.surfaceSamples, self.start, self.end, output)
        else:
            spline.getSamples_LowLevel(self.surfaceSamples, self.start, self.end, output)

    def calcEdgeIndices(self):
        return grid.quadEdges(
            xDivisions = self.splineSamples,
            yDivisions = self.surfaceSamples,
            joinHorizontal = allSplinesCyclic(self.splines),
            joinVertical = self.cyclic and self.start == 0 and self.end == 1)

    def calcPolygonIndices(self):
        return grid.quadPolygons(
            xDivisions = self.splineSamples,
            yDivisions = self.surfaceSamples,
            joinHorizontal = allSplinesCyclic(self.splines),
            joinVertical = self.cyclic and self.start == 0 and self.end == 1)

def allSplinesCyclic(splines):
    return all(spline.cyclic for spline in splines)
