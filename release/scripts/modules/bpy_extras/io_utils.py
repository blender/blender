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


# Axis conversion function, not pretty LUT
# use lookup tabes to convert between any axis
_axis_convert_matrix = (
    ((-1.0, 0.0, 0.0), (0.0, -1.0, 0.0), (0.0, 0.0, 1.0)),
    ((-1.0, 0.0, 0.0), (0.0, 0.0, -1.0), (0.0, -1.0, 0.0)),
    ((-1.0, 0.0, 0.0), (0.0, 0.0, 1.0), (0.0, 1.0, 0.0)),
    ((-1.0, 0.0, 0.0), (0.0, 1.0, 0.0), (0.0, 0.0, -1.0)),
    ((0.0, -1.0, 0.0), (-1.0, 0.0, 0.0), (0.0, 0.0, -1.0)),
    ((0.0, -1.0, 0.0), (0.0, 0.0, -1.0), (1.0, 0.0, 0.0)),
    ((0.0, -1.0, 0.0), (0.0, 0.0, 1.0), (-1.0, 0.0, 0.0)),
    ((0.0, -1.0, 0.0), (1.0, 0.0, 0.0), (0.0, 0.0, 1.0)),
    ((0.0, 0.0, -1.0), (-1.0, 0.0, 0.0), (0.0, 1.0, 0.0)),
    ((0.0, 0.0, -1.0), (0.0, -1.0, 0.0), (-1.0, 0.0, 0.0)),
    ((0.0, 0.0, -1.0), (0.0, 1.0, 0.0), (1.0, 0.0, 0.0)),
    ((0.0, 0.0, -1.0), (1.0, 0.0, 0.0), (0.0, -1.0, 0.0)),
    ((0.0, 0.0, 1.0), (-1.0, 0.0, 0.0), (0.0, -1.0, 0.0)),
    ((0.0, 0.0, 1.0), (0.0, -1.0, 0.0), (1.0, 0.0, 0.0)),
    ((0.0, 0.0, 1.0), (0.0, 1.0, 0.0), (-1.0, 0.0, 0.0)),
    ((0.0, 0.0, 1.0), (1.0, 0.0, 0.0), (0.0, 1.0, 0.0)),
    ((0.0, 1.0, 0.0), (-1.0, 0.0, 0.0), (0.0, 0.0, 1.0)),
    ((0.0, 1.0, 0.0), (0.0, 0.0, -1.0), (-1.0, 0.0, 0.0)),
    ((0.0, 1.0, 0.0), (0.0, 0.0, 1.0), (1.0, 0.0, 0.0)),
    ((0.0, 1.0, 0.0), (1.0, 0.0, 0.0), (0.0, 0.0, -1.0)),
    ((1.0, 0.0, 0.0), (0.0, -1.0, 0.0), (0.0, 0.0, -1.0)),
    ((1.0, 0.0, 0.0), (0.0, 0.0, -1.0), (0.0, 1.0, 0.0)),
    ((1.0, 0.0, 0.0), (0.0, 0.0, 1.0), (0.0, -1.0, 0.0)),
    )

# store args as a single int
# (X Y Z -X -Y -Z) --> (0, 1, 2, 3, 4, 5)
# each value is ((src_forward, src_up), (dst_forward, dst_up))
# where all 4 values are or'd into a single value...
#    (i1<<0 | i1<<3 | i1<<6 | i1<<9)
_axis_convert_lut = (
    {0x5c, 0x9a, 0x119, 0x15d, 0x20b, 0x2a2, 0x2c8, 0x365, 0x413, 0x46c, 0x4d0, 0x529, 0x644, 0x682, 0x701, 0x745, 0x823, 0x88a, 0x8e0, 0x94d, 0xa2b, 0xa54, 0xae8, 0xb11},
    {0x9c, 0xac, 0x159, 0x169, 0x22b, 0x2e8, 0x40b, 0x465, 0x4c8, 0x522, 0x684, 0x694, 0x741, 0x751, 0x813, 0x8d0, 0xa23, 0xa4d, 0xae0, 0xb0a},
    {0x99, 0xa9, 0x15c, 0x16c, 0x213, 0x2d0, 0x423, 0x44a, 0x4e0, 0x50d, 0x681, 0x691, 0x744, 0x754, 0x82b, 0x8e8, 0xa0b, 0xa62, 0xac8, 0xb25},
    {0x59, 0x85, 0x11c, 0x142, 0x223, 0x28d, 0x2e0, 0x34a, 0x42b, 0x469, 0x4e8, 0x52c, 0x641, 0x69d, 0x704, 0x75a, 0x80b, 0x8a5, 0x8c8, 0x962, 0xa13, 0xa51, 0xad0, 0xb14},
    {0xa5, 0x162, 0x21c, 0x285, 0x2d9, 0x342, 0x463, 0x46b, 0x520, 0x528, 0x68d, 0x74a, 0x804, 0x89d, 0x8c1, 0x95a, 0xa4b, 0xa53, 0xb08, 0xb10},
    {0x4b, 0x53, 0x108, 0x110, 0x29c, 0x2ac, 0x359, 0x369, 0x41a, 0x422, 0x4dd, 0x4e5, 0x663, 0x66b, 0x720, 0x728, 0x884, 0x894, 0x941, 0x951, 0xa02, 0xa0a, 0xac5, 0xacd},
    {0x63, 0x6b, 0x120, 0x128, 0x299, 0x2a9, 0x35c, 0x36c, 0x405, 0x40d, 0x4c2, 0x4ca, 0x64b, 0x653, 0x708, 0x710, 0x881, 0x891, 0x944, 0x954, 0xa1d, 0xa25, 0xada, 0xae2},
    {0x8a, 0x14d, 0x219, 0x29a, 0x2dc, 0x35d, 0x44b, 0x453, 0x508, 0x510, 0x6a2, 0x765, 0x801, 0x882, 0x8c4, 0x945, 0xa63, 0xa6b, 0xb20, 0xb28},
    {0x5a, 0x62, 0x8b, 0x11d, 0x125, 0x148, 0x22c, 0x28b, 0x293, 0x2e9, 0x348, 0x350, 0x41c, 0x42c, 0x45a, 0x4d9, 0x4e9, 0x51d, 0x642, 0x64a, 0x6a3, 0x705, 0x70d, 0x760, 0x814, 0x8a3, 0x8ab, 0x8d1, 0x960, 0x968, 0xa04, 0xa14, 0xa42, 0xac1, 0xad1, 0xb05},
    {0x54, 0xab, 0x111, 0x168, 0x21d, 0x225, 0x2da, 0x2e2, 0x45c, 0x519, 0x66c, 0x693, 0x729, 0x750, 0x805, 0x80d, 0x8c2, 0x8ca, 0xa44, 0xb01},
    {0x51, 0x93, 0x114, 0x150, 0x202, 0x20a, 0x2c5, 0x2cd, 0x459, 0x51c, 0x669, 0x6ab, 0x72c, 0x768, 0x81a, 0x822, 0x8dd, 0x8e5, 0xa41, 0xb04},
    {0x45, 0x4d, 0xa3, 0x102, 0x10a, 0x160, 0x229, 0x2a3, 0x2ab, 0x2ec, 0x360, 0x368, 0x419, 0x429, 0x445, 0x4dc, 0x4ec, 0x502, 0x65d, 0x665, 0x68b, 0x71a, 0x722, 0x748, 0x811, 0x88b, 0x893, 0x8d4, 0x948, 0x950, 0xa01, 0xa11, 0xa5d, 0xac4, 0xad4, 0xb1a},
    {0x5d, 0x65, 0xa0, 0x11a, 0x122, 0x163, 0x214, 0x2a0, 0x2a8, 0x2d1, 0x363, 0x36b, 0x404, 0x414, 0x45d, 0x4c1, 0x4d1, 0x51a, 0x645, 0x64d, 0x688, 0x702, 0x70a, 0x74b, 0x82c, 0x888, 0x890, 0x8e9, 0x94b, 0x953, 0xa1c, 0xa2c, 0xa45, 0xad9, 0xae9, 0xb02},
    {0x6c, 0x90, 0x129, 0x153, 0x21a, 0x222, 0x2dd, 0x2e5, 0x444, 0x501, 0x654, 0x6a8, 0x711, 0x76b, 0x802, 0x80a, 0x8c5, 0x8cd, 0xa5c, 0xb19},
    {0x69, 0xa8, 0x12c, 0x16b, 0x205, 0x20d, 0x2c2, 0x2ca, 0x441, 0x504, 0x651, 0x690, 0x714, 0x753, 0x81d, 0x825, 0x8da, 0x8e2, 0xa59, 0xb1c},
    {0x42, 0x4a, 0x88, 0x105, 0x10d, 0x14b, 0x211, 0x288, 0x290, 0x2d4, 0x34b, 0x353, 0x401, 0x411, 0x442, 0x4c4, 0x4d4, 0x505, 0x65a, 0x662, 0x6a0, 0x71d, 0x725, 0x763, 0x829, 0x8a0, 0x8a8, 0x8ec, 0x963, 0x96b, 0xa19, 0xa29, 0xa5a, 0xadc, 0xaec, 0xb1d},
    {0xa2, 0x165, 0x204, 0x282, 0x2c1, 0x345, 0x448, 0x450, 0x50b, 0x513, 0x68a, 0x74d, 0x81c, 0x89a, 0x8d9, 0x95d, 0xa60, 0xa68, 0xb23, 0xb2b},
    {0x60, 0x68, 0x123, 0x12b, 0x284, 0x294, 0x341, 0x351, 0x41d, 0x425, 0x4da, 0x4e2, 0x648, 0x650, 0x70b, 0x713, 0x89c, 0x8ac, 0x959, 0x969, 0xa05, 0xa0d, 0xac2, 0xaca},
    {0x48, 0x50, 0x10b, 0x113, 0x281, 0x291, 0x344, 0x354, 0x402, 0x40a, 0x4c5, 0x4cd, 0x660, 0x668, 0x723, 0x72b, 0x899, 0x8a9, 0x95c, 0x96c, 0xa1a, 0xa22, 0xadd, 0xae5},
    {0x8d, 0x14a, 0x201, 0x29d, 0x2c4, 0x35a, 0x460, 0x468, 0x523, 0x52b, 0x6a5, 0x762, 0x819, 0x885, 0x8dc, 0x942, 0xa48, 0xa50, 0xb0b, 0xb13},
    {0x44, 0x9d, 0x101, 0x15a, 0x220, 0x2a5, 0x2e3, 0x362, 0x428, 0x454, 0x4eb, 0x511, 0x65c, 0x685, 0x719, 0x742, 0x808, 0x88d, 0x8cb, 0x94a, 0xa10, 0xa6c, 0xad3, 0xb29},
    {0x84, 0x94, 0x141, 0x151, 0x210, 0x2d3, 0x420, 0x462, 0x4e3, 0x525, 0x69c, 0x6ac, 0x759, 0x769, 0x828, 0x8eb, 0xa08, 0xa4a, 0xacb, 0xb0d},
    {0x81, 0x91, 0x144, 0x154, 0x228, 0x2eb, 0x408, 0x44d, 0x4cb, 0x50a, 0x699, 0x6a9, 0x75c, 0x76c, 0x810, 0x8d3, 0xa20, 0xa65, 0xae3, 0xb22},
    )

_axis_convert_num = {'X': 0, 'Y': 1, 'Z': 2, '-X': 3, '-Y': 4, '-Z': 5}


def axis_conversion(from_forward='Y', from_up='Z', to_forward='Y', to_up='Z'):
    """
    Each argument us an axis in ['X', 'Y', 'Z', '-X', '-Y', '-Z']
    where the first 2 are a source and the second 2 are the target.
    """
    from mathutils import Matrix
    from functools import reduce

    if from_forward == to_forward and from_up == to_up:
        return Matrix().to_3x3()

    value = reduce(int.__or__, (_axis_convert_num[a] << (i * 3) for i, a in enumerate((from_up, from_forward, to_up, to_forward))))

    for i, axis_lut in enumerate(_axis_convert_lut):
        if value in axis_lut:
            return Matrix(_axis_convert_matrix[i])
    assert("internal error")


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
