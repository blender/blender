# -*- coding: utf-8 -*-
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
from bpy.props import StringProperty, BoolProperty, FloatVectorProperty, IntProperty, FloatProperty
from bpy.types import NodeTree, NodeSocket


from sverchok.core.socket_data import SvGetSocket, SvGetSocketInfo, SvNoDataError
from sverchok.node_tree import SvSocketCommon, process_from_socket, sentinel
from sverchok.core.socket_conversions import DefaultImplicitConversionPolicy

class SvSocketStandard(SvSocketCommon):
    def get_prop_data(self):
        return {"default_value" , default_value}

    def sv_get(self, default=sentinel, deepcopy=True, implicit_conversions=None):
        if self.is_linked and not self.is_output:
            return self.convert_data(SvGetSocket(self, deepcopy), implicit_conversions)
        else:
            return [[self.default_value]]

    def draw(self, context, layout, node, text):
        if self.is_linked:
            layout.label(text + '. ' + SvGetSocketInfo(self))
        elif self.is_output:
            layout.label(text)
        else:
            layout.prop(self, "default_value", text=text)


class SvFloatSocket(SvSocketStandard, NodeSocket):
    bl_idname = "SvFloatSocket"

    default_value = FloatProperty(update=process_from_socket)

    def draw_color(self, context, node):
        return (0.6, 1.0, 0.6, 1.0)


class SvIntSocket(SvSocketStandard, NodeSocket):
    bl_idname = "SvIntSocket"

    default_value = IntProperty(update=process_from_socket)

    def draw_color(self, context, node):
        return (0.6, 1.0, 0.6, 1.0)


class SvUnsignedIntSocket(SvSocketStandard, NodeSocket):
    bl_idname = "SvUnsignedIntSocket"

    default_value = IntProperty(min=0, update=process_from_socket)

    def draw_color(self, context, node):
        return (0.6, 1.0, 0.6, 1.0)



class SvObjectSocket(NodeSocket, SvSocketCommon):
    bl_idname = "SvObjectSocket"
    bl_label = "Object Socket"

    object_ref = StringProperty(update=process_from_socket)

    def draw(self, context, layout, node, text):
        if not self.is_output and not self.is_linked:
            layout.prop_search(self, 'object_ref', bpy.data, 'objects', text=self.name)
        elif self.is_linked:
            layout.label(text + '. ' + SvGetSocketInfo(self))
        else:
            layout.label(text)

    def draw_color(self, context, node):
        return (0.69,  0.74,  0.73, 1.0)

    def sv_get(self, default=sentinel, deepcopy=True, implicit_conversions=None):
        if self.is_linked and not self.is_output:
            return self.convert_data(SvGetSocket(self, deepcopy), implicit_conversions)
        elif self.object_ref:
            obj_ref = bpy.data.objects.get(self.object_ref)
            if not obj_ref:
                raise SvNoDataError(self)
            return [obj_ref]
        elif default is sentinel:
            raise SvNoDataError(self)
        else:
            return default

class SvTextSocket(NodeSocket, SvSocketCommon):
    bl_idname = "SvTextSocket"
    bl_label = "Text Socket"

    text = StringProperty(update=process_from_socket)

    def draw(self, context, layout, node, text):
        if self.is_linked and not self.is_output:
            layout.label(text)
        if not self.is_linked and not self.is_output:
            layout.prop(self, 'text')

    def draw_color(self, context, node):
        return (0.68,  0.85,  0.90, 1)

    def sv_get(self, default=sentinel, deepcopy=True, implicit_conversions=None):
        if self.is_linked and not self.is_output:
            return self.convert_data(SvGetSocket(self, deepcopy), implicit_conversions)
        elif self.text:
            return [self.text]
        elif default is sentinel:
            raise SvNoDataError(self)
        else:
            return default




classes = [
    SvFloatSocket,
    SvIntSocket,
    SvUnsignedIntSocket,
    SvTextSocket,
    SvObjectSocket,
]

def register():
    for cls in classes:
        bpy.utils.register_class(cls)

def unregister():
    for cls in classes:
        bpy.utils.unregister_class(cls)
