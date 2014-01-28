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
Stroke shaders used for creation of stylized strokes.  Also intended
to be a collection of examples for shader definition in Python
"""

# module members
from _freestyle import (
    BackboneStretcherShader,
    BezierCurveShader,
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
    )
from freestyle.utils import ContextFunctions as CF

from math import atan, cos, pi, pow, sin, sinh, sqrt
from mathutils import Vector
from random import randint


## thickness modifiers
######################

class pyDepthDiscontinuityThicknessShader(StrokeShader):
    """
    Assigns a thickness to the stroke based on the stroke's distance
    to the camera (Z-value)
    """
    def __init__(self, min, max):
        StrokeShader.__init__(self)
        self.__min = float(min)
        self.__max = float(max)
        self.__func = ZDiscontinuityF0D()

    def shade(self, stroke):
        z_min=0.0
        z_max=1.0
        a = (self.__max - self.__min)/(z_max-z_min)
        b = (self.__min*z_max-self.__max*z_min)/(z_max-z_min)
        it = stroke.stroke_vertices_begin()
        while not it.is_end:
            z = self.__func(Interface0DIterator(it))
            thickness = a*z+b
            it.object.attribute.thickness = (thickness, thickness)
            it.increment()


class pyConstantThicknessShader(StrokeShader):
    """
    Assigns a constant thickness along the stroke
    """
    def __init__(self, thickness):
        StrokeShader.__init__(self)
        self._thickness = thickness

    def shade(self, stroke):
        it = stroke.stroke_vertices_begin()
        while not it.is_end:
            t = self._thickness/2.0
            it.object.attribute.thickness = (t, t)
            it.increment()


class pyFXSVaryingThicknessWithDensityShader(StrokeShader):
    """
    Assings thickness to a stroke based on the density of the diffuse map
    """
    def __init__(self, wsize, threshold_min, threshold_max, thicknessMin, thicknessMax):
        StrokeShader.__init__(self)
        self.wsize= wsize
        self.threshold_min= threshold_min
        self.threshold_max= threshold_max
        self._thicknessMin = thicknessMin
        self._thicknessMax = thicknessMax

    def shade(self, stroke):
        n = stroke.stroke_vertices_size()
        i = 0
        it = stroke.stroke_vertices_begin()
        func = DensityF0D(self.wsize)
        while not it.is_end:
            c = func(Interface0DIterator(it))
            if c < self.threshold_min:
                c = self.threshold_min
            if c > self.threshold_max:
                c = self.threshold_max
##          t = (c - self.threshold_min)/(self.threshold_max - self.threshold_min)*(self._thicknessMax-self._thicknessMin) + self._thicknessMin
            t = (self.threshold_max - c  )/(self.threshold_max - self.threshold_min)*(self._thicknessMax-self._thicknessMin) + self._thicknessMin
            it.object.attribute.thickness = (t/2.0, t/2.0)
            i = i+1
            it.increment()


class pyIncreasingThicknessShader(StrokeShader):
    """
    Increasingly thickens the stroke
    """
    def __init__(self, thicknessMin, thicknessMax):
        StrokeShader.__init__(self)
        self._thicknessMin = thicknessMin
        self._thicknessMax = thicknessMax

    def shade(self, stroke):
        n = stroke.stroke_vertices_size()
        i = 0
        it = stroke.stroke_vertices_begin()
        while not it.is_end:
            c = float(i)/float(n)
            if i < float(n)/2.0:
                t = (1.0 - c)*self._thicknessMin + c * self._thicknessMax
            else:
                t = (1.0 - c)*self._thicknessMax + c * self._thicknessMin
            it.object.attribute.thickness = (t/2.0, t/2.0)
            i = i+1
            it.increment()


class pyConstrainedIncreasingThicknessShader(StrokeShader):
    """
    Increasingly thickens the stroke, constrained by a ratio of the
    stroke's length
    """
    def __init__(self, thicknessMin, thicknessMax, ratio):
        StrokeShader.__init__(self)
        self._thicknessMin = thicknessMin
        self._thicknessMax = thicknessMax
        self._ratio = ratio

    def shade(self, stroke):
        slength = stroke.length_2d
        tmp = self._ratio*slength
        maxT = 0.0
        if tmp < self._thicknessMax:
            maxT = tmp
        else:
            maxT = self._thicknessMax
        n = stroke.stroke_vertices_size()
        i = 0
        it = stroke.stroke_vertices_begin()
        while not it.is_end:
            att = it.object.attribute
            c = float(i)/float(n)
            if i < float(n)/2.0:
                t = (1.0 - c)*self._thicknessMin + c * maxT
            else:
                t = (1.0 - c)*maxT + c * self._thicknessMin
            att.thickness = (t/2.0, t/2.0)
            if i == n-1:
                att.thickness = (self._thicknessMin/2.0, self._thicknessMin/2.0)
            i = i+1
            it.increment()


class pyDecreasingThicknessShader(StrokeShader):
    """
    Inverse of pyIncreasingThicknessShader, decreasingly thickens the stroke
    """
    def __init__(self, thicknessMin, thicknessMax):
        StrokeShader.__init__(self)
        self._thicknessMin = thicknessMin
        self._thicknessMax = thicknessMax

    def shade(self, stroke):
        l = stroke.length_2d
        tMax = self._thicknessMax
        if self._thicknessMax > 0.33*l:
            tMax = 0.33*l
        tMin = self._thicknessMin
        if self._thicknessMin > 0.1*l:
            tMin = 0.1*l
        n = stroke.stroke_vertices_size()
        i = 0
        it = stroke.stroke_vertices_begin()
        while not it.is_end:
            c = float(i)/float(n)
            t = (1.0 - c)*tMax +c*tMin
            it.object.attribute.thickness = (t/2.0, t/2.0)
            i = i+1
            it.increment()


class pyNonLinearVaryingThicknessShader(StrokeShader):
    """
    Assigns thickness to a stroke based on an exponential function
    """
    def __init__(self, thicknessExtremity, thicknessMiddle, exponent):
        StrokeShader.__init__(self)
        self._thicknessMin = thicknessMiddle
        self._thicknessMax = thicknessExtremity
        self._exponent = exponent

    def shade(self, stroke):
        n = stroke.stroke_vertices_size()
        i = 0
        it = stroke.stroke_vertices_begin()
        while not it.is_end:
            if i < float(n)/2.0:
                c = float(i)/float(n)
            else:
                c = float(n-i)/float(n)
            c = self.smoothC(c, self._exponent)
            t = (1.0 - c)*self._thicknessMax + c * self._thicknessMin
            it.object.attribute.thickness = (t/2.0, t/2.0)
            i = i+1
            it.increment()

    def smoothC(self, a, exp):
        return pow(float(a), exp) * pow(2.0, exp)


class pySLERPThicknessShader(StrokeShader):
    """
    Assigns thickness to a stroke based on spherical linear interpolation
    """
    def __init__(self, thicknessMin, thicknessMax, omega=1.2):
        StrokeShader.__init__(self)
        self._thicknessMin = thicknessMin
        self._thicknessMax = thicknessMax
        self._omega = omega

    def shade(self, stroke):
        slength = stroke.length_2d
        tmp = 0.33*slength
        maxT = self._thicknessMax
        if tmp < self._thicknessMax:
            maxT = tmp
        n = stroke.stroke_vertices_size()
        i = 0
        it = stroke.stroke_vertices_begin()
        while not it.is_end:
            c = float(i)/float(n)
            if i < float(n)/2.0:
                t = sin((1-c)*self._omega)/sinh(self._omega)*self._thicknessMin + sin(c*self._omega)/sinh(self._omega) * maxT
            else:
                t = sin((1-c)*self._omega)/sinh(self._omega)*maxT + sin(c*self._omega)/sinh(self._omega) * self._thicknessMin
            it.object.attribute.thickness = (t/2.0, t/2.0)
            i = i+1
            it.increment()


class pyTVertexThickenerShader(StrokeShader): ## FIXME
    """
    Thickens TVertices (visual intersections between two edges)
    """
    def __init__(self, a=1.5, n=3):
        StrokeShader.__init__(self)
        self._a = a
        self._n = n

    def shade(self, stroke):
        it = stroke.stroke_vertices_begin()
        predTVertex = pyVertexNatureUP0D(Nature.T_VERTEX)
        while not it.is_end:
            if predTVertex(it) == 1:
                it2 = StrokeVertexIterator(it)
                it2.increment()
                if not (it.is_begin or it2.is_end):
                    it.increment()
                    continue
                n = self._n
                a = self._a
                if it.is_begin:
                    it3 = StrokeVertexIterator(it)
                    count = 0
                    while (not it3.is_end) and count < n:
                        att = it3.object.attribute
                        (tr, tl) = att.thickness
                        r = (a-1.0)/float(n-1)*(float(n)/float(count+1) - 1) + 1
                        #r = (1.0-a)/float(n-1)*count + a
                        att.thickness = (r*tr, r*tl)
                        it3.increment()
                        count = count + 1
                if it2.is_end:
                    it4 = StrokeVertexIterator(it)
                    count = 0
                    while (not it4.is_begin) and count < n:
                        att = it4.object.attribute
                        (tr, tl) = att.thickness
                        r = (a-1.0)/float(n-1)*(float(n)/float(count+1) - 1) + 1
                        #r = (1.0-a)/float(n-1)*count + a
                        att.thickness = (r*tr, r*tl)
                        it4.decrement()
                        count = count + 1
                    if it4.is_begin:
                        att = it4.object.attribute
                        (tr, tl) = att.thickness
                        r = (a-1.0)/float(n-1)*(float(n)/float(count+1) - 1) + 1
                        #r = (1.0-a)/float(n-1)*count + a
                        att.thickness = (r*tr, r*tl)
            it.increment()


class pyImportance2DThicknessShader(StrokeShader):
    """
    Assigns thickness based on distance to a given point in 2D space.
    the thickness is inverted, so the vertices closest to the
    specified point have the lowest thickness
    """
    def __init__(self, x, y, w, kmin, kmax):
        StrokeShader.__init__(self)
        self._x = x
        self._y = y
        self._w = float(w)
        self._kmin = float(kmin)
        self._kmax = float(kmax)

    def shade(self, stroke):
        origin = Vector((self._x, self._y))
        it = stroke.stroke_vertices_begin()
        while not it.is_end:
            v = it.object
            d = (v.point_2d - self._origin).length
            if d > self._w:
                k = self._kmin
            else:
                k = (self._kmax*(self._w-d) + self._kmin*d)/self._w
            att = v.attribute
            (tr, tl) = att.thickness
            att.thickness = (k*tr/2.0, k*tl/2.0)
            it.increment()


class pyImportance3DThicknessShader(StrokeShader):
    """
    Assigns thickness based on distance to a given point in 3D space
    """
    def __init__(self, x, y, z, w, kmin, kmax):
        StrokeShader.__init__(self)
        self._x = x
        self._y = y
        self._z = z
        self._w = float(w)
        self._kmin = float(kmin)
        self._kmax = float(kmax)

    def shade(self, stroke):
        origin = Vector((self._x, self._y, self._z))
        it = stroke.stroke_vertices_begin()
        while not it.is_end:
            v = it.object
            p = v.point_3d
            d = (p-origin).length
            if d > self._w:
                k = self._kmin
            else:
                k = (self._kmax*(self._w-d) + self._kmin*d)/self._w
            att = v.attribute
            (tr, tl) = att.thickness
            att.thickness = (k*tr/2.0, k*tl/2.0)
            it.increment()


class pyZDependingThicknessShader(StrokeShader):
    """
    Assigns thickness based on an object's local Z depth (point
    closest to camera is 1, point furthest from camera is zero)
    """
    def __init__(self, min, max):
        StrokeShader.__init__(self)
        self.__min = min
        self.__max = max
        self.__func = GetProjectedZF0D()

    def shade(self, stroke):
        it = stroke.stroke_vertices_begin()
        z_min = 1
        z_max = 0
        while not it.is_end:
            z = self.__func(Interface0DIterator(it))
            if z < z_min:
                z_min = z
            if z > z_max:
                z_max = z
            it.increment()
        z_diff = 1 / (z_max - z_min)
        it = stroke.stroke_vertices_begin()
        while not it.is_end:
            z = (self.__func(Interface0DIterator(it)) - z_min) * z_diff
            thickness = (1 - z) * self.__max + z * self.__min
            it.object.attribute.thickness = (thickness, thickness)
            it.increment()


## color modifiers
##################

class pyConstantColorShader(StrokeShader):
    """
    Assigns a constant color to the stroke
    """
    def __init__(self, r, g, b, a=1):
        StrokeShader.__init__(self)
        self._r = r
        self._g = g
        self._b = b
        self._a = a

    def shade(self, stroke):
        it = stroke.stroke_vertices_begin()
        while not it.is_end:
            att = it.object.attribute
            att.color = (self._r, self._g, self._b)
            att.alpha = self._a
            it.increment()


class pyIncreasingColorShader(StrokeShader):
    """
    Fades from one color to another along the stroke
    """
    def __init__(self,r1,g1,b1,a1, r2,g2,b2,a2):
        StrokeShader.__init__(self)
        self._c1 = [r1,g1,b1,a1]
        self._c2 = [r2,g2,b2,a2]

    def shade(self, stroke):
        n = stroke.stroke_vertices_size() - 1
        inc = 0
        it = stroke.stroke_vertices_begin()
        while not it.is_end:
            att = it.object.attribute
            c = float(inc) / float(n)

            att.color = ((1.0 - c) * self._c1[0] + c * self._c2[0],
                         (1.0 - c) * self._c1[1] + c * self._c2[1],
                         (1.0 - c) * self._c1[2] + c * self._c2[2])
            att.alpha = (1.0 - c) * self._c1[3] + c * self._c2[3]
            inc = inc + 1
            it.increment()


class pyInterpolateColorShader(StrokeShader):
    """
    Fades from one color to another and back
    """
    def __init__(self,r1,g1,b1,a1, r2,g2,b2,a2):
        StrokeShader.__init__(self)
        self._c1 = [r1,g1,b1,a1]
        self._c2 = [r2,g2,b2,a2]

    def shade(self, stroke):
        n = stroke.stroke_vertices_size() - 1
        inc = 0
        it = stroke.stroke_vertices_begin()
        while not it.is_end:
            att = it.object.attribute
            u = float(inc) / float(n)
            c = 1.0 - 2.0 * abs(u - 0.5)
            att.color = ((1.0 - c) * self._c1[0] + c * self._c2[0],
                         (1.0 - c) * self._c1[1] + c * self._c2[1],
                         (1.0 - c) * self._c1[2] + c * self._c2[2])
            att.alpha = (1.0-c) * self._c1[3] + c * self._c2[3]
            inc = inc+1
            it.increment()


class pyMaterialColorShader(StrokeShader):
    """
    Assigns the color of the underlying material to the stroke
    """
    def __init__(self, threshold=50):
        StrokeShader.__init__(self)
        self._threshold = threshold

    def shade(self, stroke):
        it = stroke.stroke_vertices_begin()
        func = MaterialF0D()
        xn = 0.312713
        yn = 0.329016
        Yn = 1.0
        un = 4.* xn / (-2.*xn + 12.*yn + 3.)
        vn= 9.* yn / (-2.*xn + 12.*yn +3.)
        while not it.is_end:
            mat = func(Interface0DIterator(it))

            r = mat.diffuse[0]
            g = mat.diffuse[1]
            b = mat.diffuse[2]

            X = 0.412453*r + 0.35758 *g + 0.180423*b
            Y = 0.212671*r + 0.71516 *g + 0.072169*b
            Z = 0.019334*r + 0.119193*g + 0.950227*b

            if (X, Y, Z) == (0, 0, 0):
                X = 0.01
                Y = 0.01
                Z = 0.01
            u = 4.*X / (X + 15.*Y + 3.*Z)
            v = 9.*Y / (X + 15.*Y + 3.*Z)

            L= 116. * pow((Y/Yn),(1./3.)) -16
            U = 13. * L * (u - un)
            V = 13. * L * (v - vn)

            if L > self._threshold:
                L = L/1.3
                U = U+10
            else:
                L = L +2.5*(100-L)/5.
                U = U/3.0
                V = V/3.0
            u = U / (13. * L) + un
            v = V / (13. * L) + vn

            Y = Yn * pow(((L+16.)/116.), 3.)
            X = -9.0 * Y * u / ((u - 4.0) * v - u * v)
            Z = (9.0 * Y - 15.0 * v * Y - v * X) / (3.0 * v)

            r = 3.240479 * X - 1.53715 * Y - 0.498535 * Z
            g = -0.969256 * X + 1.875991 * Y + 0.041556 * Z
            b = 0.055648 * X - 0.204043 * Y + 1.057311 * Z

            r = max(0,r)
            g = max(0,g)
            b = max(0,b)

            it.object.attribute.color = (r, g, b)
            it.increment()


class pyRandomColorShader(StrokeShader):
    """
    Assigns a color to the stroke based on given seed
    """
    def __init__(self, s=1):
        StrokeShader.__init__(self)
        random.seed(s)

    def shade(self, stroke):
        ## pick a random color
        c0 = float(random.uniform(15,75))/100.0
        c1 = float(random.uniform(15,75))/100.0
        c2 = float(random.uniform(15,75))/100.0
        #print(c0, c1, c2)
        it = stroke.stroke_vertices_begin()
        while not it.is_end:
            it.object.attribute.color = (c0,c1,c2)
            it.increment()


class py2DCurvatureColorShader(StrokeShader):
    """
    Assigns a color (greyscale) to the stroke based on the curvature.
    A higher curvature will yield a brighter color
    """
    def shade(self, stroke):
        it = stroke.stroke_vertices_begin()
        func = Curvature2DAngleF0D()
        while not it.is_end:
            c = func(Interface0DIterator(it))
            if c < 0:
                print("negative 2D curvature")
            color = 10.0 * c/3.1415
            it.object.attribute.color = (color, color, color)
            it.increment()


class pyTimeColorShader(StrokeShader):
    """
    Assigns a greyscale value that increases for every vertex.
    The brightness will increase along the stroke
    """
    def __init__(self, step=0.01):
        StrokeShader.__init__(self)
        self._step = step

    def shade(self, stroke):
        for i, svert in enumerate(iter(stroke)):
            c = i * self._step
            svert.attribute.color = (c,c,c)

## geometry modifiers


class pySamplingShader(StrokeShader):
    """
    Resamples the stroke, which gives the stroke the ammount of
    vertices specified
    """
    def __init__(self, sampling):
        StrokeShader.__init__(self)
        self._sampling = sampling

    def shade(self, stroke):
        stroke.resample(float(self._sampling))
        stroke.update_length()


class pyBackboneStretcherShader(StrokeShader):
    """
    Stretches the stroke's backbone by a given length (in pixels)
    """
    def __init__(self, l):
        StrokeShader.__init__(self)
        self._l = l

    def shade(self, stroke):
        it0 = stroke.stroke_vertices_begin()
        it1 = StrokeVertexIterator(it0)
        it1.increment()
        itn = stroke.stroke_vertices_end()
        itn.decrement()
        itn_1 = StrokeVertexIterator(itn)
        itn_1.decrement()
        v0 = it0.object
        v1 = it1.object
        vn_1 = itn_1.object
        vn = itn.object
        p0 = v0.point_2d
        pn = vn.point_2d
        p1 = v1.point_2d
        pn_1 = vn_1.point_2d
        d1 = (p0 - p1).normalized()
        dn = (pn - pn_1).normalized()
        newFirst = p0+d1*float(self._l)
        newLast = pn+dn*float(self._l)
        v0.point = newFirst
        vn.point = newLast
        stroke.update_length()


class pyLengthDependingBackboneStretcherShader(StrokeShader):
    """
    Stretches the stroke's backbone proportional to the stroke's length
    """
    def __init__(self, l):
        StrokeShader.__init__(self)
        self._l = l

    def shade(self, stroke):
        l = stroke.length_2d
        stretch = self._l*l
        it0 = stroke.stroke_vertices_begin()
        it1 = StrokeVertexIterator(it0)
        it1.increment()
        itn = stroke.stroke_vertices_end()
        itn.decrement()
        itn_1 = StrokeVertexIterator(itn)
        itn_1.decrement()
        v0 = it0.object
        v1 = it1.object
        vn_1 = itn_1.object
        vn = itn.object
        p0 = v0.point_2d
        pn = vn.point_2d
        p1 = v1.point_2d
        pn_1 = vn_1.point_2d
        d1 = (p0 - p1).normalized()
        dn = (pn - pn_1).normalized()
        newFirst = p0+d1*float(stretch)
        newLast = pn+dn*float(stretch)
        v0.point = newFirst
        vn.point = newLast
        stroke.update_length()



class pyGuidingLineShader(StrokeShader):
    """
    Replaces the stroke by its corresponding tangent
    """
    def shade(self, stroke):
        it = stroke.stroke_vertices_begin()          ## get the first vertex
        itlast = stroke.stroke_vertices_end()        ##
        itlast.decrement()                           ## get the last one
        t = itlast.object.point - it.object.point    ## tangent direction
        itmiddle = StrokeVertexIterator(it)          ##
        while itmiddle.object.u < 0.5:               ## look for the stroke middle vertex
            itmiddle.increment()                     ##
        it = StrokeVertexIterator(itmiddle)
        it.increment()
        while not it.is_end: ## position all the vertices along the tangent for the right part
            it.object.point = itmiddle.object.point+t*(it.object.u-itmiddle.object.u)
            it.increment()
        it = StrokeVertexIterator(itmiddle)
        it.decrement()
        while not it.is_begin: ## position all the vertices along the tangent for the left part
            it.object.point = itmiddle.object.point-t*(itmiddle.object.u-it.object.u)
            it.decrement()
        it.object.point = itmiddle.object.point-t*itmiddle.object.u ## first vertex
        stroke.update_length()


class pyBackboneStretcherNoCuspShader(StrokeShader):
    """
    Stretches the stroke's backbone, excluding cusp vertices (end junctions)
    """
    def __init__(self, l):
        StrokeShader.__init__(self)
        self._l = l

    def shade(self, stroke):
        it0 = stroke.stroke_vertices_begin()
        it1 = StrokeVertexIterator(it0)
        it1.increment()
        itn = stroke.stroke_vertices_end()
        itn.decrement()
        itn_1 = StrokeVertexIterator(itn)
        itn_1.decrement()
        v0 = it0.object
        v1 = it1.object
        if (v0.nature & Nature.CUSP) == 0 and (v1.nature & Nature.CUSP) == 0:
            d1 = (v0.point - v1.point).normalized()
            newFirst = p0+d1*float(self._l)
            v0.point = newFirst
        vn_1 = itn_1.object
        vn = itn.object
        if (vn.nature & Nature.CUSP) == 0 and (vn_1.nature & Nature.CUSP) == 0:
            dn = (vn.point - vn_1.point).normalized()
            newLast = vn.point + dn * float(self._l)
            vn.point = newLast
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
            it = stroke.stroke_vertices_begin()
            while not it.is_end:
                v = it.object
                p1 = v.point
                p2 = self._normalInfo(Interface0DIterator(it))*self._lambda*self._curvatureInfo(Interface0DIterator(it))
                v.point = p1+p2
                it.increment()
        stroke.update_length()


class pyTipRemoverShader(StrokeShader):
    """
    Removes the tips of the stroke
    """
    def __init__(self, l):
        StrokeShader.__init__(self)
        self._l = l

    def shade(self, stroke):
        originalSize = stroke.stroke_vertices_size()
        if originalSize < 4:
            return
        verticesToRemove = []
        oldAttributes = []
        it = stroke.stroke_vertices_begin()
        while not it.is_end:
            v = it.object
            if v.curvilinear_abscissa < self._l or v.stroke_length-v.curvilinear_abscissa < self._l:
                verticesToRemove.append(v)
            oldAttributes.append(StrokeAttribute(v.attribute))
            it.increment()
        if originalSize-len(verticesToRemove) < 2:
            return
        for sv in verticesToRemove:
            stroke.remove_vertex(sv)
        stroke.update_length()
        stroke.resample(originalSize)
        if stroke.stroke_vertices_size() != originalSize:
            print("pyTipRemover: Warning: resampling problem")
        it = stroke.stroke_vertices_begin()
        for a in oldAttributes:
            if it.is_end:
                break
            it.object.attribute = a
            it.increment()
        stroke.update_length()


class pyTVertexRemoverShader(StrokeShader):
    """
    Removes t-vertices from the stroke
    """
    def shade(self, stroke):
        if stroke.stroke_vertices_size() <= 3:
            return
        predTVertex = pyVertexNatureUP0D(Nature.T_VERTEX)
        it = stroke.stroke_vertices_begin()
        itlast = stroke.stroke_vertices_end()
        itlast.decrement()
        if predTVertex(it):
            stroke.remove_vertex(it.object)
        if predTVertex(itlast):
            stroke.remove_vertex(itlast.object)
        stroke.update_length()


#class pyExtremitiesOrientationShader(StrokeShader):
#    def __init__(self, x1,y1,x2=0,y2=0):
#        StrokeShader.__init__(self)
#        self._v1 = Vector((x1,y1))
#        self._v2 = Vector((x2,y2))
#    def shade(self, stroke):
#        #print(self._v1.x,self._v1.y)
#        stroke.setBeginningOrientation(self._v1.x,self._v1.y)
#        stroke.setEndingOrientation(self._v2.x,self._v2.y)


class pyHLRShader(StrokeShader):
    """
    Controlls visibility based upon the quantative invisibility (QI)
    based on hidden line removal (HLR)
    """
    def shade(self, stroke):
        originalSize = stroke.stroke_vertices_size()
        if originalSize < 4:
            return
        it = stroke.stroke_vertices_begin()
        invisible = 0
        it2 = StrokeVertexIterator(it)
        it2.increment()
        fe = self.get_fedge(it.object, it2.object)
        if fe.viewedge.qi != 0:
            invisible = 1
        while not it2.is_end:
            v = it.object
            vnext = it2.object
            if (v.nature & Nature.VIEW_VERTEX) != 0:
                #if (v.nature & Nature.T_VERTEX) != 0:
                fe = self.get_fedge(v, vnext)
                qi = fe.viewedge.qi
                if qi != 0:
                    invisible = 1
                else:
                    invisible = 0
            if invisible:
                v.attribute.visible = False
            it.increment()
            it2.increment()

    def get_fedge(self, it1, it2):
        return it1.get_fedge(it2)


# broken and a mess
class pyTVertexOrientationShader(StrokeShader):
    def __init__(self):
        StrokeShader.__init__(self)
        self._Get2dDirection = Orientation2DF1D()
    ## finds the TVertex orientation from the TVertex and
    ## the previous or next edge

    def findOrientation(self, tv, ve):
        mateVE = tv.get_mate(ve)
        if ve.qi != 0 or mateVE.qi != 0:
            ait = AdjacencyIterator(tv,1,0)
            winner = None
            incoming = True
            while not ait.is_end:
                ave = ait.object
                if  ave.id != ve.id and ave.id != mateVE.id:
                    winner = ait.object
                    if not ait.isIncoming(): # FIXME
                        incoming = False
                        break
                ait.increment()
            if winner is not None:
                if not incoming:
                    direction = self._Get2dDirection(winner.last_fedge)
                else:
                    direction = self._Get2dDirection(winner.first_fedge)
                return direction
        return None

    def castToTVertex(self, cp):
        if cp.t2d() == 0.0:
            return cp.first_svertex.viewvertex
        elif cp.t2d() == 1.0:
            return cp.second_svertex.viewvertex
        return None

    def shade(self, stroke):
        it = stroke.stroke_vertices_begin()
        it2 = StrokeVertexIterator(it)
        it2.increment()
        ## case where the first vertex is a TVertex
        v = it.object
        if (v.nature & Nature.T_VERTEX) != 0:
            tv = self.castToTVertex(v)
            if tv is not None:
                ve = self.get_fedge(v, it2.object).viewedge
                dir = self.findOrientation(tv, ve)
                if dir is not None:
                    #print(dir.x, dir.y)
                    v.attribute.set_attribute_vec2("orientation", dir)
        while not it2.is_end:
            vprevious = it.object
            v = it2.object
            if (v.nature & Nature.T_VERTEX) != 0:
                tv = self.castToTVertex(v)
                if tv is not None:
                    ve = self.get_fedge(vprevious, v).viewedge
                    dir = self.findOrientation(tv, ve)
                    if dir is not None:
                        #print(dir.x, dir.y)
                        v.attribute.set_attribute_vec2("orientation", dir)
            it.increment()
            it2.increment()
        ## case where the last vertex is a TVertex
        v = it.object
        if (v.nature & Nature.T_VERTEX) != 0:
            itPrevious = StrokeVertexIterator(it)
            itPrevious.decrement()
            tv = self.castToTVertex(v)
            if tv is not None:
                ve = self.get_fedge(itPrevious.object, v).viewedge
                dir = self.findOrientation(tv, ve)
                if dir is not None:
                    #print(dir.x, dir.y)
                    v.attribute.set_attribute_vec2("orientation", dir)

    def get_fedge(self, it1, it2):
        return it1.get_fedge(it2)


class pySinusDisplacementShader(StrokeShader):
    """
    Displaces the stroke in the shape of a sine wave
    """
    def __init__(self, f, a):
        StrokeShader.__init__(self)
        self._f = f
        self._a = a
        self._getNormal = Normal2DF0D()

    def shade(self, stroke):
        it = stroke.stroke_vertices_begin()
        while not it.is_end:
            v = it.object
            #print(self._getNormal.name)
            n = self._getNormal(Interface0DIterator(it))
            p = v.point
            u = v.u
            a = self._a*(1-2*(abs(u-0.5)))
            n = n*a*cos(self._f*u*6.28)
            #print(n.x, n.y)
            v.point = p+n
            #v.point = v.point+n*a*cos(f*v.u)
            it.increment()
        stroke.update_length()


class pyPerlinNoise1DShader(StrokeShader):
    """
    Displaces the stroke using the curvilinear abscissa.  This means
    that lines with the same length and sampling interval will be
    identically distorded
    """
    def __init__(self, freq=10, amp=10, oct=4, seed=-1):
        StrokeShader.__init__(self)
        self.__noise = Noise(seed)
        self.__freq = freq
        self.__amp = amp
        self.__oct = oct

    def shade(self, stroke):
        it = stroke.stroke_vertices_begin()
        while not it.is_end:
            v = it.object
            i = v.projected_x + v.projected_y
            nres = self.__noise.turbulence1(i, self.__freq, self.__amp, self.__oct)
            v.point = (v.projected_x + nres, v.projected_y + nres)
            it.increment()
        stroke.update_length()


class pyPerlinNoise2DShader(StrokeShader):
    """
    Displaces the stroke using the strokes coordinates.  This means
    that in a scene no strokes will be distorded identically

    More information on the noise shaders can be found at
    freestyleintegration.wordpress.com/2011/09/25/development-updates-on-september-25/
    """
    def __init__(self, freq=10, amp=10, oct=4, seed=-1):
        StrokeShader.__init__(self)
        self.__noise = Noise(seed)
        self.__freq = freq
        self.__amp = amp
        self.__oct = oct

    def shade(self, stroke):
        it = stroke.stroke_vertices_begin()
        while not it.is_end:
            v = it.object
            nres = self.__noise.turbulence2(v.point_2d, self.__freq, self.__amp, self.__oct)
            v.point = (v.projected_x + nres, v.projected_y + nres)
            it.increment()
        stroke.update_length()


class pyBluePrintCirclesShader(StrokeShader):
    """
    Draws the silhouette of the object as a circle
    """
    def __init__(self, turns=1, random_radius=3, random_center=5):
        StrokeShader.__init__(self)
        self.__turns = turns
        self.__random_center = random_center
        self.__random_radius = random_radius

    def shade(self, stroke):
        it = stroke.stroke_vertices_begin()
        if it.is_end:
            return
        p_min = it.object.point.copy()
        p_max = it.object.point.copy()
        while not it.is_end:
            p = it.object.point
            if p.x < p_min.x:
                p_min.x = p.x
            if p.x > p_max.x:
                p_max.x = p.x
            if p.y < p_min.y:
                p_min.y = p.y
            if p.y > p_max.y:
                p_max.y = p.y
            it.increment()
        stroke.resample(32 * self.__turns)
        sv_nb = stroke.stroke_vertices_size()
#       print("min  :", p_min.x, p_min.y) # DEBUG
#       print("mean :", p_sum.x, p_sum.y) # DEBUG
#       print("max  :", p_max.x, p_max.y) # DEBUG
#       print("----------------------") # DEBUG
#######################################################
        sv_nb = sv_nb // self.__turns
        center = (p_min + p_max) / 2
        radius = (center.x - p_min.x + center.y - p_min.y) / 2
        p_new = Vector((0.0, 0.0))
#######################################################
        R = self.__random_radius
        C = self.__random_center
        i = 0
        it = stroke.stroke_vertices_begin()
        for j in range(self.__turns):
            prev_radius = radius
            prev_center = center
            radius = radius + randint(-R, R)
            center = center + Vector((randint(-C, C), randint(-C, C)))
            while i < sv_nb and not it.is_end:
                t = float(i) / float(sv_nb - 1)
                r = prev_radius + (radius - prev_radius) * t
                c = prev_center + (center - prev_center) * t
                p_new.x = c.x + r * cos(2 * pi * t)
                p_new.y = c.y + r * sin(2 * pi * t)
                it.object.point = p_new
                i = i + 1
                it.increment()
            i = 1
        verticesToRemove = []
        while not it.is_end:
            verticesToRemove.append(it.object)
            it.increment()
        for sv in verticesToRemove:
            stroke.remove_vertex(sv)
        stroke.update_length()


class pyBluePrintEllipsesShader(StrokeShader):
    """
    Draws the silhouette of the object as an ellips
    """
    def __init__(self, turns=1, random_radius=3, random_center=5):
        StrokeShader.__init__(self)
        self.__turns = turns
        self.__random_center = random_center
        self.__random_radius = random_radius

    def shade(self, stroke):
        it = stroke.stroke_vertices_begin()
        if it.is_end:
            return
        p_min = it.object.point.copy()
        p_max = it.object.point.copy()
        while not it.is_end:
            p = it.object.point
            if p.x < p_min.x:
                p_min.x = p.x
            if p.x > p_max.x:
                p_max.x = p.x
            if p.y < p_min.y:
                p_min.y = p.y
            if p.y > p_max.y:
                p_max.y = p.y
            it.increment()
        stroke.resample(32 * self.__turns)
        sv_nb = stroke.stroke_vertices_size()
        sv_nb = sv_nb // self.__turns
        center = (p_min + p_max) / 2
        radius = center - p_min
        p_new = Vector((0.0, 0.0))
#######################################################
        R = self.__random_radius
        C = self.__random_center
        i = 0
        it = stroke.stroke_vertices_begin()
        for j in range(self.__turns):
            prev_radius = radius
            prev_center = center
            radius = radius + Vector((randint(-R, R), randint(-R, R)))
            center = center + Vector((randint(-C, C), randint(-C, C)))
            while i < sv_nb and not it.is_end:
                t = float(i) / float(sv_nb - 1)
                r = prev_radius + (radius - prev_radius) * t
                c = prev_center + (center - prev_center) * t
                p_new.x = c.x + r.x * cos(2 * pi * t)
                p_new.y = c.y + r.y * sin(2 * pi * t)
                it.object.point = p_new
                i = i + 1
                it.increment()
            i = 1
        verticesToRemove = []
        while not it.is_end:
            verticesToRemove.append(it.object)
            it.increment()
        for sv in verticesToRemove:
            stroke.remove_vertex(sv)
        stroke.update_length()


class pyBluePrintSquaresShader(StrokeShader):
    """
    Draws the silhouette of the object as a square
    """
    def __init__(self, turns=1, bb_len=10, bb_rand=0):
        StrokeShader.__init__(self)
        self.__turns = turns
        self.__bb_len = bb_len
        self.__bb_rand = bb_rand

    def shade(self, stroke):
        it = stroke.stroke_vertices_begin()
        if it.is_end:
            return
        p_min = it.object.point.copy()
        p_max = it.object.point.copy()
        while not it.is_end:
            p = it.object.point
            if p.x < p_min.x:
                p_min.x = p.x
            if p.x > p_max.x:
                p_max.x = p.x
            if p.y < p_min.y:
                p_min.y = p.y
            if p.y > p_max.y:
                p_max.y = p.y
            it.increment()
        stroke.resample(32 * self.__turns)
        sv_nb = stroke.stroke_vertices_size()
#######################################################
        sv_nb = sv_nb // self.__turns
        first = sv_nb // 4
        second = 2 * first
        third = 3 * first
        fourth = sv_nb
        p_first = Vector((p_min.x - self.__bb_len, p_min.y))
        p_first_end = Vector((p_max.x + self.__bb_len, p_min.y))
        p_second = Vector((p_max.x, p_min.y - self.__bb_len))
        p_second_end = Vector((p_max.x, p_max.y + self.__bb_len))
        p_third = Vector((p_max.x + self.__bb_len, p_max.y))
        p_third_end = Vector((p_min.x - self.__bb_len, p_max.y))
        p_fourth = Vector((p_min.x, p_max.y + self.__bb_len))
        p_fourth_end = Vector((p_min.x, p_min.y - self.__bb_len))
#######################################################
        R = self.__bb_rand
        r = self.__bb_rand // 2
        it = stroke.stroke_vertices_begin()
        visible = True
        for j in range(self.__turns):
            p_first = p_first + Vector((randint(-R, R), randint(-r, r)))
            p_first_end = p_first_end + Vector((randint(-R, R), randint(-r, r)))
            p_second = p_second + Vector((randint(-r, r), randint(-R, R)))
            p_second_end = p_second_end + Vector((randint(-r, r), randint(-R, R)))
            p_third = p_third + Vector((randint(-R, R), randint(-r, r)))
            p_third_end = p_third_end + Vector((randint(-R, R), randint(-r, r)))
            p_fourth = p_fourth + Vector((randint(-r, r), randint(-R, R)))
            p_fourth_end = p_fourth_end + Vector((randint(-r, r), randint(-R, R)))
            vec_first = p_first_end - p_first
            vec_second = p_second_end - p_second
            vec_third = p_third_end - p_third
            vec_fourth = p_fourth_end - p_fourth
            i = 0
            while i < sv_nb and not it.is_end:
                if i < first:
                    p_new = p_first + vec_first * float(i)/float(first - 1)
                    if i == first - 1:
                        visible = False
                elif i < second:
                    p_new = p_second + vec_second * float(i - first)/float(second - first - 1)
                    if i == second - 1:
                        visible = False
                elif i < third:
                    p_new = p_third + vec_third * float(i - second)/float(third - second - 1)
                    if i == third - 1:
                        visible = False
                else:
                    p_new = p_fourth + vec_fourth * float(i - third)/float(fourth - third - 1)
                    if i == fourth - 1:
                        visible = False
                if it.object is None:
                    i = i + 1
                    it.increment()
                    if not visible:
                        visible = True
                    continue
                it.object.point = p_new
                it.object.attribute.visible = visible
                if not visible:
                    visible = True
                i = i + 1
                it.increment()
        verticesToRemove = []
        while not it.is_end:
            verticesToRemove.append(it.object)
            it.increment()
        for sv in verticesToRemove:
            stroke.remove_vertex(sv)
        stroke.update_length()


# needs a docstring
class pyBluePrintDirectedSquaresShader(StrokeShader):
    def __init__(self, turns=1, bb_len=10, mult=1):
        StrokeShader.__init__(self)
        self.__mult = mult
        self.__turns = turns
        self.__bb_len = 1 + float(bb_len) / 100

    def shade(self, stroke):
        stroke.resample(32 * self.__turns)
        p_mean = Vector((0.0, 0.0))
        it = stroke.stroke_vertices_begin()
        while not it.is_end:
            p = it.object.point
            p_mean = p_mean + p
            it.increment()
        sv_nb = stroke.stroke_vertices_size()
        p_mean = p_mean / sv_nb
        p_var_xx = 0
        p_var_yy = 0
        p_var_xy = 0
        it = stroke.stroke_vertices_begin()
        while not it.is_end:
            p = it.object.point
            p_var_xx = p_var_xx + pow(p.x - p_mean.x, 2)
            p_var_yy = p_var_yy + pow(p.y - p_mean.y, 2)
            p_var_xy = p_var_xy + (p.x - p_mean.x) * (p.y - p_mean.y)
            it.increment()
        p_var_xx = p_var_xx / sv_nb
        p_var_yy = p_var_yy / sv_nb
        p_var_xy = p_var_xy / sv_nb
##      print(p_var_xx, p_var_yy, p_var_xy)
        trace = p_var_xx + p_var_yy
        det = p_var_xx * p_var_yy - p_var_xy * p_var_xy
        sqrt_coeff = sqrt(trace * trace - 4 * det)
        lambda1 = (trace + sqrt_coeff) / 2
        lambda2 = (trace - sqrt_coeff) / 2
##      print(lambda1, lambda2)
        theta = atan(2 * p_var_xy / (p_var_xx - p_var_yy)) / 2
##      print(theta)
        if p_var_yy > p_var_xx:
            e1 = Vector((cos(theta + pi / 2), sin(theta + pi / 2))) * sqrt(lambda1) * self.__mult
            e2 = Vector((cos(theta + pi), sin(theta + pi))) * sqrt(lambda2) * self.__mult
        else:
            e1 = Vector((cos(theta), sin(theta))) * sqrt(lambda1) * self.__mult
            e2 = Vector((cos(theta + pi / 2), sin(theta + pi / 2))) * sqrt(lambda2) * self.__mult
#######################################################
        sv_nb = sv_nb // self.__turns
        first = sv_nb // 4
        second = 2 * first
        third = 3 * first
        fourth = sv_nb
        bb_len1 = self.__bb_len
        bb_len2 = 1 + (bb_len1 - 1) * sqrt(lambda1 / lambda2)
        p_first = p_mean - e1 - e2 * bb_len2
        p_second = p_mean - e1 * bb_len1 + e2
        p_third = p_mean + e1 + e2 * bb_len2
        p_fourth = p_mean + e1 * bb_len1 - e2
        vec_first = e2 * bb_len2 * 2
        vec_second = e1 * bb_len1 * 2
        vec_third = vec_first * -1
        vec_fourth = vec_second * -1
#######################################################
        it = stroke.stroke_vertices_begin()
        visible = True
        for j in range(self.__turns):
            i = 0
            while i < sv_nb:
                if i < first:
                    p_new = p_first + vec_first * float(i)/float(first - 1)
                    if i == first - 1:
                        visible = False
                elif i < second:
                    p_new = p_second + vec_second * float(i - first)/float(second - first - 1)
                    if i == second - 1:
                        visible = False
                elif i < third:
                    p_new = p_third + vec_third * float(i - second)/float(third - second - 1)
                    if i == third - 1:
                        visible = False
                else:
                    p_new = p_fourth + vec_fourth * float(i - third)/float(fourth - third - 1)
                    if i == fourth - 1:
                        visible = False
                it.object.point = p_new
                it.object.attribute.visible = visible
                if not visible:
                    visible = True
                i = i + 1
                it.increment()
        verticesToRemove = []
        while not it.is_end:
            verticesToRemove.append(it.object)
            it.increment()
        for sv in verticesToRemove:
            stroke.remove_vertex(sv)
        stroke.update_length()


class pyModulateAlphaShader(StrokeShader):
    """
    Limits the stroke's alpha between a min and max value
    """
    def __init__(self, min=0, max=1):
        StrokeShader.__init__(self)
        self.__min = min
        self.__max = max

    def shade(self, stroke):
        it = stroke.stroke_vertices_begin()
        while not it.is_end:
            alpha = it.object.attribute.alpha
            p = it.object.point
            alpha = alpha * p.y / 400
            if alpha < self.__min:
                alpha = self.__min
            elif alpha > self.__max:
                alpha = self.__max
            it.object.attribute.alpha = alpha
            it.increment()


## various
class pyDummyShader(StrokeShader):
    def shade(self, stroke):
        it = stroke.stroke_vertices_begin()
        while not it.is_end:
            toto = Interface0DIterator(it)
            att = it.object.attribute
            att.color = (0.3, 0.4, 0.4)
            att.thickness = (0, 5)
            it.increment()


class pyDebugShader(StrokeShader):
    def shade(self, stroke):
        fe = CF.get_selected_fedge()
        id1 = fe.first_svertex.id
        id2 = fe.second_svertex.id
        #print(id1.first, id1.second)
        #print(id2.first, id2.second)
        it = stroke.stroke_vertices_begin()
        found = True
        foundfirst = True
        foundsecond = False
        while not it.is_end:
            cp = it.object
            if cp.first_svertex.id == id1 or cp.second_svertex.id == id1:
                foundfirst = True
            if cp.first_svertex.id == id2 or cp.second_svertex.id == id2:
                foundsecond = True
            if foundfirst and foundsecond:
                found = True
                break
            it.increment()
        if found:
            print("The selected Stroke id is: ", stroke.id.first, stroke.id.second)
