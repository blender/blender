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

#  Filename : shaders.py
#  Authors  : Fredo Durand, Stephane Grabli, Francois Sillion, Emmanuel Turquin
#  Date     : 11/08/2005
#  Purpose  : Stroke shaders to be used for creation of stylized strokes

"""
This module contains stroke shaders used for creation of stylized
strokes.  It is also intended to be a collection of examples for
shader definition in Python.

User-defined stroke shaders inherit the
:class:`freestyle.types.StrokeShader` class.
"""

__all__ = (
    "BackboneStretcherShader",
    "BezierCurveShader",
    "BlenderTextureShader",
    "CalligraphicShader",
    "ColorNoiseShader",
    "ColorVariationPatternShader",
    "ConstantColorShader",
    "ConstantThicknessShader",
    "ConstrainedIncreasingThicknessShader",
    "GuidingLinesShader",
    "IncreasingColorShader",
    "IncreasingThicknessShader",
    "PolygonalizationShader",
    "RoundCapShader",
    "SamplingShader",
    "SmoothingShader",
    "SpatialNoiseShader",
    "SquareCapShader",
    "StrokeTextureShader",
    "StrokeTextureStepShader",
    "TextureAssignerShader",
    "ThicknessNoiseShader",
    "ThicknessVariationPatternShader",
    "TipRemoverShader",
    "fstreamShader",
    "py2DCurvatureColorShader",
    "pyBackboneStretcherNoCuspShader",
    "pyBackboneStretcherShader",
    "pyBluePrintCirclesShader",
    "pyBluePrintDirectedSquaresShader",
    "pyBluePrintEllipsesShader",
    "pyBluePrintSquaresShader",
    "pyConstantColorShader",
    "pyConstantThicknessShader",
    "pyConstrainedIncreasingThicknessShader",
    "pyDecreasingThicknessShader",
    "pyDepthDiscontinuityThicknessShader",
    "pyDiffusion2Shader",
    "pyFXSVaryingThicknessWithDensityShader",
    "pyGuidingLineShader",
    "pyHLRShader",
    "pyImportance2DThicknessShader",
    "pyImportance3DThicknessShader",
    "pyIncreasingColorShader",
    "pyIncreasingThicknessShader",
    "pyInterpolateColorShader",
    "pyLengthDependingBackboneStretcherShader",
    "pyMaterialColorShader",
    "pyModulateAlphaShader",
    "pyNonLinearVaryingThicknessShader",
    "pyPerlinNoise1DShader",
    "pyPerlinNoise2DShader",
    "pyRandomColorShader",
    "pySLERPThicknessShader",
    "pySamplingShader",
    "pySinusDisplacementShader",
    "pyTVertexRemoverShader",
    "pyTVertexThickenerShader",
    "pyTimeColorShader",
    "pyTipRemoverShader",
    "pyZDependingThicknessShader",
    "streamShader",
    )


# module members
from _freestyle import (
    BackboneStretcherShader,
    BezierCurveShader,
    BlenderTextureShader,
    CalligraphicShader,
    ColorNoiseShader,
    ColorVariationPatternShader,
    ConstantColorShader,
    ConstantThicknessShader,
    ConstrainedIncreasingThicknessShader,
    GuidingLinesShader,
    IncreasingColorShader,
    IncreasingThicknessShader,
    PolygonalizationShader,
    SamplingShader,
    SmoothingShader,
    SpatialNoiseShader,
    StrokeTextureShader,
    StrokeTextureStepShader,
    TextureAssignerShader,
    ThicknessNoiseShader,
    ThicknessVariationPatternShader,
    TipRemoverShader,
    fstreamShader,
    streamShader,
    )

# constructs for shader definition in Python
from freestyle.types import (
    Interface0DIterator,
    Nature,
    Noise,
    StrokeAttribute,
    StrokeShader,
    StrokeVertexIterator,
    StrokeVertex,
    )
from freestyle.functions import (
    Curvature2DAngleF0D,
    DensityF0D,
    GetProjectedZF0D,
    MaterialF0D,
    Normal2DF0D,
    Orientation2DF1D,
    ZDiscontinuityF0D,
    )
from freestyle.predicates import (
    pyVertexNatureUP0D,
    pyUEqualsUP0D,
    )

from freestyle.utils import (
    bound,
    bounding_box,
    phase_to_direction,
    )

from freestyle.utils import ContextFunctions as CF

import bpy
import random

from math import atan, cos, pi, sin, sinh, sqrt
from mathutils import Vector
from random import randint


# -- Thickness Stroke Shaders -- #


class pyDepthDiscontinuityThicknessShader(StrokeShader):
    """
    Assigns a thickness to the stroke based on the stroke's distance
    to the camera (Z-value).
    """
    def __init__(self, min, max):
        StrokeShader.__init__(self)
        self.a = max - min
        self.b = min
        self.func = ZDiscontinuityF0D()

    def shade(self, stroke):
        it = Interface0DIterator(stroke)
        for svert in it:
            z = self.func(it)
            thickness = self.a * z + self.b
            svert.attribute.thickness = (thickness, thickness)


class pyConstantThicknessShader(StrokeShader):
    """
    Assigns a constant thickness along the stroke.
    """
    def __init__(self, thickness):
        StrokeShader.__init__(self)
        self._thickness = thickness / 2.0

    def shade(self, stroke):
        for svert in stroke:
            svert.attribute.thickness = (self._thickness, self._thickness)


class pyFXSVaryingThicknessWithDensityShader(StrokeShader):
    """
    Assings thickness to a stroke based on the density of the diffuse map.
    """
    def __init__(self, wsize, threshold_min, threshold_max, thicknessMin, thicknessMax):
        StrokeShader.__init__(self)
        self._func = DensityF0D(wsize)
        self.threshold_min = threshold_min
        self.threshold_max = threshold_max
        self._thicknessMin = thicknessMin
        self._thicknessMax = thicknessMax

    def shade(self, stroke):
        it = Interface0DIterator(stroke)
        delta_threshold = self.threshold_max - self.threshold_min
        delta_thickness = self._thicknessMax - self._thicknessMin

        for svert in it:
            c = self._func(it)
            c = bound(self.threshold_min, c, self.threshold_max)
            t = (self.threshold_max - c) / delta_threshold * delta_thickness + self._thicknessMin
            svert.attribute.thickness = (t / 2.0, t / 2.0)


class pyIncreasingThicknessShader(StrokeShader):
    """
    Increasingly thickens the stroke.
    """
    def __init__(self, thicknessMin, thicknessMax):
        StrokeShader.__init__(self)
        self._thicknessMin = thicknessMin
        self._thicknessMax = thicknessMax

    def shade(self, stroke):
        n = len(stroke)
        for i, svert in enumerate(stroke):
            c = i / n
            if i < (n * 0.5):
                t = (1.0 - c) * self._thicknessMin + c * self._thicknessMax
            else:
                t = (1.0 - c) * self._thicknessMax + c * self._thicknessMin
            svert.attribute.thickness = (t / 2.0, t / 2.0)


class pyConstrainedIncreasingThicknessShader(StrokeShader):
    """
    Increasingly thickens the stroke, constrained by a ratio of the
    stroke's length.
    """
    def __init__(self, thicknessMin, thicknessMax, ratio):
        StrokeShader.__init__(self)
        self._thicknessMin = thicknessMin
        self._thicknessMax = thicknessMax
        self._ratio = ratio

    def shade(self, stroke):
        n = len(stroke)
        maxT = min(self._ratio * stroke.length_2d, self._thicknessMax)

        for i, svert in enumerate(stroke):
            c = i / n
            if i < (n * 0.5):
                t = (1.0 - c) * self._thicknessMin + c * maxT
            else:
                t = (1.0 - c) * maxT + c * self._thicknessMin

            if i == (n - 1):
                svert.attribute.thickness = (self._thicknessMin / 2.0, self._thicknessMin / 2.0)
            else:
                svert.attribute.thickness = (t / 2.0, t / 2.0)


class pyDecreasingThicknessShader(StrokeShader):
    """
    Inverse of pyIncreasingThicknessShader, decreasingly thickens the stroke.
    """
    def __init__(self, thicknessMin, thicknessMax):
        StrokeShader.__init__(self)
        self._thicknessMin = thicknessMin
        self._thicknessMax = thicknessMax

    def shade(self, stroke):
        l = stroke.length_2d
        n = len(stroke)
        tMax = min(self._thicknessMax, 0.33 * l)
        tMin = min(self._thicknessMin, 0.10 * l)

        for i, svert in enumerate(stroke):
            c = i / n
            t = (1.0 - c) * tMax + c * tMin
            svert.attribute.thickness = (t / 2.0, t / 2.0)


class pyNonLinearVaryingThicknessShader(StrokeShader):
    """
    Assigns thickness to a stroke based on an exponential function.
    """
    def __init__(self, thicknessExtremity, thicknessMiddle, exponent):
        self._thicknessMin = thicknessMiddle
        self._thicknessMax = thicknessExtremity
        self._exp = exponent
        StrokeShader.__init__(self)

    def shade(self, stroke):
        n = len(stroke)
        for i, svert in enumerate(stroke):
            c = (i / n) if (i < n / 2.0) else ((n - i) / n)
            c = pow(c, self._exp) * pow(2.0, self._exp)
            t = (1.0 - c) * self._thicknessMax + c * self._thicknessMin
            svert.attribute.thickness = (t / 2.0, t / 2.0)


class pySLERPThicknessShader(StrokeShader):
    """
    Assigns thickness to a stroke based on spherical linear interpolation.
    """
    def __init__(self, thicknessMin, thicknessMax, omega=1.2):
        StrokeShader.__init__(self)
        self._thicknessMin = thicknessMin
        self._thicknessMax = thicknessMax
        self.omega = omega

    def shade(self, stroke):
        n = len(stroke)
        maxT = min(self._thicknessMax, 0.33 * stroke.length_2d)
        omega = self.omega
        sinhyp = sinh(omega)
        for i, svert in enumerate(stroke):
            c = i / n
            if i < (n * 0.5):
                t = sin((1-c) * omega) / sinhyp * self._thicknessMin + sin(c * omega) / sinhyp * maxT
            else:
                t = sin((1-c) * omega) / sinhyp * maxT + sin(c * omega) / sinhyp * self._thicknessMin
            svert.attribute.thickness = (t / 2.0, t / 2.0)


class pyTVertexThickenerShader(StrokeShader):
    """
    Thickens TVertices (visual intersections between two edges).
    """
    def __init__(self, a=1.5, n=3):
        StrokeShader.__init__(self)
        self._a = a
        self._n = n

    def shade(self, stroke):
        n = self._n
        a = self._a

        term = (a - 1.0) / (n - 1.0)

        if (stroke[0].nature & Nature.T_VERTEX):
            for count, svert in zip(range(n), stroke):
                r = term * (n / (count + 1.0) - 1.0) + 1.0
                (tr, tl) = svert.attribute.thickness
                svert.attribute.thickness = (r * tr, r * tl)

        if (stroke[-1].nature & Nature.T_VERTEX):
            for count, svert in zip(range(n), reversed(stroke)):
                r = term * (n / (count + 1.0) - 1.0) + 1.0
                (tr, tl) = svert.attribute.thickness
                svert.attribute.thickness = (r * tr, r * tl)


class pyImportance2DThicknessShader(StrokeShader):
    """
    Assigns thickness based on distance to a given point in 2D space.
    the thickness is inverted, so the vertices closest to the
    specified point have the lowest thickness.
    """
    def __init__(self, x, y, w, kmin, kmax):
        StrokeShader.__init__(self)
        self._origin = Vector((x, y))
        self._w = w
        self._kmin, self._kmax = kmin, kmax

    def shade(self, stroke):
        for svert in stroke:
            d = (svert.point_2d - self._origin).length
            k = (self._kmin if (d > self._w) else
                (self._kmax * (self._w-d) + self._kmin * d) / self._w)

            (tr, tl) = svert.attribute.thickness
            svert.attribute.thickness = (k*tr/2.0, k*tl/2.0)


class pyImportance3DThicknessShader(StrokeShader):
    """
    Assigns thickness based on distance to a given point in 3D space.
    """
    def __init__(self, x, y, z, w, kmin, kmax):
        StrokeShader.__init__(self)
        self._origin = Vector((x, y, z))
        self._w = w
        self._kmin, self._kmax = kmin, kmax

    def shade(self, stroke):
        for svert in stroke:
            d = (svert.point_3d - self._origin).length
            k = (self._kmin if (d > self._w) else
                (self._kmax * (self._w-d) + self._kmin * d) / self._w)

            (tr, tl) = svert.attribute.thickness
            svert.attribute.thickness = (k*tr/2.0, k*tl/2.0)


class pyZDependingThicknessShader(StrokeShader):
    """
    Assigns thickness based on an object's local Z depth (point
    closest to camera is 1, point furthest from camera is zero).
    """
    def __init__(self, min, max):
        StrokeShader.__init__(self)
        self.__min = min
        self.__max = max
        self.func = GetProjectedZF0D()

    def shade(self, stroke):
        it = Interface0DIterator(stroke)
        z_indices = tuple(self.func(it) for _ in it)
        z_min, z_max = min(1, *z_indices), max(0, *z_indices)
        z_diff = 1 / (z_max - z_min)

        for svert, z_index in zip(stroke, z_indices):
            z = (z_index - z_min) * z_diff
            thickness = (1 - z) * self.__max + z * self.__min
            svert.attribute.thickness = (thickness, thickness)


# -- Color & Alpha Stroke Shaders -- #


class pyConstantColorShader(StrokeShader):
    """
    Assigns a constant color to the stroke.
    """
    def __init__(self,r,g,b, a = 1):
        StrokeShader.__init__(self)
        self._color = (r, g, b)
        self._a = a
    def shade(self, stroke):
        for svert in stroke:
            svert.attribute.color = self._color
            svert.attribute.alpha = self._a


class pyIncreasingColorShader(StrokeShader):
    """
    Fades from one color to another along the stroke.
    """
    def __init__(self,r1,g1,b1,a1, r2,g2,b2,a2):
        StrokeShader.__init__(self)
        # use 4d vector to simplify math
        self._c1 = Vector((r1, g1 ,b1, a1))
        self._c2 = Vector((r2, g2, b2, a2))

    def shade(self, stroke):
        n = len(stroke) - 1

        for i, svert in enumerate(stroke):
            c = i / n
            color = (1 - c) * self._c1 + c * self._c2
            svert.attribute.color = color[:3]
            svert.attribute.alpha = color[3]


class pyInterpolateColorShader(StrokeShader):
    """
    Fades from one color to another and back.
    """
    def __init__(self,r1,g1,b1,a1, r2,g2,b2,a2):
        StrokeShader.__init__(self)
        # use 4d vector to simplify math
        self._c1 = Vector((r1, g1 ,b1, a1))
        self._c2 = Vector((r2, g2, b2, a2))

    def shade(self, stroke):
        n = len(stroke) - 1
        for i, svert in enumerate(stroke):
            c = 1.0 - 2.0 * abs((i / n) - 0.5)
            color = (1.0 - c) * self._c1 + c * self._c2
            svert.attribute.color = color[:3]
            svert.attribute.alpha = color[3]


class pyModulateAlphaShader(StrokeShader):
    """
    Limits the stroke's alpha between a min and max value.
    """
    def __init__(self, min=0, max=1):
        StrokeShader.__init__(self)
        self.__min = min
        self.__max = max
    def shade(self, stroke):
        for svert in stroke:
            alpha = svert.attribute.alpha
            alpha = bound(self.__min, alpha * svert.point.y * 0.0025, self.__max)
            svert.attribute.alpha = alpha


class pyMaterialColorShader(StrokeShader):
    """
    Assigns the color of the underlying material to the stroke.
    """
    def __init__(self, threshold=50):
        StrokeShader.__init__(self)
        self._threshold = threshold
        self._func = MaterialF0D()

    def shade(self, stroke):
        xn = 0.312713
        yn = 0.329016
        Yn = 1.0
        un = 4.0 * xn / (-2.0 * xn + 12.0 * yn + 3.0)
        vn = 9.0 * yn / (-2.0 * xn + 12.0 * yn + 3.0)

        it = Interface0DIterator(stroke)
        for svert in it:
            mat = self._func(it)

            r, g, b, *_ = mat.diffuse

            X = 0.412453 * r + 0.35758 * g + 0.180423 * b
            Y = 0.212671 * r + 0.71516 * g + 0.072169 * b
            Z = 0.019334 * r + 0.11919 * g + 0.950227 * b

            if not any((X, Y, Z)):
                X = Y = Z = 0.01

            u = 4.0 * X / (X + 15.0 * Y + 3.0 * Z)
            v = 9.0 * Y / (X + 15.0 * Y + 3.0 * Z)

            L= 116. * pow((Y/Yn),(1./3.)) - 16
            U = 13. * L * (u - un)
            V = 13. * L * (v - vn)

            if L > self._threshold:
                L /= 1.3
                U += 10.
            else:
                L = L + 2.5 * (100-L) * 0.2
                U /= 3.0
                V /= 3.0

            u = U / (13.0 * L) + un
            v = V / (13.0 * L) + vn

            Y = Yn * pow(((L+16.)/116.), 3.)
            X = -9. * Y * u / ((u - 4.)* v - u * v)
            Z = (9. * Y - 15*v*Y - v*X) /( 3. * v)

            r = 3.240479 * X - 1.53715 * Y - 0.498535 * Z
            g = -0.969256 * X + 1.875991 * Y + 0.041556 * Z
            b = 0.055648 * X - 0.204043 * Y + 1.057311 * Z

            r = max(0, r)
            g = max(0, g)
            b = max(0, b)

            svert.attribute.color = (r, g, b)


class pyRandomColorShader(StrokeShader):
    """
    Assigns a color to the stroke based on given seed.
    """
    def __init__(self, s=1):
        StrokeShader.__init__(self)
        random.seed = s

    def shade(self, stroke):
        c = (random.uniform(15, 75) * 0.01,
             random.uniform(15, 75) * 0.01,
             random.uniform(15, 75) * 0.01)
        for svert in stroke:
            svert.attribute.color = c


class py2DCurvatureColorShader(StrokeShader):
    """
    Assigns a color (greyscale) to the stroke based on the curvature.
    A higher curvature will yield a brighter color.
    """
    def shade(self, stroke):
        func = Curvature2DAngleF0D()
        it = Interface0DIterator(stroke)
        for svert in it:
            c = func(it)
            if c < 0 and bpy.app.debug_freestyle:
                print("py2DCurvatureColorShader: negative 2D curvature")
            color = 10.0 * c / pi
            svert.attribute.color = (color, color, color)


class pyTimeColorShader(StrokeShader):
    """
    Assigns a greyscale value that increases for every vertex.
    The brightness will increase along the stroke.
    """
    def __init__(self, step=0.01):
        StrokeShader.__init__(self)
        self._step = step
    def shade(self, stroke):
        for i, svert in enumerate(stroke):
            c = i * self._step
            svert.attribute.color = (c, c, c)


# -- Geometry Stroke Shaders -- #


class pySamplingShader(StrokeShader):
    """
    Resamples the stroke, which gives the stroke the ammount of
    vertices specified.
    """
    def __init__(self, sampling):
        StrokeShader.__init__(self)
        self._sampling = sampling

    def shade(self, stroke):
        stroke.resample(float(self._sampling))
        stroke.update_length()


class pyBackboneStretcherShader(StrokeShader):
    """
    Stretches the stroke's backbone by a given length (in pixels).
    """
    def __init__(self, l):
        StrokeShader.__init__(self)
        self._l = l

    def shade(self, stroke):
        # get start and end points
        v0, vn = stroke[0], stroke[-1]
        p0, pn = v0.point, vn.point
        # get the direction
        d1 = (p0 - stroke[ 1].point).normalized()
        dn = (pn - stroke[-2].point).normalized()
        v0.point += d1 * self._l
        vn.point += dn * self._l
        stroke.update_length()


class pyLengthDependingBackboneStretcherShader(StrokeShader):
    """
    Stretches the stroke's backbone proportional to the stroke's length
    NOTE: you'll probably want an l somewhere between (0.5 - 0). A value that
    is too high may yield unexpected results.
    """
    def __init__(self, l):
        StrokeShader.__init__(self)
        self._l = l
    def shade(self, stroke):
        # get start and end points
        v0, vn = stroke[0], stroke[-1]
        p0, pn = v0.point, vn.point
        # get the direction
        d1 = (p0 - stroke[ 1].point).normalized()
        dn = (pn - stroke[-2].point).normalized()
        v0.point += d1 * self._l * stroke.length_2d
        vn.point += dn * self._l * stroke.length_2d
        stroke.update_length()


class pyGuidingLineShader(StrokeShader):
    def shade(self, stroke):
        # get the tangent direction
        t = stroke[-1].point - stroke[0].point
        # look for the stroke middle vertex
        itmiddle = iter(stroke)
        while itmiddle.object.u < 0.5:
            itmiddle.increment()
        center_vertex = itmiddle.object
        # position all the vertices along the tangent for the right part
        it = StrokeVertexIterator(itmiddle)
        for svert in it:
            svert.point = center_vertex.point + t * (svert.u - center_vertex.u)
        # position all the vertices along the tangent for the left part
        it = StrokeVertexIterator(itmiddle).reversed()
        for svert in it:
            svert.point = center_vertex.point - t * (center_vertex.u - svert.u)
        stroke.update_length()


class pyBackboneStretcherNoCuspShader(StrokeShader):
    """
    Stretches the stroke's backbone, excluding cusp vertices (end junctions).
    """
    def __init__(self, l):
        StrokeShader.__init__(self)
        self._l = l

    def shade(self, stroke):

        v0, v1 = stroke[0], stroke[1]
        vn, vn_1 = stroke[-1], stroke[-2]

        if not (v0.nature & v1.nature & Nature.CUSP):
            d1 = (v0.point - v1.point).normalized()
            v0.point += d1 * self._l

        if not (vn.nature & vn_1.nature & Nature.CUSP):
            dn = (vn.point - vn_1.point).normalized()
            vn.point += dn * self._l

        stroke.update_length()


class pyDiffusion2Shader(StrokeShader):
    """
    Iteratively adds an offset to the position of each stroke vertex
    in the direction perpendicular to the stroke direction at the
    point. The offset is scaled by the 2D curvature (i.e. how quickly
    the stroke curve is) at the point.
    """
    def __init__(self, lambda1, nbIter):
        StrokeShader.__init__(self)
        self._lambda = lambda1
        self._nbIter = nbIter
        self._normalInfo = Normal2DF0D()
        self._curvatureInfo = Curvature2DAngleF0D()

    def shade(self, stroke):
        for i in range (1, self._nbIter):
            it = Interface0DIterator(stroke)
            for svert in it:
                svert.point += self._normalInfo(it) * self._lambda * self._curvatureInfo(it)
        stroke.update_length()


class pyTipRemoverShader(StrokeShader):
    """
    Removes the tips of the stroke.
    """
    def __init__(self, l):
        StrokeShader.__init__(self)
        self._l = l

    @staticmethod
    def check_vertex(v, length):
        # Returns True if the given strokevertex is less than self._l away
        # from the stroke's tip and therefore should be removed.
        return (v.curvilinear_abscissa < length or v.stroke_length-v.curvilinear_abscissa < length)

    def shade(self, stroke):
        n = len(stroke)
        if n < 4:
            return

        verticesToRemove = tuple(svert for svert in stroke if self.check_vertex(svert, self._l))
        # explicit conversion to StrokeAttribute is needed
        oldAttributes = (StrokeAttribute(svert.attribute) for svert in stroke)

        if n - len(verticesToRemove) < 2:
            return

        for sv in verticesToRemove:
            stroke.remove_vertex(sv)

        stroke.update_length()
        stroke.resample(n)
        if len(stroke) != n and bpy.app.debug_freestyle:
            print("pyTipRemover: Warning: resampling problem")

        for svert, a in zip(stroke, oldAttributes):
            svert.attribute = a
        stroke.update_length()


class pyTVertexRemoverShader(StrokeShader):
    """
    Removes t-vertices from the stroke.
    """
    def shade(self, stroke):
        if len(stroke) < 4:
            return

        v0, vn = stroke[0], stroke[-1]
        if (v0.nature & Nature.T_VERTEX):
            stroke.remove_vertex(v0)
        if (vn.nature & Nature.T_VERTEX):
            stroke.remove_vertex(vn)
        stroke.update_length()


class pyHLRShader(StrokeShader):
    """
    Controlls visibility based upon the quantative invisibility (QI)
    based on hidden line removal (HLR).
    """
    def shade(self, stroke):
        if len(stroke) < 4:
            return

        it = iter(stroke)
        for v1, v2 in zip(it, it.incremented()):
            if (v1.nature & Nature.VIEW_VERTEX):
                visible = (v1.get_fedge(v2).viewedge.qi != 0)
            v1.attribute.visible = not visible


class pySinusDisplacementShader(StrokeShader):
    """
    Displaces the stroke in the shape of a sine wave.
    """
    def __init__(self, f, a):
        StrokeShader.__init__(self)
        self._f = f
        self._a = a
        self._getNormal = Normal2DF0D()

    def shade(self, stroke):
        it = Interface0DIterator(stroke)
        for svert in it:
            normal = self._getNormal(it)
            a = self._a * (1 - 2 * (abs(svert.u - 0.5)))
            n = normal * a * cos(self._f * svert.u * 6.28)
            svert.point += n
        stroke.update_length()


class pyPerlinNoise1DShader(StrokeShader):
    """
    Displaces the stroke using the curvilinear abscissa.  This means
    that lines with the same length and sampling interval will be
    identically distorded.
    """
    def __init__(self, freq=10, amp=10, oct=4, seed=-1):
        StrokeShader.__init__(self)
        self.__noise = Noise(seed)
        self.__freq = freq
        self.__amp = amp
        self.__oct = oct

    def shade(self, stroke):
        for svert in stroke:
            s = svert.projected_x + svert.projected_y
            nres = self.__noise.turbulence1(s, self.__freq, self.__amp, self.__oct)
            svert.point = (svert.projected_x + nres, svert.projected_y + nres)
        stroke.update_length()


class pyPerlinNoise2DShader(StrokeShader):
    """
    Displaces the stroke using the strokes coordinates.  This means
    that in a scene no strokes will be distorded identically.

    More information on the noise shaders can be found at:
    freestyleintegration.wordpress.com/2011/09/25/development-updates-on-september-25/
    """
    def __init__(self, freq=10, amp=10, oct=4, seed=-1):
        StrokeShader.__init__(self)
        self.__noise = Noise(seed)
        self.__freq = freq
        self.__amp = amp
        self.__oct = oct

    def shade(self, stroke):
        for svert in stroke:
            nres = self.__noise.turbulence2(svert.point_2d, self.__freq, self.__amp, self.__oct)
            svert.point = (svert.projected_x + nres, svert.projected_y + nres)
        stroke.update_length()


class pyBluePrintCirclesShader(StrokeShader):
    """
    Draws the silhouette of the object as a circle.
    """
    def __init__(self, turns=1, random_radius=3, random_center=5):
        StrokeShader.__init__(self)
        self.__turns = turns
        self.__random_center = random_center
        self.__random_radius = random_radius

    def shade(self, stroke):
        # get minimum and maximum coordinates
        p_min, p_max = bounding_box(stroke)

        stroke.resample(32 * self.__turns)
        sv_nb = len(stroke) // self.__turns
        center = (p_min + p_max) / 2
        radius = (center.x - p_min.x + center.y - p_min.y) / 2
        R = self.__random_radius
        C = self.__random_center

        # The directions (and phases) are calculated using a seperate
        # function decorated with an lru-cache. This guarantees that
        # the directions (involving sin and cos) are calculated as few
        # times as possible.
        #
        # This works because the phases and directions are only
        # dependant on the stroke length, and the chance that
        # stroke.resample() above produces strokes of the same length
        # is quite high.
        #
        # In tests, the amount of calls to sin() and cos() went from
        # over 21000 to just 32 times, yielding a speedup of over 100%
        directions = phase_to_direction(sv_nb)

        it = iter(stroke)

        for j in range(self.__turns):
            prev_radius = radius
            prev_center = center
            radius += randint(-R, R)
            center += Vector((randint(-C, C), randint(-C, C)))

            for (phase, direction), svert in zip(directions, it):
                r = prev_radius + (radius - prev_radius) * phase
                c = prev_center + (center - prev_center) * phase
                svert.point = c + r * direction

        if not it.is_end:
            it.increment()
            for sv in tuple(it):
                stroke.remove_vertex(sv)

        stroke.update_length()


class pyBluePrintEllipsesShader(StrokeShader):
    def __init__(self, turns=1, random_radius=3, random_center=5):
        StrokeShader.__init__(self)
        self.__turns = turns
        self.__random_center = random_center
        self.__random_radius = random_radius

    def shade(self, stroke):
        p_min, p_max = bounding_box(stroke)

        stroke.resample(32 * self.__turns)
        sv_nb = len(stroke) // self.__turns

        center = (p_min + p_max) / 2
        radius = center - p_min

        R = self.__random_radius
        C = self.__random_center

        # for description of the line below, see pyBluePrintCirclesShader
        directions = phase_to_direction(sv_nb)
        it = iter(stroke)
        for j in range(self.__turns):
            prev_radius = radius
            prev_center = center
            radius = radius + Vector((randint(-R, R), randint(-R, R)))
            center = center + Vector((randint(-C, C), randint(-C, C)))

            for (phase, direction), svert in zip(directions, it):
                r = prev_radius + (radius - prev_radius) * phase
                c = prev_center + (center - prev_center) * phase
                svert.point = (c.x + r.x * direction.x, c.y + r.y * direction.y)

        # remove exessive vertices
        if not it.is_end:
            it.increment()
            for sv in tuple(it):
                stroke.remove_vertex(sv)

        stroke.update_length()


class pyBluePrintSquaresShader(StrokeShader):
    def __init__(self, turns=1, bb_len=10, bb_rand=0):
        StrokeShader.__init__(self)
        self.__turns = turns # does not have any effect atm
        self.__bb_len = bb_len
        self.__bb_rand = bb_rand

    def shade(self, stroke):
        # this condition will lead to errors later, end now
        if len(stroke) < 1:
            return

        # get minimum and maximum coordinates
        p_min, p_max = bounding_box(stroke)

        stroke.resample(32 * self.__turns)
        num_segments = len(stroke) // self.__turns
        f = num_segments // 4
        # indices of the vertices that will form corners
        first, second, third, fourth = (f, f * 2, f * 3, num_segments)

        # construct points of the backbone
        bb_len = self.__bb_len
        points = (
            Vector((p_min.x - bb_len, p_min.y)),
            Vector((p_max.x + bb_len, p_min.y)),
            Vector((p_max.x, p_min.y - bb_len)),
            Vector((p_max.x, p_max.y + bb_len)),
            Vector((p_max.x + bb_len, p_max.y)),
            Vector((p_min.x - bb_len, p_max.y)),
            Vector((p_min.x, p_max.y + bb_len)),
            Vector((p_min.x, p_min.y - bb_len)),
            )

        # add randomization to the points (if needed)
        if self.__bb_rand:
            R, r = self.__bb_rand, self.__bb_rand // 2

            randomization_mat = (
                Vector((randint(-R, R), randint(-r, r))),
                Vector((randint(-R, R), randint(-r, r))),
                Vector((randint(-r, r), randint(-R, R))),
                Vector((randint(-r, r), randint(-R, R))),
                Vector((randint(-R, R), randint(-r, r))),
                Vector((randint(-R, R), randint(-r, r))),
                Vector((randint(-r, r), randint(-R, R))),
                Vector((randint(-r, r), randint(-R, R))),
                )

            # combine both tuples
            points = tuple(p + rand for (p, rand) in zip(points, randomization_mat))


        # substract even from uneven; result is length four tuple of vectors
        it = iter(points)
        old_vecs = tuple(next(it) - current for current in it)

        it = iter(stroke)
        verticesToRemove = list()
        for j in range(self.__turns):
            for i, svert in zip(range(num_segments), it):
                if i < first:
                    svert.point = points[0] + old_vecs[0] * i / (first - 1)
                    svert.attribute.visible = (i != first - 1)
                elif i < second:
                    svert.point = points[2] + old_vecs[1] * (i - first) / (second - first - 1)
                    svert.attribute.visible = (i != second - 1)
                elif i < third:
                    svert.point = points[4] + old_vecs[2] * (i - second) / (third - second - 1)
                    svert.attribute.visible = (i != third - 1)
                elif i < fourth:
                    svert.point = points[6] + old_vecs[3] * (i - third) / (fourth - third - 1)
                    svert.attribute.visible = (i != fourth - 1)
                else:
                    # special case; remove these vertices
                    verticesToRemove.append(svert)

        # remove exessive vertices (if any)
        if not it.is_end:
            it.increment()
            verticesToRemove += [svert for svert in it]
            for sv in verticesToRemove:
                stroke.remove_vertex(sv)
        stroke.update_length()


class pyBluePrintDirectedSquaresShader(StrokeShader):
    """
    Replaces the stroke with a directed square.
    """
    def __init__(self, turns=1, bb_len=10, mult=1):
        StrokeShader.__init__(self)
        self.__mult = mult
        self.__turns = turns
        self.__bb_len = 1 + bb_len * 0.01

    def shade(self, stroke):
        stroke.resample(32 * self.__turns)
        n = len(stroke)

        p_mean = (1 / n) * sum((svert.point for svert in stroke), Vector((0.0, 0.0)))
        p_var = Vector((0, 0))
        p_var_xy = 0.0
        for d in (svert.point - p_mean for svert in stroke):
            p_var += Vector((d.x ** 2, d.y ** 2))
            p_var_xy += d.x * d.y

        # divide by number of vertices
        p_var /= n
        p_var_xy /= n
        trace = p_var.x + p_var.y
        det = p_var.x * p_var.y - pow(p_var_xy, 2)

        sqrt_coeff = sqrt(trace * trace - 4 * det)
        lambda1, lambda2 = (trace + sqrt_coeff) / 2, (trace - sqrt_coeff) / 2
        # make sure those numers aren't to small, if they are, rooting them will yield complex numbers
        lambda1, lambda2 = max(1e-12, lambda1), max(1e-12, lambda2)
        theta = atan(2 * p_var_xy / (p_var.x - p_var.y)) / 2

        if p_var.y > p_var.x:
            e1 = Vector((cos(theta + pi / 2), sin(theta + pi / 2))) * sqrt(lambda1) * self.__mult
            e2 = Vector((cos(theta + pi    ), sin(theta + pi    ))) * sqrt(lambda2) * self.__mult
        else:
            e1 = Vector((cos(theta), sin(theta)))                   * sqrt(lambda1) * self.__mult
            e2 = Vector((cos(theta + pi / 2), sin(theta + pi / 2))) * sqrt(lambda2) * self.__mult

        # partition the stroke
        num_segments = len(stroke) // self.__turns
        f = num_segments // 4
        # indices of the vertices that will form corners
        first, second, third, fourth = (f, f * 2, f * 3, num_segments)

        bb_len1 = self.__bb_len
        bb_len2 = 1 + (bb_len1 - 1) * sqrt(lambda1 / lambda2)
        points = (
            p_mean - e1 - e2 * bb_len2,
            p_mean - e1 * bb_len1 + e2,
            p_mean + e1 + e2 * bb_len2,
            p_mean + e1 * bb_len1 - e2,
            )

        old_vecs = (
            e2 * bb_len2 * 2,
            e1 * bb_len1 * 2,
           -e2 * bb_len2 * 2,
           -e1 * bb_len1 * 2,
            )

        it = iter(stroke)
        verticesToRemove = list()
        for j in range(self.__turns):
            for i, svert in zip(range(num_segments), it):
                if i < first:
                    svert.point = points[0] + old_vecs[0] * i / (first - 1)
                    svert.attribute.visible = (i != first - 1)
                elif i < second:
                    svert.point = points[1] + old_vecs[1] * (i - first) / (second - first - 1)
                    svert.attribute.visible = (i != second - 1)
                elif i < third:
                    svert.point = points[2] + old_vecs[2] * (i - second) / (third - second - 1)
                    svert.attribute.visible = (i != third - 1)
                elif i < fourth:
                    svert.point = points[3] + old_vecs[3] * (i - third) / (fourth - third - 1)
                    svert.attribute.visible = (i != fourth - 1)
                else:
                    # special case; remove these vertices
                    verticesToRemove.append(svert)

        # remove exessive vertices
        if not it.is_end:
            it.increment()
            verticesToRemove += [svert for svert in it]
            for sv in verticesToRemove:
                stroke.remove_vertex(sv)
        stroke.update_length()


# -- various (used in the parameter editor) -- #


class RoundCapShader(StrokeShader):
    def round_cap_thickness(self, x):
        x = max(0.0, min(x, 1.0))
        return pow(1.0 - (x ** 2.0), 0.5)

    def shade(self, stroke):
        # save the location and attribute of stroke vertices
        buffer = tuple((Vector(sv.point), StrokeAttribute(sv.attribute)) for sv in stroke)
        nverts = len(buffer)
        if nverts < 2:
            return
        # calculate the number of additional vertices to form caps
        thickness_beg = sum(stroke[0].attribute.thickness)
        caplen_beg = thickness_beg / 2.0
        nverts_beg = max(5, int(thickness_beg))

        thickness_end = sum(stroke[-1].attribute.thickness)
        caplen_end = (thickness_end) / 2.0
        nverts_end = max(5, int(thickness_end))

        # adjust the total number of stroke vertices
        stroke.resample(nverts + nverts_beg + nverts_end)
        # restore the location and attribute of the original vertices
        for i, (p, attr) in enumerate(buffer):
            stroke[nverts_beg + i].point = p
            stroke[nverts_beg + i].attribute = attr
        # reshape the cap at the beginning of the stroke
        q, attr = buffer[1]
        p, attr = buffer[0]
        direction = (p - q).normalized() * caplen_beg
        n = 1.0 / nverts_beg
        R, L = attr.thickness
        for t, svert in zip(range(nverts_beg, 0, -1), stroke):
            r = self.round_cap_thickness((t + 1) * n)
            svert.point = p + direction * t * n
            svert.attribute = attr
            svert.attribute.thickness = (R * r, L * r)
        # reshape the cap at the end of the stroke
        q, attr = buffer[-2]
        p, attr = buffer[-1]
        direction = (p - q).normalized() * caplen_beg
        n = 1.0 / nverts_end
        R, L = attr.thickness
        for t, svert in zip(range(nverts_end, 0, -1), reversed(stroke)):
            r = self.round_cap_thickness((t + 1) * n)
            svert.point = p + direction * t * n
            svert.attribute = attr
            svert.attribute.thickness = (R * r, L * r)
        # update the curvilinear 2D length of each vertex
        stroke.update_length()


class SquareCapShader(StrokeShader):
    def shade(self, stroke):
        # save the location and attribute of stroke vertices
        buffer = tuple((Vector(sv.point), StrokeAttribute(sv.attribute)) for sv in stroke)
        nverts = len(buffer)
        if nverts < 2:
            return
        # calculate the number of additional vertices to form caps
        caplen_beg = sum(stroke[0].attribute.thickness) / 2.0
        nverts_beg = 1

        caplen_end = sum(stroke[-1].attribute.thickness) / 2.0
        nverts_end = 1
        # adjust the total number of stroke vertices
        stroke.resample(nverts + nverts_beg + nverts_end)
        # restore the location and attribute of the original vertices
        for i, (p, attr) in zip(range(nverts), buffer):
            stroke[nverts_beg + i].point = p
            stroke[nverts_beg + i].attribute = attr
        # reshape the cap at the beginning of the stroke
        q, attr = buffer[1]
        p, attr = buffer[0]
        stroke[0].point += (p - q).normalized() * caplen_beg
        stroke[0].attribute = attr
        # reshape the cap at the end of the stroke
        q, attr = buffer[-2]
        p, attr = buffer[-1]
        stroke[-1].point += (p - q).normalized() * caplen_end
        stroke[-1].attribute = attr
        # update the curvilinear 2D length of each vertex
        stroke.update_length()
