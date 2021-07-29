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
    "name": "Geodesic Domes2",
    "author": "Noctumsolis, PKHG, Meta Androcto, Andy Houston",
    "version": (0, 3, 2),
    "blender": (2, 7, 1),
    "location": "Toolshelf > Create Tab",
    "description": "Create geodesic dome type objects.",
    "warning": "",
    "wiki_url": "https://wiki.blender.org/index.php/Extensions:2.6/Py/Scripts/Modeling/Geodesic_Domes",
    "tracker_url": "",
    "category": "Mesh"}

if "bpy" in locals():
    import importlib
    importlib.reload(third_domes_panel_271)

else:
    from . import third_domes_panel_271

import bpy


def register():
    bpy.utils.register_module(__name__)


def unregister():
    bpy.utils.unregister_module(__name__)


if __name__ == "__main__":
    register()
