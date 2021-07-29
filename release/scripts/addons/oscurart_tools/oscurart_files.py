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
from bpy.types import Operator
import os
import shutil


# ---------------------------RELOAD IMAGES------------------


class reloadImages (Operator):
    """Reloads all bitmaps in the scene"""
    bl_idname = "image.reload_images_osc"
    bl_label = "Reload Images"
    bl_options = {"REGISTER", "UNDO"}

    def execute(self, context):
        for imgs in bpy.data.images:
            imgs.reload()
        return {'FINISHED'}


# ------------------------ SAVE INCREMENTAL ------------------------

class saveIncremental(Operator):
    """Saves incremental version of the blend file Ex: "_v01", "_v02\""""
    bl_idname = "file.save_incremental_osc"
    bl_label = "Save Incremental File"
    bl_options = {"REGISTER", "UNDO"}

    def execute(self, context):
        filepath = bpy.data.filepath
        if os.path.basename(filepath).rpartition(".")[0][-5:].count("_v"):
            strnum = filepath.rpartition("_v")[-1].rpartition(".blend")[0]
            intnum = int(strnum)
            modnum = "%02d" % (intnum+1)
            output = filepath.replace(strnum, modnum)
            basename = os.path.basename(filepath)
            bpy.ops.wm.save_as_mainfile(
                filepath=os.path.join(os.path.dirname(filepath), "%s_v%s.blend" %
                                       (basename.rpartition("_v")[0], str(modnum))))

        else:
            output = filepath.rpartition(".blend")[0] + "_v01"
            bpy.ops.wm.save_as_mainfile(filepath=output)

        return {'FINISHED'}

# ------------------------ REPLACE FILE PATHS ------------------------

bpy.types.Scene.oscSearchText = bpy.props.StringProperty(default="Search Text")
bpy.types.Scene.oscReplaceText = bpy.props.StringProperty(
    default="Replace Text")


class replaceFilePath(Operator):
    """Replace the paths set on the “search test” field by the ones in “replace” field. "_v01" »» "_v02" /folder1/render_v01.png  »» /folder1/render_v02.png"""
    bl_idname = "file.replace_file_path_osc"
    bl_label = "Replace File Path"
    bl_options = {"REGISTER", "UNDO"}

    def execute(self, context):
        TEXTSEARCH = bpy.context.scene.oscSearchText
        TEXTREPLACE = bpy.context.scene.oscReplaceText

        for image in bpy.data.images:
            image.filepath = image.filepath.replace(TEXTSEARCH, TEXTREPLACE)

        return {'FINISHED'}


# ---------------------- SYNC MISSING GROUPS --------------------------

class reFreshMissingGroups(Operator):
    """Search on the libraries of the linked source and relink groups and link newones if there are. Usefull to use with the mesh cache tools"""
    bl_idname = "file.sync_missing_groups"
    bl_label = "Sync Missing Groups"
    bl_options = {"REGISTER", "UNDO"}

    def execute(self, context):
        for group in bpy.data.groups:
            if group.library is not None:
                with bpy.data.libraries.load(group.library.filepath, link=True) as (linked, local):
                    local.groups = linked.groups
        return {'FINISHED'}


# ---------------------- COLLECT IMAGES --------------------------


class collectImagesOsc(Operator):
    """Collect all images in the blend file and put them in IMAGES folder"""
    bl_idname = "file.collect_all_images"
    bl_label = "Collect Images"
    bl_options = {"REGISTER", "UNDO"}

    def execute(self, context):

        imagespath = "%s/IMAGES"  % (os.path.dirname(bpy.data.filepath))

        if not os.path.exists(imagespath):
            os.mkdir(imagespath)

        bpy.ops.file.make_paths_absolute()

        for image in bpy.data.images:
            try:
                image.update()
            
                if image.has_data:
                    if not os.path.exists(os.path.join(imagespath,os.path.basename(image.filepath))):
                        shutil.copy(image.filepath, os.path.join(imagespath,os.path.basename(image.filepath)))
                        image.filepath = os.path.join(imagespath,os.path.basename(image.filepath))
                    else:
                        print("%s exists." % (image.name))
                else:
                    print("%s missing path." % (image.name))   
            except:
                print("%s missing path." % (image.name))             

        bpy.ops.file.make_paths_relative()

        return {'FINISHED'}
