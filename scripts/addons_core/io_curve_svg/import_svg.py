# SPDX-FileCopyrightText: 2004-2009 JM Soler
# SPDX-FileCopyrightText: 2011-2022 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import re
import xml.dom.minidom
from math import cos, sin, tan, atan2, pi, ceil

import bpy
from mathutils import Vector, Matrix
from bpy.app.translations import pgettext_tip as tip_

from . import svg_colors
from .svg_util import (units,
                       srgb_to_linearrgb,
                       check_points_equal,
                       parse_array_of_floats,
                       read_float)

#### Common utilities ####

SVGEmptyStyles = {'useFill': None,
                  'fill': None}


def SVGCreateCurve(context):
    """
    Create new curve object to hold splines in
    """

    cu = bpy.data.curves.new("Curve", 'CURVE')
    obj = bpy.data.objects.new("Curve", cu)

    context['collection'].objects.link(obj)

    return obj


def SVGFinishCurve():
    """
    Finish curve creation
    """

    pass


def SVGFlipHandle(x, y, x1, y1):
    """
    Flip handle around base point
    """

    x = x + (x - x1)
    y = y + (y - y1)

    return x, y


def SVGParseCoord(coord, size):
    """
    Parse coordinate component to common basis

    Needed to handle coordinates set in cm, mm, inches.
    """

    token, last_char = read_float(coord)
    val = float(token)
    unit = coord[last_char:].strip()  # strip() in case there is a space

    if unit == '%':
        return float(size) / 100.0 * val
    return val * units[unit]


def SVGRectFromNode(node, context):
    """
    Get display rectangle from node
    """

    w = context['rect'][0]
    h = context['rect'][1]

    if node.getAttribute('viewBox'):
        viewBox = node.getAttribute('viewBox').replace(',', ' ').split()
        w = SVGParseCoord(viewBox[2], w)
        h = SVGParseCoord(viewBox[3], h)
    else:
        if node.getAttribute('width'):
            w = SVGParseCoord(node.getAttribute('width'), w)

        if node.getAttribute('height'):
            h = SVGParseCoord(node.getAttribute('height'), h)

    return (w, h)


def SVGMatrixFromNode(node, context):
    """
    Get transformation matrix from given node
    """

    tagName = node.tagName.lower()
    tags = ['svg:svg', 'svg:use', 'svg:symbol']

    if tagName not in tags and 'svg:' + tagName not in tags:
        return Matrix()

    rect = context['rect']
    has_user_coordinate = (len(context['rects']) > 1)

    m = Matrix()
    x = SVGParseCoord(node.getAttribute('x') or '0', rect[0])
    y = SVGParseCoord(node.getAttribute('y') or '0', rect[1])
    w = SVGParseCoord(node.getAttribute('width') or str(rect[0]), rect[0])
    h = SVGParseCoord(node.getAttribute('height') or str(rect[1]), rect[1])

    m = Matrix.Translation(Vector((x, y, 0.0)))
    if has_user_coordinate:
        if rect[0] != 0 and rect[1] != 0:
            m = m @ Matrix.Scale(w / rect[0], 4, Vector((1.0, 0.0, 0.0)))
            m = m @ Matrix.Scale(h / rect[1], 4, Vector((0.0, 1.0, 0.0)))

    if node.getAttribute('viewBox'):
        viewBox = node.getAttribute('viewBox').replace(',', ' ').split()
        vx = SVGParseCoord(viewBox[0], w)
        vy = SVGParseCoord(viewBox[1], h)
        vw = SVGParseCoord(viewBox[2], w)
        vh = SVGParseCoord(viewBox[3], h)

        if vw == 0 or vh == 0:
            return m

        if has_user_coordinate or (w != 0 and h != 0):
            sx = w / vw
            sy = h / vh
            scale = min(sx, sy)
        else:
            scale = 1.0
            w = vw
            h = vh

        tx = (w - vw * scale) / 2
        ty = (h - vh * scale) / 2
        m = m @ Matrix.Translation(Vector((tx, ty, 0.0)))

        m = m @ Matrix.Translation(Vector((-vx, -vy, 0.0)))
        m = m @ Matrix.Scale(scale, 4, Vector((1.0, 0.0, 0.0)))
        m = m @ Matrix.Scale(scale, 4, Vector((0.0, 1.0, 0.0)))

    return m


def SVGParseTransform(transform):
    """
    Parse transform string and return transformation matrix
    """

    m = Matrix()
    r = re.compile(r'\s*([A-z]+)\s*\((.*?)\)')

    for match in r.finditer(transform):
        func = match.group(1)
        params = match.group(2)
        params = params.replace(',', ' ').split()

        proc = SVGTransforms.get(func)
        if proc is None:
            raise Exception('Unknown transform function: ' + func)

        m = m @ proc(params)

    return m


def SVGGetMaterial(color, context):
    """
    Get material for specified color
    """

    materials = context['materials']
    rgb_re = re.compile(r'^\s*rgb\s*\(\s*(\d+)\s*,\s*(\d+)\s*,(\d+)\s*\)\s*$')

    diff = None
    if color.startswith('#'):
        color = color[1:]

        if len(color) == 3:
            color = color[0] * 2 + color[1] * 2 + color[2] * 2

        diff = (int(color[0:2], 16), int(color[2:4], 16), int(color[4:6], 16))
    elif color in svg_colors.SVGColors:
        diff = svg_colors.SVGColors[color]
    elif rgb_re.match(color):
        c = rgb_re.findall(color)[0]
        diff = (float(c[0]), float(c[1]), float(c[2]))
    else:
        return None

    diffuse_color = ([x / 255.0 for x in diff])

    if context['do_colormanage']:
        diffuse_color[0] = srgb_to_linearrgb(diffuse_color[0])
        diffuse_color[1] = srgb_to_linearrgb(diffuse_color[1])
        diffuse_color[2] = srgb_to_linearrgb(diffuse_color[2])

    if color in materials:
        return materials[color]

    mat = bpy.data.materials.new(name='SVGMat')
    mat.node_tree.nodes.clear()
    node_tree = mat.node_tree
    bsdf = node_tree.nodes.new("ShaderNodeBsdfDiffuse")
    output = node_tree.nodes.new("ShaderNodeOutputMaterial")
    output.location[0] += bsdf.width + 20.0
    node_tree.links.new(bsdf.outputs["BSDF"], output.inputs["Surface"])
    bsdf.inputs["Color"].default_value = (*diffuse_color, 0.0)

    materials[color] = mat

    return mat


def SVGTransformTranslate(params):
    """
    translate SVG transform command
    """

    tx = float(params[0])
    ty = float(params[1]) if len(params) > 1 else 0.0

    return Matrix.Translation(Vector((tx, ty, 0.0)))


def SVGTransformMatrix(params):
    """
    matrix SVG transform command
    """

    a = float(params[0])
    b = float(params[1])
    c = float(params[2])
    d = float(params[3])
    e = float(params[4])
    f = float(params[5])

    return Matrix(((a, c, 0.0, e),
                   (b, d, 0.0, f),
                   (0, 0, 1.0, 0),
                   (0, 0, 0.0, 1)))


def SVGTransformScale(params):
    """
    scale SVG transform command
    """

    sx = float(params[0])
    sy = float(params[1]) if len(params) > 1 else sx

    m = Matrix()

    m = m @ Matrix.Scale(sx, 4, Vector((1.0, 0.0, 0.0)))
    m = m @ Matrix.Scale(sy, 4, Vector((0.0, 1.0, 0.0)))

    return m


def SVGTransformSkewY(params):
    """
    skewY SVG transform command
    """

    ang = float(params[0]) * pi / 180.0

    return Matrix(((1.0, 0.0, 0.0),
                  (tan(ang), 1.0, 0.0),
                  (0.0, 0.0, 1.0))).to_4x4()


def SVGTransformSkewX(params):
    """
    skewX SVG transform command
    """

    ang = float(params[0]) * pi / 180.0

    return Matrix(((1.0, tan(ang), 0.0),
                  (0.0, 1.0, 0.0),
                  (0.0, 0.0, 1.0))).to_4x4()


def SVGTransformRotate(params):
    """
    skewX SVG transform command
    """

    ang = float(params[0]) * pi / 180.0
    cx = cy = 0.0

    if len(params) >= 3:
        cx = float(params[1])
        cy = float(params[2])

    tm = Matrix.Translation(Vector((cx, cy, 0.0)))
    rm = Matrix.Rotation(ang, 4, Vector((0.0, 0.0, 1.0)))

    return tm @ rm @ tm.inverted()


SVGTransforms = {'translate': SVGTransformTranslate,
                 'scale': SVGTransformScale,
                 'skewX': SVGTransformSkewX,
                 'skewY': SVGTransformSkewY,
                 'matrix': SVGTransformMatrix,
                 'rotate': SVGTransformRotate}


def SVGParseStyles(node, context):
    """
    Parse node to get different styles for displaying geometries
    (materials, filling flags, etc..)
    """

    styles = SVGEmptyStyles.copy()

    style = node.getAttribute('style')
    if style:
        elems = style.split(';')
        for elem in elems:
            s = elem.split(':')

            if len(s) != 2:
                continue

            name = s[0].strip().lower()
            val = s[1].strip()

            if name == 'fill':
                val = val.lower()
                if val == 'none':
                    styles['useFill'] = False
                else:
                    styles['useFill'] = True
                    styles['fill'] = SVGGetMaterial(val, context)

        if styles['useFill'] is None:
            styles['useFill'] = True
            styles['fill'] = SVGGetMaterial('#000', context)

        return styles

    if styles['useFill'] is None:
        fill = node.getAttribute('fill')
        if fill:
            fill = fill.lower()
            if fill == 'none':
                styles['useFill'] = False
            else:
                styles['useFill'] = True
                styles['fill'] = SVGGetMaterial(fill, context)

    if styles['useFill'] is None and context['style']:
        styles = context['style'].copy()

    if styles['useFill'] is None:
        styles['useFill'] = True
        styles['fill'] = SVGGetMaterial('#000', context)

    return styles


def id_names_from_node(node, ob):
    if node.getAttribute('id'):
        name = node.getAttribute('id')
        ob.name = name
        ob.data.name = name

#### SVG path helpers ####


class SVGPathData:
    """
    SVG Path data token supplier
    """

    __slots__ = ('_data',   # List of tokens
                 '_index',  # Index of current token in tokens list
                 '_len')    # Length of tokens list

    def __init__(self, d):
        """
        Initialize new path data supplier

        d - the definition of the outline of a shape
        """

        spaces = ' ,\t'
        commands = {'m', 'l', 'h', 'v', 'c', 's', 'q', '', 't', 'a', 'z'}
        current_command = ''
        tokens = []

        i = 0
        n = len(d)
        while i < n:
            c = d[i]

            if c in spaces:
                pass
            elif c.lower() in commands:
                tokens.append(c)
                current_command = c
                arg_index = 1
            elif c in ['-', '.'] or c.isdigit():
                # Special case for 'a/A' commands.
                # Arguments 4 and 5 are either 0 or 1 and might not
                # be separated from the next argument with space or comma.
                if current_command.lower() == 'a':
                    if arg_index % 7 in [4, 5]:
                        token = d[i]
                        last_char = i + 1
                    else:
                        token, last_char = read_float(d, i)
                else:
                    token, last_char = read_float(d, i)

                arg_index += 1
                tokens.append(token)

                # in most cases len(token) and (last_char - i) are the same
                # but with whitespace or ',' prefix they are not.

                i += (last_char - i) - 1

            i += 1

        self._data = tokens
        self._index = 0
        self._len = len(tokens)

    def eof(self):
        """
        Check if end of data reached
        """

        return self._index >= self._len

    def cur(self):
        """
        Return current token
        """

        if self.eof():
            return None

        return self._data[self._index]

    def lookupNext(self):
        """
        get next token without moving pointer
        """

        if self.eof():
            return None

        return self._data[self._index]

    def next(self):
        """
        Return current token and go to next one
        """

        if self.eof():
            return None

        token = self._data[self._index]
        self._index += 1

        return token

    def nextCoord(self):
        """
        Return coordinate created from current token and move to next token
        """

        token = self.next()

        if token is None:
            return None

        return float(token)


class SVGPathParser:
    """
    Parser of SVG path data
    """

    __slots__ = ('_data',  # Path data supplird
                 '_point',  # Current point coordinate
                 '_handle',  # Last handle coordinate
                 '_splines',  # List of all splies created during parsing
                 '_spline',  # Currently handling spline
                 '_commands',   # Hash of all supported path commands
                 '_use_fill',  # Splines would be filled, so expected to be closed
                 )

    def __init__(self, d, use_fill):
        """
        Initialize path parser

        d - the definition of the outline of a shape
        """

        self._data = SVGPathData(d)
        self._point = None   # Current point
        self._handle = None  # Last handle
        self._splines = []   # List of splines in path
        self._spline = None  # Current spline
        self._use_fill = use_fill

        self._commands = {'M': self._pathMoveTo,
                          'L': self._pathLineTo,
                          'H': self._pathLineTo,
                          'V': self._pathLineTo,
                          'C': self._pathCurveToCS,
                          'S': self._pathCurveToCS,
                          'Q': self._pathCurveToQT,
                          'T': self._pathCurveToQT,
                          'A': self._pathCurveToA,
                          'Z': self._pathClose,

                          'm': self._pathMoveTo,
                          'l': self._pathLineTo,
                          'h': self._pathLineTo,
                          'v': self._pathLineTo,
                          'c': self._pathCurveToCS,
                          's': self._pathCurveToCS,
                          'q': self._pathCurveToQT,
                          't': self._pathCurveToQT,
                          'a': self._pathCurveToA,
                          'z': self._pathClose}

    def _getCoordPair(self, relative, point):
        """
        Get next coordinate pair
        """

        x = self._data.nextCoord()
        y = self._data.nextCoord()

        if relative and point is not None:
            x += point[0]
            y += point[1]

        return x, y

    def _appendPoint(self, x, y, handle_left=None, handle_left_type='VECTOR',
                     handle_right=None, handle_right_type='VECTOR'):
        """
        Append point to spline

        If there's no active spline, create one and set it's first point
        to current point coordinate
        """

        if self._spline is None:
            self._spline = {'points': [],
                            'closed': False}

            self._splines.append(self._spline)

        if len(self._spline['points']) > 0:
            # Not sure about specifications, but Illustrator could create
            # last point at the same position, as start point (which was
            # reached by MoveTo command) to set needed handle coords.
            # It's also could use last point at last position to make path
            # filled.

            first = self._spline['points'][0]
            if check_points_equal((first['x'], first['y']), (x, y)):
                if handle_left is not None:
                    first['handle_left'] = handle_left
                    first['handle_left_type'] = 'FREE'

                if handle_left_type != 'VECTOR':
                    first['handle_left_type'] = handle_left_type

                if self._data.eof() or self._data.lookupNext().lower() == 'm':
                    self._spline['closed'] = True

                return

            last = self._spline['points'][-1]
            if last['handle_right_type'] == 'VECTOR' and handle_left_type == 'FREE':
                last['handle_right'] = (last['x'], last['y'])
                last['handle_right_type'] = 'FREE'
            if last['handle_right_type'] == 'FREE' and handle_left_type == 'VECTOR':
                handle_left = (x, y)
                handle_left_type = 'FREE'

        point = {'x': x,
                 'y': y,

                 'handle_left': handle_left,
                 'handle_left_type': handle_left_type,

                 'handle_right': handle_right,
                 'handle_right_type': handle_right_type}

        self._spline['points'].append(point)

    def _updateHandle(self, handle=None, handle_type=None):
        """
        Update right handle of previous point when adding new point to spline
        """

        point = self._spline['points'][-1]

        if handle_type is not None:
            point['handle_right_type'] = handle_type

        if handle is not None:
            point['handle_right'] = handle

    def _pathMoveTo(self, code):
        """
        MoveTo path command
        """

        relative = code.islower()
        x, y = self._getCoordPair(relative, self._point)

        self._spline = None  # Flag to start new spline
        self._point = (x, y)

        cur = self._data.cur()
        while cur is not None and not cur.isalpha():
            x, y = self._getCoordPair(relative, self._point)

            if self._spline is None:
                self._appendPoint(self._point[0], self._point[1])

            self._appendPoint(x, y)

            self._point = (x, y)
            cur = self._data.cur()

        self._handle = None

    def _pathLineTo(self, code):
        """
        LineTo path command
        """

        c = code.lower()

        cur = self._data.cur()
        while cur is not None and not cur.isalpha():
            if c == 'l':
                x, y = self._getCoordPair(code == 'l', self._point)
            elif c == 'h':
                x = self._data.nextCoord()
                y = self._point[1]
            else:
                x = self._point[0]
                y = self._data.nextCoord()

            if code == 'h':
                x += self._point[0]
            elif code == 'v':
                y += self._point[1]

            if self._spline is None:
                self._appendPoint(self._point[0], self._point[1])

            self._appendPoint(x, y)

            self._point = (x, y)
            cur = self._data.cur()

        self._handle = None

    def _pathCurveToCS(self, code):
        """
        Cubic BEZIER CurveTo path command
        """

        c = code.lower()
        cur = self._data.cur()
        while cur is not None and not cur.isalpha():
            if c == 'c':
                x1, y1 = self._getCoordPair(code.islower(), self._point)
                x2, y2 = self._getCoordPair(code.islower(), self._point)
            else:
                if self._handle is not None:
                    x1, y1 = SVGFlipHandle(self._point[0], self._point[1],
                                           self._handle[0], self._handle[1])
                else:
                    x1, y1 = self._point

                x2, y2 = self._getCoordPair(code.islower(), self._point)

            x, y = self._getCoordPair(code.islower(), self._point)

            if self._spline is None:
                self._appendPoint(self._point[0], self._point[1],
                                  handle_left_type='FREE', handle_left=self._point,
                                  handle_right_type='FREE', handle_right=(x1, y1))
            else:
                self._updateHandle(handle=(x1, y1), handle_type='FREE')

            self._appendPoint(x, y,
                              handle_left_type='FREE', handle_left=(x2, y2),
                              handle_right_type='FREE', handle_right=(x, y))

            self._point = (x, y)
            self._handle = (x2, y2)
            cur = self._data.cur()

    def _pathCurveToQT(self, code):
        """
        Quadratic BEZIER CurveTo path command
        """

        c = code.lower()
        cur = self._data.cur()

        while cur is not None and not cur.isalpha():
            if c == 'q':
                x1, y1 = self._getCoordPair(code.islower(), self._point)
            else:
                if self._handle is not None:
                    x1, y1 = SVGFlipHandle(self._point[0], self._point[1],
                                           self._handle[0], self._handle[1])
                else:
                    x1, y1 = self._point

            x, y = self._getCoordPair(code.islower(), self._point)

            if not check_points_equal((x, y), self._point):
                if self._spline is None:
                    self._appendPoint(self._point[0], self._point[1],
                                      handle_left_type='FREE', handle_left=self._point,
                                      handle_right_type='FREE', handle_right=self._point)

                self._appendPoint(x, y,
                                  handle_left_type='FREE', handle_left=(x1, y1),
                                  handle_right_type='FREE', handle_right=(x, y))

            self._point = (x, y)
            self._handle = (x1, y1)
            cur = self._data.cur()

    def _calcArc(self, rx, ry, ang, fa, fs, x, y):
        """
        Calc arc paths

        Copied and adopted from `paths_svg2obj.py` script for Blender 2.49:
        ``Copyright (c) jm soler juillet/novembre 2004-april 2009``.
        """

        cpx = self._point[0]
        cpy = self._point[1]
        rx = abs(rx)
        ry = abs(ry)
        px = abs((cos(ang) * (cpx - x) + sin(ang) * (cpy - y)) * 0.5) ** 2.0
        py = abs((cos(ang) * (cpy - y) - sin(ang) * (cpx - x)) * 0.5) ** 2.0
        rpx = rpy = 0.0

        if abs(rx) > 0.0:
            px = px / (rx ** 2.0)

        if abs(ry) > 0.0:
            rpy = py / (ry ** 2.0)

        pl = rpx + rpy
        if pl > 1.0:
            pl = pl ** 0.5
            rx *= pl
            ry *= pl

        carx = sarx = cary = sary = 0.0

        if abs(rx) > 0.0:
            carx = cos(ang) / rx
            sarx = sin(ang) / rx

        if abs(ry) > 0.0:
            cary = cos(ang) / ry
            sary = sin(ang) / ry

        x0 = carx * cpx + sarx * cpy
        y0 = -sary * cpx + cary * cpy
        x1 = carx * x + sarx * y
        y1 = -sary * x + cary * y
        d = (x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0)

        if abs(d) > 0.0:
            sq = 1.0 / d - 0.25
        else:
            sq = -0.25

        if sq < 0.0:
            sq = 0.0

        sf = sq ** 0.5
        if fs == fa:
            sf = -sf

        xc = 0.5 * (x0 + x1) - sf * (y1 - y0)
        yc = 0.5 * (y0 + y1) + sf * (x1 - x0)
        ang_0 = atan2(y0 - yc, x0 - xc)
        ang_1 = atan2(y1 - yc, x1 - xc)
        ang_arc = ang_1 - ang_0

        if ang_arc < 0.0 and fs == 1:
            ang_arc += 2.0 * pi
        elif ang_arc > 0.0 and fs == 0:
            ang_arc -= 2.0 * pi

        n_segs = int(ceil(abs(ang_arc * 2.0 / (pi * 0.5 + 0.001))))

        if self._spline is None:
            self._appendPoint(cpx, cpy,
                              handle_left_type='FREE', handle_left=(cpx, cpy),
                              handle_right_type='FREE', handle_right=(cpx, cpy))

        for i in range(n_segs):
            ang0 = ang_0 + i * ang_arc / n_segs
            ang1 = ang_0 + (i + 1) * ang_arc / n_segs
            ang_demi = 0.25 * (ang1 - ang0)
            t = 2.66666 * sin(ang_demi) * sin(ang_demi) / sin(ang_demi * 2.0)
            x1 = xc + cos(ang0) - t * sin(ang0)
            y1 = yc + sin(ang0) + t * cos(ang0)
            x2 = xc + cos(ang1)
            y2 = yc + sin(ang1)
            x3 = x2 + t * sin(ang1)
            y3 = y2 - t * cos(ang1)

            coord1 = ((cos(ang) * rx) * x1 + (-sin(ang) * ry) * y1,
                      (sin(ang) * rx) * x1 + (cos(ang) * ry) * y1)
            coord2 = ((cos(ang) * rx) * x3 + (-sin(ang) * ry) * y3,
                      (sin(ang) * rx) * x3 + (cos(ang) * ry) * y3)
            coord3 = ((cos(ang) * rx) * x2 + (-sin(ang) * ry) * y2,
                      (sin(ang) * rx) * x2 + (cos(ang) * ry) * y2)

            self._updateHandle(handle=coord1, handle_type='FREE')

            self._appendPoint(coord3[0], coord3[1],
                              handle_left_type='FREE', handle_left=coord2,
                              handle_right_type='FREE', handle_right=coord3)

    def _pathCurveToA(self, code):
        """
        Elliptical arc CurveTo path command
        """

        cur = self._data.cur()

        while cur is not None and not cur.isalpha():
            rx = float(self._data.next())
            ry = float(self._data.next())
            ang = float(self._data.next()) / 180 * pi
            fa = float(self._data.next())
            fs = float(self._data.next())
            x, y = self._getCoordPair(code.islower(), self._point)

            self._calcArc(rx, ry, ang, fa, fs, x, y)

            self._point = (x, y)
            self._handle = None
            cur = self._data.cur()

    def _pathClose(self, code):
        """
        Close path command
        """

        if self._spline:
            self._spline['closed'] = True

            cv = self._spline['points'][0]
            self._point = (cv['x'], cv['y'])

    def _pathCloseImplicitly(self):
        """
        Close path implicitly without changing current point coordinate
        """

        if self._spline:
            self._spline['closed'] = True

    def parse(self):
        """
        Execute parser
        """

        closed = False

        while not self._data.eof():
            code = self._data.next()
            cmd = self._commands.get(code)

            if cmd is None:
                raise Exception('Unknown path command: {0}' . format(code))

            if code in {'Z', 'z'}:
                closed = True
            else:
                closed = False

            if code in {'M', 'm'} and self._use_fill and not closed:
                self._pathCloseImplicitly()  # Ensure closed before MoveTo path command

            cmd(code)
        if self._use_fill and not closed:
            self._pathCloseImplicitly()  # Ensure closed at the end of parsing

    def getSplines(self):
        """
        Get splines definitions
        """

        return self._splines


class SVGGeometry:
    """
    Abstract SVG geometry
    """

    __slots__ = ('_node',  # XML node for geometry
                 '_context',  # Global SVG context (holds matrices stack, i.e.)
                 '_creating')  # Flag if geometry is already creating
    # for this node
    # need to detect cycles for USE node

    def __init__(self, node, context):
        """
        Initialize SVG geometry
        """

        self._node = node
        self._context = context
        self._creating = False

        if hasattr(node, 'getAttribute'):
            defs = context['defines']

            attr_id = node.getAttribute('id')
            if attr_id and defs.get('#' + attr_id) is None:
                defs['#' + attr_id] = self

            className = node.getAttribute('class')
            if className and defs.get(className) is None:
                defs[className] = self

    def _pushRect(self, rect):
        """
        Push display rectangle
        """

        self._context['rects'].append(rect)
        self._context['rect'] = rect

    def _popRect(self):
        """
        Pop display rectangle
        """

        self._context['rects'].pop()
        self._context['rect'] = self._context['rects'][-1]

    def _pushMatrix(self, matrix):
        """
        Push transformation matrix
        """

        current_matrix = self._context['matrix']
        self._context['matrix_stack'].append(current_matrix)
        self._context['matrix'] = current_matrix @ matrix

    def _popMatrix(self):
        """
        Pop transformation matrix
        """

        old_matrix = self._context['matrix_stack'].pop()
        self._context['matrix'] = old_matrix

    def _pushStyle(self, style):
        """
        Push style
        """

        self._context['styles'].append(style)
        self._context['style'] = style

    def _popStyle(self):
        """
        Pop style
        """

        self._context['styles'].pop()
        self._context['style'] = self._context['styles'][-1]

    def _transformCoord(self, point):
        """
        Transform SVG-file coords
        """

        v = Vector((point[0], point[1], 0.0))

        return self._context['matrix'] @ v

    def getNodeMatrix(self):
        """
        Get transformation matrix of node
        """

        return SVGMatrixFromNode(self._node, self._context)

    def parse(self):
        """
        Parse XML node to memory
        """

        pass

    def _doCreateGeom(self, instancing):
        """
        Internal handler to create real geometries
        """

        pass

    def getTransformMatrix(self):
        """
        Get matrix created from "transform" attribute
        """

        transform = self._node.getAttribute('transform')

        if transform:
            return SVGParseTransform(transform)

        return None

    def createGeom(self, instancing):
        """
        Create real geometries
        """

        if self._creating:
            return

        self._creating = True

        matrix = self.getTransformMatrix()
        if matrix is not None:
            self._pushMatrix(matrix)

        self._doCreateGeom(instancing)

        if matrix is not None:
            self._popMatrix()

        self._creating = False


class SVGGeometryContainer(SVGGeometry):
    """
    Container of SVG geometries
    """

    __slots__ = ('_geometries',  # List of chold geometries
                 '_styles')  # Styles, used for displaying

    def __init__(self, node, context):
        """
        Initialize SVG geometry container
        """

        super().__init__(node, context)

        self._geometries = []
        self._styles = SVGEmptyStyles

    def parse(self):
        """
        Parse XML node to memory
        """

        if type(self._node) is xml.dom.minidom.Element:
            self._styles = SVGParseStyles(self._node, self._context)

        self._pushStyle(self._styles)

        for node in self._node.childNodes:
            if type(node) is not xml.dom.minidom.Element:
                continue

            ob = parseAbstractNode(node, self._context)
            if ob is not None:
                self._geometries.append(ob)

        self._popStyle()

    def _doCreateGeom(self, instancing):
        """
        Create real geometries
        """

        for geom in self._geometries:
            geom.createGeom(instancing)

    def getGeometries(self):
        """
        Get list of parsed geometries
        """

        return self._geometries


class SVGGeometryPATH(SVGGeometry):
    """
    SVG path geometry
    """

    __slots__ = ('_splines',  # List of splines after parsing
                 '_styles')  # Styles, used for displaying

    def __init__(self, node, context):
        """
        Initialize SVG path
        """

        super().__init__(node, context)

        self._splines = []
        self._styles = SVGEmptyStyles

    def parse(self):
        """
        Parse SVG path node
        """

        d = self._node.getAttribute('d')

        self._styles = SVGParseStyles(self._node, self._context)

        pathParser = SVGPathParser(d, self._styles['useFill'])
        pathParser.parse()

        self._splines = pathParser.getSplines()

    def _doCreateGeom(self, instancing):
        """
        Create real geometries
        """

        ob = SVGCreateCurve(self._context)
        cu = ob.data

        id_names_from_node(self._node, ob)

        if self._styles['useFill']:
            cu.dimensions = '2D'
            cu.fill_mode = 'BOTH'
            cu.materials.append(self._styles['fill'])
        else:
            cu.dimensions = '3D'

        for spline in self._splines:
            act_spline = None

            if spline['closed'] and len(spline['points']) >= 2:
                first = spline['points'][0]
                last = spline['points'][-1]
                if (first['handle_left_type'] == 'FREE' and
                        last['handle_right_type'] == 'VECTOR'):
                    last['handle_right_type'] = 'FREE'
                    last['handle_right'] = (last['x'], last['y'])
                if (last['handle_right_type'] == 'FREE' and
                        first['handle_left_type'] == 'VECTOR'):
                    first['handle_left_type'] = 'FREE'
                    first['handle_left'] = (first['x'], first['y'])

            for point in spline['points']:
                co = self._transformCoord((point['x'], point['y']))

                if act_spline is None:
                    cu.splines.new('BEZIER')

                    act_spline = cu.splines[-1]
                    act_spline.use_cyclic_u = spline['closed']
                else:
                    act_spline.bezier_points.add(1)

                bezt = act_spline.bezier_points[-1]
                bezt.co = co

                bezt.handle_left_type = point['handle_left_type']
                if point['handle_left'] is not None:
                    handle = point['handle_left']
                    bezt.handle_left = self._transformCoord(handle)

                bezt.handle_right_type = point['handle_right_type']
                if point['handle_right'] is not None:
                    handle = point['handle_right']
                    bezt.handle_right = self._transformCoord(handle)

        SVGFinishCurve()


class SVGGeometryDEFS(SVGGeometryContainer):
    """
    Container for referenced elements
    """

    def createGeom(self, instancing):
        """
        Create real geometries
        """

        pass


class SVGGeometrySYMBOL(SVGGeometryContainer):
    """
    Referenced element
    """

    def _doCreateGeom(self, instancing):
        """
        Create real geometries
        """

        self._pushMatrix(self.getNodeMatrix())

        super()._doCreateGeom(False)

        self._popMatrix()

    def createGeom(self, instancing):
        """
        Create real geometries
        """

        if not instancing:
            return

        super().createGeom(instancing)


class SVGGeometryG(SVGGeometryContainer):
    """
    Geometry group
    """

    pass


class SVGGeometryUSE(SVGGeometry):
    """
    User of referenced elements
    """

    def _doCreateGeom(self, instancing):
        """
        Create real geometries
        """

        ref = self._node.getAttribute('xlink:href')
        geom = self._context['defines'].get(ref)

        if geom is not None:
            rect = SVGRectFromNode(self._node, self._context)
            self._pushRect(rect)

            self._pushMatrix(self.getNodeMatrix())

            geom.createGeom(True)

            self._popMatrix()

            self._popRect()


class SVGGeometryRECT(SVGGeometry):
    """
    SVG rectangle
    """

    __slots__ = ('_rect',  # coordinate and dimensions of rectangle
                 '_radius',  # Rounded corner radiuses
                 '_styles')  # Styles, used for displaying

    def __init__(self, node, context):
        """
        Initialize new rectangle
        """

        super().__init__(node, context)

        self._rect = ('0', '0', '0', '0')
        self._radius = ('0', '0')
        self._styles = SVGEmptyStyles

    def parse(self):
        """
        Parse SVG rectangle node
        """

        self._styles = SVGParseStyles(self._node, self._context)

        rect = []
        for attr in ['x', 'y', 'width', 'height']:
            val = self._node.getAttribute(attr)
            rect.append(val or '0')

        self._rect = (rect)

        rx = self._node.getAttribute('rx')
        ry = self._node.getAttribute('ry')

        self._radius = (rx, ry)

    def _appendCorner(self, spline, coord, firstTime, rounded):
        """
        Append new corner to rectangle
        """

        handle = None
        if len(coord) == 3:
            handle = self._transformCoord(coord[2])
            coord = (coord[0], coord[1])

        co = self._transformCoord(coord)

        if not firstTime:
            spline.bezier_points.add(1)

        bezt = spline.bezier_points[-1]
        bezt.co = co

        if rounded:
            if handle:
                bezt.handle_left_type = 'VECTOR'
                bezt.handle_right_type = 'FREE'

                bezt.handle_right = handle
            else:
                bezt.handle_left_type = 'FREE'
                bezt.handle_right_type = 'VECTOR'
                bezt.handle_left = co

        else:
            bezt.handle_left_type = 'VECTOR'
            bezt.handle_right_type = 'VECTOR'

    def _doCreateGeom(self, instancing):
        """
        Create real geometries
        """

        # Run-time parsing -- percents would be correct only if
        # parsing them now
        crect = self._context['rect']
        rect = []

        for i in range(4):
            rect.append(SVGParseCoord(self._rect[i], crect[i % 2]))

        r = self._radius
        rx = ry = 0.0

        if r[0] and r[1]:
            rx = min(SVGParseCoord(r[0], rect[0]), rect[2] / 2)
            ry = min(SVGParseCoord(r[1], rect[1]), rect[3] / 2)
        elif r[0]:
            rx = min(SVGParseCoord(r[0], rect[0]), rect[2] / 2)
            ry = min(rx, rect[3] / 2)
            rx = ry = min(rx, ry)
        elif r[1]:
            ry = min(SVGParseCoord(r[1], rect[1]), rect[3] / 2)
            rx = min(ry, rect[2] / 2)
            rx = ry = min(rx, ry)

        radius = (rx, ry)

        # Geometry creation
        ob = SVGCreateCurve(self._context)
        cu = ob.data

        id_names_from_node(self._node, ob)

        if self._styles['useFill']:
            cu.dimensions = '2D'
            cu.fill_mode = 'BOTH'
            cu.materials.append(self._styles['fill'])
        else:
            cu.dimensions = '3D'

        cu.splines.new('BEZIER')

        spline = cu.splines[-1]
        spline.use_cyclic_u = True

        x, y = rect[0], rect[1]
        w, h = rect[2], rect[3]
        rx, ry = radius[0], radius[1]
        rounded = False

        if rx or ry:
            #
            #      0 _______ 1
            #     /           \
            #    /             \
            #   7               2
            #   |               |
            #   |               |
            #   6               3
            #    \             /
            #     \           /
            #      5 _______ 4
            #

            # Optional third component -- right handle coord
            coords = [(x + rx, y),
                      (x + w - rx, y, (x + w, y)),
                      (x + w, y + ry),
                      (x + w, y + h - ry, (x + w, y + h)),
                      (x + w - rx, y + h),
                      (x + rx, y + h, (x, y + h)),
                      (x, y + h - ry),
                      (x, y + ry, (x, y))]

            rounded = True
        else:
            coords = [(x, y), (x + w, y), (x + w, y + h), (x, y + h)]

        firstTime = True
        for coord in coords:
            self._appendCorner(spline, coord, firstTime, rounded)
            firstTime = False

        SVGFinishCurve()


class SVGGeometryELLIPSE(SVGGeometry):
    """
    SVG ellipse
    """

    __slots__ = ('_cx',  # X-coordinate of center
                 '_cy',  # Y-coordinate of center
                 '_rx',  # X-axis radius of circle
                 '_ry',  # Y-axis radius of circle
                 '_styles')  # Styles, used for displaying

    def __init__(self, node, context):
        """
        Initialize new ellipse
        """

        super().__init__(node, context)

        self._cx = '0.0'
        self._cy = '0.0'
        self._rx = '0.0'
        self._ry = '0.0'
        self._styles = SVGEmptyStyles

    def parse(self):
        """
        Parse SVG ellipse node
        """

        self._styles = SVGParseStyles(self._node, self._context)

        self._cx = self._node.getAttribute('cx') or '0'
        self._cy = self._node.getAttribute('cy') or '0'
        self._rx = self._node.getAttribute('rx') or '0'
        self._ry = self._node.getAttribute('ry') or '0'

    def _doCreateGeom(self, instancing):
        """
        Create real geometries
        """

        # Run-time parsing -- percents would be correct only if
        # parsing them now
        crect = self._context['rect']

        cx = SVGParseCoord(self._cx, crect[0])
        cy = SVGParseCoord(self._cy, crect[1])
        rx = SVGParseCoord(self._rx, crect[0])
        ry = SVGParseCoord(self._ry, crect[1])

        if not rx or not ry:
            # Automaic handles will work incorrect in this case
            return

        # Create circle
        ob = SVGCreateCurve(self._context)
        cu = ob.data

        id_names_from_node(self._node, ob)

        if self._styles['useFill']:
            cu.dimensions = '2D'
            cu.fill_mode = 'BOTH'
            cu.materials.append(self._styles['fill'])
        else:
            cu.dimensions = '3D'

        coords = [((cx - rx, cy),
                   (cx - rx, cy + ry * 0.552),
                   (cx - rx, cy - ry * 0.552)),

                  ((cx, cy - ry),
                   (cx - rx * 0.552, cy - ry),
                   (cx + rx * 0.552, cy - ry)),

                  ((cx + rx, cy),
                   (cx + rx, cy - ry * 0.552),
                   (cx + rx, cy + ry * 0.552)),

                  ((cx, cy + ry),
                   (cx + rx * 0.552, cy + ry),
                   (cx - rx * 0.552, cy + ry))]

        spline = None
        for coord in coords:
            co = self._transformCoord(coord[0])
            handle_left = self._transformCoord(coord[1])
            handle_right = self._transformCoord(coord[2])

            if spline is None:
                cu.splines.new('BEZIER')
                spline = cu.splines[-1]
                spline.use_cyclic_u = True
            else:
                spline.bezier_points.add(1)

            bezt = spline.bezier_points[-1]
            bezt.co = co
            bezt.handle_left_type = 'FREE'
            bezt.handle_right_type = 'FREE'
            bezt.handle_left = handle_left
            bezt.handle_right = handle_right

        SVGFinishCurve()


class SVGGeometryCIRCLE(SVGGeometryELLIPSE):
    """
    SVG circle
    """

    def parse(self):
        """
        Parse SVG circle node
        """

        self._styles = SVGParseStyles(self._node, self._context)

        self._cx = self._node.getAttribute('cx') or '0'
        self._cy = self._node.getAttribute('cy') or '0'

        r = self._node.getAttribute('r') or '0'
        self._rx = self._ry = r


class SVGGeometryLINE(SVGGeometry):
    """
    SVG line
    """

    __slots__ = ('_x1',  # X-coordinate of beginning
                 '_y1',  # Y-coordinate of beginning
                 '_x2',  # X-coordinate of ending
                 '_y2')  # Y-coordinate of ending

    def __init__(self, node, context):
        """
        Initialize new line
        """

        super().__init__(node, context)

        self._x1 = '0.0'
        self._y1 = '0.0'
        self._x2 = '0.0'
        self._y2 = '0.0'

    def parse(self):
        """
        Parse SVG line node
        """

        self._x1 = self._node.getAttribute('x1') or '0'
        self._y1 = self._node.getAttribute('y1') or '0'
        self._x2 = self._node.getAttribute('x2') or '0'
        self._y2 = self._node.getAttribute('y2') or '0'

    def _doCreateGeom(self, instancing):
        """
        Create real geometries
        """

        # Run-time parsing -- percents would be correct only if
        # parsing them now
        crect = self._context['rect']

        x1 = SVGParseCoord(self._x1, crect[0])
        y1 = SVGParseCoord(self._y1, crect[1])
        x2 = SVGParseCoord(self._x2, crect[0])
        y2 = SVGParseCoord(self._y2, crect[1])

        # Create cline
        ob = SVGCreateCurve(self._context)
        cu = ob.data

        id_names_from_node(self._node, ob)

        coords = [(x1, y1), (x2, y2)]
        spline = None

        for coord in coords:
            co = self._transformCoord(coord)

            if spline is None:
                cu.splines.new('BEZIER')
                spline = cu.splines[-1]
                spline.use_cyclic_u = True
            else:
                spline.bezier_points.add(1)

            bezt = spline.bezier_points[-1]
            bezt.co = co
            bezt.handle_left_type = 'VECTOR'
            bezt.handle_right_type = 'VECTOR'

        SVGFinishCurve()


class SVGGeometryPOLY(SVGGeometry):
    """
    Abstract class for handling poly-geometries
    (polylines and polygons)
    """

    __slots__ = ('_points',  # Array of points for poly geometry
                 '_styles',  # Styles, used for displaying
                 '_closed')  # Should generated curve be closed?

    def __init__(self, node, context):
        """
        Initialize new poly geometry
        """

        super().__init__(node, context)

        self._points = []
        self._styles = SVGEmptyStyles
        self._closed = False

    def parse(self):
        """
        Parse poly node
        """

        self._styles = SVGParseStyles(self._node, self._context)

        points = parse_array_of_floats(self._node.getAttribute('points'))

        prev = None
        self._points = []

        for p in points:
            if prev is None:
                prev = p
            else:
                self._points.append((prev, p))
                prev = None

    def _doCreateGeom(self, instancing):
        """
        Create real geometries
        """

        ob = SVGCreateCurve(self._context)
        cu = ob.data

        id_names_from_node(self._node, ob)

        if self._closed and self._styles['useFill']:
            cu.dimensions = '2D'
            cu.fill_mode = 'BOTH'
            cu.materials.append(self._styles['fill'])
        else:
            cu.dimensions = '3D'

        spline = None

        for point in self._points:
            co = self._transformCoord(point)

            if spline is None:
                cu.splines.new('BEZIER')
                spline = cu.splines[-1]
                spline.use_cyclic_u = self._closed
            else:
                spline.bezier_points.add(1)

            bezt = spline.bezier_points[-1]
            bezt.co = co
            bezt.handle_left_type = 'VECTOR'
            bezt.handle_right_type = 'VECTOR'

        SVGFinishCurve()


class SVGGeometryPOLYLINE(SVGGeometryPOLY):
    """
    SVG polyline geometry
    """

    pass


class SVGGeometryPOLYGON(SVGGeometryPOLY):
    """
    SVG polygon geometry
    """

    def __init__(self, node, context):
        """
        Initialize new polygon geometry
        """

        super().__init__(node, context)

        self._closed = True


class SVGGeometrySVG(SVGGeometryContainer):
    """
    Main geometry holder
    """

    def _doCreateGeom(self, instancing):
        """
        Create real geometries
        """

        rect = SVGRectFromNode(self._node, self._context)

        matrix = self.getNodeMatrix()

        # Better SVG compatibility: match svg-document units
        # with blender units

        viewbox = []
        unit = ''

        if self._node.getAttribute('height'):
            raw_height = self._node.getAttribute('height')
            token, last_char = read_float(raw_height)
            document_height = float(token)
            unit = raw_height[last_char:].strip()

        if self._node.getAttribute('viewBox'):
            viewbox = parse_array_of_floats(self._node.getAttribute('viewBox'))

        if len(viewbox) == 4 and unit in ('cm', 'mm', 'in', 'pt', 'pc'):

            # convert units to BU:
            unitscale = units[unit] / 90 * 1000 / 39.3701

            # apply blender unit scale:
            unitscale = unitscale / bpy.context.scene.unit_settings.scale_length

            matrix = matrix @ Matrix.Scale(unitscale, 4, Vector((1.0, 0.0, 0.0)))
            matrix = matrix @ Matrix.Scale(unitscale, 4, Vector((0.0, 1.0, 0.0)))

        # match document origin with 3D space origin.
        if self._node.getAttribute('viewBox'):
            viewbox = parse_array_of_floats(self._node.getAttribute('viewBox'))
            matrix = matrix @ matrix.Translation([0.0, - viewbox[1] - viewbox[3], 0.0])

        self._pushMatrix(matrix)
        self._pushRect(rect)

        super()._doCreateGeom(False)

        self._popRect()
        self._popMatrix()


class SVGLoader(SVGGeometryContainer):
    """
    SVG file loader
    """

    def getTransformMatrix(self):
        """
        Get matrix created from "transform" attribute
        """

        # SVG document doesn't support transform specification
        # it can't even hold attributes

        return None

    def __init__(self, context, filepath, do_colormanage):
        """
        Initialize SVG loader
        """
        import os

        svg_name = os.path.basename(filepath)
        scene = context.scene
        collection = bpy.data.collections.new(name=svg_name)
        scene.collection.children.link(collection)

        node = xml.dom.minidom.parse(filepath)

        m = Matrix()
        m = m @ Matrix.Scale(1.0 / 90.0 * 0.3048 / 12.0, 4, Vector((1.0, 0.0, 0.0)))
        m = m @ Matrix.Scale(-1.0 / 90.0 * 0.3048 / 12.0, 4, Vector((0.0, 1.0, 0.0)))

        rect = (0, 0)

        self._context = {'defines': {},
                         'rects': [rect],
                         'rect': rect,
                         'matrix_stack': [],
                         'matrix': m,
                         'materials': {},
                         'styles': [None],
                         'style': None,
                         'do_colormanage': do_colormanage,
                         'collection': collection}

        super().__init__(node, self._context)


svgGeometryClasses = {
    'svg': SVGGeometrySVG,
    'path': SVGGeometryPATH,
    'defs': SVGGeometryDEFS,
    'symbol': SVGGeometrySYMBOL,
    'use': SVGGeometryUSE,
    'rect': SVGGeometryRECT,
    'ellipse': SVGGeometryELLIPSE,
    'circle': SVGGeometryCIRCLE,
    'line': SVGGeometryLINE,
    'polyline': SVGGeometryPOLYLINE,
    'polygon': SVGGeometryPOLYGON,
    'g': SVGGeometryG}


def parseAbstractNode(node, context):
    name = node.tagName.lower()

    if name.startswith('svg:'):
        name = name[4:]

    geomClass = svgGeometryClasses.get(name)

    if geomClass is not None:
        ob = geomClass(node, context)
        ob.parse()

        return ob

    return None


def load_svg(context, filepath, do_colormanage):
    """
    Load specified SVG file
    """

    if bpy.ops.object.mode_set.poll():
        bpy.ops.object.mode_set(mode='OBJECT')

    loader = SVGLoader(context, filepath, do_colormanage)
    loader.parse()
    loader.createGeom(False)


def load(operator, context, filepath=""):

    # error in code should raise exceptions but loading
    # non SVG files can give useful messages.
    do_colormanage = context.scene.display_settings.display_device != 'NONE'
    try:
        load_svg(context, filepath, do_colormanage)
    except (xml.parsers.expat.ExpatError, UnicodeEncodeError) as e:
        import traceback
        traceback.print_exc()

        operator.report({'WARNING'}, tip_("Unable to parse XML, %s:%s for file %r") % (type(e).__name__, e, filepath))
        return {'CANCELLED'}

    return {'FINISHED'}
