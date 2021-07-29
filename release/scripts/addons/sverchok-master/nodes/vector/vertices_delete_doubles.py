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

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import levelsOflist


class VertsDelDoublesNode(bpy.types.Node, SverchCustomTreeNode):
    ''' Delete doubles vertices '''
    bl_idname = 'VertsDelDoublesNode'
    bl_label = 'Vector X Doubles'
    bl_icon = 'OUTLINER_OB_EMPTY'

    def sv_init(self, context):
        self.inputs.new('VerticesSocket', "vers", "vers")
        self.outputs.new('VerticesSocket', "vers", "vers")

    def process(self):
        vers = self.inputs['vers'].sv_get()
        # Process data
        levs = levelsOflist(vers)
        result = self.remdou(vers, levs)
        self.outputs[0].sv_set(result)

    def remdou(self, vers, levs):
        out = []
        if levs >= 3:
            levs -= 1
            for x in vers:
                out.append(self.remdou(x, levs))
        else:
            for x in vers:
                if x not in out:
                    out.append(x)
        return out


def register():
    bpy.utils.register_class(VertsDelDoublesNode)


def unregister():
    bpy.utils.unregister_class(VertsDelDoublesNode)
