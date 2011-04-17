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
from bpy.props import StringProperty, BoolProperty, EnumProperty


class ExportHelper:
    filepath = StringProperty(name="File Path", description="Filepath used for exporting the file", maxlen=1024, default="", subtype='FILE_PATH')
    check_existing = BoolProperty(name="Check Existing", description="Check and warn on overwriting existing files", default=True, options={'HIDDEN'})

    # subclasses can override with decorator
    # True == use ext, False == no ext, None == do nothing.
    check_extension = True

    def invoke(self, context, event):
        import os
        if not self.filepath:
            blend_filepath = context.blend_data.filepath
            if not blend_filepath:
                blend_filepath = "untitled"
            else:
                blend_filepath = os.path.splitext(blend_filepath)[0]

            self.filepath = blend_filepath + self.filename_ext

        context.window_manager.fileselect_add(self)
        return {'RUNNING_MODAL'}

    def check(self, context):
        check_extension = self.check_extension

        if check_extension is None:
            return False

        filepath = bpy.path.ensure_ext(self.filepath, self.filename_ext if check_extension else "")

        if filepath != self.filepath:
            self.filepath = filepath
            return True

        return False


class ImportHelper:
    filepath = StringProperty(name="File Path", description="Filepath used for importing the file", maxlen=1024, default="", subtype='FILE_PATH')

    def invoke(self, context, event):
        context.window_manager.fileselect_add(self)
        return {'RUNNING_MODAL'}


# limited replacement for BPyImage.comprehensiveImageLoad
def load_image(imagepath, dirname):
    import os

    if os.path.exists(imagepath):
        return bpy.data.images.load(imagepath)

    variants = [imagepath, os.path.join(dirname, imagepath), os.path.join(dirname, os.path.basename(imagepath))]

    for filepath in variants:
        for nfilepath in (filepath, bpy.path.resolve_ncase(filepath)):
            if os.path.exists(nfilepath):
                return bpy.data.images.load(nfilepath)

    # TODO comprehensiveImageLoad also searched in bpy.config.textureDir
    return None


# return a tuple (free, object list), free is True if memory should be freed later with free_derived_objects()
def create_derived_objects(scene, ob):
    if ob.parent and ob.parent.dupli_type != 'NONE':
        return False, None

    if ob.dupli_type != 'NONE':
        ob.dupli_list_create(scene)
        return True, [(dob.object, dob.matrix) for dob in ob.dupli_list]
    else:
        return False, [(ob, ob.matrix_world)]


def free_derived_objects(ob):
    ob.dupli_list_clear()


def unpack_list(list_of_tuples):
    flat_list = []
    flat_list_extend = flat_list.extend  # a tich faster
    for t in list_of_tuples:
        flat_list_extend(t)
    return flat_list


# same as above except that it adds 0 for triangle faces
def unpack_face_list(list_of_tuples):
    #allocate the entire list
    flat_ls = [0] * (len(list_of_tuples) * 4)
    i = 0

    for t in list_of_tuples:
        if len(t) == 3:
            if t[2] == 0:
                t = t[1], t[2], t[0]
        else:  # assuem quad
            if t[3] == 0 or t[2] == 0:
                t = t[2], t[3], t[0], t[1]

        flat_ls[i:i + len(t)] = t
        i += 4
    return flat_ls


path_reference_mode = EnumProperty(
        name="Path Mode",
        description="Method used to reference paths",
        items=(('AUTO', "Auto", "Use Relative paths with subdirectories only"),
               ('ABSOLUTE', "Absolute", "Always write absolute paths"),
               ('RELATIVE', "Relative", "Always write relative patsh (where possible)"),
               ('MATCH', "Match", "Match Absolute/Relative setting with input path"),
               ('STRIP', "Strip Path", "Filename only"),
               ('COPY', "Copy", "copy the file to the destination path (or subdirectory)"),
               ),
        default='AUTO'
        )


def path_reference(filepath, base_src, base_dst, mode='AUTO', copy_subdir="", copy_set=None):
    """
    Return a filepath relative to a destination directory, for use with
    exporters.

    :arg filepath: the file path to return, supporting blenders relative '//' prefix.
    :type filepath: string
    :arg base_src: the directory the *filepath* is relative too (normally the blend file).
    :type base_src: string
    :arg base_dst: the directory the *filepath* will be referenced from (normally the export path).
    :type base_dst: string
    :arg mode: the method used get the path in ['AUTO', 'ABSOLUTE', 'RELATIVE', 'MATCH', 'STRIP', 'COPY']
    :type mode: string
    :arg copy_subdir: the subdirectory of *base_dst* to use when mode='COPY'.
    :type copy_subdir: string
    :arg copy_set: collect from/to pairs when mode='COPY', pass to *path_reference_copy* when exportign is done.
    :type copy_set: set
    :return: the new filepath.
    :rtype: string
    """
    import os
    is_relative = filepath.startswith("//")
    filepath_abs = os.path.normpath(bpy.path.abspath(filepath, base_src))

    if mode in ('ABSOLUTE', 'RELATIVE', 'STRIP'):
        pass
    elif mode == 'MATCH':
        mode = 'RELATIVE' if is_relative else 'ABSOLUTE'
    elif mode == 'AUTO':
        mode = 'RELATIVE' if bpy.path.is_subdir(filepath, base_dst) else 'ABSOLUTE'
    elif mode == 'COPY':
        if copy_subdir:
            subdir_abs = os.path.join(os.path.normpath(base_dst), copy_subdir)
        else:
            subdir_abs = os.path.normpath(base_dst)

        filepath_cpy = os.path.join(subdir_abs, os.path.basename(filepath))

        copy_set.add((filepath_abs, filepath_cpy))

        filepath_abs = filepath_cpy
        mode = 'RELATIVE'
    else:
        Excaption("invalid mode given %r" % mode)

    if mode == 'ABSOLUTE':
        return filepath_abs
    elif mode == 'RELATIVE':
        return os.path.relpath(filepath_abs, base_dst)
    elif mode == 'STRIP':
        return os.path.basename(filepath_abs)


def path_reference_copy(copy_set, report=print):
    """
    Execute copying files of path_reference
    
    :arg copy_set: set of (from, to) pairs to copy.
    :type copy_set: set
    :arg report: function used for reporting warnings, takes a string argument.
    :type report: function
    """
    if not copy_set:
        return

    import os
    import shutil

    for file_src, file_dst in copy_set:
        if not os.path.exists(file_src):
            report("missing %r, not copying" % file_src)
        elif os.path.exists(file_dst) and os.path.samefile(file_src, file_dst):
            pass
        else:
            dir_to = os.path.dirname(file_dst)

            if not os.path.isdir(dir_to):
                os.makedirs(dir_to)

            shutil.copy(file_src, file_dst)
