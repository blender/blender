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

def _read(context, filepath, frame_start, frame_end, global_scale=1.0):

    from mathutils import Matrix, Vector, Euler
    from math import degrees

    file = open(filepath, "w")

    obj = context.object
    arm = obj.data

    # Build a dictionary of bone children.
    # None is for parentless bones
    bone_children = {None: []}

    # initialize with blank lists
    for bone in arm.bones:
        bone_children[bone.name] = []

    for bone in arm.bones:
        bone_children[getattr(bone.parent, "name", None)].append(bone.name)

    # sort the children
    for children_list in bone_children.values():
        children_list.sort()

    # bone name list in the order that the bones are written
    bones_serialized_names = []

    bone_locs = {}

    file.write("HIERARCHY\n")

    def write_bones_recursive(bone_name, indent):
        my_bone_children = bone_children[bone_name]

        indent_str = "\t" * indent

        bone = arm.bones[bone_name]
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
        file.write("%s\tOFFSET %.6f %.6f %.6f\n" % (indent_str, loc.x * global_scale, loc.y * global_scale, loc.z * global_scale))
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
            file.write("%s\t\tOFFSET %.6f %.6f %.6f\n" % (indent_str, loc.x * global_scale, loc.y * global_scale, loc.z * global_scale))
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
    # so we can write motion

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
        "rest_local_imat",  # rest_local_mat inverted
        "prev_euler",  # last used euler to preserve euler compability in between keyframes
        )
        def __init__(self, bone_name):
            self.name = bone_name
            self.rest_bone = arm.bones[bone_name]
            self.pose_bone = obj.pose.bones[bone_name]

            self.pose_mat = self.pose_bone.matrix

            mat = self.rest_bone.matrix
            self.rest_arm_mat = self.rest_bone.matrix_local
            self.rest_local_mat = self.rest_bone.matrix

            # inverted mats
            self.pose_imat = self.pose_mat.copy().invert()
            self.rest_arm_imat = self.rest_arm_mat.copy().invert()
            self.rest_local_imat = self.rest_local_mat.copy().invert()

            self.parent = None
            self.prev_euler = Euler((0.0, 0.0, 0.0))

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

    scene = bpy.context.scene

    file.write("MOTION\n")
    file.write("Frames: %d\n" % (frame_end - frame_start + 1))
    file.write("Frame Time: %.6f\n" % (1.0 / (scene.render.fps / scene.render.fps_base)))

    for frame in range(frame_start, frame_end + 1):
        scene.frame_set(frame)

        for dbone in bones_decorated:
            dbone.update_posedata()

        for dbone in bones_decorated:
            trans = Matrix.Translation(dbone.rest_bone.head_local)
            itrans = Matrix.Translation(-dbone.rest_bone.head_local)

            if  dbone.parent:
                mat_final = dbone.parent.rest_arm_mat * dbone.parent.pose_imat * dbone.pose_mat * dbone.rest_arm_imat
                mat_final = itrans * mat_final * trans
                loc = mat_final.translation_part() + (dbone.rest_bone.head_local - dbone.parent.rest_bone.head_local)
            else:
                mat_final = dbone.pose_mat * dbone.rest_arm_imat
                mat_final = itrans * mat_final * trans
                loc = mat_final.translation_part() + dbone.rest_bone.head

            # keep eulers compatible, no jumping on interpolation.
            rot = mat_final.rotation_part().invert().to_euler('XYZ', dbone.prev_euler)

            file.write("%.6f %.6f %.6f " % (loc * global_scale)[:])
            file.write("%.6f %.6f %.6f " % (-degrees(rot[0]), -degrees(rot[1]), -degrees(rot[2])))

            dbone.prev_euler = rot

        file.write("\n")

    file.close()

    print("BVH Exported: %s frames:%d\n" % (filepath, frame_end - frame_start + 1))


def save(operator, context, filepath="",
          frame_start=-1,
          frame_end=-1,
          global_scale=1.0,
          ):

    _read(context, filepath,
           frame_start=frame_start,
           frame_end=frame_end,
           global_scale=global_scale,
           )

    return {'FINISHED'}


if __name__ == "__main__":
    scene = bpy.context.scene
    _read(bpy.data.filepath.rstrip(".blend") + ".bvh", bpy.context.object, scene.frame_start, scene.frame_end, 1.0)
