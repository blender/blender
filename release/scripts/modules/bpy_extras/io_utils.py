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

__all__ = (
    "ExportHelper",
    "ImportHelper",
    "OrientationHelper",
    "axis_conversion",
    "axis_conversion_ensure",
    "create_derived_objects",
    "free_derived_objects",
    "unpack_list",
    "unpack_face_list",
    "path_reference",
    "path_reference_copy",
    "path_reference_mode",
    "unique_name"
    )

import bpy
from bpy.props import StringProperty, BoolProperty, EnumProperty


def _check_axis_conversion(op):
    if hasattr(op, "axis_forward") and hasattr(op, "axis_up"):
        return axis_conversion_ensure(op,
                                      "axis_forward",
                                      "axis_up",
                                      )
    return False


class ExportHelper:
    filepath = StringProperty(
            name="File Path",
            description="Filepath used for exporting the file",
            maxlen=1024,
            subtype='FILE_PATH',
            )
    check_existing = BoolProperty(
            name="Check Existing",
            description="Check and warn on overwriting existing files",
            default=True,
            options={'HIDDEN'},
            )

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
        import os
        change_ext = False
        change_axis = _check_axis_conversion(self)

        check_extension = self.check_extension

        if check_extension is not None:
            filepath = self.filepath
            if os.path.basename(filepath):
                filepath = bpy.path.ensure_ext(filepath,
                                               self.filename_ext
                                               if check_extension
                                               else "")

                if filepath != self.filepath:
                    self.filepath = filepath
                    change_ext = True

        return (change_ext or change_axis)


class ImportHelper:
    filepath = StringProperty(
            name="File Path",
            description="Filepath used for importing the file",
            maxlen=1024,
            subtype='FILE_PATH',
            )

    def invoke(self, context, event):
        context.window_manager.fileselect_add(self)
        return {'RUNNING_MODAL'}

    def check(self, context):
        return _check_axis_conversion(self)


class OrientationHelper:

    def _update_axis_forward(self, context):
        if self.axis_forward[-1] == self.axis_up[-1]:
            self.axis_up = self.axis_up[0:-1] + 'XYZ'[('XYZ'.index(self.axis_up[-1]) + 1) % 3]

    axis_forward = EnumProperty(
            name="Forward",
            items=(('X', "X Forward", ""),
                   ('Y', "Y Forward", ""),
                   ('Z', "Z Forward", ""),
                   ('-X', "-X Forward", ""),
                   ('-Y', "-Y Forward", ""),
                   ('-Z', "-Z Forward", ""),
                   ),
            default='-Z',
            update=_update_axis_forward,
            )

    def _update_axis_up(self, context):
        if self.axis_up[-1] == self.axis_forward[-1]:
            self.axis_forward = self.axis_forward[0:-1] + 'XYZ'[('XYZ'.index(self.axis_forward[-1]) + 1) % 3]

    axis_up = EnumProperty(
            name="Up",
            items=(('X', "X Up", ""),
                   ('Y', "Y Up", ""),
                   ('Z', "Z Up", ""),
                   ('-X', "-X Up", ""),
                   ('-Y', "-Y Up", ""),
                   ('-Z', "-Z Up", ""),
                   ),
            default='Y',
            update=_update_axis_up,
            )


# Axis conversion function, not pretty LUT
# use lookup table to convert between any axis
_axis_convert_matrix = (
    ((-1.0, 0.0, 0.0), (0.0, -1.0, 0.0), (0.0, 0.0, 1.0)),
    ((-1.0, 0.0, 0.0), (0.0, 0.0, -1.0), (0.0, -1.0, 0.0)),
    ((-1.0, 0.0, 0.0), (0.0, 0.0, 1.0), (0.0, 1.0, 0.0)),
    ((-1.0, 0.0, 0.0), (0.0, 1.0, 0.0), (0.0, 0.0, -1.0)),
    ((0.0, -1.0, 0.0), (-1.0, 0.0, 0.0), (0.0, 0.0, -1.0)),
    ((0.0, 0.0, 1.0), (-1.0, 0.0, 0.0), (0.0, -1.0, 0.0)),
    ((0.0, 0.0, -1.0), (-1.0, 0.0, 0.0), (0.0, 1.0, 0.0)),
    ((0.0, 1.0, 0.0), (-1.0, 0.0, 0.0), (0.0, 0.0, 1.0)),
    ((0.0, -1.0, 0.0), (0.0, 0.0, 1.0), (-1.0, 0.0, 0.0)),
    ((0.0, 0.0, -1.0), (0.0, -1.0, 0.0), (-1.0, 0.0, 0.0)),
    ((0.0, 0.0, 1.0), (0.0, 1.0, 0.0), (-1.0, 0.0, 0.0)),
    ((0.0, 1.0, 0.0), (0.0, 0.0, -1.0), (-1.0, 0.0, 0.0)),
    ((0.0, -1.0, 0.0), (0.0, 0.0, -1.0), (1.0, 0.0, 0.0)),
    ((0.0, 0.0, 1.0), (0.0, -1.0, 0.0), (1.0, 0.0, 0.0)),
    ((0.0, 0.0, -1.0), (0.0, 1.0, 0.0), (1.0, 0.0, 0.0)),
    ((0.0, 1.0, 0.0), (0.0, 0.0, 1.0), (1.0, 0.0, 0.0)),
    ((0.0, -1.0, 0.0), (1.0, 0.0, 0.0), (0.0, 0.0, 1.0)),
    ((0.0, 0.0, -1.0), (1.0, 0.0, 0.0), (0.0, -1.0, 0.0)),
    ((0.0, 0.0, 1.0), (1.0, 0.0, 0.0), (0.0, 1.0, 0.0)),
    ((0.0, 1.0, 0.0), (1.0, 0.0, 0.0), (0.0, 0.0, -1.0)),
    ((1.0, 0.0, 0.0), (0.0, -1.0, 0.0), (0.0, 0.0, -1.0)),
    ((1.0, 0.0, 0.0), (0.0, 0.0, 1.0), (0.0, -1.0, 0.0)),
    ((1.0, 0.0, 0.0), (0.0, 0.0, -1.0), (0.0, 1.0, 0.0)),
    )

# store args as a single int
# (X Y Z -X -Y -Z) --> (0, 1, 2, 3, 4, 5)
# each value is ((src_forward, src_up), (dst_forward, dst_up))
# where all 4 values are or'd into a single value...
#    (i1<<0 | i1<<3 | i1<<6 | i1<<9)
_axis_convert_lut = (
    {0x8C8, 0x4D0, 0x2E0, 0xAE8, 0x701, 0x511, 0x119, 0xB29, 0x682, 0x88A,
     0x09A, 0x2A2, 0x80B, 0x413, 0x223, 0xA2B, 0x644, 0x454, 0x05C, 0xA6C,
     0x745, 0x94D, 0x15D, 0x365},
    {0xAC8, 0x8D0, 0x4E0, 0x2E8, 0x741, 0x951, 0x159, 0x369, 0x702, 0xB0A,
     0x11A, 0x522, 0xA0B, 0x813, 0x423, 0x22B, 0x684, 0x894, 0x09C, 0x2AC,
     0x645, 0xA4D, 0x05D, 0x465},
    {0x4C8, 0x2D0, 0xAE0, 0x8E8, 0x681, 0x291, 0x099, 0x8A9, 0x642, 0x44A,
     0x05A, 0xA62, 0x40B, 0x213, 0xA23, 0x82B, 0x744, 0x354, 0x15C, 0x96C,
     0x705, 0x50D, 0x11D, 0xB25},
    {0x2C8, 0xAD0, 0x8E0, 0x4E8, 0x641, 0xA51, 0x059, 0x469, 0x742, 0x34A,
     0x15A, 0x962, 0x20B, 0xA13, 0x823, 0x42B, 0x704, 0xB14, 0x11C, 0x52C,
     0x685, 0x28D, 0x09D, 0x8A5},
    {0x708, 0xB10, 0x120, 0x528, 0x8C1, 0xAD1, 0x2D9, 0x4E9, 0x942, 0x74A,
     0x35A, 0x162, 0x64B, 0xA53, 0x063, 0x46B, 0x804, 0xA14, 0x21C, 0x42C,
     0x885, 0x68D, 0x29D, 0x0A5},
    {0xB08, 0x110, 0x520, 0x728, 0x941, 0x151, 0x359, 0x769, 0x802, 0xA0A,
     0x21A, 0x422, 0xA4B, 0x053, 0x463, 0x66B, 0x884, 0x094, 0x29C, 0x6AC,
     0x8C5, 0xACD, 0x2DD, 0x4E5},
    {0x508, 0x710, 0xB20, 0x128, 0x881, 0x691, 0x299, 0x0A9, 0x8C2, 0x4CA,
     0x2DA, 0xAE2, 0x44B, 0x653, 0xA63, 0x06B, 0x944, 0x754, 0x35C, 0x16C,
     0x805, 0x40D, 0x21D, 0xA25},
    {0x108, 0x510, 0x720, 0xB28, 0x801, 0x411, 0x219, 0xA29, 0x882, 0x08A,
     0x29A, 0x6A2, 0x04B, 0x453, 0x663, 0xA6B, 0x8C4, 0x4D4, 0x2DC, 0xAEC,
     0x945, 0x14D, 0x35D, 0x765},
    {0x748, 0x350, 0x160, 0x968, 0xAC1, 0x2D1, 0x4D9, 0x8E9, 0xA42, 0x64A,
     0x45A, 0x062, 0x68B, 0x293, 0x0A3, 0x8AB, 0xA04, 0x214, 0x41C, 0x82C,
     0xB05, 0x70D, 0x51D, 0x125},
    {0x948, 0x750, 0x360, 0x168, 0xB01, 0x711, 0x519, 0x129, 0xAC2, 0x8CA,
     0x4DA, 0x2E2, 0x88B, 0x693, 0x2A3, 0x0AB, 0xA44, 0x654, 0x45C, 0x06C,
     0xA05, 0x80D, 0x41D, 0x225},
    {0x348, 0x150, 0x960, 0x768, 0xA41, 0x051, 0x459, 0x669, 0xA02, 0x20A,
     0x41A, 0x822, 0x28B, 0x093, 0x8A3, 0x6AB, 0xB04, 0x114, 0x51C, 0x72C,
     0xAC5, 0x2CD, 0x4DD, 0x8E5},
    {0x148, 0x950, 0x760, 0x368, 0xA01, 0x811, 0x419, 0x229, 0xB02, 0x10A,
     0x51A, 0x722, 0x08B, 0x893, 0x6A3, 0x2AB, 0xAC4, 0x8D4, 0x4DC, 0x2EC,
     0xA45, 0x04D, 0x45D, 0x665},
    {0x688, 0x890, 0x0A0, 0x2A8, 0x4C1, 0x8D1, 0xAD9, 0x2E9, 0x502, 0x70A,
     0xB1A, 0x122, 0x74B, 0x953, 0x163, 0x36B, 0x404, 0x814, 0xA1C, 0x22C,
     0x445, 0x64D, 0xA5D, 0x065},
    {0x888, 0x090, 0x2A0, 0x6A8, 0x501, 0x111, 0xB19, 0x729, 0x402, 0x80A,
     0xA1A, 0x222, 0x94B, 0x153, 0x363, 0x76B, 0x444, 0x054, 0xA5C, 0x66C,
     0x4C5, 0x8CD, 0xADD, 0x2E5},
    {0x288, 0x690, 0x8A0, 0x0A8, 0x441, 0x651, 0xA59, 0x069, 0x4C2, 0x2CA,
     0xADA, 0x8E2, 0x34B, 0x753, 0x963, 0x16B, 0x504, 0x714, 0xB1C, 0x12C,
     0x405, 0x20D, 0xA1D, 0x825},
    {0x088, 0x290, 0x6A0, 0x8A8, 0x401, 0x211, 0xA19, 0x829, 0x442, 0x04A,
     0xA5A, 0x662, 0x14B, 0x353, 0x763, 0x96B, 0x4C4, 0x2D4, 0xADC, 0x8EC,
     0x505, 0x10D, 0xB1D, 0x725},
    {0x648, 0x450, 0x060, 0xA68, 0x2C1, 0x4D1, 0x8D9, 0xAE9, 0x282, 0x68A,
     0x89A, 0x0A2, 0x70B, 0x513, 0x123, 0xB2B, 0x204, 0x414, 0x81C, 0xA2C,
     0x345, 0x74D, 0x95D, 0x165},
    {0xA48, 0x650, 0x460, 0x068, 0x341, 0x751, 0x959, 0x169, 0x2C2, 0xACA,
     0x8DA, 0x4E2, 0xB0B, 0x713, 0x523, 0x12B, 0x284, 0x694, 0x89C, 0x0AC,
     0x205, 0xA0D, 0x81D, 0x425},
    {0x448, 0x050, 0xA60, 0x668, 0x281, 0x091, 0x899, 0x6A9, 0x202, 0x40A,
     0x81A, 0xA22, 0x50B, 0x113, 0xB23, 0x72B, 0x344, 0x154, 0x95C, 0x76C,
     0x2C5, 0x4CD, 0x8DD, 0xAE5},
    {0x048, 0xA50, 0x660, 0x468, 0x201, 0xA11, 0x819, 0x429, 0x342, 0x14A,
     0x95A, 0x762, 0x10B, 0xB13, 0x723, 0x52B, 0x2C4, 0xAD4, 0x8DC, 0x4EC,
     0x285, 0x08D, 0x89D, 0x6A5},
    {0x808, 0xA10, 0x220, 0x428, 0x101, 0xB11, 0x719, 0x529, 0x142, 0x94A,
     0x75A, 0x362, 0x8CB, 0xAD3, 0x2E3, 0x4EB, 0x044, 0xA54, 0x65C, 0x46C,
     0x085, 0x88D, 0x69D, 0x2A5},
    {0xA08, 0x210, 0x420, 0x828, 0x141, 0x351, 0x759, 0x969, 0x042, 0xA4A,
     0x65A, 0x462, 0xACB, 0x2D3, 0x4E3, 0x8EB, 0x084, 0x294, 0x69C, 0x8AC,
     0x105, 0xB0D, 0x71D, 0x525},
    {0x408, 0x810, 0xA20, 0x228, 0x081, 0x891, 0x699, 0x2A9, 0x102, 0x50A,
     0x71A, 0xB22, 0x4CB, 0x8D3, 0xAE3, 0x2EB, 0x144, 0x954, 0x75C, 0x36C,
     0x045, 0x44D, 0x65D, 0xA65},
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

    if from_forward[-1] == from_up[-1] or to_forward[-1] == to_up[-1]:
        raise Exception("Invalid axis arguments passed, "
                        "can't use up/forward on the same axis")

    value = reduce(int.__or__, (_axis_convert_num[a] << (i * 3)
                   for i, a in enumerate((from_forward,
                                          from_up,
                                          to_forward,
                                          to_up,
                                          ))))

    for i, axis_lut in enumerate(_axis_convert_lut):
        if value in axis_lut:
            return Matrix(_axis_convert_matrix[i])
    assert(0)


def axis_conversion_ensure(operator, forward_attr, up_attr):
    """
    Function to ensure an operator has valid axis conversion settings, intended
    to be used from :class:`bpy.types.Operator.check`.

    :arg operator: the operator to access axis attributes from.
    :type operator: :class:`bpy.types.Operator`
    :arg forward_attr: attribute storing the forward axis
    :type forward_attr: string
    :arg up_attr: attribute storing the up axis
    :type up_attr: string
    :return: True if the value was modified.
    :rtype: boolean
    """
    def validate(axis_forward, axis_up):
        if axis_forward[-1] == axis_up[-1]:
            axis_up = axis_up[0:-1] + 'XYZ'[('XYZ'.index(axis_up[-1]) + 1) % 3]

        return axis_forward, axis_up

    axis = getattr(operator, forward_attr), getattr(operator, up_attr)
    axis_new = validate(*axis)

    if axis != axis_new:
        setattr(operator, forward_attr, axis_new[0])
        setattr(operator, up_attr, axis_new[1])

        return True
    else:
        return False


# return a tuple (free, object list), free is True if memory should be freed
# later with free_derived_objects()
def create_derived_objects(scene, ob):
    if ob.parent and ob.parent.dupli_type in {'VERTS', 'FACES'}:
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
    flat_list_extend = flat_list.extend  # a tiny bit faster
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
        else:  # assume quad
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
               ('RELATIVE', "Relative", "Always write relative paths "
                                        "(where possible)"),
               ('MATCH', "Match", "Match Absolute/Relative "
                                  "setting with input path"),
               ('STRIP', "Strip Path", "Filename only"),
               ('COPY', "Copy", "Copy the file to the destination path "
                                "(or subdirectory)"),
               ),
        default='AUTO',
        )


def path_reference(filepath,
                   base_src,
                   base_dst,
                   mode='AUTO',
                   copy_subdir="",
                   copy_set=None,
                   library=None,
                   ):
    """
    Return a filepath relative to a destination directory, for use with
    exporters.

    :arg filepath: the file path to return,
       supporting blenders relative '//' prefix.
    :type filepath: string
    :arg base_src: the directory the *filepath* is relative too
       (normally the blend file).
    :type base_src: string
    :arg base_dst: the directory the *filepath* will be referenced from
       (normally the export path).
    :type base_dst: string
    :arg mode: the method used get the path in
       ['AUTO', 'ABSOLUTE', 'RELATIVE', 'MATCH', 'STRIP', 'COPY']
    :type mode: string
    :arg copy_subdir: the subdirectory of *base_dst* to use when mode='COPY'.
    :type copy_subdir: string
    :arg copy_set: collect from/to pairs when mode='COPY',
       pass to *path_reference_copy* when exporting is done.
    :type copy_set: set
    :arg library: The library this path is relative to.
    :type library: :class:`bpy.types.Library` or None
    :return: the new filepath.
    :rtype: string
    """
    import os
    is_relative = filepath.startswith("//")
    filepath_abs = bpy.path.abspath(filepath, base_src, library)
    filepath_abs = os.path.normpath(filepath_abs)

    if mode in {'ABSOLUTE', 'RELATIVE', 'STRIP'}:
        pass
    elif mode == 'MATCH':
        mode = 'RELATIVE' if is_relative else 'ABSOLUTE'
    elif mode == 'AUTO':
        mode = ('RELATIVE'
                if bpy.path.is_subdir(filepath_abs, base_dst)
                else 'ABSOLUTE')
    elif mode == 'COPY':
        subdir_abs = os.path.normpath(base_dst)
        if copy_subdir:
            subdir_abs = os.path.join(subdir_abs, copy_subdir)

        filepath_cpy = os.path.join(subdir_abs, os.path.basename(filepath))

        copy_set.add((filepath_abs, filepath_cpy))

        filepath_abs = filepath_cpy
        mode = 'RELATIVE'
    else:
        raise Exception("invalid mode given %r" % mode)

    if mode == 'ABSOLUTE':
        return filepath_abs
    elif mode == 'RELATIVE':
        # can't always find the relative path
        # (between drive letters on windows)
        try:
            return os.path.relpath(filepath_abs, base_dst)
        except ValueError:
            return filepath_abs
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

            try:
                os.makedirs(dir_to, exist_ok=True)
            except:
                import traceback
                traceback.print_exc()

            try:
                shutil.copy(file_src, file_dst)
            except:
                import traceback
                traceback.print_exc()


def unique_name(key, name, name_dict, name_max=-1, clean_func=None, sep="."):
    """
    Helper function for storing unique names which may have special characters
    stripped and restricted to a maximum length.

    :arg key: unique item this name belongs to, name_dict[key] will be reused
       when available.
       This can be the object, mesh, material, etc instance its self.
    :type key: any hashable object associated with the *name*.
    :arg name: The name used to create a unique value in *name_dict*.
    :type name: string
    :arg name_dict: This is used to cache namespace to ensure no collisions
       occur, this should be an empty dict initially and only modified by this
       function.
    :type name_dict: dict
    :arg clean_func: Function to call on *name* before creating a unique value.
    :type clean_func: function
    :arg sep: Separator to use when between the name and a number when a
       duplicate name is found.
    :type sep: string
    """
    name_new = name_dict.get(key)
    if name_new is None:
        count = 1
        name_dict_values = name_dict.values()
        name_new = name_new_orig = (name if clean_func is None
                                    else clean_func(name))

        if name_max == -1:
            while name_new in name_dict_values:
                name_new = "%s%s%03d" % (name_new_orig, sep, count)
                count += 1
        else:
            name_new = name_new[:name_max]
            while name_new in name_dict_values:
                count_str = "%03d" % count
                name_new = "%.*s%s%s" % (name_max - (len(count_str) + 1),
                                         name_new_orig,
                                         sep,
                                         count_str,
                                         )
                count += 1

        name_dict[key] = name_new

    return name_new
