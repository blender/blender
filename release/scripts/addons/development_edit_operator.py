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


bl_info = {
    "name": "Edit Operator Source",
    "author": "scorpion81",
    "version": (1, 2, 2),
    "blender": (2, 78, 0),
    "location": "Text Editor > Edit > Edit Operator",
    "description": "Opens source file of chosen operator, if it is an add-on one",
    "warning": "",
    "wiki_url": "https://wiki.blender.org/index.php/Extensions:2.6/"
                "Py/Scripts/Development/Edit_Operator_Source",
    "category": "Development"}

import bpy
import sys
import inspect
from bpy.types import (
        Operator,
        Panel,
        )
from bpy.props import EnumProperty


def getclazz(opname):
    opid = opname.split(".")
    opmod = getattr(bpy.ops, opid[0])
    op = getattr(opmod, opid[1])
    id = op.get_rna().bl_rna.identifier
    clazz = getattr(bpy.types, id)
    return clazz


def getmodule(opname):
    addon = True
    clazz = getclazz(opname)
    modn = clazz.__module__

    try:
        line = inspect.getsourcelines(clazz)[1]
    except IOError:
        line = -1
    except TypeError:
        line = -1

    if modn == 'bpy.types':
        mod = 'C operator'
        addon = False
    elif modn != '__main__':
        mod = sys.modules[modn].__file__
    else:
        addon = False
        mod = modn

    return mod, line, addon


def get_ops():
    allops = []
    opsdir = dir(bpy.ops)
    for opmodname in opsdir:
        opmod = getattr(bpy.ops, opmodname)
        opmoddir = dir(opmod)
        for o in opmoddir:
            name = opmodname + "." + o
            clazz = getclazz(name)
            if (clazz.__module__ != 'bpy.types'):
                allops.append(name)
        del opmoddir

    # add own operator name too, since its not loaded yet when this is called
    allops.append("text.edit_operator")
    l = sorted(allops)
    del allops
    del opsdir

    return [(y, y, "", x) for x, y in enumerate(l)]


class EditOperator(Operator):
    bl_idname = "text.edit_operator"
    bl_label = "Edit Operator"
    bl_description = "Opens the source file of operators chosen from Menu"
    bl_property = "op"

    items = get_ops()

    op = EnumProperty(
            name="Op",
            description="",
            items=items
            )

    def invoke(self, context, event):
        context.window_manager.invoke_search_popup(self)
        return {'PASS_THROUGH'}

    def execute(self, context):
        found = False
        path, line, addon = getmodule(self.op)
        if addon:
            for t in bpy.data.texts:
                if t.filepath == path:
                    ctx = context.copy()
                    ctx['edit_text'] = t
                    bpy.ops.text.jump(ctx, line=line)
                    found = True
                    break

            if (found is False):
                self.report({'INFO'},
                            "Opened file: " + path)
                bpy.ops.text.open(filepath=path)
                bpy.ops.text.jump(line=line)

            return {'FINISHED'}
        else:
            self.report({'WARNING'},
                        "Found no source file for " + self.op)

            return {'CANCELLED'}


class EditOperatorPanel(Panel):
    bl_space_type = 'TEXT_EDITOR'
    bl_region_type = 'UI'
    bl_label = "Edit Operator"

    def draw(self, context):
        layout = self.layout
        layout.operator("text.edit_operator")


def register():
    bpy.utils.register_class(EditOperator)
    bpy.utils.register_class(EditOperatorPanel)


def unregister():
    bpy.utils.unregister_class(EditOperatorPanel)
    bpy.utils.unregister_class(EditOperator)


if __name__ == "__main__":
    register()
