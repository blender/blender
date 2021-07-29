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

# --------------------------------- TISSUE ----------------------------------- #
# ------------------------------- version 0.3 -------------------------------- #
#                                                                              #
# Creates duplicates of selected mesh to active morphing the shape according   #
# to target faces.                                                             #
#                                                                              #
#                            Alessandro Zomparelli                             #
#                                   (2017)                                     #
#                                                                              #
# http://www.co-de-it.com/                                                     #
# http://wiki.blender.org/index.php/Extensions:2.6/Py/Scripts/Mesh/Tissue      #
#                                                                              #
# ############################################################################ #

bl_info = {
    "name": "Tissue",
    "author": "Alessandro Zomparelli (Co-de-iT)",
    "version": (0, 3, 3),
    "blender": (2, 7, 9),
    "location": "",
    "description": "Tools for Computational Design",
    "warning": "",
    "wiki_url": "https://wiki.blender.org/index.php/Extensions:2.6/"
                "Py/Scripts/Mesh/Tissue",
    "tracker_url": "https://plus.google.com/u/0/+AlessandroZomparelli/",
    "category": "Mesh"}


if "bpy" in locals():
    import importlib
    importlib.reload(tessellate_numpy)
    importlib.reload(colors_groups_exchanger)
    importlib.reload(dual_mesh)
    importlib.reload(lattice)
    importlib.reload(uv_to_mesh)

else:
    from . import tessellate_numpy
    from . import colors_groups_exchanger
    from . import dual_mesh
    from . import lattice
    from . import uv_to_mesh

import bpy
from bpy.props import PointerProperty


def register():
    bpy.utils.register_module(__name__)
    bpy.types.Object.tissue_tessellate = PointerProperty(
                                            type=tessellate_numpy.tissue_tessellate_prop
                                            )


def unregister():
    tessellate_numpy.unregister()
    colors_groups_exchanger.unregister()
    dual_mesh.unregister()
    lattice.unregister()
    uv_to_mesh.unregister()

    del bpy.types.Object.tissue_tessellate


if __name__ == "__main__":
    register()
