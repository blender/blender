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

import collections
import numpy as np

import bpy
from bpy.props import StringProperty, EnumProperty, BoolProperty, FloatVectorProperty

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import (updateNode, repeat_last, fullList)

# pylint: disable=E1101
# pylint: disable=W0613

# modified (kosvor, zeffii) 2017 end november. When Blender changed VertexColor map to rgb+a

vcol_options = [(k, k, '', i) for i, k in enumerate(["RGB", "RGBA"])]


def set_vertices(loop_count, obj, index_socket, indices, input_colors, colors):
    vertex_index = np.zeros(loop_count, dtype=int)
    loops = obj.data.loops
    loops.foreach_get("vertex_index", vertex_index)
    if index_socket.is_linked:
        idx_lookup = collections.defaultdict(list)
        for idx, v_idx in enumerate(vertex_index):
            idx_lookup[v_idx].append(idx)

        for idx, col in zip(indices, input_colors):
            colors[idx_lookup[idx]] = col
    else:
        if len(obj.data.vertices) > len(input_colors):
            fullList(input_colors, len(obj.data.vertices))
        for idx, v_idx in enumerate(vertex_index):
            colors[idx] = input_colors[v_idx]

def set_polygons(polygon_count, obj, index_socket, indices, input_colors, colors):
    p_start = np.empty(polygon_count, dtype=int)
    p_total = np.empty(polygon_count, dtype=int)
    obj.data.polygons.foreach_get("loop_start", p_start)
    obj.data.polygons.foreach_get("loop_total", p_total)
    if index_socket.is_linked:
        for idx, color in zip(indices, input_colors):
            start_slice = p_start[idx]
            stop_slice = start_slice + p_total[idx]
            colors[start_slice:stop_slice] = color
    else:
        if len(input_colors) < polygon_count:
            fullList(input_colors, polygon_count)
        for idx in range(polygon_count):
            color = input_colors[idx]
            start_slice = p_start[idx]
            stop_slice = start_slice + p_total[idx]
            colors[start_slice:stop_slice] = color

def set_loops(loop_count, obj, index_socket, indices, input_colors, colors):
    if index_socket.is_linked:
        for idx, color in zip(indices, input_colors):
            colors[idx] = color
    else:
        if len(input_colors) < loop_count:
            fullList(input_colors, loop_count)
        elif len(input_colors) > loop_count:
            input_colors = input_colors[:loop_count]
        colors[:] = input_colors



class SvVertexColorNodeMK3(bpy.types.Node, SverchCustomTreeNode):
    '''
    Triggers: vcol vertex colors
    Tooltip: Set the vertex colors of the named Layer
    '''

    bl_idname = 'SvVertexColorNodeMK3'
    bl_label = 'Vertex color mk3'
    bl_icon = 'COLOR'

    modes = [
        ("vertices", "Vert", "Vcol - color per vertex", 1),
        ("polygons", "Face", "Pcol - color per face", 2),
        ("loops", "Loop", "Color per loop", 3)]

    mode = EnumProperty(items=modes, default='vertices', update=updateNode)
    object_ref = StringProperty(default='', update=updateNode)
    vertex_color = StringProperty(default='SvCol', update=updateNode)
    clear = BoolProperty(name='clear c', default=True, update=updateNode)

    clear_c = FloatVectorProperty(
        name='cl_color', subtype='COLOR', min=0, max=1, size=4,
        default=(0, 0, 0, 1), update=updateNode)

    use_active = BoolProperty(
        default=False, name="Use active layer",
        description="Use active vertex layer")

    unit_color = FloatVectorProperty(
        name='', default=(.3, .3, .2, 1.0),
        size=4, min=0.0, max=1.0, subtype='COLOR', update=updateNode)

    vcol_size = EnumProperty(
        items=vcol_options,
        name='Num Color Components',
        description="3 = rgb, 4 = rgba.  older versions of Blender only support 3 components",
        default="RGBA", update=updateNode)


    def draw_buttons(self, context, layout):
        layout.prop(self, 'use_active')
        layout.prop(self, 'vertex_color')
        layout.prop(self, "mode", expand=True)

    def draw_buttons_ext(self, context, layout):
        row = layout.row(align=True)
        row.prop(self, "clear", text="clear unindexed")
        row.prop(self, "clear_c", text="")
        layout.prop(self, "vcol_size", expand=True)

    def sv_init(self, context):
        inew = self.inputs.new
        inew('SvObjectSocket', 'Object')
        inew('StringsSocket', "Index")
        color_socket = inew('SvColorSocket', "Color")
        color_socket.prop_name = 'unit_color'


    def get_vertex_color_layer(self, obj):
        vcols = obj.data.vertex_colors
        vertex_color = None
        if vcols:
            active = self.use_active and vcols.active
            vertex_color = vcols.active if active else vcols.get(self.vertex_color)
        return vertex_color or vcols.new(name=self.vertex_color)

    def process(self):

        color_socket = self.inputs["Color"]
        index_socket = self.inputs["Index"]

        # self upgrade, shall only be called once if encountered.
        if color_socket.bl_idname == 'StringsSocket':
            color_socket.replace_socket('SvColorSocket')

        objects = self.inputs["Object"].sv_get()
        color_data = color_socket.sv_get(deepcopy=False, default=[None])
        index_data = index_socket.sv_get(deepcopy=False, default=[None])

        num_components = int(len(self.vcol_size))

        for obj, input_colors, indices in zip(objects, repeat_last(color_data), repeat_last(index_data)):
            if not input_colors:
                continue

            loops = obj.data.loops
            loop_count = len(loops)

            vertex_color = self.get_vertex_color_layer(obj)
            colors = np.empty(loop_count * num_components, dtype=np.float32)

            # we have index and colors, set colors of incoming index
            # first get all colors so we can write to them
            if self.clear:
                colors.shape = (loop_count, num_components)
                colors[:] = self.clear_c[:num_components]
            elif index_socket.is_linked:
                vertex_color.data.foreach_get("color", colors)

            colors.shape = (loop_count, num_components)
            standard_params = obj, index_socket, indices, input_colors, colors

            if self.mode == "vertices":
                set_vertices(loop_count, *standard_params)
            elif self.mode == "polygons":
                set_polygons(len(obj.data.polygons), *standard_params)
            elif self.mode == "loops":
                set_loops(loop_count, *standard_params)

            # write out data
            colors.shape = (loop_count * num_components,)
            vertex_color.data.foreach_set("color", colors)
            obj.data.update()


def register():
    bpy.utils.register_class(SvVertexColorNodeMK3)


def unregister():
    bpy.utils.unregister_class(SvVertexColorNodeMK3)
