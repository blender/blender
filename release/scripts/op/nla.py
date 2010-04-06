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


def pose_info():
    from Mathutils import Matrix

    info = {}

    obj = bpy.context.object
    pose = obj.pose

    pose_items = pose.bones.items()

    for name, pbone in pose_items:
        binfo = {}
        bone = pbone.bone

        binfo["parent"] = getattr(bone.parent, "name", None)
        binfo["bone"] = bone
        binfo["pbone"] = pbone
        binfo["matrix_local"] = bone.matrix_local.copy()
        try:
            binfo["matrix_local_inv"] = binfo["matrix_local"].copy().invert()
        except:
            binfo["matrix_local_inv"] = Matrix()

        binfo["matrix"] = bone.matrix.copy()
        binfo["matrix_pose"] = pbone.matrix.copy()
        try:
            binfo["matrix_pose_inv"] = binfo["matrix_pose"].copy().invert()
        except:
            binfo["matrix_pose_inv"] = Matrix()

        print(binfo["matrix_pose"])
        info[name] = binfo

    for name, pbone in pose_items:
        binfo = info[name]
        binfo_parent = binfo.get("parent", None)
        if binfo_parent:
            binfo_parent = info[binfo_parent]

        matrix = binfo["matrix_pose"]
        rest_matrix = binfo["matrix_local"]

        if binfo_parent:
            matrix = binfo_parent["matrix_pose_inv"] * matrix
            rest_matrix = binfo_parent["matrix_local_inv"] * rest_matrix

        matrix = rest_matrix.copy().invert() * matrix

        binfo["matrix_key"] = matrix.copy()

    return info


def bake(frame_start, frame_end, step=1, only_selected=False):
    # import nla; reload(nla); nla.bake()

    scene = bpy.context.scene
    obj = bpy.context.object
    pose = obj.pose

    info_ls = []

    frame_range = range(frame_start, frame_end + 1, step)

    # could spped this up by applying steps here too...
    for f in frame_range:
        scene.set_frame(f)

        info = pose_info()
        info_ls.append(info)
        f += 1

    action = bpy.data.actions.new("Action")

    bpy.context.object.animation_data.action = action

    pose_items = pose.bones.items()

    for name, pbone in pose_items:
        if only_selected and not pbone.selected:
            continue

        for f in frame_range:
            matrix = info_ls[int((f - frame_start) / step)][name]["matrix_key"]

            #pbone.location = matrix.translation_part()
            #pbone.rotation_quaternion = matrix.to_quat()
            pbone.matrix_local = [f for v in matrix for f in v]
            
            pbone.keyframe_insert("location", -1, f, "Location")

            rotation_mode = pbone.rotation_mode

            if rotation_mode == 'QUATERNION':
                pbone.keyframe_insert("rotation_quaternion", -1, f, "Rotation")
            elif rotation_mode == 'AXIS_ANGLE':
                pbone.keyframe_insert("rotation_axis_angle", -1, f, "Rotation")
            else: # euler, XYZ, ZXY etc
                pbone.keyframe_insert("rotation_euler", -1, f, "Rotation")

            pbone.keyframe_insert("scale", -1, f, "Scale")

    return action


from bpy.props import *


class BakeAction(bpy.types.Operator):
    '''Add a torus mesh'''
    bl_idname = "nla.bake"
    bl_label = "Bake Action"
    bl_options = {'REGISTER', 'UNDO'}

    frame_start = IntProperty(name="Start Frame",
            description="Start frame for baking",
            default=1, min=1, max=300000)
    frame_end = IntProperty(name="End Frame",
            description="End frame for baking",
            default=250, min=1, max=300000)
    step = IntProperty(name="Frame Step",
            description="Frame Step",
            default=1, min=1, max=120)
    only_selected = BoolProperty(name="Only Selected",
            default=True)

    def execute(self, context):
        props = self.properties

        action = bake(props.frame_start, props.frame_end, props.step, props.only_selected)

        # basic cleanup, could move elsewhere
        for fcu in action.fcurves:
            keyframe_points = fcu.keyframe_points
            i = 1
            while i < len(fcu.keyframe_points) - 1:
                val_prev = keyframe_points[i - 1].co[1]
                val_next = keyframe_points[i + 1].co[1]
                val = keyframe_points[i].co[1]

                if abs(val - val_prev) + abs(val - val_next) < 0.0001:
                    keyframe_points.remove(keyframe_points[i])
                else:
                    i += 1

        return {'FINISHED'}

    def invoke(self, context, event):
        wm = context.manager
        return wm.invoke_props_dialog(self)


#def menu_func(self, context):
#    self.layout.operator(BakeAction.bl_idname, text="Bake Armature Action")


def register():
    bpy.types.register(BakeAction)
    # bpy.types.INFO_MT_mesh_add.append(menu_func)


def unregister():
    bpy.types.unregister(BakeAction)
    # bpy.types.INFO_MT_mesh_add.remove(menu_func)

if __name__ == "__main__":
    register()
