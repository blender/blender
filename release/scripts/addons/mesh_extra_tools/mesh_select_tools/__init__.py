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
#  GNU General Public License for more details
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
#
# ##### END GPL LICENSE BLOCK #####

# menu & updates by meta-androcto #
# contributed to by :
# Macouno, dustractor, liero, lijenstina, #
# CoDEmanX, Dolf Veenvliet, meta-androcto #

bl_info = {
    "name": "Select Tools",
    "author": "Multiple Authors",
    "version": (0, 3, 1),
    "blender": (2, 64, 0),
    "location": "Editmode Select Menu/Toolshelf Tools Tab",
    "description": "Adds More vert/face/edge select modes.",
    "warning": "",
    "wiki_url": "",
    "category": "Mesh"
    }

if "bpy" in locals():
    import importlib
    importlib.reload(mesh_select_by_direction)
    importlib.reload(mesh_select_by_edge_length)
    importlib.reload(mesh_select_by_pi)
    importlib.reload(mesh_select_by_type)
    importlib.reload(mesh_select_connected_faces)
    importlib.reload(mesh_index_select)
    importlib.reload(mesh_selection_topokit)
    importlib.reload(mesh_info_select)
else:
    from . import mesh_select_by_direction
    from . import mesh_select_by_edge_length
    from . import mesh_select_by_pi
    from . import mesh_select_by_type
    from . import mesh_select_connected_faces
    from . import mesh_index_select
    from . import mesh_selection_topokit
    from . import mesh_info_select

import bpy


# Register

def register():
    bpy.utils.register_module(__name__)


def unregister():
    bpy.utils.unregister_module(__name__)


if __name__ == "__main__":
    register()
