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

# Script copyright (C) Campbell Barton
# fixes from Andrea Rugliancich

import bpy
from mathutils import Matrix, Vector


def bvh_export(filepath, obj, pref_startframe, pref_endframe, pref_scale=1.0):

    # Window.EditMode(0)

    file = open(filepath, "w")

    # bvh_nodes = {}
    arm_data = obj.data
    bones = arm_data.bones.values()

    # Build a dictionary of bone children.
    # None is for parentless bones
    bone_children = {None: []}

    # initialize with blank lists
    for bone in bones:
        bone_children[bone.name] = []

    for bone in bones:
        parent = bone.parent
        bone_name = bone.name
        if parent:
            bone_children[parent.name].append(bone_name)
        else:  # root node
            bone_children[None].append(bone_name)

    # sort the children
    for children_list in bone_children.values():
        children_list.sort()

    # build a (name:bone) mapping dict
    bone_dict = {}
    for bone in bones:
        bone_dict[bone.name] = bone

    # bone name list in the order that the bones are written
    bones_serialized_names = []

    bone_locs = {}

    file.write("HIERARCHY\n")

    def write_bones_recursive(bone_name, indent):
        my_bone_children = bone_children[bone_name]

        indent_str = "\t" * indent

        bone = bone_dict[bone_name]
        loc = bone.head_local
        bone_locs[bone_name] = loc

        # make relative if we can
        if bone.parent:
            loc = loc - bone_locs[bone.parent.name]

        if indent:
            file.write("%sJOINT %s\n" % (indent_str, bone_name))
        else:
            file.write("%sROOT %s\n" % (indent_str, bone_name))

        file.write("%s{\n" % indent_str)
        file.write("%s\tOFFSET %.6f %.6f %.6f\n" % (indent_str, loc.x * pref_scale, loc.y * pref_scale, loc.z * pref_scale))
        file.write("%s\tCHANNELS 6 Xposition Yposition Zposition Xrotation Yrotation Zrotation\n" % indent_str)

        if my_bone_children:
            # store the location for the children
            # to het their relative offset

            # Write children
            for child_bone in my_bone_children:
                bones_serialized_names.append(child_bone)
                write_bones_recursive(child_bone, indent + 1)

        else:
            # Write the bone end.
            file.write("%s\tEnd Site\n" % indent_str)
            file.write("%s\t{\n" % indent_str)
            loc = bone.tail_local - bone_locs[bone_name]
            file.write("%s\t\tOFFSET %.6f %.6f %.6f\n" % (indent_str, loc.x * pref_scale, loc.y * pref_scale, loc.z * pref_scale))
            file.write("%s\t}\n" % indent_str)

        file.write("%s}\n" % indent_str)

    if len(bone_children[None]) == 1:
        key = bone_children[None][0]
        bones_serialized_names.append(key)
        indent = 0

        write_bones_recursive(key, indent)

    else:
        # Write a dummy parent node
        file.write("ROOT %s\n" % key)
        file.write("{\n")
        file.write("\tOFFSET 0.0 0.0 0.0\n")
        file.write("\tCHANNELS 0\n")  # Xposition Yposition Zposition Xrotation Yrotation Zrotation
        key = None
        indent = 1

        write_bones_recursive(key, indent)

        file.write("}\n")

    # redefine bones as sorted by bones_serialized_names
    # se we can write motion
    pose_dict = obj.pose.bones
    #pose_bones = [(pose_dict[bone_name], bone_dict[bone_name].matrix_local.copy().invert()) for bone_name in  bones_serialized_names]

    class decorated_bone(object):
        __slots__ = (\
        "name",  # bone name, used as key in many places
        "parent",  # decorated bone parent, set in a later loop
        "rest_bone",  # blender armature bone
        "pose_bone",  # blender pose bone
        "pose_mat",  # blender pose matrix
        "rest_arm_mat",  # blender rest matrix (armature space)
        "rest_local_mat",  # blender rest batrix (local space)
        "pose_imat",  # pose_mat inverted
        "rest_arm_imat",  # rest_arm_mat inverted
        "rest_local_imat")  # rest_local_mat inverted

        def __init__(self, bone_name):
            self.name = bone_name
            self.rest_bone = bone_dict[bone_name]
            self.pose_bone = pose_dict[bone_name]

            self.pose_mat = self.pose_bone.matrix

            mat = self.rest_bone.matrix
            self.rest_arm_mat = self.rest_bone.matrix_local
            self.rest_local_mat = self.rest_bone.matrix

            # inverted mats
            self.pose_imat = self.pose_mat.copy().invert()
            self.rest_arm_imat = self.rest_arm_mat.copy().invert()
            self.rest_local_imat = self.rest_local_mat.copy().invert()

            self.parent = None

        def update_posedata(self):
            self.pose_mat = self.pose_bone.matrix
            self.pose_imat = self.pose_mat.copy().invert()

        def __repr__(self):
            if self.parent:
                return "[\"%s\" child on \"%s\"]\n" % (self.name, self.parent.name)
            else:
                return "[\"%s\" root bone]\n" % (self.name)

    bones_decorated = [decorated_bone(bone_name) for bone_name in  bones_serialized_names]

    # Assign parents
    bones_decorated_dict = {}
    for dbone in bones_decorated:
        bones_decorated_dict[dbone.name] = dbone

    for dbone in bones_decorated:
        parent = dbone.rest_bone.parent
        if parent:
            dbone.parent = bones_decorated_dict[parent.name]
    del bones_decorated_dict
    # finish assigning parents

    file.write("MOTION\n")
    file.write("Frames: %d\n" % (pref_endframe - pref_startframe + 1))
    file.write("Frame Time: %.6f\n" % 0.03)

    scene = bpy.context.scene

    triple = "%.6f %.6f %.6f "
    for frame in range(pref_startframe, pref_endframe + 1):
        scene.frame_set(frame)
        for dbone in bones_decorated:
            dbone.update_posedata()
        for dbone in bones_decorated:
            if  dbone.parent:
                trans = Matrix.Translation(dbone.rest_bone.head_local)
                itrans = Matrix.Translation(-dbone.rest_bone.head_local)
                mat2 = dbone.rest_arm_imat * dbone.pose_mat * dbone.parent.pose_imat * dbone.parent.rest_arm_mat
                mat2 = trans * mat2 * itrans
                myloc = mat2.translation_part() + (dbone.rest_bone.head_local - dbone.parent.rest_bone.head_local)
                rot = mat2.copy().transpose().to_euler()
            else:
                trans = Matrix.Translation(dbone.rest_bone.head_local)
                itrans = Matrix.Translation(-dbone.rest_bone.head_local)
                mat2 = dbone.rest_arm_imat * dbone.pose_mat
                mat2 = trans * mat2 * itrans
                myloc = mat2.translation_part() + dbone.rest_bone.head_local
                rot = mat2.copy().transpose().to_euler()

            file.write(triple % (myloc[0] * pref_scale, myloc[1] * pref_scale, myloc[2] * pref_scale))
            file.write(triple % (-rot[0], -rot[1], -rot[2]))

        file.write("\n")

    numframes = pref_endframe - pref_startframe + 1
    file.close()

    print("BVH Exported: %s frames:%d\n" % (filepath, numframes))

bvh_export("/foo.bvh", bpy.context.object, 1, 190, 1.0)

'''
def bvh_export_ui(filepath):
    # Dont overwrite
    if not BPyMessages.Warning_SaveOver(filepath):
        return

    scn = Scene.GetCurrent()
    ob_act = scn.objects.active
    if not ob_act or ob_act.type != 'Armature':
        BPyMessages.Error_NoArmatureActive()

    arm_ob = scn.objects.active

    if not arm_ob or arm_ob.type != 'Armature':
        Blender.Draw.PupMenu('No Armature object selected.')
        return

    ctx = scn.getRenderingContext()
    orig_frame = Blender.Get('curframe')
    pref_startframe = Blender.Draw.Create(int(ctx.startFrame()))
    pref_endframe = Blender.Draw.Create(int(ctx.endFrame()))

    block = [\
    ("Start Frame: ", pref_startframe, 1, 30000, "Start Bake from what frame?: Default 1"),\
    ("End Frame: ", pref_endframe, 1, 30000, "End Bake on what Frame?"),\
    ]

    if not Blender.Draw.PupBlock("Export MDD", block):
        return

    pref_startframe, pref_endframe = \
        min(pref_startframe.val, pref_endframe.val),\
        max(pref_startframe.val, pref_endframe.val)

    bvh_export(filepath, ob_act, pref_startframe, pref_endframe)
    Blender.Set('curframe', orig_frame)

if __name__ == '__main__':
    Blender.Window.FileSelector(bvh_export_ui, 'EXPORT BVH', sys.makename(ext='.bvh'))
'''
