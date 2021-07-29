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

from itertools import accumulate

import bpy
from bpy.props import EnumProperty, IntProperty, BoolProperty, StringProperty

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import updateNode


def normalize(a):
    # or numpy version..
    max_value = max(a)
    return [n/max_value for n in a]


def ordered_set(a):
    seen = set()
    b = []
    for x in a:
        if not x in seen:
            b.append(x)
            seen.add(x)
    return b


def unique_consecutives(a):
    prev = ''
    b = []
    for x in a:
        if not x == prev:
            b.append(x)
            prev = x
    return b


def mask_subset(a, b):
    return [(_ in b) for _ in a]


SET = 'Set'
INTX = 'Intersection'
UNION = 'Union'
DIFF = 'Difference'
SYMDIFF = 'Symmetric Diff'
SET_OPS = [SET, INTX, UNION, DIFF, SYMDIFF]

# using a purposely broad indexing value range incase other functions get into this..
node_item_list = [
    (1, 1, SET, set),
    (1, 10, "Ordered Set by input", ordered_set),
    (1, 20, "Unique Consecutives", unique_consecutives),
    (1, 30, "Sequential Set", lambda a: sorted(set(a))),
    (1, 40, "Sequential Set Rev", lambda a: sorted(set(a), reverse=True)),
    (1, 50, "Normalize", normalize),
    (1, 60, "Accumulating Sum", lambda a: list(accumulate(a))),
    (2, 69, "Mask subset (A in B)", mask_subset),
    (2, 70, INTX, lambda a, b: set(a) & set(b)),
    (2, 80, UNION, lambda a, b: set(a) | set(b)),
    (2, 90, DIFF, lambda a, b: set(a) - set(b)),
    (2, 100, SYMDIFF, lambda a, b: set(a) ^ set(b))
]

func_dict = {k: v for _, _, k, v in node_item_list}
num_inputs = {k: v for v, _, k, _ in node_item_list}


class SvListModifierNode(bpy.types.Node, SverchCustomTreeNode):
    ''' List Modifier'''
    bl_idname = 'SvListModifierNode'
    bl_label = 'List Modifier'
    bl_icon = 'MODIFIER'

    mode_items = [(name, name, "", idx) for _, idx, name, _ in node_item_list]

    func_ = EnumProperty(
        name="Modes",
        description="Mode Choices",
        default=SET, items=mode_items,
        update=updateNode
    )

    listify = BoolProperty(
        default=True,
        description='Output lists or proper sets',
        update=updateNode
    )

    help_url = StringProperty(default='list_main/list_modifier')

    def draw_buttons(self, context, layout):
        layout.prop(self, "func_", text='')
        layout.prop(self, "listify", text='output as list')

    def sv_init(self, context):
        self.inputs.new('StringsSocket', "Data1")
        self.inputs.new('StringsSocket', "Data2")        
        self.outputs.new('StringsSocket', "Result")

    def draw_label(self):
        return self.func_

    def process(self):
        inputs = self.inputs
        outputs = self.outputs

        if not outputs[0].is_linked:
            return
        
        unary = (num_inputs[self.func_] == 1)
        f = self.get_f(unary)

        if unary:
            if inputs['Data1'].is_linked:
                data1 = inputs['Data1'].sv_get()
            elif inputs['Data2'].is_linked:
                data1 = inputs['Data2'].sv_get()
            else:
                return
            out = f(data1)
        else:
            data1 = inputs['Data1'].sv_get()
            data2 = inputs['Data2'].sv_get()
            out = f(data1, data2)
            # params = match_long_repeat([data1, data2])
            # out = f(*params)

        outputs[0].sv_set(out)

    def get_f(self, unary):
        operation = func_dict[self.func_]

        do_post = (self.func_ in SET_OPS) and self.listify
        post_process = list if do_post else lambda x: x # identity function

        if unary:
            def f(d):
                if isinstance(d[0], (int, float)):
                    return post_process(operation(d))
                else:
                    return [f(x) for x in d]
        else: 
            def f(a, b):
                if isinstance(a[0], (int, float)):
                    return post_process(operation(a, b))
                else:
                    return [f(*_) for _ in zip(a, b)]

        return f


def register():
    bpy.utils.register_class(SvListModifierNode)


def unregister():
    bpy.utils.unregister_class(SvListModifierNode)
