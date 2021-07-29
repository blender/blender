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
from mathutils import Matrix
from bpy.props import StringProperty
from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import (updateNode, second_as_first_cycle as safc)

# pylint: disable=w0122
# pylint: disable=w0123
# pylint: disable=w0613


class SvSetDataObjectNodeMK2(bpy.types.Node, SverchCustomTreeNode):
    ''' Set Object Props '''
    bl_idname = 'SvSetDataObjectNodeMK2'
    bl_label = 'Object ID Set MK2'
    bl_icon = 'OUTLINER_OB_EMPTY'

    formula = StringProperty(name='formula', default='delta_location', update=updateNode)

    def draw_buttons(self, context, layout):
        layout.prop(self, "formula", text="")

    def sv_init(self, context):
        self.inputs.new('SvObjectSocket', 'Objects')
        self.inputs.new('StringsSocket', 'values')
        self.outputs.new('StringsSocket', 'outvalues')
        self.outputs.new('SvObjectSocket', 'Objects')

    def process(self):
        O, V = self.inputs
        Ov, Oo = self.outputs
        Prop = self.formula
        objs = O.sv_get()
        if isinstance(objs[0], list):
            if V.is_linked:
                v = V.sv_get()
                if "matrix" in Prop:
                    v = [Matrix(i) for i in v]
                    v = safc(objs, [v])
                    for OBL, VALL in zip(objs, v):
                        VALL = safc(OBL, VALL)
                        exec("for i, i2 in zip(OBL, VALL):\n    i."+Prop+"= i2")
                else:
                    if isinstance(v[0], list):
                        v = safc(objs, v)
                    else:
                        v = safc(objs, [v])
                    for OBL, VALL in zip(objs, v):
                        VALL = safc(OBL, VALL)
                        exec("for i, i2 in zip(OBL, VALL):\n    i."+Prop+"= i2")
            elif Ov.is_linked:
                Ov.sv_set(eval("[[i."+Prop+" for i in OBL] for OBL in objs]"))
            else:
                exec("for OL in objs:\n    for i in OL:\n        i."+Prop)
        else:
            if V.is_linked:
                v = V.sv_get()
                if "matrix" in Prop:
                    v = [Matrix(i) for i in v]
                    v = safc(objs, v)
                    exec("for i, i2 in zip(objs, v):\n    i."+Prop+"= i2")
                else:
                    if isinstance(v[0], list):
                        v = v[0]
                    v = safc(objs, v)
                    exec("for i, i2 in zip(objs, v):\n    i."+Prop+"= i2")
            elif Ov.is_linked:
                Ov.sv_set(eval("[i."+Prop+" for i in objs]"))
            else:
                exec("for i in objs:\n    i."+Prop)
        if Oo.is_linked:
            Oo.sv_set(objs)


def register():
    bpy.utils.register_class(SvSetDataObjectNodeMK2)


def unregister():
    bpy.utils.unregister_class(SvSetDataObjectNodeMK2)
