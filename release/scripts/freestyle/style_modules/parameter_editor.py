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

import Freestyle
import math
import mathutils
import time

from freestyle_init import *
from logical_operators import *
from ChainingIterators import *
from shaders import *

class ColorRampModifier(StrokeShader):
    def __init__(self, blend, influence, ramp):
        StrokeShader.__init__(self)
        self.__blend = blend
        self.__influence = influence
        self.__ramp = ramp
    def evaluate(self, t):
        col = Freestyle.evaluateColorRamp(self.__ramp, t)
        col = col.xyz # omit alpha
        return col
    def blend_ramp(self, a, b):
        return Freestyle.blendRamp(self.__blend, a, self.__influence, b)

class ScalarBlendModifier(StrokeShader):
    def __init__(self, blend, influence):
        StrokeShader.__init__(self)
        self.__blend = blend
        self.__influence = influence
    def blend(self, v1, v2):
        fac = self.__influence
        facm = 1.0 - fac
        if self.__blend == "MIX":
            v1 = facm * v1 + fac * v2
        elif self.__blend == "ADD":
            v1 += fac * v2
        elif self.__blend == "MULTIPLY":
            v1 *= facm + fac * v2;
        elif self.__blend == "SUBTRACT":
            v1 -= fac * v2
        elif self.__blend == "DIVIDE":
            if v2 != 0.0:
                v1 = facm * v1 + fac * v1 / v2
        elif self.__blend == "DIFFERENCE":
            v1 = facm * v1 + fac * abs(v1 - v2)
        elif self.__blend == "MININUM":
            tmp = fac * v1
            if v1 > tmp:
                v1 = tmp
        elif self.__blend == "MAXIMUM":
            tmp = fac * v1
            if v1 < tmp:
                v1 = tmp
        else:
            raise ValueError("unknown curve blend type: " + self.__blend)
        return v1

class CurveMappingModifier(ScalarBlendModifier):
    def __init__(self, blend, influence, mapping, invert, curve):
        ScalarBlendModifier.__init__(self, blend, influence)
        assert mapping in ("LINEAR", "CURVE")
        self.__mapping = getattr(self, mapping)
        self.__invert = invert
        self.__curve = curve
    def LINEAR(self, t):
        if self.__invert:
            return 1.0 - t
        return t
    def CURVE(self, t):
        return Freestyle.evaluateCurveMappingF(self.__curve, 0, t)
    def evaluate(self, t):
        return self.__mapping(t)

class ThicknessModifierMixIn:
    def __init__(self):
        scene = Freestyle.getCurrentScene()
        self.__persp_camera = (scene.camera.data.type == "PERSP")
    def set_thickness(self, sv, outer, inner):
        fe = sv.A().getFEdge(sv.B())
        nature = fe.getNature()
        if (nature & Nature.BORDER):
            if self.__persp_camera:
                point = -sv.getPoint3D()
                point.normalize()
                dir = point.dot(fe.normalB())
            else:
                dir = fe.normalB().z
            if dir < 0.0: # the back side is visible
                outer, inner = inner, outer
        elif (nature & Nature.SILHOUETTE):
            if fe.isSmooth(): # TODO more tests needed
                outer, inner = inner, outer
        else:
            outer = inner = (outer + inner) / 2
        sv.attribute().setThickness(outer, inner)

class ThicknessBlenderMixIn(ThicknessModifierMixIn):
    def __init__(self, position, ratio):
        ThicknessModifierMixIn.__init__(self)
        self.__position = position
        self.__ratio = ratio
    def blend_thickness(self, outer, inner, v):
        if self.__position == "CENTER":
            outer = self.blend(outer, v / 2)
            inner = self.blend(inner, v / 2)
        elif self.__position == "INSIDE":
            outer = self.blend(outer, 0)
            inner = self.blend(inner, v)
        elif self.__position == "OUTSIDE":
            outer = self.blend(outer, v)
            inner = self.blend(inner, 0)
        elif self.__position == "RELATIVE":
            outer = self.blend(outer, v * self.__ratio)
            inner = self.blend(inner, v * (1 - self.__ratio))
        else:
            raise ValueError("unknown thickness position: " + self.__position)
        return outer, inner

class BaseColorShader(ConstantColorShader):
    def getName(self):
        return "BaseColorShader"

class BaseThicknessShader(StrokeShader, ThicknessModifierMixIn):
    def __init__(self, thickness, position, ratio):
        StrokeShader.__init__(self)
        ThicknessModifierMixIn.__init__(self)
        if position == "CENTER":
            self.__outer = thickness / 2
            self.__inner = thickness / 2
        elif position == "INSIDE":
            self.__outer = 0
            self.__inner = thickness
        elif position == "OUTSIDE":
            self.__outer = thickness
            self.__inner = 0
        elif position == "RELATIVE":
            self.__outer = thickness * ratio
            self.__inner = thickness * (1 - ratio)
        else:
            raise ValueError("unknown thickness position: " + self.position)
    def getName(self):
        return "BaseThicknessShader"
    def shade(self, stroke):
        it = stroke.strokeVerticesBegin()
        while it.isEnd() == 0:
            sv = it.getObject()
            self.set_thickness(sv, self.__outer, self.__inner)
            it.increment()

# Along Stroke modifiers

def iter_t2d_along_stroke(stroke):
    total = stroke.getLength2D()
    distance = 0.0
    it = stroke.strokeVerticesBegin()
    while not it.isEnd():
        p = it.getObject().getPoint()
        if not it.isBegin():
            distance += (prev - p).length
        prev = p
        t = min(distance / total, 1.0)
        yield it, t
        it.increment()

class ColorAlongStrokeShader(ColorRampModifier):
    def getName(self):
        return "ColorAlongStrokeShader"
    def shade(self, stroke):
        for it, t in iter_t2d_along_stroke(stroke):
            attr = it.getObject().attribute()
            a = attr.getColorRGB()
            b = self.evaluate(t)
            c = self.blend_ramp(a, b)
            attr.setColor(c)

class AlphaAlongStrokeShader(CurveMappingModifier):
    def getName(self):
        return "AlphaAlongStrokeShader"
    def shade(self, stroke):
        for it, t in iter_t2d_along_stroke(stroke):
            attr = it.getObject().attribute()
            a = attr.getAlpha()
            b = self.evaluate(t)
            c = self.blend(a, b)
            attr.setAlpha(c)

class ThicknessAlongStrokeShader(ThicknessBlenderMixIn, CurveMappingModifier):
    def __init__(self, thickness_position, thickness_ratio,
                 blend, influence, mapping, invert, curve, value_min, value_max):
        ThicknessBlenderMixIn.__init__(self, thickness_position, thickness_ratio)
        CurveMappingModifier.__init__(self, blend, influence, mapping, invert, curve)
        self.__value_min = value_min
        self.__value_max = value_max
    def getName(self):
        return "ThicknessAlongStrokeShader"
    def shade(self, stroke):
        for it, t in iter_t2d_along_stroke(stroke):
            sv = it.getObject()
            a = sv.attribute().getThicknessRL()
            b = self.__value_min + self.evaluate(t) * (self.__value_max - self.__value_min)
            c = self.blend_thickness(a[0], a[1], b)
            self.set_thickness(sv, c[0], c[1])

# Distance from Camera modifiers

def iter_distance_from_camera(stroke, range_min, range_max):
    normfac = range_max - range_min # normalization factor
    it = stroke.strokeVerticesBegin()
    while not it.isEnd():
        p = it.getObject().getPoint3D() # in the camera coordinate
        distance = p.length
        if distance < range_min:
            t = 0.0
        elif distance > range_max:
            t = 1.0
        else:
            t = (distance - range_min) / normfac
        yield it, t
        it.increment()

class ColorDistanceFromCameraShader(ColorRampModifier):
    def __init__(self, blend, influence, ramp, range_min, range_max):
        ColorRampModifier.__init__(self, blend, influence, ramp)
        self.__range_min = range_min
        self.__range_max = range_max
    def getName(self):
        return "ColorDistanceFromCameraShader"
    def shade(self, stroke):
        for it, t in iter_distance_from_camera(stroke, self.__range_min, self.__range_max):
            attr = it.getObject().attribute()
            a = attr.getColorRGB()
            b = self.evaluate(t)
            c = self.blend_ramp(a, b)
            attr.setColor(c)

class AlphaDistanceFromCameraShader(CurveMappingModifier):
    def __init__(self, blend, influence, mapping, invert, curve, range_min, range_max):
        CurveMappingModifier.__init__(self, blend, influence, mapping, invert, curve)
        self.__range_min = range_min
        self.__range_max = range_max
    def getName(self):
        return "AlphaDistanceFromCameraShader"
    def shade(self, stroke):
        for it, t in iter_distance_from_camera(stroke, self.__range_min, self.__range_max):
            attr = it.getObject().attribute()
            a = attr.getAlpha()
            b = self.evaluate(t)
            c = self.blend(a, b)
            attr.setAlpha(c)

class ThicknessDistanceFromCameraShader(ThicknessBlenderMixIn, CurveMappingModifier):
    def __init__(self, thickness_position, thickness_ratio,
                 blend, influence, mapping, invert, curve, range_min, range_max, value_min, value_max):
        ThicknessBlenderMixIn.__init__(self, thickness_position, thickness_ratio)
        CurveMappingModifier.__init__(self, blend, influence, mapping, invert, curve)
        self.__range_min = range_min
        self.__range_max = range_max
        self.__value_min = value_min
        self.__value_max = value_max
    def getName(self):
        return "ThicknessDistanceFromCameraShader"
    def shade(self, stroke):
        for it, t in iter_distance_from_camera(stroke, self.__range_min, self.__range_max):
            sv = it.getObject()
            a = sv.attribute().getThicknessRL()
            b = self.__value_min + self.evaluate(t) * (self.__value_max - self.__value_min)
            c = self.blend_thickness(a[0], a[1], b)
            self.set_thickness(sv, c[0], c[1])

# Distance from Object modifiers

def iter_distance_from_object(stroke, object, range_min, range_max):
    scene = Freestyle.getCurrentScene()
    mv = scene.camera.matrix_world.copy() # model-view matrix
    mv.invert()
    loc = mv * object.location # loc in the camera coordinate
    normfac = range_max - range_min # normalization factor
    it = stroke.strokeVerticesBegin()
    while not it.isEnd():
        p = it.getObject().getPoint3D() # in the camera coordinate
        distance = (p - loc).length
        if distance < range_min:
            t = 0.0
        elif distance > range_max:
            t = 1.0
        else:
            t = (distance - range_min) / normfac
        yield it, t
        it.increment()

class ColorDistanceFromObjectShader(ColorRampModifier):
    def __init__(self, blend, influence, ramp, target, range_min, range_max):
        ColorRampModifier.__init__(self, blend, influence, ramp)
        self.__target = target
        self.__range_min = range_min
        self.__range_max = range_max
    def getName(self):
        return "ColorDistanceFromObjectShader"
    def shade(self, stroke):
        if self.__target is None:
            return
        for it, t in iter_distance_from_object(stroke, self.__target, self.__range_min, self.__range_max):
            attr = it.getObject().attribute()
            a = attr.getColorRGB()
            b = self.evaluate(t)
            c = self.blend_ramp(a, b)
            attr.setColor(c)

class AlphaDistanceFromObjectShader(CurveMappingModifier):
    def __init__(self, blend, influence, mapping, invert, curve, target, range_min, range_max):
        CurveMappingModifier.__init__(self, blend, influence, mapping, invert, curve)
        self.__target = target
        self.__range_min = range_min
        self.__range_max = range_max
    def getName(self):
        return "AlphaDistanceFromObjectShader"
    def shade(self, stroke):
        if self.__target is None:
            return
        for it, t in iter_distance_from_object(stroke, self.__target, self.__range_min, self.__range_max):
            attr = it.getObject().attribute()
            a = attr.getAlpha()
            b = self.evaluate(t)
            c = self.blend(a, b)
            attr.setAlpha(c)

class ThicknessDistanceFromObjectShader(ThicknessBlenderMixIn, CurveMappingModifier):
    def __init__(self, thickness_position, thickness_ratio,
                 blend, influence, mapping, invert, curve, target, range_min, range_max, value_min, value_max):
        ThicknessBlenderMixIn.__init__(self, thickness_position, thickness_ratio)
        CurveMappingModifier.__init__(self, blend, influence, mapping, invert, curve)
        self.__target = target
        self.__range_min = range_min
        self.__range_max = range_max
        self.__value_min = value_min
        self.__value_max = value_max
    def getName(self):
        return "ThicknessDistanceFromObjectShader"
    def shade(self, stroke):
        if self.__target is None:
            return
        for it, t in iter_distance_from_object(stroke, self.__target, self.__range_min, self.__range_max):
            sv = it.getObject()
            a = sv.attribute().getThicknessRL()
            b = self.__value_min + self.evaluate(t) * (self.__value_max - self.__value_min)
            c = self.blend_thickness(a[0], a[1], b)
            self.set_thickness(sv, c[0], c[1])

# Material modifiers

def iter_material_color(stroke, material_attr):
    func = CurveMaterialF0D()
    it = stroke.strokeVerticesBegin()
    while not it.isEnd():
        material = func(it.castToInterface0DIterator())
        if material_attr == "DIFF":
            color = (material.diffuseR(),
                     material.diffuseG(),
                     material.diffuseB())
        elif material_attr == "SPEC":
            color = (material.specularR(),
                     material.specularG(),
                     material.specularB())
        else:
            raise ValueError("unexpected material attribute: " + material_attr)
        yield it, color
        it.increment()

def iter_material_value(stroke, material_attr):
    func = CurveMaterialF0D()
    it = stroke.strokeVerticesBegin()
    while not it.isEnd():
        material = func(it.castToInterface0DIterator())
        if material_attr == "DIFF":
            r = material.diffuseR()
            g = material.diffuseG()
            b = material.diffuseB()
            t = 0.35 * r + 0.45 * r + 0.2 * b
        elif material_attr == "DIFF_R":
            t = material.diffuseR()
        elif material_attr == "DIFF_G":
            t = material.diffuseG()
        elif material_attr == "DIFF_B":
            t = material.diffuseB()
        elif material_attr == "SPEC":
            r = material.specularR()
            g = material.specularG()
            b = material.specularB()
            t = 0.35 * r + 0.45 * r + 0.2 * b
        elif material_attr == "SPEC_R":
            t = material.specularR()
        elif material_attr == "SPEC_G":
            t = material.specularG()
        elif material_attr == "SPEC_B":
            t = material.specularB()
        elif material_attr == "SPEC_HARDNESS":
            t = material.shininess()
        elif material_attr == "ALPHA":
            t = material.diffuseA()
        else:
            raise ValueError("unexpected material attribute: " + material_attr)
        yield it, t
        it.increment()

class ColorMaterialShader(ColorRampModifier):
    def __init__(self, blend, influence, ramp, material_attr, use_ramp):
        ColorRampModifier.__init__(self, blend, influence, ramp)
        self.__material_attr = material_attr
        self.__use_ramp = use_ramp
    def getName(self):
        return "ColorMaterialShader"
    def shade(self, stroke):
        if self.__material_attr in ["DIFF", "SPEC"] and not self.__use_ramp:
            for it, b in iter_material_color(stroke, self.__material_attr):
                attr = it.getObject().attribute()
                a = attr.getColorRGB()
                c = self.blend_ramp(a, b)
                attr.setColor(c)
        else:
            for it, t in iter_material_value(stroke, self.__material_attr):
                attr = it.getObject().attribute()
                a = attr.getColorRGB()
                b = self.evaluate(t)
                c = self.blend_ramp(a, b)
                attr.setColor(c)

class AlphaMaterialShader(CurveMappingModifier):
    def __init__(self, blend, influence, mapping, invert, curve, material_attr):
        CurveMappingModifier.__init__(self, blend, influence, mapping, invert, curve)
        self.__material_attr = material_attr
    def getName(self):
        return "AlphaMaterialShader"
    def shade(self, stroke):
        for it, t in iter_material_value(stroke, self.__material_attr):
            attr = it.getObject().attribute()
            a = attr.getAlpha()
            b = self.evaluate(t)
            c = self.blend(a, b)
            attr.setAlpha(c)

class ThicknessMaterialShader(ThicknessBlenderMixIn, CurveMappingModifier):
    def __init__(self, thickness_position, thickness_ratio,
                 blend, influence, mapping, invert, curve, material_attr, value_min, value_max):
        ThicknessBlenderMixIn.__init__(self, thickness_position, thickness_ratio)
        CurveMappingModifier.__init__(self, blend, influence, mapping, invert, curve)
        self.__material_attr = material_attr
        self.__value_min = value_min
        self.__value_max = value_max
    def getName(self):
        return "ThicknessMaterialShader"
    def shade(self, stroke):
        for it, t in iter_material_value(stroke, self.__material_attr):
            sv = it.getObject()
            a = sv.attribute().getThicknessRL()
            b = self.__value_min + self.evaluate(t) * (self.__value_max - self.__value_min)
            c = self.blend_thickness(a[0], a[1], b)
            self.set_thickness(sv, c[0], c[1])

# Calligraphic thickness modifier

class CalligraphicThicknessShader(ThicknessBlenderMixIn, ScalarBlendModifier):
    def __init__(self, thickness_position, thickness_ratio,
                 blend, influence, orientation, min_thickness, max_thickness):
        ThicknessBlenderMixIn.__init__(self, thickness_position, thickness_ratio)
        ScalarBlendModifier.__init__(self, blend, influence)
        rad = orientation / 180.0 * math.pi
        self.__orientation = mathutils.Vector((math.cos(rad), math.sin(rad)))
        self.__min_thickness = min_thickness
        self.__max_thickness = max_thickness
    def shade(self, stroke):
        func = VertexOrientation2DF0D()
        it = stroke.strokeVerticesBegin()
        while not it.isEnd():
            dir = func(it.castToInterface0DIterator())
            orthDir = mathutils.Vector((-dir.y, dir.x))
            orthDir.normalize()
            fac = abs(orthDir * self.__orientation)
            sv = it.getObject()
            a = sv.attribute().getThicknessRL()
            b = self.__min_thickness + fac * (self.__max_thickness - self.__min_thickness)
            b = max(b, 0.0)
            c = self.blend_thickness(a[0], a[1], b)
            self.set_thickness(sv, c[0], c[1])
            it.increment()

# Geometry modifiers

def iter_distance_along_stroke(stroke):
    distance = 0.0
    it = stroke.strokeVerticesBegin()
    while not it.isEnd():
        p = it.getObject().getPoint()
        if not it.isBegin():
            distance += (prev - p).length
        prev = p
        yield it, distance
        it.increment()

class SinusDisplacementShader(StrokeShader):
    def __init__(self, wavelength, amplitude, phase):
        StrokeShader.__init__(self)
        self._wavelength = wavelength
        self._amplitude = amplitude
        self._phase = phase / wavelength * 2 * math.pi
        self._getNormal = Normal2DF0D()
    def getName(self):
        return "SinusDisplacementShader"
    def shade(self, stroke):
        for it, distance in iter_distance_along_stroke(stroke):
            v = it.getObject()
            n = self._getNormal(it.castToInterface0DIterator())
            p = v.getPoint()
            u = v.u()
            n = n * self._amplitude * math.cos(distance / self._wavelength * 2 * math.pi + self._phase)
            v.setPoint(p + n)
        stroke.UpdateLength()

class PerlinNoise1DShader(StrokeShader):
    def __init__(self, freq = 10, amp = 10, oct = 4, angle = 45, seed = -1):
        StrokeShader.__init__(self)
        self.__noise = Noise(seed)
        self.__freq = freq
        self.__amp = amp
        self.__oct = oct
        theta = pi * angle / 180.0
        self.__dir = Vector([cos(theta), sin(theta)])
    def getName(self):
        return "PerlinNoise1DShader"
    def shade(self, stroke):
        length = stroke.getLength2D()
        it = stroke.strokeVerticesBegin()
        while not it.isEnd():
            v = it.getObject()
            nres = self.__noise.turbulence1(length * v.u(), self.__freq, self.__amp, self.__oct)
            v.setPoint(v.getPoint() + nres * self.__dir)
            it.increment()
        stroke.UpdateLength()

class PerlinNoise2DShader(StrokeShader):
    def __init__(self, freq = 10, amp = 10, oct = 4, angle = 45, seed = -1):
        StrokeShader.__init__(self)
        self.__noise = Noise(seed)
        self.__freq = freq
        self.__amp = amp
        self.__oct = oct
        theta = pi * angle / 180.0
        self.__dir = Vector([cos(theta), sin(theta)])
    def getName(self):
        return "PerlinNoise2DShader"
    def shade(self, stroke):
        it = stroke.strokeVerticesBegin()
        while not it.isEnd():
            v = it.getObject()
            vec = Vector([v.getProjectedX(), v.getProjectedY()])
            nres = self.__noise.turbulence2(vec, self.__freq, self.__amp, self.__oct)
            v.setPoint(v.getPoint() + nres * self.__dir)
            it.increment()
        stroke.UpdateLength()

class Offset2DShader(StrokeShader):
    def __init__(self, start, end, x, y):
        StrokeShader.__init__(self)
        self.__start = start
        self.__end = end
        self.__xy = Vector([x, y])
        self.__getNormal = Normal2DF0D()
    def getName(self):
        return "Offset2DShader"
    def shade(self, stroke):
        it = stroke.strokeVerticesBegin()
        while not it.isEnd():
            v = it.getObject()
            u = v.u()
            a = self.__start + u * (self.__end - self.__start)
            n = self.__getNormal(it.castToInterface0DIterator())
            n = n * a
            p = v.getPoint()
            v.setPoint(p + n + self.__xy)
            it.increment()
        stroke.UpdateLength()

class Transform2DShader(StrokeShader):
    def __init__(self, pivot, scale_x, scale_y, angle, pivot_u, pivot_x, pivot_y):
        StrokeShader.__init__(self)
        self.__pivot = pivot
        self.__scale_x = scale_x
        self.__scale_y = scale_y
        self.__angle = angle
        self.__pivot_u = pivot_u
        self.__pivot_x = pivot_x
        self.__pivot_y = pivot_y
    def getName(self):
        return "Transform2DShader"
    def shade(self, stroke):
        # determine the pivot of scaling and rotation operations
        if self.__pivot == "START":
            it = stroke.strokeVerticesBegin()
            pivot = it.getObject().getPoint()
        elif self.__pivot == "END":
            it = stroke.strokeVerticesEnd()
            it.decrement()
            pivot = it.getObject().getPoint()
        elif self.__pivot == "PARAM":
            p = None
            it = stroke.strokeVerticesBegin()
            while not it.isEnd():
                prev = p
                v = it.getObject()
                p = v.getPoint()
                u = v.u()
                if self.__pivot_u < u:
                    break
                it.increment()
            if prev is None:
                pivot = p
            else:
                delta = u - self.__pivot_u
                pivot = p + delta * (prev - p)
        elif self.__pivot == "CENTER":
            pivot = Vector([0.0, 0.0])
            n = 0
            it = stroke.strokeVerticesBegin()
            while not it.isEnd():
                p = it.getObject().getPoint()
                pivot = pivot + p
                n = n + 1
                it.increment()
            pivot.x = pivot.x / n
            pivot.y = pivot.y / n
        elif self.__pivot == "ABSOLUTE":
            pivot = Vector([self.__pivot_x, self.__pivot_y])
        # apply scaling and rotation operations
        cos_theta = math.cos(math.pi * self.__angle / 180.0)
        sin_theta = math.sin(math.pi * self.__angle / 180.0)
        it = stroke.strokeVerticesBegin()
        while not it.isEnd():
            v = it.getObject()
            p = v.getPoint()
            p = p - pivot
            x = p.x * self.__scale_x
            y = p.y * self.__scale_y
            p.x = x * cos_theta - y * sin_theta
            p.y = x * sin_theta + y * cos_theta
            v.setPoint(p + pivot)
            it.increment()
        stroke.UpdateLength()

# Predicates and helper functions

class QuantitativeInvisibilityRangeUP1D(UnaryPredicate1D):
    def __init__(self, qi_start, qi_end):
        UnaryPredicate1D.__init__(self)
        self.__getQI = QuantitativeInvisibilityF1D()
        self.__qi_start = qi_start
        self.__qi_end = qi_end
    def getName(self):
        return "QuantitativeInvisibilityRangeUP1D"
    def __call__(self, inter):
        qi = self.__getQI(inter)
        return self.__qi_start <= qi <= self.__qi_end

def join_unary_predicates(upred_list, bpred):
    if not upred_list:
        return None
    upred = upred_list[0]
    for p in upred_list[1:]:
        upred = bpred(upred, p)
    return upred

class ObjectNamesUP1D(UnaryPredicate1D):
    def __init__(self, names, negative):
        UnaryPredicate1D.__init__(self)
        self._names = names
        self._negative = negative
    def getName(self):
        return "ObjectNamesUP1D"
    def __call__(self, viewEdge):
        found = viewEdge.viewShape().getName() in self._names
        if self._negative:
            return not found
        return found

# Stroke caps

def iter_stroke_vertices(stroke):
    it = stroke.strokeVerticesBegin()
    prev_p = None
    while not it.isEnd():
        sv = it.getObject()
        p = sv.getPoint()
        if prev_p is None or (prev_p - p).length > 1e-6:
            yield sv
            prev_p = p
        it.increment()

class RoundCapShader(StrokeShader):
    def round_cap_thickness(self, x):
        x = max(0.0, min(x, 1.0))
        return math.sqrt(1.0 - (x ** 2))
    def shade(self, stroke):
        # save the location and attribute of stroke vertices
        buffer = []
        for sv in iter_stroke_vertices(stroke):
            buffer.append((sv.getPoint(), sv.attribute()))
        nverts = len(buffer)
        if nverts < 2:
            return
        # calculate the number of additional vertices to form caps
        R, L = stroke[0].attribute().getThicknessRL()
        caplen_beg = (R + L) / 2.0
        nverts_beg = max(5, int(R + L))
        R, L = stroke[-1].attribute().getThicknessRL()
        caplen_end = (R + L) / 2.0
        nverts_end = max(5, int(R + L))
        # adjust the total number of stroke vertices
        stroke.Resample(nverts + nverts_beg + nverts_end)
        # restore the location and attribute of the original vertices
        for i in range(nverts):
            p, attr = buffer[i]
            stroke[nverts_beg + i].setPoint(p)
            stroke[nverts_beg + i].setAttribute(attr)
        # reshape the cap at the beginning of the stroke
        q, attr = buffer[1]
        p, attr = buffer[0]
        d = p - q
        d = d / d.length * caplen_beg
        n = 1.0 / nverts_beg
        R, L = attr.getThicknessRL()
        for i in range(nverts_beg):
            t = (nverts_beg - i) * n
            stroke[i].setPoint(p + d * t)
            r = self.round_cap_thickness((nverts_beg - i + 1) * n)
            stroke[i].setAttribute(attr)
            stroke[i].attribute().setThickness(R * r, L * r)
        # reshape the cap at the end of the stroke
        q, attr = buffer[-2]
        p, attr = buffer[-1]
        d = p - q
        d = d / d.length * caplen_end
        n = 1.0 / nverts_end
        R, L = attr.getThicknessRL()
        for i in range(nverts_end):
            t = (nverts_end - i) * n
            stroke[-i-1].setPoint(p + d * t)
            r = self.round_cap_thickness((nverts_end - i + 1) * n)
            stroke[-i-1].setAttribute(attr)
            stroke[-i-1].attribute().setThickness(R * r, L * r)
        # update the curvilinear 2D length of each vertex
        stroke.UpdateLength()

class SquareCapShader(StrokeShader):
    def shade(self, stroke):
        # save the location and attribute of stroke vertices
        buffer = []
        for sv in iter_stroke_vertices(stroke):
            buffer.append((sv.getPoint(), sv.attribute()))
        nverts = len(buffer)
        if nverts < 2:
            return
        # calculate the number of additional vertices to form caps
        R, L = stroke[0].attribute().getThicknessRL()
        caplen_beg = (R + L) / 2.0
        nverts_beg = 1
        R, L = stroke[-1].attribute().getThicknessRL()
        caplen_end = (R + L) / 2.0
        nverts_end = 1
        # adjust the total number of stroke vertices
        stroke.Resample(nverts + nverts_beg + nverts_end)
        # restore the location and attribute of the original vertices
        for i in range(nverts):
            p, attr = buffer[i]
            stroke[nverts_beg + i].setPoint(p)
            stroke[nverts_beg + i].setAttribute(attr)
        # reshape the cap at the beginning of the stroke
        q, attr = buffer[1]
        p, attr = buffer[0]
        d = p - q
        stroke[0].setPoint(p + d / d.length * caplen_beg)
        stroke[0].setAttribute(attr)
        # reshape the cap at the end of the stroke
        q, attr = buffer[-2]
        p, attr = buffer[-1]
        d = p - q
        stroke[-1].setPoint(p + d / d.length * caplen_beg)
        stroke[-1].setAttribute(attr)
        # update the curvilinear 2D length of each vertex
        stroke.UpdateLength()

# Split by dashed line pattern

class SplitPatternStartingUP0D(UnaryPredicate0D):
    def __init__(self, controller):
        UnaryPredicate0D.__init__(self)
        self._controller = controller
    def __call__(self, inter):
        return self._controller.start()

class SplitPatternStoppingUP0D(UnaryPredicate0D):
    def __init__(self, controller):
        UnaryPredicate0D.__init__(self)
        self._controller = controller
    def __call__(self, inter):
        return self._controller.stop()

class SplitPatternController:
    def __init__(self, pattern, sampling):
        self.sampling = float(sampling)
        k = len(pattern) // 2
        n = k * 2
        self.start_pos = [pattern[i] + pattern[i+1] for i in range(0, n, 2)]
        self.stop_pos = [pattern[i] for i in range(0, n, 2)]
        self.init()
    def init(self):
        self.start_len = 0.0
        self.start_idx = 0
        self.stop_len = self.sampling
        self.stop_idx = 0
    def start(self):
        self.start_len += self.sampling
        if abs(self.start_len - self.start_pos[self.start_idx]) < self.sampling / 2.0:
            self.start_len = 0.0
            self.start_idx = (self.start_idx + 1) % len(self.start_pos)
            return True
        return False
    def stop(self):
        if self.start_len > 0.0:
            self.init()
        self.stop_len += self.sampling
        if abs(self.stop_len - self.stop_pos[self.stop_idx]) < self.sampling / 2.0:
            self.stop_len = self.sampling
            self.stop_idx = (self.stop_idx + 1) % len(self.stop_pos)
            return True
        return False

# Dashed line

class DashedLineShader(StrokeShader):
    def __init__(self, pattern):
        StrokeShader.__init__(self)
        self._pattern = pattern
    def getName(self):
        return "DashedLineShader"
    def shade(self, stroke):
        index = 0 # pattern index
        start = 0.0 # 2D curvilinear length
        visible = True
        sampling = 1.0
        it = stroke.strokeVerticesBegin(sampling)
        while not it.isEnd():
            pos = it.t()
            # The extra 'sampling' term is added below, because the
            # visibility attribute of the i-th vertex refers to the
            # visibility of the stroke segment between the i-th and
            # (i+1)-th vertices.
            if pos - start + sampling > self._pattern[index]:
                start = pos
                index += 1
                if index == len(self._pattern):
                    index = 0
                visible = not visible
            it.getObject().attribute().setVisible(visible)
            it.increment()

# predicates for chaining

class AngleLargerThanBP1D(BinaryPredicate1D):
    def __init__(self, angle):
        BinaryPredicate1D.__init__(self)
        self._angle = math.pi * angle / 180.0
    def getName(self):
        return "AngleLargerThanBP1D"
    def __call__(self, i1, i2):
        fe1a = i1.fedgeA()
        fe1b = i1.fedgeB()
        fe2a = i2.fedgeA()
        fe2b = i2.fedgeB()
        sv1a = fe1a.vertexA().getPoint2D()
        sv1b = fe1b.vertexB().getPoint2D()
        sv2a = fe2a.vertexA().getPoint2D()
        sv2b = fe2b.vertexB().getPoint2D()
        if (sv1a - sv2a).length < 1e-6:
            dir1 = sv1a - sv1b
            dir2 = sv2b - sv2a
        elif (sv1b - sv2b).length < 1e-6:
            dir1 = sv1b - sv1a
            dir2 = sv2a - sv2b
        elif (sv1a - sv2b).length < 1e-6:
            dir1 = sv1a - sv1b
            dir2 = sv2a - sv2b
        elif (sv1b - sv2a).length < 1e-6:
            dir1 = sv1b - sv1a
            dir2 = sv2b - sv2a
        else:
            return False
        denom = dir1.length * dir2.length
        if denom < 1e-6:
            return False
        x = (dir1 * dir2) / denom
        return math.acos(min(max(x, -1.0), 1.0)) > self._angle

class AndBP1D(BinaryPredicate1D):
    def __init__(self, pred1, pred2):
        BinaryPredicate1D.__init__(self)
        self.__pred1 = pred1
        self.__pred2 = pred2
    def getName(self):
        return "AndBP1D"
    def __call__(self, i1, i2):
        return self.__pred1(i1, i2) and self.__pred2(i1, i2)

# predicates for selection

class LengthThresholdUP1D(UnaryPredicate1D):
    def __init__(self, min_length=None, max_length=None):
        UnaryPredicate1D.__init__(self)
        self._min_length = min_length
        self._max_length = max_length
    def getName(self):
        return "LengthThresholdUP1D"
    def __call__(self, inter):
        length = inter.getLength2D()
        if self._min_length is not None and length < self._min_length:
            return False
        if self._max_length is not None and length > self._max_length:
            return False
        return True

class FaceMarkBothUP1D(UnaryPredicate1D):
    def __call__(self, inter): # ViewEdge
        fe = inter.fedgeA()
        while fe is not None:
            if fe.isSmooth():
                if fe.faceMark():
                    return True
            else:
                if fe.aFaceMark() and fe.bFaceMark():
                    return True
            fe = fe.nextEdge()
        return False

class FaceMarkOneUP1D(UnaryPredicate1D):
    def __call__(self, inter): # ViewEdge
        fe = inter.fedgeA()
        while fe is not None:
            if fe.isSmooth():
                if fe.faceMark():
                    return True
            else:
                if fe.aFaceMark() or fe.bFaceMark():
                    return True
            fe = fe.nextEdge()
        return False

# predicates for splitting

class MaterialBoundaryUP0D(UnaryPredicate0D):
    def getName(self):
        return "MaterialBoundaryUP0D"
    def __call__(self, it):
        if it.isBegin():
            return False
        it_prev = Interface0DIterator(it) 
        it_prev.decrement()
        v = it.getObject()
        it.increment()
        if it.isEnd():
            return False
        fe = v.getFEdge(it_prev.getObject())
        idx1 = fe.materialIndex() if fe.isSmooth() else fe.bMaterialIndex()
        fe = v.getFEdge(it.getObject())
        idx2 = fe.materialIndex() if fe.isSmooth() else fe.bMaterialIndex()
        return idx1 != idx2

class Curvature2DAngleThresholdUP0D(UnaryPredicate0D):
    def __init__(self, min_angle=None, max_angle=None):
        UnaryPredicate0D.__init__(self)
        self._min_angle = min_angle
        self._max_angle = max_angle
        self._func = Curvature2DAngleF0D()
    def getName(self):
        return "Curvature2DAngleThresholdUP0D"
    def __call__(self, inter):
        angle = math.pi - self._func(inter)
        if self._min_angle is not None and angle < self._min_angle:
            return True
        if self._max_angle is not None and angle > self._max_angle:
            return True
        return False

class Length2DThresholdUP0D(UnaryPredicate0D):
    def __init__(self, length_limit):
        UnaryPredicate0D.__init__(self)
        self._length_limit = length_limit
        self._t = 0.0
    def getName(self):
        return "Length2DThresholdUP0D"
    def __call__(self, inter):
        t = inter.t() # curvilinear abscissa
        if t < self._t:
            self._t = 0.0
            return False
        if t - self._t < self._length_limit:
            return False
        self._t = t
        return True

# Seed for random number generation

class Seed:
    def __init__(self):
        self.t_max = 2 ** 15
        self.t = int(time.time()) % self.t_max
    def get(self, seed):
        if seed < 0:
            self.t = (self.t + 1) % self.t_max
            return self.t
        return seed

_seed = Seed()

# main function for parameter processing

def process(layer_name, lineset_name):
    scene = Freestyle.getCurrentScene()
    layer = scene.render.layers[layer_name]
    lineset = layer.freestyle_settings.linesets[lineset_name]
    linestyle = lineset.linestyle

    selection_criteria = []
    # prepare selection criteria by visibility
    if lineset.select_by_visibility:
        if lineset.visibility == "VISIBLE":
            selection_criteria.append(
                QuantitativeInvisibilityUP1D(0))
        elif lineset.visibility == "HIDDEN":
            selection_criteria.append(
                NotUP1D(QuantitativeInvisibilityUP1D(0)))
        elif lineset.visibility == "RANGE":
            selection_criteria.append(
                QuantitativeInvisibilityRangeUP1D(lineset.qi_start, lineset.qi_end))
    # prepare selection criteria by edge types
    if lineset.select_by_edge_types:
        edge_type_criteria = []
        if lineset.select_silhouette:
            upred = pyNatureUP1D(Nature.SILHOUETTE)
            edge_type_criteria.append(NotUP1D(upred) if lineset.exclude_silhouette else upred)
        if lineset.select_border:
            upred = pyNatureUP1D(Nature.BORDER)
            edge_type_criteria.append(NotUP1D(upred) if lineset.exclude_border else upred)
        if lineset.select_crease:
            upred = pyNatureUP1D(Nature.CREASE)
            edge_type_criteria.append(NotUP1D(upred) if lineset.exclude_crease else upred)
        if lineset.select_ridge_valley:
            upred = pyNatureUP1D(Nature.RIDGE)
            edge_type_criteria.append(NotUP1D(upred) if lineset.exclude_ridge_valley else upred)
        if lineset.select_suggestive_contour:
            upred = pyNatureUP1D(Nature.SUGGESTIVE_CONTOUR)
            edge_type_criteria.append(NotUP1D(upred) if lineset.exclude_suggestive_contour else upred)
        if lineset.select_material_boundary:
            upred = pyNatureUP1D(Nature.MATERIAL_BOUNDARY)
            edge_type_criteria.append(NotUP1D(upred) if lineset.exclude_material_boundary else upred)
        if lineset.select_edge_mark:
            upred = pyNatureUP1D(Nature.EDGE_MARK)
            edge_type_criteria.append(NotUP1D(upred) if lineset.exclude_edge_mark else upred)
        if lineset.select_contour:
            upred = ContourUP1D()
            edge_type_criteria.append(NotUP1D(upred) if lineset.exclude_contour else upred)
        if lineset.select_external_contour:
            upred = ExternalContourUP1D()
            edge_type_criteria.append(NotUP1D(upred) if lineset.exclude_external_contour else upred)
        if lineset.edge_type_combination == "OR":
            upred = join_unary_predicates(edge_type_criteria, OrUP1D)
        else:
            upred = join_unary_predicates(edge_type_criteria, AndUP1D)
        if upred is not None:
            if lineset.edge_type_negation == "EXCLUSIVE":
                upred = NotUP1D(upred)
            selection_criteria.append(upred)
    # prepare selection criteria by face marks
    if lineset.select_by_face_marks:
        if lineset.face_mark_condition == "BOTH":
            upred = FaceMarkBothUP1D()
        else:
            upred = FaceMarkOneUP1D()
        if lineset.face_mark_negation == "EXCLUSIVE":
            upred = NotUP1D(upred)
        selection_criteria.append(upred)
    # prepare selection criteria by group of objects
    if lineset.select_by_group:
        if lineset.group is not None:
            names = dict((ob.name, True) for ob in lineset.group.objects)
            upred = ObjectNamesUP1D(names, lineset.group_negation == 'EXCLUSIVE')
            selection_criteria.append(upred)
    # prepare selection criteria by image border
    if lineset.select_by_image_border:
        w = scene.render.resolution_x
        h = scene.render.resolution_y
        if scene.render.use_border:
            xmin = scene.render.border_min_x * w
            xmax = scene.render.border_max_x * w
            ymin = scene.render.border_min_y * h
            ymax = scene.render.border_max_y * h
        else:
            xmin, xmax = 0.0, float(w)
            ymin, ymax = 0.0, float(h)
        upred = WithinImageBoundaryUP1D(xmin, ymin, xmax, ymax)
        selection_criteria.append(upred)
    # select feature edges
    upred = join_unary_predicates(selection_criteria, AndUP1D)
    if upred is None:
        upred = TrueUP1D()
    Operators.select(upred)
    # join feature edges to form chains
    if linestyle.use_chaining:
        if linestyle.chaining == "PLAIN":
            if linestyle.same_object:
                Operators.bidirectionalChain(ChainSilhouetteIterator(), NotUP1D(upred))
            else:
                Operators.bidirectionalChain(ChainPredicateIterator(upred, TrueBP1D()), NotUP1D(upred))
        elif linestyle.chaining == "SKETCHY":
            if linestyle.same_object:
                Operators.bidirectionalChain(pySketchyChainSilhouetteIterator(linestyle.rounds))
            else:
                Operators.bidirectionalChain(pySketchyChainingIterator(linestyle.rounds))
    else:
        Operators.chain(ChainPredicateIterator(FalseUP1D(), FalseBP1D()), NotUP1D(upred))
    # split chains
    if linestyle.material_boundary:
        Operators.sequentialSplit(MaterialBoundaryUP0D())
    if linestyle.use_min_angle or linestyle.use_max_angle:
        min_angle = linestyle.min_angle if linestyle.use_min_angle else None
        max_angle = linestyle.max_angle if linestyle.use_max_angle else None
        Operators.sequentialSplit(Curvature2DAngleThresholdUP0D(min_angle, max_angle))
    if linestyle.use_split_length:
        Operators.sequentialSplit(Length2DThresholdUP0D(linestyle.split_length), 1.0)
    if linestyle.use_split_pattern:
        pattern = []
        if linestyle.split_dash1 > 0 and linestyle.split_gap1 > 0:
            pattern.append(linestyle.split_dash1)
            pattern.append(linestyle.split_gap1)
        if linestyle.split_dash2 > 0 and linestyle.split_gap2 > 0:
            pattern.append(linestyle.split_dash2)
            pattern.append(linestyle.split_gap2)
        if linestyle.split_dash3 > 0 and linestyle.split_gap3 > 0:
            pattern.append(linestyle.split_dash3)
            pattern.append(linestyle.split_gap3)
        if len(pattern) > 0:
            sampling = 1.0
            controller = SplitPatternController(pattern, sampling)
            Operators.sequentialSplit(SplitPatternStartingUP0D(controller),
                                      SplitPatternStoppingUP0D(controller),
                                      sampling)
    # select chains
    if linestyle.use_min_length or linestyle.use_max_length:
        min_length = linestyle.min_length if linestyle.use_min_length else None
        max_length = linestyle.max_length if linestyle.use_max_length else None
        Operators.select(LengthThresholdUP1D(min_length, max_length))
    # prepare a list of stroke shaders
    shaders_list = []
    for m in linestyle.geometry_modifiers:
        if not m.use:
            continue
        if m.type == "SAMPLING":
            shaders_list.append(SamplingShader(
                m.sampling))
        elif m.type == "BEZIER_CURVE":
            shaders_list.append(BezierCurveShader(
                m.error))
        elif m.type == "SINUS_DISPLACEMENT":
            shaders_list.append(SinusDisplacementShader(
                m.wavelength, m.amplitude, m.phase))
        elif m.type == "SPATIAL_NOISE":
            shaders_list.append(SpatialNoiseShader(
                m.amplitude, m.scale, m.octaves, m.smooth, m.pure_random))
        elif m.type == "PERLIN_NOISE_1D":
            shaders_list.append(PerlinNoise1DShader(
                m.frequency, m.amplitude, m.octaves, m.angle, _seed.get(m.seed)))
        elif m.type == "PERLIN_NOISE_2D":
            shaders_list.append(PerlinNoise2DShader(
                m.frequency, m.amplitude, m.octaves, m.angle, _seed.get(m.seed)))
        elif m.type == "BACKBONE_STRETCHER":
            shaders_list.append(BackboneStretcherShader(
                m.backbone_length))
        elif m.type == "TIP_REMOVER":
            shaders_list.append(TipRemoverShader(
                m.tip_length))
        elif m.type == "POLYGONIZATION":
            shaders_list.append(PolygonalizationShader(
                m.error))
        elif m.type == "GUIDING_LINES":
            shaders_list.append(GuidingLinesShader(
                m.offset))
        elif m.type == "BLUEPRINT":
            if m.shape == "CIRCLES":
                shaders_list.append(pyBluePrintCirclesShader(
                    m.rounds, m.random_radius, m.random_center))
            elif m.shape == "ELLIPSES":
                shaders_list.append(pyBluePrintEllipsesShader(
                    m.rounds, m.random_radius, m.random_center))
            elif m.shape == "SQUARES":
                shaders_list.append(pyBluePrintSquaresShader(
                    m.rounds, m.backbone_length, m.random_backbone))
        elif m.type == "2D_OFFSET":
            shaders_list.append(Offset2DShader(
                m.start, m.end, m.x, m.y))
        elif m.type == "2D_TRANSFORM":
            shaders_list.append(Transform2DShader(
                m.pivot, m.scale_x, m.scale_y, m.angle, m.pivot_u, m.pivot_x, m.pivot_y))
    color = linestyle.color
    if (not linestyle.use_chaining) or (linestyle.chaining == "PLAIN" and linestyle.same_object):
        thickness_position = linestyle.thickness_position
    else:
        thickness_position = "CENTER"
        print("Warning: Thickness poisition options are applied when chaining is disabled")
        print("    or the Plain chaining is used with the Same Object option enabled.")
    shaders_list.append(BaseColorShader(color.r, color.g, color.b, linestyle.alpha))
    shaders_list.append(BaseThicknessShader(linestyle.thickness, thickness_position,
                                            linestyle.thickness_ratio))
    for m in linestyle.color_modifiers:
        if not m.use:
            continue
        if m.type == "ALONG_STROKE":
            shaders_list.append(ColorAlongStrokeShader(
                m.blend, m.influence, m.color_ramp))
        elif m.type == "DISTANCE_FROM_CAMERA":
            shaders_list.append(ColorDistanceFromCameraShader(
                m.blend, m.influence, m.color_ramp,
                m.range_min, m.range_max))
        elif m.type == "DISTANCE_FROM_OBJECT":
            shaders_list.append(ColorDistanceFromObjectShader(
                m.blend, m.influence, m.color_ramp, m.target,
                m.range_min, m.range_max))
        elif m.type == "MATERIAL":
            shaders_list.append(ColorMaterialShader(
                m.blend, m.influence, m.color_ramp, m.material_attr,
                m.use_ramp))
    for m in linestyle.alpha_modifiers:
        if not m.use:
            continue
        if m.type == "ALONG_STROKE":
            shaders_list.append(AlphaAlongStrokeShader(
                m.blend, m.influence, m.mapping, m.invert, m.curve))
        elif m.type == "DISTANCE_FROM_CAMERA":
            shaders_list.append(AlphaDistanceFromCameraShader(
                m.blend, m.influence, m.mapping, m.invert, m.curve,
                m.range_min, m.range_max))
        elif m.type == "DISTANCE_FROM_OBJECT":
            shaders_list.append(AlphaDistanceFromObjectShader(
                m.blend, m.influence, m.mapping, m.invert, m.curve, m.target,
                m.range_min, m.range_max))
        elif m.type == "MATERIAL":
            shaders_list.append(AlphaMaterialShader(
                m.blend, m.influence, m.mapping, m.invert, m.curve,
                m.material_attr))
    for m in linestyle.thickness_modifiers:
        if not m.use:
            continue
        if m.type == "ALONG_STROKE":
            shaders_list.append(ThicknessAlongStrokeShader(
                thickness_position, linestyle.thickness_ratio,
                m.blend, m.influence, m.mapping, m.invert, m.curve,
                m.value_min, m.value_max))
        elif m.type == "DISTANCE_FROM_CAMERA":
            shaders_list.append(ThicknessDistanceFromCameraShader(
                thickness_position, linestyle.thickness_ratio,
                m.blend, m.influence, m.mapping, m.invert, m.curve,
                m.range_min, m.range_max, m.value_min, m.value_max))
        elif m.type == "DISTANCE_FROM_OBJECT":
            shaders_list.append(ThicknessDistanceFromObjectShader(
                thickness_position, linestyle.thickness_ratio,
                m.blend, m.influence, m.mapping, m.invert, m.curve, m.target,
                m.range_min, m.range_max, m.value_min, m.value_max))
        elif m.type == "MATERIAL":
            shaders_list.append(ThicknessMaterialShader(
                thickness_position, linestyle.thickness_ratio,
                m.blend, m.influence, m.mapping, m.invert, m.curve,
                m.material_attr, m.value_min, m.value_max))
        elif m.type == "CALLIGRAPHY":
            shaders_list.append(CalligraphicThicknessShader(
                thickness_position, linestyle.thickness_ratio,
                m.blend, m.influence,
                m.orientation, m.min_thickness, m.max_thickness))
    if linestyle.caps == "ROUND":
        shaders_list.append(RoundCapShader())
    elif linestyle.caps == "SQUARE":
        shaders_list.append(SquareCapShader())
    if linestyle.use_dashed_line:
        pattern = []
        if linestyle.dash1 > 0 and linestyle.gap1 > 0:
            pattern.append(linestyle.dash1)
            pattern.append(linestyle.gap1)
        if linestyle.dash2 > 0 and linestyle.gap2 > 0:
            pattern.append(linestyle.dash2)
            pattern.append(linestyle.gap2)
        if linestyle.dash3 > 0 and linestyle.gap3 > 0:
            pattern.append(linestyle.dash3)
            pattern.append(linestyle.gap3)
        if len(pattern) > 0:
            shaders_list.append(DashedLineShader(pattern))
    # create strokes using the shaders list
    Operators.create(TrueUP1D(), shaders_list)
