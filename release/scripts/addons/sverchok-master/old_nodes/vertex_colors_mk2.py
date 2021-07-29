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
import bmesh
from bpy.props import StringProperty, EnumProperty, BoolProperty, FloatVectorProperty

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import (updateNode, repeat_last, fullList)


class SvVertexColorNodeMK2(bpy.types.Node, SverchCustomTreeNode):
    ''' Vertex Colors '''
    bl_idname = 'SvVertexColorNodeMK2'
    bl_label = 'Vertex color MK2'
    bl_icon = 'COLOR'

    vertex_color = StringProperty(default='SvCol', update=updateNode)
    clear = BoolProperty(name='clear c', default=True, update=updateNode)
    clear_c = FloatVectorProperty(name='cl_color', subtype='COLOR', min=0, max=1, size=3, default=(0, 0, 0), update=updateNode)
    modes = [("vertices", "Vert", "Vcol - color per vertex", 1),
             ("polygons", "Face", "Pcol - color per face", 2),
             ("loops", "Loop", "Color per loop", 3)]
    mode = EnumProperty(items=modes, default='vertices', update=updateNode)
    object_ref = StringProperty(default='', update=updateNode)

    use_active = BoolProperty(default=False, name="Use active layer",
                              description="Use active vertex layer")

    def draw_buttons(self, context, layout):
        layout.prop(self, 'use_active')
        layout.prop(self, 'vertex_color')
        layout.prop(self, "mode", expand=True)

    def draw_buttons_ext(self, context, layout):
        row = layout.row(align=True)
        row.prop(self, "clear", text="clear unindexed")
        row.prop(self, "clear_c", text="")

    def sv_init(self, context):
        self.inputs.new('SvObjectSocket', 'Object')
        self.inputs.new('StringsSocket', "Index")
        self.inputs.new('VerticesSocket', "Color")

    def process(self):

        objects = self.inputs["Object"].sv_get()
        color_socket = self.inputs["Color"]
        index_socket = self.inputs["Index"]

        color_data = color_socket.sv_get(deepcopy=False, default=[None])
        index_data = index_socket.sv_get(deepcopy=False, default=[None])

        for obj, input_colors, indices in zip(objects, repeat_last(color_data), repeat_last(index_data)):
            loops = obj.data.loops
            loop_count = len(loops)
            if obj.data.vertex_colors:
                if self.use_active and obj.data.vertex_colors.active:
                    vertex_color = obj.data.vertex_colors.active
                else:
                    vertex_color = obj.data.vertex_colors.get(self.vertex_color)
                    if not vertex_color:
                        vertex_color = obj.data.vertex_colors.new(name=self.vertex_color)
            else:
                vertex_color = obj.data.vertex_colors.new(name=self.vertex_color)

            colors = np.empty(loop_count * 3, dtype=np.float32)

            if input_colors:
                # we have index and colors, set colors of incoming index
                # first get all colors so we can write to them
                if self.clear:
                    colors.shape = (loop_count, 3)
                    colors[:] = self.clear_c
                elif index_socket.is_linked:
                    vertex_color.data.foreach_get("color", colors)

                colors.shape = (loop_count, 3)

                if self.mode == "vertices":
                    vertex_index = np.zeros(loop_count, dtype=int)  # would be good to check exackt type
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

                elif self.mode == "polygons":
                    polygon_count = len(obj.data.polygons)
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

                elif self.mode == "loops":
                    if index_socket.is_linked:
                        for idx, color in zip(indices, input_colors):
                            colors[idx] = color
                    else:
                        if len(input_colors) < loop_count:
                            fullList(input_colors, loop_count)
                        elif len(input_colors) > loop_count:
                            input_colors = input_colors[:loop_count]
                        colors[:] = input_colors
                # write out data
                colors.shape = (loop_count * 3,)
                vertex_color.data.foreach_set("color", colors)
                obj.data.update()
        """
        # So output is removed from this node
        # if no output, we are done
        if not self.outputs[0].is_linked:
            return

        colors.shape = (loop_count * 3)
        vertex_color.data.foreach_get("color", colors)
        colors.shape = (loop_count, 3)
        out = []
        if self.mode == "vertices":
            vert_loopup = {l.vertex_index: idx for idx, l in enumerate(obj.data.loops)}

            if index_socket.is_linked:
                index_seq = indices
            else:
                index_seq = range(len(obj.data.vertices))

            for idx in index_seq:
                loop_idx = vert_loopup[idx]
                out.append(colors[loop_idx].tolist())

        elif self.mode == "polygons":
            polygons = obj.data.polygons
            if index_socket.is_linked:
                index_seq = indices
            else:
                index_seq = range(len(polygons))

            for idx in index_seq:
                out.append(colors[polygons[idx].loop_start].tolist())

        elif self.mode == "loops":
            if index_socket.is_linked:
                for idx in indices:
                    out.append(colors[idx].tolist())
            else:
                out = colors.tolist()

        self.outputs[0].sv_set([out])
        """


def register():
    bpy.utils.register_class(SvVertexColorNodeMK2)


def unregister():
    bpy.utils.unregister_class(SvVertexColorNodeMK2)
