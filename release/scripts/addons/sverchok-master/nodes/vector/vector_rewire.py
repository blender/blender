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
from bpy.props import EnumProperty, FloatProperty
from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import dataCorrect
from sverchok.data_structure import updateNode

options = 'X Y Z Scalar'.split(' ')
options2 = 'X Y Z'.split(' ')
mode_options = [(n, n, '', idx) for idx, n in enumerate(options)]
mode_options2 = [(n, n, '', idx) for idx, n in enumerate(options2)]

class SvVectorRewire(bpy.types.Node, SverchCustomTreeNode):
    ''' Rewire components of a vector'''
    bl_idname = 'SvVectorRewire'
    bl_label = 'Vector Rewire'
    bl_icon = 'OUTLINER_OB_EMPTY'
    sv_icon = 'SV_REWIRE'

    selected_mode_from = EnumProperty(
        items=mode_options,
        description="offers....",
        default="X", update=updateNode
    )

    selected_mode_to = EnumProperty(
        items=mode_options2,
        description="offers....",
        default="Z", update=updateNode
    )

    scalar = FloatProperty(default=0.0, update=updateNode)
    
    def sv_init(self, context):
        self.inputs.new('VerticesSocket', "Vectors")
        self.inputs.new('StringsSocket', "Scalar").prop_name = "scalar"
        self.outputs.new('VerticesSocket', "Vectors")

    def draw_buttons(self, context, layout):
        row = layout.row(align=True)
        row.prop(self, 'selected_mode_from', text='')
        row.label(icon='ARROW_LEFTRIGHT')
        row.separator()
        row.prop(self, 'selected_mode_to', text='')

    def draw_label(self):
        if self.hide:

            val = self.selected_mode_from
            if self.selected_mode_from == 'Scalar':
                if not self.inputs[1].is_linked:
                    val = self.scalar
            
            if isinstance(val, (str, int)):
                dlabel = "{0} > {1}"
            elif isinstance(val, float):
                dlabel = "{0:.3f} > {1}"

            return dlabel.format(val, self.selected_mode_to)

        return self.label or self.name


    def process(self):
        vectors_in = self.inputs[0]
        scalar_in = self.inputs[1]
        vectors_out = self.outputs[0]

        if not all([vectors_out.is_linked, vectors_in.is_linked]):
            return
        
        xyz = vectors_in.sv_get(deepcopy=False)

        index_from = options.index(self.selected_mode_from)
        index_to = options.index(self.selected_mode_to)
        switching = (index_from, index_to)

        # for instance X->X  , return unprocessed
        if len(set(switching)) == 1:
            vectors_out.sv_set(xyz)
            return

        sorted_tuple = tuple(sorted(switching))
        rewire_dict = {(0, 1): (1, 0, 2), (0, 2): (2, 1, 0), (1, 2): (0, 2, 1)}
        
        series_vec = []
        for idx, obj in enumerate(xyz):

            if sorted_tuple in rewire_dict.keys():
                # handles xy xz yz (all xyz combos)
                x, y, z = rewire_dict.get(sorted_tuple)
                coords = ([v[x], v[y], v[z]] for v in obj)
                series_vec.append(list(coords))

            elif switching[0] == 3:
                # handles socket s. -> xyz
                scalar_data = scalar_in.sv_get(deepcopy=False)
                if not (isinstance(scalar_data, list) and len(scalar_data) > 0):
                    continue

                # this will yield until no longer called
                def next_value(idx, data):
                    midx = -1 if idx > len(data)-1 else idx
                    for d in data[midx]:
                        yield d
                    while True:
                        yield data[midx][-1]

                yield_value = next_value(idx, scalar_data)

                if switching[1] == 0:
                    coords = ([next(yield_value), v[1], v[2]] for v in obj)
                elif switching[1] == 1:
                    coords = ([v[0], next(yield_value), v[2]] for v in obj)
                else:  # 2
                    coords = ([v[0], v[1], next(yield_value)] for v in obj)
                series_vec.append(list(coords))


        vectors_out.sv_set(series_vec)                    


def register():
    bpy.utils.register_class(SvVectorRewire)


def unregister():
    bpy.utils.unregister_class(SvVectorRewire)
