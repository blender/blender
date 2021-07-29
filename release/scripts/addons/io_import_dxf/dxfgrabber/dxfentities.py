# encoding: utf-8
# Purpose: entity classes, new implementation without dxf12/dxf13 layer
# Created: 17.04.2016
# Copyright (C) 2016, Manfred Moitzi
# License: MIT License
from __future__ import unicode_literals
__author__ = "mozman <mozman@gmx.at>"

import math

from . import const
from .color import TrueColor
from .styles import default_text_style
from .decode import decode

SPECIAL_CHARS = {
    'd': '°'
}


basic_attribs = {
    5: 'handle',
    6: 'linetype',
    8: 'layer',
    39: 'thickness',
    48: 'ltscale',
    62: 'color',
    67: 'paperspace',
    210: 'extrusion',
    284: 'shadow_mode',
    330: 'owner',
    370: 'line_weight',
    410: 'layout_tab_name',
}


class DXFEntity(object):
    def __init__(self):
        self.dxftype = 'ENTITY'
        self.handle = None
        self.owner = None
        self.paperspace = None
        self.layer = '0'
        self.linetype = None
        self.thickness = 0.0
        self.extrusion = None
        self.ltscale = 1.0
        self.line_weight = 0
        self.invisible = 0
        self.color = const.BYLAYER
        self.true_color = None
        self.transparency = None
        self.shadow_mode = None
        self.layout_tab_name = None

    def setup_attributes(self, tags):
        self.dxftype = tags.get_type()
        for code, value in tags.plain_tags():
            if code in basic_attribs:
                self.__setattr__(basic_attribs[code], value)
            elif code == 420:
                self.true_color = TrueColor(value)
            elif code == 440:
                self.transparency = 1. - float(value & 0xFF) / 255.
            else:
                yield code, value  # chain of generators

    def set_default_extrusion(self):  # call only for 2d entities with extrusion vector
        if self.extrusion is None:
            self.extrusion = (0., 0., 1.)

    def __str__(self):
        return "{} [{}]".format(self.dxftype, self.handle)


class Point(DXFEntity):
    def __init__(self):
        super(Point, self).__init__()
        self.point = (0, 0, 0)

    def setup_attributes(self, tags):
        for code, value in super(Point, self).setup_attributes(tags):
            if code == 10:
                self.point = value
            else:
                yield code, value  # chain of generators
        self.set_default_extrusion()


class Line(DXFEntity):
    def __init__(self):
        super(Line, self).__init__()
        self.start = (0, 0, 0)
        self.end = (0, 0, 0)

    def setup_attributes(self, tags):
        for code, value in super(Line, self).setup_attributes(tags):
            if code == 10:
                self.start = value
            elif code == 11:
                self.end = value
            else:
                yield code, value  # chain of generators


class Circle(DXFEntity):
    def __init__(self):
        super(Circle, self).__init__()
        self.center = (0, 0, 0)
        self.radius = 1.0

    def setup_attributes(self, tags):
        for code, value in super(Circle, self).setup_attributes(tags):
            if code == 10:
                self.center = value
            elif code == 40:
                self.radius = value
            else:
                yield code, value  # chain of generators
        self.set_default_extrusion()


class Arc(Circle):
    def __init__(self):
        super(Arc, self).__init__()
        self.start_angle = 0.
        self.end_angle = 360.

    def setup_attributes(self, tags):
        for code, value in super(Arc, self).setup_attributes(tags):
            if code == 50:
                self.start_angle = value
            elif code == 51:
                self.end_angle = value
            else:
                yield code, value  # chain of generators
        self.set_default_extrusion()

TRACE_CODES = frozenset((10, 11, 12, 13))


class Trace(DXFEntity):
    def __init__(self):
        super(Trace, self).__init__()
        self.points = []

    def setup_attributes(self, tags):
        for code, value in super(Trace, self).setup_attributes(tags):
            if code in TRACE_CODES:
                self.points.append(value)
            else:
                yield code, value  # chain of generators
        self.set_default_extrusion()


Solid = Trace


class Face(Trace):
    def __init__(self):
        super(Face, self).__init__()
        self.points = []
        self.invisible_edge = 0

    def setup_attributes(self, tags):
        for code, value in super(Face, self).setup_attributes(tags):
            if code == 70:
                self.invisible_edge = value
            else:
                yield code, value  # chain of generators
        self.set_default_extrusion()

    def is_edge_invisible(self, edge):
        # edges 0 .. 3
        return bool(self.invisible_edge & (1 << edge))


class Text(DXFEntity):
    def __init__(self):
        super(Text, self).__init__()
        self.insert = (0., 0.)
        self.height = 1.0
        self.text = ""
        self.rotation = 0.
        self.oblique = 0.
        self.style = "STANDARD"
        self.width = 1.
        self.is_backwards = False
        self.is_upside_down = False
        self.halign = 0
        self.valign = 0
        self.align_point = None
        self.font = None
        self.big_font = None

    def setup_attributes(self, tags):
        for code, value in super(Text, self).setup_attributes(tags):
            if code == 10:
                self.insert = value
            elif code == 11:
                self.align_point = value
            elif code == 1:
                self.text = value
            elif code == 7:
                self.style = value
            elif code == 40:
                self.height = value
            elif code == 41:
                self.width = value
            elif code == 50:
                self.rotation = value
            elif code == 51:
                self.oblique = value
            elif code == 71:
                self.is_backwards = bool(value & 2)
                self.is_upside_down = bool(value & 4)
            elif code == 72:
                self.halign = value
            elif code == 73:
                self.valign = value
            else:
                yield code, value  # chain of generators
        self.set_default_extrusion()

    def resolve_text_style(self, text_styles):
        style = text_styles.get(self.style, None)
        if style is None:
            style = default_text_style
        if self.height == 0:
            self.height = style.height
        if self.width == 0:
            self.width = style.width
        if self.oblique is None:
            self.oblique = style.oblique
        if self.is_backwards is None:
            self.is_backwards = style.is_backwards
        if self.is_upside_down is None:
            self.is_upside_down = style.is_upside_down
        if self.font is None:
            self.font = style.font
        if self.big_font is None:
            self.big_font = style.big_font

    def plain_text(self):
        chars = []
        raw_chars = list(reversed(self.text))  # text splitted into chars, in reversed order for efficient pop()
        while len(raw_chars):
            char = raw_chars.pop()
            if char == '%':  # formatting codes and special characters
                if len(raw_chars) and raw_chars[-1] == '%':
                    raw_chars.pop()  # '%'
                    if len(raw_chars):
                        special_char = raw_chars.pop()  # command char
                        chars.append(SPECIAL_CHARS.get(special_char, ""))
                else:  # char is just a single '%'
                    chars.append(char)
            else:  # char is what it is, a character
                chars.append(char)
        return "".join(chars)


class Attrib(Text):
    def __init__(self):
        super(Attrib, self).__init__()
        self.field_length = 0
        self.tag = ""

    def setup_attributes(self, tags):
        for code, value in super(Attrib, self).setup_attributes(tags):
            if code == 2:
                self.tag = value
            elif code == 73:
                self.field_length = value
            else:
                yield code, value


class Insert(DXFEntity):
    def __init__(self):
        super(Insert, self).__init__()
        self.name = ""
        self.insert = (0., 0., 0.)
        self.rotation = 0.
        self.scale = (1., 1., 1.)
        self.row_count = 1
        self.row_spacing = 0.
        self.col_count = 1
        self.col_spacing = 0.
        self.attribsfollow = False
        self.attribs = []

    def setup_attributes(self, tags):
        xscale = 1.
        yscale = 1.
        zscale = 1.
        for code, value in super(Insert, self).setup_attributes(tags):
            if code == 2:
                self.name = value
            elif code == 10:
                self.insert = value
            elif code == 41:
                xscale = value
            elif code == 42:
                yscale = value
            elif code == 43:
                zscale = value
            elif code == 44:
                self.col_spacing = value
            elif code == 45:
                self.row_spacing = value
            elif code == 50:
                self.rotation = value
            elif code == 66:
                self.attribsfollow = bool(value)
            elif code == 70:
                self.col_count = value
            elif code == 71:
                self.row_count = value
            else:
                yield code, value  # chain of generators
        self.scale = (xscale, yscale, zscale)
        self.set_default_extrusion()

    def find_attrib(self, attrib_tag):
        for attrib in self.attribs:
            if attrib.tag == attrib_tag:
                return attrib
        return None

    def append_data(self, attribs):
        self.attribs = attribs


class Polyline(DXFEntity):
    LINE_TYPES = frozenset(('spline2d', 'polyline2d', 'polyline3d'))

    def __init__(self):
        super(Polyline, self).__init__()
        self.vertices = []  # set in append data
        self.points = []  # set in append data
        self.control_points = []  # set in append data
        self.width = []  # set in append data
        self.bulge = []  # set in append data
        self.tangents = []  # set in append data
        self.flags = 0
        self.mode = 'polyline2d'
        self.mcount = 0
        self.ncount = 0
        self.default_start_width = 0.
        self.default_end_width = 0.
        self.is_mclosed = False
        self.is_nclosed = False
        self.is_closed = False
        self.elevation = (0., 0., 0.)
        self.m_smooth_density = 0.
        self.n_smooth_density = 0.
        self.smooth_type = 0
        self.spline_type = None

    def setup_attributes(self, tags):
        def get_mode():
            flags = self.flags
            if flags & const.POLYLINE_SPLINE_FIT_VERTICES_ADDED:
                return 'spline2d'
            elif flags & const.POLYLINE_3D_POLYLINE:
                return 'polyline3d'
            elif flags & const.POLYLINE_3D_POLYMESH:
                return 'polymesh'
            elif flags & const.POLYLINE_POLYFACE:
                return 'polyface'
            else:
                return 'polyline2d'

        for code, value in super(Polyline, self).setup_attributes(tags):
            if code == 10:
                self.elevation = value
            elif code == 40:
                self.default_start_width = value
            elif code == 41:
                self.default_end_width = value
            elif code == 70:
                self.flags = value
            elif code == 71:
                self.mcount = value
            elif code == 72:
                self.ncount = value
            elif code == 73:
                self.m_smooth_density = value
            elif code == 73:
                self.n_smooth_density = value
            elif code == 75:
                self.smooth_type = value
            else:
                yield code, value  # chain of generators

        self.mode = get_mode()
        if self.mode == 'spline2d':
            if self.smooth_type == const.POLYMESH_CUBIC_BSPLINE:
                self.spline_type = 'cubic_bspline'
            elif self.smooth_type == const.POLYMESH_QUADRIC_BSPLINE:
                self.spline_type = 'quadratic_bspline'
            elif self.smooth_type == const.POLYMESH_BEZIER_SURFACE:
                self.spline_type = 'bezier_curve'  # is this a valid spline type for DXF12?
        self.is_mclosed = bool(self.flags & const.POLYLINE_MESH_CLOSED_M_DIRECTION)
        self.is_nclosed = bool(self.flags & const.POLYLINE_MESH_CLOSED_N_DIRECTION)
        self.is_closed = self.is_mclosed
        self.set_default_extrusion()

    def __len__(self):
        return len(self.vertices)

    def __getitem__(self, item):
        return self.vertices[item]

    def __iter__(self):
        return iter(self.vertices)

    def append_data(self, vertices):
        def default_width(start_width, end_width):
            if start_width == 0.:
                start_width = self.default_start_width
            if end_width == 0.:
                end_width = self.default_end_width
            return start_width, end_width

        self.vertices = vertices
        if self.mode in Polyline.LINE_TYPES:
            for vertex in self.vertices:
                if vertex.flags & const.VTX_SPLINE_FRAME_CONTROL_POINT:
                    self.control_points.append(vertex.location)
                else:
                    self.points.append(vertex.location)
                    self.width.append(default_width(vertex.start_width, vertex.end_width))
                    self.bulge.append(vertex.bulge)
                    self.tangents.append(vertex.tangent if vertex.flags & const.VTX_CURVE_FIT_TANGENT else None)

    def cast(self):
        if self.mode == 'polyface':
            return PolyFace(self)
        elif self.mode == 'polymesh':
            return PolyMesh(self)
        else:
            return self


class SubFace(object):
    def __init__(self, face_record, vertices):
        self._vertices = vertices
        self.face_record = face_record

    def __len__(self):
        return len(self.face_record.vtx)

    def __getitem__(self, item):
        return self._vertices[self._vertex_index(item)]

    def __iter__(self):
        return (self._vertices[index].location for index in self.indices())

    def _vertex_index(self, pos):
        return abs(self.face_record.vtx[pos]) - 1

    def indices(self):
        return tuple(abs(i)-1 for i in self.face_record.vtx if i != 0)

    def is_edge_visible(self, pos):
        return self.face_record.vtx[pos] > 0


class PolyShape(object):
    def __init__(self, polyline, dxftype):
        # copy all dxf attributes from polyline
        for key, value in polyline.__dict__.items():
            self.__dict__[key] = value
        self.dxftype = dxftype

    def __str__(self):
        return "{} [{}]".format(self.dxftype, self.handle)


class PolyFace(PolyShape):
    def __init__(self, polyline):
        VERTEX_FLAGS = const.VTX_3D_POLYFACE_MESH_VERTEX + const.VTX_3D_POLYGON_MESH_VERTEX

        def is_vertex(flags):
            return flags & VERTEX_FLAGS == VERTEX_FLAGS

        super(PolyFace, self).__init__(polyline, 'POLYFACE')
        vertices = []
        face_records = []
        for vertex in polyline.vertices:
            (vertices if is_vertex(vertex.flags) else face_records).append(vertex)

        self._face_records = face_records

    def __getitem__(self, item):
        return SubFace(self._face_records[item], self.vertices)

    def __len__(self):
        return len(self._face_records)

    def __iter__(self):
        return (SubFace(f, self.vertices) for f in self._face_records)


class PolyMesh(PolyShape):
    def __init__(self, polyline):
        super(PolyMesh, self).__init__(polyline, 'POLYMESH')

    def __iter__(self):
        return iter(self.vertices)

    def get_location(self, pos):
        return self.get_vertex(pos).location

    def get_vertex(self, pos):
        m, n = pos
        if 0 <= m < self.mcount and 0 <= n < self.ncount:
            pos = m * self.ncount + n
            return self.vertices[pos]
        else:
            raise IndexError(repr(pos))


class Vertex(DXFEntity):
    def __init__(self):
        super(Vertex, self).__init__()
        self.location = (0., 0., 0.)
        self.flags = 0
        self.start_width = 0.
        self.end_width = 0.
        self.bulge = 0.
        self.tangent = None
        self.vtx = None

    def setup_attributes(self, tags):
        vtx0 = 0
        vtx1 = 0
        vtx2 = 0
        vtx3 = 0
        for code, value in super(Vertex, self).setup_attributes(tags):
            if code == 10:
                self.location = value
            elif code == 40:
                self.start_width = value
            elif code == 41:
                self.end_width = value
            elif code == 42:
                self.bulge = value
            elif code == 50:
                self.tangent = value
            elif code == 70:
                self.flags = value
            elif code == 71:
                vtx0 = value
            elif code == 72:
                vtx1 = value
            elif code == 73:
                vtx2 = value
            elif code == 74:
                vtx3 = value
            else:
                yield code, value  # chain of generators
        indices = (vtx0, vtx1, vtx2, vtx3)
        if any(indices):
            self.vtx = indices

    def __getitem__(self, item):
        return self.location[item]

    def __iter__(self):
        return iter(self.location)


class Block(DXFEntity):
    def __init__(self):
        super(Block, self).__init__()
        self.basepoint = (0, 0, 0)
        self.name = ''
        self.description = ''
        self.flags = 0
        self.xrefpath = ""
        self._entities = []

    def setup_attributes(self, tags):
        for code, value in super(Block, self).setup_attributes(tags):
            if code == 2:
                self.name = value
            elif code == 4:
                self.description = value
            elif code == 1:
                self.xrefpath = value
            elif code == 10:
                self.basepoint = value
            elif code == 70:
                self.flags = value
            else:
                yield code, value  # chain of generators

    @property
    def is_xref(self):
        return bool(self.flags & const.BLK_XREF)

    @property
    def is_xref_overlay(self):
        return bool(self.flags & const.BLK_XREF_OVERLAY)

    @property
    def is_anonymous(self):
        return bool(self.flags & const.BLK_ANONYMOUS)

    def set_entities(self, entities):
        self._entities = entities

    def __iter__(self):
        return iter(self._entities)

    def __getitem__(self, item):
        return self._entities[item]

    def __len__(self):
        return len(self._entities)


class LWPolyline(DXFEntity):
    def __init__(self):
        super(LWPolyline, self).__init__()
        self.points = []
        self.width = []
        self.bulge = []
        self.elevation = 0.
        self.const_width = 0.
        self.flags = 0

    def setup_attributes(self, tags):
        bulge, start_width, end_width = 0., 0., 0.
        init = True

        for code, value in super(LWPolyline, self).setup_attributes(tags):
            if code == 10:
                if not init:
                    self.bulge.append(bulge)
                    self.width.append((start_width, end_width))
                    bulge, start_width, end_width = 0., 0., 0.
                self.points.append(value)
                init = False
            elif code == 40:
                start_width = value
            elif code == 41:
                end_width = value
            elif code == 42:
                bulge = value
            elif code == 38:
                self.elevation = value
            elif code == 39:
                self.thickness = value
            elif code == 43:
                self.const_width = value
            elif code == 70:
                self.flags = value
            elif code == 210:
                self.extrusion = value
            else:
                yield code, value  # chain of generators

        # add values for the last point
        self.bulge.append(bulge)
        self.width.append((start_width, end_width))

        if self.const_width != 0.:
            self.width = []
        self.set_default_extrusion()

    @property
    def is_closed(self):
        return bool(self.flags & 1)

    def __len__(self):
        return len(self.points)

    def __getitem__(self, item):
        return self.points[item]

    def __iter__(self):
        return iter(self.points)


class Ellipse(DXFEntity):
    def __init__(self):
        super(Ellipse, self).__init__()
        self.center = (0., 0., 0.)
        self.major_axis = (1., 0., 0.)
        self.ratio = 1.0
        self.start_param = 0.
        self.end_param = 6.283185307179586

    def setup_attributes(self, tags):
        for code, value in super(Ellipse, self).setup_attributes(tags):
            if code == 10:
                self.center = value
            elif code == 11:
                self.major_axis = value
            elif code == 40:
                self.ratio = value
            elif code == 41:
                self.start_param = value
            elif code == 42:
                self.end_param = value
            else:
                yield code, value  # chain of generators
        self.set_default_extrusion()


class Ray(DXFEntity):
    def __init__(self):
        super(Ray, self).__init__()
        self.start = (0, 0, 0)
        self.unit_vector = (1, 0, 0)

    def setup_attributes(self, tags):
        for code, value in super(Ray, self).setup_attributes(tags):
            if code == 10:
                self.start = value
            elif code == 11:
                self.unit_vector = value
            else:
                yield code, value  # chain of generators


def deg2vec(deg):
    rad = float(deg) * math.pi / 180.0
    return math.cos(rad), math.sin(rad), 0.


def normalized(vector):
    x, y, z = vector
    m = (x**2 + y**2 + z**2)**0.5
    return x/m, y/m, z/m

##################################################
# MTEXT inline codes
# \L	Start underline
# \l	Stop underline
# \O	Start overstrike
# \o	Stop overstrike
# \K	Start strike-through
# \k	Stop strike-through
# \P	New paragraph (new line)
# \pxi	Control codes for bullets, numbered paragraphs and columns
# \X	Paragraph wrap on the dimension line (only in dimensions)
# \Q	Slanting (obliquing) text by angle - e.g. \Q30;
# \H	Text height - e.g. \H3x;
# \W	Text width - e.g. \W0.8x;
# \F	Font selection
#
#     e.g. \Fgdt;o - GDT-tolerance
#     e.g. \Fkroeger|b0|i0|c238|p10 - font Kroeger, non-bold, non-italic, codepage 238, pitch 10
#
# \S	Stacking, fractions
#
#     e.g. \SA^B:
#     A
#     B
#     e.g. \SX/Y:
#     X
#     -
#     Y
#     e.g. \S1#4:
#     1/4
#
# \A	Alignment
#
#     \A0; = bottom
#     \A1; = center
#     \A2; = top
#
# \C	Color change
#
#     \C1; = red
#     \C2; = yellow
#     \C3; = green
#     \C4; = cyan
#     \C5; = blue
#     \C6; = magenta
#     \C7; = white
#
# \T	Tracking, char.spacing - e.g. \T2;
# \~	Non-wrapping space, hard space
# {}	Braces - define the text area influenced by the code
# \	Escape character - e.g. \\ = "\", \{ = "{"
#
# Codes and braces can be nested up to 8 levels deep

ESCAPED_CHARS = "\\{}"
GROUP_CHARS = "{}"
ONE_CHAR_COMMANDS = "PLlOoKkX"


class MText(DXFEntity):
    def __init__(self):
        super(MText, self).__init__()
        self.insert = (0., 0., 0.)
        self.raw_text = ""
        self.height = 0.
        self.rect_width = None
        self.horizontal_width = None
        self.vertical_height = None
        self.line_spacing = 1.
        self.attachment_point = 1
        self.style = 'STANDARD'
        self.xdirection = (1., 0., 0.)
        self.font = None
        self.big_font = None

    def setup_attributes(self, tags):
        text = ""
        lines = []
        rotation = 0.
        xdir = None
        for code, value in super(MText, self).setup_attributes(tags):
            if code == 10:
                self.insert = value
            elif code == 11:
                xdir = value
            elif code == 1:
                text = value
            elif code == 3:
                lines.append(value)
            elif code == 7:
                self.style = value
            elif code == 40:
                self.height = value
            elif code == 41:
                self.rect_width = value
            elif code == 42:
                self.horizontal_width = value
            elif code == 43:
                self.vertical_height = value
            elif code == 44:
                self.line_spacing = value
            elif code == 50:
                rotation = value
            elif code == 71:
                self.attachment_point = value
            else:
                yield code, value  # chain of generators

        lines.append(text)
        self.raw_text = "".join(lines)
        if xdir is None:
            xdir = deg2vec(rotation)
        self.xdirection = normalized(xdir)
        self.set_default_extrusion()

    def lines(self):
        return self.raw_text.split('\P')

    def plain_text(self, split=False):
        chars = []
        raw_chars = list(reversed(self.raw_text))  # text splitted into chars, in reversed order for efficient pop()
        while len(raw_chars):
            char = raw_chars.pop()
            if char == '\\':  # is a formatting command
                try:
                    char = raw_chars.pop()
                except IndexError:
                    break  # premature end of text - just ignore

                if char in ESCAPED_CHARS:  # \ { }
                    chars.append(char)
                elif char in ONE_CHAR_COMMANDS:
                    if char == 'P':  # new line
                        chars.append('\n')
                        # discard other commands
                else:  # more character commands are terminated by ';'
                    stacking = char == 'S'  # stacking command surrounds user data
                    try:
                        while char != ';':  # end of format marker
                            char = raw_chars.pop()
                            if stacking and char != ';':
                                chars.append(char)  # append user data of stacking command
                    except IndexError:
                        break  # premature end of text - just ignore
            elif char in GROUP_CHARS:  # { }
                pass  # discard group markers
            elif char == '%':  # special characters
                if len(raw_chars) and raw_chars[-1] == '%':
                    raw_chars.pop()  # discard next '%'
                    if len(raw_chars):
                        special_char = raw_chars.pop()
                        # replace or discard formatting code
                        chars.append(SPECIAL_CHARS.get(special_char, ""))
                else:  # char is just a single '%'
                    chars.append(char)
            else:  # char is what it is, a character
                chars.append(char)

        plain_text = "".join(chars)
        return plain_text.split('\n') if split else plain_text

    def resolve_text_style(self, text_styles):
        style = text_styles.get(self.style, None)
        if style is None:
            style = default_text_style
        if self.height == 0:
            self.height = style.height
        if self.font is None:
            self.font = style.font
        if self.big_font is None:
            self.big_font = style.font


class Light(DXFEntity):
    def __init__(self):
        super(Light, self).__init__()
        self.version = 1
        self.name = ""
        self.light_type = 1  # distant = 1; point = 2; spot = 3
        self.status = False  # on/off ?
        self.light_color = 0  # 0 is unset
        self.true_color = None  # None is unset
        self.plot_glyph = 0
        self.intensity = 0
        self.position = (0., 0., 1.)
        self.target = (0., 0., 0.)
        self.attenuation_type = 0  # 0 = None; 1 = Inverse Linear; 2 = Inverse Square
        self.use_attenuation_limits = False
        self.attenuation_start_limit = 0
        self.attenuation_end_limit = 0
        self.hotspot_angle = 0
        self.fall_off_angle = 0.
        self.cast_shadows = False
        self.shadow_type = 0  # 0 = Ray traced shadows; 1 = Shadow maps
        self.shadow_map_size = 0
        self.shadow_softness = 0

    def setup_attributes(self, tags):
        for code, value in super(Light, self).setup_attributes(tags):
            if code == 1:
                self.name = value
            elif code == 10:
                self.position = value
            elif code == 11:
                self.target = value
            elif code == 40:
                self.intensity = value
            elif code == 41:
                self.attenuation_start_limit = value
            elif code == 42:
                self.attenuation_end_limit = value
            elif code == 50:
                self.hotspot_angle = value
            elif code == 51:
                self.fall_off_angle = value
            elif code == 63:
                self.light_color = value
            elif code == 70:
                self.light_type = value
            elif code == 72:
                self.attenuation_type = value
            elif code == 73:
                self.shadow_type = value
            elif code == 90:
                self.version = value
            elif code == 91:
                self.shadow_map_size = value
            elif code == 280:
                self.shadow_softness = value
            elif code == 290:
                self.status = value
            elif code == 291:
                self.plot_glyph = value
            elif code == 292:
                self.use_attenuation_limits = value
            elif code == 293:
                self.cast_shadows = value
            elif code == 421:
                self.true_color = value
            else:
                yield code, value  # chain of generators


class Body(DXFEntity):
    def __init__(self):
        super(Body, self).__init__()
        # need handle to get SAB data in DXF version AC1027 and later
        self.version = 1
        self.acis = []

    def setup_attributes(self, tags):
        sat = []
        for code, value in super(Body, self).setup_attributes(tags):
            if code == 70:
                self.version = value
            elif code in (1, 3):
                sat.append(value)
            else:
                yield code, value  # chain of generators
        self.acis = decode(sat)

    def set_sab_data(self, sab_data):
        self.acis = sab_data

    @property
    def is_sat(self):
        return isinstance(self.acis, list)  # but could be an empty list

    @property
    def is_sab(self):
        return not self.is_sat  # has binary encoded ACIS data


class Surface(Body):
    def __init__(self):
        super(Body, self).__init__()
        self.u_isolines = 0
        self.v_isolines = 0

    def setup_attributes(self, tags):
        for code, value in super(Surface, self).setup_attributes(tags):
            if code == 71:
                self.u_isolines = value
            elif code == 72:
                self.v_isolines = value
            else:
                yield code, value  # chain of generators


class Mesh(DXFEntity):
    def __init__(self):
        super(Mesh, self).__init__()
        self.version = 2
        self.blend_crease = False
        self.subdivision_levels = 1
        # rest are mostly positional tags
        self.vertices = []
        self.faces = []
        self.edges = []
        self.edge_crease_list = []

    def setup_attributes(self, tags):
        status = 0
        count = 0
        index_tags = []
        for code, value in super(Mesh, self).setup_attributes(tags):
            if code == 10:
                self.vertices.append(value)
            elif status == -1:  # ignore overridden properties at the end of the mesh entity
                pass  # property override uses also group codes 90, 91, 92 but only at the end of the MESH entity
            elif code == 71:
                self.version = value
            elif code == 72:
                self.blend_crease = bool(value)
            elif code == 91:
                self.subdivision_levels = value
            elif 92 <= code <= 95:  # 92 = vertices, 93 = faces; 94 = edges 95 = edge creases
                if code in (92, 95):
                    continue  # ignore vertices and edge creases count
                status = code
                count = value
                if status == 94:  # edge count
                    count *= 2
            elif code == 140:
                self.edge_crease_list.append(value)
            elif code == 90 and count > 0:  # faces or edges
                count -= 1
                index_tags.append(value)
                if count < 1:
                    if status == 93:
                        self.setup_faces(index_tags)
                    elif status == 94:
                        self.setup_edges(index_tags)
                    index_tags = []
            elif code == 90:  # count == 0; start of overridden properties (group code 90 after face or edge list)
                status = -1
            else:
                yield code, value  # chain of generators

    def get_face(self, index):
        return tuple(self.vertices[vertex_index] for vertex_index in self.faces[index])

    def get_edge(self, index):
        return tuple(self.vertices[vertex_index] for vertex_index in self.edges[index])

    def setup_faces(self, tags):
        face = []
        count = 0
        for value in tags:
            if count == 0:
                if len(face):
                    self.faces.append(tuple(face))
                    del face[:]
                count = value
            else:
                count -= 1
                face.append(value)

        if len(face):
            self.faces.append(tuple(face))

    def setup_edges(self, tags):
        self.edges = list(zip(tags[::2], tags[1::2]))


class Spline(DXFEntity):
    def __init__(self):
        super(Spline, self).__init__()
        self.normal_vector = None
        self.flags = 0
        self.degree = 3
        self.start_tangent = None
        self.end_tangent = None
        self.knots = []
        self.weights = []
        self.tol_knot = .0000001
        self.tol_control_point = .0000001
        self.tol_fit_point = .0000000001
        self.control_points = []
        self.fit_points = []

    def setup_attributes(self, tags):
        subclass = 'AcDbSpline'
        for code, value in super(Spline, self).setup_attributes(tags):
            if subclass == 'AcDbHelix':
                yield code, value # chain of generators
            elif code == 10:
                self.control_points.append(value)
            elif code == 11:
                self.fit_points.append(value)
            elif code == 12:
                self.start_tangent = value
            elif code == 13:
                self.end_tangent = value
            elif code == 40:
                self.knots.append(value)
            elif code == 41:
                self.weights.append(value)
            elif code == 42:
                self.tol_knot = value
            elif code == 43:
                self.tol_control_point = value
            elif code == 44:
                self.tol_fit_point = value
            elif code == 70:
                self.flags = value
            elif code == 71:
                self.degree = value
            elif 72 <= code < 75:
                pass  # ignore knot-, control- and fit point count
            elif code == 100:
                subclass = value
        self.normal_vector = self.extrusion
        if len(self.weights) == 0:
            self.weights = [1.0] * len(self.control_points)

    @property
    def is_closed(self):
        return bool(self.flags & const.SPLINE_CLOSED)

    @property
    def is_periodic(self):
        return bool(self.flags & const.SPLINE_PERIODIC)

    @property
    def is_rational(self):
        return bool(self.flags & const.SPLINE_RATIONAL)

    @property
    def is_planar(self):
        return bool(self.flags & const.SPLINE_PLANAR)

    @property
    def is_linear(self):
        return bool(self.flags & const.SPLINE_LINEAR)


class Helix(Spline):
    def __init__(self):
        super(Helix, self).__init__()
        self.helix_version = (1, 1)
        self.axis_base_point = None
        self.start_point = None
        self.axis_vector = None
        self.radius = 0
        self.turns = 0
        self.turn_height = 0
        self.handedness = 0  # 0 = left, 1 = right
        self.constrain = 0
        # 0 = Constrain turn height;
        # 1 = Constrain turns;
        # 2 = Constrain height

    def setup_attributes(self, tags):
        helix_major_version = 1
        helix_maintainance_version = 1
        for code, value in super(Helix, self).setup_attributes(tags):
            if code == 10:
                self.axis_base_point = value
            elif code == 11:
                self.start_point = value
            elif code == 12:
                self.axis_vector = value
            elif code == 90:
                helix_major_version = value
            elif code == 91:
                helix_maintainance_version = value
            elif code == 91:
                helix_maintainance_version = value
            elif code == 40:
                self.radius = value
            elif code == 41:
                self.turns = value
            elif code == 42:
                self.turn_height = value
            elif code == 290:
                self.handedness = value
            elif code == 280:
                self.constrain = value
            else:
                yield code, value  # chain of generators
        self.helix_version = (helix_major_version, helix_maintainance_version)


EntityTable = {
    'LINE': Line,
    'POINT': Point,
    'CIRCLE': Circle,
    'ARC': Arc,
    'TRACE': Trace,
    'SOLID': Solid,
    '3DFACE': Face,
    'TEXT': Text,
    'INSERT': Insert,
    'SEQEND': DXFEntity,
    'ATTRIB': Attrib,
    'ATTDEF': Attrib,
    'POLYLINE': Polyline,
    'VERTEX': Vertex,
    'BLOCK': Block,
    'ENDBLK': DXFEntity,
    'LWPOLYLINE': LWPolyline,
    'ELLIPSE': Ellipse,
    'RAY': Ray,
    'XLINE': Ray,
    'SPLINE': Spline,
    'HELIX': Helix,
    'MTEXT': MText,
    'MESH': Mesh,
    'LIGHT': Light,
    'BODY': Body,
    'REGION': Body,
    '3DSOLID': Body,
    'SURFACE': Surface,
    'PLANESURFACE': Surface,
}


def entity_factory(tags):
    dxftype = tags.get_type()
    cls = EntityTable[dxftype]  # get entity class or raise KeyError
    entity = cls()  # call constructor
    list(entity.setup_attributes(tags))  # setup dxf attributes - chain of generators
    return entity

