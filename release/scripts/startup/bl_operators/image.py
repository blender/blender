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

# <pep8-80 compliant>

import bpy
from bpy.types import Operator
from bpy.props import StringProperty


class EditExternally(Operator):
    '''Edit image in an external application'''
    bl_idname = "image.external_edit"
    bl_label = "Image Edit Externally"
    bl_options = {'REGISTER'}

    filepath = StringProperty(
            subtype='FILE_PATH',
            )

    def _editor_guess(self, context):
        import sys

        image_editor = context.user_preferences.filepaths.image_editor

        # use image editor in the preferences when available.
        if not image_editor:
            if sys.platform[:3] == "win":
                image_editor = ["start"]  # not tested!
            elif sys.platform == "darwin":
                image_editor = ["open"]
            else:
                image_editor = ["gimp"]
        else:
            if sys.platform == "darwin":
                # blender file selector treats .app as a folder
                # and will include a trailing backslash, so we strip it.
                image_editor.rstrip('\\')
                image_editor = ["open", "-a", image_editor]
            else:
                image_editor = [image_editor]

        return image_editor

    def execute(self, context):
        import os
        import subprocess

        filepath = self.filepath

        if not filepath:
            self.report({'ERROR'}, "Image path not set")
            return {'CANCELLED'}

        if not os.path.exists(filepath) or not os.path.isfile(filepath):
            self.report({'ERROR'},
                        "Image path %r not found, image may be packed or "
                        "unsaved" % filepath)
            return {'CANCELLED'}

        cmd = self._editor_guess(context) + [filepath]

        try:
            subprocess.Popen(cmd)
        except:
            import traceback
            traceback.print_exc()
            self.report({'ERROR'},
                        "Image editor not found, "
                        "please specify in User Preferences > File")

            return {'CANCELLED'}

        return {'FINISHED'}

    def invoke(self, context, event):
        import os
        try:
            image = context.space_data.image
        except AttributeError:
            self.report({'ERROR'}, "Context incorrect, image not found")
            return {'CANCELLED'}

        if image.packed_file:
            self.report({'ERROR'}, "Image is packed, unpack before editing")
            return {'CANCELLED'}

        filepath = bpy.path.abspath(image.filepath, library=image.library)

        self.filepath = os.path.normpath(filepath)
        self.execute(context)

        return {'FINISHED'}


class SaveDirty(Operator):
    """Save all modified textures"""
    bl_idname = "image.save_dirty"
    bl_label = "Save Dirty"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        unique_paths = set()
        for image in bpy.data.images:
            if image.is_dirty:
                filepath = bpy.path.abspath(image.filepath)
                if "\\" not in filepath and "/" not in filepath:
                    self.report({'WARNING'}, "Invalid path: " + filepath)
                elif filepath in unique_paths:
                    self.report({'WARNING'},
                                "Path used by more then one image: %r" %
                                filepath)
                else:
                    unique_paths.add(filepath)
                    image.save()
        return {'FINISHED'}


class ProjectEdit(Operator):
    """Edit a snapshot of the view-port in an external image editor"""
    bl_idname = "image.project_edit"
    bl_label = "Project Edit"
    bl_options = {'REGISTER'}

    _proj_hack = [""]

    def execute(self, context):
        import os

        EXT = "png"  # could be made an option but for now ok

        for image in bpy.data.images:
            image.tag = True

        # opengl buffer may fail, we can't help this, but best report it.
        try:
            bpy.ops.paint.image_from_view()
        except RuntimeError as err:
            self.report({'ERROR'}, str(err))
            return {'CANCELLED'}

        image_new = None
        for image in bpy.data.images:
            if not image.tag:
                image_new = image
                break

        if not image_new:
            self.report({'ERROR'}, "Could not make new image")
            return {'CANCELLED'}

        filepath = os.path.basename(bpy.data.filepath)
        filepath = os.path.splitext(filepath)[0]
        # fixes <memory> rubbish, needs checking
        # filepath = bpy.path.clean_name(filepath)

        if bpy.data.is_saved:
            filepath = "//" + filepath
        else:
            tmpdir = context.user_preferences.filepaths.temporary_directory
            filepath = os.path.join(tmpdir, "project_edit")

        obj = context.object

        if obj:
            filepath += "_" + bpy.path.clean_name(obj.name)

        filepath_final = filepath + "." + EXT
        i = 0

        while os.path.exists(bpy.path.abspath(filepath_final)):
            filepath_final = filepath + ("%.3d.%s" % (i, EXT))
            i += 1

        image_new.name = bpy.path.basename(filepath_final)
        ProjectEdit._proj_hack[0] = image_new.name

        image_new.filepath_raw = filepath_final  # TODO, filepath raw is crummy
        image_new.file_format = 'PNG'
        image_new.save()

        filepath_final = bpy.path.abspath(filepath_final)

        try:
            bpy.ops.image.external_edit(filepath=filepath_final)
        except RuntimeError as re:
            self.report({'ERROR'}, str(re))

        return {'FINISHED'}


class ProjectApply(Operator):
    """Project edited image back onto the object"""
    bl_idname = "image.project_apply"
    bl_label = "Project Apply"
    bl_options = {'REGISTER'}

    def execute(self, context):
        image_name = ProjectEdit._proj_hack[0]  # TODO, deal with this nicer

        try:
            image = bpy.data.images[image_name]
        except KeyError:
            import traceback
            traceback.print_exc()
            self.report({'ERROR'}, "Could not find image '%s'" % image_name)
            return {'CANCELLED'}

        image.reload()
        bpy.ops.paint.project_image(image=image_name)

        return {'FINISHED'}
