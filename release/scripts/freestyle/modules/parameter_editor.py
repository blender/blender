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
    IntegrationType,
    Interface0DIterator,
    Nature,
    Noise,
    Operators,
    StrokeAttribute,
    UnaryPredicate0D,
    UnaryPredicate1D,
    TVertex,
    Material,
    ViewEdge,
    )
from freestyle.chainingiterators import (
    ChainPredicateIterator,
    ChainSilhouetteIterator,
    pySketchyChainSilhouetteIterator,
    pySketchyChainingIterator,
    )
from freestyle.functions import (
    Curvature2DAngleF0D,
    Normal2DF0D,
    QuantitativeInvisibilityF1D,
    VertexOrientation2DF0D,
    CurveMaterialF0D,
    )
from freestyle.predicates import (
    AndUP1D,
    ContourUP1D,
    ExternalContourUP1D,
    FalseBP1D,
    FalseUP1D,
    Length2DBP1D,
    NotBP1D,
    NotUP1D,
    OrUP1D,
    QuantitativeInvisibilityUP1D,
    SameShapeIdBP1D,
    TrueBP1D,
    TrueUP1D,
    WithinImageBoundaryUP1D,
    pyNFirstUP1D,
    pyNatureUP1D,
    pyProjectedXBP1D,
    pyProjectedYBP1D,
    pyZBP1D,
    )
from freestyle.shaders import (
    BackboneStretcherShader,
    BezierCurveShader,
    BlenderTextureShader,
    ConstantColorShader,
    GuidingLinesShader,
    PolygonalizationShader,
    SamplingShader,
    SpatialNoiseShader,
    StrokeShader,
    StrokeTextureStepShader,
    TipRemoverShader,
    pyBluePrintCirclesShader,
    pyBluePrintEllipsesShader,
    pyBluePrintSquaresShader,
    RoundCapShader,
    SquareCapShader,
    )
from freestyle.utils import (
    ContextFunctions,
    getCurrentScene,
    iter_distance_along_stroke,
    iter_t2d_along_stroke,
    iter_distance_from_camera,
    iter_distance_from_object,
    iter_material_value,
    stroke_normal,
    bound,
    pairwise,
    BoundedProperty,
    get_dashed_pattern,
    )
from _freestyle import (
    blendRamp,
    evaluateColorRamp,
    evaluateCurveMappingF,
    )

from export_svg import (
    SVGPathShader,
    SVGFillShader,
    ShapeZ,
    )

import time

from mathutils import Vector
from math import pi, sin, cos, acos, radians
from itertools import cycle, tee
from bpy.path import abspath
from os.path import isfile


class ColorRampModifier(StrokeShader):
    """Primitive for the color modifiers."""
    def __init__(self, blend, influence, ramp):
        StrokeShader.__init__(self)
        self.blend = blend
        self.influence = influence
        self.ramp = ramp

    def evaluate(self, t):
        col = evaluateColorRamp(self.ramp, t)
        return col.xyz  # omit alpha

    def blend_ramp(self, a, b):
        return blendRamp(self.blend, a, self.influence, b)


class ScalarBlendModifier(StrokeShader):
    """Primitive for alpha and thickness modifiers."""
    def __init__(self, blend_type, influence):
        StrokeShader.__init__(self)
        self.blend_type = blend_type
        self.influence = influence

    def blend(self, v1, v2):
        fac = self.influence
        facm = 1.0 - fac
        if self.blend_type == 'MIX':
            v1 = facm * v1 + fac * v2
        elif self.blend_type == 'ADD':
            v1 += fac * v2
        elif self.blend_type == 'MULTIPLY':
            v1 *= facm + fac * v2
        elif self.blend_type == 'SUBTRACT':
            v1 -= fac * v2
        elif self.blend_type == 'DIVIDE':
            v1 = facm * v1 + fac * v1 / v2 if v2 != 0.0 else v1
        elif self.blend_type == 'DIFFERENCE':
            v1 = facm * v1 + fac * abs(v1 - v2)
        elif self.blend_type == 'MININUM':
            v1 = min(fac * v2, v1)
        elif self.blend_type == 'MAXIMUM':
            v1 = max(fac * v2, v1)
        else:
            raise ValueError("unknown curve blend type: " + self.blend_type)
        return v1


class CurveMappingModifier(ScalarBlendModifier):
    def __init__(self, blend, influence, mapping, invert, curve):
        ScalarBlendModifier.__init__(self, blend, influence)
        assert mapping in {'LINEAR', 'CURVE'}
        self.evaluate = getattr(self, mapping)
        self.invert = invert
        self.curve = curve

    def LINEAR(self, t):
        return (1.0 - t) if self.invert else t

    def CURVE(self, t):
        return evaluateCurveMappingF(self.curve, 0, t)


class ThicknessModifierMixIn:
    def __init__(self):
        scene = getCurrentScene()
        self.persp_camera = (scene.camera.data.type == 'PERSP')

    def set_thickness(self, sv, outer, inner):
        fe = sv.fedge
        nature = fe.nature
        if (nature & Nature.BORDER):
            if self.persp_camera:
                point = -sv.point_3d.normalized()
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
        self.position = position
        self.ratio = ratio

    def blend_thickness(self, svert, v):
        """Blends and sets the thickness."""
        outer, inner = svert.attribute.thickness
        fe = svert.fedge
        v = self.blend(outer + inner, v)

        # Part 1: blend
        if self.position == 'CENTER':
            outer = inner = v * 0.5
        elif self.position == 'INSIDE':
            outer, inner = 0, v
        elif self.position == 'OUTSIDE':
            outer, inner = v, 0
        elif self.position == 'RELATIVE':
            outer, inner = v * self.ratio, v - (v * self.ratio)
        else:
            raise ValueError("unknown thickness position: " + position)

        # Part 2: set
        if (fe.nature & Nature.BORDER):
            if self.persp_camera:
                point = -svert.point_3d.normalized()
                dir = point.dot(fe.normal_left)
            else:
                dir = fe.normal_left.z
            if dir < 0.0:  # the back side is visible
                outer, inner = inner, outer
        elif (fe.nature & Nature.SILHOUETTE):
            if fe.is_smooth:  # TODO more tests needed
                outer, inner = inner, outer
        else:
            outer = inner = (outer + inner) / 2
        svert.attribute.thickness = (outer, inner)


class BaseThicknessShader(StrokeShader, ThicknessModifierMixIn):
    def __init__(self, thickness, position, ratio):
        StrokeShader.__init__(self)
        ThicknessModifierMixIn.__init__(self)
        if position == 'CENTER':
            self.outer = thickness * 0.5
            self.inner = thickness - self.outer
        elif position == 'INSIDE':
            self.outer = 0
            self.inner = thickness
        elif position == 'OUTSIDE':
            self.outer = thickness
            self.inner = 0
        elif position == 'RELATIVE':
            self.outer = thickness * ratio
            self.inner = thickness - self.outer
        else:
            raise ValueError("unknown thickness position: " + position)

    def shade(self, stroke):
        for svert in stroke:
            self.set_thickness(svert, self.outer, self.inner)


# Along Stroke modifiers

class ColorAlongStrokeShader(ColorRampModifier):
    """Maps a ramp to the color of the stroke, using the curvilinear abscissa (t)."""
    def shade(self, stroke):
        for svert, t in zip(stroke, iter_t2d_along_stroke(stroke)):
            a = svert.attribute.color
            b = self.evaluate(t)
            svert.attribute.color = self.blend_ramp(a, b)


class AlphaAlongStrokeShader(CurveMappingModifier):
    """Maps a curve to the alpha/transparancy of the stroke, using the curvilinear abscissa (t)."""
    def shade(self, stroke):
        for svert, t in zip(stroke, iter_t2d_along_stroke(stroke)):
            a = svert.attribute.alpha
            b = self.evaluate(t)
            svert.attribute.alpha = self.blend(a, b)


class ThicknessAlongStrokeShader(ThicknessBlenderMixIn, CurveMappingModifier):
    """Maps a curve to the thickness of the stroke, using the curvilinear abscissa (t)."""
    def __init__(self, thickness_position, thickness_ratio,
                 blend, influence, mapping, invert, curve, value_min, value_max):
        ThicknessBlenderMixIn.__init__(self, thickness_position, thickness_ratio)
        CurveMappingModifier.__init__(self, blend, influence, mapping, invert, curve)
        self.value = BoundedProperty(value_min, value_max, value_max - value_min)

    def shade(self, stroke):
        for svert, t in zip(stroke, iter_t2d_along_stroke(stroke)):
            b = self.value.min + self.evaluate(t) * self.value.delta
            self.blend_thickness(svert, b)


# -- Distance from Camera modifiers -- #

class ColorDistanceFromCameraShader(ColorRampModifier):
    """Picks a color value from a ramp based on the vertex' distance from the camera."""
    def __init__(self, blend, influence, ramp, range_min, range_max):
        ColorRampModifier.__init__(self, blend, influence, ramp)
        self.range = BoundedProperty(range_min, range_max, range_max - range_min)

    def shade(self, stroke):
        it = iter_distance_from_camera(stroke, *self.range)
        for svert, t in it:
            a = svert.attribute.color
            b = self.evaluate(t)
            svert.attribute.color = self.blend_ramp(a, b)


class AlphaDistanceFromCameraShader(CurveMappingModifier):
    """Picks an alpha value from a curve based on the vertex' distance from the camera"""
    def __init__(self, blend, influence, mapping, invert, curve, range_min, range_max):
        CurveMappingModifier.__init__(self, blend, influence, mapping, invert, curve)
        self.range = BoundedProperty(range_min, range_max, range_max - range_min)

    def shade(self, stroke):
        it = iter_distance_from_camera(stroke, *self.range)
        for svert, t in it:
            a = svert.attribute.alpha
            b = self.evaluate(t)
            svert.attribute.alpha = self.blend(a, b)


class ThicknessDistanceFromCameraShader(ThicknessBlenderMixIn, CurveMappingModifier):
    """Picks a thickness value from a curve based on the vertex' distance from the camera."""
    def __init__(self, thickness_position, thickness_ratio,
                 blend, influence, mapping, invert, curve, range_min, range_max, value_min, value_max):
        ThicknessBlenderMixIn.__init__(self, thickness_position, thickness_ratio)
        CurveMappingModifier.__init__(self, blend, influence, mapping, invert, curve)
        self.range = BoundedProperty(range_min, range_max, range_max - range_min)
        self.value = BoundedProperty(value_min, value_max, value_max - value_min)

    def shade(self, stroke):
        for (svert, t) in iter_distance_from_camera(stroke, *self.range):
            b = self.value.min + self.evaluate(t) * self.value.delta
            self.blend_thickness(svert, b)


# Distance from Object modifiers

class ColorDistanceFromObjectShader(ColorRampModifier):
    """Picks a color value from a ramp based on the vertex' distance from a given object."""
    def __init__(self, blend, influence, ramp, target, range_min, range_max):
        ColorRampModifier.__init__(self, blend, influence, ramp)
        if target is None:
            raise ValueError("ColorDistanceFromObjectShader: target can't be None ")
        self.range = BoundedProperty(range_min, range_max, range_max - range_min)
        # construct a model-view matrix
        matrix = getCurrentScene().camera.matrix_world.inverted()
        # get the object location in the camera coordinate
        self.loc = matrix * target.location

    def shade(self, stroke):
        it = iter_distance_from_object(stroke, self.loc, *self.range)
        for svert, t in it:
            a = svert.attribute.color
            b = self.evaluate(t)
            svert.attribute.color = self.blend_ramp(a, b)


class AlphaDistanceFromObjectShader(CurveMappingModifier):
    """Picks an alpha value from a curve based on the vertex' distance from a given object."""
    def __init__(self, blend, influence, mapping, invert, curve, target, range_min, range_max):
        CurveMappingModifier.__init__(self, blend, influence, mapping, invert, curve)
        if target is None:
            raise ValueError("AlphaDistanceFromObjectShader: target can't be None ")
        self.range = BoundedProperty(range_min, range_max, range_max - range_min)
        # construct a model-view matrix
        matrix = getCurrentScene().camera.matrix_world.inverted()
        # get the object location in the camera coordinate
        self.loc = matrix * target.location

    def shade(self, stroke):
        it = iter_distance_from_object(stroke, self.loc, *self.range)
        for svert, t in it:
            a = svert.attribute.alpha
            b = self.evaluate(t)
            svert.attribute.alpha = self.blend(a, b)


class ThicknessDistanceFromObjectShader(ThicknessBlenderMixIn, CurveMappingModifier):
    """Picks a thickness value from a curve based on the vertex' distance from a given object."""
    def __init__(self, thickness_position, thickness_ratio,
                 blend, influence, mapping, invert, curve, target, range_min, range_max, value_min, value_max):
        ThicknessBlenderMixIn.__init__(self, thickness_position, thickness_ratio)
        CurveMappingModifier.__init__(self, blend, influence, mapping, invert, curve)
        if target is None:
            raise ValueError("ThicknessDistanceFromObjectShader: target can't be None ")
        self.range = BoundedProperty(range_min, range_max, range_max - range_min)
        self.value = BoundedProperty(value_min, value_max, value_max - value_min)
        # construct a model-view matrix
        matrix = getCurrentScene().camera.matrix_world.inverted()
        # get the object location in the camera coordinate
        self.loc = matrix * target.location

    def shade(self, stroke):
        it = iter_distance_from_object(stroke, self.loc, *self.range)
        for svert, t in it:
            b = self.value.min + self.evaluate(t) * self.value.delta
            self.blend_thickness(svert, b)

# Material modifiers
class ColorMaterialShader(ColorRampModifier):
    """Assigns a color to the vertices based on their underlying material."""
    def __init__(self, blend, influence, ramp, material_attribute, use_ramp):
        ColorRampModifier.__init__(self, blend, influence, ramp)
        self.attribute = material_attribute
        self.use_ramp = use_ramp
        self.func = CurveMaterialF0D()

    def shade(self, stroke, attributes={'DIFF', 'SPEC', 'LINE'}):
        it = Interface0DIterator(stroke)
        if not self.use_ramp and self.attribute in attributes:
            for svert in it:
                material = self.func(it)
                if self.attribute == 'LINE':
                    b = material.line[0:3]
                elif self.attribute == 'DIFF':
                    b = material.diffuse[0:3]
                else:
                    b = material.specular[0:3]
                a = svert.attribute.color
                svert.attribute.color = self.blend_ramp(a, b)
        else:
            for svert, value in iter_material_value(stroke, self.func, self.attribute):
                a = svert.attribute.color
                b = self.evaluate(value)
                svert.attribute.color = self.blend_ramp(a, b)

class AlphaMaterialShader(CurveMappingModifier):
    """Assigns an alpha value to the vertices based on their underlying material."""
    def __init__(self, blend, influence, mapping, invert, curve, material_attribute):
        CurveMappingModifier.__init__(self, blend, influence, mapping, invert, curve)
        self.attribute = material_attribute
        self.func = CurveMaterialF0D()

    def shade(self, stroke):
        for svert, value in iter_material_value(stroke, self.func, self.attribute):
            a = svert.attribute.alpha
            b = self.evaluate(value)
            svert.attribute.alpha = self.blend(a, b)


class ThicknessMaterialShader(ThicknessBlenderMixIn, CurveMappingModifier):
    """Assigns a thickness value to the vertices based on their underlying material."""
    def __init__(self, thickness_position, thickness_ratio,
                 blend, influence, mapping, invert, curve, material_attribute, value_min, value_max):
        ThicknessBlenderMixIn.__init__(self, thickness_position, thickness_ratio)
        CurveMappingModifier.__init__(self, blend, influence, mapping, invert, curve)
        self.attribute = material_attribute
        self.value = BoundedProperty(value_min, value_max, value_max - value_min)
        self.func = CurveMaterialF0D()

    def shade(self, stroke):
        for svert, value in iter_material_value(stroke, self.func, self.attribute):
            b = self.value.min + self.evaluate(value) * self.value.delta
            self.blend_thickness(svert, b)


# Calligraphic thickness modifier


class CalligraphicThicknessShader(ThicknessBlenderMixIn, ScalarBlendModifier):
    """Thickness modifier for achieving a calligraphy-like effect."""
    def __init__(self, thickness_position, thickness_ratio,
                 blend_type, influence, orientation, thickness_min, thickness_max):
        ThicknessBlenderMixIn.__init__(self, thickness_position, thickness_ratio)
        ScalarBlendModifier.__init__(self, blend_type, influence)
        self.orientation = Vector((cos(orientation), sin(orientation)))
        self.thickness = BoundedProperty(thickness_min, thickness_max, thickness_max - thickness_min)
        self.func = VertexOrientation2DF0D()

    def shade(self, stroke):
        it = Interface0DIterator(stroke)
        for svert in it:
            dir = self.func(it)
            if dir.length != 0.0:
                dir.normalize()
                fac = abs(dir.orthogonal() * self.orientation)
                b = self.thickness.min + fac * self.thickness.delta
            else:
                b = self.thickness.min
            self.blend_thickness(svert, b)


# Geometry modifiers

class SinusDisplacementShader(StrokeShader):
    """Displaces the stroke in a sinewave-like shape."""
    def __init__(self, wavelength, amplitude, phase):
        StrokeShader.__init__(self)
        self.wavelength = wavelength
        self.amplitude = amplitude
        self.phase = phase / wavelength * 2 * pi

    def shade(self, stroke):
        # normals are stored in a tuple, so they don't update when we reposition vertices.
        normals = tuple(stroke_normal(stroke))
        distances = iter_distance_along_stroke(stroke)
        coeff = 1 / self.wavelength * 2 * pi
        for svert, distance, normal in zip(stroke, distances, normals):
            n = normal * self.amplitude * cos(distance * coeff + self.phase)
            svert.point += n
        stroke.update_length()


class PerlinNoise1DShader(StrokeShader):
    """
    Displaces the stroke using the curvilinear abscissa.  This means
    that lines with the same length and sampling interval will be
    identically distorded.
    """
    def __init__(self, freq=10, amp=10, oct=4, angle=radians(45), seed=-1):
        StrokeShader.__init__(self)
        self.noise = Noise(seed)
        self.freq = freq
        self.amp = amp
        self.oct = oct
        self.dir = Vector((cos(angle), sin(angle)))

    def shade(self, stroke):
        length = stroke.length_2d
        for svert in stroke:
            nres = self.noise.turbulence1(length * svert.u, self.freq, self.amp, self.oct)
            svert.point += nres * self.dir
        stroke.update_length()


class PerlinNoise2DShader(StrokeShader):
    """
    Displaces the stroke using the strokes coordinates.  This means
    that in a scene no strokes will be distorded identically.

    More information on the noise shaders can be found at:
    freestyleintegration.wordpress.com/2011/09/25/development-updates-on-september-25/
    """
    def __init__(self, freq=10, amp=10, oct=4, angle=radians(45), seed=-1):
        StrokeShader.__init__(self)
        self.noise = Noise(seed)
        self.freq = freq
        self.amp = amp
        self.oct = oct
        self.dir = Vector((cos(angle), sin(angle)))

    def shade(self, stroke):
        for svert in stroke:
            projected = Vector((svert.projected_x, svert.projected_y))
            nres = self.noise.turbulence2(projected, self.freq, self.amp, self.oct)
            svert.point += nres * self.dir
        stroke.update_length()


class Offset2DShader(StrokeShader):
    """Offsets the stroke by a given amount."""
    def __init__(self, start, end, x, y):
        StrokeShader.__init__(self)
        self.start = start
        self.end = end
        self.xy = Vector((x, y))

    def shade(self, stroke):
        # normals are stored in a tuple, so they don't update when we reposition vertices.
        normals = tuple(stroke_normal(stroke))
        for svert, normal in zip(stroke, normals):
            a = self.start + svert.u * (self.end - self.start)
            svert.point += (normal * a) + self.xy
        stroke.update_length()


class Transform2DShader(StrokeShader):
    """Transforms the stroke (scale, rotation, location) around a given pivot point """
    def __init__(self, pivot, scale_x, scale_y, angle, pivot_u, pivot_x, pivot_y):
        StrokeShader.__init__(self)
        self.pivot = pivot
        self.scale = Vector((scale_x, scale_y))
        self.cos_theta = cos(angle)
        self.sin_theta = sin(angle)
        self.pivot_u = pivot_u
        self.pivot_x = pivot_x
        self.pivot_y = pivot_y
        if pivot not in {'START', 'END', 'CENTER', 'ABSOLUTE', 'PARAM'}:
            raise ValueError("expected pivot in {'START', 'END', 'CENTER', 'ABSOLUTE', 'PARAM'}, not" + pivot)

    def shade(self, stroke):
        # determine the pivot of scaling and rotation operations
        if self.pivot == 'START':
            pivot = stroke[0].point
        elif self.pivot == 'END':
            pivot = stroke[-1].point
        elif self.pivot == 'CENTER':
            # minor rounding errors here, because
            # given v = Vector(a, b), then (v / n) != Vector(v.x / n, v.y / n)
            pivot = (1 / len(stroke)) * sum((svert.point for svert in stroke), Vector((0.0, 0.0)))
        elif self.pivot == 'ABSOLUTE':
            pivot = Vector((self.pivot_x, self.pivot_y))
        elif self.pivot == 'PARAM':
            if self.pivot_u < stroke[0].u:
                pivot = stroke[0].point
            else:
                for prev, svert in pairwise(stroke):
                    if self.pivot_u < svert.u:
                        break
                pivot = svert.point + (svert.u - self.pivot_u) * (prev.point - svert.point)

        # apply scaling and rotation operations
        for svert in stroke:
            p = (svert.point - pivot)
            x = p.x * self.scale.x
            y = p.y * self.scale.y
            p.x = x * self.cos_theta - y * self.sin_theta
            p.y = x * self.sin_theta + y * self.cos_theta
            svert.point = p + pivot
        stroke.update_length()


# Predicates and helper functions

class QuantitativeInvisibilityRangeUP1D(UnaryPredicate1D):
    def __init__(self, qi_start, qi_end):
        UnaryPredicate1D.__init__(self)
        self.getQI = QuantitativeInvisibilityF1D()
        self.qi_start = qi_start
        self.qi_end = qi_end

    def __call__(self, inter):
        qi = self.getQI(inter)
        return self.qi_start <= qi <= self.qi_end


class ObjectNamesUP1D(UnaryPredicate1D):
    def __init__(self, names, negative):
        UnaryPredicate1D.__init__(self)
        self.names = names
        self.negative = negative

    def __call__(self, viewEdge):
        found = viewEdge.viewshape.name in self.names
        if self.negative:
            return not found
        return found


# -- Split by dashed line pattern -- #

class SplitPatternStartingUP0D(UnaryPredicate0D):
    def __init__(self, controller):
        UnaryPredicate0D.__init__(self)
        self.controller = controller

    def __call__(self, inter):
        return self.controller.start()


class SplitPatternStoppingUP0D(UnaryPredicate0D):
    def __init__(self, controller):
        UnaryPredicate0D.__init__(self)
        self.controller = controller

    def __call__(self, inter):
        return self.controller.stop()


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
        self.pattern = pattern

    def shade(self, stroke):
        start = 0.0  # 2D curvilinear length
        visible = True
        # The extra 'sampling' term is added below, because the
        # visibility attribute of the i-th vertex refers to the
        # visibility of the stroke segment between the i-th and
        # (i+1)-th vertices.
        sampling = 1.0
        it = stroke.stroke_vertices_begin(sampling)
        pattern_cycle = cycle(self.pattern)
        pattern = next(pattern_cycle)
        for svert in it:
            pos = it.t  # curvilinear abscissa

            if pos - start + sampling > pattern:
                start = pos
                pattern = next(pattern_cycle)
                visible = not visible

            if not visible:
                it.object.attribute.visible = False


# predicates for chaining

class AngleLargerThanBP1D(BinaryPredicate1D):
    def __init__(self, angle):
        BinaryPredicate1D.__init__(self)
        self.angle = angle

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
        return acos(bound(-1.0, x, 1.0)) > self.angle

# predicates for selection


class LengthThresholdUP1D(UnaryPredicate1D):
    def __init__(self, length_min=None, length_max=None):
        UnaryPredicate1D.__init__(self)
        self.length_min = length_min
        self.length_max = length_max

    def __call__(self, inter):
        length = inter.length_2d
        if self.length_min is not None and length < self.length_min:
            return False
        if self.length_max is not None and length > self.length_max:
            return False
        return True


class FaceMarkBothUP1D(UnaryPredicate1D):
    def __call__(self, inter: ViewEdge):
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
    def __call__(self, inter: ViewEdge):
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
        # can't use only it.is_end here, see commit rBeb8964fb7f19
        if it.is_begin or it.at_last or it.is_end:
            return False
        it.decrement()
        prev, v, succ = next(it), next(it), next(it)
        fe = v.get_fedge(prev)
        idx1 = fe.material_index if fe.is_smooth else fe.material_index_left
        fe = v.get_fedge(succ)
        idx2 = fe.material_index if fe.is_smooth else fe.material_index_left
        return idx1 != idx2


class Curvature2DAngleThresholdUP0D(UnaryPredicate0D):
    def __init__(self, angle_min=None, angle_max=None):
        UnaryPredicate0D.__init__(self)
        self.angle_min = angle_min
        self.angle_max = angle_max
        self.func = Curvature2DAngleF0D()

    def __call__(self, inter):
        angle = pi - self.func(inter)
        if self.angle_min is not None and angle < self.angle_min:
            return True
        if self.angle_max is not None and angle > self.angle_max:
            return True
        return False


class Length2DThresholdUP0D(UnaryPredicate0D):
    def __init__(self, length_limit):
        UnaryPredicate0D.__init__(self)
        self.length_limit = length_limit
        self.t = 0.0

    def __call__(self, inter):
        t = inter.t  # curvilinear abscissa
        if t < self.t:
            self.t = 0.0
            return False
        if t - self.t < self.length_limit:
            return False
        self.t = t
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


integration_types = {
    'MEAN': IntegrationType.MEAN,
    'MIN': IntegrationType.MIN,
    'MAX': IntegrationType.MAX,
    'FIRST': IntegrationType.FIRST,
    'LAST': IntegrationType.LAST}


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
            upred = OrUP1D(*edge_type_criteria)
        else:
            upred = AndUP1D(*edge_type_criteria)
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
            names = {ob.name: True for ob in lineset.group.objects}
            upred = ObjectNamesUP1D(names, lineset.group_negation == 'EXCLUSIVE')
            selection_criteria.append(upred)
    # prepare selection criteria by image border
    if lineset.select_by_image_border:
        upred = WithinImageBoundaryUP1D(*ContextFunctions.get_border())
        selection_criteria.append(upred)
    # select feature edges
    upred = AndUP1D(*selection_criteria)
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
    # sort selected chains
    if linestyle.use_sorting:
        integration = integration_types.get(linestyle.integration_type, IntegrationType.MEAN)
        if linestyle.sort_key == 'DISTANCE_FROM_CAMERA':
            bpred = pyZBP1D(integration)
        elif linestyle.sort_key == '2D_LENGTH':
            bpred = Length2DBP1D()
        elif linestyle.sort_key == 'PROJECTED_X':
            bpred = pyProjectedXBP1D(integration)
        elif linestyle.sort_key == 'PROJECTED_Y':
            bpred = pyProjectedYBP1D(integration)
        if linestyle.sort_order == 'REVERSE':
            bpred = NotBP1D(bpred)
        Operators.sort(bpred)
    # select chains
    if linestyle.use_length_min or linestyle.use_length_max:
        length_min = linestyle.length_min if linestyle.use_length_min else None
        length_max = linestyle.length_max if linestyle.use_length_max else None
        Operators.select(LengthThresholdUP1D(length_min, length_max))
    if linestyle.use_chain_count:
        Operators.select(pyNFirstUP1D(linestyle.chain_count))
    # prepare a list of stroke shaders
    shaders_list = []
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
    # -- Base color, alpha and thickness -- #
    if (not linestyle.use_chaining) or (linestyle.chaining == 'PLAIN' and linestyle.use_same_object):
        thickness_position = linestyle.thickness_position
    else:
        thickness_position = 'CENTER'
        import bpy
        if bpy.app.debug_freestyle:
            print("Warning: Thickness position options are applied when chaining is disabled\n"
                  "         or the Plain chaining is used with the Same Object option enabled.")
    shaders_list.append(ConstantColorShader(*(linestyle.color), alpha=linestyle.alpha))
    shaders_list.append(BaseThicknessShader(linestyle.thickness, thickness_position,
                                            linestyle.thickness_ratio))
    # -- Modifiers -- #
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
        elif m.type == 'DISTANCE_FROM_OBJECT' and m.target is not None:
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
        elif m.type == 'DISTANCE_FROM_OBJECT' and m.target is not None:
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
        elif m.type == 'DISTANCE_FROM_OBJECT' and m.target is not None:
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
    # -- Textures -- #
    has_tex = False
    if scene.render.use_shading_nodes:
        if linestyle.use_nodes and linestyle.node_tree:
            shaders_list.append(BlenderTextureShader(linestyle.node_tree))
            has_tex = True
    else:
        if linestyle.use_texture:
            textures = tuple(BlenderTextureShader(slot) for slot in linestyle.texture_slots if slot is not None)
            if textures:
                shaders_list.extend(textures)
                has_tex = True
    if has_tex:
        shaders_list.append(StrokeTextureStepShader(linestyle.texture_spacing))
    # -- Dashed line -- #
    if linestyle.use_dashed_line:
        pattern = get_dashed_pattern(linestyle)
        if len(pattern) > 0:
            shaders_list.append(DashedLineShader(pattern))
    # -- SVG export -- #
    render = scene.render
    filepath = abspath(render.svg_path)
    # if the export path is invalid: log to console, but continue normal rendering
    if render.use_svg_export:
        if not isfile(filepath):
            print("Error: SVG export: path is invalid")
        else:
            height = render.resolution_y * render.resolution_percentage / 100
            split_at_inv = render.svg_split_at_invisible
            frame_current = scene.frame_current
            # SVGPathShader: keep reference and add to shader list
            renderer = SVGPathShader.from_lineset(lineset, filepath, height, split_at_inv, frame_current)
            shaders_list.append(renderer)

    # -- Stroke caps -- #
    # appended after svg shader to ensure correct svg output
    if linestyle.caps == 'ROUND':
        shaders_list.append(RoundCapShader())
    elif linestyle.caps == 'SQUARE':
        shaders_list.append(SquareCapShader())

    # create strokes using the shaders list
    Operators.create(TrueUP1D(), shaders_list)

    if render.use_svg_export and isfile(filepath):
        # write svg output to file
        renderer.write()
        if render.svg_use_object_fill:
            # reset the stroke selection (but don't delete the already generated ones)
            Operators.reset(delete_strokes=False)
            # shape detection
            upred = AndUP1D(QuantitativeInvisibilityUP1D(0), ContourUP1D())
            Operators.select(upred)
            # chain when the same shape and visible
            bpred = SameShapeIdBP1D()
            Operators.bidirectional_chain(ChainPredicateIterator(upred, bpred), NotUP1D(QuantitativeInvisibilityUP1D(0)))
            # sort according to the distance from camera
            Operators.sort(ShapeZ(scene))
            # render and write fills
            renderer = SVGFillShader(filepath, height, lineset.name)
            Operators.create(TrueUP1D(), [renderer,])
            renderer.write()
