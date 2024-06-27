# SPDX-FileCopyrightText: 2019-2022 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy

from itertools import count

from ...utils.naming import strip_org, make_derived_name
from ...utils.bones import put_bone, set_bone_widget_transform
from ...utils.widgets_basic import create_circle_widget
from ...utils.misc import map_list

from ...base_rig import stage

from ..widgets import create_ball_socket_widget

from .spine_rigs import BaseHeadTailRig


class Rig(BaseHeadTailRig):
    copy_rotation_axes: tuple[bool, bool, bool]

    def initialize(self):
        super().initialize()

        self.copy_rotation_axes = self.params.copy_rotation_axes

    def parent_bones(self):
        super().parent_bones()

        if self.connected_tweak and self.use_connect_reverse:
            self.rig_parent_bone = self.connected_tweak

    ####################################################
    # BONES

    class CtrlBones(BaseHeadTailRig.CtrlBones):
        master: str                    # Master control.

    class MchBones(BaseHeadTailRig.MchBones):
        rot_tail: str                  # Tail follow system.

    bones: BaseHeadTailRig.ToplevelBones[
        list[str],
        'Rig.CtrlBones',
        'Rig.MchBones',
        list[str]
    ]

    ####################################################
    # Master control

    @stage.generate_bones
    def make_master_control(self):
        org = self.bones.org[0]
        self.bones.ctrl.master = self.copy_bone(org, make_derived_name(org, 'ctrl', '_master'))
        self.default_prop_bone = self.bones.ctrl.master

    @stage.parent_bones
    def parent_master_control(self):
        self.set_bone_parent(self.bones.ctrl.master, self.rig_parent_bone)

    @stage.configure_bones
    def configure_master_control(self):
        bone = self.get_bone(self.bones.ctrl.master)
        bone.lock_location = True, True, True

    @stage.generate_widgets
    def make_master_control_widget(self):
        bone = self.bones.ctrl.master
        set_bone_widget_transform(self.obj, bone, self.bones.ctrl.tweak[-1])
        create_ball_socket_widget(self.obj, bone, size=0.7)

    ####################################################
    # Control bones

    @stage.parent_bones
    def parent_control_chain(self):
        self.set_bone_parent(self.bones.ctrl.fk[0], self.bones.mch.rot_tail)
        self.parent_bone_chain(self.bones.ctrl.fk, use_connect=False)

    @stage.rig_bones
    def rig_control_chain(self):
        ctrls = self.bones.ctrl.fk
        for args in zip(count(0), ctrls, [self.bones.ctrl.master] + ctrls):
            self.rig_control_bone(*args)

    def rig_control_bone(self, _i: int, ctrl: str, prev_ctrl: str):
        self.make_constraint(
            ctrl, 'COPY_ROTATION', prev_ctrl,
            use_xyz=self.copy_rotation_axes,
            space='LOCAL', mix_mode='BEFORE',
        )

    # Widgets
    def make_control_widget(self, i: int, ctrl: str):
        create_circle_widget(self.obj, ctrl, radius=0.5, head_tail=0.75)

    ####################################################
    # MCH bones associated with main controls

    @stage.generate_bones
    def make_mch_control_bones(self):
        self.bones.mch.rot_tail = self.make_mch_follow_bone(self.bones.org[0], 'tail', 0.0)

    @stage.parent_bones
    def parent_mch_control_bones(self):
        self.set_bone_parent(self.bones.mch.rot_tail, self.rig_parent_bone)

    ####################################################
    # Tweak bones

    @stage.generate_bones
    def make_tweak_chain(self):
        orgs = self.bones.org
        self.bones.ctrl.tweak = map_list(self.make_tweak_bone, count(0), orgs[0:1] + orgs)

    def make_tweak_bone(self, i: int, org: str):
        if i == 0:
            if self.check_connect_tweak(org):
                return self.connected_tweak

            else:
                name = self.copy_bone(org, 'tweak_base_' + strip_org(org), parent=False, scale=0.5)

        else:
            name = self.copy_bone(org, 'tweak_' + strip_org(org), parent=False, scale=0.5)
            put_bone(self.obj, name, self.get_bone(org).tail)

        return name

    ####################################################
    # Deform chain

    @stage.configure_bones
    def configure_deform_chain(self):
        if self.use_connect_chain and self.use_connect_reverse:
            self.get_bone(self.bones.deform[-1]).bone.bbone_easein = 0.0
            self.get_bone(self.rigify_parent.bones.deform[0]).bone.bbone_easein = 1.0
        else:
            self.get_bone(self.bones.deform[-1]).bone.bbone_easeout = 0.0

    ####################################################
    # SETTINGS

    @classmethod
    def add_parameters(cls, params):
        """ Add the parameters of this rig type to the
            RigifyParameters PropertyGroup
        """

        super().add_parameters(params)

        params.copy_rotation_axes = bpy.props.BoolVectorProperty(
            size=3,
            description="Automation axes",
            default=tuple([i == 0 for i in range(0, 3)])
            )

    @classmethod
    def parameters_ui(cls, layout, params):
        """ Create the ui for the rig parameters.
        """

        row = layout.row(align=True)
        for i, axis in enumerate(['x', 'y', 'z']):
            row.prop(params, "copy_rotation_axes", index=i, toggle=True, text=axis)

        super().parameters_ui(layout, params)


def create_sample(obj, *, parent=None):
    # generated by rigify.utils.write_metarig
    bpy.ops.object.mode_set(mode='EDIT')
    arm = obj.data

    bones = {}

    bone = arm.edit_bones.new('tail')
    bone.head[:] = 0.0000, 0.0552, 1.0099
    bone.tail[:] = -0.0000, 0.0582, 0.8669
    bone.roll = 0.0000
    bone.use_connect = False
    if parent:
        bone.parent = arm.edit_bones[parent]
    bones['tail'] = bone.name
    bone = arm.edit_bones.new('tail.001')
    bone.head[:] = -0.0000, 0.0582, 0.8669
    bone.tail[:] = -0.0000, 0.0365, 0.7674
    bone.roll = 0.0000
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['tail']]
    bones['tail.001'] = bone.name
    bone = arm.edit_bones.new('tail.002')
    bone.head[:] = -0.0000, 0.0365, 0.7674
    bone.tail[:] = -0.0000, 0.0010, 0.6984
    bone.roll = 0.0000
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['tail.001']]
    bones['tail.002'] = bone.name

    bpy.ops.object.mode_set(mode='OBJECT')
    pbone = obj.pose.bones[bones['tail']]
    pbone.rigify_type = 'spines.basic_tail'
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    try:
        pbone.rigify_parameters.connect_chain = bool(parent)
    except AttributeError:
        pass
    pbone = obj.pose.bones[bones['tail.001']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['tail.002']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'

    bpy.ops.object.mode_set(mode='EDIT')
    for bone in arm.edit_bones:
        bone.select = False
        bone.select_head = False
        bone.select_tail = False
    for b in bones:
        bone = arm.edit_bones[bones[b]]
        bone.select = True
        bone.select_head = True
        bone.select_tail = True
        arm.edit_bones.active = bone
        if bcoll := arm.collections.active:
            bcoll.assign(bone)
