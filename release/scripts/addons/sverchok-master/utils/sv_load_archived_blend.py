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
import urllib
import urllib.request
from zipfile import ZipFile

import bpy
import tempfile

"""
to the reader: when we run 'bpy.ops.wm.open_mainfile( )' there's a considerations
 - the current .blend can be discarded (and up to the user to already save it)
 - the end of the operator that was executed to call 'bpy.ops.wm.open_main_file( )'
   is never reached, and it is irrelevant whether we return {`FINISHED`}.

"""

def handle_zip(self, wm, to_path, file):
    try:
        err = 0
        with ZipFile(to_path) as zf:
            inner_files = zf.namelist()
            if len(inner_files) == 1:
                blendfile = inner_files[0]
            else:
                print('cancelled, not a github zipped .blend')
                return {'CANCELLED'}

        ZipFile(file[0]).extractall(path=self.os_temp_path, members=None, pwd=None)

        wm.progress_update(90)
        err = 1
        os.remove(file[0])
        err = 2
        wm.progress_update(100)
        wm.progress_end()
        
        fp = os.path.join(self.os_temp_path, blendfile)
        bpy.ops.wm.open_mainfile(filepath=fp)
        return {'FINISHED'}  # it never reaches here..
        
    except:
        self.report({'ERROR'}, "Cannot extract files errno {0}".format(str(err)))
        wm.progress_end()
        os.remove(file[0])
        return {'CANCELLED'}

def handle_gz(self, wm, to_path):
    wm.progress_update(100)
    wm.progress_end()
    bpy.ops.wm.open_mainfile(filepath=to_path)
    return {'FINISHED'}


class SvLoadArchivedBlendURL(bpy.types.Operator):

    bl_idname = "node.sv_load_archived_blend_url"
    bl_label = "Load archive URL (.zip .gz of blend)"

    """
    loads links like:
    https://github.com/nortikin/sverchok/files/647412/blend_name_YYYY_MM_DD_HH_mm.zip
    https://github.com/nortikin/sverchok/files/647412/blend_name_YYYY_MM_DD_HH_mm.gz
    """
    download_url = bpy.props.StringProperty()
    os_temp_path = tempfile.gettempdir()

    def execute(self, context):

        wm = bpy.data.window_managers[0]
        wm.progress_begin(0, 100)
        wm.progress_update(20)

        if not self.download_url:
            clipboard = context.window_manager.clipboard
            if not clipboard:
                self.report({'ERROR'}, "Clipboard empty")
                return {'CANCELLED'}
            else:
                self.download_url = clipboard

        # get the file and obtain the new file's path on disk
        try:
            file_and_ext = os.path.basename(self.download_url)
            to_path = os.path.join(self.os_temp_path, file_and_ext)
            file = urllib.request.urlretrieve(self.download_url, to_path)
            wm.progress_update(50)
        except Exception as fullerr:
            print(repr(fullerr))
            self.report({'ERROR'}, "Cannot get archive from Internet")
            wm.progress_end()
            return {'CANCELLED'}

        if file_and_ext.endswith('.gz'):
            return handle_gz(self, wm, to_path)
        elif file_and_ext.endswith('.zip'):
            return handle_zip(self, wm, to_path, file)
        else:
            # maybe if the url ends in .blend we could load it anyway.
            self.report({'ERROR'}, "url is not a .gz or .zip... ending operator")
            wm.progress_end()
            return {'CANCELLED'}            

        return {'FINISHED'}


classes = [
    SvLoadArchivedBlendURL,
]


def register():
    for cls in classes:
        bpy.utils.register_class(cls)


def unregister():
    for cls in classes[::-1]:
        bpy.utils.unregister_class(cls)
