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
from bpy.props import StringProperty, BoolProperty


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
