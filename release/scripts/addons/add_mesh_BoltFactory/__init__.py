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
    "name": "BoltFactory",
    "author": "Aaron Keith",
    "version": (0, 3, 3),
    "blender": (2, 78, 0),
    "location": "View3D > Add > Mesh",
    "description": "Add a bolt or nut",
    "wiki_url": "https://wiki.blender.org/index.php/Extensions:2.6/Py/"
                "Scripts/Add_Mesh/BoltFactory",
    "category": "Add Mesh",
}


if "bpy" in locals():
    import importlib
    importlib.reload(Boltfactory)
    importlib.reload(createMesh)
else:
    from . import Boltfactory
    from . import createMesh

import bpy


# ### REGISTER ###

def add_mesh_bolt_button(self, context):
    self.layout.operator(Boltfactory.add_mesh_bolt.bl_idname, text="Bolt", icon="MOD_SCREW")


def register():
    bpy.utils.register_module(__name__)

    bpy.types.INFO_MT_mesh_add.append(add_mesh_bolt_button)
    # bpy.types.VIEW3D_PT_tools_objectmode.prepend(add_mesh_bolt_button)  # just for testing


def unregister():
    bpy.utils.unregister_module(__name__)

    bpy.types.INFO_MT_mesh_add.remove(add_mesh_bolt_button)
    # bpy.types.VIEW3D_PT_tools_objectmode.remove(add_mesh_bolt_button)  # just for testing


if __name__ == "__main__":
    register()
