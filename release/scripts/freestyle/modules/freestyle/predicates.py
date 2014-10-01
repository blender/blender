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
This module contains predicates operating on vertices (0D elements)
and polylines (1D elements).  It is also intended to be a collection
of examples for predicate definition in Python.

User-defined predicates inherit one of the following base classes,
depending on the object type (0D or 1D) to operate on and the arity
(unary or binary):

- :class:`freestyle.types.BinaryPredicate0D`
- :class:`freestyle.types.BinaryPredicate1D`
- :class:`freestyle.types.UnaryPredicate0D`
- :class:`freestyle.types.UnaryPredicate1D`
"""

__all__ = (
    "AndBP1D",
    "AndUP1D",
    "ContourUP1D",
    "DensityLowerThanUP1D",
    "EqualToChainingTimeStampUP1D",
    "EqualToTimeStampUP1D",
    "ExternalContourUP1D",
    "FalseBP1D",
    "FalseUP0D",
    "FalseUP1D",
    "Length2DBP1D",
    "NotBP1D",
    "NotUP1D",
    "ObjectNamesUP1D",
    "OrBP1D",
    "OrUP1D",
    "QuantitativeInvisibilityRangeUP1D",
    "QuantitativeInvisibilityUP1D",
    "SameShapeIdBP1D",
    "ShapeUP1D",
    "TrueBP1D",
    "TrueUP0D",
    "TrueUP1D",
    "ViewMapGradientNormBP1D",
    "WithinImageBoundaryUP1D",
    "pyBackTVertexUP0D",
    "pyClosedCurveUP1D",
    "pyDensityFunctorUP1D",
    "pyDensityUP1D",
    "pyDensityVariableSigmaUP1D",
    "pyHighDensityAnisotropyUP1D",
    "pyHighDirectionalViewMapDensityUP1D",
    "pyHighSteerableViewMapDensityUP1D",
    "pyHighViewMapDensityUP1D",
    "pyHighViewMapGradientNormUP1D",
    "pyHigherCurvature2DAngleUP0D",
    "pyHigherLengthUP1D",
    "pyHigherNumberOfTurnsUP1D",
    "pyIsInOccludersListUP1D",
    "pyIsOccludedByIdListUP1D",
    "pyIsOccludedByItselfUP1D",
    "pyIsOccludedByUP1D",
    "pyLengthBP1D",
    "pyLowDirectionalViewMapDensityUP1D",
    "pyLowSteerableViewMapDensityUP1D",
    "pyNFirstUP1D",
    "pyNatureBP1D",
    "pyNatureUP1D",
    "pyParameterUP0D",
    "pyParameterUP0DGoodOne",
    "pyShapeIdListUP1D",
    "pyShapeIdUP1D",
    "pyShuffleBP1D",
    "pySilhouetteFirstBP1D",
    "pyUEqualsUP0D",
    "pyVertexNatureUP0D",
    "pyViewMapGradientNormBP1D",
    "pyZBP1D",
    "pyZDiscontinuityBP1D",
    "pyZSmallerUP1D",
    )


# module members
from _freestyle import (
    ContourUP1D,
    DensityLowerThanUP1D,
    EqualToChainingTimeStampUP1D,
    EqualToTimeStampUP1D,
    ExternalContourUP1D,
    FalseBP1D,
    FalseUP0D,
    FalseUP1D,
    Length2DBP1D,
    QuantitativeInvisibilityUP1D,
    SameShapeIdBP1D,
    ShapeUP1D,
    TrueBP1D,
    TrueUP0D,
    TrueUP1D,
    ViewMapGradientNormBP1D,
    WithinImageBoundaryUP1D,
    )

# constructs for predicate definition in Python
from freestyle.types import (
    BinaryPredicate1D,
    Id,
    IntegrationType,
    Interface0DIterator,
    Nature,
    TVertex,
    UnaryPredicate0D,
    UnaryPredicate1D,
    )
from freestyle.functions import (
    Curvature2DAngleF0D,
    CurveNatureF1D,
    DensityF1D,
    GetCompleteViewMapDensityF1D,
    GetCurvilinearAbscissaF0D,
    GetDirectionalViewMapDensityF1D,
    GetOccludersF1D,
    GetProjectedZF1D,
    GetShapeF1D,
    GetSteerableViewMapDensityF1D,
    GetZF1D,
    QuantitativeInvisibilityF0D,
    ZDiscontinuityF1D,
    pyCurvilinearLengthF0D,
    pyDensityAnisotropyF1D,
    pyViewMapGradientNormF1D,
    )

import random


# -- Unary predicates for 0D elements (vertices) -- #


class pyHigherCurvature2DAngleUP0D(UnaryPredicate0D):
    def __init__(self, a):
        UnaryPredicate0D.__init__(self)
        self._a = a
        self.func = Curvature2DAngleF0D()

    def __call__(self, inter):
        return (self.func(inter) > self._a)


class pyUEqualsUP0D(UnaryPredicate0D):
    def __init__(self, u, w):
        UnaryPredicate0D.__init__(self)
        self._u = u
        self._w = w
        self._func = pyCurvilinearLengthF0D()

    def __call__(self, inter):
        u = self._func(inter)
        return (u > (self._u - self._w)) and (u < (self._u + self._w))


class pyVertexNatureUP0D(UnaryPredicate0D):
    def __init__(self, nature):
        UnaryPredicate0D.__init__(self)
        self._nature = nature

    def __call__(self, inter):
        return bool(inter.object.nature & self._nature)


class pyBackTVertexUP0D(UnaryPredicate0D):
    """
    Check whether an Interface0DIterator references a TVertex and is
    the one that is hidden (inferred from the context).
    """
    def __init__(self):
        UnaryPredicate0D.__init__(self)
        self._getQI = QuantitativeInvisibilityF0D()

    def __call__(self, iter):
        if not (iter.object.nature & Nature.T_VERTEX) or iter.is_end:
            return False
        return self._getQI(iter) != 0


class pyParameterUP0DGoodOne(UnaryPredicate0D):
    def __init__(self, pmin, pmax):
        UnaryPredicate0D.__init__(self)
        self._m = pmin
        self._M = pmax

    def __call__(self, inter):
        u = inter.u
        return ((u >= self._m) and (u <= self._M))


class pyParameterUP0D(UnaryPredicate0D):
    def __init__(self, pmin, pmax):
        UnaryPredicate0D.__init__(self)
        self._m = pmin
        self._M = pmax
        self._func = Curvature2DAngleF0D()

    def __call__(self, inter):
        c = self._func(inter)
        b1 = (c > 0.1)
        u = inter.u
        b = ((u >= self._m) and (u <= self._M))
        return (b and b1)


# -- Unary predicates for 1D elements (curves) -- #

class AndUP1D(UnaryPredicate1D):
    def __init__(self, *predicates):
        UnaryPredicate1D.__init__(self)
        self.predicates = predicates
        # there are cases in which only one predicate is supplied (in the parameter editor)
        if len(self.predicates) < 1:
            raise ValueError("Expected one or more UnaryPredicate1D, got ", len(predicates))

    def __call__(self, inter):
        return all(pred(inter) for pred in self.predicates)


class OrUP1D(UnaryPredicate1D):
    def __init__(self, *predicates):
        UnaryPredicate1D.__init__(self)
        self.predicates = predicates
        # there are cases in which only one predicate is supplied (in the parameter editor)
        if len(self.predicates) < 1:
            raise ValueError("Expected one or more UnaryPredicate1D, got ", len(predicates))

    def __call__(self, inter):
        return any(pred(inter) for pred in self.predicates)


class NotUP1D(UnaryPredicate1D):
    def __init__(self, pred):
        UnaryPredicate1D.__init__(self)
        self.__pred = pred

    def __call__(self, inter):
        return not self.__pred(inter)


class ObjectNamesUP1D(UnaryPredicate1D):
    def __init__(self, names, negative=False):
        UnaryPredicate1D.__init__(self)
        self._names = names
        self._negative = negative

    def __call__(self, viewEdge):
        found = viewEdge.viewshape.name in self._names
        return found if not self._negative else not found


class QuantitativeInvisibilityRangeUP1D(UnaryPredicate1D):
    def __init__(self, qi_start, qi_end):
        UnaryPredicate1D.__init__(self)
        self.__getQI = QuantitativeInvisibilityF1D()
        self.__qi_start = qi_start
        self.__qi_end = qi_end

    def __call__(self, inter):
        qi = self.__getQI(inter)
        return (self.__qi_start <= qi <= self.__qi_end)


class pyNFirstUP1D(UnaryPredicate1D):
    def __init__(self, n):
        UnaryPredicate1D.__init__(self)
        self.__n = n
        self.__count = 0

    def __call__(self, inter):
        self.__count += 1
        return (self.__count <= self.__n)


class pyHigherLengthUP1D(UnaryPredicate1D):
    def __init__(self, l):
        UnaryPredicate1D.__init__(self)
        self._l = l

    def __call__(self, inter):
        return (inter.length_2d > self._l)


class pyNatureUP1D(UnaryPredicate1D):
    def __init__(self, nature):
        UnaryPredicate1D.__init__(self)
        self._nature = nature
        self._getNature = CurveNatureF1D()

    def __call__(self, inter):
        return bool(self._getNature(inter) & self._nature)


class pyHigherNumberOfTurnsUP1D(UnaryPredicate1D):
    def __init__(self, n, a):
        UnaryPredicate1D.__init__(self)
        self._n = n
        self._a = a
        self.func = Curvature2DAngleF0D()

    def __call__(self, inter):
        it = Interface0DIterator(inter)
        # sum the turns, check against n
        return sum(1 for _ in it if self.func(it) > self._a) > self._n
        # interesting fact, the line above is 70% faster than:
        # return sum(self.func(it) > self._a for _ in it) > self._n


class pyDensityUP1D(UnaryPredicate1D):
    def __init__(self, wsize, threshold, integration=IntegrationType.MEAN, sampling=2.0):
        UnaryPredicate1D.__init__(self)
        self._wsize = wsize
        self._threshold = threshold
        self._integration = integration
        self._func = DensityF1D(self._wsize, self._integration, sampling)

    def __call__(self, inter):
        return (self._func(inter) < self._threshold)


class pyLowSteerableViewMapDensityUP1D(UnaryPredicate1D):
    def __init__(self, threshold, level, integration=IntegrationType.MEAN):
        UnaryPredicate1D.__init__(self)
        self._threshold = threshold
        self._level = level
        self._integration = integration

    def __call__(self, inter):
        func = GetSteerableViewMapDensityF1D(self._level, self._integration)
        return (func(inter) < self._threshold)


class pyLowDirectionalViewMapDensityUP1D(UnaryPredicate1D):
    def __init__(self, threshold, orientation, level, integration=IntegrationType.MEAN):
        UnaryPredicate1D.__init__(self)
        self._threshold = threshold
        self._orientation = orientation
        self._level = level
        self._integration = integration

    def __call__(self, inter):
        func = GetDirectionalViewMapDensityF1D(self._orientation, self._level, self._integration)
        return (func(inter) < self._threshold)


class pyHighSteerableViewMapDensityUP1D(UnaryPredicate1D):
    def __init__(self, threshold, level, integration=IntegrationType.MEAN):
        UnaryPredicate1D.__init__(self)
        self._threshold = threshold
        self._func = GetSteerableViewMapDensityF1D(level, integration)

    def __call__(self, inter):
        return (self._func(inter) > self._threshold)


class pyHighDirectionalViewMapDensityUP1D(UnaryPredicate1D):
    def __init__(self, threshold, orientation, level, integration=IntegrationType.MEAN, sampling=2.0):
        UnaryPredicate1D.__init__(self)
        self._threshold = threshold
        self._func = GetDirectionalViewMapDensityF1D(orientation, level, integration, sampling)

    def __call__(self, inter):
        return (self.func(inter) > self._threshold)


class pyHighViewMapDensityUP1D(UnaryPredicate1D):
    def __init__(self, threshold, level, integration=IntegrationType.MEAN, sampling=2.0):
        UnaryPredicate1D.__init__(self)
        self._threshold = threshold
        self._func = GetCompleteViewMapDensityF1D(level, integration, sampling)

    def __call__(self, inter):
        return (self._func(inter) > self._threshold)


class pyDensityFunctorUP1D(UnaryPredicate1D):
    def __init__(self, wsize, threshold, functor, funcmin=0.0, funcmax=1.0, integration=IntegrationType.MEAN):
        UnaryPredicate1D.__init__(self)
        self._threshold = float(threshold)
        self._functor = functor
        self._funcmin = float(funcmin)
        self._funcmax = float(funcmax)
        self._func = DensityF1D(wsize, integration)

    def __call__(self, inter):
        res = self._functor(inter)
        k = (res - self._funcmin) / (self._funcmax - self._funcmin)
        return (func(inter) < (self._threshold * k))


class pyZSmallerUP1D(UnaryPredicate1D):
    def __init__(self, z, integration=IntegrationType.MEAN):
        UnaryPredicate1D.__init__(self)
        self._z = z
        self.func = GetProjectedZF1D(integration)

    def __call__(self, inter):
        return (self.func(inter) < self._z)


class pyIsOccludedByUP1D(UnaryPredicate1D):
    def __init__(self, id):
        UnaryPredicate1D.__init__(self)
        if not isinstance(id, Id):
            raise TypeError("pyIsOccludedByUP1D expected freestyle.types.Id, not " + type(id).__name__)
        self._id = id

    def __call__(self, inter):
        shapes = GetShapeF1D()(inter)
        if any(s.id == self._id for s in shapes):
            return False

        # construct iterators
        it = inter.vertices_begin()
        itlast = inter.vertices_end()
        itlast.decrement()

        vertex = next(it)
        if type(vertex) is TVertex:
            eit = vertex.edges_begin()
            if any(ve.id == self._id for (ve, incoming) in eit):
                return True

        vertex = next(itlast)
        if type(vertex) is TVertex:
            eit = tvertex.edges_begin()
            if any(ve.id == self._id for (ve, incoming) in eit):
                return True
        return False


class pyIsInOccludersListUP1D(UnaryPredicate1D):
    def __init__(self, id):
        UnaryPredicate1D.__init__(self)
        self._id = id

    def __call__(self, inter):
        occluders = GetOccludersF1D()(inter)
        return any(a.id == self._id for a in occluders)


class pyIsOccludedByItselfUP1D(UnaryPredicate1D):
    def __init__(self):
        UnaryPredicate1D.__init__(self)
        self.__func1 = GetOccludersF1D()
        self.__func2 = GetShapeF1D()

    def __call__(self, inter):
        lst1 = self.__func1(inter)
        lst2 = self.__func2(inter)
        return any(vs1.id == vs2.id for vs1 in lst1 for vs2 in lst2)


class pyIsOccludedByIdListUP1D(UnaryPredicate1D):
    def __init__(self, idlist):
        UnaryPredicate1D.__init__(self)
        self._idlist = idlist
        self.__func1 = GetOccludersF1D()

    def __call__(self, inter):
        lst1 = self.__func1(inter.object)
        return any(vs1.id == _id for vs1 in lst1 for _id in self._idlist)


class pyShapeIdListUP1D(UnaryPredicate1D):
    def __init__(self, idlist):
        UnaryPredicate1D.__init__(self)
        self._funcs = tuple(ShapeUP1D(_id, 0) for _id in idlist)

    def __call__(self, inter):
        return any(func(inter) for func in self._funcs)


# DEPRECATED
class pyShapeIdUP1D(UnaryPredicate1D):
    def __init__(self, _id):
        UnaryPredicate1D.__init__(self)
        self._id = _id

    def __call__(self, inter):
        shapes = GetShapeF1D()(inter)
        return any(a.id == self._id for a in shapes)


class pyHighDensityAnisotropyUP1D(UnaryPredicate1D):
    def __init__(self, threshold, level, sampling=2.0):
        UnaryPredicate1D.__init__(self)
        self._l = threshold
        self.func = pyDensityAnisotropyF1D(level, IntegrationType.MEAN, sampling)

    def __call__(self, inter):
        return (self.func(inter) > self._l)


class pyHighViewMapGradientNormUP1D(UnaryPredicate1D):
    def __init__(self, threshold, l, sampling=2.0):
        UnaryPredicate1D.__init__(self)
        self._threshold = threshold
        self._GetGradient = pyViewMapGradientNormF1D(l, IntegrationType.MEAN)

    def __call__(self, inter):
        gn = self._GetGradient(inter)
        return (gn > self._threshold)


class pyDensityVariableSigmaUP1D(UnaryPredicate1D):
    def __init__(self, functor, sigmaMin, sigmaMax, lmin, lmax, tmin, tmax, integration=IntegrationType.MEAN, sampling=2.0):
        UnaryPredicate1D.__init__(self)
        self._functor = functor
        self._sigmaMin = float(sigmaMin)
        self._sigmaMax = float(sigmaMax)
        self._lmin = float(lmin)
        self._lmax = float(lmax)
        self._tmin = tmin
        self._tmax = tmax
        self._integration = integration
        self._sampling = sampling

    def __call__(self, inter):
        result = self._functor(inter) - self._lmin
        sigma = (self._sigmaMax - self._sigmaMin) / (self._lmax - self._lmin) * result + self._sigmaMin
        t = (self._tmax - self._tmin) / (self._lmax - self._lmin) * result + self._tmin
        sigma = max(sigma, self._sigmaMin)
        self._func = DensityF1D(sigma, self._integration, self._sampling)
        return (self._func(inter) < t)


class pyClosedCurveUP1D(UnaryPredicate1D):
    def __call__(self, inter):
        it = inter.vertices_begin()
        itlast = inter.vertices_end()
        itlast.decrement()
        return (next(it).id == next(itlast).id)


# -- Binary predicates for 1D elements (curves) -- #

class AndBP1D(BinaryPredicate1D):
    def __init__(self, *predicates):
        BinaryPredicate1D.__init__(self)
        self._predicates = predicates
        if len(self.predicates) < 2:
            raise ValueError("Expected two or more BinaryPredicate1D, got ", len(predictates))

    def __call__(self, i1, i2):
        return all(pred(i1, i2) for pred in self._predicates)


class OrBP1D(BinaryPredicate1D):
    def __init__(self, *predicates):
        BinaryPredicate1D.__init__(self)
        self._predicates = predicates
        if len(self.predicates) < 2:
            raise ValueError("Expected two or more BinaryPredicate1D, got ", len(predictates))

    def __call__(self, i1, i2):
        return any(pred(i1, i2) for pred in self._predicates)


class NotBP1D(BinaryPredicate1D):
    def __init__(self, predicate):
        BinaryPredicate1D.__init__(self)
        self._predicate = predicate

    def __call__(self, i1, i2):
        return (not self._predicate(i1, i2))


class pyZBP1D(BinaryPredicate1D):
    def __init__(self, iType=IntegrationType.MEAN):
        BinaryPredicate1D.__init__(self)
        self.func = GetZF1D(iType)

    def __call__(self, i1, i2):
        return (self.func(i1) > self.func(i2))


class pyZDiscontinuityBP1D(BinaryPredicate1D):
    def __init__(self, iType=IntegrationType.MEAN):
        BinaryPredicate1D.__init__(self)
        self._GetZDiscontinuity = ZDiscontinuityF1D(iType)

    def __call__(self, i1, i2):
        return (self._GetZDiscontinuity(i1) > self._GetZDiscontinuity(i2))


class pyLengthBP1D(BinaryPredicate1D):
    def __call__(self, i1, i2):
        return (i1.length_2d > i2.length_2d)


class pySilhouetteFirstBP1D(BinaryPredicate1D):
    def __call__(self, inter1, inter2):
        bpred = SameShapeIdBP1D()
        if (not bpred(inter1, inter2)):
            return False
        if (inter1.nature & Nature.SILHOUETTE):
            return bool(inter2.nature & Nature.SILHOUETTE)
        return (inter1.nature == inter2.nature)


class pyNatureBP1D(BinaryPredicate1D):
    def __call__(self, inter1, inter2):
        return (inter1.nature & inter2.nature)


class pyViewMapGradientNormBP1D(BinaryPredicate1D):
    def __init__(self, l, sampling=2.0):
        BinaryPredicate1D.__init__(self)
        self._GetGradient = pyViewMapGradientNormF1D(l, IntegrationType.MEAN)

    def __call__(self, i1, i2):
        return (self._GetGradient(i1) > self._GetGradient(i2))


class pyShuffleBP1D(BinaryPredicate1D):
    def __init__(self):
        BinaryPredicate1D.__init__(self)
        random.seed = 1

    def __call__(self, inter1, inter2):
        return (random.uniform(0, 1) < random.uniform(0, 1))
