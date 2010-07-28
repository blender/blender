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

from freestyle_init import *
from logical_operators import *
from ChainingIterators import *
from shaders import *

def blend_curve(blend_type, v1, fac, v2):
    facm = 1.0 - fac
    if blend_type == "MIX":
        v1 = facm * v1 + fac * v2
    elif blend_type == "ADD":
        v1 += fac * v2
    elif blend_type == "MULTIPLY":
        v1 *= facm + fac * v2;
    elif blend_type == "SUBTRACT":
        v1 -= fac * v2
    elif blend_type == "DIVIDE":
        if v2 != 0.0:
            v1 = facm * v1 + fac * v1 / v2
    elif blend_type == "DIFFERENCE":
        v1 = facm * v1 + fac * abs(v1 - v2)
    elif blend_type == "MININUM":
        tmp = fac * v1
        if v1 > tmp:
            v1 = tmp
    elif blend_type == "MAXIMUM":
        tmp = fac * v1
        if v1 < tmp:
            v1 = tmp
    else:
        raise ValueError("unknown curve blend type: " + blend_type)
    return v1

class ColorAlongStrokeShader(StrokeShader):
    def __init__(self, blend, influence, ramp):
        StrokeShader.__init__(self)
        self.__blend = blend
        self.__influence = influence
        self.__ramp = ramp
    def getName(self):
        return "ColorAlongStrokeShader"
    def shade(self, stroke):
        distance, total = 0.0, stroke.getLength2D()
        it = stroke.strokeVerticesBegin()
        while not it.isEnd():
            sv = it.getObject()
            p = sv.getPoint()
            if not it.isBegin():
                distance += (prev - p).length
            prev = p
            t = min(distance / total, 1.0)
            b = Freestyle.evaluateColorRamp(self.__ramp, t)
            b = b.xyz # omit alpha
            a = sv.attribute().getColorRGB()
            c = Freestyle.blendRamp(self.__blend, a, self.__influence, b)
            sv.attribute().setColor(c)
            it.increment()

class AlphaAlongStrokeShader(StrokeShader):
    def __init__(self, blend, influence, mapping, invert, curve):
        StrokeShader.__init__(self)
        self.__blend = blend
        self.__influence = influence
        assert mapping in ("LINEAR", "CURVE")
        self.__mapping = getattr(self, mapping)
        self.__invert = invert
        self.__curve = curve
    def getName(self):
        return "AlphaAlongStrokeShader"
    def LINEAR(self, t):
        if self.__invert:
            return 1.0 - t
        return t
    def CURVE(self, t):
        return Freestyle.evaluateCurveMappingF(self.__curve, 0, t)
    def shade(self, stroke):
        distance, total = 0.0, stroke.getLength2D()
        it = stroke.strokeVerticesBegin()
        while not it.isEnd():
            sv = it.getObject()
            p = sv.getPoint()
            if not it.isBegin():
                distance += (prev - p).length
            prev = p
            t = min(distance / total, 1.0)
            b = self.__mapping(t)
            a = sv.attribute().getAlpha()
            c = blend_curve(self.__blend, a, self.__influence, b)
            sv.attribute().setAlpha(c)
            it.increment()

class ThicknessAlongStrokeShader(StrokeShader):
    def __init__(self, blend, influence, mapping, invert, curve, value_min, value_max):
        StrokeShader.__init__(self)
        self.__blend = blend
        self.__influence = influence
        assert mapping in ("LINEAR", "CURVE")
        self.__mapping = getattr(self, mapping)
        self.__invert = invert
        self.__curve = curve
        self.__value_min = value_min
        self.__value_max = value_max
    def getName(self):
        return "ThicknessAlongStrokeShader"
    def LINEAR(self, t):
        if self.__invert:
            return 1.0 - t
        return t
    def CURVE(self, t):
        return Freestyle.evaluateCurveMappingF(self.__curve, 0, t)
    def shade(self, stroke):
        distance, total = 0.0, stroke.getLength2D()
        it = stroke.strokeVerticesBegin()
        while not it.isEnd():
            sv = it.getObject()
            p = sv.getPoint()
            if not it.isBegin():
                distance += (prev - p).length
            prev = p
            t = min(distance / total, 1.0)
            t = self.__mapping(t)
            b = self.__value_min + t * (self.__value_max - self.__value_min)
            a = sv.attribute().getThicknessRL()
            a = a[0] + a[1]
            c = blend_curve(self.__blend, a, self.__influence, b)
            sv.attribute().setThickness(c/2, c/2)
            it.increment()

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
    # do feature edge selection
    upred = join_unary_predicates(selection_criteria, AndUP1D)
    if upred is None:
        upred = TrueUP1D()
    Operators.select(upred)
    # join feature edges
    Operators.bidirectionalChain(ChainSilhouetteIterator(), NotUP1D(upred)) # FIXME
    # prepare a list of stroke shaders
    color = linestyle.color
    shaders_list = [
        SamplingShader(5.0),
        ConstantThicknessShader(linestyle.thickness),
        ConstantColorShader(color.r, color.g, color.b, linestyle.alpha)]
    for m in linestyle.color_modifiers:
        if not m.enabled:
            continue
        if m.type == "ALONG_STROKE":
            shaders_list.append(
                ColorAlongStrokeShader(m.blend, m.influence, m.color_ramp))
    for m in linestyle.alpha_modifiers:
        if not m.enabled:
            continue
        if m.type == "ALONG_STROKE":
            shaders_list.append(
                AlphaAlongStrokeShader(m.blend, m.influence, m.mapping, m.invert, m.curve))
    for m in linestyle.thickness_modifiers:
        if not m.enabled:
            continue
        if m.type == "ALONG_STROKE":
            shaders_list.append(
                ThicknessAlongStrokeShader(m.blend, m.influence, m.mapping, m.invert, m.curve,
                                           m.value_min, m.value_max))
    # create strokes using the shaders list
    Operators.create(TrueUP1D(), shaders_list)
