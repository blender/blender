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

import os

import bpy
from bpy.props import StringProperty

from sverchok.utils.sv_update_utils import sv_get_local_path

sv_path = os.path.dirname(sv_get_local_path()[0])


def get_template_path():
    return os.path.join(sv_path, "node_scripts", "templates")


def get_templates():
    path = get_template_path()
    return [(t, t, "") for t in next(os.walk(path))[2]]


class SvScriptLoader(bpy.types.Operator):

    """ Load Scripts into TextEditor """
    bl_idname = "node.script_template"
    bl_label = "Sverchok script template"
    bl_options = {'REGISTER', 'UNDO'}

    # from object in
    script_path = StringProperty(name='script path')

    def execute(self, context):
        path = get_template_path()
        file_to_load = os.path.join(path, self.script_path)
        bpy.ops.text.open(filepath=file_to_load, internal=True)
        return {'FINISHED'}


class SvTextSubMenu(bpy.types.Menu):
    bl_idname = "TEXT_MT_templates_submenu"
    bl_label = "Sv NodeScripts"
    bl_options = {'REGISTER', 'UNDO'}

    def draw(self, context):
        layout = self.layout

        m = get_templates()
        t = "node.script_template"
        for name, p, _ in m:
            layout.operator(t, text=name).script_path = p


def menu_draw(self, context):
    self.layout.menu("TEXT_MT_templates_submenu")


def register():
    bpy.utils.register_class(SvScriptLoader)
    bpy.utils.register_class(SvTextSubMenu)
    bpy.types.TEXT_MT_templates.append(menu_draw)


def unregister():
    bpy.utils.unregister_class(SvScriptLoader)
    bpy.utils.unregister_class(SvTextSubMenu)
    bpy.types.TEXT_MT_templates.remove(menu_draw)

if __name__ == "__main__":
    register()
