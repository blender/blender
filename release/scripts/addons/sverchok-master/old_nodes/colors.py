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
from sverchok.data_structure import (updateNode, second_as_first_cycle, fullList)


class SvVertexColorNode(bpy.types.Node, SverchCustomTreeNode):
    ''' Vertex Colors '''
    bl_idname = 'SvVertexColorNode'
    bl_label = 'Vertex color'
    bl_icon = 'OUTLINER_OB_EMPTY'

    use_foreach = BoolProperty(default=False)
    vertex_color = StringProperty(default='', update=updateNode)
    clear = BoolProperty(name='clear c', default=True, update=updateNode)
    clear_c = FloatVectorProperty(name='cl_color', subtype='COLOR', min=0, max=1, size=3, default=(0, 0, 0), update=updateNode)
    modes = [("vertices", "Vert", "Vcol - color per vertex", 1),
             ("polygons", "Face", "Pcol - color per face", 2),
             ("loops", "Loop", "Color per loop", 3)]
    mode = EnumProperty(items=modes, default='vertices', update=updateNode)
    object_ref = StringProperty(default='', update=updateNode)

    def draw_buttons(self, context,   layout):
        #layout.prop(self, 'use_foreach')
        layout.prop_search(self, 'object_ref', bpy.data, 'objects')
        ob = bpy.data.objects.get(self.object_ref)
        if ob and ob.type == 'MESH':
            layout.prop_search(self, 'vertex_color', ob.data, "vertex_colors", text="")
            layout.prop(self, "mode", expand=True)


    def draw_buttons_ext(self, context, layout):
        row = layout.row(align=True)
        row.prop(self,    "clear",   text="clear unindexed")
        row.prop(self, "clear_c", text="")


    def sv_init(self, context):
        self.inputs.new('StringsSocket', "Index")
        self.inputs.new('VerticesSocket', "Color")
        self.outputs.new('VerticesSocket', "OutColor")

    def process_foreach(self):
        print("foreach")
        obj = bpy.data.objects[self.object_ref]
        loops = obj.data.loops
        loop_count = len(loops)
        if not obj.data.vertex_colors:
            objm.vertex_colors.new(name='Sv_VColor')
        vertex_color = obj.data.vertex_colors[self.vertex_color]

        color_socket = self.inputs["Color"]
        index_socket = self.inputs["Index"]

        if index_socket.is_linked:
            indices = index_socket.sv_get()[0]

        if color_socket.is_linked:
            input_colors =  color_socket.sv_get()[0] # until object socket only first object...
        else:
            input_colors = None

        colors = np.empty(loop_count * 3, dtype=np.float32)

        if input_colors:
            # we have index and colors, set colors of incoming index
            # first get all colors so we can write to them
            if self.clear:
                colors.shape = (loop_count, 3)
                colors[:] = self.clear_c
            elif index_socket.is_linked:
                print(self.clear , "clear?")
                vertex_color.data.foreach_get("color", colors)
            colors.shape = (loop_count, 3)

            if self.mode == "vertices":
                vertex_index = np.zeros(loop_count, dtype=int) # would be good to check exackt type
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
                p_start = np.empty(polygon_count,dtype=int)
                p_total = np.empty(polygon_count,dtype=int)
                obj.data.polygons.foreach_get("loop_start", p_start)
                obj.data.polygons.foreach_get("loop_total", p_total)

                if index_socket.is_linked:
                    for idx, color in zip(indices, input_colors):
                        start_slice = p_start[idx]
                        stop_slice = start_slice + p_total[idx]
                        colors[start_slice : stop_slice] = color
                else:
                    if len(input_colors) < polygon_count:
                        fullList(input_colors, polygon_count)

                    for idx in range(polygon_count):
                        color = input_colors[idx]
                        start_slice = p_start[idx]
                        stop_slice = start_slice + p_total[idx]
                        colors[start_slice : stop_slice] = color

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

        # if no output, we are done
        if not self.outputs[0].is_linked:
            return

        colors.shape = (loop_count * 3)
        vertex_color.data.foreach_get("color", colors)
        colors.shape = (loop_count, 3)
        out = []
        if self.mode == "vertices":
            vert_loopup = {l.vertex_index : idx for idx, l in enumerate(obj.data.loops)}

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

    def process(self):
        if self.use_foreach:
            self.process_foreach()
            return
        objm = bpy.data.objects[self.object_ref].data
        objm.update()
        if not objm.vertex_colors:
            objm.vertex_colors.new(name='Sv_VColor')
        if self.vertex_color not in objm.vertex_colors:
            return
        ovgs = objm.vertex_colors.get(self.vertex_color)
        Ind, Col = self.inputs
        if Col.is_linked:
            sm, colors = self.mode, Col.sv_get()[0]
            idxs = Ind.sv_get()[0] if Ind.is_linked else [i.index for i in getattr(objm,sm)]
            colors = second_as_first_cycle(idxs, colors)
            bm = bmesh.new()
            bm.from_mesh(objm)
            if self.clear:
                clear_c = self.clear_c[:]
                for i in range(len(ovgs.data)):
                    ovgs.data[i].color = clear_c
            if sm == 'vertices':
                bv = bm.verts
                bm.verts.ensure_lookup_table()
                for i, col in zip(idxs, colors):
                    for l in bv[i].link_loops:
                        ovgs.data[l.index].color = col
            elif sm == 'polygons':
                #bf = bm.faces[:]
                bm.faces.ensure_lookup_table()
                for i, i2 in zip(idxs, colors):
                    for loop in bm.faces[i].loops:
                        ovgs.data[loop.index].color = i2
            elif sm == 'loops':
                for idx, color in zip(idxs, colors):
                    ovgs.data[idx].color = color
            bm.free()
        if self.outputs["OutColor"].is_linked:
            out = []
            sm= self.mode
            bm = bmesh.new()
            bm.from_mesh(objm)
            if sm == 'vertices':
                #output one color per vertex
                for v in bm.verts[:]:
                    c = ovgs.data[v.link_loops[0].index].color
                    out.append(list(c))

            elif sm == 'polygons':
                #output one color per face
                for f in bm.faces[:]:
                    c = ovgs.data[f.loops[0].index].color
                    out.append(list(c))
            elif sm == 'loops':
                for i in range(len(ovgs.data)):
                    c = ovgs.data[i].color
                    out.append(c[:])

            self.outputs["OutColor"].sv_set([out])
            bm.free()





def register():
    bpy.utils.register_class(SvVertexColorNode)


def unregister():
    bpy.utils.unregister_class(SvVertexColorNode)
