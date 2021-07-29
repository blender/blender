# This file is a part of the HiRISE DTM Importer for Blender
#
# Copyright (C) 2017 Arizona Board of Regents on behalf of the Planetary Image
# Research Laboratory, Lunar and Planetary Laboratory at the University of
# Arizona.
#
# This program is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the Free
# Software Foundation, either version 3 of the License, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program.  If not, see <http://www.gnu.org/licenses/>.

"""A HiRISE DTM Importer for Blender"""

bl_info = {
    "name": "HiRISE DTM Importer",
    "author": "Nicholas Wolf (nicwolf@pirl.lpl.arizona.edu)",
    "version": (0, 2, 2),
    "blender": (2, 78, 0),
    "location": "File > Import > HiRISE DTM (.img)",
    "description": "Import a HiRISE DTM as a mesh",
    "warning": "May consume a lot of memory",
    "wiki_url": "https://wiki.blender.org/index.php/Extensions:2.6/Py/"
                "Scripts/Import-Export/HiRISE_DTM_from_PDS_IMG",
    "category": "Import-Export",
}

if "bpy" in locals():
    import importlib
    importlib.reload(importer)
    importlib.reload(terrainpanel)

else:
    from .ui import importer
    from .ui import terrainpanel

import bpy


def menu_import(self, context):
    i = importer.ImportHiRISETerrain
    self.layout.operator(i.bl_idname, text=i.bl_label)


def register():
    bpy.utils.register_module(__name__)
    bpy.types.INFO_MT_file_import.append(menu_import)


def unregister():
    bpy.utils.unregister_module(__name__)
    bpy.types.INFO_MT_file_import.remove(menu_import)


if __name__ == '__main__':
    register()
