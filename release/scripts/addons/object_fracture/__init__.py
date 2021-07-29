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
    "name": "Fracture Tools",
    "author": "pildanovak",
    "version": (2, 0, 1),
    "blender": (2, 72, 0),
    "location": "Search > Fracture Object & Add > Fracture Helper Objects",
    "description": "Fractured Object, Bomb, Projectile, Recorder",
    "warning": "",
    "wiki_url": "http://wiki.blender.org/index.php/Extensions:2.6/Py/Scripts/Object/Fracture",
    "category": "Object",
}


if "bpy" in locals():
    import importlib
    importlib.reload(fracture_ops)
    importlib.reload(fracture_setup)
else:
    from . import fracture_ops
    from . import fracture_setup

import bpy


class INFO_MT_add_fracture_objects(bpy.types.Menu):
    bl_idname = "INFO_MT_add_fracture_objects"
    bl_label = "Fracture Helper Objects"

    def draw(self, context):
        layout = self.layout
        layout.operator_context = 'INVOKE_REGION_WIN'

        layout.operator("object.import_fracture_bomb",
            text="Bomb")
        layout.operator("object.import_fracture_projectile",
            text="Projectile")
        layout.operator("object.import_fracture_recorder",
            text="Rigidbody Recorder")


def menu_func(self, context):
    self.layout.menu("INFO_MT_add_fracture_objects")


def register():
    bpy.utils.register_module(__name__)

    # Add the "add fracture objects" menu to the "Add" menu
    bpy.types.INFO_MT_add.append(menu_func)


def unregister():
    bpy.utils.unregister_module(__name__)

    # Remove "add fracture objects" menu from the "Add" menu.
    bpy.types.INFO_MT_add.remove(menu_func)


if __name__ == "__main__":
    register()
