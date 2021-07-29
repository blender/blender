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
from bpy.props import BoolProperty

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import dataCorrect

def pols_edges(obj, unique_edges=False):
    out = []
    for faces in obj:
        out_edges = []
        seen = set()
        for face in faces:
            for edge in zip(face, list(face[1:]) + list([face[0]])):
                if unique_edges and tuple(sorted(edge)) in seen:
                    continue
                if unique_edges:
                    seen.add(tuple(sorted(edge)))
                out_edges.append(edge)
        out.append(out_edges)
    return out

class Pols2EdgsNode(bpy.types.Node, SverchCustomTreeNode):
    ''' take polygon and to edges '''
    bl_idname = 'Pols2EdgsNode'
    bl_label = 'Polygons to Edges'
    bl_icon = 'OUTLINER_OB_EMPTY'

    unique_edges = BoolProperty(name="Unique Edges", default=False,
                                update=SverchCustomTreeNode.process_node)


    def draw_buttons(self, context, layout):
        layout.prop(self, "unique_edges")

    def sv_init(self, context):
        self.inputs.new('StringsSocket', "pols", "pols")
        self.outputs.new('StringsSocket', "edgs", "edgs")

    def process(self):
        if not self.outputs[0].is_linked:
            return
        X_ = self.inputs['pols'].sv_get()
        X = dataCorrect(X_)
        result = pols_edges(X, self.unique_edges)
        self.outputs['edgs'].sv_set(result)


def register():
    bpy.utils.register_class(Pols2EdgsNode)


def unregister():
    bpy.utils.unregister_class(Pols2EdgsNode)

if __name__ == '__main__':
    register()
