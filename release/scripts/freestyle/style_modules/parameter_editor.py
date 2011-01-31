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

class CurveMappingModifier(StrokeShader):
    def __init__(self, blend, influence, mapping, invert, curve):
        StrokeShader.__init__(self)
        self.__blend = blend
        self.__influence = influence
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
    def blend_curve(self, v1, v2):
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
            c = self.blend_curve(a, b)
            attr.setAlpha(c)

class ThicknessAlongStrokeShader(CurveMappingModifier):
    def __init__(self, blend, influence, mapping, invert, curve, value_min, value_max):
        CurveMappingModifier.__init__(self, blend, influence, mapping, invert, curve)
        self.__value_min = value_min
        self.__value_max = value_max
    def getName(self):
        return "ThicknessAlongStrokeShader"
    def shade(self, stroke):
        for it, t in iter_t2d_along_stroke(stroke):
            attr = it.getObject().attribute()
            a = attr.getThicknessRL()
            a = a[0] + a[1]
            b = self.__value_min + self.evaluate(t) * (self.__value_max - self.__value_min)
            c = self.blend_curve(a, b)
            attr.setThickness(c/2, c/2)

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
            c = self.blend_curve(a, b)
            attr.setAlpha(c)

class ThicknessDistanceFromCameraShader(CurveMappingModifier):
    def __init__(self, blend, influence, mapping, invert, curve, range_min, range_max, value_min, value_max):
        CurveMappingModifier.__init__(self, blend, influence, mapping, invert, curve)
        self.__range_min = range_min
        self.__range_max = range_max
        self.__value_min = value_min
        self.__value_max = value_max
    def getName(self):
        return "ThicknessDistanceFromCameraShader"
    def shade(self, stroke):
        for it, t in iter_distance_from_camera(stroke, self.__range_min, self.__range_max):
            attr = it.getObject().attribute()
            a = attr.getThicknessRL()
            a = a[0] + a[1]
            b = self.__value_min + self.evaluate(t) * (self.__value_max - self.__value_min)
            c = self.blend_curve(a, b)
            attr.setThickness(c/2, c/2)

# Distance from Object modifiers

def iter_distance_from_object(stroke, object, range_min, range_max):
    scene = Freestyle.getCurrentScene()
    mv = scene.camera.matrix_world.copy().invert() # model-view matrix
    loc = object.location * mv # loc in the camera coordinate
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
            c = self.blend_curve(a, b)
            attr.setAlpha(c)

class ThicknessDistanceFromObjectShader(CurveMappingModifier):
    def __init__(self, blend, influence, mapping, invert, curve, target, range_min, range_max, value_min, value_max):
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
            attr = it.getObject().attribute()
            a = attr.getThicknessRL()
            a = a[0] + a[1]
            b = self.__value_min + self.evaluate(t) * (self.__value_max - self.__value_min)
            c = self.blend_curve(a, b)
            attr.setThickness(c/2, c/2)

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
        return TrueUP1D()
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

class WithinImageBorderUP1D(UnaryPredicate1D):
    def __init__(self, xmin, xmax, ymin, ymax):
        UnaryPredicate1D.__init__(self)
        self._xmin = xmin
        self._xmax = xmax
        self._ymin = ymin
        self._ymax = ymax
    def getName(self):
        return "WithinImageBorderUP1D"
    def __call__(self, inter):
        return self.withinBorder(inter.A()) or self.withinBorder(inter.B())
    def withinBorder(self, vert):
        x = vert.getProjectedX()
        y = vert.getProjectedY()
        return self._xmin <= x <= self._xmax and self._ymin <= y <= self._ymax

# Stroke caps

def iter_stroke_vertices(stroke):
    it = stroke.strokeVerticesBegin()
    while not it.isEnd():
        yield it.getObject()
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
        # calculate the number of additional vertices to form caps
        R, L = stroke[0].attribute().getThicknessRL()
        caplen_beg = (R + L) / 2.0
        nverts_beg = max(5, int(R + L))
        R, L = stroke[-1].attribute().getThicknessRL()
        caplen_end = (R + L) / 2.0
        nverts_end = max(5, int(R + L))
        # increase the total number of stroke vertices
        nverts = stroke.strokeVerticesSize()
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

class SquareCapShader(StrokeShader):
    def shade(self, stroke):
        # save the location and attribute of stroke vertices
        buffer = []
        for sv in iter_stroke_vertices(stroke):
            buffer.append((sv.getPoint(), sv.attribute()))
        # calculate the number of additional vertices to form caps
        R, L = stroke[0].attribute().getThicknessRL()
        caplen_beg = (R + L) / 2.0
        nverts_beg = 1
        R, L = stroke[-1].attribute().getThicknessRL()
        caplen_end = (R + L) / 2.0
        nverts_end = 1
        # increase the total number of stroke vertices
        nverts = stroke.strokeVerticesSize()
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

# dashed line

class DashedLineStartingUP0D(UnaryPredicate0D):
    def __init__(self, controller):
        UnaryPredicate0D.__init__(self)
        self._controller = controller
    def __call__(self, inter):
        return self._controller.start()

class DashedLineStoppingUP0D(UnaryPredicate0D):
    def __init__(self, controller):
        UnaryPredicate0D.__init__(self)
        self._controller = controller
    def __call__(self, inter):
        return self._controller.stop()

class DashedLineController:
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
        if lineset.edge_type_combination == "OR":
            flags = Nature.NO_FEATURE
            if lineset.select_silhouette:
                flags |= Nature.SILHOUETTE
            if lineset.select_border:
                flags |= Nature.BORDER
            if lineset.select_crease:
                flags |= Nature.CREASE
            if lineset.select_ridge:
                flags |= Nature.RIDGE
            if lineset.select_valley:
                flags |= Nature.VALLEY
            if lineset.select_suggestive_contour:
                flags |= Nature.SUGGESTIVE_CONTOUR
            if lineset.select_material_boundary:
                flags |= Nature.MATERIAL_BOUNDARY
            if flags != Nature.NO_FEATURE:
                edge_type_criteria.append(pyNatureUP1D(flags))
        else:
            if lineset.select_silhouette:
                edge_type_criteria.append(pyNatureUP1D(Nature.SILHOUETTE))
            if lineset.select_border:
                edge_type_criteria.append(pyNatureUP1D(Nature.BORDER))
            if lineset.select_crease:
                edge_type_criteria.append(pyNatureUP1D(Nature.CREASE))
            if lineset.select_ridge:
                edge_type_criteria.append(pyNatureUP1D(Nature.RIDGE))
            if lineset.select_valley:
                edge_type_criteria.append(pyNatureUP1D(Nature.VALLEY))
            if lineset.select_suggestive_contour:
                edge_type_criteria.append(pyNatureUP1D(Nature.SUGGESTIVE_CONTOUR))
            if lineset.select_material_boundary:
                edge_type_criteria.append(pyNatureUP1D(Nature.MATERIAL_BOUNDARY))
        if lineset.select_contour:
            edge_type_criteria.append(ContourUP1D())
        if lineset.select_external_contour:
            edge_type_criteria.append(ExternalContourUP1D())
        if lineset.edge_type_combination == "OR":
            upred = join_unary_predicates(edge_type_criteria, OrUP1D)
        else:
            upred = join_unary_predicates(edge_type_criteria, AndUP1D)
        if upred is not None:
            if lineset.edge_type_negation == "EXCLUSIVE":
                upred = NotUP1D(upred)
            selection_criteria.append(upred)
    # prepare selection criteria by group of objects
    if lineset.select_by_group:
        if lineset.group is not None and len(lineset.group.objects) > 0:
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
        upred = WithinImageBorderUP1D(xmin, xmax, ymin, ymax)
        selection_criteria.append(upred)
    # do feature edge selection
    upred = join_unary_predicates(selection_criteria, AndUP1D)
    if upred is None:
        upred = TrueUP1D()
    Operators.select(upred)
    # join feature edges
    bpred = AngleLargerThanBP1D(1.0) # XXX temporary fix for occasional unexpected long lines
    if linestyle.same_object:
        bpred = AndBP1D(bpred, SameShapeIdBP1D())
    Operators.bidirectionalChain(ChainPredicateIterator(upred, bpred), NotUP1D(upred))
    # dashed line
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
            sampling = 1.0
            controller = DashedLineController(pattern, sampling)
            Operators.sequentialSplit(DashedLineStartingUP0D(controller),
                                      DashedLineStoppingUP0D(controller),
                                      sampling)
    # prepare a list of stroke shaders
    color = linestyle.color
    shaders_list = [
        SamplingShader(5.0),
        ConstantThicknessShader(linestyle.thickness),
        ConstantColorShader(color.r, color.g, color.b, linestyle.alpha)]
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
    for m in linestyle.thickness_modifiers:
        if not m.use:
            continue
        if m.type == "ALONG_STROKE":
            shaders_list.append(ThicknessAlongStrokeShader(
                m.blend, m.influence, m.mapping, m.invert, m.curve,
                m.value_min, m.value_max))
        elif m.type == "DISTANCE_FROM_CAMERA":
            shaders_list.append(ThicknessDistanceFromCameraShader(
                m.blend, m.influence, m.mapping, m.invert, m.curve,
                m.range_min, m.range_max, m.value_min, m.value_max))
        elif m.type == "DISTANCE_FROM_OBJECT":
            shaders_list.append(ThicknessDistanceFromObjectShader(
                m.blend, m.influence, m.mapping, m.invert, m.curve, m.target,
                m.range_min, m.range_max, m.value_min, m.value_max))
    if linestyle.caps == "ROUND":
        shaders_list.append(RoundCapShader())
    elif linestyle.caps == "SQUARE":
        shaders_list.append(SquareCapShader())
    # create strokes using the shaders list
    Operators.create(TrueUP1D(), shaders_list)
