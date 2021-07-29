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

import ast
import os

import bpy
from bpy.props import IntProperty, IntVectorProperty, StringProperty, FloatVectorProperty
from mathutils import Vector

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import updateNode


def info(v):
    combo = []
    for points in v:
        combo.extend(points)

    x, y, z = zip(*combo)
    minx, maxx = min(x), max(x)
    return minx, maxx, (maxx - minx)


def openjson_asdict(fname):
    sv_path = os.path.dirname(os.path.realpath(__file__))
    path_to_json = os.path.join(sv_path, fname)
    with open(path_to_json) as d:
        return ast.literal_eval(''.join(d.readlines()))


def analyze_glyphs(fdict):
    return {k: info(v) for k, v in fdict.items()}


fdict = openjson_asdict('gtext_font.dict')
fdict_sizes = analyze_glyphs(fdict)

def get_palette(tree=None, palette_name=None):
    palettes = tree.grease_pencil.palettes
    if not palette_name in palettes:
        palette = palettes.new(palette_name)
    else:
        palette = palettes.get(palette_name)
    return palette


def generate_greasepencil(node, text, col, pos, fontdict):

    scalar = node.text_scale
    line_height = scalar * 1.56
    spacing = scalar / 2.5
    char_width = scalar / 1.14

    yof = 0
    xof = 0
    bcx, bcy = pos

    nt = node.id_data
    node_name = node.name
    tree_name = nt.name
    grease_pencil_name = tree_name + "_grease"

    # get grease pencil data
    if grease_pencil_name not in bpy.data.grease_pencil:
        nt.grease_pencil = bpy.data.grease_pencil.new(grease_pencil_name)
    else:
        nt.grease_pencil = bpy.data.grease_pencil[grease_pencil_name]
    gp = nt.grease_pencil

    palette = get_palette(tree=nt, palette_name='sv_palette')
    node_specific_color = 'gt_col_' + node.name
    if not node_specific_color in palette.colors:
        named_color = palette.colors.new()
        named_color.color = node.stroke_color
        named_color.name = node_specific_color
    else:
        if node.stroke_color != palette.colors[node_specific_color].color:
            palette.colors[node_specific_color].color = node.stroke_color



    # get grease pencil layer
    if not (node_name in gp.layers):
        layer = gp.layers.new(node_name)
        layer.frames.new(1)
    else:
        layer = gp.layers[node_name]
        layer.frames[0].clear()

    layer.line_change = 1.0
    for ch in text:
        if ch == "\n":
            yof -= line_height
            xof = 0
            continue

        # use m as space unit.

        if ch == " ":
            xof += char_width
            continue

        v = fdict.get(str(ord(ch)), None)

        if not v:
            xof += char_width
            continue

        minx, maxx, xwide = fdict_sizes[str(ord(ch))]

        node_specific_color = 'gt_col_' + node.name
        for chain in v:
            s = layer.frames[0].strokes.new(colorname=node_specific_color)

            s.line_width = 1
            s.draw_mode = '2DSPACE'
            s.points.add(len(chain))
            for idx, p in enumerate(chain):
                ap = Vector(p) - Vector((minx, 0, 0))
                ap *= scalar
                x, y = ap[:2]
                xyz = ((x + bcx + xof), (y + bcy + yof), 0)
                s.points[idx].co = xyz
                s.points[idx].pressure = 1.0

        # xof += char_width
        xof += ((xwide * scalar) + spacing)


class SverchokGText(bpy.types.Operator):
    bl_idname = "node.sverchok_gtext_button"
    bl_label = "Sverchok gtext"
    bl_options = {'REGISTER', 'UNDO'}

    mode = bpy.props.StringProperty(default="")

    def execute(self, context):
        node = context.node

        # check valid greasepencil interface..
        timestamp = bpy.app.build_commit_timestamp
        # earliest_allowed_timestamp = 1487894400 # (feb 24, 2017, 2.78c)
        #                            1487946787
        earliest_allowed_timestamp = 1492214400 # (april 15, 2017)

        if timestamp and timestamp < earliest_allowed_timestamp:
            self.report({'ERROR'}, 'Please update to builder.blender.org latest builds, 2.78c will not work')
            return {'CANCELLED'}


        if self.mode == 'set':
            node.draw_gtext()
        if self.mode == 'clear':
            node.erase_gtext()
        if self.mode == 'clipboard':
            node.set_gtext()

        return {'FINISHED'}




class GTextNode(bpy.types.Node, SverchCustomTreeNode):

    def wrapped_update(self, context):
        self.draw_gtext()


    ''' G Notes '''
    bl_idname = 'GTextNode'
    bl_label = 'GText'
    bl_icon = 'OUTLINER_OB_EMPTY'

    text = StringProperty(name='text', default='your text here')
    locator = IntVectorProperty(name="locator", description="stores location", default=(0, 0), size=2)
    text_scale = IntProperty(name="font size", default=25, update=wrapped_update)
    stroke_color = FloatVectorProperty(subtype='COLOR', size=3, min=0, max=1)

    def draw_buttons(self, context, layout):
        row = layout.row(align=True)
        row.operator('node.sverchok_gtext_button', text='Set').mode = 'set'
        row.operator('node.sverchok_gtext_button', text='', icon='PASTEDOWN').mode = 'clipboard'
        row.operator('node.sverchok_gtext_button', text='', icon='X').mode = 'clear'
        row.prop(self, 'stroke_color', text='')


    def draw_buttons_ext(self, context, layout):
        row = layout.row(align=True)
        if self.id_data.grease_pencil:
            gp_layer = self.id_data.grease_pencil.layers.get(self.name)
            if gp_layer:
                # layout.prop(gp_layer, 'color')
                # layout.prop(gp_layer, 'line_width')
                layout.prop(self, 'text_scale')

    def update(self):
        pass

    def set_gtext(self):
        self.text = bpy.context.window_manager.clipboard
        self.draw_gtext()

    def erase_gtext(self):
        print("should be erasing")

        nt = self.id_data
        node_name = self.name
        tree_name = nt.name
        grease_pencil_name = tree_name + "_grease"

        # get grease pencil data
        gp = nt.grease_pencil
        if (node_name in gp.layers):
            layer = gp.layers[node_name]
            layer.frames[0].clear()

    def draw_gtext(self):
        text = self.text
        col = self.stroke_color
        pos = self.location

        x_offset = 0
        y_offset = -90
        offset = lambda x, y: (x + x_offset, y + y_offset)
        pos = offset(*pos)
        generate_greasepencil(self, text, col, pos, fdict)


def register():
    bpy.utils.register_class(GTextNode)
    bpy.utils.register_class(SverchokGText)


def unregister():
    bpy.utils.unregister_class(SverchokGText)
    bpy.utils.unregister_class(GTextNode)

if __name__ == '__main__':
    register()
