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

# This node is based on blender's built-in TorusKnot+ add-on

import bpy
from bpy.props import IntProperty, FloatProperty, BoolProperty, EnumProperty

from math import sin, cos, pi, sqrt, exp, atan, log
import re

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import updateNode, match_long_repeat
from sverchok.utils.sv_easing_functions import *

PHI = (sqrt(5) + 1) / 2  # the golden ratio
PHIPI = 2 * log(PHI) / pi  # exponent for the Fibonacci (golden) spiral

spiralTypeItems = [
    ("ARCHIMEDEAN", "Archimedean", "Generate an archimedean spiral.", 0),
    ("LOGARITHMIC", "Logarithmic", "Generate a logarithmic spiral.", 1),
    ("SPHERICAL", "Spherical", "Generate a spherical spiral.", 2),
    ("OVOIDAL", "Ovoidal", "Generate an ovoidal spiral.", 3),
    ("CORNU", "Cornu", "Generate a cornu spiral.", 4),
    ("EXO", "Exo", "Generate an exo spiral.", 5),
    ("SPIRANGLE", "Spirangle", "Generate a spirangle spiral.", 6)
]

# name : [ type, eR, iR, exponent, turns, resolution, scale, height ]
spiralPresets = {
    " ":            ["", 0.0, 0.0, 0.0, 0, 0, 0.0, 0.0],
    "FIBONACCI":    ["LOGARITHMIC", 1.0, 0.5, PHIPI, 3, 100, 1.0, 0.0],
    "HELIX":        ["LOGARITHMIC", 1.0, 0.0, 0.0, 7, 100, 1.0, 4.0],
    "ARCHIMEDEAN":  ["ARCHIMEDEAN", 1.0, 0.0, 1.0, 7, 100, 1.0, 0.0],
    "CONICAL":      ["ARCHIMEDEAN", 1.0, 0.0, 1.0, 7, 100, 1.0, 3.0],
    "PARABOLIC":    ["ARCHIMEDEAN", 1.0, 0.0, 2.0, 5, 100, 1.0, 0.0],
    "HYPERBOLIC":   ["ARCHIMEDEAN", 1.0, 0.0, -1.0, 11, 100, 1.0, 0.0],
    "LITUUS":       ["ARCHIMEDEAN", 1.0, 0.0, -2.0, 11, 100, 1.0, 0.0],
    "SPHERICAL":    ["SPHERICAL", 1.0, 0.0, 0.0, 11, 55, 1.0, 0.0],
    "OVOIDAL":      ["OVOIDAL", 5.0, 1.0, 0.0, 7, 55, 1.0, 6.0],
    "CORNU":        ["CORNU", 1.0, 1.0, 1.0, 5, 55, 1.0, 0.0],
    "EXO":          ["EXO", 1.0, 0.1, PHI, 11, 101, 1.0, 0.0],
    "SPIRANGLE SC": ["SPIRANGLE", 1.0, 0.0, 0.0, 8, 4, 1.0, 0.0],
    "SPIRANGLE HX": ["SPIRANGLE", 1.0, 0.0, 0.5, 7, 6, 1.0, 0.0]
}

normalizeItems = [
    ("ER", "eR", "Normalize the external radius.", 0),
    ("IR", "iR", "Normalize the internal radius.", 1)
]


def make_archimedean_spiral(flags, settings):
    '''
        Make an ARCHIMEDEAN spiral

        eR       : exterior radius (end radius)
        iR       : interior radius (start radius)
        exponent : rate of growth (between iR and eR)
        turns    : number of turns in the spiral
        N        : curve resolution per turn
        scale    : overall scale of the curve
        height   : the height of the spiral along z
        phase    : phase the spiral around its center
        flip     : flip the spiral direction (default is CLOCKWISE)
    '''
    cn, nn, ct, nt = flags  # compute/normalize normal/tangent (UNUSED)

    eR, iR, exponent, turns, N, scale, height, phase, flip = settings

    sign = -1 if flip else 1  # flip direction ?

    maxPhi = 2 * pi * turns * sign

    epsilon = 1e-5 if exponent < 0 else 0  # to avoid raising zero to negative power
    exponent = 1e-2 if exponent == 0 else exponent  # to avoid division by zero
    dR = eR - iR  # radius range : cached for performance
    ex = 1 / exponent  # inverse exponent : cached for performance

    N = N * turns  # total number of points in the spiral

    verts = []
    norms = []
    addVert = verts.append
    addNorm = norms.append
    for n in range(N + 1):
        t = n / N  # t : [0, 1]
        phi = maxPhi * t + phase
        r = (iR + dR * (t + epsilon) ** ex) * scale  # essentially: r = a * t ^ (1/b)
        x = r * cos(phi)
        y = r * sin(phi)
        z = height * t
        addVert([x, y, z])

    edges = [[i, i + 1] for i in range(len(verts) - 1)]

    return verts, edges, norms


def make_logarithmic_spiral(flags, settings):
    '''
        Make a LOGARITHMIC spiral

        eR       : exterior radius
        iR       : interior radius
        exponent : rate of growth
        turns    : number of turns in the spiral
        N        : curve resolution per turn
        scale    : overall scale of the curve
        height   : the height of the spiral along z
        phase    : phase the spiral around its center
        flip     : flip the spiral direction (default is CLOCKWISE)
    '''
    cn, nn, ct, nt = flags  # compute/normalize normal/tangent (UNUSED)

    eR, iR, exponent, turns, N, scale, height, phase, flip = settings

    sign = -1 if flip else 1  # flip direction ?

    maxPhi = 2 * pi * turns

    N = N * turns  # total number of points in the spiral

    verts = []
    norms = []
    addVert = verts.append
    addNorm = norms.append
    for n in range(N + 1):
        t = n / N  # t : [0, 1]
        phi = maxPhi * t
        r = eR * exp(exponent * phi) * scale  # essentially: r = a * e ^ (b*t)
        pho = phi * sign + phase  # final angle : cached for performance
        x = r * sin(pho)
        y = r * cos(pho)
        z = height * t
        addVert([x, y, z])

    edges = [[i, i + 1] for i in range(len(verts) - 1)]

    return verts, edges, norms


def make_spherical_spiral(flags, settings):
    '''
        Make a SPHERICAL spiral

        This is the approximate sperical spiral that has a finite length,
        where the phi & theta angles sweep their ranges at constant rates.

        eR       : exterior radius
        iR       : interior radius (UNUSED)
        exponent : rate of growth (sigmoid in & out)
        turns    : number of turns in the spiral
        N        : the curve resolution of one turn
        scale    : overall scale of the curve
        height   : the height of the spiral along z (UNUSED)
        phase    : phase the spiral around its center
        flip     : flip the spiral direction (default is CLOCKWISE)
    '''
    cn, nn, ct, nt = flags  # compute/normalize normal/tangent (UNUSED)

    eR, iR, exponent, turns, N, scale, height, phase, flip = settings

    sign = -1 if flip else 1  # flip direction ?

    maxPhi = 2 * pi * turns * sign

    N = N * turns  # total number of points in the spiral

    es = prepareExponentialSettings(2, exponent + 1e-5)  # used for easing

    verts = []
    norms = []
    addVert = verts.append
    addNorm = norms.append
    for n in range(N + 1):
        t = n / N  # t : [0, 1]
        phi = maxPhi * t + phase
        a = ExponentialEaseInOut(t, es)  # ease theta variation
        theta = -pi / 2 + pi * a
        RxCosTheta = (iR + eR * cos(theta)) * scale  # cached for performance
        x = cos(phi) * RxCosTheta
        y = sin(phi) * RxCosTheta
        z = eR * sin(theta)
        addVert([x, y, z])

    edges = [[i, i + 1] for i in range(len(verts) - 1)]

    return verts, edges, norms


def make_ovoidal_spiral(flags, settings):
    '''
        Make a OVOIDAL spiral

        eR       : exterior radius (vertical cross section circles)
        iR       : interior radius (horizontal cross section circle)
        exponent : rate of growth (sigmoid in & out)
        turns    : number of turns in the spiral
        N        : the curve resolution of one turn
        scale    : overall scale of the curve
        height   : the height of the spiral along z
        phase    : phase the spiral around its center
        flip     : flip the spiral direction (default is CLOCKWISE)
    '''
    cn, nn, ct, nt = flags  # compute/normalize normal/tangent (UNUSED)

    eR, iR, exponent, turns, N, scale, height, phase, flip = settings

    sign = -1 if flip else 1  # flip direction ?

    maxPhi = 2 * pi * turns * sign

    # derive eR based on iR and height (the main parameters)
    # eR = [iR - (H/2)^2/iR]/2 ::: H = 2 * sqrt(2*iR*eR - iR*iR)
    eR = 0.5 * (iR + 0.25 * height * height / iR)
    eR2 = eR * eR  # cached for performance
    dR = eR - iR # cached for performance

    N = N * turns  # total number of points in the spiral

    es = prepareExponentialSettings(2, exponent + 1e-5)  # used for easing

    verts = []
    norms = []
    addVert = verts.append
    addNorm = norms.append
    for n in range(N + 1):
        t = n / N  # t : [0, 1]
        phi = maxPhi * t + phase
        a = ExponentialEaseInOut(t, es)  # ease theta variation
        theta = -pi / 2 + pi * a
        h = 0.5 * height * sin(theta)  # [-H/2, +H/2]
        r = sqrt(eR2 - h * h) - dR  # [0 -> iR -> 0]
        x = r * cos(phi) * scale
        y = r * sin(phi) * scale
        z = h * scale
        addVert([x, y, z])

    edges = [[i, i + 1] for i in range(len(verts) - 1)]

    return verts, edges, norms


def make_cornu_spiral(flags, settings):
    '''
        Make a CORNU spiral

        L     : length
        N     : resolution
        S     : scale
        M     :

        x(t) = s * Integral(0,t) { cos(pi*u*u/2) du }
        y(t) = s * Integral(0,t) { sin(pi*u*u/2) du }

        TODO : refine the math (smoother curve, adaptive res, faster computation)
    '''
    cn, nn, ct, nt = flags  # compute/normalize normal/tangent (UNUSED)

    eR, iR, exponent, turns, N, scale, height, phase, flip = settings

    sign = -1 if flip else 1  # flip direction ?

    N = N * turns  # total number of points in the spiral
    L = iR * turns  # length
    S = eR * scale  # overall scale

    es = prepareExponentialSettings(2, exponent + 1e-5)  # used for easing

    verts1 = []  # pozitive spiral verts
    verts2 = []  # nagative spiral verts
    norms = []
    addVert1 = verts1.append
    addVert2 = verts2.append
    addNorm = norms.append
    l1 = 0
    x = 0
    y = 0
    for n in range(N + 1):
        t = n / N  # t = [0,1]

        a = QuadraticEaseOut(t)
        # a = ExponentialEaseOut(t, es)

        l = L * a  # l = [0, +L]

        r = x * x + y * y
        # print("r=", r)
        # M = 100 + int(300 * pow(r, exponent)) # integral steps
        M = 100 + int(100 * a)  # integral steps
        l2 = l

        # integral from l1 to l2
        u = l1
        du = (l2 - l1) / M
        for m in range(M + 1):
            u = u + du  # u = [l1, l2]
            phi = u * u * pi / 2
            x = x + cos(phi) * du
            y = y + sin(phi) * du
        l1 = l2

        # scale and flip
        xx = x * S
        yy = y * S * sign

        # rotate by phase amount
        px = xx * cos(phase) - yy * sin(phase)
        py = xx * sin(phase) + yy * cos(phase)
        pz = height * t

        addVert1([px, py, pz])  # positive spiral verts
        addVert2([-px, -py, -pz])  # netative spiral verts

    verts = verts2[::-1] + verts1

    edges = [[i, i + 1] for i in range(len(verts) - 1)]

    return verts, edges, norms


def make_exo_spiral(flags, settings):
    '''
        Make an EXO spiral

        This is an exponential in & out between two circles

        eR       : exterior radius
        iR       : interior radius
        exponent : rate of growth (SIGMOID : exponential in & out)
        turns    : number of turns in the spiral
        N        : the curve resolution of one turn
        scale    : overall scale of the curve
        height   : the height of the spiral along z
        phase    : phase the spiral around its center
        flip     : flip the spiral direction (default is CLOCKWISE)
    '''
    cn, nn, ct, nt = flags  # compute/normalize normal/tangent (UNUSED)

    eR, iR, exponent, turns, N, scale, height, phase, flip = settings

    sign = 1 if flip else -1  # flip direction ?

    maxPhi = 2 * pi * turns * sign

    N = N * turns  # total number of points in the spiral

    es = prepareExponentialSettings(11, exponent + 1e-5)  # used for easing

    verts = []
    norms = []
    addVert = verts.append
    addNorm = norms.append
    for n in range(N + 1):
        t = n / N  # t : [0, 1]
        a = ExponentialEaseInOut(t, es)  # ease radius variation (SIGMOID)
        r = (iR + (eR - iR) * a) * scale
        phi = maxPhi * t + phase
        x = r * cos(phi)
        y = r * sin(phi)
        z = height * t
        addVert([x, y, z])

    edges = [[i, i + 1] for i in range(len(verts) - 1)]

    return verts, edges, norms


def make_spirangle_spiral(flags, settings):
    '''
        Make a SPIRANGLE spiral

        eR       : exterior radius (end radius)
        iR       : interior radius (start radius)
        exponent : rate of growth
        turns    : number of turns in the spiral
        N        : curve resolution per turn
        scale    : overall scale of the curve
        height   : the height of the spiral along z
        phase    : phase the spiral around its center
        flip     : flip the spiral direction (default is CLOCKWISE)
    '''
    cn, nn, ct, nt = flags  # compute/normalize normal/tangent (UNUSED)

    eR, iR, exponent, turns, N, scale, height, phase, flip = settings

    sign = -1 if flip else 1  # flip direction ?

    deltaA = 2 * pi / N * sign  # angle increment
    deltaE = exponent / N  # exponent increment
    deltaR = (eR + iR)  # radius increment
    deltaZ = height / (N * turns)  # z increment
    e = 0
    r = iR
    phi = phase
    x, y, z = [0, 0, -deltaZ]

    N = N * turns  # total number of points in the spiral

    verts = []
    norms = []
    addVert = verts.append
    addNorm = norms.append
    for n in range(N + 1):
        x = x + r * cos(phi) * scale
        y = y + r * sin(phi) * scale
        z = z + deltaZ
        e = e + deltaE
        r = r + deltaR * exp(e)
        phi = phi + deltaA
        addVert([x, y, z])

    edges = [[i, i + 1] for i in range(len(verts) - 1)]

    return verts, edges, norms


def normalize_spiral(verts, normalize_eR, eR, iR, scale):
    '''
        Normalize the spiral (XY) to either exterior or interior radius
    '''
    if normalize_eR:  # normalize exterior radius (ending radius)
        psx = verts[-1][0]
        psy = verts[-1][1]
        r = sqrt(psx * psx + psy * psy)
        ss = eR / r * scale if eR != 0 else 1
    else:  # normalize interior radius (starting radius)
        psx = verts[0][0]
        psy = verts[0][1]
        r = sqrt(psx * psx + psy * psy)
        ss = iR / r * scale if iR != 0 else 1

    for n in range(len(verts)):
        verts[n][0] *= ss
        verts[n][1] *= ss

    return verts


class SvSpiralNode(bpy.types.Node, SverchCustomTreeNode):
    ''' Spiral '''
    bl_idname = 'SvSpiralNode'
    bl_label = 'Spiral'

    def update_spiral(self, context):
        if self.updating:
            return

        self.presets = " "
        updateNode(self, context)

    def update_presets(self, context):
        self.updating = True

        if self.presets == " ":
            self.updating = False
            return

        st, eR, iR, e, t, N, s, h = spiralPresets[self.presets]
        self.stype = st
        self.eRadius = eR
        self.iRadius = iR
        self.exponent = e
        self.turns = t
        self.resolution = N
        self.scale = s
        self.height = h
        self.phase = 0
        self.arms = 1

        self.updating = False
        updateNode(self, context)

    presetItems = [(k, k.title(), "", "", i) for i, (k, v) in enumerate(sorted(spiralPresets.items()))]

    presets = EnumProperty(
        name="Presets", items=presetItems,
        update=update_presets)

    stype = EnumProperty(
        name="Type", items=spiralTypeItems,
        default="ARCHIMEDEAN", update=update_spiral)

    normalize = EnumProperty(
        name="Normalize Radius", items=normalizeItems,
        default="ER", update=update_spiral)

    iRadius = FloatProperty(
        name="Interior Radius", description="Interior radius",
        default=1.0, min=0.0, update=update_spiral)

    eRadius = FloatProperty(
        name="Exterior Radius", description="Exterior radius",
        default=2.0, min=0.0, update=update_spiral)

    turns = IntProperty(
        name="Turns", description="Number of turns",
        default=11, min=1, update=update_spiral)

    arms = IntProperty(
        name="Arms", description="Number of spiral arms",
        default=1, min=1, update=update_spiral)

    flip = BoolProperty(
        name="Flip Direction", description="Flip spiral direction",
        default=False, update=update_spiral)

    scale = FloatProperty(
        name="Scale", description="Scale spiral vertices",
        default=1.0, update=update_spiral)

    height = FloatProperty(
        name="Height", description="Height of the spiral along z",
        default=0.0, update=update_spiral)

    phase = FloatProperty(
        name="Phase", description="Phase amount in radians around spiral center",
        default=0.0, update=update_spiral)

    exponent = FloatProperty(
        name="Exponent", description="Exponent attenuator",
        default=2.0, update=update_spiral)

    resolution = IntProperty(
        name="Turn Resolution", description="Number of vertices in one turn in the spiral",
        default=100, min=3, update=update_spiral)

    adaptive_resolution = BoolProperty(
        name="Adaptive Resolution",
        description="Auto adjust the curve resolution based on curve length",
        default=False, update=update_spiral)

    normalize_normals = BoolProperty(
        name="Normalize Normals", description="Normalize the normal vectors",
        default=True, update=update_spiral)

    normalize_tangents = BoolProperty(
        name="Normalize Tangents", description="Normalize the tangent vectors",
        default=True, update=update_spiral)

    updating = BoolProperty(default=False)  # used for disabling update callback

    def sv_init(self, context):
        self.width = 160
        self.inputs.new('StringsSocket', "R").prop_name = 'eRadius'
        self.inputs.new('StringsSocket', "r").prop_name = 'iRadius'
        self.inputs.new('StringsSocket', "e").prop_name = 'exponent'
        self.inputs.new('StringsSocket', "t").prop_name = 'turns'
        self.inputs.new('StringsSocket', "n").prop_name = 'resolution'
        self.inputs.new('StringsSocket', "s").prop_name = 'scale'
        self.inputs.new('StringsSocket', "h").prop_name = 'height'
        self.inputs.new('StringsSocket', "p").prop_name = 'phase'
        self.inputs.new('StringsSocket', "a").prop_name = 'arms'

        self.outputs.new('VerticesSocket', "Vertices")
        self.outputs.new('StringsSocket', "Edges")
        # self.outputs.new('VerticesSocket', "Normals")

        self.presets = "ARCHIMEDEAN"

    def draw_buttons(self, context, layout):
        layout.prop(self, 'presets')
        layout.prop(self, 'stype', text="")
        layout.prop(self, 'flip')
        if self.stype in ("LOGARITHMIC", "ARCHIMEDEAN", "SPIRANGLE"):
            layout.prop(self, 'normalize', expand=True)

    # def draw_buttons_ext(self, context, layout):
    #     box = layout.box()
    #     box.prop(self, 'adaptive_resolution')
    #     box.prop(self, 'normalize_normals')
    #     box.prop(self, 'normalize_tangents')

    def process(self):
        outputs = self.outputs
        # return if no outputs are connected
        if not any(s.is_linked for s in outputs):
            return

        # input values lists (single or multi value)
        inputs = self.inputs
        input_R = inputs["R"].sv_get()[0]  # list of interior radii
        input_r = inputs["r"].sv_get()[0]  # list of exterior radii
        input_e = inputs["e"].sv_get()[0]  # list of exponents
        input_t = inputs["t"].sv_get()[0]  # list of turns
        input_n = inputs["n"].sv_get()[0]  # list of curve resolutions
        input_s = inputs["s"].sv_get()[0]  # list of scales
        input_h = inputs["h"].sv_get()[0]  # list of heights (z)
        input_p = inputs["p"].sv_get()[0]  # list of phases
        input_a = inputs["a"].sv_get()[0]  # list of arms

        # sanitize the input
        input_t = list(map(lambda x: max(1, int(x)), input_t))
        input_n = list(map(lambda x: max(3, int(x)), input_n))
        input_a = list(map(lambda x: max(1, int(x)), input_a))

        # extra parameters
        f = self.flip  # flip direction

        # computation flags : # TODO
        # compute_normals = outputs["Normals"].is_linked
        # normalize_normals = self.normalize_normals
        # compute_tangents = outputs["Tangents"].is_linked
        # normalize_tangents = self.normalize_tangents
        # flags = [compute_normals, normalize_normals, False, False]
        normalize_eR = True if self.normalize == "ER" else False
        flags = [False, False, False, False]

        parameters = match_long_repeat([input_R, input_r, input_e, input_t,
                                        input_n, input_s, input_h, input_p, input_a])

        make_spiral = eval("make_" + self.stype.lower() + "_spiral")

        vertList = []
        edgeList = []
        # normList = []
        for R, r, e, t, n, s, h, p, a in zip(*parameters):

            for i in range(a):  # generate each arm
                p = p + 2 * pi / a
                settings = [R, r, e, t, n, s, h, p, f]  # spiral settings

                verts, edges, norms = make_spiral(flags, settings)

                if self.stype in ("LOGARITHMIC", "ARCHIMEDEAN", "SPIRANGLE"):
                    normalize_spiral(verts, normalize_eR, R, r, s)

                vertList.append(verts)
                edgeList.append(edges)
                # normList.append(norms)

        self.outputs['Vertices'].sv_set(vertList)
        self.outputs['Edges'].sv_set(edgeList)
        # self.outputs['Normals'].sv_set(normList)


def register():
    bpy.utils.register_class(SvSpiralNode)


def unregister():
    bpy.utils.unregister_class(SvSpiralNode)
