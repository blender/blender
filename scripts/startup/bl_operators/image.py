# SPDX-FileCopyrightText: 2009-2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
from bpy.types import Operator
from bpy.props import StringProperty
from bpy.app.translations import pgettext_tip as tip_


class EditExternally(Operator):
    """Edit image in an external application"""
    bl_idname = "image.external_edit"
    bl_label = "Image Edit Externally"
    bl_options = {'REGISTER'}

    filepath: StringProperty(
        subtype='FILE_PATH',
    )

    @staticmethod
    def _editor_guess(context):
        import sys

        image_editor = context.preferences.filepaths.image_editor

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
                        tip_("Image path %r not found, image may be packed or "
                             "unsaved") % filepath)
            return {'CANCELLED'}

        cmd = self._editor_guess(context) + [filepath]

        try:
            subprocess.Popen(cmd)
        except BaseException:
            import traceback
            traceback.print_exc()
            self.report({'ERROR'},
                        "Image editor could not be launched, ensure that "
                        "the path in User Preferences > File is valid, and Blender has rights to launch it")

            return {'CANCELLED'}

        return {'FINISHED'}

    def invoke(self, context, _event):
        import os
        sd = context.space_data
        try:
            image = sd.image
        except AttributeError:
            self.report({'ERROR'}, "Context incorrect, image not found")
            return {'CANCELLED'}

        if image.packed_file:
            self.report({'ERROR'}, "Image is packed, unpack before editing")
            return {'CANCELLED'}

        if sd.type == 'IMAGE_EDITOR':
            filepath = image.filepath_from_user(image_user=sd.image_user)
        else:
            filepath = image.filepath

        filepath = bpy.path.abspath(filepath, library=image.library)

        self.filepath = os.path.normpath(filepath)
        self.execute(context)

        return {'FINISHED'}


class ProjectEdit(Operator):
    """Edit a snapshot of the 3D Viewport in an external image editor"""
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
        except RuntimeError as ex:
            self.report({'ERROR'}, str(ex))
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
            filepath = os.path.join(bpy.app.tempdir, "project_edit")

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
        except RuntimeError as ex:
            self.report({'ERROR'}, str(ex))

        return {'FINISHED'}


class ProjectApply(Operator):
    """Project edited image back onto the object"""
    bl_idname = "image.project_apply"
    bl_label = "Project Apply"
    bl_options = {'REGISTER'}

    def execute(self, _context):
        image_name = ProjectEdit._proj_hack[0]  # TODO, deal with this nicer
        image = bpy.data.images.get((image_name, None))
        if image is None:
            self.report({'ERROR'}, tip_("Could not find image '%s'") % image_name)
            return {'CANCELLED'}

        image.reload()
        bpy.ops.paint.project_image(image=image_name)

        return {'FINISHED'}


classes = (
    EditExternally,
    ProjectApply,
    ProjectEdit,
)
