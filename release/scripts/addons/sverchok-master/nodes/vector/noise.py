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

import inspect
import operator

import bpy
from bpy.props import EnumProperty
from mathutils import noise

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import (updateNode, Vector_generate, Vector_degenerate)

# noise nodes
# from http://www.blender.org/documentation/blender_python_api_2_70_release/mathutils.noise.html


def avail_noise(self, context):
    n_t = [(t[0], t[0].title(), t[0].title(), '', t[1])
           for t in inspect.getmembers(noise.types) if isinstance(t[1], int)]
    n_t.sort(key=operator.itemgetter(0), reverse=True)
    return n_t


class SvNoiseNode(bpy.types.Node, SverchCustomTreeNode):
    '''Vector Noise node'''
    bl_idname = 'SvNoiseNode'
    bl_label = 'Vector Noise'
    bl_icon = 'FORCE_TURBULENCE'

    def changeMode(self, context):
        if self.out_mode == 'SCALAR':
            if 'Noise S' not in self.outputs:
                self.outputs.remove(self.outputs[0])
                self.outputs.new('StringsSocket', 'Noise S', 'Noise S')
                return
        if self.out_mode == 'VECTOR':
            if 'Noise V' not in self.outputs:
                self.outputs.remove(self.outputs[0])
                self.outputs.new('VerticesSocket', 'Noise V', 'Noise V')
                return

    out_modes = [
        ('SCALAR', 'Scalar', 'Scalar output', '', 1),
        ('VECTOR', 'Vector', 'Vector output', '', 2)]

    out_mode = EnumProperty(
        items=out_modes,
        default='VECTOR',
        description='Output type',
        update=changeMode)

    noise_type = EnumProperty(
        items=avail_noise,
        description="Noise type",
        update=updateNode)

    noise_dict = {}
    noise_f = {'SCALAR': noise.noise, 'VECTOR': noise.noise_vector}

    def sv_init(self, context):
        self.inputs.new('VerticesSocket', 'Vertices', 'Vertices')
        self.outputs.new('VerticesSocket', 'Noise V', 'Noise V')

    def draw_buttons(self, context, layout):
        layout.prop(self, 'out_mode', expand=True)
        layout.prop(self, 'noise_type', text="Type")

    def process(self):

        if not self.noise_dict:
            self.noise_dict = {t[0]: t[1]
                               for t in inspect.getmembers(noise.types)
                               if isinstance(t[1], int)}

        if not self.outputs[0].is_linked:
            return


        verts = Vector_generate(self.inputs['Vertices'].sv_get())
        out = []
        n_t = self.noise_dict[self.noise_type]
        n_f = self.noise_f[self.out_mode]

        for obj in verts:
            out.append([n_f(v, n_t) for v in obj])

        if 'Noise V' in self.outputs:
            self.outputs['Noise V'].sv_set(Vector_degenerate(out))
        else:
            self.outputs['Noise S'].sv_set(out)


def register():
    bpy.utils.register_class(SvNoiseNode)


def unregister():
    bpy.utils.unregister_class(SvNoiseNode)
