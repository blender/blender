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

"""
Functions operating on vertices (0D elements) and polylines (1D
elements).  Also intended to be a collection of examples for predicate
definition in Python.
"""

# module members
from _freestyle import (
    ChainingTimeStampF1D,
    Curvature2DAngleF0D,
    Curvature2DAngleF1D,
    CurveNatureF0D,
    CurveNatureF1D,
    DensityF0D,
    DensityF1D,
    GetCompleteViewMapDensityF1D,
    GetCurvilinearAbscissaF0D,
    GetDirectionalViewMapDensityF1D,
    GetOccludeeF0D,
    GetOccludeeF1D,
    GetOccludersF0D,
    GetOccludersF1D,
    GetParameterF0D,
    GetProjectedXF0D,
    GetProjectedXF1D,
    GetProjectedYF0D,
    GetProjectedYF1D,
    GetProjectedZF0D,
    GetProjectedZF1D,
    GetShapeF0D,
    GetShapeF1D,
    GetSteerableViewMapDensityF1D,
    GetViewMapGradientNormF0D,
    GetViewMapGradientNormF1D,
    GetXF0D,
    GetXF1D,
    GetYF0D,
    GetYF1D,
    GetZF0D,
    GetZF1D,
    IncrementChainingTimeStampF1D,
    LocalAverageDepthF0D,
    LocalAverageDepthF1D,
    MaterialF0D,
    Normal2DF0D,
    Normal2DF1D,
    Orientation2DF1D,
    Orientation3DF1D,
    QuantitativeInvisibilityF0D,
    QuantitativeInvisibilityF1D,
    ReadCompleteViewMapPixelF0D,
    ReadMapPixelF0D,
    ReadSteerableViewMapPixelF0D,
    ShapeIdF0D,
    TimeStampF1D,
    VertexOrientation2DF0D,
    VertexOrientation3DF0D,
    ZDiscontinuityF0D,
    ZDiscontinuityF1D,
    )

# constructs for function definition in Python
from freestyle.types import (
    CurvePoint,
    IntegrationType,
    UnaryFunction0DDouble,
    UnaryFunction0DMaterial,
    UnaryFunction0DVec2f,
    UnaryFunction1DDouble,
    )
from freestyle.utils import ContextFunctions as CF
from freestyle.utils import integrate

from mathutils import Vector

# -- Functions for 0D elements (vertices) -- #


class CurveMaterialF0D(UnaryFunction0DMaterial):
    """
    A replacement of the built-in MaterialF0D for stroke creation.
    MaterialF0D does not work with Curves and Strokes.  Line color
    priority is used to pick one of the two materials at material
    boundaries.

    Note: expects instances of CurvePoint to be iterated over
    """
    def __call__(self, inter):
        fe = inter.object.fedge
        assert(fe is not None), "CurveMaterialF0D: fe is None"
        if fe.is_smooth:
            return fe.material
        else:
            right, left = fe.material_right, fe.material_left
            return right if (right.priority > left.priority) else left


class pyInverseCurvature2DAngleF0D(UnaryFunction0DDouble):
    def __call__(self, inter):
        func = Curvature2DAngleF0D()
        c = func(inter)
        return (3.1415 - c)


class pyCurvilinearLengthF0D(UnaryFunction0DDouble):
    def __call__(self, inter):
        cp = inter.object
        assert(isinstance(cp, CurvePoint))
        return cp.t2d


class pyDensityAnisotropyF0D(UnaryFunction0DDouble):
    """Estimates the anisotropy of density."""
    def __init__(self, level):
        UnaryFunction0DDouble.__init__(self)
        self.IsoDensity = ReadCompleteViewMapPixelF0D(level)
        self.d0Density = ReadSteerableViewMapPixelF0D(0, level)
        self.d1Density = ReadSteerableViewMapPixelF0D(1, level)
        self.d2Density = ReadSteerableViewMapPixelF0D(2, level)
        self.d3Density = ReadSteerableViewMapPixelF0D(3, level)

    def __call__(self, inter):
        c_iso = self.IsoDensity(inter)
        c_0 = self.d0Density(inter)
        c_1 = self.d1Density(inter)
        c_2 = self.d2Density(inter)
        c_3 = self.d3Density(inter)
        cMax = max(max(c_0, c_1), max(c_2, c_3))
        cMin = min(min(c_0, c_1), min(c_2, c_3))
        return 0 if (c_iso == 0) else (cMax - cMin) / c_iso


class pyViewMapGradientVectorF0D(UnaryFunction0DVec2f):
    """Returns the gradient vector for a pixel.

    :arg level: the level at which to compute the gradient
    :type level: int
    """
    def __init__(self, level):
        UnaryFunction0DVec2f.__init__(self)
        self._l = level
        self._step = pow(2, self._l)

    def __call__(self, iter):
        p = iter.object.point_2d
        gx = CF.read_complete_view_map_pixel(self._l, int(p.x + self._step), int(p.y)) - \
             CF.read_complete_view_map_pixel(self._l, int(p.x), int(p.y))
        gy = CF.read_complete_view_map_pixel(self._l, int(p.x), int(p.y + self._step)) - \
             CF.read_complete_view_map_pixel(self._l, int(p.x), int(p.y))
        return Vector((gx, gy))


class pyViewMapGradientNormF0D(UnaryFunction0DDouble):
    def __init__(self, l):
        UnaryFunction0DDouble.__init__(self)
        self._l = l
        self._step = pow(2, self._l)

    def __call__(self, iter):
        p = iter.object.point_2d
        gx = CF.read_complete_view_map_pixel(self._l, int(p.x + self._step), int(p.y)) - \
             CF.read_complete_view_map_pixel(self._l, int(p.x), int(p.y))
        gy = CF.read_complete_view_map_pixel(self._l, int(p.x), int(p.y + self._step)) - \
             CF.read_complete_view_map_pixel(self._l, int(p.x), int(p.y))
        return Vector((gx, gy)).length

# -- Functions for 1D elements (curves) -- #


class pyGetInverseProjectedZF1D(UnaryFunction1DDouble):
    def __call__(self, inter):
        func = GetProjectedZF1D()
        z = func(inter)
        return (1.0 - z)


class pyGetSquareInverseProjectedZF1D(UnaryFunction1DDouble):
    def __call__(self, inter):
        func = GetProjectedZF1D()
        z = func(inter)
        return (1.0 - pow(z, 2))


class pyDensityAnisotropyF1D(UnaryFunction1DDouble):
    def __init__(self, level, integrationType=IntegrationType.MEAN, sampling=2.0):
        UnaryFunction1DDouble.__init__(self, integrationType)
        self._func = pyDensityAnisotropyF0D(level)
        self._integration = integrationType
        self._sampling = sampling

    def __call__(self, inter):
        v = integrate(self._func, inter.points_begin(self._sampling), inter.points_end(self._sampling), self._integration)
        return v


class pyViewMapGradientNormF1D(UnaryFunction1DDouble):
    def __init__(self, l, integrationType, sampling=2.0):
        UnaryFunction1DDouble.__init__(self, integrationType)
        self._func = pyViewMapGradientNormF0D(l)
        self._integration = integrationType
        self._sampling = sampling

    def __call__(self, inter):
        v = integrate(self._func, inter.points_begin(self._sampling), inter.points_end(self._sampling), self._integration)
        return v
