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

import bpy

class SaveDirty(bpy.types.Operator):
    '''Select object matching a naming pattern'''
    bl_idname = "image.save_dirty"
    bl_label = "Save Dirty"
    bl_register = True
    bl_undo = True

    def execute(self, context):
        unique_paths = set()
        for image in bpy.data.images:
            if image.dirty:
                path = bpy.utils.expandpath(image.filename)
                if "\\" not in path and "/" not in path:
                    self.report({'WARNING'}, "Invalid path: " + path)
                elif path in unique_paths:
                    self.report({'WARNING'}, "Path used by more then one image: " + path)
                else:
                    unique_paths.add(path)
                    image.save(path=path)
        return {'FINISHED'}


def register():
    bpy.types.register(SaveDirty)

def unregister():
    bpy.types.unregister(SaveDirty)

if __name__ == "__main__":
    register()

