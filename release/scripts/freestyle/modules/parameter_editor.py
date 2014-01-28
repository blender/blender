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

#  Filename : parameter_editor.py
#  Authors  : Tamito Kajiyama
#  Date     : 26/07/2010
#  Purpose  : Interactive manipulation of stylization parameters

from freestyle.types import (
    BinaryPredicate1D,
    Interface0DIterator,
    Nature,
    Noise,
    Operators,
    StrokeAttribute,
    UnaryPredicate0D,
    UnaryPredicate1D,
    TVertex,
    )
from freestyle.chainingiterators import (
    ChainPredicateIterator,
    ChainSilhouetteIterator,
    pySketchyChainSilhouetteIterator,
    pySketchyChainingIterator,
    )
from freestyle.functions import (
    Curvature2DAngleF0D,
    CurveMaterialF0D,
    Normal2DF0D,
    QuantitativeInvisibilityF1D,
    VertexOrientation2DF0D,
    )
from freestyle.predicates import (
    AndUP1D,
    ContourUP1D,
    ExternalContourUP1D,
    FalseBP1D,
    FalseUP1D,
    NotUP1D,
    OrUP1D,
    QuantitativeInvisibilityUP1D,
    TrueBP1D,
    TrueUP1D,
    WithinImageBoundaryUP1D,
    pyNatureUP1D,
    )
from freestyle.shaders import (
    BackboneStretcherShader,
    BezierCurveShader,
    ConstantColorShader,
    GuidingLinesShader,
    PolygonalizationShader,
    SamplingShader,
    SpatialNoiseShader,
    StrokeShader,
    TipRemoverShader,
    pyBluePrintCirclesShader,
    pyBluePrintEllipsesShader,
    pyBluePrintSquaresShader,
    )
from freestyle.utils import (
    ContextFunctions,
    getCurrentScene,
    )
from _freestyle import (
    blendRamp,
    evaluateColorRamp,
    evaluateCurveMappingF,
    )
import math
import mathutils
import time


class ColorRampModifier(StrokeShader):
    def __init__(self, blend, influence, ramp):
        StrokeShader.__init__(self)
        self.__blend = blend
        self.__influence = influence
        self.__ramp = ramp

    def evaluate(self, t):
        col = evaluateColorRamp(self.__ramp, t)
        col = col.xyz  # omit alpha
        return col

    def blend_ramp(self, a, b):
        return blendRamp(self.__blend, a, self.__influence, b)


class ScalarBlendModifier(StrokeShader):
    def __init__(self, blend, influence):
        StrokeShader.__init__(self)
        self.__blend = blend
        self.__influence = influence

    def blend(self, v1, v2):
        fac = self.__influence
        facm = 1.0 - fac
        if self.__blend == 'MIX':
            v1 = facm * v1 + fac * v2
        elif self.__blend == 'ADD':
            v1 += fac * v2
        elif self.__blend == 'MULTIPLY':
            v1 *= facm + fac * v2
        elif self.__blend == 'SUBTRACT':
            v1 -= fac * v2
        elif self.__blend == 'DIVIDE':
            if v2 != 0.0:
                v1 = facm * v1 + fac * v1 / v2
        elif self.__blend == 'DIFFERENCE':
            v1 = facm * v1 + fac * abs(v1 - v2)
        elif self.__blend == 'MININUM':
            tmp = fac * v2
            if v1 > tmp:
                v1 = tmp
        elif self.__blend == 'MAXIMUM':
            tmp = fac * v2
            if v1 < tmp:
                v1 = tmp
        else:
            raise ValueError("unknown curve blend type: " + self.__blend)
        return v1


class CurveMappingModifier(ScalarBlendModifier):
    def __init__(self, blend, influence, mapping, invert, curve):
        ScalarBlendModifier.__init__(self, blend, influence)
        assert mapping in {'LINEAR', 'CURVE'}
        self.__mapping = getattr(self, mapping)
        self.__invert = invert
        self.__curve = curve

    def LINEAR(self, t):
        if self.__invert:
            return 1.0 - t
        return t

    def CURVE(self, t):
        return evaluateCurveMappingF(self.__curve, 0, t)

    def evaluate(self, t):
        return self.__mapping(t)


class ThicknessModifierMixIn:
    def __init__(self):
        scene = getCurrentScene()
        self.__persp_camera = (scene.camera.data.type == 'PERSP')

    def set_thickness(self, sv, outer, inner):
        fe = sv.first_svertex.get_fedge(sv.second_svertex)
        nature = fe.nature
        if (nature & Nature.BORDER):
            if self.__persp_camera:
                point = -sv.point_3d.copy()
                point.normalize()
                dir = point.dot(fe.normal_left)
            else:
                dir = fe.normal_left.z
            if dir < 0.0:  # the back side is visible
                outer, inner = inner, outer
        elif (nature & Nature.SILHOUETTE):
            if fe.is_smooth:  # TODO more tests needed
                outer, inner = inner, outer
        else:
            outer = inner = (outer + inner) / 2
        sv.attribute.thickness = (outer, inner)


class ThicknessBlenderMixIn(ThicknessModifierMixIn):
    def __init__(self, position, ratio):
        ThicknessModifierMixIn.__init__(self)
        self.__position = position
        self.__ratio = ratio

    def blend_thickness(self, outer, inner, v):
        v = self.blend(outer + inner, v)
        if self.__position == 'CENTER':
            outer = v * 0.5
            inner = v - outer
        elif self.__position == 'INSIDE':
            outer = 0
            inner = v
        elif self.__position == 'OUTSIDE':
            outer = v
            inner = 0
        elif self.__position == 'RELATIVE':
            outer = v * self.__ratio
            inner = v - outer
        else:
            raise ValueError("unknown thickness position: " + self.__position)
        return outer, inner


class BaseColorShader(ConstantColorShader):
    pass


class BaseThicknessShader(StrokeShader, ThicknessModifierMixIn):
    def __init__(self, thickness, position, ratio):
        StrokeShader.__init__(self)
        ThicknessModifierMixIn.__init__(self)
        if position == 'CENTER':
            self.__outer = thickness * 0.5
            self.__inner = thickness - self.__outer
        elif position == 'INSIDE':
            self.__outer = 0
            self.__inner = thickness
        elif position == 'OUTSIDE':
            self.__outer = thickness
            self.__inner = 0
        elif position == 'RELATIVE':
            self.__outer = thickness * ratio
            self.__inner = thickness - self.__outer
        else:
            raise ValueError("unknown thickness position: " + self.position)

    def shade(self, stroke):
        it = stroke.stroke_vertices_begin()
        while not it.is_end:
            sv = it.object
            self.set_thickness(sv, self.__outer, self.__inner)
            it.increment()


# Along Stroke modifiers

def iter_t2d_along_stroke(stroke):
    total = stroke.length_2d
    distance = 0.0
    it = stroke.stroke_vertices_begin()
    prev = it.object.point
    while not it.is_end:
        p = it.object.point
        distance += (prev - p).length
        prev = p.copy()  # need a copy because the point can be altered
        t = min(distance / total, 1.0) if total > 0.0 else 0.0
        yield it, t
        it.increment()


class ColorAlongStrokeShader(ColorRampModifier):
    def shade(self, stroke):
        for it, t in iter_t2d_along_stroke(stroke):
            sv = it.object
            a = sv.attribute.color
            b = self.evaluate(t)
            sv.attribute.color = self.blend_ramp(a, b)


class AlphaAlongStrokeShader(CurveMappingModifier):
    def shade(self, stroke):
        for it, t in iter_t2d_along_stroke(stroke):
            sv = it.object
            a = sv.attribute.alpha
            b = self.evaluate(t)
            sv.attribute.alpha = self.blend(a, b)


class ThicknessAlongStrokeShader(ThicknessBlenderMixIn, CurveMappingModifier):
    def __init__(self, thickness_position, thickness_ratio,
                 blend, influence, mapping, invert, curve, value_min, value_max):
        ThicknessBlenderMixIn.__init__(self, thickness_position, thickness_ratio)
        CurveMappingModifier.__init__(self, blend, influence, mapping, invert, curve)
        self.__value_min = value_min
        self.__value_max = value_max

    def shade(self, stroke):
        for it, t in iter_t2d_along_stroke(stroke):
            sv = it.object
            a = sv.attribute.thickness
            b = self.__value_min + self.evaluate(t) * (self.__value_max - self.__value_min)
            c = self.blend_thickness(a[0], a[1], b)
            self.set_thickness(sv, c[0], c[1])


# Distance from Camera modifiers

def iter_distance_from_camera(stroke, range_min, range_max):
    normfac = range_max - range_min  # normalization factor
    it = stroke.stroke_vertices_begin()
    while not it.is_end:
        p = it.object.point_3d  # in the camera coordinate
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

    def shade(self, stroke):
        for it, t in iter_distance_from_camera(stroke, self.__range_min, self.__range_max):
            sv = it.object
            a = sv.attribute.color
            b = self.evaluate(t)
            sv.attribute.color = self.blend_ramp(a, b)


class AlphaDistanceFromCameraShader(CurveMappingModifier):
    def __init__(self, blend, influence, mapping, invert, curve, range_min, range_max):
        CurveMappingModifier.__init__(self, blend, influence, mapping, invert, curve)
        self.__range_min = range_min
        self.__range_max = range_max

    def shade(self, stroke):
        for it, t in iter_distance_from_camera(stroke, self.__range_min, self.__range_max):
            sv = it.object
            a = sv.attribute.alpha
            b = self.evaluate(t)
            sv.attribute.alpha = self.blend(a, b)


class ThicknessDistanceFromCameraShader(ThicknessBlenderMixIn, CurveMappingModifier):
    def __init__(self, thickness_position, thickness_ratio,
                 blend, influence, mapping, invert, curve, range_min, range_max, value_min, value_max):
        ThicknessBlenderMixIn.__init__(self, thickness_position, thickness_ratio)
        CurveMappingModifier.__init__(self, blend, influence, mapping, invert, curve)
        self.__range_min = range_min
        self.__range_max = range_max
        self.__value_min = value_min
        self.__value_max = value_max

    def shade(self, stroke):
        for it, t in iter_distance_from_camera(stroke, self.__range_min, self.__range_max):
            sv = it.object
            a = sv.attribute.thickness
            b = self.__value_min + self.evaluate(t) * (self.__value_max - self.__value_min)
            c = self.blend_thickness(a[0], a[1], b)
            self.set_thickness(sv, c[0], c[1])


# Distance from Object modifiers

def iter_distance_from_object(stroke, object, range_min, range_max):
    scene = getCurrentScene()
    mv = scene.camera.matrix_world.copy()  # model-view matrix
    mv.invert()
    loc = mv * object.location  # loc in the camera coordinate
    normfac = range_max - range_min  # normalization factor
    it = stroke.stroke_vertices_begin()
    while not it.is_end:
        p = it.object.point_3d  # in the camera coordinate
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

    def shade(self, stroke):
        if self.__target is None:
            return
        for it, t in iter_distance_from_object(stroke, self.__target, self.__range_min, self.__range_max):
            sv = it.object
            a = sv.attribute.color
            b = self.evaluate(t)
            sv.attribute.color = self.blend_ramp(a, b)


class AlphaDistanceFromObjectShader(CurveMappingModifier):
    def __init__(self, blend, influence, mapping, invert, curve, target, range_min, range_max):
        CurveMappingModifier.__init__(self, blend, influence, mapping, invert, curve)
        self.__target = target
        self.__range_min = range_min
        self.__range_max = range_max

    def shade(self, stroke):
        if self.__target is None:
            return
        for it, t in iter_distance_from_object(stroke, self.__target, self.__range_min, self.__range_max):
            sv = it.object
            a = sv.attribute.alpha
            b = self.evaluate(t)
            sv.attribute.alpha = self.blend(a, b)


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

    def shade(self, stroke):
        if self.__target is None:
            return
        for it, t in iter_distance_from_object(stroke, self.__target, self.__range_min, self.__range_max):
            sv = it.object
            a = sv.attribute.thickness
            b = self.__value_min + self.evaluate(t) * (self.__value_max - self.__value_min)
            c = self.blend_thickness(a[0], a[1], b)
            self.set_thickness(sv, c[0], c[1])


# Material modifiers

def iter_material_color(stroke, material_attribute):
    func = CurveMaterialF0D()
    it = stroke.stroke_vertices_begin()
    while not it.is_end:
        material = func(Interface0DIterator(it))
        if material_attribute == 'DIFF':
            color = material.diffuse[0:3]
        elif material_attribute == 'SPEC':
            color = material.specular[0:3]
        else:
            raise ValueError("unexpected material attribute: " + material_attribute)
        yield it, color
        it.increment()


def iter_material_value(stroke, material_attribute):
    func = CurveMaterialF0D()
    it = stroke.stroke_vertices_begin()
    while not it.is_end:
        material = func(Interface0DIterator(it))
        if material_attribute == 'DIFF':
            r, g, b = material.diffuse[0:3]
            t = 0.35 * r + 0.45 * r + 0.2 * b
        elif material_attribute == 'DIFF_R':
            t = material.diffuse[0]
        elif material_attribute == 'DIFF_G':
            t = material.diffuse[1]
        elif material_attribute == 'DIFF_B':
            t = material.diffuse[2]
        elif material_attribute == 'SPEC':
            r, g, b = material.specular[0:3]
            t = 0.35 * r + 0.45 * r + 0.2 * b
        elif material_attribute == 'SPEC_R':
            t = material.specular[0]
        elif material_attribute == 'SPEC_G':
            t = material.specular[1]
        elif material_attribute == 'SPEC_B':
            t = material.specular[2]
        elif material_attribute == 'SPEC_HARDNESS':
            t = material.shininess
        elif material_attribute == 'ALPHA':
            t = material.diffuse[3]
        else:
            raise ValueError("unexpected material attribute: " + material_attribute)
        yield it, t
        it.increment()


class ColorMaterialShader(ColorRampModifier):
    def __init__(self, blend, influence, ramp, material_attribute, use_ramp):
        ColorRampModifier.__init__(self, blend, influence, ramp)
        self.__material_attribute = material_attribute
        self.__use_ramp = use_ramp

    def shade(self, stroke):
        if self.__material_attribute in {'DIFF', 'SPEC'} and not self.__use_ramp:
            for it, b in iter_material_color(stroke, self.__material_attribute):
                sv = it.object
                a = sv.attribute.color
                sv.attribute.color = self.blend_ramp(a, b)
        else:
            for it, t in iter_material_value(stroke, self.__material_attribute):
                sv = it.object
                a = sv.attribute.color
                b = self.evaluate(t)
                sv.attribute.color = self.blend_ramp(a, b)


class AlphaMaterialShader(CurveMappingModifier):
    def __init__(self, blend, influence, mapping, invert, curve, material_attribute):
        CurveMappingModifier.__init__(self, blend, influence, mapping, invert, curve)
        self.__material_attribute = material_attribute

    def shade(self, stroke):
        for it, t in iter_material_value(stroke, self.__material_attribute):
            sv = it.object
            a = sv.attribute.alpha
            b = self.evaluate(t)
            sv.attribute.alpha = self.blend(a, b)


class ThicknessMaterialShader(ThicknessBlenderMixIn, CurveMappingModifier):
    def __init__(self, thickness_position, thickness_ratio,
                 blend, influence, mapping, invert, curve, material_attribute, value_min, value_max):
        ThicknessBlenderMixIn.__init__(self, thickness_position, thickness_ratio)
        CurveMappingModifier.__init__(self, blend, influence, mapping, invert, curve)
        self.__material_attribute = material_attribute
        self.__value_min = value_min
        self.__value_max = value_max

    def shade(self, stroke):
        for it, t in iter_material_value(stroke, self.__material_attribute):
            sv = it.object
            a = sv.attribute.thickness
            b = self.__value_min + self.evaluate(t) * (self.__value_max - self.__value_min)
            c = self.blend_thickness(a[0], a[1], b)
            self.set_thickness(sv, c[0], c[1])


# Calligraphic thickness modifier

class CalligraphicThicknessShader(ThicknessBlenderMixIn, ScalarBlendModifier):
    def __init__(self, thickness_position, thickness_ratio,
                 blend, influence, orientation, thickness_min, thickness_max):
        ThicknessBlenderMixIn.__init__(self, thickness_position, thickness_ratio)
        ScalarBlendModifier.__init__(self, blend, influence)
        self.__orientation = mathutils.Vector((math.cos(orientation), math.sin(orientation)))
        self.__thickness_min = thickness_min
        self.__thickness_max = thickness_max

    def shade(self, stroke):
        func = VertexOrientation2DF0D()
        it = stroke.stroke_vertices_begin()
        while not it.is_end:
            dir = func(Interface0DIterator(it))
            orthDir = mathutils.Vector((-dir.y, dir.x))
            orthDir.normalize()
            fac = abs(orthDir * self.__orientation)
            sv = it.object
            a = sv.attribute.thickness
            b = self.__thickness_min + fac * (self.__thickness_max - self.__thickness_min)
            b = max(b, 0.0)
            c = self.blend_thickness(a[0], a[1], b)
            self.set_thickness(sv, c[0], c[1])
            it.increment()


# Geometry modifiers

def iter_distance_along_stroke(stroke):
    distance = 0.0
    it = stroke.stroke_vertices_begin()
    prev = it.object.point
    while not it.is_end:
        p = it.object.point
        distance += (prev - p).length
        prev = p.copy()  # need a copy because the point can be altered
        yield it, distance
        it.increment()


class SinusDisplacementShader(StrokeShader):
    def __init__(self, wavelength, amplitude, phase):
        StrokeShader.__init__(self)
        self._wavelength = wavelength
        self._amplitude = amplitude
        self._phase = phase / wavelength * 2 * math.pi
        self._getNormal = Normal2DF0D()

    def shade(self, stroke):
        for it, distance in iter_distance_along_stroke(stroke):
            v = it.object
            n = self._getNormal(Interface0DIterator(it))
            n = n * self._amplitude * math.cos(distance / self._wavelength * 2 * math.pi + self._phase)
            v.point = v.point + n
        stroke.update_length()


class PerlinNoise1DShader(StrokeShader):
    def __init__(self, freq=10, amp=10, oct=4, angle=math.radians(45), seed=-1):
        StrokeShader.__init__(self)
        self.__noise = Noise(seed)
        self.__freq = freq
        self.__amp = amp
        self.__oct = oct
        self.__dir = mathutils.Vector((math.cos(angle), math.sin(angle)))

    def shade(self, stroke):
        length = stroke.length_2d
        it = stroke.stroke_vertices_begin()
        while not it.is_end:
            v = it.object
            nres = self.__noise.turbulence1(length * v.u, self.__freq, self.__amp, self.__oct)
            v.point = v.point + nres * self.__dir
            it.increment()
        stroke.update_length()


class PerlinNoise2DShader(StrokeShader):
    def __init__(self, freq=10, amp=10, oct=4, angle=math.radians(45), seed=-1):
        StrokeShader.__init__(self)
        self.__noise = Noise(seed)
        self.__freq = freq
        self.__amp = amp
        self.__oct = oct
        self.__dir = mathutils.Vector((math.cos(angle), math.sin(angle)))

    def shade(self, stroke):
        it = stroke.stroke_vertices_begin()
        while not it.is_end:
            v = it.object
            vec = mathutils.Vector((v.projected_x, v.projected_y))
            nres = self.__noise.turbulence2(vec, self.__freq, self.__amp, self.__oct)
            v.point = v.point + nres * self.__dir
            it.increment()
        stroke.update_length()


class Offset2DShader(StrokeShader):
    def __init__(self, start, end, x, y):
        StrokeShader.__init__(self)
        self.__start = start
        self.__end = end
        self.__xy = mathutils.Vector((x, y))
        self.__getNormal = Normal2DF0D()

    def shade(self, stroke):
        it = stroke.stroke_vertices_begin()
        while not it.is_end:
            v = it.object
            u = v.u
            a = self.__start + u * (self.__end - self.__start)
            n = self.__getNormal(Interface0DIterator(it))
            n = n * a
            v.point = v.point + n + self.__xy
            it.increment()
        stroke.update_length()


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

    def shade(self, stroke):
        # determine the pivot of scaling and rotation operations
        if self.__pivot == 'START':
            it = stroke.stroke_vertices_begin()
            pivot = it.object.point
        elif self.__pivot == 'END':
            it = stroke.stroke_vertices_end()
            it.decrement()
            pivot = it.object.point
        elif self.__pivot == 'PARAM':
            p = None
            it = stroke.stroke_vertices_begin()
            while not it.is_end:
                prev = p
                v = it.object
                p = v.point
                u = v.u
                if self.__pivot_u < u:
                    break
                it.increment()
            if prev is None:
                pivot = p
            else:
                delta = u - self.__pivot_u
                pivot = p + delta * (prev - p)
        elif self.__pivot == 'CENTER':
            pivot = mathutils.Vector((0.0, 0.0))
            n = 0
            it = stroke.stroke_vertices_begin()
            while not it.is_end:
                p = it.object.point
                pivot = pivot + p
                n += 1
                it.increment()
            pivot.x = pivot.x / n
            pivot.y = pivot.y / n
        elif self.__pivot == 'ABSOLUTE':
            pivot = mathutils.Vector((self.__pivot_x, self.__pivot_y))
        # apply scaling and rotation operations
        cos_theta = math.cos(self.__angle)
        sin_theta = math.sin(self.__angle)
        it = stroke.stroke_vertices_begin()
        while not it.is_end:
            v = it.object
            p = v.point
            p = p - pivot
            x = p.x * self.__scale_x
            y = p.y * self.__scale_y
            p.x = x * cos_theta - y * sin_theta
            p.y = x * sin_theta + y * cos_theta
            v.point = p + pivot
            it.increment()
        stroke.update_length()


# Predicates and helper functions

class QuantitativeInvisibilityRangeUP1D(UnaryPredicate1D):
    def __init__(self, qi_start, qi_end):
        UnaryPredicate1D.__init__(self)
        self.__getQI = QuantitativeInvisibilityF1D()
        self.__qi_start = qi_start
        self.__qi_end = qi_end

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

    def __call__(self, viewEdge):
        found = viewEdge.viewshape.name in self._names
        if self._negative:
            return not found
        return found


# Stroke caps

def iter_stroke_vertices(stroke):
    it = stroke.stroke_vertices_begin()
    prev_p = None
    while not it.is_end:
        sv = it.object
        p = sv.point
        if prev_p is None or (prev_p - p).length > 1e-6:
            yield sv
            prev_p = p.copy()
        it.increment()


class RoundCapShader(StrokeShader):
    def round_cap_thickness(self, x):
        x = max(0.0, min(x, 1.0))
        return math.sqrt(1.0 - (x ** 2.0))

    def shade(self, stroke):
        # save the location and attribute of stroke vertices
        buffer = []
        for sv in iter_stroke_vertices(stroke):
            buffer.append((mathutils.Vector(sv.point), StrokeAttribute(sv.attribute)))
        nverts = len(buffer)
        if nverts < 2:
            return
        # calculate the number of additional vertices to form caps
        R, L = stroke[0].attribute.thickness
        caplen_beg = (R + L) / 2.0
        nverts_beg = max(5, int(R + L))
        R, L = stroke[-1].attribute.thickness
        caplen_end = (R + L) / 2.0
        nverts_end = max(5, int(R + L))
        # adjust the total number of stroke vertices
        stroke.resample(nverts + nverts_beg + nverts_end)
        # restore the location and attribute of the original vertices
        for i in range(nverts):
            p, attr = buffer[i]
            stroke[nverts_beg + i].point = p
            stroke[nverts_beg + i].attribute = attr
        # reshape the cap at the beginning of the stroke
        q, attr = buffer[1]
        p, attr = buffer[0]
        d = p - q
        d = d / d.length * caplen_beg
        n = 1.0 / nverts_beg
        R, L = attr.thickness
        for i in range(nverts_beg):
            t = (nverts_beg - i) * n
            stroke[i].point = p + d * t
            r = self.round_cap_thickness((nverts_beg - i + 1) * n)
            stroke[i].attribute = attr
            stroke[i].attribute.thickness = (R * r, L * r)
        # reshape the cap at the end of the stroke
        q, attr = buffer[-2]
        p, attr = buffer[-1]
        d = p - q
        d = d / d.length * caplen_end
        n = 1.0 / nverts_end
        R, L = attr.thickness
        for i in range(nverts_end):
            t = (nverts_end - i) * n
            stroke[-i - 1].point = p + d * t
            r = self.round_cap_thickness((nverts_end - i + 1) * n)
            stroke[-i - 1].attribute = attr
            stroke[-i - 1].attribute.thickness = (R * r, L * r)
        # update the curvilinear 2D length of each vertex
        stroke.update_length()


class SquareCapShader(StrokeShader):
    def shade(self, stroke):
        # save the location and attribute of stroke vertices
        buffer = []
        for sv in iter_stroke_vertices(stroke):
            buffer.append((mathutils.Vector(sv.point), StrokeAttribute(sv.attribute)))
        nverts = len(buffer)
        if nverts < 2:
            return
        # calculate the number of additional vertices to form caps
        R, L = stroke[0].attribute.thickness
        caplen_beg = (R + L) / 2.0
        nverts_beg = 1
        R, L = stroke[-1].attribute.thickness
        caplen_end = (R + L) / 2.0
        nverts_end = 1
        # adjust the total number of stroke vertices
        stroke.resample(nverts + nverts_beg + nverts_end)
        # restore the location and attribute of the original vertices
        for i in range(nverts):
            p, attr = buffer[i]
            stroke[nverts_beg + i].point = p
            stroke[nverts_beg + i].attribute = attr
        # reshape the cap at the beginning of the stroke
        q, attr = buffer[1]
        p, attr = buffer[0]
        d = p - q
        stroke[0].point = p + d / d.length * caplen_beg
        stroke[0].attribute = attr
        # reshape the cap at the end of the stroke
        q, attr = buffer[-2]
        p, attr = buffer[-1]
        d = p - q
        stroke[-1].point = p + d / d.length * caplen_beg
        stroke[-1].attribute = attr
        # update the curvilinear 2D length of each vertex
        stroke.update_length()


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
        self.start_pos = [pattern[i] + pattern[i + 1] for i in range(0, n, 2)]
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

    def shade(self, stroke):
        index = 0  # pattern index
        start = 0.0  # 2D curvilinear length
        visible = True
        sampling = 1.0
        it = stroke.stroke_vertices_begin(sampling)
        while not it.is_end:
            pos = it.t  # curvilinear abscissa
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
            it.object.attribute.visible = visible
            it.increment()


# predicates for chaining

class AngleLargerThanBP1D(BinaryPredicate1D):
    def __init__(self, angle):
        BinaryPredicate1D.__init__(self)
        self._angle = angle

    def __call__(self, i1, i2):
        sv1a = i1.first_fedge.first_svertex.point_2d
        sv1b = i1.last_fedge.second_svertex.point_2d
        sv2a = i2.first_fedge.first_svertex.point_2d
        sv2b = i2.last_fedge.second_svertex.point_2d
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

    def __call__(self, i1, i2):
        return self.__pred1(i1, i2) and self.__pred2(i1, i2)


# predicates for selection

class LengthThresholdUP1D(UnaryPredicate1D):
    def __init__(self, length_min=None, length_max=None):
        UnaryPredicate1D.__init__(self)
        self._length_min = length_min
        self._length_max = length_max

    def __call__(self, inter):
        length = inter.length_2d
        if self._length_min is not None and length < self._length_min:
            return False
        if self._length_max is not None and length > self._length_max:
            return False
        return True


class FaceMarkBothUP1D(UnaryPredicate1D):
    def __call__(self, inter):  # ViewEdge
        fe = inter.first_fedge
        while fe is not None:
            if fe.is_smooth:
                if fe.face_mark:
                    return True
            elif (fe.nature & Nature.BORDER):
                if fe.face_mark_left:
                    return True
            else:
                if fe.face_mark_right and fe.face_mark_left:
                    return True
            fe = fe.next_fedge
        return False


class FaceMarkOneUP1D(UnaryPredicate1D):
    def __call__(self, inter):  # ViewEdge
        fe = inter.first_fedge
        while fe is not None:
            if fe.is_smooth:
                if fe.face_mark:
                    return True
            elif (fe.nature & Nature.BORDER):
                if fe.face_mark_left:
                    return True
            else:
                if fe.face_mark_right or fe.face_mark_left:
                    return True
            fe = fe.next_fedge
        return False


# predicates for splitting

class MaterialBoundaryUP0D(UnaryPredicate0D):
    def __call__(self, it):
        if it.is_begin:
            return False
        it_prev = Interface0DIterator(it)
        it_prev.decrement()
        v = it.object
        it.increment()
        if it.is_end:
            return False
        fe = v.get_fedge(it_prev.object)
        idx1 = fe.material_index if fe.is_smooth else fe.material_index_left
        fe = v.get_fedge(it.object)
        idx2 = fe.material_index if fe.is_smooth else fe.material_index_left
        return idx1 != idx2


class Curvature2DAngleThresholdUP0D(UnaryPredicate0D):
    def __init__(self, angle_min=None, angle_max=None):
        UnaryPredicate0D.__init__(self)
        self._angle_min = angle_min
        self._angle_max = angle_max
        self._func = Curvature2DAngleF0D()

    def __call__(self, inter):
        angle = math.pi - self._func(inter)
        if self._angle_min is not None and angle < self._angle_min:
            return True
        if self._angle_max is not None and angle > self._angle_max:
            return True
        return False


class Length2DThresholdUP0D(UnaryPredicate0D):
    def __init__(self, length_limit):
        UnaryPredicate0D.__init__(self)
        self._length_limit = length_limit
        self._t = 0.0

    def __call__(self, inter):
        t = inter.t  # curvilinear abscissa
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


### T.K. 07-Aug-2013 Temporary fix for unexpected line gaps

def iter_three_segments(stroke):
    n = stroke.stroke_vertices_size()
    if n >= 4:
        it1 = stroke.stroke_vertices_begin()
        it2 = stroke.stroke_vertices_begin()
        it2.increment()
        it3 = stroke.stroke_vertices_begin()
        it3.increment()
        it3.increment()
        it4 = stroke.stroke_vertices_begin()
        it4.increment()
        it4.increment()
        it4.increment()
        while not it4.is_end:
            yield (it1.object, it2.object, it3.object, it4.object)
            it1.increment()
            it2.increment()
            it3.increment()
            it4.increment()


def is_tvertex(svertex):
    return type(svertex.viewvertex) is TVertex


class StrokeCleaner(StrokeShader):
    def shade(self, stroke):
        for sv1, sv2, sv3, sv4 in iter_three_segments(stroke):
            seg1 = sv2.point - sv1.point
            seg2 = sv3.point - sv2.point
            seg3 = sv4.point - sv3.point
            if not ((is_tvertex(sv2.first_svertex) and is_tvertex(sv2.second_svertex)) or
                    (is_tvertex(sv3.first_svertex) and is_tvertex(sv3.second_svertex))):
                continue
            if seg1.dot(seg2) < 0.0 and seg2.dot(seg3) < 0.0 and seg2.length < 0.01:
                #print(sv2.first_svertex.viewvertex)
                #print(sv2.second_svertex.viewvertex)
                #print(sv3.first_svertex.viewvertex)
                #print(sv3.second_svertex.viewvertex)
                p2 = mathutils.Vector(sv2.point)
                p3 = mathutils.Vector(sv3.point)
                sv2.point = p3
                sv3.point = p2
        stroke.update_length()


# main function for parameter processing

def process(layer_name, lineset_name):
    scene = getCurrentScene()
    layer = scene.render.layers[layer_name]
    lineset = layer.freestyle_settings.linesets[lineset_name]
    linestyle = lineset.linestyle

    selection_criteria = []
    # prepare selection criteria by visibility
    if lineset.select_by_visibility:
        if lineset.visibility == 'VISIBLE':
            selection_criteria.append(
                QuantitativeInvisibilityUP1D(0))
        elif lineset.visibility == 'HIDDEN':
            selection_criteria.append(
                NotUP1D(QuantitativeInvisibilityUP1D(0)))
        elif lineset.visibility == 'RANGE':
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
        if lineset.edge_type_combination == 'OR':
            upred = join_unary_predicates(edge_type_criteria, OrUP1D)
        else:
            upred = join_unary_predicates(edge_type_criteria, AndUP1D)
        if upred is not None:
            if lineset.edge_type_negation == 'EXCLUSIVE':
                upred = NotUP1D(upred)
            selection_criteria.append(upred)
    # prepare selection criteria by face marks
    if lineset.select_by_face_marks:
        if lineset.face_mark_condition == 'BOTH':
            upred = FaceMarkBothUP1D()
        else:
            upred = FaceMarkOneUP1D()
        if lineset.face_mark_negation == 'EXCLUSIVE':
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
        xmin, ymin, xmax, ymax = ContextFunctions.get_border()
        upred = WithinImageBoundaryUP1D(xmin, ymin, xmax, ymax)
        selection_criteria.append(upred)
    # select feature edges
    upred = join_unary_predicates(selection_criteria, AndUP1D)
    if upred is None:
        upred = TrueUP1D()
    Operators.select(upred)
    # join feature edges to form chains
    if linestyle.use_chaining:
        if linestyle.chaining == 'PLAIN':
            if linestyle.use_same_object:
                Operators.bidirectional_chain(ChainSilhouetteIterator(), NotUP1D(upred))
            else:
                Operators.bidirectional_chain(ChainPredicateIterator(upred, TrueBP1D()), NotUP1D(upred))
        elif linestyle.chaining == 'SKETCHY':
            if linestyle.use_same_object:
                Operators.bidirectional_chain(pySketchyChainSilhouetteIterator(linestyle.rounds))
            else:
                Operators.bidirectional_chain(pySketchyChainingIterator(linestyle.rounds))
    else:
        Operators.chain(ChainPredicateIterator(FalseUP1D(), FalseBP1D()), NotUP1D(upred))
    # split chains
    if linestyle.material_boundary:
        Operators.sequential_split(MaterialBoundaryUP0D())
    if linestyle.use_angle_min or linestyle.use_angle_max:
        angle_min = linestyle.angle_min if linestyle.use_angle_min else None
        angle_max = linestyle.angle_max if linestyle.use_angle_max else None
        Operators.sequential_split(Curvature2DAngleThresholdUP0D(angle_min, angle_max))
    if linestyle.use_split_length:
        Operators.sequential_split(Length2DThresholdUP0D(linestyle.split_length), 1.0)
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
            Operators.sequential_split(SplitPatternStartingUP0D(controller),
                                       SplitPatternStoppingUP0D(controller),
                                       sampling)
    # select chains
    if linestyle.use_length_min or linestyle.use_length_max:
        length_min = linestyle.length_min if linestyle.use_length_min else None
        length_max = linestyle.length_max if linestyle.use_length_max else None
        Operators.select(LengthThresholdUP1D(length_min, length_max))
    # prepare a list of stroke shaders
    shaders_list = []
    ###
    shaders_list.append(StrokeCleaner())
    ###
    for m in linestyle.geometry_modifiers:
        if not m.use:
            continue
        if m.type == 'SAMPLING':
            shaders_list.append(SamplingShader(
                m.sampling))
        elif m.type == 'BEZIER_CURVE':
            shaders_list.append(BezierCurveShader(
                m.error))
        elif m.type == 'SINUS_DISPLACEMENT':
            shaders_list.append(SinusDisplacementShader(
                m.wavelength, m.amplitude, m.phase))
        elif m.type == 'SPATIAL_NOISE':
            shaders_list.append(SpatialNoiseShader(
                m.amplitude, m.scale, m.octaves, m.smooth, m.use_pure_random))
        elif m.type == 'PERLIN_NOISE_1D':
            shaders_list.append(PerlinNoise1DShader(
                m.frequency, m.amplitude, m.octaves, m.angle, _seed.get(m.seed)))
        elif m.type == 'PERLIN_NOISE_2D':
            shaders_list.append(PerlinNoise2DShader(
                m.frequency, m.amplitude, m.octaves, m.angle, _seed.get(m.seed)))
        elif m.type == 'BACKBONE_STRETCHER':
            shaders_list.append(BackboneStretcherShader(
                m.backbone_length))
        elif m.type == 'TIP_REMOVER':
            shaders_list.append(TipRemoverShader(
                m.tip_length))
        elif m.type == 'POLYGONIZATION':
            shaders_list.append(PolygonalizationShader(
                m.error))
        elif m.type == 'GUIDING_LINES':
            shaders_list.append(GuidingLinesShader(
                m.offset))
        elif m.type == 'BLUEPRINT':
            if m.shape == 'CIRCLES':
                shaders_list.append(pyBluePrintCirclesShader(
                    m.rounds, m.random_radius, m.random_center))
            elif m.shape == 'ELLIPSES':
                shaders_list.append(pyBluePrintEllipsesShader(
                    m.rounds, m.random_radius, m.random_center))
            elif m.shape == 'SQUARES':
                shaders_list.append(pyBluePrintSquaresShader(
                    m.rounds, m.backbone_length, m.random_backbone))
        elif m.type == '2D_OFFSET':
            shaders_list.append(Offset2DShader(
                m.start, m.end, m.x, m.y))
        elif m.type == '2D_TRANSFORM':
            shaders_list.append(Transform2DShader(
                m.pivot, m.scale_x, m.scale_y, m.angle, m.pivot_u, m.pivot_x, m.pivot_y))
    color = linestyle.color
    if (not linestyle.use_chaining) or (linestyle.chaining == 'PLAIN' and linestyle.use_same_object):
        thickness_position = linestyle.thickness_position
    else:
        thickness_position = 'CENTER'
        import bpy
        if bpy.app.debug_freestyle:
            print("Warning: Thickness position options are applied when chaining is disabled\n"
                  "         or the Plain chaining is used with the Same Object option enabled.")
    shaders_list.append(BaseColorShader(color.r, color.g, color.b, linestyle.alpha))
    shaders_list.append(BaseThicknessShader(linestyle.thickness, thickness_position,
                                            linestyle.thickness_ratio))
    for m in linestyle.color_modifiers:
        if not m.use:
            continue
        if m.type == 'ALONG_STROKE':
            shaders_list.append(ColorAlongStrokeShader(
                m.blend, m.influence, m.color_ramp))
        elif m.type == 'DISTANCE_FROM_CAMERA':
            shaders_list.append(ColorDistanceFromCameraShader(
                m.blend, m.influence, m.color_ramp,
                m.range_min, m.range_max))
        elif m.type == 'DISTANCE_FROM_OBJECT':
            shaders_list.append(ColorDistanceFromObjectShader(
                m.blend, m.influence, m.color_ramp, m.target,
                m.range_min, m.range_max))
        elif m.type == 'MATERIAL':
            shaders_list.append(ColorMaterialShader(
                m.blend, m.influence, m.color_ramp, m.material_attribute,
                m.use_ramp))
    for m in linestyle.alpha_modifiers:
        if not m.use:
            continue
        if m.type == 'ALONG_STROKE':
            shaders_list.append(AlphaAlongStrokeShader(
                m.blend, m.influence, m.mapping, m.invert, m.curve))
        elif m.type == 'DISTANCE_FROM_CAMERA':
            shaders_list.append(AlphaDistanceFromCameraShader(
                m.blend, m.influence, m.mapping, m.invert, m.curve,
                m.range_min, m.range_max))
        elif m.type == 'DISTANCE_FROM_OBJECT':
            shaders_list.append(AlphaDistanceFromObjectShader(
                m.blend, m.influence, m.mapping, m.invert, m.curve, m.target,
                m.range_min, m.range_max))
        elif m.type == 'MATERIAL':
            shaders_list.append(AlphaMaterialShader(
                m.blend, m.influence, m.mapping, m.invert, m.curve,
                m.material_attribute))
    for m in linestyle.thickness_modifiers:
        if not m.use:
            continue
        if m.type == 'ALONG_STROKE':
            shaders_list.append(ThicknessAlongStrokeShader(
                thickness_position, linestyle.thickness_ratio,
                m.blend, m.influence, m.mapping, m.invert, m.curve,
                m.value_min, m.value_max))
        elif m.type == 'DISTANCE_FROM_CAMERA':
            shaders_list.append(ThicknessDistanceFromCameraShader(
                thickness_position, linestyle.thickness_ratio,
                m.blend, m.influence, m.mapping, m.invert, m.curve,
                m.range_min, m.range_max, m.value_min, m.value_max))
        elif m.type == 'DISTANCE_FROM_OBJECT':
            shaders_list.append(ThicknessDistanceFromObjectShader(
                thickness_position, linestyle.thickness_ratio,
                m.blend, m.influence, m.mapping, m.invert, m.curve, m.target,
                m.range_min, m.range_max, m.value_min, m.value_max))
        elif m.type == 'MATERIAL':
            shaders_list.append(ThicknessMaterialShader(
                thickness_position, linestyle.thickness_ratio,
                m.blend, m.influence, m.mapping, m.invert, m.curve,
                m.material_attribute, m.value_min, m.value_max))
        elif m.type == 'CALLIGRAPHY':
            shaders_list.append(CalligraphicThicknessShader(
                thickness_position, linestyle.thickness_ratio,
                m.blend, m.influence,
                m.orientation, m.thickness_min, m.thickness_max))
    if linestyle.caps == 'ROUND':
        shaders_list.append(RoundCapShader())
    elif linestyle.caps == 'SQUARE':
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
