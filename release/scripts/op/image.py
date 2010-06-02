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
from bpy.props import StringProperty


class EditExternally(bpy.types.Operator):
    '''Edit image in an external application'''
    bl_idname = "image.external_edit"
    bl_label = "Image Edit Externally"
    bl_options = {'REGISTER'}

    path = StringProperty(name="File Path", description="Path to an image file", maxlen=1024, default="")

    def _editor_guess(self, context):
        import platform
        system = platform.system()

        image_editor = context.user_preferences.filepaths.image_editor

        # use image editor in the preferences when available.
        if not image_editor:
            if system == 'Windows':
                image_editor = ["start"] # not tested!
            elif system == 'Darwin':
                image_editor = ["open"]
            else:
                image_editor = ["gimp"]
        else:
            if system == 'Darwin':
                # blender file selector treats .app as a folder
                # and will include a trailing backslash, so we strip it.
                image_editor.rstrip('\\')
                image_editor = ["open", "-a", image_editor]
            else:
                image_editor = [image_editor]

        return image_editor

    def execute(self, context):
        import subprocess
        path = self.properties.path
        image_editor = self._editor_guess(context)

        cmd = []
        cmd.extend(image_editor)
        cmd.append(bpy.utils.expandpath(path))

        subprocess.Popen(cmd)

        return {'FINISHED'}

    def invoke(self, context, event):
        try:
            path = context.space_data.image.filepath
        except:
            self.report({'ERROR'}, "Image not found on disk")
            return {'CANCELLED'}

        self.properties.path = path
        self.execute(context)

        return {'FINISHED'}


class SaveDirty(bpy.types.Operator):
    '''Select object matching a naming pattern'''
    bl_idname = "image.save_dirty"
    bl_label = "Save Dirty"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        unique_paths = set()
        for image in bpy.data.images:
            if image.dirty:
                path = bpy.utils.expandpath(image.filepath)
                if "\\" not in path and "/" not in path:
                    self.report({'WARNING'}, "Invalid path: " + path)
                elif path in unique_paths:
                    self.report({'WARNING'}, "Path used by more then one image: " + path)
                else:
                    unique_paths.add(path)
                    image.save()
        return {'FINISHED'}


class ProjectEdit(bpy.types.Operator):
    '''Select object matching a naming pattern'''
    bl_idname = "image.project_edit"
    bl_label = "Project Edit"
    bl_options = {'REGISTER'}

    _proj_hack = [""]

    def execute(self, context):
        import os
        import subprocess

        EXT = "png" # could be made an option but for now ok

        for image in bpy.data.images:
            image.tag = True

        bpy.ops.paint.image_from_view()

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
        # filepath = bpy.utils.clean_name(filepath) # fixes <memory> rubbish, needs checking

        if filepath.startswith(".") or filepath == "":
            # TODO, have a way to check if the file is saved, assume .B25.blend
            tmpdir = context.user_preferences.filepaths.temporary_directory
            filepath = os.path.join(tmpdir, "project_edit")
        else:
            filepath = "//" + filepath

        obj = context.object

        if obj:
            filepath += "_" + bpy.utils.clean_name(obj.name)

        filepath_final = filepath + "." + EXT
        i = 0

        while os.path.exists(bpy.utils.expandpath(filepath_final)):
            filepath_final = filepath + ("%.3d.%s" % (i, EXT))
            i += 1

        image_new.name = os.path.basename(filepath_final)
        ProjectEdit._proj_hack[0] = image_new.name

        image_new.filepath_raw = filepath_final # TODO, filepath raw is crummy
        image_new.file_format = 'PNG'
        image_new.save()

        bpy.ops.image.external_edit(path=filepath_final)

        return {'FINISHED'}


class ProjectApply(bpy.types.Operator):
    '''Select object matching a naming pattern'''
    bl_idname = "image.project_apply"
    bl_label = "Project Apply"
    bl_options = {'REGISTER'}

    def execute(self, context):
        image_name = ProjectEdit._proj_hack[0] # TODO, deal with this nicer

        try:
            image = bpy.data.images[image_name]
        except KeyError:
            self.report({'ERROR'}, "Could not find image '%s'" % image_name)
            return {'CANCELLED'}

        image.reload()
        bpy.ops.paint.project_image(image=image_name)

        return {'FINISHED'}

classes = [
    EditExternally,
    SaveDirty,
    ProjectEdit,
    ProjectApply]


def register():
    register = bpy.types.register
    for cls in classes:
        register(cls)


def unregister():
    unregister = bpy.types.unregister
    for cls in classes:
        unregister(cls)

if __name__ == "__main__":
    register()
