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

bl_info = {
    "name": "Freestyle SVG Exporter",
    "author": "Folkert de Vries",
    "version": (1, 0),
    "blender": (2, 72, 1),
    "location": "Properties > Render > Freestyle SVG Export",
    "description": "Exports Freestyle's stylized edges in SVG format",
    "warning": "",
    "wiki_url": "",
    "category": "Render",
    }

import bpy
import parameter_editor
import itertools
import os

import xml.etree.cElementTree as et

from bpy.app.handlers import persistent
from collections import OrderedDict
from functools import partial
from mathutils import Vector

from freestyle.types import (
        StrokeShader,
        Interface0DIterator,
        Operators,
        Nature,
        StrokeVertex,
        )
from freestyle.utils import (
    getCurrentScene,
    BoundingBox,
    is_poly_clockwise,
    StrokeCollector,
    material_from_fedge,
    get_object_name,
    )
from freestyle.functions import (
    GetShapeF1D,
    CurveMaterialF0D,
    )
from freestyle.predicates import (
        AndBP1D,
        AndUP1D,
        ContourUP1D,
        ExternalContourUP1D,
        MaterialBP1D,
        NotBP1D,
        NotUP1D,
        OrBP1D,
        OrUP1D,
        pyNatureUP1D,
        pyZBP1D,
        pyZDiscontinuityBP1D,
        QuantitativeInvisibilityUP1D,
        SameShapeIdBP1D,
        TrueBP1D,
        TrueUP1D,
        )
from freestyle.chainingiterators import ChainPredicateIterator
from parameter_editor import get_dashed_pattern

from bpy.props import (
        BoolProperty,
        EnumProperty,
        PointerProperty,
        )


# use utf-8 here to keep ElementTree happy, end result is utf-16
svg_primitive = """<?xml version="1.0" encoding="ascii" standalone="no"?>
<!DOCTYPE svg PUBLIC "-//W3C//DTD SVG 1.1//EN"
  "http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd">
<svg xmlns="http://www.w3.org/2000/svg" version="1.1" width="{:d}" height="{:d}">
</svg>"""


# xml namespaces
namespaces = {
    "inkscape": "http://www.inkscape.org/namespaces/inkscape",
    "svg": "http://www.w3.org/2000/svg",
    "sodipodi": "http://sodipodi.sourceforge.net/DTD/sodipodi-0.dtd",
    "": "http://www.w3.org/2000/svg",
    }


# wrap XMLElem.find, so the namespaces don't need to be given as an argument
def find_xml_elem(obj, search, namespaces, *, all=False):
    if all:
        return obj.findall(search, namespaces=namespaces)
    return obj.find(search, namespaces=namespaces)

find_svg_elem = partial(find_xml_elem, namespaces=namespaces)


def render_height(scene):
    return int(scene.render.resolution_y * scene.render.resolution_percentage / 100)


def render_width(scene):
    return int(scene.render.resolution_x * scene.render.resolution_percentage / 100)


def format_rgb(color):
    return 'rgb({}, {}, {})'.format(*(int(v * 255) for v in color))


# stores the state of the render, used to differ between animation and single frame renders.
class RenderState:

    # Note that this flag is set to False only after the first frame
    # has been written to file.
    is_preview = True


@persistent
def render_init(scene):
    RenderState.is_preview = True


@persistent
def render_write(scene):
    RenderState.is_preview = False


def is_preview_render(scene):
    return RenderState.is_preview or scene.svg_export.mode == 'FRAME'


def create_path(scene):
    """Creates the output path for the svg file"""
    path = os.path.dirname(scene.render.frame_path())
    file_dir_path = os.path.dirname(bpy.data.filepath)

    # try to use the given path if it is absolute
    if os.path.isabs(path):
        dirname = path

    # otherwise, use current file's location as a start for the relative path
    elif bpy.data.is_saved and file_dir_path:
        dirname = os.path.normpath(os.path.join(file_dir_path, path))

    # otherwise, use the folder from which blender was called as the start
    else:
        dirname = os.path.abspath(bpy.path.abspath(path))


    basename = bpy.path.basename(scene.render.filepath)
    if scene.svg_export.mode == 'FRAME':
        frame = "{:04d}".format(scene.frame_current)
    else:
        frame = "{:04d}-{:04d}".format(scene.frame_start, scene.frame_end)

    return os.path.join(dirname, basename + frame + ".svg")


class SVGExporterLinesetPanel(bpy.types.Panel):
    """Creates a Panel in the Render Layers context of the properties editor"""
    bl_idname = "RENDER_PT_SVGExporterLinesetPanel"
    bl_space_type = 'PROPERTIES'
    bl_label = "Freestyle Line Style SVG Export"
    bl_region_type = 'WINDOW'
    bl_context = "render_layer"

    def draw(self, context):
        layout = self.layout

        scene = context.scene
        svg = scene.svg_export
        freestyle = scene.render.layers.active.freestyle_settings

        try:
            linestyle = freestyle.linesets.active.linestyle

        except AttributeError:
            # Linestyles can be removed, so 0 linestyles is possible.
            # there is nothing to draw in those cases.
            # see https://developer.blender.org/T49855
            return

        else:
            layout.active = (svg.use_svg_export and freestyle.mode != 'SCRIPT')
            row = layout.row()
            column = row.column()
            column.prop(linestyle, 'use_export_strokes')

            column = row.column()
            column.active = svg.object_fill
            column.prop(linestyle, 'use_export_fills')

            row = layout.row()
            row.prop(linestyle, "stroke_color_mode", expand=True)


class SVGExport(bpy.types.PropertyGroup):
    """Implements the properties for the SVG exporter"""
    bl_idname = "RENDER_PT_svg_export"

    use_svg_export = BoolProperty(
            name="SVG Export",
            description="Export Freestyle edges to an .svg format",
            )
    split_at_invisible = BoolProperty(
            name="Split at Invisible",
            description="Split the stroke at an invisible vertex",
            )
    object_fill = BoolProperty(
            name="Fill Contours",
            description="Fill the contour with the object's material color",
            )
    mode = EnumProperty(
            name="Mode",
            items=(
                ('FRAME', "Frame", "Export a single frame", 0),
                ('ANIMATION', "Animation", "Export an animation", 1),
                ),
            default='FRAME',
            )
    line_join_type = EnumProperty(
            name="Linejoin",
            items=(
                ('MITTER', "Mitter", "Corners are sharp", 0),
                ('ROUND', "Round", "Corners are smoothed", 1),
                ('BEVEL', "Bevel", "Corners are bevelled", 2),
                ),
            default='ROUND',
            )


class SVGExporterPanel(bpy.types.Panel):
    """Creates a Panel in the render context of the properties editor"""
    bl_idname = "RENDER_PT_SVGExporterPanel"
    bl_space_type = 'PROPERTIES'
    bl_label = "Freestyle SVG Export"
    bl_region_type = 'WINDOW'
    bl_context = "render"

    def draw_header(self, context):
        self.layout.prop(context.scene.svg_export, "use_svg_export", text="")

    def draw(self, context):
        layout = self.layout

        scene = context.scene
        svg = scene.svg_export
        freestyle = scene.render.layers.active.freestyle_settings

        layout.active = (svg.use_svg_export and freestyle.mode != 'SCRIPT')

        row = layout.row()
        row.prop(svg, "mode", expand=True)

        row = layout.row()
        row.prop(svg, "split_at_invisible")
        row.prop(svg, "object_fill")

        row = layout.row()
        row.prop(svg, "line_join_type", expand=True)


@persistent
def svg_export_header(scene):
    if not (scene.render.use_freestyle and scene.svg_export.use_svg_export):
        return

    # write the header only for the first frame when animation is being rendered
    if not is_preview_render(scene) and scene.frame_current != scene.frame_start:
        return

    # this may fail still. The error is printed to the console.
    with open(create_path(scene), "w") as f:
        f.write(svg_primitive.format(render_width(scene), render_height(scene)))


@persistent
def svg_export_animation(scene):
    """makes an animation of the exported SVG file """
    render = scene.render
    svg = scene.svg_export

    if render.use_freestyle and svg.use_svg_export and not is_preview_render(scene):
        write_animation(create_path(scene), scene.frame_start, render.fps)


def write_animation(filepath, frame_begin, fps):
    """Adds animate tags to the specified file."""
    tree = et.parse(filepath)
    root = tree.getroot()

    linesets = find_svg_elem(tree, ".//svg:g[@inkscape:groupmode='lineset']", all=True)
    for i, lineset in enumerate(linesets):
        name = lineset.get('id')
        frames = find_svg_elem(lineset, ".//svg:g[@inkscape:groupmode='frame']", all=True)
        n_of_frames = len(frames)
        keyTimes = ";".join(str(round(x / n_of_frames, 3)) for x in range(n_of_frames)) + ";1"

        style = {
            'attributeName': 'display',
            'values': "none;" * (n_of_frames - 1) + "inline;none",
            'repeatCount': 'indefinite',
            'keyTimes': keyTimes,
            'dur': "{:.3f}s".format(n_of_frames / fps),
            }

        for j, frame in enumerate(frames):
            id = 'anim_{}_{:06n}'.format(name, j + frame_begin)
            # create animate tag
            frame_anim = et.XML('<animate id="{}" begin="{:.3f}s" />'.format(id, (j - n_of_frames) / fps))
            # add per-lineset style attributes
            frame_anim.attrib.update(style)
            # add to the current frame
            frame.append(frame_anim)

    # write SVG to file
    indent_xml(root)
    tree.write(filepath, encoding='ascii', xml_declaration=True)


# - StrokeShaders - #
class SVGPathShader(StrokeShader):
    """Stroke Shader for writing stroke data to a .svg file."""
    def __init__(self, name, style, filepath, res_y, split_at_invisible, stroke_color_mode, frame_current):
        StrokeShader.__init__(self)
        # attribute 'name' of 'StrokeShader' objects is not writable, so _name is used
        self._name = name
        self.filepath = filepath
        self.h = res_y
        self.frame_current = frame_current
        self.elements = []
        self.split_at_invisible = split_at_invisible
        self.stroke_color_mode = stroke_color_mode # BASE | FIRST | LAST
        self.style = style


    @classmethod
    def from_lineset(cls, lineset, filepath, res_y, split_at_invisible, use_stroke_color, frame_current, *, name=""):
        """Builds a SVGPathShader using data from the given lineset"""
        name = name or lineset.name
        linestyle = lineset.linestyle
        # extract style attributes from the linestyle and scene
        svg = getCurrentScene().svg_export
        style = {
            'fill': 'none',
            'stroke-width': linestyle.thickness,
            'stroke-linecap': linestyle.caps.lower(),
            'stroke-opacity': linestyle.alpha,
            'stroke': format_rgb(linestyle.color),
            'stroke-linejoin': svg.line_join_type.lower(),
            }
        # get dashed line pattern (if specified)
        if linestyle.use_dashed_line:
            style['stroke-dasharray'] = ",".join(str(elem) for elem in get_dashed_pattern(linestyle))
        # return instance
        return cls(name, style, filepath, res_y, split_at_invisible, use_stroke_color, frame_current)


    @staticmethod
    def pathgen(stroke, style, height, split_at_invisible, stroke_color_mode, f=lambda v: not v.attribute.visible):
        """Generator that creates SVG paths (as strings) from the current stroke """
        if len(stroke) <= 1:
            return ""

        if stroke_color_mode != 'BASE':
            # try to use the color of the first or last vertex
            try:
                index = 0 if stroke_color_mode == 'FIRST' else -1
                color = format_rgb(stroke[index].attribute.color)
                style["stroke"] = color
            except (ValueError, IndexError):
                # default is linestyle base color
                pass

        # put style attributes into a single svg path definition
        path = '\n<path ' + "".join('{}="{}" '.format(k, v) for k, v in style.items()) + 'd=" M '

        it = iter(stroke)
        # start first path
        yield path
        for v in it:
            x, y = v.point
            yield '{:.3f}, {:.3f} '.format(x, height - y)
            if split_at_invisible and v.attribute.visible is False:
                # end current and start new path;
                yield '" />' + path
                # fast-forward till the next visible vertex
                it = itertools.dropwhile(f, it)
                # yield next visible vertex
                svert = next(it, None)
                if svert is None:
                    break
                x, y = svert.point
                yield '{:.3f}, {:.3f} '.format(x, height - y)
        # close current path
        yield '" />'

    def shade(self, stroke):
        stroke_to_paths = "".join(self.pathgen(stroke, self.style, self.h, self.split_at_invisible, self.stroke_color_mode)).split("\n")
        # convert to actual XML. Empty strokes are empty strings; they are ignored.
        self.elements.extend(et.XML(elem) for elem in stroke_to_paths if elem) # if len(elem.strip()) > len(self.path))

    def write(self):
        """Write SVG data tree to file """
        tree = et.parse(self.filepath)
        root = tree.getroot()
        name = self._name
        scene = bpy.context.scene

        # create <g> for lineset as a whole (don't overwrite)
        # when rendering an animation, frames will be nested in here, otherwise a group of strokes and optionally fills.
        lineset_group = find_svg_elem(tree, ".//svg:g[@id='{}']".format(name))
        if lineset_group is None:
            lineset_group = et.XML('<g/>')
            lineset_group.attrib = {
                'id': name,
                'xmlns:inkscape': namespaces["inkscape"],
                'inkscape:groupmode': 'lineset',
                'inkscape:label': name,
                }
            root.append(lineset_group)

        # create <g> for the current frame
        id = "frame_{:04n}".format(self.frame_current)

        stroke_group = et.XML("<g/>")
        stroke_group.attrib = {
            'xmlns:inkscape': namespaces["inkscape"],
            'inkscape:groupmode': 'layer',
            'id': 'strokes',
            'inkscape:label': 'strokes'
            }
        # nest the structure
        stroke_group.extend(self.elements)
        if scene.svg_export.mode == 'ANIMATION':
            frame_group = et.XML("<g/>")
            frame_group.attrib = {'id': id, 'inkscape:groupmode': 'frame', 'inkscape:label': id}
            frame_group.append(stroke_group)
            lineset_group.append(frame_group)
        else:
            lineset_group.append(stroke_group)

        # write SVG to file
        print("SVG Export: writing to", self.filepath)
        indent_xml(root)
        tree.write(self.filepath, encoding='ascii', xml_declaration=True)


class SVGFillBuilder:
    def __init__(self, filepath, height, name):
        self.filepath = filepath
        self._name = name
        self.stroke_to_fill = partial(self.stroke_to_svg, height=height)

    @staticmethod
    def pathgen(vertices, path, height):
        yield path
        for point in vertices:
            x, y = point
            yield '{:.3f}, {:.3f} '.format(x, height - y)
        yield ' z" />'  # closes the path; connects the current to the first point


    @staticmethod
    def get_merged_strokes(strokes):
        def extend_stroke(stroke, vertices):
            for vert in map(StrokeVertex, vertices):
                stroke.insert_vertex(vert, stroke.stroke_vertices_end())
            return stroke

        base_strokes = tuple(stroke for stroke in strokes if not is_poly_clockwise(stroke))
        merged_strokes = OrderedDict((s, list()) for s in base_strokes)

        for stroke in filter(is_poly_clockwise, strokes):
            for base in base_strokes:
                # don't merge when diffuse colors don't match
                if diffuse_from_stroke(stroke) != diffuse_from_stroke(stroke):
                    continue
                # only merge when the 'hole' is inside the base
                elif stroke_inside_stroke(stroke, base):
                    merged_strokes[base].append(stroke)
                    break
                # if it isn't a hole, it is likely that there are two strokes belonging
                # to the same object separated by another object. let's try to join them
                elif (get_object_name(base) == get_object_name(stroke) and
                      diffuse_from_stroke(stroke) == diffuse_from_stroke(stroke)):
                    base = extend_stroke(base, (sv for sv in stroke))
                    break
            else:
                # if all else fails, treat this stroke as a base stroke
                merged_strokes.update({stroke:  []})
        return merged_strokes


    def stroke_to_svg(self, stroke, height, parameters=None):
        if parameters is None:
            *color, alpha = diffuse_from_stroke(stroke)
            color = tuple(int(255 * c) for c in color)
            parameters = {
                'fill_rule': 'evenodd',
                'stroke': 'none',
                'fill-opacity': alpha,
                'fill': 'rgb' + repr(color),
            }
        param_str = " ".join('{}="{}"'.format(k, v) for k, v in parameters.items())
        path = '<path {} d=" M '.format(param_str)
        vertices = (svert.point for svert in stroke)
        s = "".join(self.pathgen(vertices, path, height))
        result = et.XML(s)
        return result

    def create_fill_elements(self, strokes):
        """Creates ElementTree objects by merging stroke objects together and turning them into SVG paths."""
        merged_strokes = self.get_merged_strokes(strokes)
        for k, v in merged_strokes.items():
            base = self.stroke_to_fill(k)
            fills = (self.stroke_to_fill(stroke).get("d") for stroke in v)
            merged_points = " ".join(fills)
            base.attrib['d'] += merged_points
            yield base

    def write(self, strokes):
        """Write SVG data tree to file """

        tree = et.parse(self.filepath)
        root = tree.getroot()
        scene = bpy.context.scene
        name = self._name

        lineset_group = find_svg_elem(tree, ".//svg:g[@id='{}']".format(self._name))
        if lineset_group is None:
            lineset_group = et.XML('<g/>')
            lineset_group.attrib = {
                'id': name,
                'xmlns:inkscape': namespaces["inkscape"],
                'inkscape:groupmode': 'lineset',
                'inkscape:label': name,
                }
            root.append(lineset_group)
            print('added new lineset group ', name)


        # <g> for the fills of the current frame
        fill_group = et.XML('<g/>')
        fill_group.attrib = {
            'xmlns:inkscape': namespaces["inkscape"],
            'inkscape:groupmode': 'layer',
            'inkscape:label': 'fills',
            'id': 'fills'
           }

        fill_elements = self.create_fill_elements(strokes)
        fill_group.extend(reversed(tuple(fill_elements)))
        if scene.svg_export.mode == 'ANIMATION':
            # add the fills to the <g> of the current frame
            frame_group = find_svg_elem(lineset_group, ".//svg:g[@id='frame_{:04n}']".format(scene.frame_current))
            frame_group.insert(0, fill_group)
        else:
            lineset_group.insert(0, fill_group)

        # write SVG to file
        indent_xml(root)
        tree.write(self.filepath, encoding='ascii', xml_declaration=True)


def stroke_inside_stroke(a, b):
    box_a = BoundingBox.from_sequence(svert.point for svert in a)
    box_b = BoundingBox.from_sequence(svert.point for svert in b)
    return box_a.inside(box_b)


def diffuse_from_stroke(stroke, curvemat=CurveMaterialF0D()):
    material = curvemat(Interface0DIterator(stroke))
    return material.diffuse

# - Callbacks - #
class ParameterEditorCallback(object):
    """Object to store callbacks for the Parameter Editor in"""
    def lineset_pre(self, scene, layer, lineset):
        raise NotImplementedError()

    def modifier_post(self, scene, layer, lineset):
        raise NotImplementedError()

    def lineset_post(self, scene, layer, lineset):
        raise NotImplementedError()



class SVGPathShaderCallback(ParameterEditorCallback):
    @classmethod
    def poll(cls, scene, linestyle):
        return scene.render.use_freestyle and scene.svg_export.use_svg_export and linestyle.use_export_strokes

    @classmethod
    def modifier_post(cls, scene, layer, lineset):
        if not cls.poll(scene, lineset.linestyle):
            return []

        split = scene.svg_export.split_at_invisible
        stroke_color_mode = lineset.linestyle.stroke_color_mode
        cls.shader = SVGPathShader.from_lineset(
                lineset, create_path(scene),
                render_height(scene), split, stroke_color_mode, scene.frame_current, name=layer.name + '_' + lineset.name)
        return [cls.shader]

    @classmethod
    def lineset_post(cls, scene, layer, lineset):
        if not cls.poll(scene, lineset.linestyle):
            return []
        cls.shader.write()


class SVGFillShaderCallback(ParameterEditorCallback):
    @classmethod
    def poll(cls, scene, linestyle):
        return scene.render.use_freestyle and scene.svg_export.use_svg_export and scene.svg_export.object_fill and linestyle.use_export_fills

    @classmethod
    def lineset_post(cls, scene, layer, lineset):
        if not cls.poll(scene, lineset.linestyle):
            return

        # reset the stroke selection (but don't delete the already generated strokes)
        Operators.reset(delete_strokes=False)
        # Unary Predicates: visible and correct edge nature
        upred = AndUP1D(
            QuantitativeInvisibilityUP1D(0),
            OrUP1D(ExternalContourUP1D(),
                   pyNatureUP1D(Nature.BORDER)),
            )
        # select the new edges
        Operators.select(upred)
        # Binary Predicates
        bpred = AndBP1D(
            MaterialBP1D(),
            NotBP1D(pyZDiscontinuityBP1D()),
            )
        bpred = OrBP1D(bpred, AndBP1D(NotBP1D(bpred), AndBP1D(SameShapeIdBP1D(), MaterialBP1D())))
        # chain the edges
        Operators.bidirectional_chain(ChainPredicateIterator(upred, bpred))
        # export SVG
        collector = StrokeCollector()
        Operators.create(TrueUP1D(), [collector])

        builder = SVGFillBuilder(create_path(scene), render_height(scene), layer.name + '_' + lineset.name)
        builder.write(collector.strokes)
        # make strokes used for filling invisible
        for stroke in collector.strokes:
            for svert in stroke:
                svert.attribute.visible = False



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


def register_namespaces(namespaces=namespaces):
    for name, url in namespaces.items():
        if name != 'svg': # creates invalid xml
            et.register_namespace(name, url)

@persistent
def handle_versions(self):
    # We don't modify startup file because it assumes to
    # have all the default values only.
    if not bpy.data.is_saved:
        return

    # Revision https://developer.blender.org/rBA861519e44adc5674545fa18202dc43c4c20f2d1d
    # changed the default for fills.
    # fix by Sergey https://developer.blender.org/T46150
    if bpy.data.version <= (2, 76, 0):
        for linestyle in bpy.data.linestyles:
            linestyle.use_export_fills = True



classes = (
    SVGExporterPanel,
    SVGExporterLinesetPanel,
    SVGExport,
    )


def register():
    linestyle = bpy.types.FreestyleLineStyle
    linestyle.use_export_strokes = BoolProperty(
            name="Export Strokes",
            description="Export strokes for this Line Style",
            default=True,
            )
    linestyle.stroke_color_mode = EnumProperty(
            name="Stroke Color Mode",
            items=(
                ('BASE', "Base Color", "Use the linestyle's base color", 0),
                ('FIRST', "First Vertex", "Use the color of a stroke's first vertex", 1),
                ('FINAL', "Final Vertex", "Use the color of a stroke's final vertex", 2),
                ),
            default='BASE',
            )
    linestyle.use_export_fills = BoolProperty(
            name="Export Fills",
            description="Export fills for this Line Style",
            default=False,
            )

    for cls in classes:
        bpy.utils.register_class(cls)
    bpy.types.Scene.svg_export = PointerProperty(type=SVGExport)


    # add callbacks
    bpy.app.handlers.render_init.append(render_init)
    bpy.app.handlers.render_write.append(render_write)
    bpy.app.handlers.render_pre.append(svg_export_header)
    bpy.app.handlers.render_complete.append(svg_export_animation)

    # manipulate shaders list
    parameter_editor.callbacks_modifiers_post.append(SVGPathShaderCallback.modifier_post)
    parameter_editor.callbacks_lineset_post.append(SVGPathShaderCallback.lineset_post)
    parameter_editor.callbacks_lineset_post.append(SVGFillShaderCallback.lineset_post)

    # register namespaces
    register_namespaces()

    # handle regressions
    bpy.app.handlers.version_update.append(handle_versions)


def unregister():

    for cls in classes:
        bpy.utils.unregister_class(cls)
    del bpy.types.Scene.svg_export
    linestyle = bpy.types.FreestyleLineStyle
    del linestyle.use_export_strokes
    del linestyle.use_export_fills

    # remove callbacks
    bpy.app.handlers.render_init.remove(render_init)
    bpy.app.handlers.render_write.remove(render_write)
    bpy.app.handlers.render_pre.remove(svg_export_header)
    bpy.app.handlers.render_complete.remove(svg_export_animation)

    # manipulate shaders list
    parameter_editor.callbacks_modifiers_post.remove(SVGPathShaderCallback.modifier_post)
    parameter_editor.callbacks_lineset_post.remove(SVGPathShaderCallback.lineset_post)
    parameter_editor.callbacks_lineset_post.remove(SVGFillShaderCallback.lineset_post)

    bpy.app.handlers.version_update.remove(handle_versions)


if __name__ == "__main__":
    register()
