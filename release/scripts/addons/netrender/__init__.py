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

# This directory is a Python package.

bl_info = {
    "name": "Network Renderer",
    "author": "Martin Poirier",
    "version": (1, 8, 1),
    "blender": (2, 60, 0),
    "location": "Render > Engine > Network Render",
    "description": "Distributed rendering for Blender",
    "warning": "Stable but still work in progress",
    "wiki_url": "https://wiki.blender.org/index.php/Extensions:2.6/Py/"
                "Scripts/Render/Net_render",
    "category": "Render",
}


# To support reload properly, try to access a package var, if it's there, reload everything
if "init_data" in locals():
    import importlib
    importlib.reload(model)
    importlib.reload(operators)
    importlib.reload(client)
    importlib.reload(slave)
    importlib.reload(master)
    importlib.reload(master_html)
    importlib.reload(utils)
    importlib.reload(balancing)
    importlib.reload(ui)
    importlib.reload(repath)
    importlib.reload(versioning)
    importlib.reload(baking)
else:
    from netrender import model
    from netrender import operators
    from netrender import client
    from netrender import slave
    from netrender import master
    from netrender import master_html
    from netrender import utils
    from netrender import balancing
    from netrender import ui
    from netrender import repath
    from netrender import versioning
    from netrender import baking

jobs = []
slaves = []
blacklist = []

init_file = ""
valid_address = False
init_data = True


def register():
    import bpy
    bpy.utils.register_module(__name__)

def unregister():
    import bpy
    bpy.utils.unregister_module(__name__)
