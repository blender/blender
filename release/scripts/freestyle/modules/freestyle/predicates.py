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
Predicates operating on vertices (0D elements) and polylines (1D
elements).  Also intended to be a collection of examples for predicate
definition in Python
"""

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
    IntegrationType,
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


## Unary predicates for 0D elements (vertices)
##############################################

class pyHigherCurvature2DAngleUP0D(UnaryPredicate0D):
    def __init__(self,a):
        UnaryPredicate0D.__init__(self)
        self._a = a

    def __call__(self, inter):
        func = Curvature2DAngleF0D()
        a = func(inter)
        return (a > self._a)


class pyUEqualsUP0D(UnaryPredicate0D):
    def __init__(self,u, w):
        UnaryPredicate0D.__init__(self)
        self._u = u
        self._w = w

    def __call__(self, inter):
        func = pyCurvilinearLengthF0D()
        u = func(inter)
        return (u > (self._u-self._w)) and (u < (self._u+self._w))


class pyVertexNatureUP0D(UnaryPredicate0D):
    def __init__(self,nature):
        UnaryPredicate0D.__init__(self)
        self._nature = nature

    def __call__(self, inter):
        v = inter.object
        return (v.nature & self._nature) != 0


## check whether an Interface0DIterator
## is a TVertex and is the one that is
## hidden (inferred from the context)
class pyBackTVertexUP0D(UnaryPredicate0D):
    def __init__(self):
        UnaryPredicate0D.__init__(self)
        self._getQI = QuantitativeInvisibilityF0D()

    def __call__(self, iter):
        if (iter.object.nature & Nature.T_VERTEX) == 0:
            return False
        if iter.is_end:
            return False
        if self._getQI(iter) != 0:
            return True
        return False


class pyParameterUP0DGoodOne(UnaryPredicate0D):
    def __init__(self,pmin,pmax):
        UnaryPredicate0D.__init__(self)
        self._m = pmin
        self._M = pmax

    def __call__(self, inter):
        u = inter.u
        return ((u>=self._m) and (u<=self._M))


class pyParameterUP0D(UnaryPredicate0D):
    def __init__(self,pmin,pmax):
        UnaryPredicate0D.__init__(self)
        self._m = pmin
        self._M = pmax

    def __call__(self, inter):
        func = Curvature2DAngleF0D()
        c = func(inter)
        b1 = (c>0.1)
        u = inter.u
        b = ((u>=self._m) and (u<=self._M))
        return b and b1

## Unary predicates for 1D elements (curves)
############################################


class AndUP1D(UnaryPredicate1D):
    def __init__(self, pred1, pred2):
        UnaryPredicate1D.__init__(self)
        self.__pred1 = pred1
        self.__pred2 = pred2

    def __call__(self, inter):
        return self.__pred1(inter) and self.__pred2(inter)


class OrUP1D(UnaryPredicate1D):
    def __init__(self, pred1, pred2):
        UnaryPredicate1D.__init__(self)
        self.__pred1 = pred1
        self.__pred2 = pred2

    def __call__(self, inter):
        return self.__pred1(inter) or self.__pred2(inter)


class NotUP1D(UnaryPredicate1D):
    def __init__(self, pred):
        UnaryPredicate1D.__init__(self)
        self.__pred = pred

    def __call__(self, inter):
        return not self.__pred(inter)


class pyNFirstUP1D(UnaryPredicate1D):
    def __init__(self, n):
        UnaryPredicate1D.__init__(self)
        self.__n = n
        self.__count = 0

    def __call__(self, inter):
        self.__count = self.__count + 1
        if self.__count <= self.__n:
            return True
        return False


class pyHigherLengthUP1D(UnaryPredicate1D):
    def __init__(self,l):
        UnaryPredicate1D.__init__(self)
        self._l = l

    def __call__(self, inter):
        return (inter.length_2d > self._l)


class pyNatureUP1D(UnaryPredicate1D):
    def __init__(self,nature):
        UnaryPredicate1D.__init__(self)
        self._nature = nature
        self._getNature = CurveNatureF1D()

    def __call__(self, inter):
        if(self._getNature(inter) & self._nature):
            return True
        return False


class pyHigherNumberOfTurnsUP1D(UnaryPredicate1D):
    def __init__(self,n,a):
        UnaryPredicate1D.__init__(self)
        self._n = n
        self._a = a

    def __call__(self, inter):
        count = 0
        func = Curvature2DAngleF0D()
        it = inter.vertices_begin()
        while not it.is_end:
            if func(it) > self._a:
                count = count+1
            if count > self._n:
                return True
            it.increment()
        return False


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
        self._level = level
        self._integration = integration
        self._func = GetSteerableViewMapDensityF1D(self._level, self._integration)

    def __call__(self, inter):
        return (self._func(inter) > self._threshold)


class pyHighDirectionalViewMapDensityUP1D(UnaryPredicate1D):
    def __init__(self, threshold, orientation, level, integration=IntegrationType.MEAN, sampling=2.0):
        UnaryPredicate1D.__init__(self)
        self._threshold = threshold
        self._orientation = orientation
        self._level = level
        self._integration = integration
        self._sampling = sampling

    def __call__(self, inter):
        func = GetDirectionalViewMapDensityF1D(self._orientation, self._level, self._integration, self._sampling)
        return (func(inter) > self._threshold)


class pyHighViewMapDensityUP1D(UnaryPredicate1D):
    def __init__(self, threshold, level, integration=IntegrationType.MEAN, sampling=2.0):
        UnaryPredicate1D.__init__(self)
        self._threshold = threshold
        self._level = level
        self._integration = integration
        self._sampling = sampling
        self._func = GetCompleteViewMapDensityF1D(self._level, self._integration, self._sampling) # 2.0 is the smpling

    def __call__(self, inter):
        return (self._func(inter) > self._threshold)


class pyDensityFunctorUP1D(UnaryPredicate1D):
    def __init__(self, wsize, threshold, functor, funcmin=0.0, funcmax=1.0, integration=IntegrationType.MEAN):
        UnaryPredicate1D.__init__(self)
        self._wsize = wsize
        self._threshold = float(threshold)
        self._functor = functor
        self._funcmin = float(funcmin)
        self._funcmax = float(funcmax)
        self._integration = integration

    def __call__(self, inter):
        func = DensityF1D(self._wsize, self._integration)
        res = self._functor(inter)
        k = (res-self._funcmin)/(self._funcmax-self._funcmin)
        return (func(inter) < (self._threshold * k))


class pyZSmallerUP1D(UnaryPredicate1D):
    def __init__(self,z, integration=IntegrationType.MEAN):
        UnaryPredicate1D.__init__(self)
        self._z = z
        self._integration = integration

    def __call__(self, inter):
        func = GetProjectedZF1D(self._integration)
        return (func(inter) < self._z)


class pyIsOccludedByUP1D(UnaryPredicate1D):
    def __init__(self,id):
        UnaryPredicate1D.__init__(self)
        self._id = id

    def __call__(self, inter):
        func = GetShapeF1D()
        shapes = func(inter)
        for s in shapes:
            if(s.id == self._id):
                return False
        it = inter.vertices_begin()
        itlast = inter.vertices_end()
        itlast.decrement()
        v = it.object
        vlast = itlast.object
        tvertex = v.viewvertex
        if type(tvertex) is TVertex:
            #print("TVertex: [ ", tvertex.id.first, ",",  tvertex.id.second," ]")
            eit = tvertex.edges_begin()
            while not eit.is_end:
                ve, incoming = eit.object
                if ve.id == self._id:
                    return True
                #print("-------", ve.id.first, "-", ve.id.second)
                eit.increment()
        tvertex = vlast.viewvertex
        if type(tvertex) is TVertex:
            #print("TVertex: [ ", tvertex.id.first, ",",  tvertex.id.second," ]")
            eit = tvertex.edges_begin()
            while not eit.is_end:
                ve, incoming = eit.object
                if ve.id == self._id:
                    return True
                #print("-------", ve.id.first, "-", ve.id.second)
                eit.increment()
        return False


class pyIsInOccludersListUP1D(UnaryPredicate1D):
    def __init__(self,id):
        UnaryPredicate1D.__init__(self)
        self._id = id

    def __call__(self, inter):
        func = GetOccludersF1D()
        occluders = func(inter)
        for a in occluders:
            if a.id == self._id:
                return True
        return False


class pyIsOccludedByItselfUP1D(UnaryPredicate1D):
    def __init__(self):
        UnaryPredicate1D.__init__(self)
        self.__func1 = GetOccludersF1D()
        self.__func2 = GetShapeF1D()

    def __call__(self, inter):
        lst1 = self.__func1(inter)
        lst2 = self.__func2(inter)
        for vs1 in lst1:
            for vs2 in lst2:
                if vs1.id == vs2.id:
                    return True
        return False


class pyIsOccludedByIdListUP1D(UnaryPredicate1D):
    def __init__(self, idlist):
        UnaryPredicate1D.__init__(self)
        self._idlist = idlist
        self.__func1 = GetOccludersF1D()

    def __call__(self, inter):
        lst1 = self.__func1(inter)
        for vs1 in lst1:
            for _id in self._idlist:
                if vs1.id == _id:
                    return True
        return False


class pyShapeIdListUP1D(UnaryPredicate1D):
    def __init__(self,idlist):
        UnaryPredicate1D.__init__(self)
        self._idlist = idlist
        self._funcs = []
        for _id in idlist:
            self._funcs.append(ShapeUP1D(_id.first, _id.second))

    def __call__(self, inter):
        for func in self._funcs:
            if func(inter) == 1:
                return True
        return False


## deprecated
class pyShapeIdUP1D(UnaryPredicate1D):
    def __init__(self, _id):
        UnaryPredicate1D.__init__(self)
        self._id = _id

    def __call__(self, inter):
        func = GetShapeF1D()
        shapes = func(inter)
        for a in shapes:
            if a.id == self._id:
                return True
        return False


class pyHighDensityAnisotropyUP1D(UnaryPredicate1D):
    def __init__(self,threshold, level, sampling=2.0):
        UnaryPredicate1D.__init__(self)
        self._l = threshold
        self.func = pyDensityAnisotropyF1D(level, IntegrationType.MEAN, sampling)

    def __call__(self, inter):
        return (self.func(inter) > self._l)


class pyHighViewMapGradientNormUP1D(UnaryPredicate1D):
    def __init__(self,threshold, l, sampling=2.0):
        UnaryPredicate1D.__init__(self)
        self._threshold = threshold
        self._GetGradient = pyViewMapGradientNormF1D(l, IntegrationType.MEAN)

    def __call__(self, inter):
        gn = self._GetGradient(inter)
        #print(gn)
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
        sigma = (self._sigmaMax-self._sigmaMin)/(self._lmax-self._lmin)*(self._functor(inter)-self._lmin) + self._sigmaMin
        t = (self._tmax-self._tmin)/(self._lmax-self._lmin)*(self._functor(inter)-self._lmin) + self._tmin
        sigma = max(sigma, self._sigmaMin)
        self._func = DensityF1D(sigma, self._integration, self._sampling)
        return (self._func(inter) < t)


class pyClosedCurveUP1D(UnaryPredicate1D):
    def __call__(self, inter):
        it = inter.vertices_begin()
        itlast = inter.vertices_end()
        itlast.decrement()
        vlast = itlast.object
        v = it.object
        #print(v.id.first, v.id.second)
        #print(vlast.id.first, vlast.id.second)
        if v.id == vlast.id:
            return True
        return False

## Binary predicates for 1D elements (curves)
#############################################


class pyZBP1D(BinaryPredicate1D):
    def __call__(self, i1, i2):
        func = GetZF1D()
        return (func(i1) > func(i2))


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
        if (bpred(inter1, inter2) != 1):
            return False
        if (inter1.nature & Nature.SILHOUETTE):
            return (inter2.nature & Nature.SILHOUETTE) != 0
        return (inter1.nature == inter2.nature)


class pyNatureBP1D(BinaryPredicate1D):
    def __call__(self, inter1, inter2):
        return (inter1.nature & inter2.nature)


class pyViewMapGradientNormBP1D(BinaryPredicate1D):
    def __init__(self,l, sampling=2.0):
        BinaryPredicate1D.__init__(self)
        self._GetGradient = pyViewMapGradientNormF1D(l, IntegrationType.MEAN)

    def __call__(self, i1,i2):
        #print("compare gradient")
        return (self._GetGradient(i1) > self._GetGradient(i2))


class pyShuffleBP1D(BinaryPredicate1D):
    def __init__(self):
        BinaryPredicate1D.__init__(self)
        random.seed(1)

    def __call__(self, inter1, inter2):
        r1 = random.uniform(0,1)
        r2 = random.uniform(0,1)
        return (r1<r2)
