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

# <pep8 compliant>


import bpy
import xml.etree.cElementTree as et

from bpy.path import abspath
from bpy.app.handlers import persistent
from bpy_extras.object_utils import world_to_camera_view

from freestyle.types import StrokeShader, ChainingIterator, BinaryPredicate1D, Interface0DIterator, AdjacencyIterator
from freestyle.utils import getCurrentScene, get_dashed_pattern, get_test_stroke
from freestyle.functions import GetShapeF1D, CurveMaterialF0D

from itertools import dropwhile, repeat
from collections import OrderedDict

__all__ = (
    "SVGPathShader",
    "SVGFillShader",
    "ShapeZ",
    "indent_xml",
    "svg_export_header",
    "svg_export_animation",
    )

# register namespaces
et.register_namespace("", "http://www.w3.org/2000/svg")
et.register_namespace("inkscape", "http://www.inkscape.org/namespaces/inkscape")
et.register_namespace("sodipodi", "http://sodipodi.sourceforge.net/DTD/sodipodi-0.dtd")


# use utf-8 here to keep ElementTree happy
svg_primitive = """<?xml version="1.0" encoding="UTF-8" standalone="no"?>
<svg xmlns="http://www.w3.org/2000/svg" version="1.1" width="{:d}" height="{:d}">
</svg>"""


# xml namespaces
namespaces = {
    "inkscape": "http://www.inkscape.org/namespaces/inkscape",
    "svg": "http://www.w3.org/2000/svg",
    }

# - SVG export - #
class SVGPathShader(StrokeShader):
    """Stroke Shader for writing stroke data to a .svg file."""
    def __init__(self, name, style, filepath, res_y, split_at_invisible, frame_current):
        StrokeShader.__init__(self)
        # attribute 'name' of 'StrokeShader' objects is not writable, so _name is used
        self._name = name
        self.filepath = filepath
        self.h = res_y
        self.frame_current = frame_current
        self.elements = []
        self.split_at_invisible = split_at_invisible
        # put style attributes into a single svg path definition
        self.path = '\n<path ' + "".join('{}="{}" '.format(k, v) for k, v in style.items()) + 'd=" M '

    @classmethod
    def from_lineset(cls, lineset, filepath, res_y, split_at_invisible, frame_current, *, name=""):
        """Builds a SVGPathShader using data from the given lineset"""
        name = name or lineset.name
        linestyle = lineset.linestyle
        # extract style attributes from the linestyle
        style = {
            'fill': 'none',
            'stroke-width': linestyle.thickness,
            'stroke-linecap': linestyle.caps.lower(),
            'stroke-opacity': linestyle.alpha,
            'stroke': 'rgb({}, {}, {})'.format(*(int(c * 255) for c in linestyle.color))
            }
        # get dashed line pattern (if specified)
        if linestyle.use_dashed_line:
            style['stroke-dasharray'] = ",".join(str(elem) for elem in get_dashed_pattern(linestyle))
        # return instance
        return cls(name, style, filepath, res_y, split_at_invisible, frame_current)

    @staticmethod
    def pathgen(stroke, path, height, split_at_invisible, f=lambda v: not v.attribute.visible):
        """Generator that creates SVG paths (as strings) from the current stroke """
        it = iter(stroke)
        # start first path
        yield path
        for v in it:
            x, y = v.point
            yield '{:.3f}, {:.3f} '.format(x, height - y)
            if split_at_invisible and v.attribute.visible == False:
                # end current and start new path;
                yield '" />' + path
                # fast-forward till the next visible vertex
                it = dropwhile(f, it)
                # yield next visible vertex
                svert = next(it, None)
                if svert is None:
                    break
                x, y = svert.point
                yield '{:.3f}, {:.3f} '.format(x, height - y)
        # close current path
        yield '" />'

    def shade(self, stroke):
        stroke_to_paths = "".join(self.pathgen(stroke, self.path, self.h, self.split_at_invisible)).split("\n")
        # convert to actual XML, check to prevent empty paths
        self.elements.extend(et.XML(elem) for elem in stroke_to_paths if len(elem.strip()) > len(self.path))

    def write(self):
        """Write SVG data tree to file """
        tree = et.parse(self.filepath)
        root = tree.getroot()
        name = self._name

        # make <g> for lineset as a whole (don't overwrite)
        lineset_group = tree.find(".//svg:g[@id='{}']".format(name), namespaces=namespaces)
        if lineset_group is None:
            lineset_group = et.XML('<g/>')
            lineset_group.attrib = {
                'id': name,
                'xmlns:inkscape': namespaces["inkscape"],
                'inkscape:groupmode': 'lineset',
                'inkscape:label': name,
                }
            root.insert(0, lineset_group)

        # make <g> for the current frame
        id = "{}_frame_{:06n}".format(name, self.frame_current)
        frame_group = et.XML("<g/>")
        frame_group.attrib = {'id': id, 'inkscape:groupmode': 'frame', 'inkscape:label': id}
        frame_group.extend(self.elements)
        lineset_group.append(frame_group)

        # write SVG to file
        indent_xml(root)
        tree.write(self.filepath, encoding='UTF-8', xml_declaration=True)

# - Fill export - #
class ShapeZ(BinaryPredicate1D):
    """Sort ViewShapes by their z-index"""
    def __init__(self, scene):
        BinaryPredicate1D.__init__(self)
        self.z_map = dict()
        self.scene = scene

    def __call__(self, i1, i2):
        return self.get_z_curve(i1) < self.get_z_curve(i2)

    def get_z_curve(self, curve, func=GetShapeF1D()):
        shape = func(curve)[0]
        # get the shapes z-index
        z = self.z_map.get(shape.id.first)
        if z is None:
            o = bpy.data.objects[shape.name]
            z = world_to_camera_view(self.scene, self.scene.camera, o.location).z
            self.z_map[shape.id.first] = z
        return z


class SVGFillShader(StrokeShader):
    """Creates SVG fills from the current stroke set"""
    def __init__(self, filepath, height, name):
        StrokeShader.__init__(self)
        # use an ordered dict to maintain input and z-order
        self.shape_map = OrderedDict()
        self.filepath = filepath
        self.h = height
        self._name = name

    def shade(self, stroke, func=GetShapeF1D(), curvemat=CurveMaterialF0D()):
        shape = func(stroke)[0]
        shape = shape.id.first
        item = self.shape_map.get(shape)
        if len(stroke) > 2:
            if item is not None:
                item[0].append(stroke)
            else:
                # the shape is not yet present, let's create it.
                material = curvemat(Interface0DIterator(stroke))
                *color, alpha = material.diffuse
                self.shape_map[shape] = ([stroke], color, alpha)
        # make the strokes of the second drawing invisible
        for v in stroke:
            v.attribute.visible = False

    @staticmethod
    def pathgen(vertices, path, height):
        yield path
        for point in vertices:
            x, y = point
            yield '{:.3f}, {:.3f} '.format(x, height - y)
        yield 'z" />' # closes the path; connects the current to the first point

    def write(self):
        """Write SVG data tree to file """
        # initialize SVG
        tree = et.parse(self.filepath)
        root = tree.getroot()
        name = self._name

        # create XML elements from the acquired data
        elems = []
        path = '<path fill-rule="evenodd" stroke="none" fill-opacity="{}" fill="rgb({}, {}, {})"  d=" M '
        for strokes, col, alpha in self.shape_map.values():
            p = path.format(alpha, *(int(255 * c) for c in col))
            for stroke in strokes:
                elems.append(et.XML("".join(self.pathgen((sv.point for sv in stroke), p, self.h))))

        # make <g> for lineset as a whole (don't overwrite)
        lineset_group = tree.find(".//svg:g[@id='{}']".format(name), namespaces=namespaces)
        if lineset_group is None:
            lineset_group = et.XML('<g/>')
            lineset_group.attrib = {
                'id': name,
                'xmlns:inkscape': namespaces["inkscape"],
                'inkscape:groupmode': 'lineset',
                'inkscape:label': name,
                }
            root.insert(0, lineset_group)

        # make <g> for fills
        frame_group = et.XML('<g />')
        frame_group.attrib = {'id': "layer_fills", 'inkscape:groupmode': 'fills', 'inkscape:label': 'fills'}
        # reverse the elements so they are correctly ordered in the image
        frame_group.extend(reversed(elems))
        lineset_group.insert(0, frame_group)

        # write SVG to file
        indent_xml(root)
        tree.write(self.filepath, encoding='UTF-8', xml_declaration=True)


def indent_xml(elem, level=0, indentsize=4):
    """Prettifies XML code (used in SVG exporter) """
    i = "\n" + level * " " * indentsize
    if len(elem):
        if not elem.text or not elem.text.strip():
            elem.text = i + " " * indentsize
        if not elem.tail or not elem.tail.strip():
            elem.tail = i
        for elem in elem:
            indent_xml(elem, level + 1)
        if not elem.tail or not elem.tail.strip():
            elem.tail = i
    elif level and (not elem.tail or not elem.tail.strip()):
        elem.tail = i

# - callbacks - #
@persistent
def svg_export_header(scene):
    render = scene.render
    if not (render.use_freestyle and render.use_svg_export):
        return
    #create new file (overwrite existing)
    width, height = render.resolution_x, render.resolution_y
    scale = render.resolution_percentage / 100

    try:
        with open(abspath(render.svg_path), "w") as f:
            f.write(svg_primitive.format(int(width * scale), int(height * scale)))
    except:
        # invalid path is properly handled in the parameter editor
        print("SVG export: invalid path")

@persistent
def svg_export_animation(scene):
    """makes an animation of the exported SVG file """
    render = scene.render
    if render.use_freestyle and render.use_svg_export and render.svg_mode == 'ANIMATION':
        write_animation(abspath(render.svg_path), scene.frame_start, render.fps)


def write_animation(filepath, frame_begin, fps=25):
    """Adds animate tags to the specified file."""
    tree = et.parse(filepath)
    root = tree.getroot()

    linesets = tree.findall(".//svg:g[@inkscape:groupmode='lineset']", namespaces=namespaces)
    for i, lineset in enumerate(linesets):
        name = lineset.get('id')
        frames = lineset.findall(".//svg:g[@inkscape:groupmode='frame']", namespaces=namespaces)
        fills = lineset.findall(".//svg:g[@inkscape:groupmode='fills']", namespaces=namespaces)
        fills = reversed(fills) if fills else repeat(None, len(frames))

        n_of_frames = len(frames)
        keyTimes = ";".join(str(round(x / n_of_frames, 3)) for x in range(n_of_frames)) + ";1"

        style = {
            'attributeName': 'display',
            'values': "none;" * (n_of_frames - 1) + "inline;none",
            'repeatCount': 'indefinite',
            'keyTimes': keyTimes,
            'dur': str(n_of_frames / fps) + 's',
            }

        for j, (frame, fill) in enumerate(zip(frames, fills)):
            id = 'anim_{}_{:06n}'.format(name, j + frame_begin)
            # create animate tag
            frame_anim = et.XML('<animate id="{}" begin="{}s" />'.format(id, (j - n_of_frames) / fps))
            # add per-lineset style attributes
            frame_anim.attrib.update(style)
            # add to the current frame
            frame.append(frame_anim)
            # append the animation to the associated fill as well (if valid)
            if fill is not None:
                fill.append(frame_anim)

    # write SVG to file
    indent_xml(root)
    tree.write(filepath, encoding='UTF-8', xml_declaration=True)
