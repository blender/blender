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

# <pep8 compliant>


import bpy as _bpy
import bpyml
from bpyml import TAG, ARGS, CHILDREN

_uilayout_rna = _bpy.types.UILayout.bl_rna

_uilayout_tags = (
    ["ui"] +
    _uilayout_rna.properties.keys() +
    _uilayout_rna.functions.keys()
    )

# these need to be imported directly
# >>> from bpyml_ui.locals import *
locals = bpyml.tag_module("%s.locals" % __name__, _uilayout_tags)


def _parse_rna(prop, value):
    if prop.type == 'FLOAT':
        value = float(value)
    elif prop.type == 'INT':
        value = int(value)
    elif prop.type == 'BOOLEAN':
        if value in {True, False}:
            pass
        else:
            if value not in {"True", "False"}:
                raise Exception("invalid bool value: %s" % value)
            value = bool(value == "True")
    elif prop.type in {'STRING', 'ENUM'}:
        pass
    elif prop.type == 'POINTER':
        value = eval("_bpy." + value)
    else:
        raise Exception("type not supported %s.%s" % (prop.identifier, prop.type))
    return value


def _parse_rna_args(base, py_node):
    rna_params = base.bl_rna.functions[py_node[TAG]].parameters
    args = {}
    for key, value in py_node[ARGS].items():
        args[key] = _parse_rna(rna_params[key], value)
    return args


def _call_recursive(context, base, py_node):
    # prop = base.bl_rna.properties.get(py_node[TAG])
    if py_node[TAG] in base.bl_rna.properties:
        value = py_node[ARGS].get("expr")
        if value:
            value = eval(value, {"context": _bpy.context})
            setattr(base, py_node[TAG], value)
        else:
            value = py_node[ARGS]["value"]  # have to have this
            setattr(base, py_node[TAG], value)
    else:
        args = _parse_rna_args(base, py_node)
        func_new = getattr(base, py_node[TAG])
        base_new = func_new(**args)  # call blender func
        if base_new is not None:
            for py_node_child in py_node[CHILDREN]:
                _call_recursive(context, base_new, py_node_child)


class BPyML_BaseUI():
    '''
    This is a mix-in class that defines a draw function
    which checks for draw_data
    '''

    def draw(self, context):
        layout = self.layout
        for py_node in self.draw_data[CHILDREN]:
            _call_recursive(context, layout, py_node)

    def draw_header(self, context):
        layout = self.layout
        for py_node in self.draw_header_data[CHILDREN]:
            _call_recursive(context, layout, py_node)
