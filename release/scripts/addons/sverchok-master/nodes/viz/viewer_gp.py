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

import itertools
import bpy
# import mathutils
# from mathutils import Vector
# from bpy.props import FloatProperty, BoolProperty
from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import updateNode, fullList
from sverchok.utils.context_managers import new_input


nodule_color = (0.899, 0.8052, 0.0, 1.0)


def set_correct_stroke_count(strokes, coords, BLACK):
    """ ensure that the number of strokes match the sets of coordinates """
    diff = len(strokes) - len(coords)
    if diff < 0:
        # add new strokes
        for _ in range(abs(diff)):
            strokes.new(colorname=BLACK.name)
    elif diff > 0:
        # remove excess strokes
        for _ in range(diff):
            strokes.remove(strokes[-1])


def pass_data_to_stroke(stroke, coord_set):
    """ adjust the number of points per stroke, to match the incoming coord_set """
    sdiff = len(stroke.points) - len(coord_set)
    if sdiff < 0:
        stroke.points.add(count=abs(sdiff))
    elif sdiff > 0:
        for _ in range(sdiff):
            stroke.points.pop()
    flat_coords = list(itertools.chain.from_iterable(coord_set))
    stroke.points.foreach_set('co', flat_coords)


def pass_pressures_to_stroke(stroke, flat_pressures):
    stroke.points.foreach_set('pressure', flat_pressures)


def match_points_and_pressures(pressure_set, num_points):
    num_pressures = len(pressure_set)
    if num_pressures < num_points:
        fullList(pressure_set, num_points)
    elif num_pressures > num_points:
        pressure_set = pressure_set[:num_points]
    return pressure_set


def get_palette(grease_pencil, palette_name=None):
    palettes = grease_pencil.palettes
    if not palette_name in palettes:
        palette = palettes.new(palette_name)
    else:
        palette = palettes.get(palette_name)
    return palette


def remove_unused_colors(palette, strokes):
    """
    optional cleanup step, probably best to not have this switched on by default
    """
    named_colors = [stroke.colorname for stroke in strokes] + [str([0,0,0])]
    unused_named_colors = {color.name for color in palette.colors} - set(named_colors)
    for unused_color in unused_named_colors:
        palette.colors.remove(palette.colors[unused_color])


def ensure_color_in_palette(node, palette, color, named_color=None, fill=None):

    if not named_color:
        if fill:
            rounded_color = str([round(i, 4) for i in color[:4]]) + str([round(i, 4) for i in fill[:4]])
        else:
            rounded_color = str([round(i, 4) for i in color[:4]])
        named_color = str(rounded_color)
    else:
        named_color = 'BLACK'

    if not named_color in palette.colors:
        new_color = palette.colors.new()
        new_color.name = named_color
        new_color.color = color[:3]
        new_color.alpha = color[3]
        new_color.use_hq_fill = node.use_hq_fill
        if fill:
            new_color.fill_color = fill[:3]
            new_color.fill_alpha = fill[3]

        # print('made', new_color.name)
        return new_color
    else:
        return palette.colors[named_color]


class SvGreasePencilStrokes(bpy.types.Node, SverchCustomTreeNode):
    ''' Make GreasePencil Strokes '''
    bl_idname = 'SvGreasePencilStrokes'
    bl_label = 'Grease Pencil'
    bl_icon = 'GREASEPENCIL'

    # SCREEN / 3DSPACE / 2DSPACE / 2DIMAGE
    mode_options = [(k, k, '', i) for i, k in enumerate(['3DSPACE', '2DSPACE'])]
    
    draw_mode = bpy.props.EnumProperty(
        items=mode_options, description="Draw Mode",
        default="2DSPACE", update=updateNode
    )

    stroke_color = bpy.props.FloatVectorProperty(
        update=updateNode, name='Stroke', default=(0.958, 1.0, 0.897, 1.0),
        size=4, min=0.0, max=1.0, subtype='COLOR'
    )

    fill_color = bpy.props.FloatVectorProperty(
        update=updateNode, name='Fill', default=(0.2, 0.6, 0.9, 1.0),
        size=4, min=0.0, max=1.0, subtype='COLOR'
    )

    auto_cleanup_colors = bpy.props.BoolProperty(default=True, update=updateNode)
    use_hq_fill = bpy.props.BoolProperty(default=False, update=updateNode)

    draw_cyclic = bpy.props.BoolProperty(default=True, update=updateNode)
    pressure = bpy.props.FloatProperty(default=2.0, min=0.1, max=8.0, update=updateNode)
    num_strokes = bpy.props.IntProperty()

    def sv_init(self, context):
        inew = self.inputs.new
        inew('StringsSocket', 'frame')
        inew('VerticesSocket', 'coordinates')  # per stroke
        inew('StringsSocket', 'draw cyclic').prop_name = 'draw_cyclic'   # per stroke
        inew('StringsSocket', 'pressure').prop_name = 'pressure'         # per point

        with new_input(self, 'StringsSocket', 'stroke color') as c1:
            c1.prop_name = 'stroke_color'
            c1.nodule_color = nodule_color       

        with new_input(self, 'StringsSocket', 'fill color') as c2:
            c2.prop_name = 'fill_color'
            c2.nodule_color = nodule_color

        onew = self.outputs.new
        onew('SvObjectSocket', 'frame')
        


    def draw_buttons(self, context, layout):
        layout.prop(self, 'draw_mode', expand=True)
        layout.prop(self, 'use_hq_fill', expand=True)

    def draw_buttons_ext(self, context, layout):
        layout.prop(self, 'auto_cleanup_colors', text='auto remove unused colors')
    

    def get_pressures(self):
        pressures = self.inputs["pressure"].sv_get()
        num_strokes = self.num_strokes

        # the default state will always
        if len(pressures) == 1:
            if len(pressures[0]) < num_strokes:
                fullList(pressures[0], num_strokes)
            elif len(pressures[0]) > num_strokes:
                pressures[0] = pressures[0][:num_strokes]
            pressures = [[n] for n in pressures[0]]
        else:
            fullList(pressures, num_strokes)

        return pressures


    def process(self):
        frame = self.inputs[0]
        coordinates = self.inputs[1]
        colors = self.inputs["stroke color"]
        fills = self.inputs["fill color"]

        if frame.is_linked and coordinates.is_linked:

            strokes = frame.sv_get()
            GP_DATA = strokes.id_data
            PALETTE = get_palette(GP_DATA, "drafting_" + self.name)
            BLACK = ensure_color_in_palette(self, PALETTE, [0,0,0], None, None)

            coords = coordinates.sv_get()
            self.num_strokes = len(coords)
            set_correct_stroke_count(strokes, coords, BLACK)
            cols = colors.sv_get()[0]
            fill_cols = fills.sv_get()[0]

            cyclic_socket_value = self.inputs["draw cyclic"].sv_get()[0]
            fullList(cyclic_socket_value, self.num_strokes)
            fullList(cols, self.num_strokes)
            fullList(fill_cols, self.num_strokes)
            pressures = self.get_pressures()

            for idx, (stroke, coord_set, color, fill) in enumerate(zip(strokes, coords, cols, fill_cols)):
                color_from_palette = ensure_color_in_palette(self, PALETTE, color=color, named_color=None, fill=fill)

                stroke.draw_mode = self.draw_mode
                stroke.draw_cyclic = cyclic_socket_value[idx]

                num_points = len(coord_set)
                pass_data_to_stroke(stroke, coord_set)

                flat_pressures = match_points_and_pressures(pressures[idx], num_points)
                pass_pressures_to_stroke(stroke, flat_pressures)

                stroke.line_width = 1
                try:
                    stroke.colorname = color_from_palette.name
                except:
                    print('stroke with index', idx, 'is not generated yet.')

            remove_unused_colors(PALETTE, strokes)
            self.outputs[0].sv_set(strokes)





def register():
    bpy.utils.register_class(SvGreasePencilStrokes)


def unregister():
    bpy.utils.unregister_class(SvGreasePencilStrokes)

#if __name__ == '__main__':
#    register()
