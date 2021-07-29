'''
Quat/Euler Rotation Mode Converter v0.1

This script/addon:
    - Changes (pose) bone rotation mode
    - Converts keyframes from one rotation mode to another
    - Creates fcurves/keyframes in target rotation mode
    - Deletes previous fcurves/keyframes.
    - Converts multiple bones
    - Converts multiple Actions

TO-DO:
    - To convert object's rotation mode (alrady done in Mutant Bob script,
		but not done in this one.
    - To understand "EnumProperty" and write it well.
    - Code clean
    - ...

GitHub: https://github.com/MarioMey/rotation_mode_addon/
BlenderArtist thread: http://blenderartists.org/forum/showthread.php?388197-Quat-Euler-Rotation-Mode-Converter

Mutant Bob did the "hard code" of this script. Thanks him!
blender.stackexchange.com/questions/40711/how-to-convert-quaternions-keyframes-to-euler-ones-in-several-actions


'''

# bl_info = {
#     "name": "Quat/Euler Rotation Mode Converter",
#     "author": "Mario Mey / Mutant Bob",
#     "version": (0, 1),
#     "blender": (2, 76, 0),
#     'location': '',
#     "description": "Converts bones rotation mode",
#     "warning": "",
#     "wiki_url": "",
#     "tracker_url": "https://github.com/MarioMey/rotation_mode_addon/",
#     "category": "Animation"}

import bpy

order_list = ['QUATERNION', 'XYZ', 'XZY', 'YXZ', 'YZX', 'ZXY', 'ZYX']


class convert():
    def get_or_create_fcurve(self, action, data_path, array_index=-1, group=None):
        for fc in action.fcurves:
            if fc.data_path == data_path and (array_index < 0 or fc.array_index == array_index):
                return fc

        fc = action.fcurves.new(data_path, array_index)
        fc.group = group
        return fc

    def add_keyframe_quat(self, action, quat, frame, bone_prefix, group):
        for i in range(len(quat)):
            fc = self.get_or_create_fcurve(action, bone_prefix + "rotation_quaternion", i, group)
            pos = len(fc.keyframe_points)
            fc.keyframe_points.add(1)
            fc.keyframe_points[pos].co = [frame, quat[i]]
            fc.update()

    def add_keyframe_euler(self, action, euler, frame, bone_prefix, group):
        for i in range(len(euler)):
            fc = self.get_or_create_fcurve(action, bone_prefix + "rotation_euler", i, group)
            pos = len(fc.keyframe_points)
            fc.keyframe_points.add(1)
            fc.keyframe_points[pos].co = [frame, euler[i]]
            fc.update()

    def frames_matching(self, action, data_path):
        frames = set()
        for fc in action.fcurves:
            if fc.data_path == data_path:
                fri = [kp.co[0] for kp in fc.keyframe_points]
                frames.update(fri)
        return frames

    # Converts only one group/bone in one action - Quat to euler
    def group_qe(self, obj, action, bone, bone_prefix, order):

        pose_bone = bone
        data_path = bone_prefix + "rotation_quaternion"
        frames = self.frames_matching(action, data_path)
        group = action.groups[bone.name]

        for fr in frames:
            quat = bone.rotation_quaternion.copy()
            for fc in action.fcurves:
                if fc.data_path == data_path:
                    quat[fc.array_index] = fc.evaluate(fr)
            euler = quat.to_euler(order)

            self.add_keyframe_euler(action, euler, fr, bone_prefix, group)
            bone.rotation_mode = order

    # Converts only one group/bone in one action - Euler to Quat
    def group_eq(self, obj, action, bone, bone_prefix, order):

        pose_bone = bone
        data_path = bone_prefix + "rotation_euler"
        frames = self.frames_matching(action, data_path)
        group = action.groups[bone.name]

        for fr in frames:
            euler = bone.rotation_euler.copy()
            for fc in action.fcurves:
                if fc.data_path == data_path:
                    euler[fc.array_index] = fc.evaluate(fr)
            quat = euler.to_quaternion()

            self.add_keyframe_quat(action, quat, fr, bone_prefix, group)
            bone.rotation_mode = order

    # One Action - One Bone
    def one_act_one_bon(self, obj, action, bone, order):
        do = False
        bone_prefix = ''

        # What kind of conversion
        cond1 = order == 'XYZ'
        cond2 = order == 'XZY'
        cond3 = order == 'YZX'
        cond4 = order == 'YXZ'
        cond5 = order == 'ZXY'
        cond6 = order == 'ZYX'

        order_euler = cond1 or cond2 or cond3 or cond4 or cond5 or cond6
        order_quat = order == 'QUATERNION'

        for fcurve in action.fcurves:
            if fcurve.group.name == bone.name:

                # If To-Euler conversion
                if order != 'QUATERNION':
                    if fcurve.data_path.endswith('rotation_quaternion'):
                        do = True
                        bone_prefix = fcurve.data_path[:-len('rotation_quaternion')]
                        break

                # If To-Quat conversion
                else:
                    if fcurve.data_path.endswith('rotation_euler'):
                        do = True
                        bone_prefix = fcurve.data_path[:-len('rotation_euler')]
                        break

        # If To-Euler conversion
        if do and order != 'QUATERNION':
            # Converts the group/bone from Quat to Euler
            self.group_qe(obj, action, bone, bone_prefix, order)

            # Removes quaternion fcurves
            for key in action.fcurves:
                if key.data_path == 'pose.bones["' + bone.name + '"].rotation_quaternion':
                    action.fcurves.remove(key)

        # If To-Quat conversion
        elif do:
            # Converts the group/bone from Euler to Quat
            self.group_eq(obj, action, bone, bone_prefix, order)

            # Removes euler fcurves
            for key in action.fcurves:
                if key.data_path == 'pose.bones["' + bone.name + '"].rotation_euler':
                    action.fcurves.remove(key)

        # Changes rotation mode to new one
        bone.rotation_mode = order

    # One Action, selected bones
    def one_act_sel_bon(self, obj, action, pose_bones, order):
        for bone in pose_bones:
            self.one_act_one_bon(obj, action, bone, order)

    # One action, all Bones (in Action)
    def one_act_every_bon(self, obj, action, order):

        # Collects pose_bones that are in the action
        pose_bones = set()
        # Checks all fcurves
        for fcurve in action.fcurves:
            # Look for the ones that has rotation_euler
            if order == 'QUATERNION':
                if fcurve.data_path.endswith('rotation_euler'):
                    # If the bone from action really exists
                    if fcurve.group.name in obj.pose.bones:
                        if obj.pose.bones[fcurve.group.name] not in pose_bones:
                            pose_bones.add(obj.pose.bones[fcurve.group.name])
                    else:
                        print(fcurve.group.name, 'does not exist in Armature. Fcurve-group is not affected')

            # Look for the ones that has rotation_quaternion
            else:
                if fcurve.data_path.endswith('rotation_quaternion'):
                    # If the bone from action really exists
                    if fcurve.group.name in obj.pose.bones:
                        if obj.pose.bones[fcurve.group.name] not in pose_bones:
                            pose_bones.add(obj.pose.bones[fcurve.group.name])
                    else:
                        print(fcurve.group.name, 'does not exist in Armature. Fcurve-group is not affected')

        # Convert current action and pose_bones that are in each action
        for bone in pose_bones:
            self.one_act_one_bon(obj, action, bone, order)

    # All Actions, selected bones
    def all_act_sel_bon(self, obj, pose_bones, order):
        for action in bpy.data.actions:
            for bone in pose_bones:
                self.one_act_one_bon(obj, action, bone, order)

    # All actions, All Bones (in each Action)
    def all_act_every_bon(self, obj, order):
        for action in bpy.data.actions:
            self.one_act_every_bon(obj, action, order)


convert = convert()


# def initSceneProperties(scn):
#
# 	bpy.types.Scene.order_list = bpy.props.EnumProperty(
# 	items = [('QUATERNION', 'QUATERNION', 'QUATERNION' ),
# 	('XYZ', 'XYZ', 'XYZ' ),
# 	('XZY', 'XZY', 'XZY' ),
# 	('YXZ', 'YXZ', 'YXZ' ),
# 	('YZX', 'YZX', 'YZX' ),
# 	('ZXY', 'ZXY', 'ZXY' ),
# 	('ZYX', 'ZYX', 'ZYX' ) ],
# 	name = "Order",
# 	description = "The target rotation mode")
#
# 	scn['order_list'] = 0
#
# 	return
#
# initSceneProperties(bpy.context.scene)


# GUI (Panel)
#
class ToolsPanel(bpy.types.Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'TOOLS'
    bl_category = "Tools"
    bl_context = "posemode"
    bl_label = 'Rigify Quat/Euler Converter'

    # draw the gui
    def draw(self, context):
        layout = self.layout
        scn = context.scene
        # ~ toolsettings = context.tool_settings

        col = layout.column(align=True)
        row = col.row(align=True)
        id_store = context.window_manager

        layout.prop(scn, 'order_list')

        if id_store.rigify_convert_only_selected:
            icon = 'OUTLINER_DATA_ARMATURE'
        else:
            icon = 'ARMATURE_DATA'

        layout.prop(id_store, 'rigify_convert_only_selected', toggle=True, icon=icon)

        col = layout.column(align=True)
        row = col.row(align=True)

        row.operator('rigify_quat2eu.current', icon='ACTION')
        row = col.row(align=True)
        row.operator('rigify_quat2eu.all', icon='NLA')


class CONVERT_OT_quat2eu_current_action(bpy.types.Operator):
    bl_label = 'Convert Current Action'
    bl_idname = 'rigify_quat2eu.current'
    bl_description = 'Converts bones in current Action'
    bl_options = {'REGISTER', 'UNDO'}

    # on mouse up:
    def invoke(self, context, event):
        self.execute(context)
        return {'FINISHED'}

    def execute(op, context):
        obj = bpy.context.active_object
        pose_bones = bpy.context.selected_pose_bones
        action = obj.animation_data.action
        order = order_list[bpy.context.scene['order_list']]
        id_store = context.window_manager

        if id_store.rigify_convert_only_selected:
            convert.one_act_sel_bon(obj, action, pose_bones, order)
        else:
            convert.one_act_every_bon(obj, action, order)

        return {'FINISHED'}


class CONVERT_OT_quat2eu_all_actions(bpy.types.Operator):
    bl_label = 'Convert All Actions'
    bl_idname = 'rigify_quat2eu.all'
    bl_description = 'Converts bones in every Action'
    bl_options = {'REGISTER', 'UNDO'}

    # on mouse up:
    def invoke(self, context, event):
        self.execute(context)
        return {'FINISHED'}

    def execute(op, context):
        obj = bpy.context.active_object
        pose_bones = bpy.context.selected_pose_bones
        order = order_list[bpy.context.scene['order_list']]
        id_store = context.window_manager

        if id_store.rigify_convert_only_selected:
            convert.all_act_sel_bon(obj, pose_bones, order)
        else:
            convert.all_act_every_bon(obj, order)

        return {'FINISHED'}


def register():
    IDStore = bpy.types.WindowManager

    items = [('QUATERNION', 'QUATERNION', 'QUATERNION'),
             ('XYZ', 'XYZ', 'XYZ'),
             ('XZY', 'XZY', 'XZY'),
             ('YXZ', 'YXZ', 'YXZ'),
             ('YZX', 'YZX', 'YZX'),
             ('ZXY', 'ZXY', 'ZXY'),
             ('ZYX', 'ZYX', 'ZYX')]

    bpy.types.Scene.order_list = bpy.props.EnumProperty(items=items, name='Convert to',
                                                        description="The target rotation mode", default='QUATERNION')

    IDStore.rigify_convert_only_selected = bpy.props.BoolProperty(
        name="Convert Only Selected", description="Convert selected bones only", default=True)

    bpy.utils.register_class(ToolsPanel)
    bpy.utils.register_class(CONVERT_OT_quat2eu_current_action)
    bpy.utils.register_class(CONVERT_OT_quat2eu_all_actions)

def unregister():
    IDStore = bpy.types.WindowManager

    bpy.utils.unregister_class(ToolsPanel)
    bpy.utils.unregister_class(CONVERT_OT_quat2eu_current_action)
    bpy.utils.unregister_class(CONVERT_OT_quat2eu_all_actions)

    del IDStore.rigify_convert_only_selected

# bpy.utils.register_module(__name__)
