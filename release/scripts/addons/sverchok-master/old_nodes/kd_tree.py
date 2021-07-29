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

import bpy
from bpy.props import EnumProperty, StringProperty
import mathutils
from mathutils import Vector

from sverchok.node_tree import SverchCustomTreeNode


# documentation/blender_python_api_2_70_release/mathutils.kdtree.html

class SvKDTreeNode(bpy.types.Node, SverchCustomTreeNode):

    bl_idname = 'SvKDTreeNode'
    bl_label = 'KDT Closest Verts'
    bl_icon = 'OUTLINER_OB_EMPTY'

    current_mode = StringProperty(default="FIND")

    def mode_change(self, context):

        # just because click doesn't mean we need to change mode
        mode = self.mode
        if mode == self.current_mode:
            return

        inputs = self.inputs
        outputs = self.outputs

        socket_map = {
            'Verts': 'VerticesSocket',
            'Check Verts': 'VerticesSocket',
            'n nearest': 'StringsSocket',
            'radius': 'StringsSocket',
            'proxima .co': 'VerticesSocket',
            'proxima .idx': 'StringsSocket',
            'proxima dist': 'StringsSocket',
            'n proxima .co': 'VerticesSocket',
            'n proxima .idx': 'StringsSocket',
            'n proxima dist': 'StringsSocket',
            'grouped .co': 'VerticesSocket',
            'grouped .idx': 'StringsSocket',
            'grouped dist': 'StringsSocket'
        }

        standard_inputs = ['Verts', 'Check Verts']

        while len(inputs) > 2:
            inputs.remove(inputs[-1])
        while len(outputs) > 0:
            outputs.remove(outputs[-1])

        if mode == 'FIND':
            self.current_mode = mode
            outs = ['proxima .co', 'proxima .idx', 'proxima dist']
            for socket_name in outs:
                socket_type = socket_map[socket_name]
                outputs.new(socket_type, socket_name, socket_name)

        elif mode == 'FIND_N':
            self.current_mode = mode
            socket_name = 'n nearest'
            socket_type = socket_map[socket_name]
            inputs.new(socket_type, socket_name, socket_name)

            outs = ['n proxima .co', 'n proxima .idx', 'n proxima dist']
            for socket_name in outs:
                socket_type = socket_map[socket_name]
                outputs.new(socket_type, socket_name, socket_name)

        elif mode == 'FIND_RANGE':
            self.current_mode = mode
            socket_name = 'radius'
            socket_type = socket_map[socket_name]
            inputs.new(socket_type, socket_name, socket_name)

            outs = ['grouped .co', 'grouped .idx', 'grouped dist']
            for socket_name in outs:
                socket_type = socket_map[socket_name]
                outputs.new(socket_type, socket_name, socket_name)

        else:
            return

    modes = [
        ("FIND", " 1 ", "Find nearest", 1),
        ("FIND_N", " n ", "Find n nearest", 2),
        ("FIND_RANGE", "radius", "Find within Distance", 3)
    ]

    mode = EnumProperty(items=modes,
                        default='FIND',
                        update=mode_change)

    def draw_buttons(self, context, layout):
        layout.label("Search mode:")
        layout.prop(self, "mode", expand=True)

    def sv_init(self, context):
        self.inputs.new('VerticesSocket', 'Verts', 'Verts')
        self.inputs.new('VerticesSocket', 'Check Verts', 'Check Verts')

        defaults = ['proxima .co', 'proxima .idx', 'proxima dist']
        pVerts, pIdxs, pDists = defaults
        self.outputs.new('VerticesSocket', pVerts, pVerts)
        self.outputs.new('StringsSocket', pIdxs, pIdxs)
        self.outputs.new('StringsSocket', pDists, pDists)

    def process(self):
        inputs = self.inputs
        outputs = self.outputs

        if not inputs['Verts'].is_linked:
            return

        try:
            verts = inputs['Verts'].sv_get()[0]
        except IndexError:
            return

        '''
        - assumptions:
            : MainVerts are potentially different on each update
            : not nested input ([vlist1],[vlist2],...)

        with small vert lists I don't imagine this will be very noticeable,
        '''

        # make kdtree
        # documentation/blender_python_api_2_70_release/mathutils.kdtree.html
        size = len(verts)
        kd = mathutils.kdtree.KDTree(size)

        for i, xyz in enumerate(verts):
            kd.insert(xyz, i)
        kd.balance()

        # before continuing check at least that there is one output.
        try:
            some_output = any([outputs[i].is_linked for i in range(3)])
        except (IndexError, KeyError) as e:
            return

        cVerts = inputs['Check Verts'].sv_get()[0]
        if not cVerts:
            return

        # consumables and aliases
        out_co_list = []
        out_idx_list = []
        out_dist_list = []
        add_verts_coords = out_co_list.append
        add_verts_idxs = out_idx_list.append
        add_verts_dists = out_dist_list.append

        if self.mode == 'FIND':
            ''' [Verts.co,..] =>
                [Verts.idx,.] =>
                [Verts.dist,.] =>
            => [Main Verts]
            => [cVert,..]
            '''
            for i, vtx in enumerate(cVerts):
                co, index, dist = kd.find(vtx)
                add_verts_coords(co.to_tuple())
                add_verts_idxs(index)
                add_verts_dists(dist)

        elif self.mode == 'FIND_N':
            ''' [[Verts.co,..n],..c] => from MainVerts closest to v.co
                [[Verts.idx,..n],.c] => from MainVerts closest to v.co
                [[Verts.dist,.n],.c] => from MainVerts closest to v.co
            => [Main Verts]
            => [cVert,..]
            => [n, max n nearest
            '''
            n = inputs['n nearest'].sv_get()[0][0]
            if (not n) or (n < 1):
                return

            for i, vtx in enumerate(cVerts):
                co_list = []
                idx_list = []
                dist_list = []
                n_list = kd.find_n(vtx, n)
                for co, index, dist in n_list:
                    co_list.append(co.to_tuple())
                    idx_list.append(index)
                    dist_list.append(dist)
                add_verts_coords(co_list)
                add_verts_idxs(idx_list)
                add_verts_dists(dist_list)

        elif self.mode == 'FIND_RANGE':
            ''' [grouped [.co for p in MainVerts in r of v in cVert]] =>
                [grouped [.idx for p in MainVerts in r of v in cVert]] =>
                [grouped [.dist for p in MainVerts in r of v in cVert]] =>
            => [Main Verts]
            => [cVert,..]
            => n
            '''
            r = inputs['radius'].sv_get()[0][0]
            if (not r) or r < 0:
                return

            for i, vtx in enumerate(cVerts):
                co_list = []
                idx_list = []
                dist_list = []
                n_list = kd.find_range(vtx, r)
                for co, index, dist in n_list:
                    co_list.append(co.to_tuple())
                    idx_list.append(index)
                    dist_list.append(dist)
                add_verts_coords(co_list)
                add_verts_idxs(idx_list)
                add_verts_dists(dist_list)

        outputs[0].sv_set(out_co_list)
        if outputs[1].is_linked:
            outputs[1].sv_set(out_idx_list)
        if outputs[2].is_linked:
            outputs[2].sv_set(out_dist_list)


def register():
    bpy.utils.register_class(SvKDTreeNode)


def unregister():
    bpy.utils.unregister_class(SvKDTreeNode)
