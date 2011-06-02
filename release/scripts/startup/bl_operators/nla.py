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
    from mathutils import Matrix

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
            binfo["matrix_local_inv"] = binfo["matrix_local"].inverted()
        except:
            binfo["matrix_local_inv"] = Matrix()

        binfo["matrix"] = bone.matrix.copy()
        binfo["matrix_pose"] = pbone.matrix.copy()
        try:
            binfo["matrix_pose_inv"] = binfo["matrix_pose"].inverted()
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

        matrix = rest_matrix.inverted() * matrix

        binfo["matrix_key"] = matrix.copy()

    return info


def bake(frame_start, frame_end, step=1, only_selected=False):
    scene = bpy.context.scene
    obj = bpy.context.object
    pose = obj.pose

    info_ls = []

    frame_range = range(frame_start, frame_end + 1, step)

    # could spped this up by applying steps here too...
    for f in frame_range:
        scene.frame_set(f)

        info = pose_info()
        info_ls.append(info)
        f += 1

    action = bpy.data.actions.new("Action")

    bpy.context.object.animation_data.action = action

    pose_items = pose.bones.items()

    for name, pbone in pose_items:
        if only_selected and not pbone.bone.select:
            continue

        for f in frame_range:
            matrix = info_ls[int((f - frame_start) / step)][name]["matrix_key"]

            #pbone.location = matrix.to_translation()
            #pbone.rotation_quaternion = matrix.to_quaternion()
            pbone.matrix_basis = matrix

            pbone.keyframe_insert("location", -1, f, name)

            rotation_mode = pbone.rotation_mode

            if rotation_mode == 'QUATERNION':
                pbone.keyframe_insert("rotation_quaternion", -1, f, name)
            elif rotation_mode == 'AXIS_ANGLE':
                pbone.keyframe_insert("rotation_axis_angle", -1, f, name)
            else:  # euler, XYZ, ZXY etc
                pbone.keyframe_insert("rotation_euler", -1, f, name)

            pbone.keyframe_insert("scale", -1, f, name)

    return action


from bpy.props import IntProperty, BoolProperty


class BakeAction(bpy.types.Operator):
    '''Bake animation to an Action'''
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

        action = bake(self.frame_start, self.frame_end, self.step, self.only_selected)

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
        wm = context.window_manager
        return wm.invoke_props_dialog(self)
        
#################################

class ClearUselessActions(bpy.types.Operator):
    '''Mark actions with no F-Curves for deletion after save+reload of file preserving "action libraries"'''
    bl_idname = "anim.clear_useless_actions"
    bl_label = "Clear Useless Actions"
    bl_options = {'REGISTER', 'UNDO'}
    
    only_unused = BoolProperty(name="Only Unused", 
            description="Only unused (Fake User only) actions get considered",
            default=True)
    
    @classmethod
    def poll(cls, context):
        return len(bpy.data.actions) != 0
        
    def execute(self, context):
        removed = 0
        
        for action in bpy.data.actions:
            # if only user is "fake" user...
            if ((self.only_unused is False) or 
                (action.use_fake_user and action.users == 1)):
                
                # if it has F-Curves, then it's a "action library" (i.e. walk, wave, jump, etc.) 
                # and should be left alone as that's what fake users are for!
                if not action.fcurves:
                    # mark action for deletion
                    action.user_clear()
                    removed += 1
        
        self.report({'INFO'}, "Removed %d empty and/or fake-user only Actions" % (removed))
        return {'FINISHED'}
